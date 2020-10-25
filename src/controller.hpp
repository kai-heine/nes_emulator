#ifndef NES_CONTROLLER_HPP
#define NES_CONTROLLER_HPP

#include "types.hpp"
#include <cassert>
#include <functional>

namespace nes {

struct controller_state {
    bool a : 1 {};
    bool b : 1 {};
    bool select : 1 {};
    bool start : 1 {};
    bool up : 1 {};
    bool down : 1 {};
    bool left : 1 {};
    bool right : 1 {};

    constexpr operator u8() const noexcept {
        return (a ? 0x01 : 0) | (b ? 0x02 : 0) | (select ? 0x04 : 0) | (start ? 0x08 : 0) |
               (up ? 0x10 : 0) | (down ? 0x20 : 0) | (left ? 0x40 : 0) | (right ? 0x80 : 0);
    }
};

struct controller_states {
    controller_state joy1;
    controller_state joy2;
};

struct controller_port {
    using callback_type = std::function<controller_states()>;

    bool controller_port_latch{false};
    u8 joy1_shift_reg{0};
    u8 joy2_shift_reg{0};

    // function that reads both controller ports
    callback_type read_controller;

    u8 read(u16 address) noexcept {
        assert((address == 0x4016) || (address == 0x4017));

        // 7  bit  0
        // ---- ----
        // xxxD DDDD
        // |||+-++++- Input data lines D4 D3 D2 D1 D0
        // +++------- Open bus

        // read controllers again if the latch was not reset
        if (controller_port_latch) {
            update_shift_regs();
        }

        auto& shift_reg = (address == 0x4016) ? joy1_shift_reg : joy2_shift_reg;
        u8 register_value = shift_reg & 0x01; // standard controller in D0
        shift_reg >>= 1;
        return register_value;
    }

    void write(u8 value) noexcept {
        // 7  bit  0
        // ---- ----
        // xxxx xEES
        //       |||
        //       ||+- Controller port latch bit
        //       ++-- Expansion port latch bits

        auto const previous = controller_port_latch;
        controller_port_latch = ((value & 0x01) != 0);

        // read controllers on falling edge of controller port latch
        if (previous && !controller_port_latch) {
            update_shift_regs();
        }
    }

    void update_shift_regs() noexcept {
        if (!read_controller) {
            return;
        }

        auto const [joy1, joy2] = read_controller();
        joy1_shift_reg = joy1;
        joy2_shift_reg = joy2;
    }
};

} // namespace nes

#endif
