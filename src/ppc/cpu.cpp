#include "nerith/ppc/cpu.h"

#include <cstdint>
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

// Ecrit 4 bits (LT = 8, GT = 4, EQ = 2) dans le champ crf de CR.
void set_cr_bits(Cpu& cpu, std::uint8_t crf, std::uint32_t bits) {
    const unsigned shift = 4u * (7u - crf);
    cpu.cr = (cpu.cr & ~(0xFu << shift)) | (bits << shift);
}

// Ecrit le resultat d'une comparaison avec signe (32 ou 64 bits, deja convertis en 64) dans le champ crf de CR.
void set_cr_field(Cpu& cpu, std::uint8_t crf, std::int64_t a, std::int64_t b) {
    set_cr_bits(cpu, crf, a < b ? 8u : (a > b ? 4u : 2u));   // LT, GT ou EQ
}

// Pareil, mais les deux nombres sont compares SANS signe (cmplw, cmplwi, cmpld, cmpldi).
void set_cr_field_unsigned(Cpu& cpu, std::uint8_t crf, std::uint64_t a, std::uint64_t b) {
    set_cr_bits(cpu, crf, a < b ? 8u : (a > b ? 4u : 2u));
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

std::uint32_t low32u(std::uint64_t value) { return static_cast<std::uint32_t>(value); }

// Un nombre de 32 bits avec signe, etendu sur 64 bits (le signe est recopie dans les 32 bits de poids fort).
std::uint64_t sext32(std::uint32_t value) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(value)));
}

// Rotation a gauche de 64 bits (n de 0 a 63).
std::uint64_t rotl64(std::uint64_t value, unsigned n) {
    return n == 0 ? value : ((value << n) | (value >> (64 - n)));
}

// Rotation des 32 bits de poids faible, comme dans rlwinm : on duplique les 32 bits dans les deux moities.
std::uint64_t rotl32_doubled(std::uint64_t value, unsigned n) {
    const std::uint64_t low = low32u(value);
    return rotl64((low << 32) | low, n);
}

unsigned count_leading_zeros(std::uint64_t value, unsigned bits) {
    unsigned count = 0;
    for (unsigned bit = bits; bit > 0 && ((value >> (bit - 1)) & 1u) == 0; --bit) {
        ++count;
    }
    return count;
}

// Description d'un acces memoire de la table : taille, lecture ou ecriture, etc.
struct MemOp {
    unsigned size;   // 1, 2, 4 ou 8 octets
    bool store;      // ecriture (sinon lecture)
    bool sign;       // lecture : etendre le signe sur 64 bits (lha, lwa) au lieu de completer par des 0
    bool update;     // "u" : rA recoit l'adresse calculee apres l'acces
    bool indexed;    // "x" : adresse = rA + rB (sinon rA + deplacement)
    bool reverse;    // "br" : octets dans l'ordre inverse (little-endian)
};

