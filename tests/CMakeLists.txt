find_package(Catch2 CONFIG REQUIRED)

enable_testing()

add_executable(tests
    test_main.cpp
    test_addressing_modes.cpp
    test_instructions.cpp
)
target_link_libraries(tests PRIVATE
    nes_emulator_lib
    Catch2::Catch2
)

add_test(
    NAME nes_emulator_tests
    COMMAND tests
)
