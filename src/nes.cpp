#include "nes.hpp"

namespace nes {
void nintendo_entertainment_system::run_single_frame() noexcept {
    while (!ppu_.has_frame_buffer()) {
        run_cpu_cycle();
    }
}

void nintendo_entertainment_system::run_cpu_cycle() noexcept {
    if (oam_dma_) {
        oam_dma_ = step(cpu_, *oam_dma_);
    } else {
        state_ = step(cpu_, state_);
    }

    memory_.set_address(cpu_.address_bus);

    if (cpu_.rw == data_dir::write) {
        if (cpu_.address_bus == 0x4014) {
            oam_dma_ = oam_dma_state(cpu_.data_bus, (cpu_.cycle_count % 2 == 0));
        } else {
            memory_.write(cpu_.data_bus);
        }
    }

    for (u8 i = 0; i < 3; i++) {
        ppu_.step();

        if (ppu_.video_memory_access) {
            if (*ppu_.video_memory_access == data_dir::read) {
                ppu_.video_data_bus = video_memory_.read(ppu_.video_address_bus);
            } else {
                video_memory_.write(ppu_.video_address_bus, ppu_.video_data_bus);
            }
        }
    }

    cpu_.nmi = ppu_.nmi;

    if (cpu_.rw == data_dir::read) {
        cpu_.data_bus = memory_.read();
    }

    apu_.step();
    cpu_.irq = apu_.interrupt();
}

} // namespace nes