bool mem_op_of(Kind kind, MemOp& op) {
    switch (kind) {
        //                       taille store  sign  upd   idx   rev
        case Kind::Lwzu:   op = {4, false, false, true,  false, false}; return true;
        case Kind::Lbzu:   op = {1, false, false, true,  false, false}; return true;
        case Kind::Stbu:   op = {1, true,  false, true,  false, false}; return true;
        case Kind::Lhz:    op = {2, false, false, false, false, false}; return true;
        case Kind::Lhzu:   op = {2, false, false, true,  false, false}; return true;
        case Kind::Lha:    op = {2, false, true,  false, false, false}; return true;
        case Kind::Lhau:   op = {2, false, true,  true,  false, false}; return true;
        case Kind::Sth:    op = {2, true,  false, false, false, false}; return true;
        case Kind::Sthu:   op = {2, true,  false, true,  false, false}; return true;
        case Kind::Lwa:    op = {4, false, true,  false, false, false}; return true;
        case Kind::Ldu:    op = {8, false, false, true,  false, false}; return true;
        case Kind::Stdu:   op = {8, true,  false, true,  false, false}; return true;
        case Kind::Lhzx:   op = {2, false, false, false, true,  false}; return true;
        case Kind::Lhax:   op = {2, false, true,  false, true,  false}; return true;
        case Kind::Sthx:   op = {2, true,  false, false, true,  false}; return true;
        case Kind::Lwzux:  op = {4, false, false, true,  true,  false}; return true;
        case Kind::Lbzux:  op = {1, false, false, true,  true,  false}; return true;
        case Kind::Lhzux:  op = {2, false, false, true,  true,  false}; return true;
        case Kind::Lhaux:  op = {2, false, true,  true,  true,  false}; return true;
        case Kind::Stwux:  op = {4, true,  false, true,  true,  false}; return true;
        case Kind::Stbux:  op = {1, true,  false, true,  true,  false}; return true;
        case Kind::Sthux:  op = {2, true,  false, true,  true,  false}; return true;
        case Kind::Lwax:   op = {4, false, true,  false, true,  false}; return true;
        case Kind::Lwaux:  op = {4, false, true,  true,  true,  false}; return true;
        case Kind::Ldx:    op = {8, false, false, false, true,  false}; return true;
        case Kind::Stdx:   op = {8, true,  false, false, true,  false}; return true;
        case Kind::Ldux:   op = {8, false, false, true,  true,  false}; return true;
        case Kind::Stdux:  op = {8, true,  false, true,  true,  false}; return true;
        case Kind::Lwbrx:  op = {4, false, false, false, true,  true};  return true;
        case Kind::Stwbrx: op = {4, true,  false, false, true,  true};  return true;
        case Kind::Lhbrx:  op = {2, false, false, false, true,  true};  return true;
        case Kind::Sthbrx: op = {2, true,  false, false, true,  true};  return true;
        case Kind::Ldbrx:  op = {8, false, false, false, true,  true};  return true;
        case Kind::Stdbrx: op = {8, true,  false, false, true,  true};  return true;
        default: return false;
    }
}

// Execute un acces memoire de la table. Rien ne change si l'acces echoue (exception out_of_range).
Status do_memory(const Instruction& i, const MemOp& op, Cpu& cpu, Memory& memory, std::uint64_t next) {
    // formes "avec mise a jour" : rA ne peut pas valoir 0 ; pour une lecture, rA ne peut pas etre rD non plus
    if (op.update && (i.ra == 0 || (!op.store && i.ra == i.rd))) {
        return Status::InvalidInstruction;
    }
    const std::uint64_t offset =
        op.indexed ? cpu.gpr[i.rb] : static_cast<std::uint64_t>(static_cast<std::int64_t>(i.imm));
    const std::uint64_t address = base_value(cpu, i.ra) + offset;
    const std::size_t where = static_cast<std::size_t>(address);

    if (op.store) {
        // on verifie d'abord que le premier et le dernier octet existent : sinon aucun octet n'est ecrit
        (void)memory.read8(where);
        (void)memory.read8(where + op.size - 1);
        const std::uint64_t value = cpu.gpr[i.rd];
        for (unsigned k = 0; k < op.size; k++) {
            const unsigned shift = 8u * (op.reverse ? k : op.size - 1 - k);   // big-endian, ou inverse
            memory.write8(where + k, static_cast<std::uint8_t>(value >> shift));
        }
    } else {
        std::uint64_t value = 0;
        for (unsigned k = 0; k < op.size; k++) {
            const std::uint64_t byte = memory.read8(where + k);
            if (op.reverse) {
                value |= byte << (8u * k);
            } else {
                value = (value << 8) | byte;
            }
        }
        if (op.sign && op.size < 8) {
            const std::uint64_t sign_bit = std::uint64_t{1} << (8 * op.size - 1);
            value = (value ^ sign_bit) - sign_bit;   // recopie le bit de signe vers la gauche
        }
        cpu.gpr[i.rd] = value;
    }
    if (op.update) {
        cpu.gpr[i.ra] = address;
    }
    cpu.pc = next;
    return Status::Running;
}

