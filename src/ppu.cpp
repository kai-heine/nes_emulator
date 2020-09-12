#include "ppu.hpp"

namespace nes {

void picture_processing_unit::handle_register_access() noexcept {
    if (!cpu_register_access) {
        // nothing to do
        return;
    }

    switch (*cpu_register_access) {
    case data_dir::read: {
        switch (static_cast<register_map>(cpu_address_bus)) {
        case register_map::ppustatus: {
            // TODO: clear "address latch"?
            // TODO: lower bits contain previously written data?
            first_write = true;
            cpu_data_bus = internal_data_latch = ppu_status;
            ppu_status.vertical_blank_started = false;
            break;
        }
        case register_map::oamdata: {
            // TODO: reading during rendering
            cpu_data_bus = internal_data_latch = object_attribute_memory[oam_addr];
            break;
        }
        case register_map::ppudata: {
            // what actually happens according to visual2c02:
            // this cycle: address is put on video address bus and sets ALE
            // NEXT cycle: data is read from video data bus and written to cpu data bus

            video_address_bus = current_vram_address;
            // in the palette ram range, /rd is still asserted,
            // so the underlying mirrored vram data gets placed into the read buffer
            video_memory_access = data_dir::read;
            if (current_vram_address < 0x3f00) {
                cpu_data_bus = internal_read_buffer;
            } else {
                cpu_data_bus = palette_ram[current_vram_address % palette_ram.size()];
            }
            current_vram_address += ppu_ctrl.vram_address_increment;
            internal_read_buffer = video_data_bus;
            break;
        }
        case register_map::ppuctrl:
        case register_map::ppumask:
        case register_map::oamaddr:
        case register_map::ppuscroll:
        case register_map::ppuaddr: {
            // reading write-only registers yields contents of internal data latch
            cpu_data_bus = internal_data_latch;
            break;
        }
        }
        break;
    }
    case data_dir::write: {
        // TODO: writing:
        // 1. set address bus, rw and data bus from outside
        // 2. call step() -> performs write action internally
        internal_data_latch = cpu_data_bus;

        switch (static_cast<register_map>(cpu_address_bus)) {
        case register_map::ppuctrl: {
            temporary_vram_address.nametable_select = cpu_data_bus & 0x03u;
            ppu_ctrl = cpu_data_bus;
            break;
        }
        case register_map::ppumask: {
            ppu_mask = cpu_data_bus;
            break;
        }
        case register_map::ppustatus: break; // read only
        case register_map::oamaddr: {
            oam_addr = cpu_data_bus;
            break;
        }
        case register_map::oamdata: {
            // TODO: write during rendering
            object_attribute_memory[oam_addr++] = cpu_data_bus;
            break;
        }
        case register_map::ppuscroll: {
            // write only, two writes
            if (first_write) {
                temporary_vram_address.coarse_x_scroll = (cpu_data_bus >> 3u) & 0x1fu;
                fine_x_scroll = cpu_data_bus & 0x07u;
            } else {
                temporary_vram_address.coarse_y_scroll = (cpu_data_bus >> 3u) & 0x1fu;
                temporary_vram_address.fine_y_scroll = cpu_data_bus & 0x07u;
            }
            first_write = !first_write;
            break;
        }
        case register_map::ppuaddr: {
            // write only, two writes
            if (first_write) {
                // upper 6 bits of vram address
                temporary_vram_address.fine_y_scroll = (cpu_data_bus >> 4u) & 0x03u;
                temporary_vram_address.nametable_select = (cpu_data_bus >> 2u) & 0x03u;
                temporary_vram_address.coarse_y_scroll =
                    (temporary_vram_address.coarse_y_scroll & 0x07u) |
                    ((cpu_data_bus << 3u) & 0x18u);
            } else {
                // lower byte of vram address
                temporary_vram_address.coarse_x_scroll = cpu_data_bus & 0x1fu;
                temporary_vram_address.coarse_y_scroll =
                    (temporary_vram_address.coarse_y_scroll & 0xf8u) |
                    ((cpu_data_bus >> 5u) & 0x07u);
                current_vram_address = temporary_vram_address;
            }
            first_write = !first_write;
            break;
        }
        case register_map::ppudata: {
            // read write
            // what actually happens according to visual2c02:
            // puts current vram address on video address bus and sets ALE
            // writes the value in the NEXT cycle

            video_address_bus = current_vram_address;
            video_data_bus = cpu_data_bus;
            if (current_vram_address < 0x3f00) {
                // in the palette ram range, data is still put on the data bus,
                // but not actually written to the underlying mirrored vram
                video_memory_access = data_dir::write;
            } else {
                palette_ram[current_vram_address % palette_ram.size()] = cpu_data_bus;
            }

            current_vram_address += ppu_ctrl.vram_address_increment;

            break;
        }
        }
        break;
    }
    }

    cpu_register_access.reset();
}

void picture_processing_unit::render_background() noexcept {
    if (!ppu_mask.show_background) {
        return;
    }

    if (!in_visible_scanline()) {
        return;
    }

    if ((current_scanline_cycle == 0) || (current_scanline_cycle > 256)) {
        return;
    }

    // address into palette ram (base address 0x3f00)
    // 43210
    // |||||
    // |||++- Pixel value from tile data
    // |++--- Palette number from attribute table or OAM
    // +----- Background/Sprite select
    // the byte at that memory location is the color value
    // (index into the complete color palette of the nes)
    // fine_x_scroll selects the bit from the shift regs
    u8 const palette_address =
        (((upper_background_palette_shift_reg << fine_x_scroll) >> 4u) & 0x08u) |
        (((lower_background_palette_shift_reg << fine_x_scroll) >> 5u) & 0x04u) |
        ((((upper_background_pattern_shift_reg >> 8u) << fine_x_scroll) >> 6u) & 0x02u) |
        ((((lower_background_pattern_shift_reg >> 8u) << fine_x_scroll) >> 7u) & 0x01u);

    u8 const pixel_color = palette_ram[palette_address];
    frame_buffer[current_pixel++] = pixel_color;
    if (current_pixel >= (256u * 240u)) {
        current_pixel = 0;
    }
}

void picture_processing_unit::reload_shift_regs() noexcept {
    assert(rendering_enabled());

    if (!in_visible_scanline() && !in_pre_render_scanline()) {
        return;
    }

    if ((((current_scanline_cycle > 8u) && (current_scanline_cycle < 258u)) ||
         (current_scanline_cycle > 320u)) &&
        ((current_scanline_cycle % 8u) == 1u)) {

        upper_background_pattern_shift_reg =
            (upper_background_pattern_shift_reg & 0xff00u) | upper_background_pattern;
        lower_background_pattern_shift_reg =
            (lower_background_pattern_shift_reg & 0xff00u) | lower_background_pattern;
        background_palette_latch = attribute_table_entry & 0x03u;
    }
}

void picture_processing_unit::fetch_vram() noexcept {
    assert(rendering_enabled());

    if (!(in_visible_scanline() || in_pre_render_scanline())) {
        return;
    }

    if ((current_scanline_cycle == 0) ||
        ((current_scanline_cycle > 256) && (current_scanline_cycle < 321))) {
        return;
    }

    // temp storage:
    // nametable entry = pattern table index
    // attribute table entry = palette table index
    switch (current_scanline_cycle % 8) {
    case 1: {
        // fetch nametable entry (tile) (vram address without fine y scroll)
        video_address_bus = 0x2000 | (current_vram_address.coarse_x_scroll |
                                      (current_vram_address.coarse_y_scroll << 5u) |
                                      (current_vram_address.nametable_select << 10u));
        video_memory_access = data_dir::read;
        break;
    }
    case 2: {
        nametable_entry = video_data_bus;
        break;
    }
    case 3: {
        // fetch attribute table byte
        // 10 NN 1111 YYY XXX
        //    || |||| ||| +++-- high 3 bits of coarse X (x/4)
        //    || |||| +++------ high 3 bits of coarse Y (y/4)
        //    || ++++---------- attribute offset (960 bytes)
        //    ++--------------- nametable select
        video_address_bus = 0x23C0 | // attribute table base address
                            (current_vram_address.nametable_select << 10u) |
                            (((current_vram_address.coarse_y_scroll / 4u) << 3u) & 0x38u) |
                            ((current_vram_address.coarse_x_scroll / 4u) & 0x07u);
        video_memory_access = data_dir::read;
        break;
    }
    case 4: {
        attribute_table_entry = video_data_bus;
        // pick the right quadrant of the attribute table entry
        if (((current_vram_address.coarse_x_scroll / 2u) % 2u) != 0) {
            // on the right half (odd coarse x scroll)
            attribute_table_entry >>= 2;
        }
        if (((current_vram_address.coarse_y_scroll / 2u) % 2u) != 0) {
            // on the lower half (odd coarse y scroll)
            attribute_table_entry >>= 4;
        }
        break;
    }
    case 5: {
        // fetch low bg pattern table byte. address:
        // 0HRRRR CCCCPTTT
        // |||||| |||||+++- T: Fine Y offset, the row number within a tile
        // |||||| ||||+---- P: Bit plane (0: "lower"; 1: "upper")
        // |||||| ++++----- C: Tile column
        // ||++++---------- R: Tile row
        // |+-------------- H: Half of sprite table (0: "left"; 1: "right")
        // +--------------- 0: Pattern table is at $0000-$1FFF

        video_address_bus = ppu_ctrl.background_pattern_table_address |     // left/right half
                            ((nametable_entry << 4u) & 0x0ff0u) |           // tile row/column
                            ((current_vram_address.fine_y_scroll) & 0x07u); // fine y offset
        video_memory_access = data_dir::read;
        break;
    }
    case 6: {
        lower_background_pattern = video_data_bus;
        // lower_background_pattern = 0xaa;
        break;
    }
    case 7: {
        // fetch high bg pattern table byte
        video_address_bus |= 0x08u; // upper bit plane
        video_memory_access = data_dir::read;
        break;
    }
    case 0: {
        upper_background_pattern = video_data_bus;
        // upper_background_pattern = 0xcc;
        break;
    }
    }
}

void picture_processing_unit::update_vram_address() noexcept {
    assert(rendering_enabled());

    if (!(in_visible_scanline() || in_pre_render_scanline())) {
        return;
    }

    if (current_scanline_cycle == 0) {
        return;
    }

    if (((current_scanline_cycle < 256) || (current_scanline_cycle > 320)) &&
        ((current_scanline_cycle % 8) == 0)) {
        // increment horizontal position
        if (++current_vram_address.coarse_x_scroll == 0) {
            // coarse x overflow: switch horizontal nametable
            current_vram_address.nametable_select ^= 0x1u;
        }
    } else if (current_scanline_cycle == 256) {
        // increment vertical position
        if (++current_vram_address.fine_y_scroll == 0) {
            // overflow from fine y to coarse y
            if (++current_vram_address.coarse_y_scroll == 0) {
                // coarse y overflow: switch vertical nametable
                current_vram_address.nametable_select ^= 0x2u;
            }
        }
    } else if (current_scanline_cycle == 257) {
        current_vram_address.coarse_x_scroll = temporary_vram_address.coarse_x_scroll;
        current_vram_address.nametable_select = (current_vram_address.nametable_select & 0x2u) |
                                                (temporary_vram_address.nametable_select & 0x1u);
    }

    if (in_pre_render_scanline() &&
        ((current_scanline_cycle >= 280) && (current_scanline_cycle <= 304))) {
        // copy vertical bits of t to v
        current_vram_address.coarse_y_scroll = temporary_vram_address.coarse_y_scroll;
        current_vram_address.fine_y_scroll = temporary_vram_address.fine_y_scroll;
        current_vram_address.nametable_select = (current_vram_address.nametable_select & 0x1u) |
                                                (temporary_vram_address.nametable_select & 0x2u);
    }
}

void picture_processing_unit::shift_registers() noexcept {
    if (!(in_visible_scanline() || in_pre_render_scanline())) {
        return;
    }

    if ((current_scanline_cycle > 0) && (current_scanline_cycle < 337)) {
        upper_background_pattern_shift_reg <<= 1u;
        lower_background_pattern_shift_reg <<= 1u;
        upper_background_palette_shift_reg =
            (upper_background_palette_shift_reg << 1u) | ((background_palette_latch >> 1u) & 0x01u);
        lower_background_palette_shift_reg =
            (lower_background_palette_shift_reg << 1u) | ((background_palette_latch >> 0u) & 0x01u);
    }
}

void picture_processing_unit::step() noexcept {
    // the ppu clock is internally divided by four
    //  - ale is high for the first half of the first read cycle
    //  - /rd is low during the complete second read cycle, data is read during that cycle
    //      ->  that would mean i have to at least implement half cycles
    //          or actively read from here, but i don't want that
    // i will make the following simplifications for memory access:
    //  - address and data lines are not muxed
    //  - there is no address latch enable
    //  - data will be available for reading/writing for the complete second cycle
    //  - the address must only be modified every 2 cycles to be accurate
    //  - /rd or /wr will be active in the first cycle and inactive in the second

    // defaults:
    video_memory_access.reset();

    handle_register_access();

    if (rendering_enabled()) {
        reload_shift_regs();
        render_background();
        fetch_vram();
        update_vram_address();
    }

    shift_registers();

    // TODO OAMADDR is set to 0 during each of ticks 257-320 (the sprite tile loading
    // interval) of the pre-render and visible scanlines.

    // vblank
    if (current_scanline == 241) {
        // first vertical blank scanline
        if (current_scanline_cycle == 1) {
            ppu_status.vertical_blank_started = true;
            frame_buffer_valid = true;
        }
    } else if (current_scanline == 261) {
        // pre-render scanline (261)
        if (current_scanline_cycle == 1) {
            ppu_status.vertical_blank_started = false;
            frame_buffer_valid = false;
            // TODO: sprite zero hit clear?
        }
    }

    nmi = ppu_ctrl.generate_vblank_nmi && ppu_status.vertical_blank_started;

    current_scanline_cycle++;
    if (current_scanline_cycle > 340) {
        current_scanline_cycle = 0;
        current_scanline++;
        if (current_scanline > 261) {
            current_scanline = 0;
        }
    }
}

} // namespace nes
