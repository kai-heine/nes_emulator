#ifndef NES_PPU_HPP
#define NES_PPU_HPP

#include "cartridge.hpp"
#include "types.hpp"
#include <cassert>

namespace nes {

struct ppu_memory_map {
    array<u8, 2048> vram{};
    cartridge& cart;

    constexpr u8 read(u16 address) const noexcept {
        assert(address < 0x4000); // 14 bit address space

        if (address < 0x2000) {
            // pattern tables from chr-rom of cartridge
            return cart.chr_rom[address];
        } else {
            // nametables
            // ram with mirroring to fill 4kb
            // (0x3000 - 0x3fff are mirrors of 0x2000 - 0x2eff)
            switch (cart.nametable_mirroring) {
                // TODO: make this part of cartridge?
            case mirroring::horizontal: address &= ~(0x0400); break;
            case mirroring::vertical: address &= ~(0x0800); break;
            }
            return vram[(address - 0x2000u) % vram.size()];
        }
    }

    constexpr void write(u16 address, u8 value) noexcept {
        assert(address < 0x4000);  // 14 bit address space
        assert(address >= 0x2000); // TODO: CHR RAM
        assert(address < 0x3f00);  // writes to palette ram should not assert /WR

        switch (cart.nametable_mirroring) {
        case mirroring::horizontal: address &= ~(0x0400); break;
        case mirroring::vertical: address &= ~(0x0800); break;
        }
        vram[(address - 0x2000u) % vram.size()] = value;
    }
};

struct ppu_control_register {
    u16 nametable_base_address;
    u8 vram_address_increment;
    u16 sprite_pattern_table_address;
    u16 background_pattern_table_address;
    enum class pixels : bool { eight_by_eight, eight_by_sixteen } sprite_size;
    bool ext_master;
    bool generate_vblank_nmi;

    constexpr ppu_control_register(u8 value) noexcept
        : nametable_base_address{static_cast<u16>(0x2000 + (value & 0x03) * 0x0400)},
          vram_address_increment{static_cast<u8>((value & 0x04) != 0 ? 32 : 1)},
          sprite_pattern_table_address{static_cast<u16>((value & 0x08) != 0 ? 0x1000 : 0x0000)},
          background_pattern_table_address{static_cast<u16>((value & 0x10) != 0 ? 0x1000 : 0x0000)},
          sprite_size{(value & 0x20) ? pixels::eight_by_sixteen : pixels::eight_by_eight},
          ext_master{(value & 0x40) != 0}, generate_vblank_nmi{(value & 0x80) != 0} {}
};

struct ppu_status_register {
    bool sprite_overflow{false};
    bool sprite_zero_hit{false};
    bool vertical_blank_started{false};

    constexpr operator u8() const noexcept {
        return (sprite_overflow ? 0x20 : 0) | (sprite_zero_hit ? 0x40 : 0) |
               (vertical_blank_started ? 0x80 : 0);
    }
};

struct ppu_mask_register {
    bool greyscale{false};
    bool show_background_on_left{false};
    bool show_sprites_on_left{false};
    bool show_background{false};
    bool show_sprites{false};
    bool emphasize_red{false};
    bool emphasize_green{false};
    bool emphasize_blue{false};

    constexpr ppu_mask_register(u8 value) noexcept
        : greyscale{(value & 0x01) != 0},               //
          show_background_on_left{(value & 0x02) != 0}, //
          show_sprites_on_left{(value & 0x04) != 0},    //
          show_background{(value & 0x08) != 0},         //
          show_sprites{(value & 0x10) != 0},            //
          emphasize_red{(value & 0x20) != 0},           //
          emphasize_green{(value & 0x40) != 0},         //
          emphasize_blue{(value & 0x80) != 0} {}
};

struct vram_address_register {
    u8 coarse_x_scroll : 5 {0};
    u8 coarse_y_scroll : 5 {0};
    u8 nametable_select : 2 {0};
    u8 fine_y_scroll : 3 {0};

    constexpr vram_address_register& operator=(u16 value) noexcept {
        coarse_x_scroll = value & 0x1fu;
        coarse_y_scroll = (value >> 5u) & 0x1fu;
        nametable_select = (value >> 10u) & 0x03u;
        fine_y_scroll = (value >> 12u) & 0x07u;
        return *this;
    }

    constexpr operator u16() const noexcept {
        return static_cast<u16>((fine_y_scroll << 12u) | (nametable_select << 10u) |
                                (coarse_y_scroll << 5u) | coarse_x_scroll);
    }