// Les calculs de la table (logique, decalages, multiplications, rotations, comparaisons sans signe).
// Renvoie false si l'instruction n'en fait pas partie. Ne touche pas a pc.
bool execute_alu(const Instruction& i, Cpu& cpu) {
    auto& g = cpu.gpr;
    const std::uint64_t s = g[i.rd];   // champ 6-10 : rS pour les instructions logiques, rD sinon
    const std::uint64_t a = g[i.ra];
    const std::uint64_t b = g[i.rb];
    const std::uint64_t uimm = static_cast<std::uint32_t>(i.imm);   // pour les nombres sans signe

    switch (i.kind) {
        case Kind::Mulli:  g[i.rd] = a * static_cast<std::uint64_t>(static_cast<std::int64_t>(i.imm)); return true;
        case Kind::Mulld:  g[i.rd] = a * b; return true;
        case Kind::Mulhw: {
            const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(low32u(a))) *
                                         static_cast<std::int64_t>(static_cast<std::int32_t>(low32u(b)));
            g[i.rd] = sext32(static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32));
            return true;
        }
        case Kind::Mulhwu:
            g[i.rd] = (static_cast<std::uint64_t>(low32u(a)) * low32u(b)) >> 32;
            return true;
        case Kind::Divw: {   // division par 0 ou INT_MIN / -1 : le manuel dit "indefini", Nerith met 0
            const std::int32_t x = static_cast<std::int32_t>(low32u(a));
            const std::int32_t y = static_cast<std::int32_t>(low32u(b));
            const bool undefined = y == 0 || (x == INT32_MIN && y == -1);
            g[i.rd] = undefined ? 0 : sext32(static_cast<std::uint32_t>(x / y));
            return true;
        }
        case Kind::Divwu:
            g[i.rd] = low32u(b) == 0 ? 0 : static_cast<std::uint64_t>(low32u(a) / low32u(b));
            return true;
        case Kind::Divd: {
            const std::int64_t x = static_cast<std::int64_t>(a);
            const std::int64_t y = static_cast<std::int64_t>(b);
            const bool undefined = y == 0 || (x == INT64_MIN && y == -1);
            g[i.rd] = undefined ? 0 : static_cast<std::uint64_t>(x / y);
            return true;
        }
        case Kind::Divdu:  g[i.rd] = b == 0 ? 0 : a / b; return true;
        case Kind::Neg:    g[i.rd] = std::uint64_t{0} - a; return true;

        case Kind::And:    g[i.ra] = s & b; return true;
        case Kind::Andc:   g[i.ra] = s & ~b; return true;
        case Kind::Xor:    g[i.ra] = s ^ b; return true;
        case Kind::Nor:    g[i.ra] = ~(s | b); return true;
        case Kind::Nand:   g[i.ra] = ~(s & b); return true;
        case Kind::Orc:    g[i.ra] = s | ~b; return true;
        case Kind::Eqv:    g[i.ra] = ~(s ^ b); return true;

        case Kind::Slw: {   // decalage de 32 bits : au-dela de 31, tout est perdu
            const unsigned n = static_cast<unsigned>(b & 0x3F);
            g[i.ra] = n > 31 ? 0 : static_cast<std::uint64_t>(low32u(s) << n);
            return true;
        }
        case Kind::Srw: {
            const unsigned n = static_cast<unsigned>(b & 0x3F);
            g[i.ra] = n > 31 ? 0 : static_cast<std::uint64_t>(low32u(s) >> n);
            return true;
        }
        case Kind::Sld: {
            const unsigned n = static_cast<unsigned>(b & 0x7F);
            g[i.ra] = n > 63 ? 0 : s << n;
            return true;
        }
        case Kind::Srd: {
            const unsigned n = static_cast<unsigned>(b & 0x7F);
            g[i.ra] = n > 63 ? 0 : s >> n;
            return true;
        }
        case Kind::Cntlzw: g[i.ra] = count_leading_zeros(low32u(s), 32); return true;
        case Kind::Cntlzd: g[i.ra] = count_leading_zeros(s, 64); return true;
        case Kind::Extsb:  g[i.ra] = static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int8_t>(s))); return true;
        case Kind::Extsh:  g[i.ra] = static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int16_t>(s))); return true;
        case Kind::Extsw:  g[i.ra] = sext32(low32u(s)); return true;

        case Kind::Oris:   g[i.ra] = s | (uimm << 16); return true;
        case Kind::Xori:   g[i.ra] = s ^ uimm; return true;
        case Kind::Xoris:  g[i.ra] = s ^ (uimm << 16); return true;
        case Kind::AndiDot:
        case Kind::AndisDot: {   // le seul "et" avec nombre : il met aussi a jour le champ cr0 (comme un "Rc = 1")
            const std::uint64_t result = s & (i.kind == Kind::AndiDot ? uimm : uimm << 16);
            g[i.ra] = result;
            set_cr_field(cpu, 0, static_cast<std::int64_t>(result), 0);
            return true;
        }

        case Kind::Cmplwi: set_cr_field_unsigned(cpu, i.crf, low32u(a), uimm); return true;
        case Kind::Cmpldi: set_cr_field_unsigned(cpu, i.crf, a, uimm); return true;
        case Kind::Cmplw:  set_cr_field_unsigned(cpu, i.crf, low32u(a), low32u(b)); return true;
        case Kind::Cmpld:  set_cr_field_unsigned(cpu, i.crf, a, b); return true;

        case Kind::Rlwimi: {   // insere dans rA les bits du masque, pris dans rS tourne
            const std::uint64_t mask = mask64(i.mb + 32u, i.me + 32u);
            g[i.ra] = (rotl32_doubled(s, i.sh) & mask) | (g[i.ra] & ~mask);
            return true;
        }
        case Kind::Rlwnm:
            g[i.ra] = rotl32_doubled(s, static_cast<unsigned>(b & 0x1F)) & mask64(i.mb + 32u, i.me + 32u);
            return true;
        case Kind::Rldicl: g[i.ra] = rotl64(s, i.sh) & mask64(i.mb, 63); return true;
        case Kind::Rldicr: g[i.ra] = rotl64(s, i.sh) & mask64(0, i.me); return true;
        case Kind::Rldic:  g[i.ra] = rotl64(s, i.sh) & mask64(i.mb, 63u - i.sh); return true;
        case Kind::Rldimi: {
            const std::uint64_t mask = mask64(i.mb, 63u - i.sh);
            g[i.ra] = (rotl64(s, i.sh) & mask) | (g[i.ra] & ~mask);
            return true;
        }
        default:
            return false;
    }
}

}  // namespace

