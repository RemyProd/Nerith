#include "nerith/cpu.h"

#include <stdexcept>

namespace nerith {

Status step(Cpu& cpu, Memory& memory) {
    try {
        // fetch : lire l'instruction en memoire (les acces sont verifies)
        const std::uint8_t opcode = memory.read8(cpu.pc);
        const std::uint8_t reg    = memory.read8(cpu.pc + 1);
        const std::uint8_t value  = memory.read8(cpu.pc + 2);
        cpu.pc += 3;
        ++cpu.instructions_executed;

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
            case Op::Load:
                cpu.registers.at(reg) = memory.read32(value);
                break;
            case Op::Store:
                memory.write32(value, cpu.registers.at(reg));
                break;
            case Op::Jmp:
                cpu.pc = value;
                break;
            case Op::Jnz:
                if (cpu.registers.at(reg) != 0) {
                    cpu.pc = value;
                }
                break;
            case Op::Halt:
                return Status::Halted;
            default:
                return Status::InvalidInstruction;
        }
        return Status::Running;
    } catch (const std::out_of_range&) {
        return Status::OutOfBounds;
    }
}

Status run(Cpu& cpu, Memory& memory, std::size_t max_steps) {
    while (true) {
        // garde-fou : on ne laisse pas tourner une boucle infinie
        if (cpu.instructions_executed >= max_steps) {
            return Status::StepLimit;
        }

        const Status status = step(cpu, memory);
        if (status != Status::Running) {
            return status;
        }
    }
}

}  // namespace nerith
