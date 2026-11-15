#include "nerith/ppc/cpu.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
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

constexpr std::uint32_t kXerSo = 0x80000000u;   // SO : "summary overflow"
constexpr std::uint32_t kXerCa = 0x20000000u;   // CA : retenue (carry)
constexpr std::uint32_t kXerMask = 0xE000007Fu; // les bits de XER que Nerith garde (SO, OV, CA et le compteur d'octets)

// Ecrit 4 bits (LT = 8, GT = 4, EQ = 2) dans le champ crf de CR ; le bit SO du champ recopie celui de XER.
void set_cr_bits(Cpu& cpu, std::uint8_t crf, std::uint32_t bits) {
    bits |= (cpu.xer & kXerSo) != 0 ? 1u : 0u;
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

// a + b + retenue entrante, sur 64 bits ; renvoie aussi la retenue sortante (le "carry").
std::uint64_t add_carry(std::uint64_t a, std::uint64_t b, bool carry_in, bool& carry_out) {
    const std::uint64_t sum = a + b;
    const std::uint64_t total = sum + (carry_in ? 1u : 0u);
    carry_out = sum < a || total < sum;
    return total;
}

void set_carry(Cpu& cpu, bool carry) {
    cpu.xer = carry ? (cpu.xer | kXerCa) : (cpu.xer & ~kXerCa);
}

// Decalage a droite AVEC signe (les bits qui entrent a gauche copient le signe) de "bits" bits de valeur.
// Renvoie le resultat sur 64 bits et dit si la retenue doit etre posee : valeur negative ET un bit 1 est parti.
std::uint64_t shift_right_signed(std::int64_t value, unsigned amount, unsigned bits, bool& carry) {
    const unsigned last = bits - 1;   // le plus grand decalage utile (31 ou 63)
    const unsigned n = amount > last ? last : amount;
    std::uint64_t lost = 0;
    if (amount > last) {
        lost = value != 0 ? 1 : 0;   // tout est parti ; le resultat est 0 ou -1
    } else if (n > 0) {
        lost = static_cast<std::uint64_t>(value) & ((std::uint64_t{1} << n) - 1);
    }
    carry = value < 0 && lost != 0;
    if (amount > last) {
        return value < 0 ? ~std::uint64_t{0} : 0;
    }
    return static_cast<std::uint64_t>(value >> n);   // decalage avec signe (defini depuis C++20)
}

unsigned count_leading_zeros(std::uint64_t value, unsigned bits) {
    unsigned count = 0;
    for (unsigned bit = bits; bit > 0 && ((value >> (bit - 1)) & 1u) == 0; --bit) {
        ++count;
    }
    return count;
}

// Bit n de CR (le bit 0 est celui de poids FORT, comme dans les manuels).
bool cr_bit(const Cpu& cpu, unsigned n) {
    return ((cpu.cr >> (31u - n)) & 1u) != 0;
}

void set_cr_bit(Cpu& cpu, unsigned n, bool value) {
    const std::uint32_t mask = 1u << (31u - n);
    cpu.cr = value ? (cpu.cr | mask) : (cpu.cr & ~mask);
}

// Masque de CR pour un champ FXM de mtcrf/mfocrf : le bit de poids fort de FXM (bit 7) choisit le champ 0.
std::uint32_t cr_mask_of_fxm(std::uint32_t fxm) {
    std::uint32_t mask = 0;
    for (unsigned field = 0; field < 8; field++) {
        if (((fxm >> (7u - field)) & 1u) != 0) {
            mask |= 0xFu << (4u * (7u - field));
        }
    }
    return mask;
}

// Condition d'un saut conditionnel (champ BO) : decremente d'abord CTR si BO le demande.
// Renvoie true si le saut est pris ; new_ctr recoit la nouvelle valeur de CTR (a ecrire si l'instruction aboutit).
bool branch_taken(const Cpu& cpu, unsigned bo, unsigned bi, std::uint64_t& new_ctr, bool use_ctr) {
    new_ctr = cpu.ctr;
    bool ctr_ok = true;
    if (use_ctr && (bo & 4u) == 0) {   // bit "ne pas toucher a CTR" absent
        new_ctr -= 1;
        ctr_ok = (new_ctr != 0) != ((bo & 2u) != 0);
    }
    const bool cond_ok = (bo & 16u) != 0 || cr_bit(cpu, bi) == ((bo & 8u) != 0);
    return ctr_ok && cond_ok;
}

// Lit 4 ou 8 octets en big-endian a une adresse.
std::uint64_t read_big_endian(Memory& memory, std::size_t where, unsigned size) {
    std::uint64_t value = 0;
    for (unsigned k = 0; k < size; k++) {
        value = (value << 8) | memory.read8(where + k);
    }
    return value;
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


// ============================ Flottants ============================

constexpr std::uint64_t kSignBit = 0x8000000000000000ull;
constexpr std::uint64_t kDefaultNan = 0x7FF8000000000000ull;   // le NaN "par defaut" de PowerPC (positif, silencieux)
constexpr std::uint64_t kQuietBit = 0x0008000000000000ull;
constexpr std::uint32_t kFpscrMask = 0x9FFFFFFFu;               // bits 1 et 2 (FEX, VX) : calcules par le processeur, pas ecrits

double as_double(std::uint64_t bits) { return std::bit_cast<double>(bits); }
std::uint64_t as_bits(double value) { return std::bit_cast<std::uint64_t>(value); }
bool is_nan_bits(std::uint64_t bits) { return std::isnan(as_double(bits)); }

// Resultat d'un calcul : si c'est un NaN, PowerPC renvoie le premier NaN en entree (rendu silencieux),
// ou le NaN par defaut 0x7FF8... si les entrees ne sont pas des NaN (le processeur x86 donnerait 0xFFF8...).
std::uint64_t result_bits(double value, std::uint64_t in_a, std::uint64_t in_b, std::uint64_t in_c) {
    if (!std::isnan(value)) {
        return as_bits(value);
    }
    for (const std::uint64_t input : {in_a, in_b, in_c}) {
        if (is_nan_bits(input)) {
            return input | kQuietBit;
        }
    }
    return kDefaultNan;
}

// Arrondit un double a la precision simple (32 bits), pour fadds, fmuls, frsp...
std::uint64_t single_bits(std::uint64_t bits) {
    return is_nan_bits(bits) ? bits : as_bits(static_cast<double>(static_cast<float>(as_double(bits))));
}

// Arrondi vers un entier selon le mode RN de FPSCR (0 = au plus proche pair, 1 = vers zero, 2 = vers +inf, 3 = vers -inf).
double round_by_mode(double value, unsigned mode) {
    switch (mode) {
        case 1:  return std::trunc(value);
        case 2:  return std::ceil(value);
        case 3:  return std::floor(value);
        default: return std::nearbyint(value);   // mode d'arrondi par defaut de la machine : au plus proche pair
    }
}

// Conversion en entier de 32 bits avec saturation (fctiw, fctiwz) : un NaN donne le plus petit nombre.
std::uint32_t to_int32_saturated(double value, unsigned mode) {
    if (std::isnan(value)) {
        return 0x80000000u;
    }
    const double rounded = round_by_mode(value, mode);
    if (rounded >= 2147483648.0) {
        return 0x7FFFFFFFu;
    }
    if (rounded < -2147483648.0) {
        return 0x80000000u;
    }
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(rounded));
}

std::uint64_t to_int64_saturated(double value, unsigned mode) {
    if (std::isnan(value)) {
        return 0x8000000000000000ull;
    }
    const double rounded = round_by_mode(value, mode);
    if (rounded >= 9223372036854775808.0) {
        return 0x7FFFFFFFFFFFFFFFull;
    }
    if (rounded < -9223372036854775808.0) {
        return 0x8000000000000000ull;
    }
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(rounded));
}

