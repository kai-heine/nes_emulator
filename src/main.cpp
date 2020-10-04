#include "controller.hpp"
#include "cpu/cpu.hpp"
#include "cpu/instructions.hpp"
#include "oam_dma.hpp"
#include "types.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <spdlog/spdlog.h>

#include <spdlog/async.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "sdl_helper.hpp"
#include <SDL2/SDL.h>

namespace fs = std::filesystem;
using namespace nes;

// TODO: load colors from .pal file
constexpr array<SDL_Color, 64> nes_color_palette{{
    {84, 84, 84, 255},    {0, 30, 116, 255},    {8, 16, 144, 255},    {48, 0, 136, 255},
    {68, 0, 100, 255},    {92, 0, 48, 255},     {84, 4, 0, 255},      {60, 24, 0, 255},
    {32, 42, 0, 255},     {8, 58, 0, 255},      {0, 64, 0, 255},      {0, 60, 0, 255},
    {0, 50, 60, 255},     {0, 0, 0, 255},       {0, 0, 0, 255},       {0, 0, 0, 255}, //
    {152, 150, 152, 255}, {8, 76, 196, 255},    {48, 50, 236, 255},   {92, 30, 228, 255},
    {136, 20, 176, 255},  {160, 20, 100, 255},  {152, 34, 32, 255},   {120, 60, 0, 255},
    {84, 90, 0, 255},     {40, 114, 0, 255},    {8, 124, 0, 255},     {0, 118, 40, 255},
    {0, 102, 120, 255},   {0, 0, 0, 255},       {0, 0, 0, 255},       {0, 0, 0, 255}, //
    {236, 238, 236, 255}, {76, 154, 236, 255},  {120, 124, 236, 255}, {176, 98, 236, 255},
    {228, 84, 236, 255},  {236, 88, 180, 255},  {236, 106, 100, 255}, {212, 136, 32, 255},
    {160, 170, 0, 255},   {116, 196, 0, 255},   {76, 208, 32, 255},   {56, 204, 108, 255},
    {56, 180, 204, 255},  {60, 60, 60, 255},    {0, 0, 0, 255},       {0, 0, 0, 255}, //
    {236, 238, 236, 255}, {168, 204, 236, 255}, {188, 188, 236, 255}, {212, 178, 236, 255},
    {236, 174, 236, 255}, {236, 174, 212, 255}, {236, 180, 176, 255}, {228, 196, 144, 255},
    {204, 210, 120, 255}, {180, 222, 120, 255}, {168, 226, 144, 255}, {152, 226, 180, 255},
    {160, 214, 228, 255}, {160, 162, 160, 255}, {0, 0, 0, 255},       {0, 0, 0, 255},
}};

constexpr array<std::string_view, 256> instruction_names = {{
    "BRK impl", "ORA X,ind", "---",      "---", "---",       "ORA zpg",   "ASL zpg",   "---",
    "PHP impl", "ORA #",     "ASL A",    "---", "---",       "ORA abs",   "ASL abs",   "---",
    "BPL rel",  "ORA ind,Y", "---",      "---", "---",       "ORA zpg,X", "ASL zpg,X", "---",
    "CLC impl", "ORA abs,Y", "---",      "---", "---",       "ORA abs,X", "ASL abs,X", "---",
    "JSR abs",  "AND X,ind", "---",      "---", "BIT zpg",   "AND zpg",   "ROL zpg",   "---",
    "PLP impl", "AND #",     "ROL A",    "---", "BIT abs",   "AND abs",   "ROL abs",   "---",
    "BMI rel",  "AND ind,Y", "---",      "---", "---",       "AND zpg,X", "ROL zpg,X", "---",
    "SEC impl", "AND abs,Y", "---",      "---", "---",       "AND abs,X", "ROL abs,X", "---",
    "RTI impl", "EOR X,ind", "---",      "---", "---",       "EOR zpg",   "LSR zpg",   "---",
    "PHA impl", "EOR #",     "LSR A",    "---", "JMP abs",   "EOR abs",   "LSR abs",   "---",
    "BVC rel",  "EOR ind,Y", "---",      "---", "---",       "EOR zpg,X", "LSR zpg,X", "---",
    "CLI impl", "EOR abs,Y", "---",      "---", "---",       "EOR abs,X", "LSR abs,X", "---",
    "RTS impl", "ADC X,ind", "---",      "---", "---",       "ADC zpg",   "ROR zpg",   "---",
    "PLA impl", "ADC #",     "ROR A",    "---", "JMP ind",   "ADC abs",   "ROR abs",   "---",
    "BVS rel",  "ADC ind,Y", "---",      "---", "---",       "ADC zpg,X", "ROR zpg,X", "---",
    "SEI impl", "ADC abs,Y", "---",      "---", "---",       "ADC abs,X", "ROR abs,X", "---",
    "---",      "STA X,ind", "---",      "---", "STY zpg",   "STA zpg",   "STX zpg",   "---",
    "DEY impl", "---",       "TXA impl", "---", "STY abs",   "STA abs",   "STX abs",   "---",
    "BCC rel",  "STA ind,Y", "---",      "---", "STY zpg,X", "STA zpg,X", "STX zpg,Y", "---",
    "TYA impl", "STA abs,Y", "TXS impl", "---", "---",       "STA abs,X", "---",       "---",
    "LDY #",    "LDA X,ind", "LDX #",    "---", "LDY zpg",   "LDA zpg",   "LDX zpg",   "---",
    "TAY impl", "LDA #",     "TAX impl", "---", "LDY abs",   "LDA abs",   "LDX abs",   "---",
    "BCS rel",  "LDA ind,Y", "---",      "---", "LDY zpg,X", "LDA zpg,X", "LDX zpg,Y", "---",
    "CLV impl", "LDA abs,Y", "TSX impl", "---", "LDY abs,X", "LDA abs,X", "LDX abs,Y", "---",
    "CPY #",    "CMP X,ind", "---",      "---", "CPY zpg",   "CMP zpg",   "DEC zpg",   "---",
    "INY impl", "CMP #",     "DEX impl", "---", "CPY abs",   "CMP abs",   "DEC abs",   "---",
    "BNE rel",  "CMP ind,Y", "---",      "---", "---",       "CMP zpg,X", "DEC zpg,X", "---",
    "CLD impl", "CMP abs,Y", "---",      "---", "---",       "CMP abs,X", "DEC abs,X", "---",
    "CPX #",    "SBC X,ind", "---",      "---", "CPX zpg",   "SBC zpg",   "INC zpg",   "---",
    "INX impl", "SBC #",     "NOP impl", "---", "CPX abs",   "SBC abs",   "INC abs",   "---",
    "BEQ rel",  "SBC ind,Y", "---",      "---", "---",       "SBC zpg,X", "INC zpg,X", "---",
    "SED impl", "SBC abs,Y", "---",      "---", "---",       "SBC abs,X", "INC abs,X", "---",
}};

