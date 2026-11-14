#include "nerith/cpu.h"

#include <stdexcept>

namespace nerith {

Status run(Cpu& cpu, const std::vector<std::uint8_t>& program) {
    std::size_t pc = 0;   // program counter : position dans le programme

    try {
        while (true) {
            // fetch : lire l'instruction (.at() verifie les limites)
            const std::uint8_t opcode = program.at(pc);
            const std::uint8_t reg    = program.at(pc + 1);
            const std::uint8_t value  = program.at(pc + 2);
            pc += 3;

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
