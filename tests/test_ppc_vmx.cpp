// Tests des instructions vectorielles : AltiVec (v0-v31) et VMX128 de la Xbox 360 (v0-v127).
// Les valeurs attendues sont calculees a la main. Les encodages ont ete verifies a part (llvm pour AltiVec,
// le patch binutils de Xenia pour VMX128) ; ici on verifie le texte, l'aller-retour et surtout ce que font les instructions.
#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <iostream>
#include <random>
#include <string>

#include "nerith/memory/memory.h"
#include "nerith/ppc/assembler.h"
#include "nerith/ppc/cpu.h"
#include "nerith/ppc/decoder.h"

namespace {

using nerith::Memory;
using nerith::ppc::Cpu;
using nerith::ppc::Kind;
using nerith::ppc::Status;
using Vec = std::array<std::uint32_t, 4>;

int failures = 0;

void check(bool condition, const std::string& name) {
    if (!condition) {
        std::cerr << "ECHEC : " << name << "\n";
        ++failures;
    }
}

void check_vec(const Vec& actual, const Vec& expected, const std::string& name) {
    if (actual != expected) {
        std::cerr << std::hex << "ECHEC : " << name << " : obtenu " << actual[0] << " " << actual[1] << " " << actual[2]
                  << " " << actual[3] << " au lieu de " << expected[0] << " " << expected[1] << " " << expected[2]
                  << " " << expected[3] << std::dec << "\n";
        ++failures;
    }
}

constexpr std::uint64_t kStart = 0x100;
constexpr std::size_t kData = 0x200;

std::uint32_t F(float x) { return std::bit_cast<std::uint32_t>(x); }
Vec FV(float a, float b, float c, float d) { return {F(a), F(b), F(c), F(d)}; }

using Setup = std::function<void(Cpu&, Memory&)>;

// Execute UNE instruction (texte) avec une memoire de 0x400 octets dont les octets 0x200.. valent 0xA0 + k.
Cpu one(const std::string& line, const Setup& setup, Memory* keep = nullptr, Status* status = nullptr) {
    Memory local(0x400);
    Memory& memory = keep != nullptr ? *keep : local;
    for (std::size_t k = 0; k < 32; ++k) {
        memory.write8(kData + k, static_cast<std::uint8_t>(0xA0 + k));
    }
    memory.write32(kStart, nerith::ppc::assemble(line).at(0));
    Cpu cpu;
    cpu.pc = kStart;
    setup(cpu, memory);
    const Status result = nerith::ppc::step(cpu, memory);
    if (status != nullptr) {
        *status = result;
    } else {
        check(result == Status::Running, line + " : doit s'executer");
    }
    return cpu;
}

Cpu one(const std::string& line, const std::function<void(Cpu&)>& setup) {
    return one(line, [&](Cpu& c, Memory&) { setup(c); });
}

const Vec kBytes0 = {0x00010203, 0x04050607, 0x08090A0B, 0x0C0D0E0F};   // octets 0 a 15
const Vec kBytes16 = {0x10111213, 0x14151617, 0x18191A1B, 0x1C1D1E1F};  // octets 16 a 31

std::string text(std::uint32_t word) { return nerith::ppc::to_text(nerith::ppc::decode(word)); }

}  // namespace

