#include "ppu.hpp"
#include <algorithm>
#include <limits>
#include <span>

namespace nes {

// TODO: can use consteval once compiler support is there
constexpr auto generate_lookup_table() noexcept {
    constexpr auto reverse_byte = [](u8 n) {
        u8 rv = 0;
        for (std::size_t i = 0; i < std::numeric_limits<u8>::digits; ++i, n >>= 1) {
            rv = (rv << 1) | (n & 0x01);
        }
        return rv;
    };

    std::array<u8, 256> table{};
    std::generate(table.begin(), table.end(),
                  [=, i = u8{0}]() mutable { return reverse_byte(i++); });
    return table;
}

static constexpr auto reverse_lut = generate_lookup_table();

constexpr u8 reverse(u8 x) noexcept { return reverse_lut[x]; }

constexpr u8& oam_raw_access(std::span<sprite_info> oam, std::size_t address) noexcept {
    std::size_t const sprite_index = address / 4;
    std::size_t const selector = address % 4;
    switch (selector) {
    default: assert(selector < 4); [[fallthrough]]; // silence a nonsensical warning
    case 0: return oam[sprite_index].y_position;
    case 1: return oam[sprite_index].tile_index;
    case 2: return oam[sprite_index].attributes;
    case 3: return oam[sprite_index].x_position;
    }
}

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
            cpu_data_bus = internal_data_latch = oam_raw_access(primary_oam, oam_addr);
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
        internal_data_latch = cpu_data_bus;

