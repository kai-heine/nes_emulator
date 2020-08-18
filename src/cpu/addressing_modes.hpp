#ifndef NES_CPU_ADDRESSING_MODES_HPP
#define NES_CPU_ADDRESSING_MODES_HPP

#include "../types.hpp"
#include "cpu.hpp"
#include <cassert>

namespace nes {

struct fetching_address {
    u8 cycle{0}; // cycle of address fetching
    u16 address{0};
};

using addressing_mode = bool (*)(cpu_state&, fetching_address&, bool);

// placeholder function to be used for template specialization.
inline bool accumulator(cpu_state&, fetching_address&, bool) noexcept {
    assert(false);
    return true;
}

constexpr bool immediate(cpu_state& cpu, fetching_address&, bool) noexcept {
    cpu.address_bus = cpu.pc++;
    return true;
}

constexpr bool zero_page_indexed(cpu_state& cpu, fetching_address& state,
                                 std::optional<u8> index) noexcept {
    switch (state.cycle++) {
    case 0: cpu.address_bus = cpu.pc++; return false;
    case 1: cpu.address_bus = cpu.data_bus; return !index;
    case 2: cpu.address_bus = (cpu.address_bus + *index) & 0xff; return true;
    default: assert(false); return true;
    }
}
constexpr bool zero_page(cpu_state& cpu, fetching_address& state, bool) noexcept {
    return zero_page_indexed(cpu, state, std::nullopt);
}
constexpr bool zero_page_x(cpu_state& cpu, fetching_address& state, bool) noexcept {
    return zero_page_indexed(cpu, state, cpu.x);
}
constexpr bool zero_page_y(cpu_state& cpu, fetching_address& state, bool) noexcept {
    return zero_page_indexed(cpu, state, cpu.y);
}

constexpr bool absolute_indexed(cpu_state& cpu, fetching_address& state, bool skip_same_page_cycle,
                                optional<u8> index) noexcept {
    switch (state.cycle++) {
    case 0: {
        cpu.address_bus = cpu.pc++;
        return false;
    }
    case 1: {
        state.address = cpu.data_bus;
        cpu.address_bus = cpu.pc++;
        return false;
    }
    case 2: {
        if (index) {
            u16 const adl = state.address + *index;
            u16 const adh = cpu.data_bus << 8;
            bool const page_boundary_crossed = adl & 0x0100;
            state.address = adh + adl;
            cpu.address_bus = adh | (adl & 0xff);
            return skip_same_page_cycle && !page_boundary_crossed;
        } else {
            cpu.address_bus = (cpu.data_bus << 8) | state.address;
            return true;
        }
    }
    case 3: cpu.address_bus = state.address; return true;
    default: assert(false); return true;
    }
}
constexpr bool absolute(cpu_state& cpu, fetching_address& state,
                        bool skip_same_page_cycle) noexcept {
    return absolute_indexed(cpu, state, skip_same_page_cycle, std::nullopt);
}
constexpr bool absolute_x(cpu_state& cpu, fetching_address& state,
                          bool skip_same_page_cycle) noexcept {
    return absolute_indexed(cpu, state, skip_same_page_cycle, cpu.x);
}
constexpr bool absolute_y(cpu_state& cpu, fetching_address& state,
                          bool skip_same_page_cycle) noexcept {
    return absolute_indexed(cpu, state, skip_same_page_cycle, cpu.y);
}

constexpr bool indirect_x(cpu_state& cpu, fetching_address& state, bool) noexcept {
    switch (state.cycle++) {
    case 0: cpu.address_bus = cpu.pc++; return false;
    case 1: cpu.address_bus = cpu.data_bus; return false;
    case 2: cpu.address_bus = (cpu.address_bus + cpu.x) & 0x00ff; return false;
    case 3: {
        state.address = cpu.data_bus;
        cpu.address_bus = (cpu.address_bus + 1) & 0x00ff;
        return false;
    }
    case 4: cpu.address_bus = (cpu.data_bus << 8) | state.address; return true;
    default: assert(false); return true;
    }
}

constexpr bool indirect_y(cpu_state& cpu, fetching_address& state,
                          bool skip_same_page_cycle) noexcept {
    switch (state.cycle++) {
    case 0: cpu.address_bus = cpu.pc++; return false;
    case 1: cpu.address_bus = cpu.data_bus; return false;
    case 2: {
        cpu.address_bus = (cpu.address_bus + 1) & 0x00ff;
        state.address = cpu.data_bus;
        return false;
    }
    case 3: {
        u16 const adl = state.address + cpu.y;
        u16 const adh = (cpu.data_bus << 8);
        bool const page_boundary_crossed = adl & 0x0100;
        state.address = adh + adl;
        cpu.address_bus = adh | (adl & 0xff);
        return skip_same_page_cycle && !page_boundary_crossed;
    }
    case 4: cpu.address_bus = state.address; return true;
    default: assert(false); return true;
    }
}

constexpr bool indirect(cpu_state& cpu, fetching_address& state, bool) noexcept {
    switch (state.cycle++) {
    case 0: {
        cpu.address_bus = cpu.pc++; // fetch IAL
        return false;
    }
    case 1: {
        cpu.address_bus = cpu.pc;
        state.address = cpu.data_bus; // fetch IAH
        return false;
    }
    case 2: {
        state.address |= (cpu.data_bus << 8u);
        cpu.address_bus = state.address; // fetch ADL
        return false;
    }
    case 3: {
        cpu.address_bus = (state.address & 0xff00u) | ((state.address + 1) & 0x00ffu); // fetch ADH
        state.address = cpu.data_bus;
        return false;
    }
    case 4: {
        cpu.address_bus = (cpu.data_bus << 8) | (state.address & 0x00ffu);
        return true;
    }
    default: assert(false); return true;
    }
}

} // namespace nes

#endif
