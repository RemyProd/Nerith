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

// Ecrit le resultat d'une comparaison avec signe (32 bits) dans le champ crf de CR.
void set_cr_field(Cpu& cpu, std::uint8_t crf, std::int32_t a, std::int32_t b) {
    const std::uint32_t bits = a < b ? 8u : (a > b ? 4u : 2u);   // LT, GT ou EQ
    const unsigned shift = 4u * (7u - crf);
    cpu.cr = (cpu.cr & ~(0xFu << shift)) | (bits << shift);
}

// Les 32 bits de poids faible d'un registre, vus comme un nombre avec signe.
std::int32_t low32(std::uint64_t value) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(value));
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

            case Kind::Cmpwi:
                set_cr_field(cpu, i.crf, low32(cpu.gpr[i.ra]), i.imm);
                cpu.pc = next;
                return Status::Running;

            case Kind::Cmpw:
                set_cr_field(cpu, i.crf, low32(cpu.gpr[i.ra]), low32(cpu.gpr[i.rb]));
                cpu.pc = next;
                return Status::Running;

            case Kind::Addis:
                cpu.gpr[i.rd] = base_value(cpu, i.ra) +
                                static_cast<std::uint64_t>(static_cast<std::int64_t>(i.imm) * 65536);
                cpu.pc = next;
                return Status::Running;

            case Kind::Sub:   // rD = rB - rA (les champs du mot sont dans cet ordre)
                cpu.gpr[i.rd] = cpu.gpr[i.rb] - cpu.gpr[i.ra];
                cpu.pc = next;
                return Status::Running;

            case Kind::Mullw: {
                // produit des 32 bits de poids faible (avec signe) ; on garde 32 bits, etendus avec le signe
                const std::int64_t product = static_cast<std::int64_t>(low32(cpu.gpr[i.ra])) *
                                             static_cast<std::int64_t>(low32(cpu.gpr[i.rb]));
                cpu.gpr[i.rd] = static_cast<std::uint64_t>(
                    static_cast<std::int64_t>(low32(static_cast<std::uint64_t>(product))));
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Or:   // rA = rS | rB
                cpu.gpr[i.ra] = cpu.gpr[i.rd] | cpu.gpr[i.rb];
                cpu.pc = next;
                return Status::Running;

            case Kind::Lbz: {
                const std::uint64_t address = add_signed(base_value(cpu, i.ra), i.imm);
                const std::uint8_t value = memory.read8(static_cast<std::size_t>(address));
                cpu.gpr[i.rd] = value;
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Stb: {
                const std::uint64_t address = add_signed(base_value(cpu, i.ra), i.imm);
                memory.write8(static_cast<std::size_t>(address), static_cast<std::uint8_t>(cpu.gpr[i.rd]));
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