int main() {
    // ---------- textes et aller-retour assembleur / decodeur ----------
    const char* roundtrip[] = {
        "lvx v3, 0, r5", "lvx v31, r4, r5", "stvx v1, r2, r3", "lvewx v9, 0, r7", "lvsl v2, 0, r9", "lvlx v4, r5, r6",
        "stvrxl v8, 0, r1", "vaddfp v1, v2, v3", "vsubfp v31, v0, v17", "vor v3, v4, v5", "vmr v3, v4", "vnot v3, v4",
        "vmaddfp v3, v4, v6, v5", "vnmsubfp v3, v4, v6, v5", "vperm v1, v2, v3, v4", "vsel v1, v2, v3, v4",
        "vsldoi v1, v2, v3, 15", "vspltw v1, v2, 3", "vspltisw v1, -16", "vcmpgtfp. v1, v2, v3", "vcfsx v1, v2, 31",
        "vrefp v1, v2", "vpkuhum v1, v2, v3", "vupkhsb v1, v2",
        "lvx128 v127, 0, r5", "stvx128 v64, r2, r3", "lvlx128 v100, 0, r9", "vaddfp128 v127, v64, v33",
        "vmulfp128 v1, v100, v127", "vmaddfp128 v100, v70, v127, v100", "vmaddcfp128 v100, v70, v100, v127",
        "vnmsubfp128 v3, v4, v5, v3", "vsel128 v90, v1, v2, v90", "vperm128 v100, v70, v127, v7",
        "vsldoi128 v100, v70, v127, 9", "vcmpeqfp128. v1, v2, v3", "vcmpequw128 v100, v1, v2",
        "vpermwi128 v100, v127, 255", "vrlimi128 v100, v127, 31, 3", "vspltisw128 v100, v127, -16",
        "vspltw128 v100, v127, 3", "vcsxwfp128 v1, v2, 31", "vcfpuxws128 v1, v2, 0", "vmsum3fp128 v1, v2, v3",
        "vrfin128 v100, v127", "vpkd3d128 v100, v127, 7, 3, 3", "vupkd3d128 v100, v127, 3", "vupkhsb128 v100, v127",
        "vand128 v1, v2, v3", "vandc128 v1, v2, v3", "vnor128 v100, v64, v33", "vslo128 v1, v2, v3", "vmrghw128 v1, v2, v3",
    };
    for (const char* line : roundtrip) {
        const std::uint32_t word = nerith::ppc::assemble(line).at(0);
        check(text(word) == line, std::string("aller-retour : ") + line + " -> " + text(word));
    }
    // quelques mots ecrits "a la main" (encodages verifies avec llvm)
    check(text(0x7C6028CE) == "lvx v3, 0, r5", "lvx : mot connu");
    check(text(0x1064280A) == "vaddfp v3, v4, v5", "vaddfp : mot connu");
    check(text(0x106429AE) == "vmaddfp v3, v4, v6, v5", "vmaddfp : ordre vA, vC, vB (comme llvm)");

    // bits reserves : un mot avec un bit reserve non nul n'est pas reconnu
    check(nerith::ppc::decode(0x1000010A).kind == Kind::Vector, "vrefp v0, v0 : valide");
    check(nerith::ppc::decode(0x1001010A).kind == Kind::Unknown, "vrefp avec le champ vA non nul : refuse");
    check(nerith::ppc::decode(0x7C6028CF).kind == Kind::Unknown, "lvx avec le bit 31 : refuse");

    // aller-retour sur des mots au hasard : si le mot est vectoriel, texte -> assembleur redonne le meme mot
    {
        std::mt19937 rng(12345);
        int vectors = 0;
        for (int k = 0; k < 400000; ++k) {
            std::uint32_t word = rng();
            const unsigned ops[] = {4, 5, 6, 31};
            word = (word & 0x03FFFFFF) | (ops[k % 4] << 26);
            const auto decoded = nerith::ppc::decode(word);
            if (decoded.kind != Kind::Vector) {
                continue;
            }
            ++vectors;
            const std::string t = nerith::ppc::to_text(decoded);
            std::uint32_t again = 0;
            try {
                again = nerith::ppc::assemble(t).at(0);
            } catch (const std::exception& e) {
                check(false, "aller-retour aleatoire : " + t + " : " + e.what());
                continue;
            }
            if (again != word) {
                std::cerr << std::hex << "ECHEC : aller-retour aleatoire " << word << " -> " << t << " -> " << again
                          << std::dec << "\n";
                ++failures;
            }
        }
        check(vectors > 10000, "aller-retour aleatoire : assez de mots vectoriels testes");
    }

    // erreurs de l'assembleur
    {
        const char* bad[] = {"vaddfp v32, v0, v0", "vaddfp128 v128, v0, v0", "vperm128 v1, v2, v3, v8",
                             "vmaddfp128 v1, v2, v3, v4", "vsldoi v1, v2, v3, 16", "vspltw v1, v2, 4",
                             "vspltisw v1, 16", "lvx v1, r40, r5", "lvx v1, 0", "vcfsx v1, v2, 32"};
        for (const char* line : bad) {
            bool failed = false;
            try {
                (void)nerith::ppc::assemble(line);
            } catch (const nerith::ppc::AssemblyError&) {
                failed = true;
            }
            check(failed, std::string("doit etre refuse : ") + line);
        }
    }

    // ---------- lectures et ecritures ----------
    {
        Cpu c = one("lvx v1, 0, r5", [](Cpu& cpu) { cpu.gpr[5] = kData + 7; });   // adresse alignee sur 16 : 0x200
        check_vec(c.vr[1], {0xA0A1A2A3, 0xA4A5A6A7, 0xA8A9AAAB, 0xACADAEAF}, "lvx : bloc de 16 octets aligne");
        Cpu c2 = one("lvx128 v127, r4, r5", [](Cpu& cpu) { cpu.gpr[4] = kData; cpu.gpr[5] = 0x10; });
        check_vec(c2.vr[127], {0xB0B1B2B3, 0xB4B5B6B7, 0xB8B9BABB, 0xBCBDBEBF}, "lvx128 : v127, adresse rA + rB");
        Cpu c3 = one("lvsl v1, 0, r5", [](Cpu& cpu) { cpu.gpr[5] = kData + 5; });
        check_vec(c3.vr[1], {0x05060708, 0x090A0B0C, 0x0D0E0F10, 0x11121314}, "lvsl : decalage 5");
        Cpu c4 = one("lvsr v1, 0, r5", [](Cpu& cpu) { cpu.gpr[5] = kData + 5; });
        check_vec(c4.vr[1], {0x0B0C0D0E, 0x0F101112, 0x13141516, 0x1718191A}, "lvsr : decalage 5");
        Cpu c5 = one("lvewx v1, 0, r5", [](Cpu& cpu) {
            cpu.gpr[5] = kData + 0xC;
            cpu.vr[1] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};
        });
        check_vec(c5.vr[1], {0x11111111, 0x22222222, 0x33333333, 0xACADAEAF}, "lvewx : le mot va a sa place, le reste ne bouge pas");
        Cpu c6 = one("lvebx v1, 0, r5", [](Cpu& cpu) {
            cpu.gpr[5] = kData + 3;
            cpu.vr[1] = {0, 0, 0, 0};
        });
        check_vec(c6.vr[1], {0x00000000 | 0xA3, 0, 0, 0}, "lvebx : l'octet va a la place 3 -> {0,0,0,0xA3} dans le mot 0");
        Cpu c7 = one("lvlx v1, 0, r5", [](Cpu& cpu) { cpu.gpr[5] = kData + 5; });
        check_vec(c7.vr[1], {0xA5A6A7A8, 0xA9AAABAC, 0xADAEAF00, 0}, "lvlx : de l'adresse a la fin du bloc, cale a gauche");
        Cpu c8 = one("lvrx v1, 0, r5", [](Cpu& cpu) { cpu.gpr[5] = kData + 5; });
        check_vec(c8.vr[1], {0, 0, 0x000000A0, 0xA1A2A3A4}, "lvrx : du debut du bloc a l'adresse, cale a droite");
        Cpu c9 = one("lvrx v1, 0, r5", [](Cpu& cpu) { cpu.gpr[5] = kData; cpu.vr[1] = {1, 2, 3, 4}; });
        check_vec(c9.vr[1], {0, 0, 0, 0}, "lvrx : adresse alignee -> zero");

        Memory memory(0x400);
        one("stvx v1, 0, r5", [](Cpu& cpu, Memory&) { cpu.gpr[5] = kData + 0x23; cpu.vr[1] = kBytes0; }, &memory);
        check(memory.read32(kData + 0x20) == 0x00010203 && memory.read32(kData + 0x2C) == 0x0C0D0E0F, "stvx : bloc aligne");
        Memory m2(0x400);
        one("stvewx v1, 0, r5", [](Cpu& cpu, Memory&) { cpu.gpr[5] = kData + 8; cpu.vr[1] = {1, 2, 3, 4}; }, &m2);
        check(m2.read32(kData + 8) == 3 && m2.read32(kData + 4) == 0xA4A5A6A7 && m2.read32(kData + 12) == 0xACADAEAF,
              "stvewx : ecrit le mot 2 seulement");
        Memory m3(0x400);
        one("stvlx v1, 0, r5", [](Cpu& cpu, Memory&) { cpu.gpr[5] = kData + 0x15; cpu.vr[1] = kBytes0; }, &m3);
        // memoire initiale en 0x210.. : 0 (la boucle d'init ne couvre que 0x200-0x21F : 0xB0+k), ici on verifie l'ecriture
        check(m3.read8(kData + 0x14) == 0xB4 && m3.read8(kData + 0x15) == 0x00 && m3.read8(kData + 0x1F) == 0x0A,
              "stvlx : octets 0 a 10 du registre en 0x215-0x21F, avant 0x215 intact");
        Memory m4(0x400);
        one("stvrx v1, 0, r5", [](Cpu& cpu, Memory&) { cpu.gpr[5] = kData + 0x15; cpu.vr[1] = kBytes0; }, &m4);
        check(m4.read8(kData + 0x10) == 0x0B && m4.read8(kData + 0x14) == 0x0F && m4.read8(kData + 0x15) == 0xB5,
              "stvrx : 5 derniers octets du registre en 0x210-0x214, 0x215 intact");
        Memory m5(0x400);
        one("stvx128 v100, 0, r5", [](Cpu& cpu, Memory&) { cpu.gpr[5] = kData; cpu.vr[100] = kBytes16; }, &m5);
        check(m5.read32(kData) == 0x10111213 && m5.read32(kData + 12) == 0x1C1D1E1F, "stvx128 : v100");

        // erreurs : rien ne change
        Status st = Status::Running;
        Cpu e1 = one("lvx v1, 0, r5", [](Cpu& cpu, Memory&) { cpu.gpr[5] = 0x1000; cpu.vr[1] = {1, 2, 3, 4}; }, nullptr, &st);
        check(st == Status::OutOfBounds && e1.vr[1] == Vec{1, 2, 3, 4} && e1.pc == kStart, "lvx hors memoire : rien ne change");
        Memory m6(0x3F8);   // le bloc de 16 octets en 0x3F0 depasse la fin de la memoire
        one("stvx v1, 0, r5", [](Cpu& cpu, Memory&) { cpu.gpr[5] = 0x3F0; cpu.vr[1] = kBytes0; }, &m6, &st);
        check(st == Status::OutOfBounds && m6.read8(0x3F0) == 0, "stvx a cheval sur la fin de la memoire : rien n'est ecrit");
    }

    // ---------- flottants ----------
    {
        auto two = [](const std::string& line, const Vec& a, const Vec& b) {
            return one(line, [&](Cpu& c) { c.vr[2] = a; c.vr[3] = b; });
        };
        check_vec(two("vaddfp v1, v2, v3", FV(1, 2, -3.5f, 0.5f), FV(0.5f, 2, 3.5f, 0.25f)).vr[1],
                  {0x3FC00000, 0x40800000, 0x00000000, 0x3F400000}, "vaddfp");
        check_vec(two("vsubfp v1, v2, v3", FV(1, 2, 3, 4), FV(0.5f, 2, 5, 1)).vr[1], FV(0.5f, 0, -2, 3), "vsubfp");
        check_vec(two("vmulfp128 v1, v2, v3", FV(1.5f, 2, -3, 0), FV(2, 2, 2, 5)).vr[1], FV(3, 4, -6, 0), "vmulfp128");
        // vmaddfp vD, vA, vC, vB : vA * vC + vB
        Cpu m = one("vmaddfp v1, v2, v4, v3", [](Cpu& c) { c.vr[2] = FV(2, 2, 2, 2); c.vr[4] = FV(3, 3, 3, 3); c.vr[3] = FV(1, 1, 1, 1); });
        check_vec(m.vr[1], FV(7, 7, 7, 7), "vmaddfp : 2 * 3 + 1");
        Cpu n = one("vnmsubfp v1, v2, v4, v3", [](Cpu& c) { c.vr[2] = FV(2, 2, 2, 2); c.vr[4] = FV(3, 3, 3, 3); c.vr[3] = FV(1, 1, 1, 1); });
        check_vec(n.vr[1], FV(-5, -5, -5, -5), "vnmsubfp : -(2 * 3 - 1)");
        auto three = [](const std::string& line) {
            return one(line, [](Cpu& c) { c.vr[100] = FV(3, 3, 3, 3); c.vr[70] = FV(2, 2, 2, 2); c.vr[127] = FV(5, 5, 5, 5); });
        };
        check_vec(three("vmaddfp128 v100, v70, v127, v100").vr[100], FV(13, 13, 13, 13), "vmaddfp128 : vA * vB + vD = 2*5+3");
        check_vec(three("vmaddcfp128 v100, v70, v100, v127").vr[100], FV(11, 11, 11, 11), "vmaddcfp128 : vA * vD + vB = 2*3+5");
        check_vec(three("vnmsubfp128 v100, v70, v127, v100").vr[100], FV(-7, -7, -7, -7), "vnmsubfp128 : -(vA * vB - vD) = -(10-3)");
        check_vec(two("vmsum3fp128 v1, v2, v3", FV(1, 2, 3, 100), FV(4, 5, 6, 100)).vr[1], FV(32, 32, 32, 32), "vmsum3fp128 : 4+10+18");
        check_vec(two("vmsum4fp128 v1, v2, v3", FV(1, 2, 3, 2), FV(4, 5, 6, 3)).vr[1], {0x42180000, 0x42180000, 0x42180000, 0x42180000},
                  "vmsum4fp128 : 4+10+18+6 = 38");
        check_vec(two("vmaxfp v1, v2, v3", {0x00000000, F(1), F(-1), 0x80000000}, {0x80000000, F(3), F(-2), 0x00000000}).vr[1],
                  {0x00000000, F(3), F(-1), 0x00000000}, "vmaxfp : +0 contre -0 donne +0");
        check_vec(two("vminfp v1, v2, v3", {0x00000000, F(1), F(-1), 0x80000000}, {0x80000000, F(3), F(-2), 0x00000000}).vr[1],
                  {0x80000000, F(1), F(-2), 0x80000000}, "vminfp : +0 contre -0 donne -0");
        // arrondis
        auto one_in = [](const std::string& line, const Vec& b) { return one(line, [&](Cpu& c) { c.vr[3] = b; }).vr[1]; };
        check_vec(one_in("vrfin v1, v3", FV(1.5f, 2.5f, -0.5f, 3.5f)), FV(2, 2, -0.0f, 4), "vrfin : au plus proche, pair en cas d'egalite");
        check_vec(one_in("vrfiz v1, v3", FV(1.5f, -1.5f, 2.9f, -0.2f)), FV(1, -1, 2, -0.0f), "vrfiz : vers zero");
        check_vec(one_in("vrfip v1, v3", FV(1.5f, -1.5f, 2.1f, -0.2f)), FV(2, -1, 3, -0.0f), "vrfip : vers +infini");
        check_vec(one_in("vrfim v1, v3", FV(1.5f, -1.5f, 2.9f, -0.2f)), FV(1, -2, 2, -1), "vrfim : vers -infini");
        check_vec(one_in("vrefp v1, v3", FV(4, 0.5f, -2, 1)), FV(0.25f, 2, -0.5f, 1), "vrefp (valeur exacte)");
        check_vec(one_in("vrsqrtefp v1, v3", FV(4, 1, 16, 0.25f)), FV(0.5f, 1, 0.25f, 2), "vrsqrtefp (valeur exacte)");
        check_vec(one_in("vexptefp v1, v3", FV(3, 0, -1, 10)), FV(8, 1, 0.5f, 1024), "vexptefp");
        check_vec(one_in("vlogefp v1, v3", FV(8, 1, 0.5f, 1024)), FV(3, 0, -1, 10), "vlogefp");
        // conversions
        check_vec(one_in("vcfsx v1, v3, 2", {0xFFFFFFFD, 8, 0, 100}), FV(-0.75f, 2, 0, 25), "vcfsx : -3 / 4, 8 / 4...");
        check_vec(one_in("vcfux v1, v3, 0", {0xFFFFFFFF, 1, 0, 16777216}), FV(4294967296.0f, 1, 0, 16777216.0f), "vcfux");
        Cpu t = one("vctsxs v1, v3, 0", [](Cpu& c) { c.vr[3] = FV(1.5f, -2.5f, 3.0e10f, std::bit_cast<float>(0x7FC00000u)); });
        check_vec(t.vr[1], {1, 0xFFFFFFFE, 0x7FFFFFFF, 0}, "vctsxs : tronque, sature, NaN -> 0");
        check(t.vscr == 1, "vctsxs : SAT leve");
        Cpu t2 = one("vctsxs v1, v3, 3", [](Cpu& c) { c.vr[3] = FV(1.5f, -0.25f, 2, 0); });
        check_vec(t2.vr[1], {12, 0xFFFFFFFE, 16, 0}, "vctsxs avec facteur 2^3");
        check(t2.vscr == 0, "vctsxs : pas de saturation, SAT reste a 0");
        Cpu t3 = one("vctuxs v1, v3, 0", [](Cpu& c) { c.vr[3] = FV(1.5f, -2.5f, 5.0e9f, 0.5f); });
        check_vec(t3.vr[1], {1, 0, 0xFFFFFFFF, 0}, "vctuxs : negatif -> 0, trop grand -> max");
        check(t3.vscr == 1, "vctuxs : SAT leve");
        // la saturation d'UN SEUL mot suffit a lever SAT
        check(one("vctsxs v1, v3, 0", [](Cpu& c) { c.vr[3] = FV(1, 2, 3.0e10f, 4); }).vscr == 1, "vctsxs : debordement positif seul -> SAT");
        check(one("vctsxs v1, v3, 0", [](Cpu& c) { c.vr[3] = FV(1, -3.0e10f, 3, 4); }).vscr == 1, "vctsxs : debordement negatif seul -> SAT");
        check(one("vctuxs v1, v3, 0", [](Cpu& c) { c.vr[3] = FV(1, 2, -2.5f, 4); }).vscr == 1, "vctuxs : negatif seul -> SAT");
        check(one("vctuxs v1, v3, 0", [](Cpu& c) { c.vr[3] = FV(1, 2, 5.0e9f, 4); }).vscr == 1, "vctuxs : trop grand seul -> SAT");
        check(one("vctuxs v1, v3, 0", [](Cpu& c) { c.vr[3] = FV(1, 2, -0.5f, 4); }).vscr == 0, "vctuxs : -0.5 tronque a 0, pas de saturation");
        check_vec(one_in("vcfpsxws128 v1, v3, 1", FV(1.5f, -1, 0, 100)), {3, 0xFFFFFFFE, 0, 200}, "vcfpsxws128 = vctsxs");
        check_vec(one_in("vcsxwfp128 v1, v3, 1", {3, 0xFFFFFFFE, 0, 200}), FV(1.5f, -1, 0, 100), "vcsxwfp128 = vcfsx");
    }

    // ---------- comparaisons ----------
    {
        auto cmp = [](const std::string& line, const Vec& a, const Vec& b) {
            return one(line, [&](Cpu& c) { c.vr[2] = a; c.vr[3] = b; });
        };
        const std::uint32_t T = 0xFFFFFFFF;
        const float nan = std::bit_cast<float>(0x7FC00000u);
        check_vec(cmp("vcmpgtfp v1, v2, v3", FV(2, 1, 0, nan), FV(1, 1, 1, 1)).vr[1], {T, 0, 0, 0}, "vcmpgtfp");
        check_vec(cmp("vcmpgefp v1, v2, v3", FV(2, 1, 0, nan), FV(1, 1, 1, 1)).vr[1], {T, T, 0, 0}, "vcmpgefp");
        check_vec(cmp("vcmpeqfp v1, v2, v3", FV(2, 1, 0, nan), FV(1, 1, 1, nan)).vr[1], {0, T, 0, 0}, "vcmpeqfp : un NaN n'est egal a rien");
        check_vec(cmp("vcmpequw v1, v2, v3", {1, 2, 3, 4}, {1, 0, 3, 0}).vr[1], {T, 0, T, 0}, "vcmpequw");
        check((cmp("vcmpgtfp. v1, v2, v3", FV(2, 2, 2, 2), FV(1, 1, 1, 1)).cr & 0xF0) == 0x80, "vcmpgtfp. : tous vrais -> CR6 = LT");
        check((cmp("vcmpgtfp. v1, v2, v3", FV(2, 0, 2, 2), FV(1, 1, 1, 1)).cr & 0xF0) == 0x00, "vcmpgtfp. : mixte -> CR6 = 0");
        check((cmp("vcmpgtfp. v1, v2, v3", FV(0, 0, 0, 0), FV(1, 1, 1, 1)).cr & 0xF0) == 0x20, "vcmpgtfp. : tous faux -> CR6 = EQ");
        check((cmp("vcmpgtfp v1, v2, v3", FV(2, 2, 2, 2), FV(1, 1, 1, 1)).cr & 0xF0) == 0x00, "vcmpgtfp sans point : CR6 intact");
        check((cmp("vcmpequw128. v100, v2, v3", {1, 2, 3, 4}, {1, 2, 3, 4}).cr & 0xF0) == 0x80, "vcmpequw128. : tous egaux");
        check((cmp("vcmpequw. v1, v2, v3", {1, 2, 3, 4}, {1, 2, 3, 4}).cr & 0xFFFFFF0F) == 0, "vcmpequw. : seul CR6 change");
        // vcmpbfp : bit 0 si a > b, bit 1 si a < -b
        check_vec(cmp("vcmpbfp v1, v2, v3", FV(1, 2, -2, 0), FV(1, 1, 1, 1)).vr[1], {0, 0x80000000, 0x40000000, 0}, "vcmpbfp");
        check_vec(cmp("vcmpbfp v1, v2, v3", FV(nan, 0, 0, 0), FV(1, nan, 1, 1)).vr[1], {0xC0000000, 0xC0000000, 0, 0}, "vcmpbfp : un NaN met les deux bits");
        check((cmp("vcmpbfp. v1, v2, v3", FV(1, 0, -1, 0), FV(1, 1, 1, 1)).cr & 0xF0) == 0x20, "vcmpbfp. : tous dans les bornes -> EQ");
        check((cmp("vcmpbfp. v1, v2, v3", FV(1, 5, -1, 0), FV(1, 1, 1, 1)).cr & 0xF0) == 0x00, "vcmpbfp. : un hors borne -> 0");
    }

    // ---------- logique, entiers, permutations ----------
    {
        auto two = [](const std::string& line, const Vec& a, const Vec& b) {
            return one(line, [&](Cpu& c) { c.vr[2] = a; c.vr[3] = b; });
        };
        check_vec(two("vand v1, v2, v3", {0xFF00FF00, 1, 2, 3}, {0x0FF00FF0, 3, 3, 3}).vr[1], {0x0F000F00, 1, 2, 3}, "vand");
        check_vec(two("vandc v1, v2, v3", {0xFF00FF00, 7, 0, 0}, {0x0FF00FF0, 3, 0, 0}).vr[1], {0xF000F000, 4, 0, 0}, "vandc");
        check_vec(two("vor v1, v2, v3", {0xF0, 0, 0, 0}, {0x0F, 0, 0, 0}).vr[1], {0xFF, 0, 0, 0}, "vor");
        check_vec(two("vnor v1, v2, v3", {0xF0, 0, 0, 0}, {0x0F, 0, 0, 0}).vr[1], {0xFFFFFF00, ~0u, ~0u, ~0u}, "vnor");
        check_vec(two("vxor v1, v2, v2", {1, 2, 3, 4}, {0, 0, 0, 0}).vr[1], {0, 0, 0, 0}, "vxor v, v : remise a zero");
        check_vec(two("vor128 v100, v2, v3", {1, 0, 0, 0}, {2, 0, 0, 0}).vr[100], {3, 0, 0, 0}, "vor128 : v100");
        Cpu sel = one("vsel v1, v2, v3, v4", [](Cpu& c) {
            c.vr[2] = {0xAAAAAAAA, 0, 0, 0}; c.vr[3] = {0x55555555, 0, 0, 0}; c.vr[4] = {0xFF00FF00, 0, 0, 0};
        });
        check_vec(sel.vr[1], {0x55AA55AA, 0, 0, 0}, "vsel : les bits 1 de vC prennent vB, les bits 0 prennent vA");
        Cpu sel2 = one("vsel128 v90, v2, v3, v90", [](Cpu& c) {
            c.vr[2] = {0xAAAAAAAA, 0, 0, 0}; c.vr[3] = {0x55555555, 0, 0, 0}; c.vr[90] = {0xFF00FF00, 0, 0, 0};
        });
        check_vec(sel2.vr[90], {0x55AA55AA, 0, 0, 0}, "vsel128 : le masque est l'ancien vD");
        check_vec(two("vadduwm v1, v2, v3", {0xFFFFFFFF, 1, 2, 3}, {1, 1, 1, 1}).vr[1], {0, 2, 3, 4}, "vadduwm : modulo 2^32");
        check_vec(two("vsubuwm v1, v2, v3", {0, 1, 2, 3}, {1, 1, 1, 1}).vr[1], {0xFFFFFFFF, 0, 1, 2}, "vsubuwm");
        check_vec(two("vslw v1, v2, v3", {1, 1, 1, 1}, {4, 31, 32, 33}).vr[1], {16, 0x80000000, 1, 2}, "vslw : decalage modulo 32");
        check_vec(two("vsrw v1, v2, v3", {0x80000000, 0x80000000, 0x80000000, 0x80000000}, {4, 31, 32, 33}).vr[1],
                  {0x08000000, 1, 0x80000000, 0x40000000}, "vsrw");
        check_vec(two("vsraw v1, v2, v3", {0x80000000, 0x80000000, 0x80000000, 0x40000000}, {1, 31, 32, 30}).vr[1],
                  {0xC0000000, 0xFFFFFFFF, 0x80000000, 1}, "vsraw");
        check_vec(two("vrlw v1, v2, v3", {0x80000001, 0x80000001, 1, 1}, {1, 0, 31, 33}).vr[1], {3, 0x80000001, 0x80000000, 2}, "vrlw");
        check_vec(two("vmrghw v1, v2, v3", {1, 2, 3, 4}, {5, 6, 7, 8}).vr[1], {1, 5, 2, 6}, "vmrghw");
        check_vec(two("vmrglw v1, v2, v3", {1, 2, 3, 4}, {5, 6, 7, 8}).vr[1], {3, 7, 4, 8}, "vmrglw");
        check_vec(two("vmrghw128 v1, v2, v3", {1, 2, 3, 4}, {5, 6, 7, 8}).vr[1], {1, 5, 2, 6}, "vmrghw128");
        check_vec(one("vspltw v1, v3, 2", [](Cpu& c) { c.vr[3] = {1, 2, 3, 4}; }).vr[1], {3, 3, 3, 3}, "vspltw");
        check_vec(one("vspltisw v1, -3", [](Cpu&) {}).vr[1], {0xFFFFFFFD, 0xFFFFFFFD, 0xFFFFFFFD, 0xFFFFFFFD}, "vspltisw -3");
        check_vec(one("vspltisw128 v100, v1, 15", [](Cpu&) {}).vr[100], {15, 15, 15, 15}, "vspltisw128");
        Cpu perm = one("vperm v1, v2, v3, v4", [](Cpu& c) {
            c.vr[2] = kBytes0; c.vr[3] = kBytes16;
            c.vr[4] = {0x3F1E1D1C, 0x1B1A1918, 0x17161514, 0x13121110};   // 0x3F : seuls les 5 bits de poids faible comptent
        });
        check_vec(perm.vr[1], {0x1F1E1D1C, 0x1B1A1918, 0x17161514, 0x13121110}, "vperm : octets de vA puis vB");
        Cpu perm2 = one("vperm128 v100, v70, v127, v7", [](Cpu& c) {
            c.vr[70] = kBytes0; c.vr[127] = kBytes16; c.vr[7] = {0x00010203, 0x10111213, 0x0F0E0D0C, 0x1F1E1D1C};
        });
        check_vec(perm2.vr[100], {0x00010203, 0x10111213, 0x0F0E0D0C, 0x1F1E1D1C}, "vperm128 : v70, v127, vC = v7");
        check_vec(two("vsldoi v1, v2, v3, 4", kBytes0, kBytes16).vr[1], {0x04050607, 0x08090A0B, 0x0C0D0E0F, 0x10111213}, "vsldoi 4");
        check_vec(two("vsldoi128 v1, v2, v3, 15", kBytes0, kBytes16).vr[1], {0x0F101112, 0x13141516, 0x1718191A, 0x1B1C1D1E}, "vsldoi128 15");
        check_vec(two("vsldoi v1, v2, v3, 0", kBytes0, kBytes16).vr[1], kBytes0, "vsldoi 0 = vA");
        check_vec(two("vslo v1, v2, v3", kBytes0, {0, 0, 0, 0x20}).vr[1], {0x04050607, 0x08090A0B, 0x0C0D0E0F, 0}, "vslo : 4 octets");
        check_vec(two("vsro v1, v2, v3", kBytes0, {0, 0, 0, 0x20}).vr[1], {0, 0x00010203, 0x04050607, 0x08090A0B}, "vsro : 4 octets");
        check_vec(one("vpermwi128 v100, v127, 0x1B", [](Cpu& c) { c.vr[127] = {1, 2, 3, 4}; }).vr[100], {1, 2, 3, 4}, "vpermwi128 0x1B");
        check_vec(one("vpermwi128 v100, v127, 0xE4", [](Cpu& c) { c.vr[127] = {1, 2, 3, 4}; }).vr[100], {4, 3, 2, 1}, "vpermwi128 0xE4 : ordre inverse");
        check_vec(one("vpermwi128 v100, v127, 0x00", [](Cpu& c) { c.vr[127] = {1, 2, 3, 4}; }).vr[100], {1, 1, 1, 1}, "vpermwi128 0");
        check_vec(one("vrlimi128 v1, v2, 10, 1", [](Cpu& c) { c.vr[1] = {10, 20, 30, 40}; c.vr[2] = {1, 2, 3, 4}; }).vr[1],
                  {2, 20, 4, 40}, "vrlimi128 : masque 1010, rotation d'un mot");
    }

    // ---------- tailles : pack / unpack ----------
    {
        auto two = [](const std::string& line, const Vec& a, const Vec& b) {
            return one(line, [&](Cpu& c) { c.vr[2] = a; c.vr[3] = b; });
        };
        const Vec a = {0x01020304, 0x05060708, 0x090A0B0C, 0x0D0E0F10};
        const Vec b = {0x11121314, 0x15161718, 0x191A1B1C, 0x1D1E1F20};
        check_vec(two("vpkuhum v1, v2, v3", a, b).vr[1], {0x02040608, 0x0A0C0E10, 0x12141618, 0x1A1C1E20}, "vpkuhum");
        check_vec(two("vpkuwum v1, v2, v3", a, b).vr[1], {0x03040708, 0x0B0C0F10, 0x13141718, 0x1B1C1F20}, "vpkuwum");
        Cpu s = two("vpkshus v1, v2, v3", {0x0100FFFF, 0x007F8000, 0, 0}, {0, 0, 0, 0});
        check_vec(s.vr[1], {0xFF007F00, 0x00000000, 0, 0}, "vpkshus : 256 -> 255, -1 -> 0, 127, -32768 -> 0");
        check(s.vscr == 1, "vpkshus : SAT leve");
        check(two("vpkuhum v1, v2, v3", a, b).vscr == 0, "vpkuhum : jamais de saturation");
        Cpu sw = two("vpkswss v1, v2, v3", {0x00012345, 0xFFFF0000, 5, 0xFFFFFFFB}, {0, 0, 0, 0});
        check_vec(sw.vr[1], {0x7FFF8000, 0x0005FFFB, 0, 0}, "vpkswss : sature en 16 bits avec signe");
        Cpu swu = two("vpkswus v1, v2, v3", {0x00012345, 0xFFFF0000, 5, 0x00007FFF}, {0, 0, 0, 0});
        check_vec(swu.vr[1], {0xFFFF0000, 0x00057FFF, 0, 0}, "vpkswus : 0x12345 -> 0xFFFF, negatif -> 0");
        Cpu sh = two("vpkshss v1, v2, v3", {0x00800080, 0xFF80FF7F, 0, 0}, {0, 0, 0, 0});
        check_vec(sh.vr[1], {0x7F7F8080, 0, 0, 0}, "vpkshss : 128 -> 127, -128, -129 -> -128");
        Cpu uh = two("vpkuhus v1, v2, v3", {0x01000042, 0, 0, 0}, {0, 0, 0, 0});
        check_vec(uh.vr[1], {0xFF420000, 0, 0, 0}, "vpkuhus");
        Cpu uw = two("vpkuwus v1, v2, v3", {0x00010000, 0x00001234, 0, 0}, {0, 0, 0, 0});
        check_vec(uw.vr[1], {0xFFFF1234, 0, 0, 0}, "vpkuwus");
        check_vec(one("vupkhsb v1, v3", [](Cpu& c) { c.vr[3] = {0x807F01FF, 0x11223344, 0x55667788, 0x99AABBCC}; }).vr[1],
                  {0xFF80007F, 0x0001FFFF, 0x00110022, 0x00330044}, "vupkhsb : octets 0 a 7 avec signe");
        check_vec(one("vupklsb v1, v3", [](Cpu& c) { c.vr[3] = {0x807F01FF, 0x11223344, 0x55667788, 0x99AABBCC}; }).vr[1],
                  {0x00550066, 0x0077FF88, 0xFF99FFAA, 0xFFBBFFCC}, "vupklsb : octets 8 a 15 avec signe");
        check_vec(one("vupkhsb128 v100, v127", [](Cpu& c) { c.vr[127] = {0x807F0000, 0, 0, 0}; }).vr[100],
                  {0xFF80007F, 0, 0, 0}, "vupkhsb128");
    }

    // ---------- divers ----------
    {
        Status st = Status::Running;
        Cpu c = one("vpkd3d128 v100, v127, 1, 2, 3", [](Cpu&, Memory&) {}, nullptr, &st);
        check(st == Status::InvalidInstruction && c.pc == kStart, "vpkd3d128 : reconnue, pas encore executee");
        Cpu t = one("vaddfp v1, v2, v3", [](Cpu&) {});
        check(t.pc == kStart + 4 && t.tb == 1, "une instruction vectorielle avance pc et la base de temps");
        // les 128 registres existent et sont independants
        Cpu r = one("vor128 v127, v126, v125", [](Cpu& cpu) { cpu.vr[126] = {1, 0, 0, 0}; cpu.vr[125] = {0, 2, 0, 0}; });
        check_vec(r.vr[127], {1, 2, 0, 0}, "v127 = v126 | v125");
        check_vec(r.vr[0], {0, 0, 0, 0}, "les autres registres ne bougent pas");
    }

    if (failures != 0) {
        std::cerr << failures << " test(s) en echec\n";
        return 1;
    }
    std::cout << "tous les tests ppc_vmx passent\n";
    return 0;
}