// Description d'un acces memoire flottant.
struct FpMemOp {
    unsigned size;   // 4 ou 8 octets
    bool store;
    bool single;     // 4 octets : convertit entre le format 32 bits en memoire et le double du registre
    bool update;     // "u" : rA recoit l'adresse calculee
    bool indexed;    // "x" : adresse = rA + rB
};

bool fp_mem_op_of(Kind kind, FpMemOp& op) {
    switch (kind) {
        //                       taille store  single upd    idx
        case Kind::Lfs:    op = {4, false, true,  false, false}; return true;
        case Kind::Lfsu:   op = {4, false, true,  true,  false}; return true;
        case Kind::Lfd:    op = {8, false, false, false, false}; return true;
        case Kind::Lfdu:   op = {8, false, false, true,  false}; return true;
        case Kind::Stfs:   op = {4, true,  true,  false, false}; return true;
        case Kind::Stfsu:  op = {4, true,  true,  true,  false}; return true;
        case Kind::Stfd:   op = {8, true,  false, false, false}; return true;
        case Kind::Stfdu:  op = {8, true,  false, true,  false}; return true;
        case Kind::Lfsx:   op = {4, false, true,  false, true};  return true;
        case Kind::Lfsux:  op = {4, false, true,  true,  true};  return true;
        case Kind::Lfdx:   op = {8, false, false, false, true};  return true;
        case Kind::Lfdux:  op = {8, false, false, true,  true};  return true;
        case Kind::Stfsx:  op = {4, true,  true,  false, true};  return true;
        case Kind::Stfsux: op = {4, true,  true,  true,  true};  return true;
        case Kind::Stfdx:  op = {8, true,  false, false, true};  return true;
        case Kind::Stfdux: op = {8, true,  false, true,  true};  return true;
        case Kind::Stfiwx: op = {4, true,  false, false, true};  return true;   // ecrit les 32 bits de poids faible tels quels
        default: return false;
    }
}

