#include "instructions.hpp"
#include "../types.hpp"
#include "addressing_modes.hpp"
#include "cpu.hpp"

namespace nes {

using instruction = instruction_state (*)(cpu_state&, instruction_state);

instruction_state illegal(cpu_state&, instruction_state) noexcept {
    assert(false);
    return {};
}

template <addressing_mode Mode>
constexpr instruction_state ADC(cpu_state& cpu, instruction_state state) noexcept {
    return internal_execution_on_memory_data<Mode>(cpu, state, adc_impl);
}

template <addressing_mode Mode>
constexpr instruction_state AND(cpu_state& cpu, instruction_state state) noexcept {
    return internal_execution_on_memory_data<Mode>(cpu, state, [](cpu_state& cpu) {
        u8 const result = cpu.a & cpu.data_bus;
        set_negative_zero(cpu, result);
        cpu.a = result;
    });
}

template <addressing_mode Mode>
constexpr instruction_state ASL(cpu_state& cpu, instruction_state state) noexcept {
    return read_modify_write<Mode>(cpu, state, asl_impl);
}

template <>
constexpr instruction_state ASL<accumulator>(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state,
                                   [](cpu_state& cpu) { cpu.a = asl_impl(cpu, cpu.a); });
}

constexpr instruction_state BCC(cpu_state& cpu, instruction_state state) noexcept {
    return branch_operation(cpu, state, [](cpu_state& cpu) { return !cpu.p.carry; });
}
constexpr instruction_state BCS(cpu_state& cpu, instruction_state state) noexcept {
    return branch_operation(cpu, state, [](cpu_state& cpu) { return cpu.p.carry; });
}
constexpr instruction_state BEQ(cpu_state& cpu, instruction_state state) noexcept {
    return branch_operation(cpu, state, [](cpu_state& cpu) { return cpu.p.zero; });
}

template <addressing_mode Mode>
constexpr instruction_state BIT(cpu_state& cpu, instruction_state state) noexcept {
    return internal_execution_on_memory_data<Mode>(cpu, state, [](cpu_state& cpu) {
        u8 const result = cpu.a & cpu.data_bus;
        cpu.p.negative = cpu.data_bus & 0x80;
        cpu.p.overflow = cpu.data_bus & 0x40;
        cpu.p.zero = result == 0;
    });
}

constexpr instruction_state BMI(cpu_state& cpu, instruction_state state) noexcept {
    return branch_operation(cpu, state, [](cpu_state& cpu) { return cpu.p.negative; });
}
constexpr instruction_state BNE(cpu_state& cpu, instruction_state state) noexcept {
    return branch_operation(cpu, state, [](cpu_state& cpu) { return !cpu.p.zero; });
}
constexpr instruction_state BPL(cpu_state& cpu, instruction_state state) noexcept {
    return branch_operation(cpu, state, [](cpu_state& cpu) { return !cpu.p.negative; });
}

constexpr instruction_state BRK(cpu_state& cpu, instruction_state state) noexcept {
    return interrupt_sequence(cpu, state);
}

constexpr instruction_state BVC(cpu_state& cpu, instruction_state state) noexcept {
    return branch_operation(cpu, state, [](cpu_state& cpu) { return !cpu.p.overflow; });
}
constexpr instruction_state BVS(cpu_state& cpu, instruction_state state) noexcept {
    return branch_operation(cpu, state, [](cpu_state& cpu) { return cpu.p.overflow; });
}

constexpr instruction_state CLC(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state, [](cpu_state& cpu) { cpu.p.carry = false; });
}

constexpr instruction_state CLD(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state, [](cpu_state& cpu) { cpu.p.decimal = false; });
}

constexpr instruction_state CLI(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state,
                                   [](cpu_state& cpu) { cpu.p.interrupt_disable = false; });
}

constexpr instruction_state CLV(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state, [](cpu_state& cpu) { cpu.p.overflow = false; });
}

template <addressing_mode Mode>
constexpr instruction_state CMP(cpu_state& cpu, instruction_state state) noexcept {
    return internal_execution_on_memory_data<Mode>(cpu, state, [](cpu_state& cpu) {
        u8 const result = cpu.a - cpu.data_bus;
        set_negative_zero(cpu, result);
        cpu.p.carry = cpu.data_bus <= cpu.a;
    });
}

template <addressing_mode Mode>
constexpr instruction_state CPX(cpu_state& cpu, instruction_state state) noexcept {
    return internal_execution_on_memory_data<Mode>(cpu, state, [](cpu_state& cpu) {
        u8 const result = cpu.x - cpu.data_bus;
        set_negative_zero(cpu, result);
        cpu.p.carry = cpu.data_bus <= cpu.x;
    });
}

