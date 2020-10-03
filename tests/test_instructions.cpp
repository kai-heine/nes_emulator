#include <catch2/catch.hpp>
#include <cpu/instructions.hpp>
#include <fmt/format.h>

using namespace nes;

TEST_CASE("single_byte_instruction", "[instructions]") {
    cpu_state cpu{.pc = 0};
    instruction_state state{fetching_address{}};
    constexpr auto op = [](cpu_state& cpu) { cpu.a = 0x42; };

    auto prev_cpu = cpu;
    state = single_byte_instruction(cpu, state, op);
    CHECK(cpu.pc == prev_cpu.pc);
    CHECK(cpu.address_bus == cpu.pc);
    CHECK(cpu.a != 0x42);
    CHECK(!cpu.sync);

    prev_cpu = cpu;
    state = single_byte_instruction(cpu, state, op);
    CHECK(cpu.pc == prev_cpu.pc);
    CHECK(cpu.address_bus == prev_cpu.pc);
    CHECK(cpu.sync);
    CHECK(cpu.a == 0x42);
}

TEST_CASE("internal_execution_on_memory_data", "[instructions]") {
    cpu_state cpu{.pc = 0};
    instruction_state state{fetching_address{}};
    constexpr auto op = [](cpu_state& cpu) { cpu.a = 0x42; };

    // testing with immediate addressing to keep it simple
    state = internal_execution_on_memory_data<immediate>(cpu, state, op);
    CHECK(cpu.a != 0x42);
    CHECK(!cpu.sync);

    auto prev_cpu = cpu;
    state = internal_execution_on_memory_data<immediate>(cpu, state, op);
    CHECK(cpu.pc == prev_cpu.pc);
    CHECK(cpu.address_bus == prev_cpu.pc);
    CHECK(cpu.sync);
    CHECK(cpu.a == 0x42);
}

TEST_CASE("store_operation", "[instructions]") {
    cpu_state cpu{.pc = 0};
    instruction_state state{fetching_address{}};
    constexpr u8 store_value = 0x55;

    auto prev_cpu = cpu;
    state = store_operation<zero_page>(cpu, state, store_value);
    CHECK(cpu.address_bus == prev_cpu.pc);
    CHECK(cpu.pc == (prev_cpu.pc + 1));
    CHECK(!cpu.sync);

    cpu.data_bus = 0x42;

    prev_cpu = cpu;
    state = store_operation<zero_page>(cpu, state, store_value);
    CHECK(cpu.pc == prev_cpu.pc);
    CHECK(cpu.address_bus == 0x0042);
    CHECK(cpu.rw == data_dir::write);
    CHECK(cpu.data_bus == store_value);
    CHECK(!cpu.sync);

    prev_cpu = cpu;
    state = store_operation<zero_page>(cpu, state, store_value);
    CHECK(cpu.pc == prev_cpu.pc);
    CHECK(cpu.address_bus == prev_cpu.pc);
    CHECK(cpu.sync);
}

TEST_CASE("read_modify_write", "[instructions]") {
    cpu_state cpu{.pc = 0};
    instruction_state state{fetching_address{}};
    auto op = [](cpu_state&, u8 in) -> u8 {
        CHECK(in == 0x55);
        return 0x42;
    };

    auto prev_cpu = cpu;
    state = read_modify_write<zero_page>(cpu, state, op);
    CHECK(cpu.address_bus == prev_cpu.pc);
    CHECK(cpu.pc == (prev_cpu.pc + 1));
    CHECK(cpu.rw == data_dir::read);
    CHECK(!cpu.sync);

    cpu.data_bus = 0x13;

    prev_cpu = cpu;
    state = read_modify_write<zero_page>(cpu, state, op);
    CHECK(cpu.address_bus == 0x0013);
    CHECK(cpu.rw == data_dir::read);
    CHECK(cpu.pc == prev_cpu.pc);
    CHECK(!cpu.sync);

    cpu.data_bus = 0x55;

    prev_cpu = cpu;
    state = read_modify_write<zero_page>(cpu, state, op);
    CHECK(cpu.address_bus == 0x0013);
    CHECK(cpu.rw != data_dir::read);
    CHECK(cpu.data_bus == 0x55);
    CHECK(cpu.pc == prev_cpu.pc);
    CHECK(!cpu.sync);

    prev_cpu = cpu;
    state = read_modify_write<zero_page>(cpu, state, op);
    CHECK(cpu.address_bus == 0x0013);
    CHECK(cpu.pc == prev_cpu.pc);
    CHECK(cpu.rw != data_dir::read);
    CHECK(!cpu.sync);
    CHECK(cpu.data_bus == 0x42);

    prev_cpu = cpu;
    state = read_modify_write<zero_page>(cpu, state, op);
    CHECK(cpu.pc == prev_cpu.pc);
    CHECK(cpu.address_bus == prev_cpu.pc);
    CHECK(cpu.sync);
}