    constexpr vram_address_register& operator+=(u16 increment) noexcept {
        *this = (static_cast<u16>(*this) + increment);
        return *this;
    }
};

struct picture_processing_unit {
  public:
    // external

    // connection to video memory bus - ppu is master
    u16 video_address_bus : 14 {0};           // read only from outside
    u8 video_data_bus{0};                     // read write from outside
    optional<data_dir> video_memory_access{}; // read only from outside

    // connection to cpu memory bus - ppu is slave - "control bus" ?
    u8 cpu_address_bus : 3 {0};             // write only from outside
    u8 cpu_data_bus{0};                     // read write from outside
    optional<data_dir> cpu_register_access; // write only from outside (combines /cs and r/w)

    bool nmi{false};

    void step() noexcept;

    u8* get_frame_buffer() noexcept { return frame_buffer.data(); }

    [[nodiscard]] constexpr bool has_frame_buffer() noexcept {
        auto ret = frame_buffer_valid;
        frame_buffer_valid = false;
        return ret;
    }

  private:
    // internal

    // TODO: make them structs like the cpu status register?
    ppu_control_register ppu_ctrl{0};
    ppu_mask_register ppu_mask{0};
    ppu_status_register ppu_status{};
    u8 oam_addr{};

    bool odd_frame{false}; // TODO?

    array<u8, 32> palette_ram{};
    array<u8, 256> object_attribute_memory{};
    array<u8, 32> secondary_oam{};

    // v and t registers:
    // yyy NN YYYYY XXXXX
    // ||| || ||||| +++++-- coarse X scroll
    // ||| || +++++-------- coarse Y scroll
    // ||| ++-------------- nametable select
    // +++----------------- fine Y scroll
    // TODO: types!
    vram_address_register current_vram_address{};   // "v" register
    vram_address_register temporary_vram_address{}; // "t" register
    u8 fine_x_scroll : 3 {};                        // "x" register
    bool first_write{true};                         // "w" register

    // shift registers for background rendering:
    // 2 16bit registers for pattern table data.
    //      upper byte is next tile, lower is current tile
    //      two registers for two planes of pattern table entry
    // 2 8bit registers for palette data for current tile
    //      one for bit 0, one for bit 1
    //      when shifted, bit from latch is shifted in
    //      next tile data is in a latch
    //      latch is reloaded every 8 cycles
    //      -> line of 8 bits have the same palette

    enum class register_map : u8 {
        ppuctrl,
        ppumask,
        ppustatus,
        oamaddr,
        oamdata,
        ppuscroll,
        ppuaddr,
        ppudata
    };

    u16 current_scanline{0};
    u16 current_scanline_cycle{0};
    // temp storage
    u8 nametable_entry{0};
    u8 attribute_table_entry{0};
    u8 lower_background_pattern{0};
    u8 upper_background_pattern{0};

    // TODO: types for this might be useful
    // TODO: maybe a single array with complete addresses?
    // 2 16 bit shift registers for pattern table data for two tiles (upper and lower planes)
    u16 upper_background_pattern_shift_reg{0};
    u16 lower_background_pattern_shift_reg{0};

    // 2 8 bit shift registers for palette attributes for current tile (two bits of palette index)
    u8 upper_background_palette_shift_reg{0};
    u8 lower_background_palette_shift_reg{0};
    // 2 1-bit latches that feed the shift regs
    u8 background_palette_latch : 2 {0};

    u8 internal_data_latch{0};  // TODO: decay?
    u8 internal_read_buffer{0}; // updated when reading PPUDATA

    vector<u8> frame_buffer = vector<u8>(256u * 240u);
    u16 current_pixel{0};
    bool frame_buffer_valid = false; // frame buffer contains a complete image (in vblank)

    // perform internal read/write action on register access
    // called from step()
    void handle_register_access() noexcept;

    void render_background() noexcept;
    void reload_shift_regs() noexcept;
    void fetch_vram() noexcept;
    void update_vram_address() noexcept;
    void shift_registers()noexcept;

    constexpr bool rendering_enabled() const noexcept {
        return ppu_mask.show_background || ppu_mask.show_sprites;
    }
    constexpr bool in_visible_scanline() const noexcept { return current_scanline < 240; }
    constexpr bool in_pre_render_scanline() const noexcept { return current_scanline == 261; }
};

} // namespace nes

#endif
