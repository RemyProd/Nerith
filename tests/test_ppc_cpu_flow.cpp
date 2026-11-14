// Tests des sauts conditionnels complets (bc, bclr, bcctr), des operations sur CR, de sc, des barrieres
// et des verrous atomiques (lwarx / stwcx.). Les valeurs attendues sont calculees a la main.
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

constexpr std::uint64_t kStart = 0x100;   // adresse de l'instruction testee

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

// Execute un programme complet (termine par blr) depuis l'adresse 0 ; r4 = adresse de travail 0x80.
Cpu run_program(const std::string& source, const Setup& setup, Memory& memory) {
    const auto words = nerith::ppc::assemble(source + "\nblr\n");
    for (std::size_t n = 0; n < words.size(); n++) {
        memory.write32(n * 4, words[n]);
    }
    Cpu cpu;
    setup(cpu);
    check(nerith::ppc::run(cpu, memory, 1000) == Status::Halted, "programme : doit se terminer");
    return cpu;
}

constexpr std::uint32_t kEq = 0x20000000;   // bit EQ du champ cr0
constexpr std::uint32_t kLt = 0x80000000;

}  // namespace

int main() {
    // --- operations sur les bits de CR (bit 0 = poids fort). a = bit 1 = 1, b = bit 2 = 0, destination = bit 3 ---
    {
        const Setup base = [](Cpu& c) { c.cr = 0x40000000; };
        check_value(one("crand 3, 1, 2", base).cr, 0x40000000, "crand : 1 et 0 = 0");
        check_value(one("cror 3, 1, 2", base).cr, 0x50000000, "cror : 1 ou 0 = 1");
        check_value(one("crxor 3, 1, 2", base).cr, 0x50000000, "crxor : 1 xor 0 = 1");
        check_value(one("crnor 3, 1, 2", base).cr, 0x40000000, "crnor");
        check_value(one("crnand 3, 1, 2", base).cr, 0x50000000, "crnand");
        check_value(one("creqv 3, 1, 2", base).cr, 0x40000000, "creqv : 1 == 0 est faux");
        check_value(one("crandc 3, 1, 2", base).cr, 0x50000000, "crandc : a et non b");
        check_value(one("crorc 3, 1, 2", base).cr, 0x50000000, "crorc : a ou non b");
        check_value(one("crand 3, 1, 2", [](Cpu& c) { c.cr = 0x50000000; }).cr, 0x40000000,
                    "crand : le bit de destination est bien efface");
        check_value(one("crset 31", [](Cpu& c) { c.cr = 0; }).cr, 0x00000001, "crset 31 : le dernier bit");
        check_value(one("crclr 0", [](Cpu& c) { c.cr = 0xFFFFFFFF; }).cr, 0x7FFFFFFF, "crclr 0 : le premier bit");
        check_value(one("crnot 4, 0", [](Cpu& c) { c.cr = 0; }).cr, 0x08000000, "crnot : bit 4 = non bit 0");
        check_value(one("crmove 4, 0", [](Cpu& c) { c.cr = 0x80000000; }).cr, 0x88000000, "crmove");

        check_value(one("mcrf 7, 0", [](Cpu& c) { c.cr = 0x12345678; }).cr, 0x12345671, "mcrf cr7, cr0");
        check_value(one("mcrf cr0, cr3", [](Cpu& c) { c.cr = 0x12345678; }).cr, 0x42345678, "mcrf cr0, cr3");
        check_value(one("mcrf cr1, cr2", [](Cpu& c) { c.cr = 0x12345678; }).cr, 0x13345678, "mcrf cr1, cr2");

        check_value(one("mfcr r3", [](Cpu& c) { c.cr = 0xDEADBEEF; c.gpr[3] = ~0ull; }).gpr[3], 0xDEADBEEF,
                    "mfcr : 32 bits, le reste vaut 0");
        check_value(one("mtcrf 255, r3", [](Cpu& c) { c.gpr[3] = 0xAAAA00001234ABCDull; }).cr, 0x1234ABCD,
                    "mtcrf 255 : copie les 32 bits de poids faible");
        check_value(one("mtcr r3", [](Cpu& c) { c.gpr[3] = 0x0F0F0F0F; }).cr, 0x0F0F0F0F, "mtcr = mtcrf 255");
        check_value(one("mtcrf 0x81, r3", [](Cpu& c) { c.cr = 0x55555555; c.gpr[3] = 0xAAAAAAAA; }).cr, 0xA555555A,
                    "mtcrf 0x81 : seuls les champs 0 et 7 changent");
        check_value(one("mtcrf 0, r3", [](Cpu& c) { c.cr = 0x55555555; c.gpr[3] = 0xAAAAAAAA; }).cr, 0x55555555,
                    "mtcrf 0 : rien ne change");
        check_value(one("mfocrf r3, 0x40", [](Cpu& c) { c.cr = 0x12345678; c.gpr[3] = ~0ull; }).gpr[3], 0x02000000,
                    "mfocrf : seul le champ 1 est lu");
        check_value(one("mtocrf 2, r3", [](Cpu& c) { c.gpr[3] = 0xFFFFFFFF; }).cr, 0x000000F0,
                    "mtocrf 2 : le champ 6 (bits 4 a 7 en partant de la droite)");
        // un champ ecrit par mtcrf sert ensuite a un saut
        const Cpu c = one("beq +8", [](Cpu& cpu) { cpu.cr = 0x20000000; });
        check_value(c.pc, kStart + 8, "beq apres cr0 = EQ : saute");
    }

    // --- sauts conditionnels bc ---
    {
        check_value(one("bc 12, 2, +8", [](Cpu& c) { c.cr = kEq; }).pc, kStart + 8, "bc 12, 2 avec EQ : saute");
        check_value(one("bc 12, 2, +8", [](Cpu& c) { c.cr = 0; }).pc, kStart + 4, "bc 12, 2 sans EQ : ne saute pas");
        check_value(one("bne +8", [](Cpu& c) { c.cr = 0; }).pc, kStart + 8, "bne : saute si EQ vaut 0");
        check_value(one("bgt cr1, +8", [](Cpu& c) { c.cr = 0x04000000; }).pc, kStart + 8, "bgt cr1 : bit 5");
        check_value(one("bgt cr1, +8", [](Cpu& c) { c.cr = 0x40000000; }).pc, kStart + 4, "bgt cr1 ignore le bit 1");
        check_value(one("bso +8", [](Cpu& c) { c.cr = 0x10000000; }).pc, kStart + 8, "bso : bit 3");
        check_value(one("bc 20, 0, -8", [](Cpu& c) { c.cr = 0; }).pc, kStart - 8, "bc 20 : toujours, deplacement negatif");
        check_value(one("bc 20, 0, -8", [](Cpu& c) { c.ctr = 0; }).ctr, 0, "bc 20 : CTR n'est pas touche");
        check_value(one("beq +8", [](Cpu& c) { c.ctr = 5; c.cr = kEq; }).ctr, 5, "beq : CTR inchange");

        Cpu c = one("bdnz +8", [](Cpu& cpu) { cpu.ctr = 2; });
        check(c.pc == kStart + 8 && c.ctr == 1, "bdnz CTR=2 : saute, CTR=1");
        c = one("bdnz +8", [](Cpu& cpu) { cpu.ctr = 1; });
        check(c.pc == kStart + 4 && c.ctr == 0, "bdnz CTR=1 : tombe a 0, ne saute pas");
        c = one("bdz +8", [](Cpu& cpu) { cpu.ctr = 1; });
        check(c.pc == kStart + 8 && c.ctr == 0, "bdz CTR=1 : saute");
        c = one("bdnzt 2, +8", [](Cpu& cpu) { cpu.ctr = 3; cpu.cr = kEq; });
        check(c.pc == kStart + 8 && c.ctr == 2, "bdnzt : CTR != 0 et EQ");
        c = one("bdnzt 2, +8", [](Cpu& cpu) { cpu.ctr = 3; cpu.cr = 0; });
        check(c.pc == kStart + 4 && c.ctr == 2, "bdnzt sans EQ : ne saute pas mais CTR est decremente");
        c = one("bdnzf 2, +8", [](Cpu& cpu) { cpu.ctr = 3; cpu.cr = 0; });
        check(c.pc == kStart + 8 && c.ctr == 2, "bdnzf : CTR != 0 et pas EQ");
        c = one("bdzt 2, +8", [](Cpu& cpu) { cpu.ctr = 1; cpu.cr = kEq; });
        check(c.pc == kStart + 8 && c.ctr == 0, "bdzt : CTR == 0 et EQ");
        c = one("bdzf 2, +8", [](Cpu& cpu) { cpu.ctr = 1; cpu.cr = kEq; });
        check(c.pc == kStart + 4 && c.ctr == 0, "bdzf avec EQ : ne saute pas");
        c = one("bdnz +8", [](Cpu& cpu) { cpu.ctr = 0; });
        check(c.pc == kStart + 8 && c.ctr == 0xFFFFFFFFFFFFFFFFull, "bdnz CTR=0 : CTR boucle sur 64 bits, CTR != 0");

        // LR (LK = 1) : mis a jour meme quand le saut n'est pas pris
        c = one("beql +8", [](Cpu& cpu) { cpu.cr = 0; });
        check(c.pc == kStart + 4 && c.lr == kStart + 4, "beql sans saut : LR change quand meme");
        c = one("beql +8", [](Cpu& cpu) { cpu.cr = kEq; });
        check(c.pc == kStart + 8 && c.lr == kStart + 4, "beql avec saut");
        c = one("beq +8", [](Cpu& cpu) { cpu.cr = kEq; cpu.lr = 77; });
        check(c.lr == 77, "beq sans LK : LR inchange");

        // adresse absolue (AA = 1)
        c = one("bca 12, 2, 0x20", [](Cpu& cpu) { cpu.cr = kEq; });
        check(c.pc == 0x20, "bca : adresse absolue");
        c = one("beqla 0x20", [](Cpu& cpu) { cpu.cr = kEq; });
        check(c.pc == 0x20 && c.lr == kStart + 4, "beqla : absolu et lien");
        c = one("bca 20, 0, -8", [](Cpu&) {});
        check_value(c.pc, 0xFFFFFFF8ull, "bca avec adresse negative : 32 bits");
    }

    // --- sauts vers LR et CTR ---
    {
        Cpu c = one("beqlr", [](Cpu& cpu) { cpu.cr = kEq; cpu.lr = 0x200; });
        check(c.pc == 0x200 && c.lr == 0x200, "beqlr : saute a LR, LR inchange");
        c = one("beqlr", [](Cpu& cpu) { cpu.cr = 0; cpu.lr = 0x200; });
        check(c.pc == kStart + 4, "beqlr sans EQ : suite");
        c = one("beqlrl", [](Cpu& cpu) { cpu.cr = kEq; cpu.lr = 0x200; });
        check(c.pc == 0x200 && c.lr == kStart + 4, "beqlrl : on saute a l'ANCIEN LR, puis LR = retour");
        c = one("bnelr cr1", [](Cpu& cpu) { cpu.cr = 0x02000000; cpu.lr = 0x200; });
        check(c.pc == kStart + 4, "bnelr cr1 avec EQ de cr1 : ne saute pas");
        c = one("bdnzlr", [](Cpu& cpu) { cpu.ctr = 5; cpu.lr = 0x200; });
        check(c.pc == 0x200 && c.ctr == 4, "bdnzlr : decremente CTR");
        c = one("bclr 20, 3", [](Cpu& cpu) { cpu.lr = 0x200; });
        check(c.pc == 0x200, "bclr 20, 3 : toujours");
        c = one("blr", [](Cpu& cpu) { cpu.lr = 0x203; });
        check_value(c.pc, 0x203, "blr : l'adresse de LR est prise telle quelle (comportement historique)");
        c = one("beqlr", [](Cpu& cpu) { cpu.cr = kEq; cpu.lr = 0x203; });
        check_value(c.pc, 0x200, "beqlr : les 2 bits de poids faible de LR sont ignores");

        Memory memory(0x400);
        Status status = Status::Running;
        c = one("bdnzlr", [](Cpu& cpu) { cpu.ctr = 5; cpu.lr = 0; }, memory, &status);
        check(status == Status::Halted && c.ctr == 5 && c.pc == kStart, "retour pris vers 0 : arret, rien ne change");
        c = one("beqlr", [](Cpu& cpu) { cpu.cr = 0; cpu.lr = 0; }, memory, &status);
        check(status == Status::Running && c.pc == kStart + 4, "retour vers 0 non pris : pas d'arret");
        c = one("beqlrl", [](Cpu& cpu) { cpu.cr = kEq; cpu.lr = 0; }, memory, &status);
        check(status == Status::Running && c.pc == 0 && c.lr == kStart + 4, "beqlrl vers 0 : c'est un appel, pas un arret");

        c = one("beqctr", [](Cpu& cpu) { cpu.cr = kEq; cpu.ctr = 0x300; cpu.lr = 9; });
        check(c.pc == 0x300 && c.lr == 9 && c.ctr == 0x300, "beqctr");
        c = one("beqctrl", [](Cpu& cpu) { cpu.cr = kEq; cpu.ctr = 0x303; });
        check_value(c.pc, 0x300, "beqctrl : bits 0-1 de CTR ignores");
        check_value(c.lr, kStart + 4, "beqctrl : LR");
        c = one("bnectr", [](Cpu& cpu) { cpu.cr = kEq; cpu.ctr = 0x300; });
        check(c.pc == kStart + 4, "bnectr avec EQ : suite");
        c = one("bcctr 20, 3", [](Cpu& cpu) { cpu.ctr = 0x300; });
        check(c.pc == 0x300, "bcctr 20, 3 : toujours");
    }

    // --- formes refusees ---
    {
        bool refused = false;
        try {
            nerith::ppc::assemble("bdnzctr\n");
        } catch (const nerith::ppc::AssemblyError&) {
            refused = true;
        }
        check(refused, "bdnzctr : bcctr ne peut pas decrementer CTR");
        refused = false;
        try {
            nerith::ppc::assemble("bca 12, 2, 6\n");
        } catch (const nerith::ppc::AssemblyError&) {
            refused = true;
        }
        check(refused, "bca avec une adresse qui n'est pas un multiple de 4");
        check(nerith::ppc::decode(0x4D020420u).kind == Kind::Unknown, "bcctr avec BO sans le bit 4 : invalide");
        check(nerith::ppc::decode(0x4D828020u).kind == Kind::Unknown, "bclr avec un bit reserve a 1 : invalide");
        check(nerith::ppc::decode(0x4D820820u).kind == Kind::Unknown, "bclr avec BH != 0 : pas gere");
        check(nerith::ppc::decode(0x7C780126u).kind == Kind::Unknown, "mtocrf avec 2 champs : invalide");
    }

    // --- sc et barrieres ---
    {
        Memory memory(0x400);
        Status status = Status::Running;
        Cpu c = one("sc", [](Cpu& cpu) { cpu.gpr[3] = 7; }, memory, &status);
        check(status == Status::SystemCall && c.pc == kStart + 4 && c.gpr[3] == 7, "sc : SystemCall, pc deja sur la suite");
        for (const char* line : {"sync", "lwsync", "ptesync", "eieio", "isync"}) {
            c = one(line, [](Cpu& cpu) { cpu.cr = 0x1234; cpu.gpr[5] = 5; });
            check(c.pc == kStart + 4 && c.cr == 0x1234 && c.gpr[5] == 5, std::string(line) + " : ne change rien");
        }
    }

    // --- verrous atomiques ---
    {
        const Setup addr = [](Cpu& c) { c.gpr[4] = 0x80; c.gpr[5] = 0xCAFEBABE; };
        Memory memory(0x400);
        memory.write32(0x80, 0x11223344);
        Cpu c = one("lwarx r3, 0, r4", addr, memory);
        check(c.gpr[3] == 0x11223344 && c.reserved && c.reserve_address == 0x80, "lwarx : lit et reserve");

        // stwcx. apres lwarx : reussit
        Cpu cpu;
        cpu.gpr[4] = 0x80;
        cpu.gpr[5] = 0xCAFEBABE;
        memory.write32(0, nerith::ppc::assemble("lwarx r3, 0, r4").at(0));
        memory.write32(4, nerith::ppc::assemble("stwcx. r5, 0, r4").at(0));
        memory.write32(8, nerith::ppc::assemble("stwcx. r5, 0, r4").at(0));
        check(nerith::ppc::step(cpu, memory) == Status::Running, "lwarx");
        check(nerith::ppc::step(cpu, memory) == Status::Running, "stwcx. (1)");
        check(memory.read32(0x80) == 0xCAFEBABE && cpu.cr == kEq && !cpu.reserved, "stwcx. reussi : memoire ecrite, cr0 = EQ");
        memory.write32(0x80, 1);
        check(nerith::ppc::step(cpu, memory) == Status::Running, "stwcx. (2)");
        check(memory.read32(0x80) == 1 && (cpu.cr & 0xF0000000) == 0, "stwcx. sans reservation : echec, rien ecrit, EQ = 0");

        // adresse differente de la reservation : echec, et la reservation disparait
        Memory m2(0x400);
        m2.write32(0, nerith::ppc::assemble("lwarx r3, 0, r4").at(0));
        m2.write32(4, nerith::ppc::assemble("stwcx. r5, 0, r6").at(0));
        Cpu d;
        d.gpr[4] = 0x80;
        d.gpr[5] = 9;
        d.gpr[6] = 0x84;
        nerith::ppc::step(d, m2);
        nerith::ppc::step(d, m2);
        check(m2.read32(0x84) == 0 && (d.cr & 0xF0000000) == 0 && !d.reserved, "stwcx. a une autre adresse : echec");

        // le bit SO de XER est recopie dans cr0
        Memory m3(0x400);
        m3.write32(0, nerith::ppc::assemble("stwcx. r5, 0, r4").at(0));
        Cpu e;
        e.gpr[4] = 0x80;
        e.xer = 0x80000000;
        nerith::ppc::step(e, m3);
        check_value(e.cr & 0xF0000000, 0x10000000, "stwcx. echec avec XER.SO : cr0 = SO seulement");
        e.pc = 0;
        e.reserved = true;
        e.reserve_address = 0x80;
        nerith::ppc::step(e, m3);
        check_value(e.cr & 0xF0000000, 0x30000000, "stwcx. reussi avec XER.SO : cr0 = EQ + SO");

        // 64 bits
        Memory m4(0x400);
        m4.write32(0x80, 0x01020304);
        m4.write32(0x84, 0x05060708);
        m4.write32(0, nerith::ppc::assemble("ldarx r3, 0, r4").at(0));
        m4.write32(4, nerith::ppc::assemble("stdcx. r5, 0, r4").at(0));
        Cpu f;
        f.gpr[4] = 0x80;
        f.gpr[5] = 0x1122334455667788ull;
        nerith::ppc::step(f, m4);
        check_value(f.gpr[3], 0x0102030405060708ull, "ldarx");
        nerith::ppc::step(f, m4);
        check(m4.read32(0x80) == 0x11223344 && m4.read32(0x84) == 0x55667788 && f.cr == kEq, "stdcx. reussi");

        // ecriture hors memoire : rien ne change, la reservation reste
        Memory m5(256);
        m5.write32(0, nerith::ppc::assemble("stdcx. r5, 0, r4").at(0));
        Cpu g;
        g.gpr[4] = 252;   // 252 a 259 : depasse
        g.reserved = true;
        g.reserve_address = 252;
        check(nerith::ppc::step(g, m5) == Status::OutOfBounds, "stdcx. hors memoire : OutOfBounds");
        check(g.reserved && g.cr == 0 && g.pc == 0, "stdcx. hors memoire : etat inchange");

        // incrementer atomiquement un compteur : boucle lwarx / stwcx.
        Memory m6(0x400);
        m6.write32(0x80, 41);
        const Cpu h = run_program(
            "        li r4, 0x80\n"
            "again:  lwarx r5, 0, r4\n"
            "        addi r5, r5, 1\n"
            "        stwcx. r5, 0, r4\n"
            "        bne again\n",
            [](Cpu&) {}, m6);
        check(m6.read32(0x80) == 42 && h.gpr[5] == 42, "compteur atomique : 41 + 1");
    }

    if (failures == 0) {
        std::cout << "Tous les tests de sauts, CR et atomiques passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
