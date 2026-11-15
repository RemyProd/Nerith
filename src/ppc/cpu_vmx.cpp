#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "nerith/ppc/vmx_exec.h"

namespace nerith::ppc {

namespace {

using Vec = std::array<std::uint32_t, 4>;     // 4 mots, le mot 0 est le premier en memoire
using Bytes = std::array<std::uint8_t, 16>;

constexpr std::uint32_t kVscrSat = 1u;

Bytes to_bytes(const Vec& v) {
    Bytes b{};
    for (unsigned k = 0; k < 16; ++k) {
        b[k] = static_cast<std::uint8_t>(v[k / 4] >> (24 - 8 * (k % 4)));
    }
    return b;
}

Vec from_bytes(const Bytes& b) {
    Vec v{};
    for (unsigned k = 0; k < 16; ++k) {
        v[k / 4] |= static_cast<std::uint32_t>(b[k]) << (24 - 8 * (k % 4));
    }
    return v;
}

float as_f(std::uint32_t bits) { return std::bit_cast<float>(bits); }
std::uint32_t as_u(float value) { return std::bit_cast<std::uint32_t>(value); }

bool is_nan_bits(std::uint32_t bits) { return (bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0; }

// Maximum et minimum : un NaN gagne ; entre +0 et -0, le maximum est +0 et le minimum est -0.
std::uint32_t fmax_bits(std::uint32_t a, std::uint32_t b) {
    if (is_nan_bits(a)) return a;
    if (is_nan_bits(b)) return b;
    const float x = as_f(a), y = as_f(b);
    if (x > y) return a;
    if (y > x) return b;
    return a & b;   // egaux : seuls +0/-0 different, et le bit de signe du resultat est le ET des deux
}

std::uint32_t fmin_bits(std::uint32_t a, std::uint32_t b) {
    if (is_nan_bits(a)) return a;
    if (is_nan_bits(b)) return b;
    const float x = as_f(a), y = as_f(b);
    if (x < y) return a;
    if (y < x) return b;
    return a | b;
}

// Lecture d'un octet / ecriture : une adresse invalide leve std::out_of_range (attrapee plus bas).
std::size_t ea_of(const Instruction& i, const Cpu& cpu) {
    const std::uint64_t base = i.ra == 0 ? 0 : cpu.gpr[i.ra];
    return static_cast<std::size_t>(base + cpu.gpr[i.rb]);
}

Status memory_op(const Instruction& i, Cpu& cpu, Memory& memory) {
    const std::size_t ea = ea_of(i, cpu);
    Vec& d = cpu.vr[i.rd];
    switch (i.vop) {
        case VecOp::Lvx:
        case VecOp::Lvxl: {
            const std::size_t at = ea & ~std::size_t{15};
            Bytes b{};
            for (unsigned k = 0; k < 16; ++k) b[k] = memory.read8(at + k);
            d = from_bytes(b);
            break;
        }
        case VecOp::Stvx:
        case VecOp::Stvxl: {
            const std::size_t at = ea & ~std::size_t{15};
            (void)memory.read8(at);
            (void)memory.read8(at + 15);   // tout ou rien
            const Bytes b = to_bytes(d);
            for (unsigned k = 0; k < 16; ++k) memory.write8(at + k, b[k]);
            break;
        }
        case VecOp::Lvsl:
        case VecOp::Lvsr: {
            const unsigned sh = static_cast<unsigned>(ea & 15);
            Bytes b{};
            for (unsigned k = 0; k < 16; ++k) b[k] = static_cast<std::uint8_t>((i.vop == VecOp::Lvsl ? sh : 16 - sh) + k);
            d = from_bytes(b);
            break;
        }
        case VecOp::Lvebx: {   // l'octet va a sa place dans le registre ; le reste est "indefini" : Nerith ne le touche pas
            Bytes b = to_bytes(d);
            b[ea & 15] = memory.read8(ea);
            d = from_bytes(b);
            break;
        }
        case VecOp::Lvehx: {
            const std::size_t at = ea & ~std::size_t{1};
            Bytes b = to_bytes(d);
            b[at & 15] = memory.read8(at);
            b[(at & 15) + 1] = memory.read8(at + 1);
            d = from_bytes(b);
            break;
        }
        case VecOp::Lvewx: {
            const std::size_t at = ea & ~std::size_t{3};
            Bytes b = to_bytes(d);
            for (unsigned k = 0; k < 4; ++k) b[(at & 15) + k] = memory.read8(at + k);
            d = from_bytes(b);
            break;
        }
        case VecOp::Stvebx:
            memory.write8(ea, to_bytes(d)[ea & 15]);
            break;
        case VecOp::Stvehx: {
            const std::size_t at = ea & ~std::size_t{1};
            (void)memory.read8(at + 1);
            const Bytes b = to_bytes(d);
            memory.write8(at, b[at & 15]);
            memory.write8(at + 1, b[(at & 15) + 1]);
            break;
        }
        case VecOp::Stvewx: {
            const std::size_t at = ea & ~std::size_t{3};
            (void)memory.read8(at + 3);
            const Bytes b = to_bytes(d);
            for (unsigned k = 0; k < 4; ++k) memory.write8(at + k, b[(at & 15) + k]);
            break;
        }
        case VecOp::Lvlx:
        case VecOp::Lvlxl: {   // les octets de EA a la fin du bloc de 16, calles a gauche ; le reste = 0
            const unsigned eb = static_cast<unsigned>(ea & 15);
            Bytes b{};
            for (unsigned k = 0; k < 16 - eb; ++k) b[k] = memory.read8(ea + k);
            d = from_bytes(b);
            break;
        }
        case VecOp::Lvrx:
        case VecOp::Lvrxl: {   // les eb octets du debut du bloc jusqu'a EA (exclu), calles a droite ; le reste = 0
            const unsigned eb = static_cast<unsigned>(ea & 15);
            Bytes b{};
            for (unsigned k = 0; k < eb; ++k) b[16 - eb + k] = memory.read8(ea - eb + k);
            d = from_bytes(b);
            break;
        }
        case VecOp::Stvlx:
        case VecOp::Stvlxl: {
            const unsigned eb = static_cast<unsigned>(ea & 15);
            (void)memory.read8(ea);
            (void)memory.read8(ea + (15 - eb));
            const Bytes b = to_bytes(d);
            for (unsigned k = 0; k < 16 - eb; ++k) memory.write8(ea + k, b[k]);
            break;
        }
        case VecOp::Stvrx:
        case VecOp::Stvrxl: {
            const unsigned eb = static_cast<unsigned>(ea & 15);
            if (eb != 0) {
                (void)memory.read8(ea - eb);
                (void)memory.read8(ea - 1);
                const Bytes b = to_bytes(d);
                for (unsigned k = 0; k < eb; ++k) memory.write8(ea - eb + k, b[16 - eb + k]);
            }
            break;
        }
        default:
            return Status::InvalidInstruction;
    }
    cpu.pc += 4;
    return Status::Running;
}

bool is_memory_op(VecOp op) {
    switch (op) {
        case VecOp::Lvebx: case VecOp::Lvehx: case VecOp::Lvewx: case VecOp::Lvsl: case VecOp::Lvsr:
        case VecOp::Lvx: case VecOp::Lvxl: case VecOp::Stvebx: case VecOp::Stvehx: case VecOp::Stvewx:
        case VecOp::Stvx: case VecOp::Stvxl: case VecOp::Lvlx: case VecOp::Lvlxl: case VecOp::Lvrx:
        case VecOp::Lvrxl: case VecOp::Stvlx: case VecOp::Stvlxl: case VecOp::Stvrx: case VecOp::Stvrxl:
            return true;
        default:
            return false;
    }
}

// Conversion float -> entier avec saturation. Renvoie le resultat ; "sat" passe a vrai si on a du saturer.
std::uint32_t float_to_s32(float x, int scale, bool& sat) {
    if (std::isnan(x)) { sat = true; return 0; }
    const double t = std::trunc(std::ldexp(static_cast<double>(x), scale));
    if (t > 2147483647.0) { sat = true; return 0x7FFFFFFFu; }
    if (t < -2147483648.0) { sat = true; return 0x80000000u; }
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(t));
}

std::uint32_t float_to_u32(float x, int scale, bool& sat) {
    if (std::isnan(x)) { sat = true; return 0; }
    const double t = std::trunc(std::ldexp(static_cast<double>(x), scale));
    if (t < 0.0) { sat = true; return 0; }
    if (t > 4294967295.0) { sat = true; return 0xFFFFFFFFu; }
    return static_cast<std::uint32_t>(t);
}

// Saturation d'un entier vers [lo, hi] ; sat devient vrai si on a coupe.
std::int64_t clamp_sat(std::int64_t v, std::int64_t lo, std::int64_t hi, bool& sat) {
    if (v < lo) { sat = true; return lo; }
    if (v > hi) { sat = true; return hi; }
    return v;
}

// Les 16 demi-mots (ou 8 mots) de vA suivis de ceux de vB, vus comme une seule suite.
std::array<std::uint16_t, 16> halves_of(const Vec& a, const Vec& b) {
    std::array<std::uint16_t, 16> h{};
    for (unsigned k = 0; k < 4; ++k) {
        h[2 * k] = static_cast<std::uint16_t>(a[k] >> 16);
        h[2 * k + 1] = static_cast<std::uint16_t>(a[k]);
        h[8 + 2 * k] = static_cast<std::uint16_t>(b[k] >> 16);
        h[8 + 2 * k + 1] = static_cast<std::uint16_t>(b[k]);
    }
    return h;
}

std::array<std::uint32_t, 8> words_of(const Vec& a, const Vec& b) {
    return {a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]};
}

Bytes pack_halves(const Vec& a, const Vec& b, bool is_signed_src, std::int64_t lo, std::int64_t hi, bool modulo,
                  bool& sat) {
    Bytes out{};
    const auto h = halves_of(a, b);
    for (unsigned k = 0; k < 16; ++k) {
        if (modulo) {
            out[k] = static_cast<std::uint8_t>(h[k]);
        } else {
            const std::int64_t v = is_signed_src ? static_cast<std::int16_t>(h[k]) : static_cast<std::int64_t>(h[k]);
            out[k] = static_cast<std::uint8_t>(clamp_sat(v, lo, hi, sat));
        }
    }
    return out;
}

Vec pack_words(const Vec& a, const Vec& b, bool is_signed_src, std::int64_t lo, std::int64_t hi, bool modulo,
               bool& sat) {
    Bytes out{};
    const auto w = words_of(a, b);
    for (unsigned k = 0; k < 8; ++k) {
        std::uint16_t half;
        if (modulo) {
            half = static_cast<std::uint16_t>(w[k]);
        } else {
            const std::int64_t v = is_signed_src ? static_cast<std::int32_t>(w[k]) : static_cast<std::int64_t>(w[k]);
            half = static_cast<std::uint16_t>(clamp_sat(v, lo, hi, sat));
        }
        out[2 * k] = static_cast<std::uint8_t>(half >> 8);
        out[2 * k + 1] = static_cast<std::uint8_t>(half);
    }
    return from_bytes(out);
}

void set_cr6(Cpu& cpu, std::uint32_t bits) {
    cpu.cr = (cpu.cr & ~0x000000F0u) | (bits << 4);
}

}  // namespace

