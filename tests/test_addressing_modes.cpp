#include "cpu/addressing_modes.hpp"
#include <catch2/catch.hpp>

using namespace nes;

constexpr bool _ = true;

TEST_CASE("immediate", "[addressing modes]") {
    cpu_state cpu{.pc = 42};
    fetching_address state;
    auto finished = immediate(cpu, state, _);
    CHECK(finished);
    CHECK(cpu.address_bus == 42);
    CHECK(cpu.pc == 43);
}

TEST_CASE("zero_page", "[addressing modes]") {
    cpu_state cpu{.pc = 42};
    fetching_address state;

    auto finished = zero_page(cpu, state, _);
    CHECK(!finished);
    CHECK(cpu.address_bus == 42);
    CHECK(cpu.pc == 43);

    cpu.data_bus = 34;

    finished = zero_page(cpu, state, _);
    CHECK(finished);
    CHECK(cpu.address_bus == 34);
    CHECK(cpu.pc == 43);
}

TEST_CASE("zero_page_indexed", "[addressing modes]") {
    for (auto zero_page_z : {zero_page_x, zero_page_y}) {
        cpu_state cpu{.pc = 42, .x = 0xff, .y = 0xff};
        fetching_address state;

        auto finished = zero_page_z(cpu, state, _);
        CHECK(!finished);
        CHECK(cpu.address_bus == 42);
        CHECK(cpu.pc == 43);

        cpu.data_bus = 0x01;

        finished = zero_page_z(cpu, state, _);
        CHECK(!finished);
        CHECK(cpu.address_bus == 0x0001);
        CHECK(cpu.pc == 43);

        finished = zero_page_z(cpu, state, _);
        CHECK(finished);
        CHECK(cpu.address_bus == 0x0000);
        CHECK(cpu.pc == 43);
    }
}

TEST_CASE("absolute", "[addressing modes]") {
    cpu_state cpu{.pc = 42};
    fetching_address state;

    auto finished = absolute(cpu, state, _);
    CHECK(!finished);
    CHECK(cpu.address_bus == 42);
    CHECK(cpu.pc == 43);

    cpu.data_bus = 0x34;

    finished = absolute(cpu, state, _);
    CHECK(!finished);
    CHECK(cpu.address_bus == 43);
    CHECK(cpu.pc == 44);

    cpu.data_bus = 0x12;

    finished = absolute(cpu, state, _);
    CHECK(finished);
    CHECK(cpu.address_bus == 0x1234);
    CHECK(cpu.pc == 44);
}

TEST_CASE("absolute_indexed", "[addressing modes]") {
    SECTION("no page boundary cross") {
        for (auto absolute_z : {absolute_x, absolute_y}) {
            cpu_state cpu{.pc = 42, .x = 0x10, .y = 0x10};
            fetching_address state;

            auto finished = absolute_z(cpu, state, true);
            CHECK(!finished);
            CHECK(cpu.address_bus == 42);
            CHECK(cpu.pc == 43);

            cpu.data_bus = 0x34;

            finished = absolute_z(cpu, state, true);
            CHECK(!finished);
            CHECK(cpu.address_bus == 43);
            CHECK(cpu.pc == 44);

            cpu.data_bus = 0x12;

            finished = absolute_z(cpu, state, true);
            CHECK(finished);
            CHECK(cpu.address_bus == 0x1244);
            CHECK(cpu.pc == 44);
        }
    }

    SECTION("page boundary cross") {
        for (auto absolute_z : {absolute_x, absolute_y}) {
            cpu_state cpu{.pc = 42, .x = 0x01, .y = 0x01};
            fetching_address state;

            auto finished = absolute_z(cpu, state, false);
            CHECK(!finished);
            CHECK(cpu.address_bus == 42);
            CHECK(cpu.pc == 43);

            cpu.data_bus = 0xff;

            finished = absolute_z(cpu, state, false);
            CHECK(!finished);
            CHECK(cpu.address_bus == 43);
            CHECK(cpu.pc == 44);

            cpu.data_bus = 0xff;

            finished = absolute_z(cpu, state, false);
            CHECK(!finished);
            CHECK(cpu.address_bus == 0xff00);
            CHECK(cpu.pc == 44);

            finished = absolute_z(cpu, state, false);
            CHECK(finished);
            CHECK(cpu.address_bus == 0x0000);
            CHECK(cpu.pc == 44);
        }
    }
}

