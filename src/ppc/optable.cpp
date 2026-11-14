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
        if (opcode == 31) {
            if ((word & 1u) == 0 && static_cast<int>((word >> 1) & 0x3FF) == op.xo) {
                return &op;
            }
        } else if (opcode == 58 || opcode == 62) {
            if (static_cast<int>(word & 3u) == op.xo) {
                return &op;
            }
        } else {
            return &op;
        }
    }
    return nullptr;
}

}  // namespace nerith::ppc
