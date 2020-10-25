#ifndef NES_NES_HPP
#define NES_NES_HPP

#include "apu.hpp"
#include "controller.hpp"
#include "cpu/cpu.hpp"
#include "cpu/instructions.hpp"
#include "memory.hpp"
#include "oam_dma.hpp"
#include "ppu.hpp"

namespace nes {

class nintendo_entertainment_system {
  public:
    explicit nintendo_entertainment_system(cartridge&& cart) : cartridge_{cart} {}

    void run_single_frame() noexcept;

    auto frame_buffer() noexcept {
        // TODO span? but i only ever need a pointer, size is constant
        return ppu_.get_frame_buffer();
    }

    // TODO: audio callback?

    auto sample_buffer() noexcept { return apu_.get_sample_buffer(); }

    void set_controller_callback(controller_port::callback_type&& callback) {
        controller_.read_controller = callback;
    }

  private:
    void run_cpu_cycle() noexcept;

    cpu_state cpu_{.reset_pending = true};
    instruction_state state_{fetching_address{}};
    optional<oam_dma_state> oam_dma_;

    picture_processing_unit ppu_;
    ppu_memory_map video_memory_{.cart = cartridge_};

    controller_port controller_;

    audio_processing_unit apu_;

    cpu_memory_map memory_{ppu_, cartridge_, controller_, apu_};

    cartridge cartridge_;
};

} // namespace nes

#endif