Status step(Cpu& cpu, Memory& memory) {
    try {
        const Instruction i = decode(memory.read32(static_cast<std::size_t>(cpu.pc)));
        const std::uint64_t next = cpu.pc + 4;

        MemOp mem{};
        if (mem_op_of(i.kind, mem)) {
            return do_memory(i, mem, cpu, memory, next);
        }
        if (execute_alu(i, cpu)) {
            cpu.pc = next;
            return Status::Running;
        }

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

            case Kind::Cmpdi:   // comparaison sur les 64 bits
                set_cr_field(cpu, i.crf, static_cast<std::int64_t>(cpu.gpr[i.ra]), i.imm);
                cpu.pc = next;
                return Status::Running;

            case Kind::Cmpd:
                set_cr_field(cpu, i.crf, static_cast<std::int64_t>(cpu.gpr[i.ra]),
                             static_cast<std::int64_t>(cpu.gpr[i.rb]));
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

            case Kind::Lwzx: {
                const std::uint64_t address = base_value(cpu, i.ra) + cpu.gpr[i.rb];
                const std::uint32_t value = memory.read32(static_cast<std::size_t>(address));
                cpu.gpr[i.rd] = value;
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Stwx: {
                const std::uint64_t address = base_value(cpu, i.ra) + cpu.gpr[i.rb];
                memory.write32(static_cast<std::size_t>(address), static_cast<std::uint32_t>(cpu.gpr[i.rd]));
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Lbzx: {
                const std::uint64_t address = base_value(cpu, i.ra) + cpu.gpr[i.rb];
                const std::uint8_t value = memory.read8(static_cast<std::size_t>(address));
                cpu.gpr[i.rd] = value;
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Stbx: {
                const std::uint64_t address = base_value(cpu, i.ra) + cpu.gpr[i.rb];
                memory.write8(static_cast<std::size_t>(address), static_cast<std::uint8_t>(cpu.gpr[i.rd]));
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Ld: {   // 64 bits en big-endian : le mot de poids fort est a l'adresse la plus basse
                const std::size_t address = static_cast<std::size_t>(add_signed(base_value(cpu, i.ra), i.imm));
                const std::uint64_t high = memory.read32(address);
                const std::uint64_t low = memory.read32(address + 4);
                cpu.gpr[i.rd] = (high << 32) | low;
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Std: {
                const std::size_t address = static_cast<std::size_t>(add_signed(base_value(cpu, i.ra), i.imm));
                (void)memory.read32(address + 4);   // verifie que les 8 octets existent avant d'ecrire quoi que ce soit
                (void)memory.read32(address);
                memory.write32(address, static_cast<std::uint32_t>(cpu.gpr[i.rd] >> 32));
                memory.write32(address + 4, static_cast<std::uint32_t>(cpu.gpr[i.rd]));
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

            default:   // Unknown : mot inconnu ou forme pas encore geree
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