Status do_fp_memory(const Instruction& i, const FpMemOp& op, Cpu& cpu, Memory& memory, std::uint64_t next) {
    if (op.update && i.ra == 0) {
        return Status::InvalidInstruction;
    }
    const std::uint64_t offset =
        op.indexed ? cpu.gpr[i.rb] : static_cast<std::uint64_t>(static_cast<std::int64_t>(i.imm));
    const std::uint64_t address = base_value(cpu, i.ra) + offset;
    const std::size_t where = static_cast<std::size_t>(address);

    if (op.store) {
        std::uint64_t value = cpu.fpr[i.rd];
        if (op.single && i.kind != Kind::Stfiwx) {   // double du registre -> format 32 bits
            value = std::bit_cast<std::uint32_t>(static_cast<float>(as_double(value)));
        }
        (void)memory.read8(where);
        (void)memory.read8(where + op.size - 1);
        for (unsigned k = 0; k < op.size; k++) {
            memory.write8(where + k, static_cast<std::uint8_t>(value >> (8u * (op.size - 1 - k))));
        }
    } else {
        const std::uint64_t value = read_big_endian(memory, where, op.size);
        cpu.fpr[i.rd] = op.single ? as_bits(static_cast<double>(std::bit_cast<float>(static_cast<std::uint32_t>(value))))
                                  : value;
    }
    if (op.update) {
        cpu.gpr[i.ra] = address;
    }
    cpu.pc = next;
    return Status::Running;
}

// Les calculs, copies, conversions et comparaisons flottants, et les instructions du registre FPSCR.
// Renvoie false si l'instruction n'en fait pas partie. Ne touche pas a pc.
bool execute_fp(const Instruction& i, Cpu& cpu) {
    auto& f = cpu.fpr;
    const std::uint64_t bits_a = f[i.ra];
    const std::uint64_t bits_b = f[i.rb];
    const std::uint64_t bits_c = f[i.frc];
    const double a = as_double(bits_a);
    const double b = as_double(bits_b);
    const double c = as_double(bits_c);
    const unsigned rounding = cpu.fpscr & 3u;

    // pour fmadd et ses cousins : (a * c) +/- b d'un seul arrondi
    auto arith = [&](double value, bool single, std::uint64_t in_a, std::uint64_t in_b, std::uint64_t in_c) {
        const std::uint64_t bits = result_bits(value, in_a, in_b, in_c);
        f[i.rd] = single ? single_bits(bits) : bits;
    };

    switch (i.kind) {
        case Kind::Fadd:   arith(a + b, false, bits_a, bits_b, 0); return true;
        case Kind::Fadds:  arith(a + b, true,  bits_a, bits_b, 0); return true;
        case Kind::Fsub:   arith(a - b, false, bits_a, bits_b, 0); return true;
        case Kind::Fsubs:  arith(a - b, true,  bits_a, bits_b, 0); return true;
        case Kind::Fmul:   arith(a * c, false, bits_a, 0, bits_c); return true;
        case Kind::Fmuls:  arith(a * c, true,  bits_a, 0, bits_c); return true;
        case Kind::Fdiv:   arith(a / b, false, bits_a, bits_b, 0); return true;
        case Kind::Fdivs:  arith(a / b, true,  bits_a, bits_b, 0); return true;
        case Kind::Fsqrt:  arith(std::sqrt(b), false, 0, bits_b, 0); return true;
        case Kind::Fsqrts: arith(std::sqrt(b), true,  0, bits_b, 0); return true;
        case Kind::Fres:   arith(1.0 / b, true,  0, bits_b, 0); return true;   // exact, la machine donne une estimation
        case Kind::Frsqrte: arith(1.0 / std::sqrt(b), false, 0, bits_b, 0); return true;
        case Kind::Fmadd:  arith(std::fma(a, c, b),  false, bits_a, bits_b, bits_c); return true;
        case Kind::Fmadds: arith(std::fma(a, c, b),  true,  bits_a, bits_b, bits_c); return true;
        case Kind::Fmsub:  arith(std::fma(a, c, -b), false, bits_a, bits_b, bits_c); return true;
        case Kind::Fmsubs: arith(std::fma(a, c, -b), true,  bits_a, bits_b, bits_c); return true;
        case Kind::Fnmadd:  arith(-std::fma(a, c, b),  false, bits_a, bits_b, bits_c); return true;
        case Kind::Fnmadds: arith(-std::fma(a, c, b),  true,  bits_a, bits_b, bits_c); return true;
        case Kind::Fnmsub:  arith(-std::fma(a, c, -b), false, bits_a, bits_b, bits_c); return true;
        case Kind::Fnmsubs: arith(-std::fma(a, c, -b), true,  bits_a, bits_b, bits_c); return true;
        case Kind::Fsel:   // frD = (frA >= 0) ? frC : frB ; un NaN donne frB
            f[i.rd] = (a >= 0.0) ? bits_c : bits_b;
            return true;

        case Kind::Fmr:   f[i.rd] = bits_b; return true;
        case Kind::Fneg:  f[i.rd] = bits_b ^ kSignBit; return true;
        case Kind::Fabs:  f[i.rd] = bits_b & ~kSignBit; return true;
        case Kind::Fnabs: f[i.rd] = bits_b | kSignBit; return true;
        case Kind::Frsp:  f[i.rd] = single_bits(is_nan_bits(bits_b) ? (bits_b | kQuietBit) : bits_b); return true;

        // le mot de 32 bits est dans les bits de poids faible ; les bits de poids fort sont indefinis (comme Dolphin : 0xFFF80000)
        case Kind::Fctiw:  f[i.rd] = 0xFFF8000000000000ull | to_int32_saturated(b, rounding); return true;
        case Kind::Fctiwz: f[i.rd] = 0xFFF8000000000000ull | to_int32_saturated(b, 1); return true;
        case Kind::Fctid:  f[i.rd] = to_int64_saturated(b, rounding); return true;
        case Kind::Fctidz: f[i.rd] = to_int64_saturated(b, 1); return true;
        case Kind::Fcfid:  f[i.rd] = as_bits(static_cast<double>(static_cast<std::int64_t>(bits_b))); return true;

        case Kind::Fcmpu:
        case Kind::Fcmpo: {   // LT, GT, EQ ou "non ordonnes" (un des deux est un NaN) : 8, 4, 2, 1 ; rien n'est melange avec XER
            const std::uint32_t result = std::isnan(a) || std::isnan(b) ? 1u : (a < b ? 8u : (a > b ? 4u : 2u));
            const unsigned shift = 4u * (7u - i.crf);
            cpu.cr = (cpu.cr & ~(0xFu << shift)) | (result << shift);
            cpu.fpscr = (cpu.fpscr & ~0x0000F000u) | (result << 12);   // FPCC, bits 16-19
            return true;
        }

        // --- registre FPSCR ---
        case Kind::Mffs:   // les 32 bits de poids fort de frD sont indefinis (convention de Dolphin : 0xFFF80000)
            f[i.rd] = 0xFFF8000000000000ull | cpu.fpscr;
            return true;
        case Kind::Mtfsf: {
            const std::uint32_t mask = cr_mask_of_fxm(static_cast<std::uint32_t>(i.imm)) & kFpscrMask;
            cpu.fpscr = (cpu.fpscr & ~mask) | (static_cast<std::uint32_t>(bits_b) & mask);
            return true;
        }
        case Kind::Mtfsfi: {
            const unsigned shift = 4u * (7u - i.crf);
            const std::uint32_t mask = (0xFu << shift) & kFpscrMask;
            cpu.fpscr = (cpu.fpscr & ~mask) | ((static_cast<std::uint32_t>(i.imm) << shift) & mask);
            return true;
        }
        case Kind::Mtfsb0:
        case Kind::Mtfsb1: {
            const std::uint32_t bit = (0x80000000u >> i.rd) & kFpscrMask;
            cpu.fpscr = i.kind == Kind::Mtfsb1 ? (cpu.fpscr | bit) : (cpu.fpscr & ~bit);
            return true;
        }
        case Kind::Mcrfs: {   // copie un champ de FPSCR dans CR, puis efface les bits d'exception "collants" de ce champ
            constexpr std::uint32_t kStickyBits = 0x9FF80700u;
            const unsigned from = 4u * (7u - i.bi);
            const unsigned to = 4u * (7u - i.crf);
            const std::uint32_t field = (cpu.fpscr >> from) & 0xFu;
            cpu.cr = (cpu.cr & ~(0xFu << to)) | (field << to);
            cpu.fpscr &= ~(kStickyBits & (0xFu << from));
            return true;
        }
        default:
            return false;
    }
}

