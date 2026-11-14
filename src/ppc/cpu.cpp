#include "nerith/ppc/cpu.h"

#include <stdexcept>

#include "nerith/ppc/decoder.h"

namespace nerith::ppc {

namespace {

// Valeur de rA dans addi, lwz et stw : si le champ vaut 0, c'est le NOMBRE 0 (pas le registre r0).
std::uint64_t base_value(const Cpu& cpu, std::uint8_t ra) {
    return ra == 0 ? 0 : cpu.gpr[ra];
}

// Ajoute un deplacement avec signe a une valeur de 64 bits (le calcul "boucle" si ca depasse).
std::uint64_t add_signed(std::uint64_t value, std::int32_t offset) {
    return value + static_cast<std::uint64_t>(static_cast<std::int64_t>(offset));
}

}  // namespace

Status step(Cpu& cpu, Memory& memory) {
    try {
        const Instruction i = decode(memory.read32(static_cast<std::size_t>(cpu.pc)));
        const std::uint64_t next = cpu.pc + 4;

        switch (i.kind) {
            case Kind::Addi:
                cpu.gpr[i.rd] = add_signed(base_value(cpu, i.ra), i.imm);
                cpu.pc = next;
                return Status::Running;

            case Kind::Add:
                cpu.gpr[i.rd] = cpu.gpr[i.ra] + cpu.gpr[i.rb];
                cpu.pc = next;
                return Status::Running;

            case Kind::Ori:   // rA = rS | constante
                cpu.gpr[i.ra] = cpu.gpr[i.rd] | static_cast<std::uint64_t>(i.imm);
                cpu.pc = next;
                return Status::Running;

            case Kind::Cmpwi: {
                // comparaison avec signe sur les 32 bits de poids faible de rA
                const std::int32_t a = static_cast<std::int32_t>(static_cast<std::uint32_t>(cpu.gpr[i.ra]));
                const std::uint32_t bits = a < i.imm ? 8u : (a > i.imm ? 4u : 2u);
                const unsigned shift = 4u * (7u - i.crf);
                cpu.cr = (cpu.cr & ~(0xFu << shift)) | (bits << shift);
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Bc: {
                const bool bit = ((cpu.cr >> (31u - i.bi)) & 1u) != 0;
                const bool taken = (i.bo == 12) ? bit : !bit;
                cpu.pc = taken ? add_signed(cpu.pc, i.imm) : next;
                return Status::Running;
            }

            case Kind::Lwz: {
                const std::uint64_t address = add_signed(base_value(cpu, i.ra), i.imm);
                // on lit d'abord : si ca echoue, rien n'a change
                const std::uint32_t value = memory.read32(static_cast<std::size_t>(address));
                cpu.gpr[i.rd] = value;   // les 32 bits de poids fort sont mis a 0
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Stw: {
                const std::uint64_t address = add_signed(base_value(cpu, i.ra), i.imm);
                memory.write32(static_cast<std::size_t>(address),
                               static_cast<std::uint32_t>(cpu.gpr[i.rd]));   // 32 bits de poids faible
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::B: {
                const std::uint64_t target =
                    i.absolute ? static_cast<std::uint64_t>(static_cast<std::uint32_t>(i.imm))
                               : add_signed(cpu.pc, i.imm);
                if (i.link) {
                    cpu.lr = next;
                }
                cpu.pc = target;
                return Status::Running;
            }

            case Kind::Blr:
                if (cpu.lr == 0) {
                    return Status::Halted;   // pc reste sur le blr
                }
                cpu.pc = cpu.lr;
                return Status::Running;

            case Kind::Unknown:
                break;
        }
        return Status::InvalidInstruction;
    } catch (const std::out_of_range&) {
        return Status::OutOfBounds;
    }
}

Status run(Cpu& cpu, Memory& memory, std::size_t max_steps) {
    for (std::size_t count = 0; count < max_steps; count++) {
        const Status status = step(cpu, memory);
        if (status != Status::Running) {
            return status;
        }
    }
    return Status::StepLimit;
}

}  // namespace nerith::ppc