TEST_CASE("push_operation", "[instructions]") {
    cpu_state cpu{.pc = 0};
    instruction_state state{fetching_address{}};

    u8 const push_value = 0x42;

    auto prev_cpu = cpu;
    state = push_operation(cpu, state, push_value);
    CHECK(cpu.pc == prev_cpu.pc);
    CHECK(cpu.address_bus == cpu.pc);
    CHECK(cpu.rw == data_dir::read);

    prev_cpu = cpu;
    state = push_operation(cpu, state, push_value);
    CHECK(cpu.pc == prev_cpu.pc);
    CHECK(cpu.address_bus == (0x0100 | prev_cpu.s));
    CHECK(cpu.s == static_cast<u8>(prev_cpu.s - 1));
    CHECK(cpu.rw == data_dir::write);
    CHECK(cpu.data_bus == push_value);

    prev_cpu = cpu;
    state = push_operation(cpu, state, push_value);
    CHECK(cpu.sync);
    CHECK(cpu.pc == prev_cpu.pc);
    CHECK(cpu.address_bus == prev_cpu.pc);
}

TEST_CASE("pull_operation", "[instructions]") {
    cpu_state cpu{.pc = 0};
    instruction_state state{fetching_address{}};

    u8 pull_value = 0;

    auto prev_cpu = cpu;
    state = pull_operation(cpu, state, pull_value);
    CHECK(cpu.pc == prev_cpu.pc);
    CHECK(cpu.address_bus == cpu.pc);
    CHECK(pull_value == 0);

    prev_cpu = cpu;
    state = pull_operation(cpu, state, pull_value);
    CHECK(cpu.pc == prev_cpu.pc);
    CHECK(cpu.address_bus == (stack_page | prev_cpu.s));
    CHECK(cpu.s == (prev_cpu.s + 1));
    CHECK(pull_value == 0);

    cpu.data_bus = 0x42;

    prev_cpu = cpu;
    state = pull_operation(cpu, state, pull_value);
    CHECK(cpu.pc == prev_cpu.pc);
    CHECK(cpu.address_bus == (stack_page | prev_cpu.s));
    CHECK(cpu.s == prev_cpu.s);
    CHECK(pull_value == 0);

    cpu.data_bus = 0x53;

    prev_cpu = cpu;
    state = pull_operation(cpu, state, pull_value);
    CHECK(cpu.pc == prev_cpu.pc);
    CHECK(cpu.address_bus == prev_cpu.pc);
    CHECK(pull_value == 0x53);
}

TEST_CASE("jump_to_subroutine", "[instructions]") {
    cpu_state cpu{.pc = 0x0101, .s = 0xff};
    instruction_state state{fetching_address{}};

    auto prev_cpu = cpu;
    state = jump_to_subroutine(cpu, state);
    CHECK(cpu.address_bus == 0x0101);
    CHECK(cpu.pc == 0x0102);
    CHECK(cpu.rw == data_dir::read);

    cpu.data_bus = 0x34; // ADL

    prev_cpu = cpu;
    state = jump_to_subroutine(cpu, state);
    CHECK(cpu.address_bus == 0x01ff);
    CHECK(cpu.rw == data_dir::read);

    prev_cpu = cpu;
    state = jump_to_subroutine(cpu, state);
    CHECK(cpu.address_bus == 0x01ff);
    CHECK(cpu.rw == data_dir::write);
    CHECK(cpu.data_bus == 0x01);
    CHECK(cpu.s == 0xfe);

    cpu.rw = data_dir::read;

    prev_cpu = cpu;
    state = jump_to_subroutine(cpu, state);
    CHECK(cpu.address_bus == 0x01fe);
    CHECK(cpu.rw == data_dir::write);
    CHECK(cpu.data_bus == 0x02);
    CHECK(cpu.s == 0xfd);

    cpu.rw = data_dir::read;

    prev_cpu = cpu;
    state = jump_to_subroutine(cpu, state);
    CHECK(cpu.rw == data_dir::read);
    CHECK(cpu.address_bus == 0x0102);

    cpu.data_bus = 0x12; // ADH

    prev_cpu = cpu;
    state = jump_to_subroutine(cpu, state);
    CHECK(cpu.rw == data_dir::read);
    CHECK(cpu.address_bus == 0x1234);
    CHECK(cpu.pc == 0x1234);
    CHECK(cpu.sync);
}

