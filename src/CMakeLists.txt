add_library(nes_emulator_lib OBJECT
    cpu/addressing_modes.hpp
    cpu/cpu.hpp
    cpu/instructions.hpp     cpu/instructions.cpp
    apu.hpp                  apu.cpp
    cartridge.hpp
    controller.hpp
    memory.hpp               memory.cpp
    oam_dma.hpp
    ppu.hpp                  ppu.cpp
    types.hpp                types.cpp
)
target_include_directories(nes_emulator_lib PUBLIC ${CMAKE_CURRENT_LIST_DIR})


find_package(spdlog CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)

add_executable(nes_emulator
    main.cpp
    sdl_helper.hpp
)
target_link_libraries(nes_emulator PRIVATE
    nes_emulator_lib
    spdlog::spdlog
    SDL2::SDL2
    SDL2::SDL2main
)
