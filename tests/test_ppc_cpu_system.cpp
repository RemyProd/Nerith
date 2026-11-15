// Tests des instructions de cache (dcbz, dcbt...) et des registres systeme (MSR, SRR0/1, SPRG, DEC, PVR,
// base de temps). Les valeurs attendues sont calculees a la main.
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

using Setup = std::function<void(Cpu&)>;

// Execute UNE instruction placee en kStart, apres avoir prepare le processeur avec "setup".
Cpu one(const std::string& line, const Setup& setup, Memory& memory, Status* status = nullptr) {
    memory.write32(kStart, nerith::ppc::assemble(line).at(0));
    Cpu cpu;
    cpu.pc = kStart;
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
    Memory memory(0x400);
    return one(line, setup, memory);
}

// --- dcbz et dcbzl : mise a zero d'un bloc aligne ---

void test_dcbz() {
    Memory memory(0x400);
    for (std::size_t a = 0x180; a < 0x220; a++) {
        memory.write8(a, 0xAA);
    }
    // adresse 0x1C5 : le bloc de 32 octets aligne est 0x1C0 a 0x1DF
    one("dcbz r4, r5", [](Cpu& c) { c.gpr[4] = 0x1C0; c.gpr[5] = 0x5; }, memory);
    check(memory.read8(0x1BF) == 0xAA, "dcbz : l'octet avant le bloc est intact");
    check(memory.read8(0x1C0) == 0 && memory.read8(0x1DF) == 0, "dcbz : le bloc de 32 octets est a zero");
    check(memory.read8(0x1E0) == 0xAA, "dcbz : l'octet apres le bloc est intact");

    Memory memory2(0x400);
    for (std::size_t a = 0x100 + 4; a < 0x300; a++) {
        memory2.write8(a, 0xBB);
    }
    // dcbzl : bloc de 128 octets, 0x200 a 0x27F (adresse 0x233 alignee vers le bas)
    one("dcbzl 0, r5", [](Cpu& c) { c.gpr[5] = 0x233; }, memory2);
    check(memory2.read8(0x1FF) == 0xBB, "dcbzl : l'octet avant le bloc est intact");
    check(memory2.read8(0x200) == 0 && memory2.read8(0x27F) == 0, "dcbzl : le bloc de 128 octets est a zero");
    check(memory2.read8(0x280) == 0xBB, "dcbzl : l'octet apres le bloc est intact");

    // dcbz128 : le nom Xenon, meme mot que dcbzl
    check(nerith::ppc::assemble("dcbz128 0, r5").at(0) == nerith::ppc::assemble("dcbzl 0, r5").at(0),
          "dcbz128 et dcbzl donnent le meme mot");

    // bloc en dehors de la memoire : rien n'est ecrit
    Memory memory3(0x400);
    Status status = Status::Running;
    Cpu after = one("dcbz 0, r5", [](Cpu& c) { c.gpr[5] = 0x405; }, memory3, &status);
    check(status == Status::OutOfBounds, "dcbz hors memoire : OutOfBounds");
    check_value(after.pc, kStart, "dcbz hors memoire : pc inchange");
}

// --- les autres instructions de cache ne font rien ---

void test_cache_hints() {
    Memory memory(0x400);
    memory.write8(0x200, 0x77);
    for (const char* line : {"dcbt r4, r5", "dcbtst r4, r5", "dcbst r4, r5", "dcbf r4, r5", "icbi r4, r5"}) {
        Cpu after = one(line, [](Cpu& c) { c.gpr[4] = 0x100; c.gpr[5] = 0x100; }, memory);
        check_value(after.pc, kStart + 4, std::string(line) + " : pc avance");
        check(memory.read8(0x200) == 0x77, std::string(line) + " : la memoire est intacte");
    }
}

// --- MSR ---

void test_msr() {
    // mfmsr lit la valeur telle quelle
    Cpu a = one("mfmsr r3", [](Cpu& c) { c.msr = 0x9000000000008002ull; });
    check_value(a.gpr[3], 0x9000000000008002ull, "mfmsr : lit le MSR");

    // mtmsrd (L = 0) remplace tout
    Cpu b = one("mtmsrd r3", [](Cpu& c) { c.gpr[3] = 0x8000000000001234ull; c.msr = 0xFFFF; });
    check_value(b.msr, 0x8000000000001234ull, "mtmsrd : remplace les 64 bits");

    // mtmsr (L = 0) ne touche que les 32 bits de poids faible
    Cpu c = one("mtmsr r3", [](Cpu& c2) { c2.gpr[3] = 0x1234; c2.msr = 0x90000000FFFFFFFFull; });
    check_value(c.msr, 0x9000000000001234ull, "mtmsr : garde les 32 bits de poids fort");

    // L = 1 : seulement EE (0x8000) et RI (0x2) changent
    Cpu d = one("mtmsrd r3, 1", [](Cpu& c2) { c2.gpr[3] = 0; c2.msr = 0x900000000000F00Full; });
    check_value(d.msr, 0x900000000000700Dull, "mtmsrd 1 : eteint seulement EE et RI");
    Cpu e = one("mtmsr r3, 1", [](Cpu& c2) { c2.gpr[3] = 0xFFFFFFFFFFFFFFFFull; c2.msr = 0; });
    check_value(e.msr, 0x8002, "mtmsr 1 : allume seulement EE et RI");
}

// --- SPR : SRR0/1, SPRG, DEC, PVR ---