TEST_CASE("jump_operation", "[instructions]") {
    cpu_state cpu{.pc = 0x0101};
    instruction_state state{fetching_address{}};

    state = jump_operation<absolute>(cpu, state);
    CHECK(cpu.address_bus == 0x0101);

    cpu.data_bus = 0x25;

    state = jump_operation<absolute>(cpu, state);
    CHECK(cpu.address_bus == 0x0102);

    cpu.data_bus = 0x36;

    state = jump_operation<absolute>(cpu, state);
    CHECK(cpu.address_bus == 0x3625);
    CHECK(cpu.pc == 0x3625);
    CHECK(cpu.sync);
}

TEST_CASE("return_from_subroutine", "[instructions]") {
    cpu_state cpu{.pc = 0x0301, .s = 0xfd};
    instruction_state state{fetching_address{}};

    state = return_from_subroutine(cpu, state);
    CHECK(cpu.address_bus == 0x0301);

    state = return_from_subroutine(cpu, state);
    CHECK(cpu.address_bus == 0x01fd);
    CHECK(cpu.s == 0xfe);

    state = return_from_subroutine(cpu, state);
    CHECK(cpu.address_bus == 0x01fe);
    CHECK(cpu.s == 0xff);

    cpu.data_bus = 0x02;

    state = return_from_subroutine(cpu, state);
    CHECK(cpu.address_bus == 0x01ff);

    cpu.data_bus = 0x01;

    state = return_from_subroutine(cpu, state);
    CHECK(cpu.address_bus == 0x0102);
    CHECK(cpu.pc == 0x0103);

    state = return_from_subroutine(cpu, state);
    CHECK(cpu.address_bus == 0x0103);
    CHECK(cpu.pc == 0x0103);
    CHECK(cpu.sync);
}

TEST_CASE("branch_operation", "[instructions]") {
    cpu_state cpu{.pc = 0x0101};
    instruction_state state{fetching_address{}};

    SECTION("branch not taken") {
        constexpr auto dont_branch = [](cpu_state&) { return false; };

        state = branch_operation(cpu, state, dont_branch);
        CHECK(cpu.address_bus == 0x0101);
        CHECK(cpu.pc == 0x0102);

        cpu.data_bus = 0x42;

        state = branch_operation(cpu, state, dont_branch);
        CHECK(cpu.address_bus == 0x0102);
        CHECK(cpu.pc == 0x0102);
        CHECK(cpu.sync);
    }

    SECTION("branch taken") {
        constexpr auto do_branch = [](cpu_state&) { return true; };

        state = branch_operation(cpu, state, do_branch);
        CHECK(cpu.address_bus == 0x0101);
        CHECK(cpu.pc == 0x0102);

        SECTION("no crossing of page boundaries") {
            cpu.data_bus = 0x50; // +50h

            state = branch_operation(cpu, state, do_branch);
            CHECK(cpu.address_bus == 0x0102);
            CHECK(cpu.pc == 0x0152);

            state = branch_operation(cpu, state, do_branch);
            CHECK(cpu.address_bus == 0x0152);
            CHECK(cpu.sync);
            CHECK(cpu.pc == 0x0152);
        }

        SECTION("crossing of page boundary") {
            cpu.data_bus = 0xb0; // -50h

            state = branch_operation(cpu, state, do_branch);
            CHECK(cpu.address_bus == 0x0102);
            CHECK(cpu.pc == 0x01b2);

            state = branch_operation(cpu, state, do_branch);
            CHECK(cpu.address_bus == 0x01b2);
            CHECK(cpu.pc == 0x00b2);

            state = branch_operation(cpu, state, do_branch);
            CHECK(cpu.address_bus == 0x00b2);
            CHECK(cpu.pc == 0x00b2);
            CHECK(cpu.sync);
        }
    }
}

