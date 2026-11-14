#include "nerith/ppc/optable.h"

namespace nerith::ppc {

namespace {

// Les XO de l'opcode 31 sont ceux du manuel PowerPC ; les noms et mots ont ete verifies avec llvm-mc.
constexpr OpInfo kOps[] = {
    // --- calculs entiers (opcode 31) ---
    {Kind::Mulhw,   "mulhw",   31, 75,  Layout::RtRaRb},
    {Kind::Mulhwu,  "mulhwu",  31, 11,  Layout::RtRaRb},
    {Kind::Mulld,   "mulld",   31, 233, Layout::RtRaRb},
    {Kind::Divw,    "divw",    31, 491, Layout::RtRaRb},
    {Kind::Divwu,   "divwu",   31, 459, Layout::RtRaRb},
    {Kind::Divd,    "divd",    31, 489, Layout::RtRaRb},
    {Kind::Divdu,   "divdu",   31, 457, Layout::RtRaRb},
    {Kind::Neg,     "neg",     31, 104, Layout::RtRa},
    // --- logique et decalages (opcode 31) ---
    {Kind::And,     "and",     31, 28,  Layout::RaRsRb},
    {Kind::Andc,    "andc",    31, 60,  Layout::RaRsRb},
    {Kind::Xor,     "xor",     31, 316, Layout::RaRsRb},
    {Kind::Nor,     "nor",     31, 124, Layout::RaRsRb},
    {Kind::Nand,    "nand",    31, 476, Layout::RaRsRb},
    {Kind::Orc,     "orc",     31, 412, Layout::RaRsRb},
    {Kind::Eqv,     "eqv",     31, 284, Layout::RaRsRb},
    {Kind::Slw,     "slw",     31, 24,  Layout::RaRsRb},
    {Kind::Srw,     "srw",     31, 536, Layout::RaRsRb},
    {Kind::Sld,     "sld",     31, 27,  Layout::RaRsRb},
    {Kind::Srd,     "srd",     31, 539, Layout::RaRsRb},
    {Kind::Cntlzw,  "cntlzw",  31, 26,  Layout::RaRs},
    {Kind::Cntlzd,  "cntlzd",  31, 58,  Layout::RaRs},
    {Kind::Extsb,   "extsb",   31, 954, Layout::RaRs},
    {Kind::Extsh,   "extsh",   31, 922, Layout::RaRs},
    {Kind::Extsw,   "extsw",   31, 986, Layout::RaRs},
    // --- retenue : bit CA du registre XER ---
    {Kind::Addc,    "addc",    31, 10,  Layout::RtRaRb},
    {Kind::Adde,    "adde",    31, 138, Layout::RtRaRb},
    {Kind::Subfc,   "subc",    31, 8,   Layout::RtRbRa},   // subfc rD, rA, rB s'ecrit "subc rD, rB, rA"
    {Kind::Subfe,   "subfe",   31, 136, Layout::RtRaRb},
    {Kind::Addze,   "addze",   31, 202, Layout::RtRa},
    {Kind::Addme,   "addme",   31, 234, Layout::RtRa},
    {Kind::Subfze,  "subfze",  31, 200, Layout::RtRa},
    {Kind::Subfme,  "subfme",  31, 232, Layout::RtRa},
    {Kind::Sraw,    "sraw",    31, 792, Layout::RaRsRb},
    {Kind::Srad,    "srad",    31, 794, Layout::RaRsRb},
    {Kind::Srawi,   "srawi",   31, 824, Layout::RaRsSh},
    {Kind::Addic,   "addic",   12, -1,  Layout::RtRaSimm},
    {Kind::AddicDot,"addic.",  13, -1,  Layout::RtRaSimm},
    {Kind::Subfic,  "subfic",  8,  -1,  Layout::RtRaSimm},
    // --- operations sur les bits de CR (opcode 19) ---
    {Kind::Crnor,   "crnor",   19, 33,  Layout::CrBits},
    {Kind::Crandc,  "crandc",  19, 129, Layout::CrBits},
    {Kind::Crxor,   "crxor",   19, 193, Layout::CrBits},
    {Kind::Crnand,  "crnand",  19, 225, Layout::CrBits},
    {Kind::Crand,   "crand",   19, 257, Layout::CrBits},
    {Kind::Creqv,   "creqv",   19, 289, Layout::CrBits},
    {Kind::Crorc,   "crorc",   19, 417, Layout::CrBits},
    {Kind::Cror,    "cror",    19, 449, Layout::CrBits},
    // --- verrous atomiques : lecture avec reservation ---
    {Kind::Lwarx,   "lwarx",   31, 20,  Layout::MemX},
    {Kind::Ldarx,   "ldarx",   31, 84,  Layout::MemX},
    // --- flottants : memoire ---
    {Kind::Lfs,     "lfs",     48, -1,  Layout::FMemD},
    {Kind::Lfsu,    "lfsu",    49, -1,  Layout::FMemD},
    {Kind::Lfd,     "lfd",     50, -1,  Layout::FMemD},
    {Kind::Lfdu,    "lfdu",    51, -1,  Layout::FMemD},
    {Kind::Stfs,    "stfs",    52, -1,  Layout::FMemD},
    {Kind::Stfsu,   "stfsu",   53, -1,  Layout::FMemD},
    {Kind::Stfd,    "stfd",    54, -1,  Layout::FMemD},
    {Kind::Stfdu,   "stfdu",   55, -1,  Layout::FMemD},
    {Kind::Lfsx,    "lfsx",    31, 535, Layout::FMemX},
    {Kind::Lfsux,   "lfsux",   31, 567, Layout::FMemX},
    {Kind::Lfdx,    "lfdx",    31, 599, Layout::FMemX},
    {Kind::Lfdux,   "lfdux",   31, 631, Layout::FMemX},
    {Kind::Stfsx,   "stfsx",   31, 663, Layout::FMemX},
    {Kind::Stfsux,  "stfsux",  31, 695, Layout::FMemX},
    {Kind::Stfdx,   "stfdx",   31, 727, Layout::FMemX},
    {Kind::Stfdux,  "stfdux",  31, 759, Layout::FMemX},
    {Kind::Stfiwx,  "stfiwx",  31, 983, Layout::FMemX},
    // --- flottants : calculs en double (opcode 63) et en simple (opcode 59), formes "A" : XO de 5 bits ---
    {Kind::Fdiv,    "fdiv",    63, 18,  Layout::FA3},
    {Kind::Fsub,    "fsub",    63, 20,  Layout::FA3},
    {Kind::Fadd,    "fadd",    63, 21,  Layout::FA3},
    {Kind::Fsqrt,   "fsqrt",   63, 22,  Layout::FA2},
    {Kind::Fsel,    "fsel",    63, 23,  Layout::FA4},
    {Kind::Fmul,    "fmul",    63, 25,  Layout::FA3c},
    {Kind::Frsqrte, "frsqrte", 63, 26,  Layout::FA2},
    {Kind::Fmsub,   "fmsub",   63, 28,  Layout::FA4},
    {Kind::Fmadd,   "fmadd",   63, 29,  Layout::FA4},
    {Kind::Fnmsub,  "fnmsub",  63, 30,  Layout::FA4},
    {Kind::Fnmadd,  "fnmadd",  63, 31,  Layout::FA4},
    {Kind::Fdivs,   "fdivs",   59, 18,  Layout::FA3},
    {Kind::Fsubs,   "fsubs",   59, 20,  Layout::FA3},
    {Kind::Fadds,   "fadds",   59, 21,  Layout::FA3},
    {Kind::Fsqrts,  "fsqrts",  59, 22,  Layout::FA2},
    {Kind::Fres,    "fres",    59, 24,  Layout::FA2},
    {Kind::Fmuls,   "fmuls",   59, 25,  Layout::FA3c},
    {Kind::Fmsubs,  "fmsubs",  59, 28,  Layout::FA4},
    {Kind::Fmadds,  "fmadds",  59, 29,  Layout::FA4},
    {Kind::Fnmsubs, "fnmsubs", 59, 30,  Layout::FA4},
    {Kind::Fnmadds, "fnmadds", 59, 31,  Layout::FA4},
    // --- flottants : copies, conversions, comparaisons, FPSCR (opcode 63, formes "X" : XO de 10 bits) ---
    {Kind::Fcmpu,   "fcmpu",   63, 0,   Layout::FCmp},
    {Kind::Frsp,    "frsp",    63, 12,  Layout::FX2},
    {Kind::Fctiw,   "fctiw",   63, 14,  Layout::FX2},
    {Kind::Fctiwz,  "fctiwz",  63, 15,  Layout::FX2},
    {Kind::Fcmpo,   "fcmpo",   63, 32,  Layout::FCmp},
    {Kind::Mtfsb1,  "mtfsb1",  63, 38,  Layout::BtOnly},
    {Kind::Fneg,    "fneg",    63, 40,  Layout::FX2},
    {Kind::Mcrfs,   "mcrfs",   63, 64,  Layout::CrfCrf},
    {Kind::Mtfsb0,  "mtfsb0",  63, 70,  Layout::BtOnly},
    {Kind::Fmr,     "fmr",     63, 72,  Layout::FX2},
    {Kind::Mtfsfi,  "mtfsfi",  63, 134, Layout::CrfImm},
    {Kind::Fnabs,   "fnabs",   63, 136, Layout::FX2},
    {Kind::Fabs,    "fabs",    63, 264, Layout::FX2},
    {Kind::Mffs,    "mffs",    63, 583, Layout::FrD},
    {Kind::Mtfsf,   "mtfsf",   63, 711, Layout::FmFrb},
    {Kind::Fctid,   "fctid",   63, 814, Layout::FX2},
    {Kind::Fctidz,  "fctidz",  63, 815, Layout::FX2},
    {Kind::Fcfid,   "fcfid",   63, 846, Layout::FX2},
    // --- nombres de 16 bits ---
    {Kind::Mulli,   "mulli",   7,  -1,  Layout::RtRaSimm},
    {Kind::Oris,    "oris",    25, -1,  Layout::RaRsUimm},
    {Kind::Xori,    "xori",    26, -1,  Layout::RaRsUimm},
    {Kind::Xoris,   "xoris",   27, -1,  Layout::RaRsUimm},
    {Kind::AndiDot, "andi.",   28, -1,  Layout::RaRsUimm},
    {Kind::AndisDot,"andis.",  29, -1,  Layout::RaRsUimm},
    // --- memoire avec deplacement ---
    {Kind::Lwzu,    "lwzu",    33, -1,  Layout::MemD},
    {Kind::Lbzu,    "lbzu",    35, -1,  Layout::MemD},
    {Kind::Stbu,    "stbu",    39, -1,  Layout::MemD},
    {Kind::Lhz,     "lhz",     40, -1,  Layout::MemD},
    {Kind::Lhzu,    "lhzu",    41, -1,  Layout::MemD},
    {Kind::Lha,     "lha",     42, -1,  Layout::MemD},
    {Kind::Lhau,    "lhau",    43, -1,  Layout::MemD},
    {Kind::Sth,     "sth",     44, -1,  Layout::MemD},
    {Kind::Sthu,    "sthu",    45, -1,  Layout::MemD},
    {Kind::Ldu,     "ldu",     58, 1,   Layout::MemDS},
    {Kind::Lwa,     "lwa",     58, 2,   Layout::MemDS},
    {Kind::Stdu,    "stdu",    62, 1,   Layout::MemDS},
    // --- memoire indexee (rA + rB) ---
    {Kind::Lhzx,    "lhzx",    31, 279, Layout::MemX},
    {Kind::Lhax,    "lhax",    31, 343, Layout::MemX},
    {Kind::Sthx,    "sthx",    31, 407, Layout::MemX},
    {Kind::Lwzux,   "lwzux",   31, 55,  Layout::MemX},
    {Kind::Lbzux,   "lbzux",   31, 119, Layout::MemX},
    {Kind::Lhzux,   "lhzux",   31, 311, Layout::MemX},
    {Kind::Lhaux,   "lhaux",   31, 375, Layout::MemX},
    {Kind::Stwux,   "stwux",   31, 183, Layout::MemX},
    {Kind::Stbux,   "stbux",   31, 247, Layout::MemX},
    {Kind::Sthux,   "sthux",   31, 439, Layout::MemX},
    {Kind::Lwax,    "lwax",    31, 341, Layout::MemX},
    {Kind::Lwaux,   "lwaux",   31, 373, Layout::MemX},
    {Kind::Ldx,     "ldx",     31, 21,  Layout::MemX},
    {Kind::Stdx,    "stdx",    31, 149, Layout::MemX},
    {Kind::Ldux,    "ldux",    31, 53,  Layout::MemX},
    {Kind::Stdux,   "stdux",   31, 181, Layout::MemX},
    {Kind::Lwbrx,   "lwbrx",   31, 534, Layout::MemX},
    {Kind::Stwbrx,  "stwbrx",  31, 662, Layout::MemX},
    {Kind::Lhbrx,   "lhbrx",   31, 790, Layout::MemX},
    {Kind::Sthbrx,  "sthbrx",  31, 918, Layout::MemX},
    {Kind::Ldbrx,   "ldbrx",   31, 532, Layout::MemX},
    {Kind::Stdbrx,  "stdbrx",  31, 660, Layout::MemX},
};

}  // namespace

const OpInfo* op_table() { return kOps; }
std::size_t op_table_size() { return sizeof(kOps) / sizeof(kOps[0]); }

bool is_a_form(Layout layout) {
    return layout == Layout::FA3 || layout == Layout::FA3c || layout == Layout::FA4 || layout == Layout::FA2;
}

std::uint32_t reserved_mask(Layout layout) {
    switch (layout) {
        case Layout::RaRs:
        case Layout::RtRa:    return 0x0000F800u;                  // rB (bits 16-20)
        case Layout::FA3:     return 0x000007C0u;                  // frC (bits 21-25)
        case Layout::FA3c:    return 0x0000F800u;                  // frB
        case Layout::FA2:     return 0x001F07C0u;                  // frA et frC
        case Layout::FX2:     return 0x001F0000u;                  // frA
        case Layout::FCmp:    return 0x00600000u;                  // bits 9-10
        case Layout::FrD:
        case Layout::BtOnly:  return 0x001FF800u;                  // bits 11-20
        case Layout::FmFrb:   return 0x02010000u;                  // bits 6 (L) et 15 (W)
        case Layout::CrfImm:  return 0x007F0800u;                  // bits 9-15 et 20
        case Layout::CrfCrf:  return 0x0063F800u;                  // bits 9-10 et 14-20
        default:              return 0;
    }
}

const OpInfo* find_op(Kind kind) {
    for (const OpInfo& op : kOps) {
        if (op.kind == kind) {
            return &op;
        }
    }
    return nullptr;
}

const OpInfo* find_op(const std::string& name) {
    for (const OpInfo& op : kOps) {
        if (name == op.name) {
            return &op;
        }
    }
    return nullptr;
}

const OpInfo* find_op_for_word(std::uint32_t word) {
    const unsigned opcode = word >> 26;
    for (const OpInfo& op : kOps) {
        if (op.opcode != opcode) {
            continue;
        }
        if (opcode == 31 || opcode == 19) {
            if ((word & 1u) == 0 && static_cast<int>((word >> 1) & 0x3FF) == op.xo) {
                return &op;
            }
        } else if (opcode == 58 || opcode == 62) {
            if (static_cast<int>(word & 3u) == op.xo) {
                return &op;
            }
        } else if (opcode == 59 || opcode == 63) {   // flottants : le bit Rc doit valoir 0
            const unsigned xo = is_a_form(op.layout) ? ((word >> 1) & 0x1F) : ((word >> 1) & 0x3FF);
            if ((word & 1u) == 0 && static_cast<int>(xo) == op.xo) {
                return &op;
            }
        } else {
            return &op;
        }
    }
    return nullptr;
}

}  // namespace nerith::ppc
