#include "cpu/cpu.hpp"
#include "cpu/instructions.hpp"
#include "types.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <spdlog/spdlog.h>
#include <thread>

#include <spdlog/fmt/bin_to_hex.h>

namespace fs = std::filesystem;
using namespace nes;

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

enum class mapper_id { nrom };

struct rom_header_info {
    u16 prg_rom_size{};
    u16 chr_rom_size{};
    mapper_id mapper{};
    // TODO PRG RAM
};

std::optional<rom_header_info> read_header(array<u8, 16> const& header) {
    constexpr std::string_view ines_format{"NES\x1a"};

    if (!std::equal(begin(header), begin(header) + 4, begin(ines_format))) {
        return {};
    }

    const u16 prg_rom_size = header[4] * 16 * 1024;
    const u16 chr_rom_size = header[5] * 8 * 1024;
    const auto mapper = static_cast<mapper_id>((header[6] >> 4) | (header[7] & 0xf0));

    return rom_header_info{prg_rom_size, chr_rom_size, mapper};
}

int main(int argc, char** argv) {
    spdlog::set_pattern("%^%v%$");

    fs::path rom_file{argc > 1 ? argv[1] : "smb.nes"};
    spdlog::info("ROM: {}, file size: {} bytes", fs::absolute(rom_file).string(),
                 fs::file_size(rom_file));

    std::ifstream rom{rom_file, std::ios::binary};
    if (!rom) {
        spdlog::critical("Could not open {}", argv[1]);
        std::exit(EXIT_FAILURE);
    }

    array<u8, 16> header{};
    rom.read(reinterpret_cast<char*>(header.data()), header.size());
    spdlog::info("header: {}", spdlog::to_hex(header));

    const auto header_info = read_header(header);
    if (!header_info) {
        spdlog::critical("Unsupported ROM header format");
        std::exit(EXIT_FAILURE);
    }

    spdlog::info("prg rom: {} bytes, chr rom: {} bytes, mapper: {}", header_info->prg_rom_size,
                 header_info->chr_rom_size, header_info->mapper);

    if (header_info->mapper != mapper_id::nrom) {
        spdlog::critical("Unsupported mapper");
        std::exit(EXIT_FAILURE);
    }

    array<u8, 2048> ram{};
    picture_processing_unit ppu;

    cartridge cart;
    cart.prg_ram.resize(8192);
    cart.prg_rom.resize(header_info->prg_rom_size);
    rom.read(reinterpret_cast<char*>(cart.prg_rom.data()), header_info->prg_rom_size);

    nes_memory_map memory{ram, ppu, cart};
    cpu_state cpu{.reset_pending = true};
    instruction_state state{fetching_address{}};

    spdlog::info("  ab   db pc   a  x  y  s  ir");

    while (true) {
        spdlog::info("- {:04x} {:02x} {:04x} {:02x} {:02x} {:02x} {:02x} {:02x} {}",
                     cpu.address_bus, cpu.data_bus, cpu.pc, cpu.a, cpu.x, cpu.y, cpu.s,
                     cpu.instruction_register, instruction_names[cpu.instruction_register]);

        // std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (cpu.rw == data_dir::read) {
            cpu.data_bus = memory[cpu.address_bus];
        } else {
            memory[cpu.address_bus] = cpu.data_bus;
        }
        /*
        spdlog::info("  {:04x} {:02x} {:04x} {:02x} {:02x} {:02x} {:02x} {:02x} {}",
                     cpu.address_bus, cpu.data_bus, cpu.pc, cpu.a, cpu.x, cpu.y, cpu.s,
                     cpu.instruction_register, instruction_names[cpu.instruction_register]);
        */
        // std::this_thread::sleep_for(std::chrono::milliseconds(10));

        state = step(cpu, state);
    }
}
