#ifndef NES_MEMORY_HPP
#define NES_MEMORY_HPP

#include "ppu.hpp"
#include "types.hpp"
#include <cassert>

namespace nes {

struct cartridge {
    vector<u8> prg_rom;
    vector<u8> prg_ram;
    vector<u8> chr_rom;

    [[nodiscard]] u8& operator[](u16 address) noexcept {
        assert(address >= 0x6000);
        assert((prg_rom.size() % 0x4000) == 0);

        if (address < 0x8000) {
            return prg_ram[(address - 0x6000u) % prg_ram.size()];
        } else {
            return prg_rom[(address - 0x8000u) % prg_rom.size()];
        }
    }
};

struct nes_memory_map {
    array<u8, 2048>& ram_;
    picture_processing_unit& ppu_;
    cartridge& cartridge_;

    nes_memory_map(array<u8, 2048>& ram, picture_processing_unit& ppu, cartridge& cart)
        : ram_{ram}, ppu_{ppu}, cartridge_{cart} {}

    [[nodiscard]] u8& operator[](u16 address) noexcept {
        if (address < 0x2000) {
            return ram_[address % 0x0800];
        } else if (address < 0x4000) {
            // assert(false);
            // TODO
            return ppu_[address % 8];
        } else if (address < 0x4020) {
            // TODO NES APU + I/O registers
            // assert(false);
            static u8 shut_up = 0;
            return shut_up;
        } else {
            return cartridge_[address];
        }
    }
};

} // namespace nes

#endif
