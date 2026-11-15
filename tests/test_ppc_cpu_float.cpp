// Tests des instructions a virgule flottante : chargements, ecritures, calculs, conversions, comparaisons, FPSCR.
// Les valeurs attendues sont calculees a la main (puissances de 2, valeurs exactes en binaire).
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>

#include "nerith/memory/memory.h"
#include "nerith/ppc/assembler.h"
#include "nerith/ppc/cpu.h"
#include "nerith/ppc/decoder.h"

namespace {

using nerith::Memory;
using nerith::ppc::Cpu;
using nerith::ppc::Status;

int failures = 0;

void check(bool condition, const std::string& name) {
    if (!condition) {
        std::cerr << "ECHEC : " << name << "\n";
        ++failures;
    }
}

void check_value(std::uint64_t actual, std::uint64_t expected, const std::string& name) {
    if (actual != expected) {
        std::cerr << "ECHEC : " << name << " : obtenu 0x" << std::hex << actual << " au lieu de 0x" << expected
                  << std::dec << "\n";
        ++failures;
    }
}

std::uint64_t bits(double value) { return std::bit_cast<std::uint64_t>(value); }

using Setup = std::function<void(Cpu&)>;

Cpu one(const std::string& line, const Setup& setup, Memory& memory, Status* status = nullptr) {
    memory.write32(0, nerith::ppc::assemble(line).at(0));
    Cpu cpu;
    setup(cpu);
    const Status result = nerith::ppc::step(cpu, memory);
    if (status != nullptr) {
        *status = result;
    } else {
        check(result == Status::Running, line + " : doit s'executer");
    }
    return cpu;
}

Cpu one(const std::string& line, const Setup& setup) {
    Memory memory(256);
    return one(line, setup, memory);
}

constexpr std::uint64_t kDefaultNan = 0x7FF8000000000000ull;

}  // namespace