TEST_CASE("interrupt_sequence", "[instructions]") {
    cpu_state cpu{.pc = 0x0101, .s = 0xff, .p = 0x00};
    instruction_state state{fetching_address{}};

    SECTION("brk") {
        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x0101);

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x01ff);
        CHECK(cpu.rw == data_dir::write);
        CHECK(cpu.data_bus == 0x01);

        cpu.rw = data_dir::read;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x01fe);
        CHECK(cpu.rw == data_dir::write);
        CHECK(cpu.data_bus == 0x02);

        cpu.rw = data_dir::read;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x01fd);
        CHECK(cpu.rw == data_dir::write);
        CHECK(cpu.data_bus == 0x30); // b flag and reserved bit set

        cpu.rw = data_dir::read;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == brk_irq_vector);
        CHECK(cpu.rw == data_dir::read);

        cpu.data_bus = 0x34;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == (brk_irq_vector + 1));
        CHECK(cpu.rw == data_dir::read);

        cpu.data_bus = 0x12;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x1234);
        CHECK(cpu.rw == data_dir::read);
        CHECK(cpu.sync);
        CHECK(cpu.pc == 0x1234);
    }

    SECTION("irq") {
        cpu.irq_pending = true;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x0101);

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x01ff);
        CHECK(cpu.rw == data_dir::write);
        CHECK(cpu.data_bus == 0x01);

        cpu.rw = data_dir::read;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x01fe);
        CHECK(cpu.rw == data_dir::write);
        CHECK(cpu.data_bus == 0x01); // pc is not incremented

        cpu.rw = data_dir::read;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x01fd);
        CHECK(cpu.rw == data_dir::write);
        CHECK(cpu.data_bus == 0x20); // only reserved bit set

        cpu.rw = data_dir::read;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == brk_irq_vector);
        CHECK(cpu.rw == data_dir::read);

        cpu.data_bus = 0x34;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == (brk_irq_vector + 1));
        CHECK(cpu.rw == data_dir::read);

        cpu.data_bus = 0x12;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x1234);
        CHECK(cpu.rw == data_dir::read);
        CHECK(cpu.sync);
        CHECK(cpu.pc == 0x1234);
    }

    SECTION("nmi") {
        cpu.nmi_pending = true;
        cpu.irq_pending = true; // nmi should take priority

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x0101);

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x01ff);
        CHECK(cpu.rw == data_dir::write);
        CHECK(cpu.data_bus == 0x01);

        cpu.rw = data_dir::read;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x01fe);
        CHECK(cpu.rw == data_dir::write);
        CHECK(cpu.data_bus == 0x01); // pc is not incremented

        cpu.rw = data_dir::read;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x01fd);
        CHECK(cpu.rw == data_dir::write);
        CHECK(cpu.data_bus == 0x20); // only reserved bit set

        cpu.rw = data_dir::read;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == nmi_vector);
        CHECK(cpu.rw == data_dir::read);

        cpu.data_bus = 0x34;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == (nmi_vector + 1));
        CHECK(cpu.rw == data_dir::read);

        cpu.data_bus = 0x12;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x1234);
        CHECK(cpu.rw == data_dir::read);
        CHECK(cpu.sync);
        CHECK(cpu.pc == 0x1234);
    }

    SECTION("reset") {
        cpu.reset_pending = true;

        // reset should take priority
        cpu.nmi_pending = true;
        cpu.irq_pending = true;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x0101);

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x01ff);
        CHECK(cpu.rw == data_dir::read);

        cpu.rw = data_dir::read;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x01fe);
        CHECK(cpu.rw == data_dir::read);

        cpu.rw = data_dir::read;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x01fd);
        CHECK(cpu.rw == data_dir::read);

        cpu.rw = data_dir::read;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == reset_vector);
        CHECK(cpu.rw == data_dir::read);

        cpu.data_bus = 0x34;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == (reset_vector + 1));
        CHECK(cpu.rw == data_dir::read);

        cpu.data_bus = 0x12;

        state = interrupt_sequence(cpu, state);
        CHECK(cpu.address_bus == 0x1234);
        CHECK(cpu.rw == data_dir::read);
        CHECK(cpu.sync);
        CHECK(cpu.pc == 0x1234);
    }
}

