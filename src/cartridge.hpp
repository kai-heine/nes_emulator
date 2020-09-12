#ifndef NES_CARTRIDGE_HPP
#define NES_CARTRIDGE_HPP

#include "types.hpp"
#include <cassert>

namespace nes {

enum class mirroring : u8 { horizontal, vertical };

// TODO: should the mapper be part of the cartridge?
struct cartridge {
    vector<u8> prg_rom;
    vector<u8> prg_ram;
    vector<u8> chr_rom;
    mirroring nametable_mirroring{};

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

} // namespace nes

#endif
