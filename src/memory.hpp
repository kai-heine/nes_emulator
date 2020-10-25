#ifndef NES_MEMORY_HPP
#define NES_MEMORY_HPP

#include "apu.hpp"
#include "cartridge.hpp"
#include "controller.hpp"
#include "ppu.hpp"
#include "types.hpp"

namespace nes {

struct cpu_memory_map {
    vector<u8> ram_ = vector<u8>(2048);
    picture_processing_unit& ppu_;
    cartridge& cartridge_;
    controller_port& controller_port_;
    audio_processing_unit& apu_;
    u16 address_{0};

    cpu_memory_map(picture_processing_unit& ppu, cartridge& cart, controller_port& controller,
                   audio_processing_unit& apu)
        : ppu_{ppu}, cartridge_{cart}, controller_port_{controller}, apu_{apu} {}

    constexpr void set_address(u16 address) {
        address_ = address;

        if ((0x2000 <= address) && (address < 0x4000)) {
            ppu_.cpu_address_bus = address;
            ppu_.cpu_register_access = data_dir::read;
        }
    }

    u8 read() const noexcept;

    void write(u8 value) noexcept;
};

struct ppu_memory_map {
    vector<u8> vram = vector<u8>(2048);
    cartridge& cart;

    u8 read(u16 address) const noexcept;

    void write(u16 address, u8 value) noexcept;
};

} // namespace nes

#endif