Status execute_vector(const Instruction& i, Cpu& cpu, Memory& memory) {
    try {
        if (is_memory_op(i.vop)) {
            return memory_op(i, cpu, memory);
        }
        const Vec a = cpu.vr[i.ra];
        const Vec b = cpu.vr[i.rb];
        const Vec old_d = cpu.vr[i.rd];
        Vec r{};
        bool sat = false;
        const unsigned imm = static_cast<unsigned>(i.imm);

        auto lanes = [&](auto&& f) {
            for (unsigned k = 0; k < 4; ++k) r[k] = f(k);
        };

        switch (i.vop) {
            // ---- flottants ----
            case VecOp::Vaddfp:      lanes([&](unsigned k) { return as_u(as_f(a[k]) + as_f(b[k])); }); break;
            case VecOp::Vsubfp:      lanes([&](unsigned k) { return as_u(as_f(a[k]) - as_f(b[k])); }); break;
            case VecOp::Vmulfp128:   lanes([&](unsigned k) { return as_u(as_f(a[k]) * as_f(b[k])); }); break;
            case VecOp::Vmaddfp: {   // vD = vA * vC + vB (fusionne)
                const Vec c = cpu.vr[i.frc];
                lanes([&](unsigned k) { return as_u(std::fmaf(as_f(a[k]), as_f(c[k]), as_f(b[k]))); });
                break;
            }
            case VecOp::Vnmsubfp: {  // vD = -(vA * vC - vB)
                const Vec c = cpu.vr[i.frc];
                lanes([&](unsigned k) { return as_u(-std::fmaf(as_f(a[k]), as_f(c[k]), -as_f(b[k]))); });
                break;
            }
            case VecOp::Vmaddfp128:  // vD = vA * vB + vD
                lanes([&](unsigned k) { return as_u(std::fmaf(as_f(a[k]), as_f(b[k]), as_f(old_d[k]))); });
                break;
            case VecOp::Vmaddcfp128: // vD = vA * vD + vB
                lanes([&](unsigned k) { return as_u(std::fmaf(as_f(a[k]), as_f(old_d[k]), as_f(b[k]))); });
                break;
            case VecOp::Vnmsubfp128: // vD = -(vA * vB - vD)
                lanes([&](unsigned k) { return as_u(-std::fmaf(as_f(a[k]), as_f(b[k]), -as_f(old_d[k]))); });
                break;
            case VecOp::Vmsum3fp128:
            case VecOp::Vmsum4fp128: {   // produit scalaire (3 ou 4 mots), copie dans les 4 mots
                const unsigned n = i.vop == VecOp::Vmsum3fp128 ? 3 : 4;
                float sum = as_f(a[0]) * as_f(b[0]);
                for (unsigned k = 1; k < n; ++k) sum += as_f(a[k]) * as_f(b[k]);
                lanes([&](unsigned) { return as_u(sum); });
                break;
            }
            case VecOp::Vmaxfp:      lanes([&](unsigned k) { return fmax_bits(a[k], b[k]); }); break;
            case VecOp::Vminfp:      lanes([&](unsigned k) { return fmin_bits(a[k], b[k]); }); break;
            case VecOp::Vrefp:       lanes([&](unsigned k) { return as_u(1.0f / as_f(b[k])); }); break;
            case VecOp::Vrsqrtefp:   lanes([&](unsigned k) { return as_u(1.0f / std::sqrt(as_f(b[k]))); }); break;
            case VecOp::Vexptefp:    lanes([&](unsigned k) { return as_u(std::exp2(as_f(b[k]))); }); break;
            case VecOp::Vlogefp:     lanes([&](unsigned k) { return as_u(std::log2(as_f(b[k]))); }); break;
            case VecOp::Vrfin:       lanes([&](unsigned k) { return as_u(std::nearbyint(as_f(b[k]))); }); break;
            case VecOp::Vrfiz:       lanes([&](unsigned k) { return as_u(std::trunc(as_f(b[k]))); }); break;
            case VecOp::Vrfip:       lanes([&](unsigned k) { return as_u(std::ceil(as_f(b[k]))); }); break;
            case VecOp::Vrfim:       lanes([&](unsigned k) { return as_u(std::floor(as_f(b[k]))); }); break;
            case VecOp::Vcfsx:
                lanes([&](unsigned k) {
                    return as_u(std::ldexp(static_cast<float>(static_cast<std::int32_t>(b[k])), -static_cast<int>(imm)));
                });
                break;
            case VecOp::Vcfux:
                lanes([&](unsigned k) { return as_u(std::ldexp(static_cast<float>(b[k]), -static_cast<int>(imm))); });
                break;
            case VecOp::Vctsxs:
                lanes([&](unsigned k) { return float_to_s32(as_f(b[k]), static_cast<int>(imm), sat); });
                break;
            case VecOp::Vctuxs:
                lanes([&](unsigned k) { return float_to_u32(as_f(b[k]), static_cast<int>(imm), sat); });
                break;

            // ---- comparaisons : chaque mot vaut tout a 1 (vrai) ou 0 (faux) ----
            case VecOp::Vcmpeqfp:
                lanes([&](unsigned k) { return as_f(a[k]) == as_f(b[k]) ? 0xFFFFFFFFu : 0u; });
                break;
            case VecOp::Vcmpgefp:
                lanes([&](unsigned k) { return as_f(a[k]) >= as_f(b[k]) ? 0xFFFFFFFFu : 0u; });
                break;
            case VecOp::Vcmpgtfp:
                lanes([&](unsigned k) { return as_f(a[k]) > as_f(b[k]) ? 0xFFFFFFFFu : 0u; });
                break;
            case VecOp::Vcmpequw:
                lanes([&](unsigned k) { return a[k] == b[k] ? 0xFFFFFFFFu : 0u; });
                break;
            case VecOp::Vcmpbfp:   // bit 0 : a > b ; bit 1 : a < -b ; un NaN met les deux
                lanes([&](unsigned k) {
                    const float x = as_f(a[k]), y = as_f(b[k]);
                    if (std::isnan(x) || std::isnan(y)) return 0xC0000000u;
                    return (x <= y ? 0u : 0x80000000u) | (x >= -y ? 0u : 0x40000000u);
                });
                break;

            // ---- logique ----
            case VecOp::Vand:   lanes([&](unsigned k) { return a[k] & b[k]; }); break;
            case VecOp::Vandc:  lanes([&](unsigned k) { return a[k] & ~b[k]; }); break;
            case VecOp::Vor:    lanes([&](unsigned k) { return a[k] | b[k]; }); break;
            case VecOp::Vnor:   lanes([&](unsigned k) { return ~(a[k] | b[k]); }); break;
            case VecOp::Vxor:   lanes([&](unsigned k) { return a[k] ^ b[k]; }); break;
            case VecOp::Vsel: {   // bits de vC a 1 -> vB, a 0 -> vA
                const Vec c = cpu.vr[i.frc];
                lanes([&](unsigned k) { return (a[k] & ~c[k]) | (b[k] & c[k]); });
                break;
            }
            case VecOp::Vsel128:  // le masque est l'ancien vD
                lanes([&](unsigned k) { return (a[k] & ~old_d[k]) | (b[k] & old_d[k]); });
                break;

            // ---- entiers 32 bits ----
            case VecOp::Vadduwm: lanes([&](unsigned k) { return a[k] + b[k]; }); break;
            case VecOp::Vsubuwm: lanes([&](unsigned k) { return a[k] - b[k]; }); break;
            case VecOp::Vrlw:    lanes([&](unsigned k) { return std::rotl(a[k], static_cast<int>(b[k] & 31)); }); break;
            case VecOp::Vslw:    lanes([&](unsigned k) { return a[k] << (b[k] & 31); }); break;
            case VecOp::Vsrw:    lanes([&](unsigned k) { return a[k] >> (b[k] & 31); }); break;
            case VecOp::Vsraw:
                lanes([&](unsigned k) {
                    return static_cast<std::uint32_t>(static_cast<std::int32_t>(a[k]) >> (b[k] & 31));
                });
                break;

            // ---- permutations ----
            case VecOp::Vperm: {
                const Bytes ab = to_bytes(a), bb = to_bytes(b), cb = to_bytes(cpu.vr[i.frc]);
                Bytes out{};
                for (unsigned k = 0; k < 16; ++k) {
                    const unsigned idx = cb[k] & 0x1F;
                    out[k] = idx < 16 ? ab[idx] : bb[idx - 16];
                }
                r = from_bytes(out);
                break;
            }
            case VecOp::Vsldoi: {
                const Bytes ab = to_bytes(a), bb = to_bytes(b);
                Bytes out{};
                for (unsigned k = 0; k < 16; ++k) {
                    const unsigned idx = (imm & 15) + k;
                    out[k] = idx < 16 ? ab[idx] : bb[idx - 16];
                }
                r = from_bytes(out);
                break;
            }
            case VecOp::Vmrghw: r = {a[0], b[0], a[1], b[1]}; break;
            case VecOp::Vmrglw: r = {a[2], b[2], a[3], b[3]}; break;
            case VecOp::Vspltw: lanes([&](unsigned) { return b[imm & 3]; }); break;
            case VecOp::Vspltisw:
                lanes([&](unsigned) { return static_cast<std::uint32_t>(i.imm); });   // deja etendu avec signe par le decodeur
                break;
            case VecOp::Vslo:
            case VecOp::Vsro: {   // decalage de n octets, n = bits 3 a 6 de l'octet 15 de vB
                const unsigned n = (to_bytes(b)[15] >> 3) & 0xF;
                const Bytes ab = to_bytes(a);
                Bytes out{};
                for (unsigned k = 0; k < 16; ++k) {
                    if (i.vop == VecOp::Vslo) {
                        out[k] = k + n < 16 ? ab[k + n] : 0;
                    } else {
                        out[k] = k >= n ? ab[k - n] : 0;
                    }
                }
                r = from_bytes(out);
                break;
            }
            case VecOp::Vpermwi128:   // mot k du resultat = mot (PERM >> (6 - 2k)) & 3 de vB
                lanes([&](unsigned k) { return b[(imm >> (6 - 2 * k)) & 3]; });
                break;
            case VecOp::Vrlimi128: {  // vB tourne de Z mots vers la gauche, puis les mots dont le bit (3 - k) du masque vaut 1 remplacent ceux de vD
                r = old_d;
                for (unsigned k = 0; k < 4; ++k) {
                    if (((imm >> (3 - k)) & 1u) != 0) {
                        r[k] = b[(k + i.mb) & 3];
                    }
                }
                break;
            }

            // ---- conversions de taille ----
            case VecOp::Vpkuhum: r = from_bytes(pack_halves(a, b, false, 0, 0, true, sat)); break;
            case VecOp::Vpkuhus: r = from_bytes(pack_halves(a, b, false, 0, 255, false, sat)); break;
            case VecOp::Vpkshus: r = from_bytes(pack_halves(a, b, true, 0, 255, false, sat)); break;
            case VecOp::Vpkshss: r = from_bytes(pack_halves(a, b, true, -128, 127, false, sat)); break;
            case VecOp::Vpkuwum: r = pack_words(a, b, false, 0, 0, true, sat); break;
            case VecOp::Vpkuwus: r = pack_words(a, b, false, 0, 65535, false, sat); break;
            case VecOp::Vpkswus: r = pack_words(a, b, true, 0, 65535, false, sat); break;
            case VecOp::Vpkswss: r = pack_words(a, b, true, -32768, 32767, false, sat); break;
            case VecOp::Vupkhsb:
            case VecOp::Vupklsb: {   // 8 octets avec signe -> 8 demi-mots
                const Bytes bb = to_bytes(b);
                const unsigned from = i.vop == VecOp::Vupkhsb ? 0 : 8;
                for (unsigned k = 0; k < 4; ++k) {
                    const auto hi = static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<std::int8_t>(bb[from + 2 * k])));
                    const auto lo = static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<std::int8_t>(bb[from + 2 * k + 1])));
                    r[k] = (static_cast<std::uint32_t>(hi) << 16) | lo;
                }
                break;
            }

            default:   // Vpkd3d128, Vupkd3d128 : pas encore geres
                return Status::InvalidInstruction;
        }

        cpu.vr[i.rd] = r;
        if (sat) {
            cpu.vscr |= kVscrSat;
        }
        if (i.rc) {   // comparaisons "point" : CR6 = tous vrais (LT) / tous faux (EQ) ; vcmpbfp : EQ = tous dans les bornes
            if (i.vop == VecOp::Vcmpbfp) {
                set_cr6(cpu, (r[0] | r[1] | r[2] | r[3]) == 0 ? 2u : 0u);
            } else {
                const bool all_true = (r[0] & r[1] & r[2] & r[3]) == 0xFFFFFFFFu;
                const bool all_false = (r[0] | r[1] | r[2] | r[3]) == 0;
                set_cr6(cpu, (all_true ? 8u : 0u) | (all_false ? 2u : 0u));
            }
        }
        cpu.pc += 4;
        return Status::Running;
    } catch (const std::out_of_range&) {
        return Status::OutOfBounds;
    }
}

}  // namespace nerith::ppc