TEST_CASE("return from interrupt", "[instructions]") {
    cpu_state cpu{.pc = 0x0301, .s = 0xfc, .p = 0x00};
    instruction_state state{fetching_address{}};

    state = return_from_interrupt(cpu, state);
    CHECK(cpu.address_bus == 0x0301);
    CHECK(cpu.rw == data_dir::read);

    state = return_from_interrupt(cpu, state);
    CHECK(cpu.address_bus == 0x01fc);
    CHECK(cpu.rw == data_dir::read);
    CHECK(cpu.s == 0xfd);

    state = return_from_interrupt(cpu, state);
    CHECK(cpu.address_bus == 0x01fd);
    CHECK(cpu.rw == data_dir::read);
    CHECK(cpu.s == 0xfe);

    cpu.data_bus = 0xff;

    state = return_from_interrupt(cpu, state);
    CHECK(cpu.address_bus == 0x01fe);
    CHECK(cpu.rw == data_dir::read);
    CHECK(cpu.s == 0xff);
    CHECK(cpu.p == 0xef); // 0xff without b flag

    cpu.data_bus = 0x34;

    state = return_from_interrupt(cpu, state);
    CHECK(cpu.address_bus == 0x01ff);
    CHECK(cpu.rw == data_dir::read);

    cpu.data_bus = 0x12;

    state = return_from_interrupt(cpu, state);
    CHECK(cpu.address_bus == 0x1234);
    CHECK(cpu.rw == data_dir::read);
    CHECK(cpu.pc == 0x1234);
    CHECK(cpu.sync);
}

TEST_CASE("adc", "[instructions]") {
    cpu_state cpu{};

    SECTION("add 2 numbers with carry; no carry generation") {
        cpu.a = 0x0d;
        cpu.data_bus = 0xd3;
        cpu.p.carry = true;
        adc_impl(cpu);
        CHECK(cpu.a == 0xe1);
        CHECK(cpu.p.carry == false);
    }

    SECTION("add 2 numbers with carry; carry generation") {
        cpu.a = 0xfe;
        cpu.data_bus = 0x06;
        cpu.p.carry = true;
        adc_impl(cpu);
        CHECK(cpu.a == 0x05);
        CHECK(cpu.p.carry == true);
    }

    SECTION("add 2 positive numbers with no overflow") {
        cpu.a = 0x05;
        cpu.data_bus = 0x07;
        cpu.p.carry = false;
        adc_impl(cpu);
        CHECK(cpu.a == 0x0c);
        CHECK(cpu.p.carry == false);
        CHECK(cpu.p.overflow == false);
    }

    SECTION("add 2 positive numbers with overflow") {
        cpu.a = 0x7f;
        cpu.data_bus = 0x02;
        cpu.p.carry = false;
        adc_impl(cpu);
        CHECK(cpu.a == 0x81);
        CHECK(cpu.p.carry == false);
        CHECK(cpu.p.overflow == true);
    }

    SECTION("add positive and negative number with positive result") {
        cpu.a = 0x05;
        cpu.data_bus = 0xfd;
        cpu.p.carry = false;
        adc_impl(cpu);
        CHECK(cpu.a == 0x02);
        CHECK(cpu.p.carry == true);
        CHECK(cpu.p.overflow == false);
    }

    SECTION("add positive and negative number with negative result") {
        cpu.a = 0x05;
        cpu.data_bus = 0xf9;
        cpu.p.carry = false;
        adc_impl(cpu);
        CHECK(cpu.a == 0xfe);
        CHECK(cpu.p.carry == false);
        CHECK(cpu.p.overflow == false);
    }

    SECTION("add 2 negative numbers without overflow") {
        cpu.a = 0xfb;
        cpu.data_bus = 0xf9;
        cpu.p.carry = false;
        adc_impl(cpu);
        CHECK(cpu.a == 0xf4);
        CHECK(cpu.p.carry == true);
        CHECK(cpu.p.overflow == false);
    }

    SECTION("add 2 negative numbers with overflow") {
        cpu.a = 0xbe;
        cpu.data_bus = 0xbf;
        cpu.p.carry = false;
        adc_impl(cpu);
        CHECK(cpu.a == 0x7d);
        CHECK(cpu.p.carry == true);
        CHECK(cpu.p.overflow == true);
    }
}