// Les 64 bits de poids fort du produit de deux nombres de 64 bits SANS signe (sans __int128, que Visual Studio n'a pas).
std::uint64_t mulh_unsigned64(std::uint64_t a, std::uint64_t b) {
    const std::uint64_t a_lo = a & 0xFFFFFFFFu, a_hi = a >> 32;
    const std::uint64_t b_lo = b & 0xFFFFFFFFu, b_hi = b >> 32;
    const std::uint64_t p0 = a_lo * b_lo;
    const std::uint64_t p1 = a_lo * b_hi;
    const std::uint64_t p2 = a_hi * b_lo;
    const std::uint64_t p3 = a_hi * b_hi;
    const std::uint64_t carry = ((p0 >> 32) + (p1 & 0xFFFFFFFFu) + (p2 & 0xFFFFFFFFu)) >> 32;
    return p3 + (p1 >> 32) + (p2 >> 32) + carry;
}

// Pareil AVEC signe : on part du resultat sans signe, puis on retire b si a est negatif, et a si b est negatif.
std::uint64_t mulh_signed64(std::uint64_t a, std::uint64_t b) {
    std::uint64_t high = mulh_unsigned64(a, b);
    if (static_cast<std::int64_t>(a) < 0) {
        high -= b;
    }
    if (static_cast<std::int64_t>(b) < 0) {
        high -= a;
    }
    return high;
}

