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

// Masque de 64 bits avec des 1 du bit b au bit e (le bit 0 est le bit de poids FORT, comme dans les manuels).
// Si b > e, le masque "fait le tour" : des 1 de b a 63 et de 0 a e.
std::uint64_t mask64(unsigned b, unsigned e) {
    const std::uint64_t from_b = ~std::uint64_t{0} >> b;
    const std::uint64_t to_e = ~std::uint64_t{0} << (63 - e);
    return b <= e ? (from_b & to_e) : (from_b | to_e);
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

            case Kind::Rlwinm: {
                // d'apres le manuel : on rotate les 32 bits de poids faible de rS (dupliques sur 64 bits),
                // puis on garde les bits du masque MB..ME (numerotes dans les 32 bits de poids faible)
                const std::uint64_t low = cpu.gpr[i.rd] & 0xFFFFFFFFull;
                const std::uint64_t doubled = (low << 32) | low;
                const unsigned n = i.sh;
                const std::uint64_t rotated = n == 0 ? doubled : ((doubled << n) | (doubled >> (64 - n)));
                cpu.gpr[i.ra] = rotated & mask64(i.mb + 32u, i.me + 32u);
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Or:   // rA = rS | rB
                cpu.gpr[i.ra] = cpu.gpr[i.rd] | cpu.gpr[i.rb];
                cpu.pc = next;
                return Status::Running;

            case Kind::Stwu: {
                if (i.ra == 0) {
                    break;   // forme invalide : rA ne peut pas etre 0
                }
                const std::uint64_t address = add_signed(cpu.gpr[i.ra], i.imm);
                // on ecrit d'abord : si ca echoue, rA n'a pas change
                memory.write32(static_cast<std::size_t>(address), static_cast<std::uint32_t>(cpu.gpr[i.rd]));
                cpu.gpr[i.ra] = address;
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Mflr:
                cpu.gpr[i.rd] = cpu.lr;
                cpu.pc = next;
                return Status::Running;

            case Kind::Mtlr:
                cpu.lr = cpu.gpr[i.rd];
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
                bool taken = false;
                if (i.bo == 16 || i.bo == 18) {   // bdnz, bdz : on decremente CTR, puis on le teste
                    cpu.ctr -= 1;
                    taken = (i.bo == 16) ? (cpu.ctr != 0) : (cpu.ctr == 0);
                } else {
                    const bool bit = ((cpu.cr >> (31u - i.bi)) & 1u) != 0;
                    taken = (i.bo == 12) ? bit : !bit;
                }
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

            case Kind::Blr: {
                if (!i.link && cpu.lr == 0) {
                    return Status::Halted;   // pc reste sur le blr
                }
                const std::uint64_t target = cpu.lr;
                if (i.link) {
                    cpu.lr = next;   // blrl : l'adresse de retour du nouvel appel
                }
                cpu.pc = target;
                return Status::Running;
            }

            case Kind::Bctr: {
                const std::uint64_t target = cpu.ctr & ~std::uint64_t{3};
                if (i.link) {
                    cpu.lr = next;
                }
                cpu.pc = target;
                return Status::Running;
            }

            case Kind::Mfctr:
                cpu.gpr[i.rd] = cpu.ctr;
                cpu.pc = next;
                return Status::Running;

            case Kind::Mtctr:
                cpu.ctr = cpu.gpr[i.rd];
                cpu.pc = next;
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
