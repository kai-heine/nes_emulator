#ifndef NES_OAM_DMA_HPP
#define NES_OAM_DMA_HPP

#include "cpu/cpu.hpp"
#include "types.hpp"

namespace nes {

struct oam_dma_state {
    constexpr oam_dma_state(u8 page, bool even_cycle) noexcept
        : address{static_cast<u16>(page << 8u)}, //
          cycles_pending{static_cast<u16>(even_cycle ? 513u : 514u)} {}

    u16 address{};
    u16 cycles_pending{};
};

constexpr optional<oam_dma_state> step(cpu_state& cpu, oam_dma_state state) noexcept {
    if (state.cycles_pending > 512) {
        // dummy cycles
        state.cycles_pending--;

        // there is probably a better way
        cpu.rw = data_dir::read;
        cpu.address_bus = 0x0000u;
        return state;
    }

    if (state.cycles_pending % 2 == 0) {
        // read from cpu memory
        cpu.rw = data_dir::read;
        cpu.address_bus = state.address++;
    } else {
        // write to ppu oamdata
        cpu.rw = data_dir::write;
        cpu.address_bus = 0x2004;
        // data from last read is on data_bus
    }

    state.cycles_pending--;
    if (state.cycles_pending == 0) {
        // finished
        return std::nullopt;
    }
    return state;
}

} // namespace nes

#endif
