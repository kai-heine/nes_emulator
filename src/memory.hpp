#ifndef NES_MEMORY_HPP
#define NES_MEMORY_HPP

#include "apu.hpp"
#include "cartridge.hpp"
#include "controller.hpp"
#include "ppu.hpp"
#include "types.hpp"

namespace nes {

struct cpu_memory_map {
    array<u8, 2048>& ram_;
    picture_processing_unit& ppu_;
    cartridge& cartridge_;
    controller_port& controller_port_;
    audio_processing_unit& apu_;
    u16 address_{0};

    cpu_memory_map(array<u8, 2048>& ram, picture_processing_unit& ppu, cartridge& cart,
                   controller_port& controller, audio_processing_unit& apu)
        : ram_{ram}, ppu_{ppu}, cartridge_{cart}, controller_port_{controller}, apu_{apu} {}

    constexpr void set_address(u16 address) {
        address_ = address;

        if ((0x2000 <= address) && (address < 0x4000)) {
            ppu_.cpu_address_bus = address;
            ppu_.cpu_register_access = data_dir::read;
        }
    }

    constexpr u8 read() const noexcept {
        if (address_ < 0x2000) {
            return ram_[address_ % 0x0800];
        } else if (address_ < 0x4000) {
            return ppu_.cpu_data_bus;
        } else if (address_ == 0x4014) {
            // oam dma handled elsewhere
            std::abort();
        } else if (address_ < 0x4016) {
            return apu_.read(address_);
        } else if (address_ < 0x4018) {
            return controller_port_.read(address_);
        } else if (address_ < 0x4020) {
            // CPU Test Mode not implemented
            std::abort();
        } else {
            return cartridge_[address_];
        }
    }

    constexpr void write(u8 value) noexcept {
        if (address_ < 0x2000) {
            ram_[address_ % 0x0800] = value;
        } else if (address_ < 0x4000) {
            ppu_.cpu_address_bus = address_;
            ppu_.cpu_data_bus = value;
            ppu_.cpu_register_access = data_dir::write;
        } else if (address_ == 0x4014) {
            // oam dma handled elsewhere
            std::abort();
        } else if (address_ == 0x4016) {
            controller_port_.write(value);
        } else if (address_ < 0x4018) {
            apu_.write(address_, value);
        } else if (address_ < 0x4020) {
            // CPU Test Mode not implemented
            std::abort();
        } else {
            cartridge_[address_] = value;
        }
    }
};

} // namespace nes

#endif
