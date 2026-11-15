#include "nerith/ppc/vmx.h"

#include <cctype>
#include <stdexcept>

#include "nerith/ppc/vmx_codec.h"

namespace nerith::ppc {

namespace {

// ---- fabrication des motifs (mot de 32 bits) ----
constexpr std::uint32_t kVx = 0xFC0007FFu;       // forme VX : opcode + XO de 11 bits
constexpr std::uint32_t kVa = 0xFC00003Fu;       // forme VA : opcode + XO de 6 bits
constexpr std::uint32_t kVxMem = 0xFC0007FFu;    // lectures/ecritures standard (opcode 31, XO de 10 bits + bit 0)
constexpr std::uint32_t k128 = 0xFC0003D0u;      // VX128 : opcode + 5 bits de code
constexpr std::uint32_t k128Mem = 0xFC0007F3u;   // VX128_1 : lectures/ecritures 128
constexpr std::uint32_t k128Imm = 0xFC0007F0u;   // VX128_3 : une operation sur vB avec un nombre ou rien
constexpr std::uint32_t k128Perm = 0xFC000210u;  // VX128_2 : vperm128
constexpr std::uint32_t k128Shb = 0xFC000010u;   // VX128_5 : vsldoi128
constexpr std::uint32_t k128Pwi = 0xFC000630u;   // VX128_P : vpermwi128
constexpr std::uint32_t k128Four = 0xFC000730u;  // VX128_4 : vpkd3d128, vrlimi128

constexpr std::uint32_t vx(std::uint32_t xo) { return (4u << 26) | xo; }
constexpr std::uint32_t mem31(std::uint32_t xo) { return (31u << 26) | (xo << 1); }
constexpr std::uint32_t x128(std::uint32_t op, std::uint32_t xo) { return (op << 26) | (xo & 0x3D0u); }
constexpr std::uint32_t x128_1(std::uint32_t xo) { return (4u << 26) | (xo & 0x7F3u); }
constexpr std::uint32_t x128_3(std::uint32_t xo) { return (6u << 26) | (xo & 0x7F0u); }
constexpr std::uint32_t x128_4(std::uint32_t xo) { return (6u << 26) | (xo & 0x730u); }

using Op = VecOp;
using F = VForm;

// L'ordre compte : la premiere ligne qui correspond gagne (vsldoi128, tout a la fin, est la plus "gourmande").
const VecInfo kTable[] = {
    // ---- AltiVec standard : lectures et ecritures (opcode 31) ----
    {Op::Lvsl,   "lvsl",   mem31(6),   kVxMem, F::Mem, false, false},
    {Op::Lvsr,   "lvsr",   mem31(38),  kVxMem, F::Mem, false, false},
    {Op::Lvebx,  "lvebx",  mem31(7),   kVxMem, F::Mem, false, false},
    {Op::Lvehx,  "lvehx",  mem31(39),  kVxMem, F::Mem, false, false},
    {Op::Lvewx,  "lvewx",  mem31(71),  kVxMem, F::Mem, false, false},
    {Op::Lvx,    "lvx",    mem31(103), kVxMem, F::Mem, false, false},
    {Op::Lvxl,   "lvxl",   mem31(359), kVxMem, F::Mem, false, false},
    {Op::Stvebx, "stvebx", mem31(135), kVxMem, F::Mem, false, false},
    {Op::Stvehx, "stvehx", mem31(167), kVxMem, F::Mem, false, false},
    {Op::Stvewx, "stvewx", mem31(199), kVxMem, F::Mem, false, false},
    {Op::Stvx,   "stvx",   mem31(231), kVxMem, F::Mem, false, false},
    {Op::Stvxl,  "stvxl",  mem31(487), kVxMem, F::Mem, false, false},
    {Op::Lvlx,   "lvlx",   mem31(519), kVxMem, F::Mem, false, false},
    {Op::Lvrx,   "lvrx",   mem31(551), kVxMem, F::Mem, false, false},
    {Op::Lvlxl,  "lvlxl",  mem31(775), kVxMem, F::Mem, false, false},
    {Op::Lvrxl,  "lvrxl",  mem31(807), kVxMem, F::Mem, false, false},
    {Op::Stvlx,  "stvlx",  mem31(647), kVxMem, F::Mem, false, false},
    {Op::Stvrx,  "stvrx",  mem31(679), kVxMem, F::Mem, false, false},
    {Op::Stvlxl, "stvlxl", mem31(903), kVxMem, F::Mem, false, false},
    {Op::Stvrxl, "stvrxl", mem31(935), kVxMem, F::Mem, false, false},

    // ---- AltiVec standard : calculs (opcode 4) ----
    {Op::Vaddfp,   "vaddfp",   vx(10),   kVx, F::Ab, false, false},
    {Op::Vsubfp,   "vsubfp",   vx(74),   kVx, F::Ab, false, false},
    {Op::Vand,     "vand",     vx(1028), kVx, F::Ab, false, false},
    {Op::Vandc,    "vandc",    vx(1092), kVx, F::Ab, false, false},
    {Op::Vor,      "vor",      vx(1156), kVx, F::Ab, false, false},
    {Op::Vxor,     "vxor",     vx(1220), kVx, F::Ab, false, false},
    {Op::Vnor,     "vnor",     vx(1284), kVx, F::Ab, false, false},
    {Op::Vmrghw,   "vmrghw",   vx(140),  kVx, F::Ab, false, false},
    {Op::Vmrglw,   "vmrglw",   vx(396),  kVx, F::Ab, false, false},
    {Op::Vrlw,     "vrlw",     vx(132),  kVx, F::Ab, false, false},
    {Op::Vslw,     "vslw",     vx(388),  kVx, F::Ab, false, false},
    {Op::Vsrw,     "vsrw",     vx(644),  kVx, F::Ab, false, false},
    {Op::Vsraw,    "vsraw",    vx(900),  kVx, F::Ab, false, false},
    {Op::Vmaxfp,   "vmaxfp",   vx(1034), kVx, F::Ab, false, false},
    {Op::Vminfp,   "vminfp",   vx(1098), kVx, F::Ab, false, false},
    {Op::Vadduwm,  "vadduwm",  vx(128),  kVx, F::Ab, false, false},
    {Op::Vsubuwm,  "vsubuwm",  vx(1152), kVx, F::Ab, false, false},
    {Op::Vslo,     "vslo",     vx(1036), kVx, F::Ab, false, false},
    {Op::Vsro,     "vsro",     vx(1100), kVx, F::Ab, false, false},
    {Op::Vpkuhum,  "vpkuhum",  vx(14),   kVx, F::Ab, false, false},
    {Op::Vpkuwum,  "vpkuwum",  vx(78),   kVx, F::Ab, false, false},
    {Op::Vpkuhus,  "vpkuhus",  vx(142),  kVx, F::Ab, false, false},
    {Op::Vpkuwus,  "vpkuwus",  vx(206),  kVx, F::Ab, false, false},
    {Op::Vpkshus,  "vpkshus",  vx(270),  kVx, F::Ab, false, false},
    {Op::Vpkswus,  "vpkswus",  vx(334),  kVx, F::Ab, false, false},
    {Op::Vpkshss,  "vpkshss",  vx(398),  kVx, F::Ab, false, false},
    {Op::Vpkswss,  "vpkswss",  vx(462),  kVx, F::Ab, false, false},
    {Op::Vcmpeqfp, "vcmpeqfp",  vx(198),          kVx, F::Ab, false, false},
    {Op::Vcmpeqfp, "vcmpeqfp.", vx(198) | 0x400u, kVx, F::Ab, false, true},
    {Op::Vcmpgefp, "vcmpgefp",  vx(454),          kVx, F::Ab, false, false},
    {Op::Vcmpgefp, "vcmpgefp.", vx(454) | 0x400u, kVx, F::Ab, false, true},
    {Op::Vcmpgtfp, "vcmpgtfp",  vx(710),          kVx, F::Ab, false, false},
    {Op::Vcmpgtfp, "vcmpgtfp.", vx(710) | 0x400u, kVx, F::Ab, false, true},
    {Op::Vcmpbfp,  "vcmpbfp",   vx(966),          kVx, F::Ab, false, false},
    {Op::Vcmpbfp,  "vcmpbfp.",  vx(966) | 0x400u, kVx, F::Ab, false, true},
    {Op::Vcmpequw, "vcmpequw",  vx(134),          kVx, F::Ab, false, false},
    {Op::Vcmpequw, "vcmpequw.", vx(134) | 0x400u, kVx, F::Ab, false, true},
    {Op::Vrefp,      "vrefp",      vx(266), kVx | 0x001F0000u, F::B, false, false},
    {Op::Vrsqrtefp,  "vrsqrtefp",  vx(330), kVx | 0x001F0000u, F::B, false, false},
    {Op::Vexptefp,   "vexptefp",   vx(394), kVx | 0x001F0000u, F::B, false, false},
    {Op::Vlogefp,    "vlogefp",    vx(458), kVx | 0x001F0000u, F::B, false, false},
    {Op::Vrfin,      "vrfin",      vx(522), kVx | 0x001F0000u, F::B, false, false},
    {Op::Vrfiz,      "vrfiz",      vx(586), kVx | 0x001F0000u, F::B, false, false},
    {Op::Vrfip,      "vrfip",      vx(650), kVx | 0x001F0000u, F::B, false, false},
    {Op::Vrfim,      "vrfim",      vx(714), kVx | 0x001F0000u, F::B, false, false},
    {Op::Vupkhsb,    "vupkhsb",    vx(526), kVx | 0x001F0000u, F::B, false, false},
    {Op::Vupklsb,    "vupklsb",    vx(654), kVx | 0x001F0000u, F::B, false, false},
    {Op::Vcfux,  "vcfux",  vx(778), kVx, F::BUimm, false, false},
    {Op::Vcfsx,  "vcfsx",  vx(842), kVx, F::BUimm, false, false},
    {Op::Vctuxs, "vctuxs", vx(906), kVx, F::BUimm, false, false},
    {Op::Vctsxs, "vctsxs", vx(970), kVx, F::BUimm, false, false},
    {Op::Vspltw,   "vspltw",   vx(652), kVx | 0x001C0000u, F::BSplat, false, false},
    {Op::Vspltisw, "vspltisw", vx(908), kVx | 0x0000F800u, F::Simm, false, false},
    // VA : 4 registres
    {Op::Vsel,     "vsel",     vx(42), kVa, F::AbC, false, false},
    {Op::Vperm,    "vperm",    vx(43), kVa, F::AbC, false, false},
    {Op::Vsldoi,   "vsldoi",   vx(44), kVa | 0x00000400u, F::AbShb, false, false},
    {Op::Vmaddfp,  "vmaddfp",  vx(46), kVa, F::AcB, false, false},
    {Op::Vnmsubfp, "vnmsubfp", vx(47), kVa, F::AcB, false, false},

    // ---- VMX128 : lectures et ecritures (opcode 4) ----
    {Op::Lvsl,   "lvsl128",   x128_1(3),    k128Mem, F::Mem, true, false},
    {Op::Lvsr,   "lvsr128",   x128_1(67),   k128Mem, F::Mem, true, false},
    {Op::Lvewx,  "lvewx128",  x128_1(131),  k128Mem, F::Mem, true, false},
    {Op::Lvx,    "lvx128",    x128_1(195),  k128Mem, F::Mem, true, false},
    {Op::Stvewx, "stvewx128", x128_1(387),  k128Mem, F::Mem, true, false},
    {Op::Stvx,   "stvx128",   x128_1(451),  k128Mem, F::Mem, true, false},
    {Op::Lvxl,   "lvxl128",   x128_1(707),  k128Mem, F::Mem, true, false},
    {Op::Stvxl,  "stvxl128",  x128_1(963),  k128Mem, F::Mem, true, false},
    {Op::Lvlx,   "lvlx128",   x128_1(1027), k128Mem, F::Mem, true, false},
    {Op::Lvrx,   "lvrx128",   x128_1(1091), k128Mem, F::Mem, true, false},
    {Op::Stvlx,  "stvlx128",  x128_1(1283), k128Mem, F::Mem, true, false},
    {Op::Stvrx,  "stvrx128",  x128_1(1347), k128Mem, F::Mem, true, false},
    {Op::Lvlxl,  "lvlxl128",  x128_1(1539), k128Mem, F::Mem, true, false},
    {Op::Lvrxl,  "lvrxl128",  x128_1(1603), k128Mem, F::Mem, true, false},
    {Op::Stvlxl, "stvlxl128", x128_1(1795), k128Mem, F::Mem, true, false},
    {Op::Stvrxl, "stvrxl128", x128_1(1859), k128Mem, F::Mem, true, false},

    // ---- VMX128 : calculs, opcode 5 ----
    {Op::Vperm,       "vperm128",    (5u << 26), k128Perm, F::Perm128, true, false},
    {Op::Vaddfp,      "vaddfp128",   x128(5, 16),  k128, F::Ab, true, false},
    {Op::Vsubfp,      "vsubfp128",   x128(5, 80),  k128, F::Ab, true, false},
    {Op::Vmulfp128,   "vmulfp128",   x128(5, 144), k128, F::Ab, true, false},
    {Op::Vmaddfp128,  "vmaddfp128",  x128(5, 208), k128, F::AbVd, true, false},
    {Op::Vmaddcfp128, "vmaddcfp128", x128(5, 272), k128, F::AdB, true, false},
    {Op::Vnmsubfp128, "vnmsubfp128", x128(5, 336), k128, F::AbVd, true, false},
    {Op::Vmsum3fp128, "vmsum3fp128", x128(5, 400), k128, F::Ab, true, false},
    {Op::Vmsum4fp128, "vmsum4fp128", x128(5, 464), k128, F::Ab, true, false},
    {Op::Vpkshss,     "vpkshss128",  x128(5, 512), k128, F::Ab, true, false},
    {Op::Vand,        "vand128",     x128(5, 528), k128, F::Ab, true, false},
    {Op::Vpkshus,     "vpkshus128",  x128(5, 576), k128, F::Ab, true, false},
    {Op::Vandc,       "vandc128",    x128(5, 592), k128, F::Ab, true, false},
    {Op::Vpkswss,     "vpkswss128",  x128(5, 640), k128, F::Ab, true, false},
    {Op::Vnor,        "vnor128",     x128(5, 656), k128, F::Ab, true, false},
    {Op::Vpkswus,     "vpkswus128",  x128(5, 704), k128, F::Ab, true, false},
    {Op::Vor,         "vor128",      x128(5, 720), k128, F::Ab, true, false},
    {Op::Vpkuhum,     "vpkuhum128",  x128(5, 768), k128, F::Ab, true, false},
    {Op::Vxor,        "vxor128",     x128(5, 784), k128, F::Ab, true, false},
    {Op::Vpkuhus,     "vpkuhus128",  x128(5, 832), k128, F::Ab, true, false},
    {Op::Vsel128,     "vsel128",     x128(5, 848), k128, F::AbVd, true, false},
    {Op::Vpkuwum,     "vpkuwum128",  x128(5, 896), k128, F::Ab, true, false},
    {Op::Vslo,        "vslo128",     x128(5, 912), k128, F::Ab, true, false},
    {Op::Vpkuwus,     "vpkuwus128",  x128(5, 960), k128, F::Ab, true, false},
    {Op::Vsro,        "vsro128",     x128(5, 976), k128, F::Ab, true, false},

    // ---- VMX128 : calculs, opcode 6 ----
    {Op::Vpermwi128,  "vpermwi128",  x128(6, 0) | 0x210u, k128Pwi, F::Pwi128, true, false},
    {Op::Vctsxs,      "vcfpsxws128", x128_3(560),  k128Imm, F::BUimm, true, false},
    {Op::Vctuxs,      "vcfpuxws128", x128_3(624),  k128Imm, F::BUimm, true, false},
    {Op::Vcfsx,       "vcsxwfp128",  x128_3(688),  k128Imm, F::BUimm, true, false},
    {Op::Vcfux,       "vcuxwfp128",  x128_3(752),  k128Imm, F::BUimm, true, false},
    {Op::Vrfim,       "vrfim128",    x128_3(816),  k128Imm | 0x001F0000u, F::B, true, false},
    {Op::Vrfin,       "vrfin128",    x128_3(880),  k128Imm | 0x001F0000u, F::B, true, false},
    {Op::Vrfip,       "vrfip128",    x128_3(944),  k128Imm | 0x001F0000u, F::B, true, false},
    {Op::Vrfiz,       "vrfiz128",    x128_3(1008), k128Imm | 0x001F0000u, F::B, true, false},
    {Op::Vpkd3d128,   "vpkd3d128",   x128_4(1552), k128Four, F::Pkd3d128, true, false},
    {Op::Vrefp,       "vrefp128",    x128_3(1584), k128Imm | 0x001F0000u, F::B, true, false},
    {Op::Vrsqrtefp,   "vrsqrtefp128",x128_3(1648), k128Imm | 0x001F0000u, F::B, true, false},
    {Op::Vexptefp,    "vexptefp128", x128_3(1712), k128Imm | 0x001F0000u, F::B, true, false},
    {Op::Vlogefp,     "vlogefp128",  x128_3(1776), k128Imm | 0x001F0000u, F::B, true, false},
    {Op::Vrlimi128,   "vrlimi128",   x128_4(1808), k128Four, F::Rlimi128, true, false},
    {Op::Vspltw,      "vspltw128",   x128_3(1840), k128Imm, F::BUimm, true, false},
    {Op::Vspltisw,    "vspltisw128", x128_3(1904), k128Imm, F::BSimm128, true, false},
    {Op::Vupkd3d128,  "vupkd3d128",  x128_3(2032), k128Imm, F::BUimm, true, false},
    {Op::Vcmpeqfp,    "vcmpeqfp128",  x128(6, 0),   k128, F::Ab, true, false},
    {Op::Vcmpeqfp,    "vcmpeqfp128.", x128(6, 64),  k128, F::Ab, true, true},
    {Op::Vrlw,        "vrlw128",      x128(6, 80),  k128, F::Ab, true, false},
    {Op::Vcmpgefp,    "vcmpgefp128",  x128(6, 128), k128, F::Ab, true, false},
    {Op::Vcmpgefp,    "vcmpgefp128.", x128(6, 192), k128, F::Ab, true, true},
    {Op::Vslw,        "vslw128",      x128(6, 208), k128, F::Ab, true, false},
    {Op::Vcmpgtfp,    "vcmpgtfp128",  x128(6, 256), k128, F::Ab, true, false},
    {Op::Vcmpgtfp,    "vcmpgtfp128.", x128(6, 320), k128, F::Ab, true, true},
    {Op::Vsraw,       "vsraw128",     x128(6, 336), k128, F::Ab, true, false},
    {Op::Vcmpbfp,     "vcmpbfp128",   x128(6, 384), k128, F::Ab, true, false},
    {Op::Vcmpbfp,     "vcmpbfp128.",  x128(6, 448), k128, F::Ab, true, true},
    {Op::Vsrw,        "vsrw128",      x128(6, 464), k128, F::Ab, true, false},
    {Op::Vcmpequw,    "vcmpequw128",  x128(6, 512), k128, F::Ab, true, false},
    {Op::Vcmpequw,    "vcmpequw128.", x128(6, 576), k128, F::Ab, true, true},
    {Op::Vmaxfp,      "vmaxfp128",    x128(6, 640), k128, F::Ab, true, false},
    {Op::Vminfp,      "vminfp128",    x128(6, 704), k128, F::Ab, true, false},
    {Op::Vmrghw,      "vmrghw128",    x128(6, 768), k128, F::Ab, true, false},
    {Op::Vmrglw,      "vmrglw128",    x128(6, 832), k128, F::Ab, true, false},
    {Op::Vupkhsb,     "vupkhsb128",   x128(6, 896), k128 | 0x001F0460u, F::B, true, false},
    {Op::Vupklsb,     "vupklsb128",   x128(6, 960), k128 | 0x001F0460u, F::B, true, false},

    // ---- VMX128 : vsldoi128 (opcode 4, bit 4 a 1) : en dernier, car son masque est tres court ----
    {Op::Vsldoi,      "vsldoi128",    (4u << 26) | 0x10u, k128Shb, F::AbShb, true, false},
};

constexpr std::size_t kTableSize = sizeof(kTable) / sizeof(kTable[0]);

std::int32_t sext5(std::uint32_t v) { return static_cast<std::int32_t>((v & 0x1Fu) ^ 0x10u) - 0x10; }

std::string vreg(unsigned n) { return "v" + std::to_string(n); }
std::string gpr_or_zero(unsigned n) { return n == 0 ? "0" : "r" + std::to_string(n); }

}  // namespace

const VecInfo* vec_table() { return kTable; }
std::size_t vec_table_size() { return kTableSize; }

const VecInfo* find_vec_for_word(std::uint32_t word) {
    for (const VecInfo& info : kTable) {
        if ((word & info.mask) == info.match) {
            return &info;
        }
    }
    return nullptr;
}

const VecInfo* find_vec_by_name(const std::string& name) {
    for (const VecInfo& info : kTable) {
        if (name == info.name) {
            return &info;
        }
    }
    return nullptr;
}

bool decode_vector(std::uint32_t w, Instruction& out) {
    const VecInfo* info = find_vec_for_word(w);
    if (info == nullptr) {
        return false;
    }
    Instruction r;
    r.word = w;
    r.kind = Kind::Vector;
    r.vop = info->op;
    r.vec = info;
    r.rc = info->rc;
    const bool big = info->is128;
    const auto d = static_cast<std::uint8_t>(big ? vmx_vd128(w) : (w >> 21) & 0x1Fu);
    const auto a = static_cast<std::uint8_t>(big ? vmx_va128(w) : (w >> 16) & 0x1Fu);
    const auto b = static_cast<std::uint8_t>(big ? vmx_vb128(w) : (w >> 11) & 0x1Fu);
    r.rd = d;
    switch (info->form) {
        case VForm::Mem:
            r.ra = static_cast<std::uint8_t>((w >> 16) & 0x1Fu);   // registres entiers, pas vectoriels
            r.rb = static_cast<std::uint8_t>((w >> 11) & 0x1Fu);
            break;
        case VForm::Ab:
        case VForm::AbVd:
        case VForm::AdB:
            r.ra = a;
            r.rb = b;
            break;
        case VForm::AbC:
        case VForm::AcB:
            r.ra = a;
            r.rb = b;
            r.frc = static_cast<std::uint8_t>((w >> 6) & 0x1Fu);
            break;
        case VForm::Perm128:
            r.ra = a;
            r.rb = b;
            r.frc = static_cast<std::uint8_t>((w >> 6) & 0x7u);
            break;
        case VForm::AbShb:
            r.ra = a;
            r.rb = b;
            r.imm = static_cast<std::int32_t>((w >> 6) & 0xFu);
            break;
        case VForm::B:
            r.rb = b;
            break;
        case VForm::BUimm:
            r.rb = b;
            r.imm = static_cast<std::int32_t>((w >> 16) & 0x1Fu);
            break;
        case VForm::BSplat:
            r.rb = b;
            r.imm = static_cast<std::int32_t>((w >> 16) & 0x3u);
            break;
        case VForm::Simm:
            r.imm = sext5(w >> 16);
            break;
        case VForm::BSimm128:
            r.rb = b;
            r.imm = sext5(w >> 16);
            break;
        case VForm::Pwi128:
            r.rb = b;
            r.imm = static_cast<std::int32_t>(((w >> 1) & 0xE0u) | ((w >> 16) & 0x1Fu));
            break;
        case VForm::Rlimi128:
            r.rb = b;
            r.imm = static_cast<std::int32_t>((w >> 16) & 0x1Fu);
            r.mb = static_cast<std::uint8_t>((w >> 6) & 0x3u);
            break;
        case VForm::Pkd3d128:
            r.rb = b;
            r.imm = static_cast<std::int32_t>((w >> 18) & 0x7u);
            r.sh = static_cast<std::uint8_t>((w >> 16) & 0x3u);
            r.mb = static_cast<std::uint8_t>((w >> 6) & 0x3u);
            break;
    }
    out = r;
    return true;
}

std::string vector_text(const Instruction& i) {
    const VecInfo* info = i.vec;
    if (info == nullptr) {
        return ".long";
    }
    std::string n = info->name;
    const std::string d = vreg(i.rd);
    const std::string a = vreg(i.ra);
    const std::string b = vreg(i.rb);
    const std::string c = vreg(i.frc);
    switch (info->form) {
        case VForm::Mem:
            return n + " " + d + ", " + gpr_or_zero(i.ra) + ", r" + std::to_string(i.rb);
        case VForm::Ab:
            // llvm ecrit "vmr" pour "vor x, y, y" et "vnot" pour "vnor x, y, y"
            if (!info->is128 && i.ra == i.rb && i.vop == VecOp::Vor) {
                return "vmr " + d + ", " + a;
            }
            if (!info->is128 && i.ra == i.rb && i.vop == VecOp::Vnor) {
                return "vnot " + d + ", " + a;
            }
            return n + " " + d + ", " + a + ", " + b;
        case VForm::AbC:
        case VForm::Perm128:
            return n + " " + d + ", " + a + ", " + b + ", " + c;
        case VForm::AcB:
            return n + " " + d + ", " + a + ", " + c + ", " + b;
        case VForm::AbShb:
            return n + " " + d + ", " + a + ", " + b + ", " + std::to_string(i.imm);
        case VForm::AbVd:
            return n + " " + d + ", " + a + ", " + b + ", " + d;
        case VForm::AdB:
            return n + " " + d + ", " + a + ", " + d + ", " + b;
        case VForm::B:
            return n + " " + d + ", " + b;
        case VForm::BUimm:
        case VForm::BSplat:
        case VForm::BSimm128:
        case VForm::Pwi128:
            return n + " " + d + ", " + b + ", " + std::to_string(i.imm);
        case VForm::Simm:
            return n + " " + d + ", " + std::to_string(i.imm);
        case VForm::Rlimi128:
            return n + " " + d + ", " + b + ", " + std::to_string(i.imm) + ", " + std::to_string(i.mb);
        case VForm::Pkd3d128:
            return n + " " + d + ", " + b + ", " + std::to_string(i.imm) + ", " + std::to_string(i.sh) + ", " +
                   std::to_string(i.mb);
    }
    return n;
}

namespace {

std::string lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

[[noreturn]] void fail(const std::string& message) { throw std::invalid_argument(message); }

// "v12" (ou "12") : numero de registre vectoriel de 0 a max.
unsigned parse_v(const std::string& text, unsigned max) {
    std::string t = lower(text);
    if (!t.empty() && t[0] == 'v') {
        t = t.substr(1);
    }
    if (t.empty() || t.size() > 3) {
        fail("registre vectoriel attendu (v0 a v" + std::to_string(max) + "), trouve \"" + text + "\"");
    }
    for (const char c : t) {
        if (c < '0' || c > '9') {
            fail("registre vectoriel attendu (v0 a v" + std::to_string(max) + "), trouve \"" + text + "\"");
        }
    }
    const unsigned n = static_cast<unsigned>(std::stoi(t));
    if (n > max) {
        fail("registre vectoriel inexistant : " + text + " (v0 a v" + std::to_string(max) + " ici)");
    }
    return n;
}

unsigned parse_r(const std::string& text, bool zero_ok) {
    if (zero_ok && text == "0") {
        return 0;
    }
    const std::string t = lower(text);
    if (t.size() < 2 || t.size() > 3 || t[0] != 'r') {
        fail("registre attendu (r0 a r31), trouve \"" + text + "\"");
    }
    for (std::size_t k = 1; k < t.size(); ++k) {
        if (t[k] < '0' || t[k] > '9') {
            fail("registre attendu (r0 a r31), trouve \"" + text + "\"");
        }
    }
    const int n = std::stoi(t.substr(1));
    if (n > 31) {
        fail("registre inexistant : " + text);
    }
    return static_cast<unsigned>(n);
}

long long parse_n(const std::string& text, long long lo, long long hi) {
    std::size_t used = 0;
    long long v = 0;
    try {
        v = std::stoll(text, &used, 0);
    } catch (const std::exception&) {
        fail("nombre attendu, trouve \"" + text + "\"");
    }
    if (used != text.size()) {
        fail("nombre attendu, trouve \"" + text + "\"");
    }
    if (v < lo || v > hi) {
        fail("nombre " + text + " hors limites (" + std::to_string(lo) + " a " + std::to_string(hi) + ")");
    }
    return v;
}

void need(const std::vector<std::string>& ops, std::size_t count) {
    if (ops.size() != count) {
        fail("cette instruction veut " + std::to_string(count) + " operandes, " + std::to_string(ops.size()) + " donnes");
    }
}

}  // namespace

bool assemble_vector(const std::string& mnemonic, const std::vector<std::string>& ops, std::uint32_t& word) {
    std::string name = mnemonic;
    // alias de llvm : vmr = vor x, y, y ; vnot = vnor x, y, y
    if (name == "vmr" || name == "vnot") {
        need(ops, 2);
        const unsigned d = parse_v(ops[0], 31);
        const unsigned s = parse_v(ops[1], 31);
        const VecInfo* base = find_vec_by_name(name == "vmr" ? "vor" : "vnor");
        word = base->match | (d << 21) | (s << 16) | (s << 11);
        return true;
    }
    const VecInfo* info = find_vec_by_name(name);
    if (info == nullptr) {
        return false;
    }
    const unsigned max = info->is128 ? 127u : 31u;
    std::uint32_t w = info->match;
    auto put_d = [&](unsigned v) { w |= info->is128 ? vmx_put_vd128(v) : (v << 21); };
    auto put_a = [&](unsigned v) { w |= info->is128 ? vmx_put_va128(v) : (v << 16); };
    auto put_b = [&](unsigned v) { w |= info->is128 ? vmx_put_vb128(v) : (v << 11); };
    switch (info->form) {
        case VForm::Mem:
            need(ops, 3);
            put_d(parse_v(ops[0], max));
            w |= (parse_r(ops[1], true) << 16) | (parse_r(ops[2], false) << 11);
            break;
        case VForm::Ab:
            need(ops, 3);
            put_d(parse_v(ops[0], max));
            put_a(parse_v(ops[1], max));
            put_b(parse_v(ops[2], max));
            break;
        case VForm::AbC: {
            need(ops, 4);
            put_d(parse_v(ops[0], max));
            put_a(parse_v(ops[1], max));
            put_b(parse_v(ops[2], max));
            w |= parse_v(ops[3], 31) << 6;
            break;
        }
        case VForm::Perm128:
            need(ops, 4);
            put_d(parse_v(ops[0], max));
            put_a(parse_v(ops[1], max));
            put_b(parse_v(ops[2], max));
            w |= parse_v(ops[3], 7) << 6;
            break;
        case VForm::AcB:
            need(ops, 4);
            put_d(parse_v(ops[0], max));
            put_a(parse_v(ops[1], max));
            w |= parse_v(ops[2], 31) << 6;
            put_b(parse_v(ops[3], max));
            break;
        case VForm::AbShb:
            need(ops, 4);
            put_d(parse_v(ops[0], max));
            put_a(parse_v(ops[1], max));
            put_b(parse_v(ops[2], max));
            w |= static_cast<std::uint32_t>(parse_n(ops[3], 0, 15)) << 6;
            break;
        case VForm::AbVd:
        case VForm::AdB: {
            need(ops, 4);
            const unsigned d = parse_v(ops[0], max);
            put_d(d);
            put_a(parse_v(ops[1], max));
            const std::string& same = info->form == VForm::AbVd ? ops[3] : ops[2];
            const std::string& other = info->form == VForm::AbVd ? ops[2] : ops[3];
            put_b(parse_v(other, max));
            if (parse_v(same, max) != d) {
                fail("le dernier operande de " + name + " doit etre le meme registre que le premier (v" +
                     std::to_string(d) + ")");
            }
            break;
        }
        case VForm::B:
            need(ops, 2);
            put_d(parse_v(ops[0], max));
            put_b(parse_v(ops[1], max));
            break;
        case VForm::BUimm:
            need(ops, 3);
            put_d(parse_v(ops[0], max));
            put_b(parse_v(ops[1], max));
            w |= static_cast<std::uint32_t>(parse_n(ops[2], 0, 31)) << 16;
            break;
        case VForm::BSplat:
            need(ops, 3);
            put_d(parse_v(ops[0], max));
            put_b(parse_v(ops[1], max));
            w |= static_cast<std::uint32_t>(parse_n(ops[2], 0, 3)) << 16;
            break;
        case VForm::Simm:
            need(ops, 2);
            put_d(parse_v(ops[0], max));
            w |= (static_cast<std::uint32_t>(parse_n(ops[1], -16, 15)) & 0x1Fu) << 16;
            break;
        case VForm::BSimm128:
            need(ops, 3);
            put_d(parse_v(ops[0], max));
            put_b(parse_v(ops[1], max));
            w |= (static_cast<std::uint32_t>(parse_n(ops[2], -16, 15)) & 0x1Fu) << 16;
            break;
        case VForm::Pwi128: {
            need(ops, 3);
            put_d(parse_v(ops[0], max));
            put_b(parse_v(ops[1], max));
            const auto perm = static_cast<std::uint32_t>(parse_n(ops[2], 0, 255));
            w |= ((perm & 0xE0u) << 1) | ((perm & 0x1Fu) << 16);
            break;
        }
        case VForm::Rlimi128:
            need(ops, 4);
            put_d(parse_v(ops[0], max));
            put_b(parse_v(ops[1], max));
            w |= static_cast<std::uint32_t>(parse_n(ops[2], 0, 31)) << 16;
            w |= static_cast<std::uint32_t>(parse_n(ops[3], 0, 3)) << 6;
            break;
        case VForm::Pkd3d128:
            need(ops, 5);
            put_d(parse_v(ops[0], max));
            put_b(parse_v(ops[1], max));
            w |= static_cast<std::uint32_t>(parse_n(ops[2], 0, 7)) << 18;
            w |= static_cast<std::uint32_t>(parse_n(ops[3], 0, 3)) << 16;
            w |= static_cast<std::uint32_t>(parse_n(ops[4], 0, 3)) << 6;
            break;
    }
    word = w;
    return true;
}

}  // namespace nerith::ppc