enum class mapper_id : u8 { nrom };

struct rom_header_info {
    u16 prg_rom_size{};
    u16 chr_rom_size{};
    mapper_id mapper{};
    mirroring nametable_mirroring{};
    // TODO PRG RAM
};

std::optional<rom_header_info> read_header(array<u8, 16> const& header) {
    constexpr std::string_view ines_format{"NES\x1a"};

    if (!std::equal(begin(header), begin(header) + 4, begin(ines_format))) {
        return {};
    }

    u16 const prg_rom_size = header[4] * 16 * 1024;
    u16 const chr_rom_size = header[5] * 8 * 1024;
    auto const mapper = static_cast<mapper_id>((header[6] >> 4) | (header[7] & 0xf0));
    auto const nametable_mirroring = static_cast<mirroring>(header[6] & 0x01);

    return rom_header_info{prg_rom_size, chr_rom_size, mapper, nametable_mirroring};
}

// TODO button mapping etc.
class game_controller {
  public:
    game_controller() {
        for (int i = 0; i < std::min(SDL_NumJoysticks(), 2); ++i) {
            if (SDL_IsGameController(i)) {
                controllers.emplace_back(sdl::make_scoped(SDL_GameControllerOpen(i)));
            }
        }
    }

    controller_states read_controllers() {

        auto read_controller = [](auto& controller) {
            controller_state state;
            state.a = SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_A) != 0;
            state.b = SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_X) != 0;
            state.select =
                SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_BACK) != 0;
            state.start =
                SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_START) != 0;
            state.up =
                SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
            state.down =
                SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
            state.left =
                SDL_GameControllerGetButton(controller.get(), SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
            state.right = SDL_GameControllerGetButton(controller.get(),
                                                      SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
            return state;
        };

        controller_states states;

        if (controllers.size() > 0) {
            states.joy1 = read_controller(controllers[0]);
        }
        if (controllers.size() > 1) {
            states.joy1 = read_controller(controllers[1]);
        }

        return states;
    }

  private:
    vector<sdl::ptr<SDL_GameController>> controllers;
};