template <addressing_mode Mode>
constexpr instruction_state CPY(cpu_state& cpu, instruction_state state) noexcept {
    return internal_execution_on_memory_data<Mode>(cpu, state, [](cpu_state& cpu) {
        u8 const result = cpu.y - cpu.data_bus;
        set_negative_zero(cpu, result);
        cpu.p.carry = cpu.data_bus <= cpu.y;
    });
}

template <addressing_mode Mode>
constexpr instruction_state DEC(cpu_state& cpu, instruction_state state) noexcept {
    return read_modify_write<Mode>(cpu, state, [](cpu_state& cpu, u8 value) {
        value--;
        set_negative_zero(cpu, value);
        return value;
    });
}

constexpr instruction_state DEX(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state, [](cpu_state& cpu) {
        cpu.x--;
        set_negative_zero(cpu, cpu.x);
    });
}

constexpr instruction_state DEY(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state, [](cpu_state& cpu) {
        cpu.y--;
        set_negative_zero(cpu, cpu.y);
    });
}

template <addressing_mode Mode>
constexpr instruction_state EOR(cpu_state& cpu, instruction_state state) noexcept {
    return internal_execution_on_memory_data<Mode>(cpu, state, [](cpu_state& cpu) {
        cpu.a ^= cpu.data_bus;
        set_negative_zero(cpu, cpu.a);
    });
}

template <addressing_mode Mode>
constexpr instruction_state INC(cpu_state& cpu, instruction_state state) noexcept {
    return read_modify_write<Mode>(cpu, state, [](cpu_state& cpu, u8 value) {
        value++;
        set_negative_zero(cpu, value);
        return value;
    });
}

constexpr instruction_state INX(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state, [](cpu_state& cpu) {
        cpu.x++;
        set_negative_zero(cpu, cpu.x);
    });
}

constexpr instruction_state INY(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state, [](cpu_state& cpu) {
        cpu.y++;
        set_negative_zero(cpu, cpu.y);
    });
}

template <addressing_mode Mode>
constexpr instruction_state JMP(cpu_state& cpu, instruction_state state) noexcept {
    return jump_operation<Mode>(cpu, state);
}

constexpr instruction_state JSR(cpu_state& cpu, instruction_state state) noexcept {
    return jump_to_subroutine(cpu, state);
}

template <addressing_mode Mode>
constexpr instruction_state LDA(cpu_state& cpu, instruction_state state) noexcept {
    return internal_execution_on_memory_data<Mode>(cpu, state, [](cpu_state& cpu) {
        cpu.a = cpu.data_bus;
        set_negative_zero(cpu, cpu.a);
    });
}

template <addressing_mode Mode>
constexpr instruction_state LDX(cpu_state& cpu, instruction_state state) noexcept {
    return internal_execution_on_memory_data<Mode>(cpu, state, [](cpu_state& cpu) {
        cpu.x = cpu.data_bus;
        set_negative_zero(cpu, cpu.x);
    });
}

template <addressing_mode Mode>
constexpr instruction_state LDY(cpu_state& cpu, instruction_state state) noexcept {
    return internal_execution_on_memory_data<Mode>(cpu, state, [](cpu_state& cpu) {
        cpu.y = cpu.data_bus;
        set_negative_zero(cpu, cpu.y);
    });
}

template <addressing_mode Mode>
constexpr instruction_state LSR(cpu_state& cpu, instruction_state state) noexcept {
    return read_modify_write<Mode>(cpu, state, lsr_impl);
}

template <>
constexpr instruction_state LSR<accumulator>(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state,
                                   [](cpu_state& cpu) { cpu.a = lsr_impl(cpu, cpu.a); });
}

constexpr instruction_state NOP(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state, [](cpu_state&) {});
}

template <addressing_mode Mode>
constexpr instruction_state ORA(cpu_state& cpu, instruction_state state) noexcept {
    return internal_execution_on_memory_data<Mode>(cpu, state, [](cpu_state& cpu) {
        cpu.a |= cpu.data_bus;
        set_negative_zero(cpu, cpu.a);
    });
}

constexpr instruction_state PHA(cpu_state& cpu, instruction_state state) noexcept {
    return push_operation(cpu, state, cpu.a);
}

constexpr instruction_state PHP(cpu_state& cpu, instruction_state state) noexcept {
    return push_operation(cpu, state, cpu.p | break_bit);
}

constexpr instruction_state PLA(cpu_state& cpu, instruction_state state) noexcept {
    return pull_operation(cpu, state, cpu.a);
}

constexpr instruction_state PLP(cpu_state& cpu, instruction_state state) noexcept {
    return pull_operation(cpu, state, cpu.p);
}

