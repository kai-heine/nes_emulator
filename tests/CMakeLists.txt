find_package(Catch2 CONFIG REQUIRED)

add_executable(tests
    test_main.cpp
    test_addressing_modes.cpp
    test_instructions.cpp
    test_misc.cpp
)
target_link_libraries(tests PRIVATE
    nes_emulator_lib
    Catch2::Catch2
)

include(Catch)
catch_discover_tests(tests)
