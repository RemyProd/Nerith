// Tests des bits Rc (le point : "add.") et OE (le "o" : "addo"), et des instructions mulhd, mulhdu, rldcl, rldcr.
// Les valeurs attendues sont calculees a la main.
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
using nerith::ppc::Kind;
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

constexpr std::uint64_t kStart = 0x100;
constexpr std::uint32_t kLt = 0x80000000u;   // champ cr0 : LT
constexpr std::uint32_t kGt = 0x40000000u;   // GT
constexpr std::uint32_t kEq = 0x20000000u;   // EQ
constexpr std::uint32_t kSo = 0x10000000u;   // SO du champ cr0
constexpr std::uint32_t kXerSo = 0x80000000u;
constexpr std::uint32_t kXerOv = 0x40000000u;
constexpr std::uint32_t kXerCa = 0x20000000u;
constexpr std::uint64_t kMin64 = 0x8000000000000000ull;
constexpr std::uint64_t kMax64 = 0x7FFFFFFFFFFFFFFFull;

using Setup = std::function<void(Cpu&)>;

// Execute UNE instruction placee en kStart, apres avoir prepare le processeur avec "setup".
Cpu one(const std::string& line, const Setup& setup) {
    Memory memory(0x400);
    memory.write32(kStart, nerith::ppc::assemble(line).at(0));
    Cpu cpu;
    cpu.pc = kStart;
    setup(cpu);
    check(nerith::ppc::step(cpu, memory) == Status::Running, line + " : doit s'executer");
    return cpu;
}

// --- Rc = 1 : CR0 recoit le signe du resultat (64 bits) ---