template <addressing_mode Mode>
constexpr instruction_state ROL(cpu_state& cpu, instruction_state state) noexcept {
    return read_modify_write<Mode>(cpu, state, rol_impl);
}

template <>
constexpr instruction_state ROL<accumulator>(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state,
                                   [](cpu_state& cpu) { cpu.a = rol_impl(cpu, cpu.a); });
}

template <addressing_mode Mode>
constexpr instruction_state ROR(cpu_state& cpu, instruction_state state) noexcept {
    return read_modify_write<Mode>(cpu, state, ror_impl);
}

template <>
constexpr instruction_state ROR<accumulator>(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state,
                                   [](cpu_state& cpu) { cpu.a = ror_impl(cpu, cpu.a); });
}

constexpr instruction_state RTI(cpu_state& cpu, instruction_state state) noexcept {
    return return_from_interrupt(cpu, state);
}

constexpr instruction_state RTS(cpu_state& cpu, instruction_state state) noexcept {
    return return_from_subroutine(cpu, state);
}

template <addressing_mode Mode>
constexpr instruction_state SBC(cpu_state& cpu, instruction_state state) noexcept {
    return internal_execution_on_memory_data<Mode>(cpu, state, sbc_impl);
}

constexpr instruction_state SEC(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state, [](cpu_state& cpu) { cpu.p.carry = true; });
}

constexpr instruction_state SED(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state, [](cpu_state& cpu) { cpu.p.decimal = true; });
}

constexpr instruction_state SEI(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state,
                                   [](cpu_state& cpu) { cpu.p.interrupt_disable = true; });
}

template <addressing_mode Mode>
constexpr instruction_state STA(cpu_state& cpu, instruction_state state) noexcept {
    return store_operation<Mode>(cpu, state, cpu.a);
}
template <addressing_mode Mode>
constexpr instruction_state STX(cpu_state& cpu, instruction_state state) noexcept {
    return store_operation<Mode>(cpu, state, cpu.x);
}
template <addressing_mode Mode>
constexpr instruction_state STY(cpu_state& cpu, instruction_state state) noexcept {
    return store_operation<Mode>(cpu, state, cpu.y);
}

constexpr instruction_state TAX(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state, [](cpu_state& cpu) {
        cpu.x = cpu.a;
        set_negative_zero(cpu, cpu.x);
    });
}

constexpr instruction_state TAY(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state, [](cpu_state& cpu) {
        cpu.y = cpu.a;
        set_negative_zero(cpu, cpu.y);
    });
}

constexpr instruction_state TSX(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state, [](cpu_state& cpu) {
        cpu.x = cpu.s;
        set_negative_zero(cpu, cpu.x);
    });
}

constexpr instruction_state TXA(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state, [](cpu_state& cpu) {
        cpu.a = cpu.x;
        set_negative_zero(cpu, cpu.a);
    });
}

constexpr instruction_state TXS(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state, [](cpu_state& cpu) { cpu.s = cpu.x; });
}

constexpr instruction_state TYA(cpu_state& cpu, instruction_state state) noexcept {
    return single_byte_instruction(cpu, state, [](cpu_state& cpu) {
        cpu.a = cpu.y;
        set_negative_zero(cpu, cpu.a);
    });
}