TEST_CASE("indirect_x", "[addressing modes]") {
    cpu_state cpu{.pc = 42, .x = 0x01};
    fetching_address state;

    auto finished = indirect_x(cpu, state, _);
    CHECK(!finished);
    CHECK(cpu.address_bus == 42);
    CHECK(cpu.pc == 43);

    cpu.data_bus = 0xff;

    finished = indirect_x(cpu, state, _);
    CHECK(!finished);
    CHECK(cpu.address_bus == 0x00ff);
    CHECK(cpu.pc == 43);

    cpu.data_bus = 0x55;

    finished = indirect_x(cpu, state, _);
    CHECK(!finished);
    CHECK(cpu.address_bus == 0x0000);
    CHECK(cpu.pc == 43);

    cpu.data_bus = 0x34;

    finished = indirect_x(cpu, state, _);
    CHECK(!finished);
    CHECK(cpu.address_bus == 0x0001);
    CHECK(cpu.pc == 43);

    cpu.data_bus = 0x12;

    finished = indirect_x(cpu, state, _);
    CHECK(finished);
    CHECK(cpu.address_bus == 0x1234);
    CHECK(cpu.pc == 43);
}

TEST_CASE("indirect_y", "[addressing modes]") {
    SECTION("no page boundary cross") {
        cpu_state cpu{.pc = 42, .y = 0x10};
        fetching_address state;

        auto finished = indirect_y(cpu, state, true);
        CHECK(!finished);
        CHECK(cpu.address_bus == 42);
        CHECK(cpu.pc == 43);

        cpu.data_bus = 0x30;

        finished = indirect_y(cpu, state, true);
        CHECK(!finished);
        CHECK(cpu.address_bus == 0x30);
        CHECK(cpu.pc == 43);

        cpu.data_bus = 0x34;

        finished = indirect_y(cpu, state, true);
        CHECK(!finished);
        CHECK(cpu.address_bus == 0x31);
        CHECK(cpu.pc == 43);

        cpu.data_bus = 0x12;

        finished = indirect_y(cpu, state, true);
        CHECK(finished);
        CHECK(cpu.address_bus == 0x1244);
        CHECK(cpu.pc == 43);
    }

    SECTION("page boundary cross") {
        cpu_state cpu{.pc = 42, .y = 0x01};
        fetching_address state;

        auto finished = indirect_y(cpu, state, false);
        CHECK(!finished);
        CHECK(cpu.address_bus == 42);
        CHECK(cpu.pc == 43);

        cpu.data_bus = 0x30;

        finished = indirect_y(cpu, state, false);
        CHECK(!finished);
        CHECK(cpu.address_bus == 0x30);
        CHECK(cpu.pc == 43);

        cpu.data_bus = 0xff;

        finished = indirect_y(cpu, state, false);
        CHECK(!finished);
        CHECK(cpu.address_bus == 0x31);
        CHECK(cpu.pc == 43);

        cpu.data_bus = 0xff;

        finished = indirect_y(cpu, state, false);
        CHECK(!finished);
        CHECK(cpu.address_bus == 0xff00);
        CHECK(cpu.pc == 43);

        cpu.data_bus = 0x55;

        finished = indirect_y(cpu, state, false);
        CHECK(finished);
        CHECK(cpu.address_bus == 0x0000);
        CHECK(cpu.pc == 43);
    }
}

TEST_CASE("indirect", "[addressing modes]") {
    cpu_state cpu{.pc = 0x0101};
    fetching_address state{};

    auto finished = indirect(cpu, state, false);
    CHECK(!finished);
    CHECK(cpu.address_bus == 0x0101);
    CHECK(cpu.pc == 0x0102);

    cpu.data_bus = 0x34;

    finished = indirect(cpu, state, false);
    CHECK(!finished);
    CHECK(cpu.address_bus == 0x0102);
    CHECK(cpu.pc == 0x0102);

    cpu.data_bus = 0x12;

    finished = indirect(cpu, state, false);
    CHECK(!finished);
    CHECK(cpu.address_bus == 0x1234);

    cpu.data_bus = 0x55;

    finished = indirect(cpu, state, false);
    CHECK(!finished);
    CHECK(cpu.address_bus == 0x1235);

    cpu.data_bus = 0xaa;

    finished = indirect(cpu, state, false);
    CHECK(finished);
    CHECK(cpu.address_bus == 0xaa55);
}