void test_rc_basic() {
    // add. : resultat negatif, positif, nul
    Cpu a = one("add. r3, r4, r5", [](Cpu& c) { c.gpr[4] = 1; c.gpr[5] = static_cast<std::uint64_t>(-5); });
    check_value(a.cr, kLt, "add. : -4 donne LT dans cr0");
    Cpu b = one("add. r3, r4, r5", [](Cpu& c) { c.gpr[4] = 1; c.gpr[5] = 2; });
    check_value(b.cr, kGt, "add. : 3 donne GT");
    Cpu c = one("add. r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 5; c2.gpr[5] = static_cast<std::uint64_t>(-5); });
    check_value(c.cr, kEq, "add. : 0 donne EQ");
    check_value(c.gpr[3], 0, "add. : le resultat est bien ecrit");

    // sans le point, cr ne bouge pas
    Cpu d = one("add r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 1; c2.gpr[5] = 2; c2.cr = 0x0F0F0F0F; });
    check_value(d.cr, 0x0F0F0F0F, "add sans point : cr inchange");

    // seul le champ cr0 change : les 7 autres champs sont gardes
    Cpu e = one("add. r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 1; c2.gpr[5] = 2; c2.cr = 0x0ABCDEF0; });
    check_value(e.cr, kGt | 0x0ABCDEF0, "add. : seul cr0 est remplace (GT), les autres champs restent");

    // le bit SO de XER est recopie dans cr0
    Cpu f = one("add. r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 1; c2.gpr[5] = 2; c2.xer = kXerSo; });
    check_value(f.cr, kGt | kSo, "add. : XER.SO recopie dans cr0");
}

void test_rc_destination() {
    // pour les instructions logiques, le resultat est dans rA (pas rS) : and. r3, r4, r5 -> r3 = r4 & r5
    Cpu a = one("and. r3, r4, r5", [](Cpu& c) { c.gpr[3] = 0xFF; c.gpr[4] = 0xF0; c.gpr[5] = 0x0F; });
    check_value(a.gpr[3], 0, "and. : resultat dans rA");
    check_value(a.cr, kEq, "and. : EQ car rA vaut 0 (et non rS)");

    // 64 bits : un resultat positif sur 32 bits mais dont le bit 63 est a 1 est negatif
    Cpu b = one("or. r3, r4, r5", [](Cpu& c) { c.gpr[4] = kMin64; c.gpr[5] = 0; });
    check_value(b.cr, kLt, "or. : le bit 63 donne LT");

    // slw. : le resultat 32 bits est complete par des 0 : 0x80000000 est POSITIF sur 64 bits
    Cpu c = one("slw. r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 1; c2.gpr[5] = 31; });
    check_value(c.gpr[3], 0x80000000ull, "slw : 1 << 31");
    check_value(c.cr, kGt, "slw. : 0x80000000 est GT (comparaison sur 64 bits)");

    // extsw. : etend le signe : negatif
    Cpu d = one("extsw. r3, r4", [](Cpu& c2) { c2.gpr[4] = 0x80000000ull; });
    check_value(d.cr, kLt, "extsw. : LT");

    // srawi. : met aussi CA ; le CR vient du resultat
    Cpu e = one("srawi. r3, r4, 1", [](Cpu& c2) { c2.gpr[4] = static_cast<std::uint64_t>(-3); });
    check_value(e.gpr[3], static_cast<std::uint64_t>(-2), "srawi : -3 >> 1 = -2");
    check_value(e.xer & kXerCa, kXerCa, "srawi. : CA posee");
    check_value(e.cr, kLt, "srawi. : LT");

    // rlwinm. , rldicl. , cntlzw.
    Cpu f = one("rlwinm. r3, r4, 0, 31, 31", [](Cpu& c2) { c2.gpr[4] = 2; });
    check_value(f.cr, kEq, "rlwinm. : masque a 0 donne EQ");
    Cpu g = one("rldicl. r3, r4, 0, 63", [](Cpu& c2) { c2.gpr[4] = 3; });
    check_value(g.cr, kGt, "rldicl. : 1 donne GT");
    Cpu h = one("cntlzw. r3, r4", [](Cpu& c2) { c2.gpr[4] = 0; });
    check_value(h.gpr[3], 32, "cntlzw 0 = 32");
    check_value(h.cr, kGt, "cntlzw. : 32 donne GT");

    // mullw. et mulhw.
    Cpu i = one("mullw. r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 0x10000; c2.gpr[5] = 0x10000; });
    check_value(i.gpr[3], 0, "mullw : 2^32 tient pas sur 32 bits : 0");
    check_value(i.cr, kEq, "mullw. : EQ");
    Cpu j = one("sub. r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 10; c2.gpr[5] = 3; });   // r3 = r4 - r5 = 7
    check_value(j.gpr[3], 7, "sub : r4 - r5");
    check_value(j.cr, kGt, "sub. : 7 donne GT");

    // rD = rA : le resultat remplace l'operande, le test porte sur le NOUVEAU contenu
    Cpu k = one("add. r3, r3, r4", [](Cpu& c2) { c2.gpr[3] = 1; c2.gpr[4] = static_cast<std::uint64_t>(-1); });
    check_value(k.cr, kEq, "add. r3, r3, r4 : le test porte sur le resultat (0)");
}

// --- OE = 1 : XER.OV et XER.SO ---

void test_oe_add_sub() {
    // addo : debordement positif
    Cpu a = one("addo r3, r4, r5", [](Cpu& c) { c.gpr[4] = kMax64; c.gpr[5] = 1; });
    check_value(a.gpr[3], kMin64, "addo : MAX + 1 deborde");
    check_value(a.xer & (kXerOv | kXerSo), kXerOv | kXerSo, "addo : OV et SO poses");

    // pas de debordement : OV s'eteint, SO reste (collant)
    Cpu b = one("addo r3, r4, r5", [](Cpu& c) { c.gpr[4] = 1; c.gpr[5] = 2; c.xer = kXerOv | kXerSo; });
    check_value(b.xer & kXerOv, 0, "addo sans debordement : OV eteint");
    check_value(b.xer & kXerSo, kXerSo, "addo sans debordement : SO reste (collant)");

    // sans le "o" : XER ne bouge pas
    Cpu c = one("add r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = kMax64; c2.gpr[5] = 1; });
    check_value(c.xer, 0, "add sans o : pas d'overflow note");

    // negatif + negatif
    Cpu d = one("addo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = kMin64; c2.gpr[5] = kMin64; });
    check_value(d.xer & kXerOv, kXerOv, "addo : MIN + MIN deborde");
    // positif + negatif ne deborde jamais
    Cpu e = one("addo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = kMax64; c2.gpr[5] = kMin64; });
    check_value(e.xer & kXerOv, 0, "addo : MAX + MIN ne deborde pas");

    // addo. : les deux a la fois ; l'overflow est pose AVANT le calcul de CR0 : SO apparait dans cr0
    Cpu f = one("addo. r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = kMax64; c2.gpr[5] = 1; });
    check_value(f.cr, kLt | kSo, "addo. : resultat negatif (LT) et SO recopie dans cr0");

    // rD == rA : le debordement est evalue sur les valeurs AVANT l'ecriture
    Cpu g = one("addo r3, r3, r4", [](Cpu& c2) { c2.gpr[3] = kMax64; c2.gpr[4] = 1; });
    check_value(g.xer & kXerOv, kXerOv, "addo r3, r3, r4 : overflow vu avec les valeurs d'origine");

    // subfo : rB - rA ; MIN - 1 deborde
    Cpu h = one("subfo r3, r5, r4", [](Cpu& c2) { c2.gpr[4] = kMin64; c2.gpr[5] = 1; });   // r3 = r4 - r5
    check_value(h.gpr[3], kMax64, "subfo : MIN - 1");
    check_value(h.xer & kXerOv, kXerOv, "subfo : deborde");
    Cpu i = one("subo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 5; c2.gpr[5] = 3; });   // alias : r3 = r4 - r5
    check_value(i.gpr[3], 2, "subo : 5 - 3");
    check_value(i.xer & kXerOv, 0, "subo : pas de debordement");
    check_value(nerith::ppc::assemble("subo r3, r4, r5").at(0), nerith::ppc::assemble("subfo r3, r5, r4").at(0),
                "subo et subfo r3, r5, r4 : meme mot");

    // nego : seul MIN deborde
    Cpu j = one("nego r3, r4", [](Cpu& c2) { c2.gpr[4] = kMin64; });
    check_value(j.xer & kXerOv, kXerOv, "nego : MIN deborde");
    check_value(j.gpr[3], kMin64, "nego : -MIN = MIN");
    Cpu k = one("nego r3, r4", [](Cpu& c2) { c2.gpr[4] = 5; });
    check_value(k.xer & kXerOv, 0, "nego 5 : pas de debordement");

    // avec retenue : addeo, addzeo, addmeo
    Cpu l = one("addeo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = kMax64; c2.gpr[5] = 0; c2.xer = kXerCa; });
    check_value(l.xer & kXerOv, kXerOv, "addeo : MAX + 0 + CA deborde");
    Cpu m = one("addzeo r3, r4", [](Cpu& c2) { c2.gpr[4] = kMax64; c2.xer = kXerCa; });
    check_value(m.xer & kXerOv, kXerOv, "addzeo : MAX + CA deborde");
    Cpu n = one("addmeo r3, r4", [](Cpu& c2) { c2.gpr[4] = kMin64; });   // MIN + (-1) + 0
    check_value(n.xer & kXerOv, kXerOv, "addmeo : MIN - 1 deborde");
    Cpu o = one("addco r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = kMax64; c2.gpr[5] = 1; });
    check_value(o.xer & (kXerOv | kXerCa), kXerOv, "addco : OV pose, pas de retenue (MAX + 1 = MIN, sans carry non signe)");
}

void test_oe_multiply_divide() {
    // mullwo : le produit des 32 bits doit tenir sur 32 bits avec signe
    Cpu a = one("mullwo r3, r4, r5", [](Cpu& c) { c.gpr[4] = 0x10000; c.gpr[5] = 0x8000; });   // 2^31 : ne tient pas
    check_value(a.xer & kXerOv, kXerOv, "mullwo : 2^31 deborde");
    Cpu b = one("mullwo r3, r4, r5", [](Cpu& c) { c.gpr[4] = 0x10000; c.gpr[5] = 0x7FFF; });   // 2^31 - 2^16 : tient
    check_value(b.xer & kXerOv, 0, "mullwo : 0x7FFF0000 tient");
    Cpu c = one("mullwo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = static_cast<std::uint64_t>(-0x10000); c2.gpr[5] = 0x8000; });   // -2^31 : tient
    check_value(c.xer & kXerOv, 0, "mullwo : -2^31 tient (MIN32)");

    // mulldo : 64 bits
    Cpu d = one("mulldo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 0x100000000ull; c2.gpr[5] = 0x80000000ull; });   // 2^63
    check_value(d.xer & kXerOv, kXerOv, "mulldo : 2^63 deborde");
    Cpu e = one("mulldo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 0x100000000ull; c2.gpr[5] = 0x7FFFFFFFull; });
    check_value(e.xer & kXerOv, 0, "mulldo : 2^63 - 2^32 tient");
    Cpu f = one("mulldo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = kMin64; c2.gpr[5] = static_cast<std::uint64_t>(-1); });
    check_value(f.xer & kXerOv, kXerOv, "mulldo : MIN * -1 deborde");
    Cpu f2 = one("mulldo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = static_cast<std::uint64_t>(-1); c2.gpr[5] = kMin64; });
    check_value(f2.xer & kXerOv, kXerOv, "mulldo : -1 * MIN deborde (ordre inverse)");
    Cpu g = one("mulldo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 0; c2.gpr[5] = kMin64; });
    check_value(g.xer & kXerOv, 0, "mulldo : 0 * MIN ne deborde pas");

    // divisions : par zero, ou MIN / -1
    Cpu h = one("divwo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 7; c2.gpr[5] = 0; });
    check_value(h.xer & kXerOv, kXerOv, "divwo : division par 0");
    Cpu i = one("divwo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 0x80000000ull; c2.gpr[5] = 0xFFFFFFFFull; });
    check_value(i.xer & kXerOv, kXerOv, "divwo : INT_MIN / -1");
    Cpu j = one("divwo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 7; c2.gpr[5] = 2; });
    check_value(j.gpr[3], 3, "divwo : 7 / 2 = 3");
    check_value(j.xer & kXerOv, 0, "divwo : pas de debordement");
    Cpu k = one("divwuo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 7; c2.gpr[5] = 0; });
    check_value(k.xer & kXerOv, kXerOv, "divwuo : division par 0");
    Cpu l = one("divdo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = kMin64; c2.gpr[5] = static_cast<std::uint64_t>(-1); });
    check_value(l.xer & kXerOv, kXerOv, "divdo : MIN / -1");
    Cpu m = one("divduo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 9; c2.gpr[5] = 0; });
    check_value(m.xer & kXerOv, kXerOv, "divduo : division par 0");
    Cpu n = one("divduo r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 9; c2.gpr[5] = 3; });
    check_value(n.gpr[3], 3, "divduo : 9 / 3");
    check_value(n.xer & kXerOv, 0, "divduo : pas de debordement");

    // le "o" et le point ensemble sur une division par zero : CR0 vient du resultat (0 chez Nerith) + SO
    Cpu o = one("divwo. r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 7; c2.gpr[5] = 0; });
    check_value(o.cr, kEq | kSo, "divwo. : EQ (resultat 0) et SO");
}

// --- mulhd, mulhdu ---

void test_mulh() {
    // mulhdu : 2^64-1 fois 2^64-1 = 2^128 - 2^65 + 1 : les 64 bits de poids fort valent 2^64 - 2
    Cpu a = one("mulhdu r3, r4, r5", [](Cpu& c) { c.gpr[4] = ~0ull; c.gpr[5] = ~0ull; });
    check_value(a.gpr[3], 0xFFFFFFFFFFFFFFFEull, "mulhdu : (2^64-1)^2 >> 64");
    Cpu b = one("mulhdu r3, r4, r5", [](Cpu& c) { c.gpr[4] = 0x100000000ull; c.gpr[5] = 0x100000000ull; });
    check_value(b.gpr[3], 1, "mulhdu : 2^32 * 2^32 = 2^64 -> 1");
    Cpu c = one("mulhdu r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 12345; c2.gpr[5] = 6789; });
    check_value(c.gpr[3], 0, "mulhdu : petit produit -> 0");
    // 0x8000000000000000 * 3 = 0x1_8000000000000000 : poids fort = 1
    Cpu d = one("mulhdu r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = kMin64; c2.gpr[5] = 3; });
    check_value(d.gpr[3], 1, "mulhdu : 2^63 * 3 -> 1");
    // des morceaux de 32 bits qui retiennent : 0xFFFFFFFF * 0xFFFFFFFF * ... pour tester la retenue interne
    Cpu e = one("mulhdu r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 0xFFFFFFFF00000001ull; c2.gpr[5] = 0xFFFFFFFF00000001ull; });
    // (calcule avec des entiers de taille illimitee : (0xFFFFFFFF00000001)^2 >> 64 = 0xFFFFFFFE00000002)
    check_value(e.gpr[3], 0xFFFFFFFE00000002ull, "mulhdu : retenues internes");

    // mulhd : avec signe
    Cpu f = one("mulhd r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = static_cast<std::uint64_t>(-1); c2.gpr[5] = static_cast<std::uint64_t>(-1); });
    check_value(f.gpr[3], 0, "mulhd : (-1) * (-1) = 1 -> poids fort 0");
    Cpu g = one("mulhd r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = static_cast<std::uint64_t>(-1); c2.gpr[5] = 1; });
    check_value(g.gpr[3], ~0ull, "mulhd : -1 * 1 = -1 -> poids fort tout a 1");
    Cpu h = one("mulhd r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = kMin64; c2.gpr[5] = kMin64; });
    check_value(h.gpr[3], 0x4000000000000000ull, "mulhd : MIN * MIN = 2^126 -> 2^62");
    Cpu i = one("mulhd r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = kMin64; c2.gpr[5] = 2; });
    check_value(i.gpr[3], ~0ull, "mulhd : MIN * 2 = -2^64 -> -1");
    Cpu j = one("mulhd. r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = static_cast<std::uint64_t>(-1); c2.gpr[5] = 1; });
    check_value(j.cr, kLt, "mulhd. : resultat negatif");

    // mulhd n'a pas de forme OE
    check(nerith::ppc::decode(0x7C642C92u).kind == Kind::Unknown, "mulhdo : n'existe pas");
}

// --- rldcl, rldcr ---

void test_rldc() {
    // rldcl rA, rS, rB, MB : rotation par rB (6 bits), masque MB..63
    Cpu a = one("rldcl r3, r4, r5, 0", [](Cpu& c) { c.gpr[4] = 0x8000000000000001ull; c.gpr[5] = 4; });
    check_value(a.gpr[3], 0x0000000000000018ull, "rldcl : rotation de 4");
    Cpu b = one("rldcl r3, r4, r5, 60", [](Cpu& c) { c.gpr[4] = 0x8000000000000001ull; c.gpr[5] = 4; });
    check_value(b.gpr[3], 0x8ull, "rldcl : masque MB = 60 garde les 4 bits de poids faible");
    // seuls les 6 bits de poids faible de rB comptent
    Cpu c = one("rldcl r3, r4, r5, 0", [](Cpu& c2) { c2.gpr[4] = 1; c2.gpr[5] = 0x44; });
    check_value(c.gpr[3], 0x10ull, "rldcl : rB = 0x44 compte pour 4");
    // le compte de rotation a 6 bits : 36 (0x24) n'est pas 4 ; avec 5 bits on obtiendrait 4
    Cpu c36 = one("rldcl r3, r4, r5, 0", [](Cpu& c2) { c2.gpr[4] = 1; c2.gpr[5] = 36; });
    check_value(c36.gpr[3], 1ull << 36, "rldcl : rotation de 36 (6 bits)");
    Cpu c63 = one("rldcr r3, r4, r5, 63", [](Cpu& c2) { c2.gpr[4] = 1; c2.gpr[5] = 40; });
    check_value(c63.gpr[3], 1ull << 40, "rldcr : rotation de 40 (6 bits)");
    Cpu d = one("rotld r3, r4, r5", [](Cpu& c2) { c2.gpr[4] = 0x0123456789ABCDEFull; c2.gpr[5] = 8; });
    check_value(d.gpr[3], 0x23456789ABCDEF01ull, "rotld : rotation de 8");
    Cpu e = one("rldcl. r3, r4, r5, 63", [](Cpu& c2) { c2.gpr[4] = 2; c2.gpr[5] = 0; });
    check_value(e.gpr[3], 0, "rldcl : masque sur le dernier bit");
    check_value(e.cr, kEq, "rldcl. : EQ");

    // rldcr rA, rS, rB, ME : masque 0..ME
    Cpu f = one("rldcr r3, r4, r5, 3", [](Cpu& c2) { c2.gpr[4] = 0xFFFFFFFFFFFFFFFFull; c2.gpr[5] = 0; });
    check_value(f.gpr[3], 0xF000000000000000ull, "rldcr : masque 0..3");
    Cpu g = one("rldcr r3, r4, r5, 63", [](Cpu& c2) { c2.gpr[4] = 0x1; c2.gpr[5] = 63; });
    check_value(g.gpr[3], 0x8000000000000000ull, "rldcr : rotation de 63 puis masque complet");
    Cpu h = one("rldcr. r3, r4, r5, 63", [](Cpu& c2) { c2.gpr[4] = 0x1; c2.gpr[5] = 63; });
    check_value(h.cr, kLt, "rldcr. : LT");

    // les mots (verifies avec llvm-mc)
    check_value(nerith::ppc::assemble("rldcl r3, r4, r5, 6").at(0), 0x78832990u, "mot rldcl r3, r4, r5, 6");
    check_value(nerith::ppc::assemble("rldcr r3, r4, r5, 6").at(0), 0x78832992u, "mot rldcr r3, r4, r5, 6");
    check_value(nerith::ppc::assemble("rotld r3, r4, r5").at(0), 0x78832810u, "mot rotld r3, r4, r5 (rldcl avec MB = 0)");
    check_value(nerith::ppc::assemble("rldcl r3, r4, r5, 63").at(0), 0x78832FF0u, "mot rldcl avec MB = 63 (6 bits retournes)");
    check_value(nerith::ppc::assemble("rldcr r3, r4, r5, 63").at(0), 0x78832FF2u, "mot rldcr avec ME = 63");
}

// --- flottants : Rc = 1 copie FPSCR[0..3] dans CR1 ---

void test_float_rc() {
    // fmr. : CR1 recoit les 4 bits de poids fort de FPSCR
    Cpu a = one("fmr. f1, f2", [](Cpu& c) { c.fpscr = 0xA0000000u; c.cr = 0; });
    check_value(a.cr, 0x0A000000u, "fmr. : CR1 = FX | VX (bits de poids fort de FPSCR)");
    Cpu b = one("fmr f1, f2", [](Cpu& c) { c.fpscr = 0xA0000000u; c.cr = 0x01234567u; });
    check_value(b.cr, 0x01234567u, "fmr sans point : CR inchange");
    Cpu c = one("fadd. f1, f2, f3", [](Cpu& c2) { c2.fpscr = 0; c2.cr = 0xFFFFFFFFu; });
    check_value(c.cr, 0xF0FFFFFFu, "fadd. : seul le champ CR1 est remplace (FPSCR = 0)");
    // le CR0 et les autres champs ne sont pas touches par un flottant
    Cpu d = one("fneg. f1, f2", [](Cpu& c2) { c2.fpscr = 0; c2.cr = 0x80000000u; });
    check_value(d.cr, 0x80000000u, "fneg. : cr0 intact, CR1 = 0");
    // mtfsb1. 0 : allume FX (bit 0 de FPSCR) puis CR1 le recopie
    Cpu e = one("mtfsb1. 0", [](Cpu& c2) { c2.fpscr = 0; c2.cr = 0; });
    check_value(e.fpscr & 0x80000000u, 0x80000000u, "mtfsb1 0 : FX allume");
    check_value(e.cr, 0x08000000u, "mtfsb1. : CR1 recoit FX");
}

// --- suffixes de l'assembleur et du texte ---

void test_text_and_assembler() {
    const auto t = [](std::uint32_t word) { return nerith::ppc::to_text(nerith::ppc::decode(word)); };
    check(t(nerith::ppc::assemble("addo. r3, r4, r5").at(0)) == "addo. r3, r4, r5", "texte addo.");
    check(t(nerith::ppc::assemble("nego r3, r4").at(0)) == "nego r3, r4", "texte nego");
    check(t(nerith::ppc::assemble("mr. r3, r4").at(0)) == "mr. r3, r4", "texte mr.");
    check(t(nerith::ppc::assemble("not. r3, r4").at(0)) == "not. r3, r4", "texte not.");
    check(t(nerith::ppc::assemble("subfo r3, r5, r4").at(0)) == "subfo r3, r5, r4", "texte subfo (ordre du mot, comme llvm)");
    check(t(nerith::ppc::assemble("subfco. r3, r5, r4").at(0)) == "subfco. r3, r5, r4", "texte subfco.");
    check(t(nerith::ppc::assemble("sub. r3, r4, r5").at(0)) == "sub. r3, r4, r5", "texte sub. (alias)");
    check(t(nerith::ppc::assemble("slwi. r3, r4, 8").at(0)) == "rlwinm. r3, r4, 8, 0, 23",
          "texte slwi. : pas d'alias avec point (comme llvm)");
    check(t(nerith::ppc::assemble("rotld r3, r4, r5").at(0)) == "rotld r3, r4, r5", "texte rotld");
    check(t(nerith::ppc::assemble("rldcl r3, r4, r5, 7").at(0)) == "rldcl r3, r4, r5, 7", "texte rldcl");
    check(t(nerith::ppc::assemble("fadd. f1, f2, f3").at(0)) == "fadd. f1, f2, f3", "texte fadd.");

    // les formes qui n'existent pas sont refusees a l'assemblage, avec un message
    for (const char* bad : {"ori. r3, r4, 5", "lwz. r3, 0(r4)", "mulhwo r3, r4, r5", "mulhdo r3, r4, r5", "cmpw. r3, r4",
                            "fcmpu. cr0, f1, f2", "ando r3, r4, r5", "b. 8", "foo. r1"}) {
        bool refused = false;
        try {
            nerith::ppc::assemble(bad);
        } catch (const nerith::ppc::AssemblyError&) {
            refused = true;
        }
        check(refused, std::string("refuse : ") + bad);
    }
}

}  // namespace

int main() {
    test_rc_basic();
    test_rc_destination();
    test_oe_add_sub();
    test_oe_multiply_divide();
    test_mulh();
    test_rldc();
    test_float_rc();
    test_text_and_assembler();
    if (failures != 0) {
        std::cerr << failures << " echec(s)\n";
        return 1;
    }
    std::cout << "test_ppc_cpu_rc_oe : tout est bon\n";
    return 0;
}