        switch (static_cast<register_map>(cpu_address_bus)) {
        case register_map::ppuctrl: {
            temporary_vram_address.nametable_select = cpu_data_bus & 0x03;
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
            oam_raw_access(primary_oam, oam_addr++) = cpu_data_bus;
            break;
        }
        case register_map::ppuscroll: {
            // write only, two writes
            if (first_write) {
                temporary_vram_address.coarse_x_scroll = (cpu_data_bus >> 3) & 0x1f;
                fine_x_scroll = cpu_data_bus & 0x07;
            } else {
                temporary_vram_address.coarse_y_scroll = (cpu_data_bus >> 3) & 0x1f;
                temporary_vram_address.fine_y_scroll = cpu_data_bus & 0x07;
            }
            first_write = !first_write;
            break;
        }
        case register_map::ppuaddr: {
            // write only, two writes
            if (first_write) {
                // upper 6 bits of vram address
                temporary_vram_address.fine_y_scroll = (cpu_data_bus >> 4) & 0x03;
                temporary_vram_address.nametable_select = (cpu_data_bus >> 2) & 0x03;
                temporary_vram_address.coarse_y_scroll =
                    (temporary_vram_address.coarse_y_scroll & 0x07) | ((cpu_data_bus << 3) & 0x18);
            } else {
                // lower byte of vram address
                temporary_vram_address.coarse_x_scroll = cpu_data_bus & 0x1f;
                temporary_vram_address.coarse_y_scroll =
                    (temporary_vram_address.coarse_y_scroll & 0xf8) | ((cpu_data_bus >> 5) & 0x07);
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

void picture_processing_unit::render_pixel() noexcept {
    if (!rendering_enabled()) {
        return;
    }

    if (!in_visible_scanline()) {
        return;
    }

    if ((current_scanline_cycle == 0) || (current_scanline_cycle > 256)) {
        return;
    }

    u8 palette_number = 0;
    u8 pixel_value = 0;
    bool sprite_select = false;

    if (ppu_mask.show_background) {
        palette_number = background_palette_shift_reg.at(fine_x_scroll);
        pixel_value = background_pattern_shift_reg.at(fine_x_scroll);
    }

    if (ppu_mask.show_sprites) {
        auto const first_visible_sprite =
            std::find_if(sprites.begin(), sprites.end(), [this](sprite_data const& sprite) {
                // sprite is active and has a non-transparent pixel value
                return (sprite.x_position_counter == 0) &&
                       ((sprite.pattern_shift_reg.at(fine_x_scroll) != 0));
            });

        if (first_visible_sprite != sprites.end()) {
            if ((pixel_value == 0) || first_visible_sprite->attribute_latch.has_priority()) {
                // background pixel is transparent or sprite has priority
                pixel_value = first_visible_sprite->pattern_shift_reg.at(fine_x_scroll);
                palette_number = first_visible_sprite->attribute_latch.palette();
                sprite_select = true;
            }
        }
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
        (sprite_select ? 0x10 : 0x00) | ((palette_number << 2) & 0x0c) | (pixel_value & 0x03);

    u8 const pixel_color = palette_ram[palette_address];
    frame_buffer[current_pixel++] = pixel_color;
    if (current_pixel >= (256 * 240)) {
        current_pixel = 0;
    }
}

void picture_processing_unit::reload_shift_regs() noexcept {
    assert(rendering_enabled());

    if (!in_visible_scanline() && !in_pre_render_scanline()) {
        return;
    }

    if ((((current_scanline_cycle > 8) && (current_scanline_cycle < 258)) ||
         (current_scanline_cycle > 320)) &&
        ((current_scanline_cycle % 8) == 1)) {
        background_pattern_shift_reg.reload(upper_background_pattern, lower_background_pattern);
        background_palette_latch = attribute_table_entry & 0x03;
    }
}

void picture_processing_unit::fetch_background_data() noexcept {
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
                                      (current_vram_address.coarse_y_scroll << 5) |
                                      (current_vram_address.nametable_select << 10));
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
                            (current_vram_address.nametable_select << 10) |
                            (((current_vram_address.coarse_y_scroll / 4) << 3) & 0x38) |
                            ((current_vram_address.coarse_x_scroll / 4) & 0x07);
        video_memory_access = data_dir::read;
        break;
    }
    case 4: {
        attribute_table_entry = video_data_bus;
        // pick the right quadrant of the attribute table entry
        if (((current_vram_address.coarse_x_scroll / 2) % 2) != 0) {
            // on the right half (odd coarse x scroll)
            attribute_table_entry >>= 2;
        }
        if (((current_vram_address.coarse_y_scroll / 2) % 2) != 0) {
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

        video_address_bus = ppu_ctrl.background_pattern_table_address |    // left/right half
                            ((nametable_entry << 4) & 0x0ff0) |            // tile row/column
                            ((current_vram_address.fine_y_scroll) & 0x07); // fine y offset
        video_memory_access = data_dir::read;
        break;
    }
    case 6: {
        lower_background_pattern = video_data_bus;
        break;
    }
    case 7: {
        // fetch high bg pattern table byte
        video_address_bus |= 0x08; // upper bit plane
        video_memory_access = data_dir::read;
        break;
    }
    case 0: {
        upper_background_pattern = video_data_bus;
        break;
    }
    }
}

void picture_processing_unit::fetch_sprite_data() noexcept {
    assert(rendering_enabled());

    if (!(in_visible_scanline() || in_pre_render_scanline())) {
        return;
    }

    if ((current_scanline_cycle < 257) || (current_scanline_cycle > 320)) {
        return;
    }

    oam_addr = 0;

    std::size_t const sprite_number = (current_scanline_cycle - 257) / 8;
    assert(sprite_number < 8); // never trust anyone

    switch (current_scanline_cycle % 8) {
    case 1: {
        // TODO garbage nametable byte
        break;
    }
    case 2: {
        // TODO garbage nametable byte
        break;
    }
    case 3: {
        // TODO garbage attribute byte
        break;
    }
    case 4: {
        // TODO garbage attribute byte
        break;
    }
    case 5: {
        // fetch low sprite pattern table byte. address:
        // 0HRRRR CCCCPTTT
        // |||||| |||||+++- T: Fine Y offset, the row number within a tile
        // |||||| ||||+---- P: Bit plane (0: "lower"; 1: "upper")
        // |||||| ++++----- C: Tile column
        // ||++++---------- R: Tile row
        // |+-------------- H: Half of sprite table (0: "left"; 1: "right")
        // +--------------- 0: Pattern table is at $0000-$1FFF

        // TODO: unused sprites (dummy data ff) are loaded with transparent pattern data

        u8 const tile_index = secondary_oam[sprite_number].tile_index;
        u16 const pattern_table_address = (ppu_ctrl.sprite_size == pixels::eight_by_eight)
                                              ? ppu_ctrl.sprite_pattern_table_address
                                              : ((tile_index & 0x01) << 12);
        u16 const tile_address = (ppu_ctrl.sprite_size == pixels::eight_by_eight)
                                     ? (tile_index << 4)
                                     : ((tile_index & 0xfe) << 4);
        u8 const fine_y_offset = (current_vram_address.fine_y_scroll -
                                  (secondary_oam[sprite_number].y_position + 1) % 8) &
                                 0x07;
        video_address_bus = pattern_table_address | tile_address | fine_y_offset;
        video_memory_access = data_dir::read;
        break;
    }
    case 6: {
        sprites[sprite_number].pattern_shift_reg.lower =
            secondary_oam[sprite_number].attributes.flip_horizontally() ? reverse(video_data_bus)
                                                                        : video_data_bus;
        break;
    }
    case 7: {
        // fetch high sprite pattern table byte
        video_address_bus |= 0x08; // upper bit plane
        video_memory_access = data_dir::read;
        break;
    }
    case 0: {
        sprites[sprite_number].pattern_shift_reg.upper =
            secondary_oam[sprite_number].attributes.flip_horizontally() ? reverse(video_data_bus)
                                                                        : video_data_bus;
        sprites[sprite_number].attribute_latch = secondary_oam[sprite_number].attributes;
        sprites[sprite_number].x_position_counter = secondary_oam[sprite_number].x_position;
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
            current_vram_address.nametable_select ^= 0x1;
        }
    } else if (current_scanline_cycle == 256) {
        // increment vertical position
        if (++current_vram_address.fine_y_scroll == 0) {
            // overflow from fine y to coarse y
            if (++current_vram_address.coarse_y_scroll == 0) {
                // coarse y overflow: switch vertical nametable
                current_vram_address.nametable_select ^= 0x2;
            }
        }
    } else if (current_scanline_cycle == 257) {
        current_vram_address.coarse_x_scroll = temporary_vram_address.coarse_x_scroll;
        current_vram_address.nametable_select = (current_vram_address.nametable_select & 0x2) |
                                                (temporary_vram_address.nametable_select & 0x1);
    }

    if (in_pre_render_scanline() &&
        ((current_scanline_cycle >= 280) && (current_scanline_cycle <= 304))) {
        // copy vertical bits of t to v
        current_vram_address.coarse_y_scroll = temporary_vram_address.coarse_y_scroll;
        current_vram_address.fine_y_scroll = temporary_vram_address.fine_y_scroll;
        current_vram_address.nametable_select = (current_vram_address.nametable_select & 0x1) |
                                                (temporary_vram_address.nametable_select & 0x2);
    }
}

void picture_processing_unit::shift_registers() noexcept {
    if (!(in_visible_scanline() || in_pre_render_scanline())) {
        return;
    }

    if ((current_scanline_cycle > 0) && (current_scanline_cycle < 337)) {
        background_pattern_shift_reg.shift();
        background_palette_shift_reg.shift(background_palette_latch);
    }

    if ((current_scanline_cycle > 0) && (current_scanline_cycle < 257)) {
        for (auto& sprite : sprites) {
            if (sprite.x_position_counter == 0) {
                // sprite is active
                sprite.pattern_shift_reg.shift();
            } else {
                sprite.x_position_counter--;
            }
        }
    }
}

// copy_if but respects size of output range and returns pair of iterators
template <class InputIt, class OutputIt, class UnaryPredicate>
std::pair<InputIt, OutputIt> copy_if(InputIt in_first, InputIt in_last, OutputIt out_first,
                                     OutputIt out_last, UnaryPredicate pred) {
    while ((in_first != in_last) && (out_first != out_last)) {
        if (pred(*in_first)) {
            *out_first++ = *in_first;
        }
        in_first++;
    }
    return {in_first, out_first};
}

void picture_processing_unit::evaluate_sprites() noexcept {
    assert(rendering_enabled());

    if (!in_visible_scanline()) {
        return;
    }

    if ((current_scanline_cycle == 0) || (current_scanline_cycle > 256)) {
        return;
    }

    if (current_scanline_cycle == 1) {
        // secondary oam clear
        secondary_oam.fill(sprite_info{});
    }

    if (current_scanline_cycle == 65) {
        // sprite evaluation for next scanline
        auto const is_on_scanline = [this](sprite_info const& oam_entry) {
            int const distance = current_scanline - oam_entry.y_position;
            return (distance >= 0) && (distance < 8); // TODO: 8x16 sprites?
        };

        auto const [primary_it, secondary_it] =
            copy_if(primary_oam.begin(), primary_oam.end(), secondary_oam.begin(),
                    secondary_oam.end(), is_on_scanline);

        ppu_status.sprite_overflow =
            std::count_if(primary_it, primary_oam.end(), is_on_scanline) > 0;
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
        render_pixel();
        fetch_background_data();
        fetch_sprite_data();
        update_vram_address();
        evaluate_sprites();
        shift_registers();
    }

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