// Vrai si le produit de deux nombres de 64 bits avec signe ne tient pas sur 64 bits.
bool multiply_overflows64(std::int64_t x, std::int64_t y) {
    if (x == 0 || y == 0) {
        return false;
    }
    if (x == INT64_MIN && y == -1) {   // le seul cas ou la division de controle ci-dessous planterait
        return true;
    }
    const std::int64_t product = static_cast<std::int64_t>(static_cast<std::uint64_t>(x) * static_cast<std::uint64_t>(y));
    return product / y != x;
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

        // --- retenue (CA) : ces instructions lisent et ecrivent le bit CA de XER ---
        case Kind::Addic:
        case Kind::AddicDot: {   // rA est un vrai registre ici (r0 vaut r0, pas 0)
            bool carry = false;
            const std::uint64_t result = add_carry(a, static_cast<std::uint64_t>(static_cast<std::int64_t>(i.imm)), false, carry);
            g[i.rd] = result;
            set_carry(cpu, carry);
            if (i.kind == Kind::AddicDot) {
                set_cr_field(cpu, 0, static_cast<std::int64_t>(result), 0);
            }
            return true;
        }
        case Kind::Subfic: {   // rD = SIMM - rA
            bool carry = false;
            g[i.rd] = add_carry(~a, static_cast<std::uint64_t>(static_cast<std::int64_t>(i.imm)), true, carry);
            set_carry(cpu, carry);
            return true;
        }
        case Kind::Addc:
        case Kind::Adde:
        case Kind::Subfc:
        case Kind::Subfe:
        case Kind::Addze:
        case Kind::Addme:
        case Kind::Subfze:
        case Kind::Subfme: {
            const bool ca = (cpu.xer & kXerCa) != 0;
            const std::uint64_t ones = ~std::uint64_t{0};
            bool carry = false;
            std::uint64_t result = 0;
            switch (i.kind) {
                case Kind::Addc:   result = add_carry(a, b, false, carry); break;
                case Kind::Adde:   result = add_carry(a, b, ca, carry); break;
                case Kind::Subfc:  result = add_carry(~a, b, true, carry); break;   // rB - rA
                case Kind::Subfe:  result = add_carry(~a, b, ca, carry); break;
                case Kind::Addze:  result = add_carry(a, 0, ca, carry); break;
                case Kind::Addme:  result = add_carry(a, ones, ca, carry); break;
                case Kind::Subfze: result = add_carry(~a, 0, ca, carry); break;
                default:           result = add_carry(~a, ones, ca, carry); break;   // Subfme
            }
            g[i.rd] = result;
            set_carry(cpu, carry);
            return true;
        }
        case Kind::Sraw:
        case Kind::Srawi: {   // decalage de 32 bits avec signe : le resultat est etendu sur 64 bits
            const unsigned n = i.kind == Kind::Srawi ? i.sh : static_cast<unsigned>(b & 0x3F);
            bool carry = false;
            const std::uint64_t result =
                shift_right_signed(static_cast<std::int32_t>(low32u(s)), n, 32, carry);
            g[i.ra] = result;
            set_carry(cpu, carry);
            return true;
        }
        case Kind::Srad:
        case Kind::Sradi: {
            const unsigned n = i.kind == Kind::Sradi ? i.sh : static_cast<unsigned>(b & 0x7F);
            bool carry = false;
            g[i.ra] = shift_right_signed(static_cast<std::int64_t>(s), n, 64, carry);
            set_carry(cpu, carry);
            return true;
        }
        case Kind::Mfxer:  g[i.rd] = cpu.xer; return true;
        case Kind::Mtxer:  cpu.xer = static_cast<std::uint32_t>(s) & kXerMask; return true;
        case Kind::Mulhw: {
            const std::int64_t product = static_cast<std::int64_t>(static_cast<std::int32_t>(low32u(a))) *
                                         static_cast<std::int64_t>(static_cast<std::int32_t>(low32u(b)));
            g[i.rd] = sext32(static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32));
            return true;
        }
        case Kind::Mulhwu:
            g[i.rd] = (static_cast<std::uint64_t>(low32u(a)) * low32u(b)) >> 32;
            return true;
        case Kind::Mulhd:  g[i.rd] = mulh_signed64(a, b); return true;
        case Kind::Mulhdu: g[i.rd] = mulh_unsigned64(a, b); return true;
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
        case Kind::Rldcl:  g[i.ra] = rotl64(s, static_cast<unsigned>(b & 0x3F)) & mask64(i.mb, 63); return true;
        case Kind::Rldcr:  g[i.ra] = rotl64(s, static_cast<unsigned>(b & 0x3F)) & mask64(0, i.me); return true;
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