void test_sprs() {
    Cpu a = one("mtsrr0 r3", [](Cpu& c) { c.gpr[3] = 0xDEAD0000CAFEull; });
    check_value(a.srr0, 0xDEAD0000CAFEull, "mtsrr0 : ecrit SRR0");
    Cpu b = one("mfsrr1 r4", [](Cpu& c) { c.srr1 = 0x8002; });
    check_value(b.gpr[4], 0x8002, "mfsrr1 : lit SRR1");

    Cpu c = one("mtspr 274, r5", [](Cpu& c2) { c2.gpr[5] = 42; });
    check_value(c.sprg[2], 42, "mtspr 274 : ecrit SPRG2");
    Cpu d = one("mfspr r6, 275", [](Cpu& c2) { c2.sprg[3] = 0x123456789ull; });
    check_value(d.gpr[6], 0x123456789ull, "mfspr 275 : lit SPRG3");

    // le decompteur descend de 1 a CHAQUE instruction : mtdec pose la valeur, puis le pas la diminue
    Cpu e = one("mtdec r3", [](Cpu& c2) { c2.gpr[3] = 100; });
    check_value(e.dec, 99, "mtdec : pose 100, et l'instruction elle-meme compte un tic");
    Cpu f = one("mfdec r4", [](Cpu& c2) { c2.dec = 50; });
    check_value(f.gpr[4], 50, "mfdec : lit la valeur AVANT le tic de l'instruction");
    check_value(f.dec, 49, "mfdec : le decompteur a ensuite perdu un tic");

    Cpu g = one("mfpvr r3", [](Cpu& c2) { c2.pvr = 0x00710200; });
    check_value(g.gpr[3], 0x00710200, "mfpvr : lit le PVR");

    // ecrire le PVR ou la base de temps est refuse (lecture seule)
    check(nerith::ppc::decode(0x7C7F43A6u).kind == Kind::Unknown, "mtspr 287 (PVR) : refuse");
    check(nerith::ppc::decode(0x7C6C43A6u).kind == Kind::Unknown, "mtspr 268 (base de temps) : refuse");
}

// --- base de temps ---

void test_time_base() {
    // chaque instruction executee ajoute 1 : apres 3 nop, tb = 3
    Memory memory(0x400);
    const auto words = nerith::ppc::assemble("nop\nnop\nnop\nmftb r3\nmftbu r4\nmfspr r5, 268\nblr");
    for (std::size_t n = 0; n < words.size(); n++) {
        memory.write32(kStart + n * 4, words[n]);
    }
    Cpu cpu;
    cpu.pc = kStart;
    cpu.tb = 0xFFFFFFFEull;   // proche du passage au bit 32, pour tester mftbu
    check(nerith::ppc::run(cpu, memory, 100) == Status::Halted, "programme base de temps : se termine");
    check_value(cpu.gpr[3], 0xFFFFFFFEull + 3, "mftb : 3 nop deja executes");
    check_value(cpu.gpr[4], 0x1, "mftbu : la moitie haute apres le passage");
    check_value(cpu.gpr[5], 0xFFFFFFFEull + 5, "mfspr 268 : la meme base de temps");
    check_value(cpu.tb, 0xFFFFFFFEull + 7, "tb : 7 instructions executees en tout (blr compris)");

    // une instruction refusee ne compte pas un tic
    Memory memory2(0x400);
    Status status = Status::Running;
    Cpu after = one("mfmsr r3", [](Cpu&) {}, memory2, &status);
    check_value(after.tb, 1, "tb : une instruction valide compte un tic");
    Memory memory3(0x400);
    memory3.write32(kStart, 0x00000001u);   // mot invalide (opcode 0)
    Cpu bad;
    bad.pc = kStart;
    check(nerith::ppc::step(bad, memory3) == Status::InvalidInstruction, "mot invalide : refuse");
    check_value(bad.tb, 0, "tb : un mot invalide ne compte pas");
}

// --- textes du decodeur ---

void test_text() {
    const auto t = [](std::uint32_t word) { return nerith::ppc::to_text(nerith::ppc::decode(word)); };
    check(t(0x7C042FECu) == "dcbz r4, r5", "texte dcbz");
    check(t(0x7C242FECu) == "dcbzl r4, r5", "texte dcbzl");
    check(t(0x7C002FECu) == "dcbz 0, r5", "texte dcbz avec rA = 0");
    check(t(0x7C610124u) == "mtmsr r3, 1", "texte mtmsr L = 1");
    check(t(0x7C600164u) == "mtmsrd r3", "texte mtmsrd");
    check(t(0x7C7A02A6u) == "mfsrr0 r3", "texte mfsrr0");
    check(t(0x7C7342A6u) == "mfspr r3, 275", "texte mfspr generique");
    check(t(0x7C7143A6u) == "mtspr 273, r3", "texte mtspr : le numero en premier");
    check(t(0x7C6C42E6u) == "mftb r3, 268", "texte mftb");
    check(t(0x7C6D42E6u) == "mftbu r3", "texte mftbu");
    // champs reserves non nuls : refuses
    check(nerith::ppc::decode(0x7C6200A6u).kind == Kind::Unknown, "mfmsr avec rA non nul : refuse");
    check(nerith::ppc::decode(0x7D042A2Cu).kind == Kind::Unknown, "dcbt avec indication TH : refuse");
    check(nerith::ppc::decode(0x7C2428ACu).kind == Kind::Unknown, "dcbf avec L non nul : refuse");
}

}  // namespace

int main() {
    test_dcbz();
    test_cache_hints();
    test_msr();
    test_sprs();
    test_time_base();
    test_text();
    if (failures != 0) {
        std::cerr << failures << " echec(s)\n";
        return 1;
    }
    std::cout << "test_ppc_cpu_system : tout est bon\n";
    return 0;
}
