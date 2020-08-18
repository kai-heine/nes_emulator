#ifndef NES_PPU_HPP
#define NES_PPU_HPP

#include "types.hpp"

namespace nes {

struct picture_processing_unit {
    [[nodiscard]] u8& operator[](u16) noexcept {
        // TODO
        static u8 shut_up = 0;
        return shut_up;
    }
};

} // namespace nes

#endif
