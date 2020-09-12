#include "oam_dma.hpp"
#include <catch2/catch.hpp>

using namespace nes;

TEST_CASE("oam_dma") {
    cpu_state cpu{.address_bus = 0x1234};
    auto state = std::make_optional<oam_dma_state>(u8{0x04}, false);

    // dummy cycle
    state = step(cpu, *state);
    CHECK(cpu.rw == data_dir::read);
    CHECK(cpu.address_bus == 0x1234);

    // dummy cycle on odd cpu cycle
    state = step(cpu, *state);
    CHECK(cpu.rw == data_dir::read);
    CHECK(cpu.address_bus == 0x1234);

    unsigned int loop_count = 0;
    while (state.has_value()) {
        // read cycle
        state = step(cpu, *state);
        CHECK(cpu.rw == data_dir::read);
        CHECK(cpu.address_bus == (0x0400 + loop_count));

        auto const data = static_cast<u8>(0x42 + loop_count);
        cpu.data_bus = data;

        // write cycle
        state = step(cpu, *state);
        CHECK(cpu.rw == data_dir::write);
        CHECK(cpu.address_bus == 0x2004); // oamdata
        CHECK(cpu.data_bus == data);

        loop_count++;
    }
    CHECK(loop_count == 256);
}
