#ifndef NES_CPU_INSTRUCTIONS_HPP
#define NES_CPU_INSTRUCTIONS_HPP

#include "addressing_modes.hpp"
#include <variant>

namespace nes {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

struct fetching_opcode {};
struct storing_data {};
struct waiting {};

using instruction_state = std::variant<fetching_opcode, fetching_address, storing_data, waiting>;
using operation = void (*)(cpu_state&);
using inout_operation = u8 (*)(cpu_state&, u8);
using in_operation = void (*)(cpu_state&, u8);
using branch_condition = bool (*)(cpu_state&);

instruction_state step(cpu_state& cpu, instruction_state state) noexcept;

// util header?
constexpr bool msb_of(u8 value) noexcept {
    constexpr u8 msb = 0x80;
    return (value & msb) != 0;
}

constexpr void fetch_opcode(cpu_state& cpu) noexcept {
    cpu.sync = true;
    cpu.address_bus = cpu.pc;
}

constexpr instruction_state single_byte_instruction(cpu_state& cpu, instruction_state state,
                                                    operation execute_operation) noexcept {
    return std::visit( //
        overloaded{
            [&](fetching_opcode) -> instruction_state {
                execute_operation(cpu);
                fetch_opcode(cpu);
                return fetching_address{};
            },
            [&](fetching_address) -> instruction_state {
                cpu.address_bus = cpu.pc;
                return fetching_opcode{};
            },
            [](auto) -> instruction_state { std::abort(); },
        },
        state);
}

template <addressing_mode fetch_address>
constexpr instruction_state
internal_execution_on_memory_data(cpu_state& cpu, instruction_state state,
                                  operation execute_operation) noexcept {
    return std::visit( //
        overloaded{
            [&](fetching_opcode) -> instruction_state {
                // data is available on data bus
                fetch_opcode(cpu);
                execute_operation(cpu);
                return fetching_address{};
            },
            [&](fetching_address addressing_state) -> instruction_state {
                if (fetch_address(cpu, addressing_state, true)) {
                    // address is on address bus and data will be fetched
                    return fetching_opcode{};
                } else {
                    return addressing_state;
                }
            },
            [](auto) -> instruction_state { std::abort(); },
        },
        state);
}

template <addressing_mode fetch_address>
constexpr instruction_state store_operation(cpu_state& cpu, instruction_state state,
                                            u8 register_to_store) noexcept {
    return std::visit( //
        overloaded{
            [&](fetching_opcode) -> instruction_state {
                fetch_opcode(cpu);
                return fetching_address{};
            },
            [&](fetching_address addressing_state) -> instruction_state {
                if (fetch_address(cpu, addressing_state, false)) {
                    // address is on address bus
                    cpu.rw = data_dir::write;
                    cpu.data_bus = register_to_store;
                    return fetching_opcode{};
                } else {
                    return addressing_state;
                }
            },
            [](auto) -> instruction_state { std::abort(); },
        },
        state);
}

template <addressing_mode fetch_address>
constexpr instruction_state read_modify_write(cpu_state& cpu, instruction_state state,
                                              inout_operation modify_data) noexcept {
    return std::visit( //
        overloaded{
            [&](fetching_opcode) -> instruction_state {
                fetch_opcode(cpu);
                return fetching_address{};
            },
            [&](fetching_address addressing_state) -> instruction_state {
                if (fetch_address(cpu, addressing_state, false)) {
                    // address is on address bus, data will be fetched
                    return waiting{};
                } else {
                    return addressing_state;
                }
            },
            [&](waiting) -> instruction_state {
                // data is on data bus
                cpu.rw = data_dir::write;
                return storing_data{};
            },
            [&](storing_data) -> instruction_state {
                cpu.rw = data_dir::write;
                cpu.data_bus = modify_data(cpu, cpu.data_bus);
                return fetching_opcode{};
            },
            [](auto) -> instruction_state { std::abort(); },
        } // namespace nes
        ,
        state);
}

constexpr instruction_state push_operation(cpu_state& cpu, instruction_state state,
                                           u8 register_to_push) noexcept {
    return std::visit( //
        overloaded{
            [&](fetching_opcode) -> instruction_state {
                fetch_opcode(cpu);
                return fetching_address{};
            },
            [&](fetching_address addressing_state) -> instruction_state {
                switch (addressing_state.cycle++) {
                case 0: cpu.address_bus = cpu.pc; return addressing_state;
                case 1: {
                    cpu.address_bus = stack_page | cpu.s--;
                    cpu.data_bus = register_to_push;
                    cpu.rw = data_dir::write;
                    return fetching_opcode{};
                }
                default: assert(false); return fetching_opcode{};
                }
            },
            [](auto) -> instruction_state { std::abort(); },
        },
        state);
}

constexpr instruction_state pull_operation(cpu_state& cpu, instruction_state state,
                                           in_operation pull_register) noexcept {
    return std::visit( //
        overloaded{
            [&](fetching_opcode) -> instruction_state {
                pull_register(cpu, cpu.data_bus);
                fetch_opcode(cpu);
                return fetching_address{};
            },
            [&](fetching_address addressing_state) -> instruction_state {
                switch (addressing_state.cycle++) {
                case 0: cpu.address_bus = cpu.pc; return addressing_state;
                case 1: {
                    cpu.address_bus = stack_page | cpu.s++;
                    return addressing_state;
                }
                case 2: {
                    cpu.address_bus = stack_page | cpu.s;
                    return fetching_opcode{};
                }
                default: assert(false); return fetching_opcode{};
                }
            },
            [](auto) -> instruction_state { std::abort(); },
        },
        state);
}

constexpr instruction_state jump_to_subroutine(cpu_state& cpu, instruction_state state) noexcept {
    return std::visit( //
        overloaded{
            [&](fetching_opcode) -> instruction_state {
                cpu.pc |= (cpu.data_bus << 8);
                fetch_opcode(cpu);
                return fetching_address{};
            },
            [&](fetching_address addressing_state) -> instruction_state {
                switch (addressing_state.cycle++) {
                case 0: cpu.address_bus = cpu.pc++; return addressing_state;
                case 1: {
                    cpu.address_bus = stack_page | cpu.s--;
                    addressing_state.address = cpu.data_bus; // store adl
                    return addressing_state;
                }
                case 2: {
                    cpu.rw = data_dir::write;
                    cpu.data_bus = (cpu.pc >> 8);
                    return addressing_state;
                }
                case 3: {
                    cpu.rw = data_dir::write;
                    cpu.address_bus = stack_page | cpu.s--;
                    cpu.data_bus = (cpu.pc & 0xff);
                    return addressing_state;
                }
                case 4: {
                    cpu.address_bus = cpu.pc;
                    cpu.pc = addressing_state.address & 0xff;
                    return fetching_opcode{};
                }
                default: assert(false); return fetching_opcode{};
                }
            },
            [](auto) -> instruction_state { std::abort(); },
        },
        state);
}

template <addressing_mode fetch_address>
constexpr instruction_state jump_operation(cpu_state& cpu, instruction_state state) noexcept {
    return std::visit( //
        overloaded{
            // no fetching_opcode state because the address being fetched _is_ the new pc
            [&](fetching_address addressing_state) -> instruction_state {
                if (fetch_address(cpu, addressing_state, false)) {
                    // jump address is on address bus
                    cpu.pc = addressing_state.address;
                    fetch_opcode(cpu);
                    return fetching_address{};
                } else {
                    return addressing_state;
                }
            },
            [](auto) -> instruction_state { std::abort(); },
        },
        state);
}

constexpr instruction_state return_from_subroutine(cpu_state& cpu,
                                                   instruction_state state) noexcept {
    return std::visit( //
        overloaded{
            [&](fetching_opcode) -> instruction_state {
                fetch_opcode(cpu);
                return fetching_address{};
            },
            [&](fetching_address addressing_state) -> instruction_state {
                switch (addressing_state.cycle++) {
                case 0: {
                    cpu.address_bus = cpu.pc++; // fetch data
                    return addressing_state;
                }
                case 1: {
                    // discard data
                    cpu.address_bus = (stack_page | cpu.s++); // fetch data
                    return addressing_state;
                }
                case 2: {
                    // discard data
                    cpu.address_bus = (stack_page | cpu.s++); // fetch PCL
                    return addressing_state;
                }
                case 3: {
                    addressing_state.address = cpu.data_bus; // save PCL
                    cpu.address_bus = (stack_page | cpu.s);  // fetch PCH
                    return addressing_state;
                }
                case 4: {
                    cpu.pc = (cpu.data_bus << 8u) | (addressing_state.address & 0x00ffu);
                    cpu.address_bus = cpu.pc++;
                    return fetching_opcode{};
                }
                default: assert(false); return fetching_opcode{};
                }
            },
            [](auto) -> instruction_state { std::abort(); },
        },
        state);
}

constexpr instruction_state branch_operation(cpu_state& cpu, instruction_state state,
                                             branch_condition branch_taken) noexcept {
    return std::visit( //
        overloaded{
            [&](fetching_opcode) -> instruction_state {
                fetch_opcode(cpu);
                return fetching_address{};
            },
            [&](fetching_address addressing_state) -> instruction_state {
                switch (addressing_state.cycle++) {
                case 0: {
                    cpu.address_bus = cpu.pc++;
                    if (branch_taken(cpu)) {
                        return addressing_state;
                    } else {
                        return fetching_opcode{};
                    }
                }
                case 1: {
                    cpu.address_bus = cpu.pc;
                    // sign extension
                    auto const offset = static_cast<u16>(static_cast<i8>(cpu.data_bus));
                    addressing_state.address = cpu.pc + offset;
                    cpu.pc = (cpu.pc & 0xff00) | (addressing_state.address & 0x00ff);
                    if (cpu.pc != addressing_state.address) {
                        return addressing_state;
                    } else {
                        return fetching_opcode{};
                    }
                }
                case 2: {
                    cpu.address_bus = cpu.pc;
                    cpu.pc = addressing_state.address;
                    return fetching_opcode{};
                }
                default: assert(false); return fetching_opcode{};
                }
            },
            [](auto) -> instruction_state { std::abort(); },
        },
        state);
}

constexpr instruction_state interrupt_sequence(cpu_state& cpu, instruction_state state) noexcept {
    return std::visit( //
        overloaded{
            [&](fetching_opcode) -> instruction_state {
                cpu.pc |= (cpu.data_bus << 8u);
                fetch_opcode(cpu);
                return fetching_address{};
            },
            [&](fetching_address addressing_state) -> instruction_state {
                switch (addressing_state.cycle++) {
                case 0: {
                    cpu.address_bus = cpu.pc;
                    if (!cpu.nmi_pending && !cpu.irq_pending) {
                        cpu.pc++;
                    }
                    return addressing_state;
                }
                case 1: {
                    cpu.address_bus = (stack_page | cpu.s--);
                    cpu.data_bus = (cpu.pc >> 8u); // store PCH on stack

                    if (!cpu.reset_pending) {
                        cpu.rw = data_dir::write;
                    }
                    return addressing_state;
                }
                case 2: {
                    cpu.address_bus = (stack_page | cpu.s--);
                    cpu.data_bus = (cpu.pc & 0x00ff); // store PCL on stack

                    if (!cpu.reset_pending) {
                        cpu.rw = data_dir::write;
                    }
                    return addressing_state;
                }
                case 3: {
                    cpu.address_bus = (stack_page | cpu.s--);
                    cpu.data_bus = [&] {
                        u8 value = cpu.p;
                        if (!cpu.reset_pending && !cpu.nmi_pending && !cpu.irq_pending) {
                            value |= break_bit;
                        }
                        return value;
                    }();

                    if (!cpu.reset_pending) {
                        cpu.rw = data_dir::write;
                    }
                    return addressing_state;
                }
                case 4: {
                    addressing_state.address = [&] {
                        if (cpu.reset_pending) {
                            return reset_vector;
                        } else if (cpu.nmi_pending) {
                            return nmi_vector;
                        } else {
                            return brk_irq_vector;
                        }
                    }();
                    cpu.address_bus = addressing_state.address;

                    cpu.reset_pending = false;
                    cpu.nmi_pending = false;
                    cpu.irq_pending = false;

                    return addressing_state;
                }
                case 5: {
                    cpu.pc = cpu.data_bus;
                    addressing_state.address++;
                    cpu.address_bus = addressing_state.address;
                    cpu.p.interrupt_disable = true;
                    return fetching_opcode{};
                }
                default: assert(false); return fetching_opcode{};
                }
            },
            [](auto) -> instruction_state { std::abort(); },
        },
        state);
}

constexpr instruction_state return_from_interrupt(cpu_state& cpu,
                                                  instruction_state state) noexcept {
    return std::visit( //
        overloaded{
            [&](fetching_opcode) -> instruction_state {
                cpu.pc |= (cpu.data_bus << 8u); // store PCH
                fetch_opcode(cpu);
                return fetching_address{};
            },
            [&](fetching_address addressing_state) -> instruction_state {
                switch (addressing_state.cycle++) {
                case 0: {
                    cpu.address_bus = cpu.pc; // fetch next op code
                    return addressing_state;
                }
                case 1: {
                    // discard data
                    cpu.address_bus = (stack_page | cpu.s++); // discarded stack fetch
                    return addressing_state;
                }
                case 2: {
                    // discard data
                    cpu.address_bus = (stack_page | cpu.s++); // fetch status register
                    return addressing_state;
                }
                case 3: {
                    cpu.p = cpu.data_bus;                     // store status register
                    cpu.address_bus = (stack_page | cpu.s++); // fetch PCL
                    return addressing_state;
                }
                case 4: {
                    cpu.pc = cpu.data_bus;                  // save PCL
                    cpu.address_bus = (stack_page | cpu.s); // fetch PCH
                    return fetching_opcode{};
                }
                default: assert(false); return fetching_opcode{};
                }
            },
            [](auto) -> instruction_state { std::abort(); },
        },
        state);
}

/*************************************************************************************************/

constexpr void set_negative_zero(cpu_state& cpu, u8 value) noexcept {
    cpu.p.negative = msb_of(value);
    cpu.p.zero = value == 0;
}

constexpr void adc_impl_(cpu_state& cpu, u8 operand) noexcept {
    // TODO: decimal mode?
    auto const result = cpu.a + operand + cpu.p.carry;
    cpu.p.carry = result & 0x100;
    cpu.p.overflow = (cpu.a ^ result) & (operand ^ result) & 0x80;
    cpu.a = static_cast<u8>(result);
    set_negative_zero(cpu, cpu.a);
}

constexpr void adc_impl(cpu_state& cpu) noexcept { adc_impl_(cpu, cpu.data_bus); }
constexpr void sbc_impl(cpu_state& cpu) noexcept { adc_impl_(cpu, ~cpu.data_bus); }

constexpr u8 asl_impl(cpu_state& cpu, u8 operand) noexcept {
    u8 const result = operand << 1;
    cpu.p.carry = msb_of(operand);
    set_negative_zero(cpu, result);
    return result;
}

constexpr u8 lsr_impl(cpu_state& cpu, u8 operand) noexcept {
    u8 const result = operand >> 1;
    cpu.p.carry = operand & 0x01;
    set_negative_zero(cpu, result);
    return result;
}

constexpr u8 rol_impl(cpu_state& cpu, u8 operand) noexcept {
    u16 const result = (operand << 1) | (cpu.p.carry ? 0x01 : 0x00);
    cpu.p.carry = msb_of(operand);
    set_negative_zero(cpu, static_cast<u8>(result));
    return static_cast<u8>(result);
}

constexpr u8 ror_impl(cpu_state& cpu, u8 operand) noexcept {
    auto const old_carry = cpu.p.carry;
    cpu.p.carry = operand & 0x01;
    u8 const result = (operand >> 1) | (old_carry ? 0x80 : 0x00);
    set_negative_zero(cpu, result);
    return static_cast<u8>(result);
}

} // namespace nes

#endif