// clang-format off
constexpr array<instruction, 256> instruction_set{
    BRK,                ORA<indirect_x>,    illegal,            illegal,
    illegal,            ORA<zero_page>,     ASL<zero_page>,     illegal,
    PHP,                ORA<immediate>,     ASL<accumulator>,   illegal,
    illegal,            ORA<absolute>,      ASL<absolute>,      illegal,
    BPL,                ORA<indirect_y>,    illegal,            illegal,
    illegal,            ORA<zero_page_x>,   ASL<zero_page_x>,   illegal,
    CLC,                ORA<absolute_y>,    illegal,            illegal,
    illegal,            ORA<absolute_x>,    ASL<absolute_x>,    illegal,

    JSR,                AND<indirect_x>,    illegal,            illegal,
    BIT<zero_page>,     AND<zero_page>,     ROL<zero_page>,     illegal,
    PLP,                AND<immediate>,     ROL<accumulator>,   illegal,
    BIT<absolute>,      AND<absolute>,      ROL<absolute>,      illegal,
    BMI,                AND<indirect_y>,    illegal,            illegal,
    illegal,            AND<zero_page_x>,   ROL<zero_page_x>,   illegal,
    SEC,                AND<absolute_y>,    illegal,            illegal,
    illegal,            AND<absolute_x>,    ROL<absolute_x>,    illegal,

    RTI,                EOR<indirect_x>,    illegal,            illegal,
    illegal,            EOR<zero_page>,     LSR<zero_page>,     illegal,
    PHA,                EOR<immediate>,     LSR<accumulator>,   illegal,
    JMP<absolute>,      EOR<absolute>,      LSR<absolute>,      illegal,
    BVC,                EOR<indirect_y>,    illegal,            illegal,
    illegal,            EOR<zero_page_x>,   LSR<zero_page_x>,   illegal,
    CLI,                EOR<absolute_y>,    illegal,            illegal,
    illegal,            EOR<absolute_x>,    LSR<absolute_x>,    illegal,

    RTS,                ADC<indirect_x>,    illegal,            illegal,
    illegal,            ADC<zero_page>,     ROR<zero_page>,     illegal,
    PLA,                ADC<immediate>,     ROR<accumulator>,   illegal,
    JMP<indirect>,      ADC<absolute>,      ROR<absolute>,      illegal,
    BVS,                ADC<indirect_y>,    illegal,            illegal,
    illegal,            ADC<zero_page_x>,   ROR<zero_page_x>,   illegal,
    SEI,                ADC<absolute_y>,    illegal,            illegal,
    illegal,            ADC<absolute_x>,    ROR<absolute_x>,    illegal,

    illegal,            STA<indirect_x>,    illegal,            illegal,
    STY<zero_page>,     STA<zero_page>,     STX<zero_page>,     illegal,
    DEY,                illegal,            TXA,                illegal,
    STY<absolute>,      STA<absolute>,      STX<absolute>,      illegal,
    BCC,                STA<indirect_y>,    illegal,            illegal,
    STY<zero_page_x>,   STA<zero_page_x>,   STX<zero_page_y>,   illegal,
    TYA,                STA<absolute_y>,    TXS,                illegal,
    illegal,            STA<absolute_x>,    illegal,            illegal,

    LDY<immediate>,     LDA<indirect_x>,    LDX<immediate>,     illegal,
    LDY<zero_page>,     LDA<zero_page>,     LDX<zero_page>,     illegal,
    TAY,                LDA<immediate>,     TAX,                illegal,
    LDY<absolute>,      LDA<absolute>,      LDX<absolute>,      illegal,
    BCS,                LDA<indirect_y>,    illegal,            illegal,
    LDY<zero_page_x>,   LDA<zero_page_x>,   LDX<zero_page_y>,   illegal,
    CLV,                LDA<absolute_y>,    TSX,                illegal,
    LDY<absolute_x>,    LDA<absolute_x>,    LDX<absolute_y>,    illegal,

    CPY<immediate>,     CMP<indirect_x>,    illegal,            illegal,
    CPY<zero_page>,     CMP<zero_page>,     DEC<zero_page>,     illegal,
    INY,                CMP<immediate>,     DEX,                illegal,
    CPY<absolute>,      CMP<absolute>,      DEC<absolute>,      illegal,
    BNE,                CMP<indirect_y>,    illegal,            illegal,
    illegal,            CMP<zero_page_x>,   DEC<zero_page_x>,   illegal,
    CLD,                CMP<absolute_y>,    illegal,            illegal,
    illegal,            CMP<absolute_x>,    DEC<absolute_x>,    illegal,

    CPX<immediate>,     SBC<indirect_x>,    illegal,            illegal,
    CPX<zero_page>,     SBC<zero_page>,     INC<zero_page>,     illegal,
    INX,                SBC<immediate>,     NOP,                illegal,
    CPX<absolute>,      SBC<absolute>,      INC<absolute>,      illegal,
    BEQ,                SBC<indirect_y>,    illegal,            illegal,
    illegal,            SBC<zero_page_x>,   INC<zero_page_x>,   illegal,
    SED,                SBC<absolute_y>,    illegal,            illegal,
    illegal,            SBC<absolute_x>,    INC<absolute_x>,    illegal,
};
// clang-format on

instruction_state step(cpu_state& cpu, instruction_state state) noexcept {
    // reset handling, probably wrong
    if (cpu.reset) {
        cpu = cpu_state{.reset_pending = true};
        return state;
    }

    // nmi handling, probably wrong
    static bool last_nmi = false;
    if (cpu.nmi && !last_nmi) {
        cpu.nmi_pending = true;
    }
    last_nmi = cpu.nmi;

    // irq handling, probably wrong
    if (cpu.irq && !cpu.p.interrupt_disable) {
        cpu.irq_pending = true;
    }

    if (cpu.sync) {
        if (cpu.reset_pending || cpu.nmi_pending || cpu.irq_pending) {
            cpu.instruction_register = 0x00; // inject BRK instruction
        } else {
            cpu.instruction_register = cpu.data_bus;
            cpu.pc++;
        }
    }

    // default assignments for every cycle
    cpu.rw = data_dir::read;
    cpu.sync = false;

    return instruction_set[cpu.instruction_register](cpu, state);
}

} // namespace nes