namespace {

// Execute une instruction DEJA decodee (sans s'occuper de Rc ni de OE : step_inner s'en charge ensuite).
Status execute_decoded(const Instruction& i, Cpu& cpu, Memory& memory) {
    try {
        const std::uint64_t next = cpu.pc + 4;

        MemOp mem{};
        if (mem_op_of(i.kind, mem)) {
            return do_memory(i, mem, cpu, memory, next);
        }
        if (execute_alu(i, cpu)) {
            cpu.pc = next;
            return Status::Running;
        }
        FpMemOp fp_mem{};
        if (fp_mem_op_of(i.kind, fp_mem)) {
            return do_fp_memory(i, fp_mem, cpu, memory, next);
        }
        if (execute_fp(i, cpu)) {
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
                std::uint64_t new_ctr = 0;
                const bool taken = branch_taken(cpu, i.bo, i.bi, new_ctr, true);
                cpu.ctr = new_ctr;
                if (i.link) {
                    cpu.lr = next;   // LR change meme si le saut n'est pas pris
                }
                const std::uint64_t target = i.absolute
                                                 ? static_cast<std::uint64_t>(static_cast<std::uint32_t>(i.imm))
                                                 : add_signed(cpu.pc, i.imm);
                cpu.pc = taken ? target : next;
                return Status::Running;
            }

            case Kind::Bclr: {
                std::uint64_t new_ctr = 0;
                const bool taken = branch_taken(cpu, i.bo, i.bi, new_ctr, true);
                if (taken && !i.link && cpu.lr == 0) {
                    return Status::Halted;   // retour vers 0 : fin du programme, rien ne change
                }
                const std::uint64_t target = cpu.lr & ~std::uint64_t{3};
                cpu.ctr = new_ctr;
                if (i.link) {
                    cpu.lr = next;
                }
                cpu.pc = taken ? target : next;
                return Status::Running;
            }

            case Kind::Bcctr: {   // CTR n'est jamais decremente ici : le decodeur refuse BO sans le bit 4
                std::uint64_t unused = 0;
                const bool taken = branch_taken(cpu, i.bo, i.bi, unused, false);
                const std::uint64_t target = cpu.ctr & ~std::uint64_t{3};
                if (i.link) {
                    cpu.lr = next;
                }
                cpu.pc = taken ? target : next;
                return Status::Running;
            }

            // --- registre de conditions (CR) ---
            case Kind::Mcrf: {
                const unsigned from = 4u * (7u - i.bi);
                const unsigned to = 4u * (7u - i.crf);
                cpu.cr = (cpu.cr & ~(0xFu << to)) | (((cpu.cr >> from) & 0xFu) << to);
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Crand:
            case Kind::Crnand:
            case Kind::Cror:
            case Kind::Crxor:
            case Kind::Crnor:
            case Kind::Creqv:
            case Kind::Crandc:
            case Kind::Crorc: {   // rd = bit de destination (BT), ra = BA, rb = BB
                const bool a = cr_bit(cpu, i.ra);
                const bool b = cr_bit(cpu, i.rb);
                bool result = false;
                switch (i.kind) {
                    case Kind::Crand:  result = a && b; break;
                    case Kind::Crnand: result = !(a && b); break;
                    case Kind::Cror:   result = a || b; break;
                    case Kind::Crxor:  result = a != b; break;
                    case Kind::Crnor:  result = !(a || b); break;
                    case Kind::Creqv:  result = a == b; break;
                    case Kind::Crandc: result = a && !b; break;
                    default:           result = a || !b; break;   // crorc
                }
                set_cr_bit(cpu, i.rd, result);
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Mfcr:
                cpu.gpr[i.rd] = cpu.cr;
                cpu.pc = next;
                return Status::Running;

            case Kind::Mfocrf:   // seuls les champs choisis sont recopies, le reste de rD vaut 0
                cpu.gpr[i.rd] = cpu.cr & cr_mask_of_fxm(static_cast<std::uint32_t>(i.imm));
                cpu.pc = next;
                return Status::Running;

            case Kind::Mtcrf:
            case Kind::Mtocrf: {
                const std::uint32_t mask = cr_mask_of_fxm(static_cast<std::uint32_t>(i.imm));
                cpu.cr = (cpu.cr & ~mask) | (static_cast<std::uint32_t>(cpu.gpr[i.rd]) & mask);
                cpu.pc = next;
                return Status::Running;
            }

            // --- appel systeme et barrieres ---
            case Kind::Sc:
                cpu.pc = next;
                return Status::SystemCall;

            case Kind::Sync:
            case Kind::Eieio:
            case Kind::Isync:   // un seul coeur, memoire immediate : rien a faire
            case Kind::Dcbt:
            case Kind::Dcbtst:
            case Kind::Dcbst:
            case Kind::Dcbf:
            case Kind::Icbi:    // pas de vrai cache non plus : ces indications ne font rien
                cpu.pc = next;
                return Status::Running;

            case Kind::Dcbz:    // met a zero un bloc de 32 octets (dcbz) ou 128 (dcbzl), aligne sur sa taille
            case Kind::Dcbzl: {
                const std::uint64_t block = i.kind == Kind::Dcbz ? 32u : 128u;
                const std::uint64_t start = (base_value(cpu, i.ra) + cpu.gpr[i.rb]) & ~(block - 1);
                if (start + block > memory.size()) {   // tout ou rien : pas de bloc a moitie ecrit
                    return Status::OutOfBounds;
                }
                for (std::uint64_t k = 0; k < block; ++k) {
                    memory.write8(static_cast<std::size_t>(start + k), 0);
                }
                cpu.pc = next;
                return Status::Running;
            }

            // --- registres systeme ---
            case Kind::Mfmsr:
                cpu.gpr[i.rd] = cpu.msr;
                cpu.pc = next;
                return Status::Running;

            case Kind::Mtmsr:    // L = 0 : remplace les 32 bits de poids faible ; L = 1 : seulement EE et RI
            case Kind::Mtmsrd: { // pareil, mais L = 0 remplace les 64 bits
                const std::uint64_t value = cpu.gpr[i.rd];
                if (i.imm != 0) {
                    const std::uint64_t ee_ri = 0x8002;   // EE (interruptions externes) et RI (interruption recuperable)
                    cpu.msr = (cpu.msr & ~ee_ri) | (value & ee_ri);
                } else if (i.kind == Kind::Mtmsr) {
                    cpu.msr = (cpu.msr & 0xFFFFFFFF00000000ull) | (value & 0xFFFFFFFFull);
                } else {
                    cpu.msr = value;
                }
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Mfspr: {
                std::uint64_t value = 0;
                switch (i.imm) {
                    case 22:  value = cpu.dec;  break;
                    case 26:  value = cpu.srr0; break;
                    case 27:  value = cpu.srr1; break;
                    case 268: value = cpu.tb;   break;
                    case 269: value = cpu.tb >> 32; break;
                    case 287: value = cpu.pvr;  break;
                    default:  value = cpu.sprg[i.imm - 272]; break;   // 272 a 275
                }
                cpu.gpr[i.rd] = value;
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Mtspr: {
                const std::uint64_t value = cpu.gpr[i.rd];
                switch (i.imm) {
                    case 22:  cpu.dec = static_cast<std::uint32_t>(value);  break;
                    case 26:  cpu.srr0 = value; break;
                    case 27:  cpu.srr1 = value; break;
                    default:  cpu.sprg[i.imm - 272] = value; break;
                }
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Mftb:
                cpu.gpr[i.rd] = i.imm == 269 ? (cpu.tb >> 32) : cpu.tb;
                cpu.pc = next;
                return Status::Running;

            // --- verrous atomiques : une reservation par processeur ---
            case Kind::Lwarx:
            case Kind::Ldarx: {
                const std::uint64_t address = base_value(cpu, i.ra) + cpu.gpr[i.rb];
                const unsigned size = i.kind == Kind::Lwarx ? 4u : 8u;
                const std::uint64_t value = read_big_endian(memory, static_cast<std::size_t>(address), size);
                cpu.gpr[i.rd] = value;
                cpu.reserved = true;
                cpu.reserve_address = address;
                cpu.pc = next;
                return Status::Running;
            }

            case Kind::Stwcx:
            case Kind::Stdcx: {
                const std::uint64_t address = base_value(cpu, i.ra) + cpu.gpr[i.rb];
                const unsigned size = i.kind == Kind::Stwcx ? 4u : 8u;
                const bool success = cpu.reserved && cpu.reserve_address == address;
                if (success) {
                    const std::size_t where = static_cast<std::size_t>(address);
                    (void)memory.read8(where);
                    (void)memory.read8(where + size - 1);   // les deux bouts existent : on peut ecrire
                    for (unsigned k = 0; k < size; k++) {
                        memory.write8(where + k, static_cast<std::uint8_t>(cpu.gpr[i.rd] >> (8u * (size - 1 - k))));
                    }
                }
                cpu.reserved = false;
                set_cr_bits(cpu, 0, success ? 2u : 0u);   // EQ si l'ecriture a eu lieu ; SO recopie depuis XER
                cpu.pc = next;
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

// --- bits Rc et OE ---

constexpr std::uint32_t kXerOv = 0x40000000u;   // OV : overflow de la derniere instruction avec OE = 1

// Vrai si l'addition x + y + retenue (r est le resultat) deborde, vue comme une addition AVEC signe de 64 bits.
bool add_overflows(std::uint64_t x, std::uint64_t y, std::uint64_t r) {
    return (((x ^ r) & (y ^ r)) >> 63) != 0;
}

// Y a-t-il overflow pour une instruction avec OE = 1 ? A calculer AVANT l'execution
// (rD peut etre le meme registre que rA ou rB, et serait ecrase).
bool overflow_of(const Instruction& i, const Cpu& cpu) {
    const std::uint64_t a = cpu.gpr[i.ra];
    const std::uint64_t b = cpu.gpr[i.rb];
    const std::uint64_t ca = (cpu.xer & kXerCa) != 0 ? 1u : 0u;
    const std::uint64_t ones = ~std::uint64_t{0};
    switch (i.kind) {
        case Kind::Add:
        case Kind::Addc:   return add_overflows(a, b, a + b);
        case Kind::Adde:   return add_overflows(a, b, a + b + ca);
        case Kind::Addze:  return add_overflows(a, 0, a + ca);
        case Kind::Addme:  return add_overflows(a, ones, a + ones + ca);
        case Kind::Sub:    // rB - rA = ~rA + rB + 1
        case Kind::Subfc:  return add_overflows(~a, b, ~a + b + 1);
        case Kind::Subfe:  return add_overflows(~a, b, ~a + b + ca);
        case Kind::Subfze: return add_overflows(~a, 0, ~a + ca);
        case Kind::Subfme: return add_overflows(~a, ones, ~a + ones + ca);
        case Kind::Neg:    return a == 0x8000000000000000ull;
        case Kind::Mullw: {
            const std::int64_t product = static_cast<std::int64_t>(low32(a)) * static_cast<std::int64_t>(low32(b));
            return product != static_cast<std::int64_t>(static_cast<std::int32_t>(product));
        }
        case Kind::Mulld:  return multiply_overflows64(static_cast<std::int64_t>(a), static_cast<std::int64_t>(b));
        case Kind::Divw:   return low32(b) == 0 || (low32(a) == INT32_MIN && low32(b) == -1);
        case Kind::Divwu:  return low32u(b) == 0;
        case Kind::Divd:   return b == 0 || (static_cast<std::int64_t>(a) == INT64_MIN && static_cast<std::int64_t>(b) == -1);
        case Kind::Divdu:  return b == 0;
        default:           return false;
    }
}

// Quel registre entier recoit le resultat (pour le test de Rc) ? -1 si l'instruction n'est pas entiere.
int result_register(const Instruction& i) {
    switch (i.kind) {
        case Kind::Add: case Kind::Sub: case Kind::Mullw: case Kind::Mulhw: case Kind::Mulhwu: case Kind::Mulhd:
        case Kind::Mulhdu: case Kind::Mulld: case Kind::Divw: case Kind::Divwu: case Kind::Divd: case Kind::Divdu:
        case Kind::Neg: case Kind::Addc: case Kind::Adde: case Kind::Subfc: case Kind::Subfe: case Kind::Addze:
        case Kind::Addme: case Kind::Subfze: case Kind::Subfme:
            return i.rd;   // ces instructions ecrivent dans rD
        case Kind::Or: case Kind::And: case Kind::Andc: case Kind::Xor: case Kind::Nor: case Kind::Nand:
        case Kind::Orc: case Kind::Eqv: case Kind::Slw: case Kind::Srw: case Kind::Sld: case Kind::Srd:
        case Kind::Sraw: case Kind::Srawi: case Kind::Srad: case Kind::Sradi: case Kind::Cntlzw: case Kind::Cntlzd:
        case Kind::Extsb: case Kind::Extsh: case Kind::Extsw: case Kind::Rlwinm: case Kind::Rlwimi:
        case Kind::Rlwnm: case Kind::Rldicl: case Kind::Rldicr: case Kind::Rldic: case Kind::Rldimi:
        case Kind::Rldcl: case Kind::Rldcr:
            return i.ra;   // ... celles-ci dans rA
        default:
            return -1;
    }
}

// Decode, execute, puis applique OE (XER) et Rc (CR0, ou CR1 pour les flottants).
Status step_inner(Cpu& cpu, Memory& memory) {
    Instruction i;
    try {
        i = decode(memory.read32(static_cast<std::size_t>(cpu.pc)));
    } catch (const std::out_of_range&) {
        return Status::OutOfBounds;
    }
    const bool overflow = i.oe && overflow_of(i, cpu);
    const Status status = execute_decoded(i, cpu, memory);
    if (status != Status::Running) {
        return status;
    }
    if (i.oe) {   // OV prend la valeur du dernier calcul ; SO est "collant" : il reste a 1 une fois pose
        cpu.xer = overflow ? (cpu.xer | kXerOv | kXerSo) : (cpu.xer & ~kXerOv);
    }
    if (i.rc) {
        const int target = result_register(i);
        if (target >= 0) {   // entiers : CR0 = comparaison du resultat (64 bits, avec signe) a 0
            set_cr_field(cpu, 0, static_cast<std::int64_t>(cpu.gpr[target]), 0);
        } else {             // flottants : CR1 recoit les 4 bits de poids fort de FPSCR (FX, FEX, VX, OX)
            cpu.cr = (cpu.cr & ~0x0F000000u) | ((cpu.fpscr >> 28) << 24);
        }
    }
    return status;
}

}  // namespace

Status step(Cpu& cpu, Memory& memory) {
    const Status status = step_inner(cpu, memory);
    if (status != Status::InvalidInstruction && status != Status::OutOfBounds) {
        cpu.tb += 1;    // notre horloge : un tic par instruction executee
        cpu.dec -= 1;   // le decompteur descend au meme rythme (et reboucle a zero, pas d'interruption)
    }
    return status;
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