int main() {
    // --- lecture et ecriture ---
    {
        Memory memory(256);
        memory.write32(0x40, 0x3FF80000);   // 1.5 en double : 0x3FF8000000000000
        memory.write32(0x44, 0x00000000);
        Cpu c = one("lfd f1, 64(r0)", [](Cpu&) {}, memory);
        check_value(c.fpr[1], 0x3FF8000000000000ull, "lfd");

        memory.write32(0x50, 0x3FC00000);   // 1.5 en simple precision
        c = one("lfs f2, 80(r0)", [](Cpu&) {}, memory);
        check_value(c.fpr[2], 0x3FF8000000000000ull, "lfs : le simple est converti en double");
        memory.write32(0x50, 0x7F800000);
        c = one("lfs f2, 80(r0)", [](Cpu&) {}, memory);
        check_value(c.fpr[2], 0x7FF0000000000000ull, "lfs : l'infini reste l'infini");
        memory.write32(0x50, 0x00000001);   // le plus petit denormalise simple : 2^-149
        c = one("lfs f2, 80(r0)", [](Cpu&) {}, memory);
        check_value(c.fpr[2], bits(std::ldexp(1.0, -149)), "lfs : un denormalise simple devient un double normal");

        Memory m2(256);
        c = one("stfd f1, 32(r0)", [](Cpu& cpu) { cpu.fpr[1] = 0x0102030405060708ull; }, m2);
        check(m2.read32(32) == 0x01020304 && m2.read32(36) == 0x05060708, "stfd : 8 octets, big-endian");
        c = one("stfs f1, 48(r0)", [](Cpu& cpu) { cpu.fpr[1] = bits(1.5); }, m2);
        check(m2.read32(48) == 0x3FC00000, "stfs : 1.5 en simple precision");
        c = one("stfs f1, 52(r0)", [](Cpu& cpu) { cpu.fpr[1] = bits(0.1); }, m2);
        check(m2.read32(52) == 0x3DCCCCCD, "stfs : 0.1 arrondi a la precision simple");
        c = one("stfiwx f1, r3, r4", [](Cpu& cpu) { cpu.fpr[1] = 0x1122334455667788ull; cpu.gpr[3] = 8; cpu.gpr[4] = 56; }, m2);
        check(m2.read32(64) == 0x55667788, "stfiwx : les 32 bits de poids faible tels quels");

        // indexees, avec mise a jour
        Memory m3(256);
        m3.write32(0x60, 0x3FF00000);   // 1.0
        c = one("lfdx f3, r4, r5", [](Cpu& cpu) { cpu.gpr[4] = 0x50; cpu.gpr[5] = 0x10; }, m3);
        check_value(c.fpr[3], bits(1.0), "lfdx");
        c = one("lfdux f3, r4, r5", [](Cpu& cpu) { cpu.gpr[4] = 0x50; cpu.gpr[5] = 0x10; }, m3);
        check(c.fpr[3] == bits(1.0) && c.gpr[4] == 0x60, "lfdux : rA recoit l'adresse");
        c = one("lfdu f3, 16(r4)", [](Cpu& cpu) { cpu.gpr[4] = 0x50; }, m3);
        check(c.fpr[3] == bits(1.0) && c.gpr[4] == 0x60, "lfdu");
        c = one("stfdu f3, -8(r4)", [](Cpu& cpu) { cpu.gpr[4] = 0x70; cpu.fpr[3] = bits(2.0); }, m3);
        check(m3.read32(0x68) == 0x40000000 && c.gpr[4] == 0x68, "stfdu : ecrit puis met a jour rA");
        c = one("lfsx f1, 0, r5", [](Cpu& cpu) { cpu.gpr[5] = 0x60; }, m3);
        check_value(c.fpr[1], bits(static_cast<double>(std::bit_cast<float>(0x3FF00000u))),
                    "lfsx avec rA = 0 : le nombre 0");

        Status status = Status::Running;
        c = one("lfdu f3, 8(r0)", [](Cpu&) {}, m3, &status);
        check(status == Status::InvalidInstruction, "lfdu avec rA = 0 : invalide");

        // hors memoire : rien ne change
        Memory m4(256);
        m4.write8(255, 0x5A);
        c = one("lfd f1, 252(r0)", [](Cpu& cpu) { cpu.fpr[1] = 77; }, m4, &status);
        check(status == Status::OutOfBounds && c.fpr[1] == 77, "lfd a cheval sur la fin : OutOfBounds, f1 inchange");
        c = one("stfd f1, 252(r0)", [](Cpu& cpu) { cpu.fpr[1] = 77; }, m4, &status);
        check(status == Status::OutOfBounds && m4.read8(255) == 0x5A, "stfd a cheval : rien ecrit");
        c = one("lfdu f1, 252(r4)", [](Cpu& cpu) { cpu.gpr[4] = 0; cpu.fpr[1] = 77; }, m4, &status);
        check(status == Status::OutOfBounds && c.gpr[4] == 0, "lfdu hors memoire : rA inchange");
    }

    // --- calculs ---
    {
        const Setup ab = [](Cpu& c) { c.fpr[2] = bits(1.5); c.fpr[3] = bits(2.25); };
        check_value(one("fadd f1, f2, f3", ab).fpr[1], bits(3.75), "fadd");
        check_value(one("fsub f1, f2, f3", ab).fpr[1], bits(-0.75), "fsub");
        check_value(one("fmul f1, f2, f3", ab).fpr[1], bits(3.375), "fmul : frA * frC");
        check_value(one("fdiv f1, f3, f2", ab).fpr[1], bits(1.5), "fdiv : 2.25 / 1.5");
        check_value(one("fdiv f1, f2, f4", [](Cpu& c) { c.fpr[2] = bits(1.0); }).fpr[1], 0x7FF0000000000000ull,
                    "fdiv par zero : +infini");
        check_value(one("fdiv f1, f4, f4", [](Cpu&) {}).fpr[1], kDefaultNan, "0 / 0 : le NaN par defaut 0x7FF8...");
        check_value(one("fsub f1, f2, f2", [](Cpu& c) { c.fpr[2] = 0x7FF0000000000000ull; }).fpr[1], kDefaultNan,
                    "infini - infini : le NaN par defaut");
        check_value(one("fadd f1, f2, f3", [](Cpu& c) { c.fpr[2] = 0x7FF0000000000001ull; c.fpr[3] = bits(1.0); }).fpr[1],
                    0x7FF8000000000001ull, "NaN en entree : propage et rendu silencieux");

        // simple precision : le resultat est arrondi a 32 bits
        const Setup tenth = [](Cpu& c) { c.fpr[2] = bits(0.1); c.fpr[3] = bits(0.2); };
        check_value(one("fadd f1, f2, f3", tenth).fpr[1], bits(0.1 + 0.2), "fadd : precision double");
        check_value(one("fadds f1, f2, f3", tenth).fpr[1], bits(static_cast<double>(0.3f)), "fadds : arrondi a 32 bits");
        check_value(one("fmuls f1, f2, f3", ab).fpr[1], bits(3.375), "fmuls : 3.375 est exact en simple");
        check_value(one("fsubs f1, f2, f3", ab).fpr[1], bits(-0.75), "fsubs");
        check_value(one("fdivs f1, f3, f2", ab).fpr[1], bits(1.5), "fdivs");

        // multiplication-addition : frA * frC + frB, avec UN SEUL arrondi
        const Setup mad = [](Cpu& c) { c.fpr[2] = bits(2.0); c.fpr[3] = bits(3.0); c.fpr[4] = bits(4.0); };
        check_value(one("fmadd f1, f2, f3, f4", mad).fpr[1], bits(10.0), "fmadd : 2 * 3 + 4");
        check_value(one("fmsub f1, f2, f3, f4", mad).fpr[1], bits(2.0), "fmsub : 2 * 3 - 4");
        check_value(one("fnmadd f1, f2, f3, f4", mad).fpr[1], bits(-10.0), "fnmadd");
        check_value(one("fnmsub f1, f2, f3, f4", mad).fpr[1], bits(-2.0), "fnmsub");
        check_value(one("fmadds f1, f2, f3, f4", mad).fpr[1], bits(10.0), "fmadds");
        check_value(one("fnmsubs f1, f2, f3, f4", mad).fpr[1], bits(-2.0), "fnmsubs");
        // (1 + 2^-27) * (1 - 2^-27) = 1 - 2^-54 exactement ; avec un seul arrondi, ajouter -1 donne -2^-54
        const Setup fused = [](Cpu& c) {
            c.fpr[2] = bits(1.0 + std::ldexp(1.0, -27));
            c.fpr[3] = bits(1.0 - std::ldexp(1.0, -27));
            c.fpr[4] = bits(-1.0);
        };
        check_value(one("fmadd f1, f2, f3, f4", fused).fpr[1], bits(-std::ldexp(1.0, -54)), "fmadd : un seul arrondi");

        // fsel : frD = (frA >= 0) ? frC : frB
        const std::uint64_t pick_c = bits(30.0);
        const std::uint64_t pick_b = bits(20.0);
        auto sel = [&](double a) {
            return one("fsel f1, f2, f3, f4", [&](Cpu& c) { c.fpr[2] = bits(a); c.fpr[3] = pick_c; c.fpr[4] = pick_b; }).fpr[1];
        };
        check_value(sel(1.0), pick_c, "fsel : positif -> frC");
        check_value(sel(-0.0), pick_c, "fsel : -0 compte comme >= 0");
        check_value(sel(-1.0), pick_b, "fsel : negatif -> frB");
        check_value(one("fsel f1, f2, f3, f4", [&](Cpu& c) { c.fpr[2] = 0x7FF8000000000000ull; c.fpr[3] = pick_c; c.fpr[4] = pick_b; }).fpr[1],
                    pick_b, "fsel : NaN -> frB");

        check_value(one("fsqrt f1, f2", [](Cpu& c) { c.fpr[2] = bits(16.0); }).fpr[1], bits(4.0), "fsqrt");
        check_value(one("fsqrts f1, f2", [](Cpu& c) { c.fpr[2] = bits(2.25); }).fpr[1], bits(1.5), "fsqrts");
        check_value(one("fres f1, f2", [](Cpu& c) { c.fpr[2] = bits(4.0); }).fpr[1], bits(0.25), "fres");
        check_value(one("frsqrte f1, f2", [](Cpu& c) { c.fpr[2] = bits(4.0); }).fpr[1], bits(0.5), "frsqrte");
    }

    // --- copies et conversions ---
    {
        const std::uint64_t odd_nan = 0x7FF4000000000001ull;   // un NaN "signalant" avec une charge utile
        check_value(one("fmr f1, f2", [&](Cpu& c) { c.fpr[2] = odd_nan; }).fpr[1], odd_nan, "fmr : copie exacte, meme un NaN");
        check_value(one("fneg f1, f2", [](Cpu& c) { c.fpr[2] = bits(2.0); }).fpr[1], bits(-2.0), "fneg");
        check_value(one("fabs f1, f2", [](Cpu& c) { c.fpr[2] = bits(-2.0); }).fpr[1], bits(2.0), "fabs");
        check_value(one("fnabs f1, f2", [](Cpu& c) { c.fpr[2] = bits(2.0); }).fpr[1], bits(-2.0), "fnabs");
        check_value(one("fneg f1, f2", [&](Cpu& c) { c.fpr[2] = odd_nan; }).fpr[1], odd_nan | 0x8000000000000000ull,
                    "fneg : seul le signe change");

        check_value(one("frsp f1, f2", [](Cpu& c) { c.fpr[2] = bits(0.1); }).fpr[1], bits(static_cast<double>(0.1f)), "frsp");
        check_value(one("frsp f1, f2", [](Cpu& c) { c.fpr[2] = bits(1e40); }).fpr[1], 0x7FF0000000000000ull, "frsp : trop grand -> infini");

        // fctiwz : vers zero ; les 32 bits de poids fort sont indefinis (0xFFF80000 ici)
        auto ctiwz = [](double v) { return one("fctiwz f1, f2", [&](Cpu& c) { c.fpr[2] = bits(v); }).fpr[1]; };
        check_value(ctiwz(3.9), 0xFFF8000000000003ull, "fctiwz 3.9");
        check_value(ctiwz(-3.9), 0xFFF80000FFFFFFFDull, "fctiwz -3.9");
        check_value(ctiwz(3e10), 0xFFF800007FFFFFFFull, "fctiwz : trop grand, sature");
        check_value(ctiwz(-3e10), 0xFFF8000080000000ull, "fctiwz : trop petit, sature");
        check_value(ctiwz(std::nan("")), 0xFFF8000080000000ull, "fctiwz : NaN");
        // fctiw suit le mode d'arrondi de FPSCR (bits 30-31) : 0 = plus proche pair, 1 = zero, 2 = +inf, 3 = -inf
        auto ctiw = [](double v, std::uint32_t mode) {
            return one("fctiw f1, f2", [&](Cpu& c) { c.fpr[2] = bits(v); c.fpscr = mode; }).fpr[1] & 0xFFFFFFFFull;
        };
        check_value(ctiw(2.5, 0), 2, "fctiw 2.5 au plus proche : 2 (pair)");
        check_value(ctiw(3.5, 0), 4, "fctiw 3.5 au plus proche : 4 (pair)");
        check_value(ctiw(2.7, 1), 2, "fctiw 2.7 vers zero");
        check_value(ctiw(2.1, 2), 3, "fctiw 2.1 vers +inf");
        check_value(ctiw(-2.1, 3), 0xFFFFFFFD, "fctiw -2.1 vers -inf : -3");
        auto ctidz = [](double v) { return one("fctidz f1, f2", [&](Cpu& c) { c.fpr[2] = bits(v); }).fpr[1]; };
        check_value(ctidz(3.9), 3, "fctidz 3.9");
        check_value(ctidz(-1e30), 0x8000000000000000ull, "fctidz : sature");
        check_value(ctidz(1e30), 0x7FFFFFFFFFFFFFFFull, "fctidz : sature en haut");
        check_value(one("fctid f1, f2", [](Cpu& c) { c.fpr[2] = bits(2.5); }).fpr[1], 2, "fctid 2.5 : pair");
        check_value(one("fcfid f1, f2", [](Cpu& c) { c.fpr[2] = 0xFFFFFFFFFFFFFFFEull; }).fpr[1], bits(-2.0), "fcfid : -2");
        check_value(one("fcfid f1, f2", [](Cpu& c) { c.fpr[2] = 1ull << 40; }).fpr[1], bits(std::ldexp(1.0, 40)), "fcfid : 2^40");
    }

    // --- comparaisons : LT = 8, GT = 4, EQ = 2, non ordonnes = 1, dans le champ choisi et dans FPSCR (bits 16-19) ---
    {
        Cpu c = one("fcmpu cr0, f2, f3", [](Cpu& cpu) { cpu.fpr[2] = bits(1.0); cpu.fpr[3] = bits(2.0); });
        check(c.cr == 0x80000000 && (c.fpscr & 0xF000) == 0x8000, "fcmpu 1 < 2 : LT");
        c = one("fcmpu cr1, f2, f3", [](Cpu& cpu) { cpu.fpr[2] = bits(2.0); cpu.fpr[3] = bits(1.0); });
        check(c.cr == 0x04000000 && (c.fpscr & 0xF000) == 0x4000, "fcmpu cr1 : GT dans le champ 1");
        c = one("fcmpu cr7, f2, f3", [](Cpu& cpu) { cpu.fpr[2] = bits(0.0); cpu.fpr[3] = bits(-0.0); });
        check(c.cr == 0x00000002 && (c.fpscr & 0xF000) == 0x2000, "fcmpu cr7 : +0 == -0");
        c = one("fcmpu cr0, f2, f3", [](Cpu& cpu) { cpu.fpr[2] = kDefaultNan; cpu.fpr[3] = bits(1.0); cpu.xer = 0x80000000; });
        check(c.cr == 0x10000000 && (c.fpscr & 0xF000) == 0x1000, "fcmpu avec un NaN : non ordonnes (le bit SO du champ)");
        c = one("fcmpu cr0, f2, f3", [](Cpu& cpu) { cpu.fpr[2] = bits(1.0); cpu.fpr[3] = kDefaultNan; });
        check(c.cr == 0x10000000, "fcmpu : un NaN dans frB aussi donne \"non ordonnes\"");
        c = one("fcmpo cr0, f2, f3", [](Cpu& cpu) { cpu.fpr[2] = bits(3.0); cpu.fpr[3] = bits(3.0); cpu.cr = 0xFFFFFFFF; });
        check(c.cr == 0x2FFFFFFF, "fcmpo : seul le champ cr0 change");
        c = one("fcmpu cr0, f2, f3", [](Cpu& cpu) { cpu.fpr[2] = bits(1.0); cpu.fpr[3] = bits(2.0); cpu.xer = 0x80000000; });
        check(c.cr == 0x80000000, "fcmpu : XER.SO n'est pas melange dans le champ");
    }

    // --- registre FPSCR ---
    {
        check_value(one("mffs f1", [](Cpu& c) { c.fpscr = 0x00000003; }).fpr[1], 0xFFF8000000000003ull, "mffs");
        check_value(one("mtfsf 255, f2", [](Cpu& c) { c.fpr[2] = 0xAAAAAAAAFFFFFFFFull; }).fpscr, 0x9FFFFFFF,
                    "mtfsf 255 : FEX et VX (bits 1 et 2) ne s'ecrivent pas");
        check_value(one("mtfsf 1, f2", [](Cpu& c) { c.fpr[2] = 0x12345678; c.fpscr = 0xA0000000; }).fpscr, 0xA0000008,
                    "mtfsf 1 : seul le dernier champ (bits 28 a 31) change");
        check_value(one("mtfsb1 31", [](Cpu&) {}).fpscr, 1, "mtfsb1 31");
        check_value(one("mtfsb1 1", [](Cpu&) {}).fpscr, 0, "mtfsb1 1 : FEX ne s'ecrit pas");
        check_value(one("mtfsb0 31", [](Cpu& c) { c.fpscr = 3; }).fpscr, 2, "mtfsb0 31");
        check_value(one("mtfsfi 7, 3", [](Cpu& c) { c.fpscr = 0xFFFFFFF0; }).fpscr, 0xFFFFFFF3, "mtfsfi 7, 3");
        const Cpu c = one("mcrfs cr3, cr0", [](Cpu& cpu) { cpu.fpscr = 0x90000000; });
        check(c.cr == 0x00090000 && c.fpscr == 0, "mcrfs : copie le champ, efface FX et OX");
        // l'arrondi choisi par mtfsfi sert a fctiw
        Memory memory(256);
        memory.write32(0, nerith::ppc::assemble("mtfsfi 7, 3").at(0));
        memory.write32(4, nerith::ppc::assemble("fctiw f1, f2").at(0));
        Cpu cpu;
        cpu.fpr[2] = bits(-2.1);
        nerith::ppc::step(cpu, memory);
        nerith::ppc::step(cpu, memory);
        check_value(cpu.fpr[1] & 0xFFFFFFFFull, 0xFFFFFFFD, "mtfsfi 7, 3 (vers -inf) puis fctiw -2.1 : -3");
    }

    // --- formes refusees ---
    {
        check(nerith::ppc::decode(0xFC22182B).kind == nerith::ppc::Kind::Fadd && nerith::ppc::decode(0xFC22182B).rc, "fadd. (Rc = 1) : decode");
        check(nerith::ppc::decode(0xFC011001).kind == nerith::ppc::Kind::Unknown, "fcmpu avec Rc = 1 : refuse");
        check(nerith::ppc::decode(0xFC2218EA).kind == nerith::ppc::Kind::Unknown, "fadd avec frC non nul : invalide");
        check(nerith::ppc::decode(0xFC211090).kind == nerith::ppc::Kind::Unknown, "fmr avec frA non nul : invalide");
    }

    // --- un petit programme : (3 / 4)^2 + 3 / 4, converti en entier ---
    {
        Memory memory(0x200);
        const auto words = nerith::ppc::assemble(
            "        li r4, 0x100\n"
            "        li r3, 3\n"
            "        std r3, 0(r4)\n"
            "        lfd f1, 0(r4)\n"
            "        fcfid f1, f1           ; f1 = 3.0\n"
            "        li r3, 4\n"
            "        std r3, 8(r4)\n"
            "        lfd f2, 8(r4)\n"
            "        fcfid f2, f2           ; f2 = 4.0\n"
            "        fdiv f3, f1, f2        ; 0.75\n"
            "        fmul f4, f3, f3        ; 0.5625\n"
            "        fadd f5, f4, f3        ; 1.3125\n"
            "        stfd f5, 16(r4)\n"
            "        lwz r5, 16(r4)\n"
            "        fctiwz f6, f3\n"
            "        blr\n");
        for (std::size_t n = 0; n < words.size(); n++) {
            memory.write32(n * 4, words[n]);
        }
        Cpu cpu;
        check(nerith::ppc::run(cpu, memory, 100) == Status::Halted, "programme flottant : doit se terminer");
        check_value(cpu.fpr[5], bits(1.3125), "programme : f5 = 1.3125");
        check_value(cpu.gpr[5], 0x3FF50000, "programme : mot de poids fort de 1.3125 en memoire");
        check_value(cpu.fpr[6] & 0xFFFFFFFFull, 0, "programme : 0.75 converti vers zero = 0");
    }

    if (failures == 0) {
        std::cout << "Tous les tests des flottants passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
