#include "memory.hpp"

namespace nes {

u8 cpu_memory_map::read() const noexcept {
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

void cpu_memory_map::write(u8 value) noexcept {
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

u8 ppu_memory_map::read(u16 address) const noexcept {
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

void ppu_memory_map::write(u16 address, u8 value) noexcept {
    assert(address < 0x4000);  // 14 bit address space
    assert(address >= 0x2000); // TODO: CHR RAM
    assert(address < 0x3f00);  // writes to palette ram should not assert /WR

    switch (cart.nametable_mirroring) {
    case mirroring::horizontal: address &= ~(0x0400); break;
    case mirroring::vertical: address &= ~(0x0800); break;
    }
    vram[(address - 0x2000u) % vram.size()] = value;
}

} // namespace nes