int main(int argc, char** argv) {
    try {
        sdl::initializer sdl_init{SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER};

        auto log = spdlog::create<spdlog::sinks::stdout_color_sink_mt>("nes");
        log->set_pattern("%^%v%$");
        // log->set_level(spdlog::level::debug);

        spdlog::enable_backtrace(10);

        fs::path rom_file{argc > 1 ? argv[1] : "smb.nes"};
        log->info("ROM: {}, file size: {} bytes", fs::absolute(rom_file).string(),
                  fs::file_size(rom_file));

        std::ifstream rom{rom_file, std::ios::binary};
        if (!rom) {
            log->critical("Could not open {}", argv[1]);
            std::exit(EXIT_FAILURE);
        }

        array<u8, 16> header{};
        rom.read(reinterpret_cast<char*>(header.data()), header.size());
        log->debug("header: {}", spdlog::to_hex(header));

        const auto header_info = read_header(header);
        if (!header_info) {
            log->critical("Unsupported ROM header format");
            std::exit(EXIT_FAILURE);
        }

        log->debug("prg rom: {} bytes, chr rom: {} bytes, mapper: {}", header_info->prg_rom_size,
                   header_info->chr_rom_size, header_info->mapper);

        if (header_info->mapper != mapper_id::nrom) {
            log->critical("Unsupported mapper");
            std::exit(EXIT_FAILURE);
        }

        cartridge cart;
        cart.nametable_mirroring = header_info->nametable_mirroring;
        cart.prg_ram.resize(8192);
        cart.prg_rom.resize(header_info->prg_rom_size);
        rom.read(reinterpret_cast<char*>(cart.prg_rom.data()), header_info->prg_rom_size);
        cart.chr_rom.resize(header_info->chr_rom_size);
        rom.read(reinterpret_cast<char*>(cart.chr_rom.data()), header_info->chr_rom_size);

        array<u8, 2048> ram{};

        picture_processing_unit ppu;
        ppu_memory_map video_memory{.cart = cart};

        game_controller controller_manager;
        controller_port controller;
        controller.read_controller = [&] { return controller_manager.read_controllers(); };

        cpu_memory_map memory{ram, ppu, cart, controller};
        cpu_state cpu{.reset_pending = true};
        instruction_state state{fetching_address{}};

        optional<oam_dma_state> oam_dma;

        auto window = sdl::make_scoped(SDL_CreateWindow("NES Emulator", SDL_WINDOWPOS_CENTERED,
                                                        SDL_WINDOWPOS_CENTERED, 256 * 2, 240 * 2,
                                                        SDL_WINDOW_RESIZABLE));
        auto renderer = sdl::make_scoped(SDL_CreateRenderer(
            window.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));

        auto const pixel_format = [&]() {
            SDL_RendererInfo info{};
            SDL_GetRendererInfo(renderer.get(), &info);
            return info.texture_formats[0];
        }();

        // source image with color palette
        auto picture_surface = sdl::make_scoped(SDL_CreateRGBSurfaceWithFormatFrom(
            ppu.get_frame_buffer(), 256, 240, 8, 256, SDL_PIXELFORMAT_INDEX8));
        SDL_SetPaletteColors(picture_surface->format->palette, nes_color_palette.data(), 0,
                             static_cast<int>(nes_color_palette.size()));

        // surface and texture with a more native pixel format for rendering
        auto render_surface = sdl::make_scoped(
            SDL_CreateRGBSurface(0, picture_surface->w, picture_surface->h, 32, 0, 0, 0, 0));
        auto render_texture = sdl::make_scoped(
            SDL_CreateTexture(renderer.get(), pixel_format, SDL_TEXTUREACCESS_STREAMING,
                              picture_surface->w, picture_surface->h));

        log->debug("  ab   db pc   a  x  y  s  ir");

        while (true) {
            while (!ppu.has_frame_buffer()) {
                if (oam_dma) {
                    oam_dma = step(cpu, *oam_dma);
                } else {
                    state = step(cpu, state);
                }

                log->debug("- {:04x} {:02x} {:04x} {:02x} {:02x} {:02x} {:02x} {:02x} {}",
                           cpu.address_bus, cpu.data_bus, cpu.pc, cpu.a, cpu.x, cpu.y, cpu.s,
                           cpu.instruction_register, instruction_names[cpu.instruction_register]);

                memory.set_address(cpu.address_bus);

                if (cpu.rw == data_dir::write) {
                    if (cpu.address_bus == 0x4014) {
                        oam_dma = oam_dma_state(cpu.data_bus, (cpu.cycle_count % 2 == 0));
                    } else {
                        memory.write(cpu.data_bus);
                    }
                }

                for (u8 i = 0; i < 3; i++) {
                    ppu.step();

                    if (ppu.video_memory_access) {
                        if (*ppu.video_memory_access == data_dir::read) {
                            ppu.video_data_bus = video_memory.read(ppu.video_address_bus);
                        } else {
                            video_memory.write(ppu.video_address_bus, ppu.video_data_bus);
                        }
                    }
                }

                cpu.nmi = ppu.nmi;

                if (cpu.rw == data_dir::read) {
                    cpu.data_bus = memory.read();
                }
            }

            SDL_Event e;
            while (SDL_PollEvent(&e) == 1) {
                if (e.type == SDL_QUIT) {
                    return 0;
                }
            }

            // this converts the pixel format
            sdl::checked(
                SDL_BlitSurface(picture_surface.get(), nullptr, render_surface.get(), nullptr));

            {
                // this just copies
                void* pixels{nullptr};
                int pitch{};

                sdl::checked(SDL_LockTexture(render_texture.get(), NULL, &pixels, &pitch));
                sdl::checked(SDL_ConvertPixels(
                    render_surface->w, render_surface->h, render_surface->format->format,
                    render_surface->pixels, render_surface->pitch, pixel_format, pixels, pitch));
                SDL_UnlockTexture(render_texture.get());
            }

            sdl::checked(SDL_RenderCopy(renderer.get(), render_texture.get(), nullptr, nullptr));
            SDL_RenderPresent(renderer.get());
        }
    } catch (std::exception const& e) {
        spdlog::critical("Error: {}", e.what());
        return -1;
    }
    return 0;
}
