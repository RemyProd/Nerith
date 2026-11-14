#include "nerith/cpu.h"

#include <stdexcept>

namespace nerith {

Status run(Cpu& cpu, const Memory& memory) {
    try {
        while (true) {
            // fetch : lire l'instruction en memoire (les acces sont verifies)
            const std::uint8_t opcode = memory.read8(cpu.pc);
            const std::uint8_t reg    = memory.read8(cpu.pc + 1);
            const std::uint8_t value  = memory.read8(cpu.pc + 2);
            cpu.pc += 3;

            // decode + execute
            switch (static_cast<Op>(opcode)) {
                case Op::Set:
                    cpu.registers.at(reg) = value;
                    break;
                case Op::Add:
                    cpu.registers.at(reg) += cpu.registers.at(value);
                    break;
                case Op::Sub:
                    cpu.registers.at(reg) -= cpu.registers.at(value);
                    break;
                case Op::Mul:
                    cpu.registers.at(reg) *= cpu.registers.at(value);
                    break;
                case Op::Inc:
                    cpu.registers.at(reg) += 1;
                    break;
                case Op::Dec:
                    cpu.registers.at(reg) -= 1;
                    break;
                case Op::Halt:
                    return Status::Halted;
                default:
                    return Status::InvalidInstruction;
            }
        }
    } catch (const std::out_of_range&) {
        return Status::OutOfBounds;
    }
}

}  // namespace nerith
