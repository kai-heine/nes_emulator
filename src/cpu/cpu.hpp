#ifndef NES_CPU_CPU_HPP
#define NES_CPU_CPU_HPP

#include "../memory.hpp"
#include "../types.hpp"

namespace nes {

struct status_register {
    bool carry{false};
    bool zero{false};
    bool interrupt_disable{true};
    bool decimal{false};
    bool overflow{false};
    bool negative{false};

    constexpr status_register(u8 val) noexcept
        : carry((val & 0x01) != 0), zero((val & 0x02) != 0), interrupt_disable((val & 0x04) != 0),
          decimal((val & 0x08) != 0), overflow((val & 0x40) != 0), negative((val & 0x80) != 0) {}

    constexpr operator u8() const noexcept {
        return (carry << 0) | (zero << 1) | (interrupt_disable << 2) | (decimal << 3) | (1 << 5) |
               (overflow << 6) | (negative << 7);
    }
};

constexpr u8 break_bit = (1 << 4); // b flag

enum class data_dir : bool { read, write };

// TODO: better interface, enforce invariants? make internal stuff private?
struct cpu_state {
    // external
    u16 address_bus{0};
    u8 data_bus{0};
    data_dir rw{data_dir::read};

    bool reset{false};
    bool nmi{false};
    bool irq{false};

    // internal
    u16 pc{0}; // program counter
    u8 a{0};   // accumulator
    u8 x{0};   // index register x
    u8 y{0};   // index register y
    u8 s{0};   // stack pointer
    status_register p{0x34};
    u8 instruction_register{0x00};

    bool sync{false};

    bool reset_pending{false};
    bool nmi_pending{false};
    bool irq_pending{false};
};

constexpr u16 stack_page = 0x0100;
constexpr u16 nmi_vector = 0xfffa;
constexpr u16 reset_vector = 0xfffc;
constexpr u16 brk_irq_vector = 0xfffe;

} // namespace nes

#endif
