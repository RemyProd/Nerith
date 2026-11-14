#include "nerith/toy/disassembler.h"

#include <cstdint>
#include <iomanip>
#include <sstream>

#include "nerith/toy/cpu.h"

namespace nerith {

namespace {

std::string reg_name(std::uint8_t reg) {
    return "r" + std::to_string(static_cast<int>(reg));
}

}  // namespace

std::string disassemble(const Memory& memory, std::size_t address) {
    const std::uint8_t opcode = memory.read8(address);
    const std::uint8_t reg    = memory.read8(address + 1);
    const std::uint8_t value  = memory.read8(address + 2);

    const int number = static_cast<int>(value);

    switch (static_cast<Op>(opcode)) {
        case Op::Set:   return "Set " + reg_name(reg) + ", " + std::to_string(number);
        case Op::Add:   return "Add " + reg_name(reg) + ", " + reg_name(value);
        case Op::Sub:   return "Sub " + reg_name(reg) + ", " + reg_name(value);
        case Op::Mul:   return "Mul " + reg_name(reg) + ", " + reg_name(value);
        case Op::Inc:   return "Inc " + reg_name(reg);
        case Op::Dec:   return "Dec " + reg_name(reg);
        case Op::Load:  return "Load " + reg_name(reg) + ", [" + std::to_string(number) + "]";
        case Op::Store: return "Store " + reg_name(reg) + ", [" + std::to_string(number) + "]";
        case Op::Jmp:   return "Jmp " + std::to_string(number);
        case Op::Jnz:   return "Jnz " + reg_name(reg) + ", " + std::to_string(number);
        case Op::Halt:  return "Halt";
        default:
            break;
    }

    std::ostringstream out;
    out << "??? 0x" << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(opcode);
    return out.str();
}

std::string disassemble_range(const Memory& memory, std::size_t address, std::size_t count) {
    std::ostringstream out;
    for (std::size_t i = 0; i < count; i++) {
        const std::size_t current = address + i * 3;
        out << std::setw(4) << std::setfill('0') << current << "  "
            << disassemble(memory, current) << "\n";
    }
    return out.str();
}

}  // namespace nerith