TEST_CASE("sbc", "[instructions]") {
    cpu_state cpu{};
    cpu.p.carry = true; // "no borrow"

    SECTION("subtract 2 numbers with borrow; positive result") {
        cpu.data_bus = 0x03;
        cpu.a = 0x05;
        sbc_impl(cpu);
        CHECK(cpu.a == 0x02);
        CHECK(cpu.p.carry == true);
    }

    SECTION("subtract 2 numbers with borrow; negative result") {
        cpu.data_bus = 0x06;
        cpu.a = 0x05;
        sbc_impl(cpu);
        CHECK(cpu.a == 0xff);
        CHECK(cpu.p.carry == false);
    }

    // *************************************

    SECTION("unsigned borrow but no signed overflow") {
        cpu.a = 0x50;
        cpu.data_bus = 0xf0;
        sbc_impl(cpu);
        CHECK(cpu.a == 0x60);
        CHECK(cpu.p.carry == false);
        CHECK(cpu.p.overflow == false);

        cpu.a = 0x50;
        cpu.data_bus = 0x70;
        cpu.p.carry = true;
        sbc_impl(cpu);
        CHECK(cpu.a == 0xe0);
        CHECK(cpu.p.carry == false);
        CHECK(cpu.p.overflow == false);

        cpu.a = 0xd0;
        cpu.data_bus = 0xf0;
        cpu.p.carry = true;
        sbc_impl(cpu);
        CHECK(cpu.a == 0xe0);
        CHECK(cpu.p.carry == false);
        CHECK(cpu.p.overflow == false);
    }

    SECTION("unsigned borrow and signed overflow") {
        cpu.a = 0x50;
        cpu.data_bus = 0xb0;
        sbc_impl(cpu);
        CHECK(cpu.a == 0xa0);
        CHECK(cpu.p.carry == false);
        CHECK(cpu.p.overflow == true);
    }

    SECTION("no unsigned borrow or signed overflow") {
        cpu.a = 0x50;
        cpu.data_bus = 0x30;
        sbc_impl(cpu);
        CHECK(cpu.a == 0x20);
        CHECK(cpu.p.carry == true);
        CHECK(cpu.p.overflow == false);

        cpu.a = 0xd0;
        cpu.data_bus = 0xb0;
        cpu.p.carry = true;
        sbc_impl(cpu);
        CHECK(cpu.a == 0x20);
        CHECK(cpu.p.carry == true);
        CHECK(cpu.p.overflow == false);

        cpu.a = 0xd0;
        cpu.data_bus = 0x30;
        cpu.p.carry = true;
        sbc_impl(cpu);
        CHECK(cpu.a == 0xa0);
        CHECK(cpu.p.carry == true);
        CHECK(cpu.p.overflow == false);
    }

    SECTION("no unsigned borrow but signed overflow") {
        cpu.a = 0xd0;
        cpu.data_bus = 0x70;
        sbc_impl(cpu);
        CHECK(cpu.a == 0x60);
        CHECK(cpu.p.carry == true);
        CHECK(cpu.p.overflow == true);
    }
}

TEST_CASE("asl", "[instructions]") {
    cpu_state cpu{};
    cpu.p.carry = false;

    auto result = asl_impl(cpu, 0b10001000);
    CHECK(result == 0b00010000);
    CHECK(cpu.p.carry == true);

    result = asl_impl(cpu, 0b00010000);
    CHECK(result == 0b00100000);
    CHECK(cpu.p.carry == false);
}

TEST_CASE("rol", "[instructions]") {
    cpu_state cpu{};
    cpu.p.carry = false;

    auto result = rol_impl(cpu, 0b10001000);
    CHECK(result == 0b00010000);
    CHECK(cpu.p.carry == true);

    result = rol_impl(cpu, 0b00010000);
    CHECK(result == 0b00100001);
    CHECK(cpu.p.carry == false);
}

TEST_CASE("lsr", "[instructions]") {
    cpu_state cpu{};
    cpu.p.carry = false;

    auto result = lsr_impl(cpu, 0b00010001);
    CHECK(result == 0b00001000);
    CHECK(cpu.p.carry == true);

    result = lsr_impl(cpu, 0b00010000);
    CHECK(result == 0b00001000);
    CHECK(cpu.p.carry == false);
}

TEST_CASE("ror", "[instructions]") {
    cpu_state cpu{};
    cpu.p.carry = false;

    auto result = ror_impl(cpu, 0b00010001);
    CHECK(result == 0b00001000);
    CHECK(cpu.p.carry == true);

    result = ror_impl(cpu, 0b00010000);
    CHECK(result == 0b10001000);
    CHECK(cpu.p.carry == false);
}
