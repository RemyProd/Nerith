#include <cstdint>
#include <iostream>
#include <vector>

#include "nerith/memory/memory.h"
#include "nerith/ppc/cpu.h"

namespace {

int failures = 0;

void check(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "ECHEC : " << name << "\n";
        ++failures;
    }
}

// Ecrit des mots de 32 bits a partir de l'adresse 0.
void load_words(nerith::Memory& memory, const std::vector<std::uint32_t>& words) {
    std::size_t address = 0;
    for (const std::uint32_t word : words) {
        memory.write32(address, word);
        address += 4;
    }
}

}  // namespace

int main() {
    using namespace nerith;
    using namespace nerith::ppc;

    {   // addition, ecriture puis lecture en memoire, fin du programme
        Memory memory(256);
        load_words(memory, {
            0x38600005,   // li r3, 5
            0x38800007,   // li r4, 7
            0x7CA32214,   // add r5, r3, r4
            0x90A00040,   // stw r5, 64(0)
            0x80C00040,   // lwz r6, 64(0)
            0x4E800020}); // blr   (lr vaut 0 : fin du programme)
        Cpu cpu;
        check(run(cpu, memory, 100) == Status::Halted, "programme : se termine par Halted");
        check(cpu.gpr[3] == 5 && cpu.gpr[4] == 7, "programme : li");
        check(cpu.gpr[5] == 12, "programme : add");
        check(memory.read32(64) == 12, "programme : stw ecrit en memoire");
        check(cpu.gpr[6] == 12, "programme : lwz relit la valeur");
        check(cpu.pc == 20, "programme : pc reste sur le blr");
    }
    {   // addi avec un nombre negatif, et rA = 0 qui vaut le nombre 0
        Memory memory(16);
        load_words(memory, {
            0x38600064,   // li r3, 100
            0x3863FFF0}); // addi r3, r3, -16
        Cpu cpu;
        step(cpu, memory);
        step(cpu, memory);
        check(cpu.gpr[3] == 84, "addi : valeur negative");
        Cpu other;
        other.gpr[0] = 999;
        Memory m2(8);
        m2.write32(0, 0x38600005);   // li r3, 5 : r0 ne doit pas etre utilise
        step(other, m2);
        check(other.gpr[3] == 5, "addi : rA = 0 est le nombre 0, pas r0");
    }
    {   // ori : rA = rS | constante
        Memory memory(8);
        memory.write32(0, 0x60830005);   // ori r3, r4, 5
        Cpu cpu;
        cpu.gpr[4] = 0x10;
        step(cpu, memory);
        check(cpu.gpr[3] == 0x15, "ori");
    }
    {   // nop : seul pc avance
        Memory memory(8);
        memory.write32(0, 0x60000000);
        Cpu cpu;
        check(step(cpu, memory) == Status::Running && cpu.pc == 4, "nop");
    }
    {   // bl et blr : appel de fonction puis retour
        Memory memory(64);
        memory.write32(0, 0x48000011);    // bl +16
        memory.write32(4, 0x38800001);    // li r4, 1
        memory.write32(16, 0x38600009);   // li r3, 9   (la fonction)
        memory.write32(20, 0x4E800020);   // blr
        Cpu cpu;
        step(cpu, memory);
        check(cpu.pc == 16 && cpu.lr == 4, "bl : saute et sauvegarde le retour dans lr");
        step(cpu, memory);
        step(cpu, memory);
        check(cpu.pc == 4 && cpu.gpr[3] == 9, "blr : revient a lr");
        step(cpu, memory);
        check(cpu.gpr[4] == 1, "apres le retour, le programme continue");
    }
    {   // saut vers l'arriere (b -4 sur lui-meme + 4 = boucle infinie) : StepLimit
        Memory memory(8);
        memory.write32(0, 0x48000000);   // b +0 : saute sur lui-meme
        Cpu cpu;
        check(run(cpu, memory, 50) == Status::StepLimit, "boucle infinie : StepLimit");
    }
    {   // boucle : r3 = 3, r4 = 0 ; tant que r3 != 0 : r4 += r3, r3 -= 1  (somme 3+2+1 = 6)
        Memory memory(64);
        // adresse : instruction
        load_words(memory, {
            0x38600003,   // 00 li r3, 3
            0x38800000,   // 04 li r4, 0
            0x7C841A14,   // 08 add r4, r4, r3
            0x3863FFFF,   // 0C addi r3, r3, -1
            0x2C030000,   // 10 cmpwi r3, 0
            0x4082FFF4,   // 14 bne -12  (retour a l'adresse 08)
            0x4E800020}); // 18 blr
        Cpu cpu;
        check(run(cpu, memory, 100) == Status::Halted, "boucle : se termine");
        check(cpu.gpr[4] == 6, "boucle : 3 + 2 + 1 = 6");
        check(cpu.gpr[3] == 0, "boucle : r3 arrive a 0");
        check(cpu.cr == 0x20000000, "cmpwi : CR0 = egal");
    }
    {   // cmpwi : plus petit, plus grand, champ 1, comparaison avec signe
        Memory memory(16);
        memory.write32(0, 0x2C030005);   // cmpwi r3, 5
        Cpu cpu;
        cpu.gpr[3] = 2;
        step(cpu, memory);
        check(cpu.cr == 0x80000000, "cmpwi : plus petit (LT)");
        cpu.pc = 0;
        cpu.gpr[3] = 9;
        step(cpu, memory);
        check(cpu.cr == 0x40000000, "cmpwi : plus grand (GT)");
        cpu.pc = 0;
        cpu.gpr[3] = 0xFFFFFFFF;   // -1 en 32 bits avec signe
        step(cpu, memory);
        check(cpu.cr == 0x80000000, "cmpwi : -1 est plus petit que 5 (avec signe)");
        memory.write32(4, 0x2C830005);   // cmpwi cr1, r3, 5
        step(cpu, memory);
        check(cpu.cr == 0x88000000, "cmpwi cr1 : n'ecrase pas le champ 0");
    }
    {   // sub, mullw, mr, or
        Memory memory(32);
        load_words(memory, {
            0x7C652050,   // sub r3, r4, r5   (r3 = r4 - r5)
            0x7CC429D6,   // mullw r6, r4, r5
            0x7C832378,   // mr r3, r4 (ecrase r3)
            0x7CC72B78}); // or r7, r6, r5
        Cpu cpu;
        cpu.gpr[4] = 10;
        cpu.gpr[5] = 3;
        step(cpu, memory);
        check(cpu.gpr[3] == 7, "sub : 10 - 3 = 7");
        step(cpu, memory);
        check(cpu.gpr[6] == 30, "mullw : 10 * 3 = 30");
        step(cpu, memory);
        check(cpu.gpr[3] == 10, "mr : copie r4 dans r3");
        step(cpu, memory);
        check(cpu.gpr[7] == (30 | 3), "or");
    }
    {   // mullw avec signe, et sub qui passe sous zero
        Memory memory(16);
        load_words(memory, {0x7C6429D6, 0x7C652050});   // mullw r3, r4, r5 ; sub r3, r4, r5
        Cpu cpu;
        cpu.gpr[4] = 0xFFFFFFFD;   // -3 sur 32 bits
        cpu.gpr[5] = 4;
        step(cpu, memory);
        check(cpu.gpr[3] == 0xFFFFFFFFFFFFFFF4ull, "mullw : -3 * 4 = -12 (signe etendu sur 64 bits)");
        Cpu c2;
        c2.gpr[4] = 1;
        c2.gpr[5] = 2;
        memory.write32(0, 0x7C652050);
        step(c2, memory);
        check(c2.gpr[3] == 0xFFFFFFFFFFFFFFFFull, "sub : 1 - 2 boucle sur 64 bits");
        Cpu c3;
        c3.gpr[4] = 0x10000;
        c3.gpr[5] = 0x10000;
        memory.write32(0, 0x7C6429D6);
        step(c3, memory);
        check(c3.gpr[3] == 0, "mullw : on garde seulement 32 bits (65536 * 65536 donne 0)");
    }
    {   // cmpw : deux registres
        Memory memory(8);
        memory.write32(0, 0x7C032000);   // cmpw r3, r4
        Cpu cpu;
        cpu.gpr[3] = 2;
        cpu.gpr[4] = 5;
        step(cpu, memory);
        check(cpu.cr == 0x80000000, "cmpw : plus petit (LT)");
        cpu.pc = 0;
        cpu.gpr[3] = 5;
        step(cpu, memory);
        check(cpu.cr == 0x20000000, "cmpw : egal (EQ)");
    }
    {   // lis puis ori : construire une adresse de 32 bits
        Memory memory(16);
        load_words(memory, {
            0x3C608200,   // lis r3, 0x8200
            0x60631234}); // ori r3, r3, 0x1234
        Cpu cpu;
        step(cpu, memory);
        // lis met 0x8200 dans les 16 bits au-dessus de ceux de poids faible, avec le signe etendu sur 64 bits
        check(cpu.gpr[3] == 0xFFFFFFFF82000000ull, "lis : valeur sur 64 bits avec signe");
        step(cpu, memory);
        check((cpu.gpr[3] & 0xFFFFFFFFull) == 0x82001234, "lis + ori : 0x82001234 dans les 32 bits de poids faible");
    }
    {   // lbz et stb : un octet a la fois
        Memory memory(256);
        memory.write32(0x20, 0x11223344);
        load_words(memory, {
            0x88600020,   // lbz r3, 0x20(r0)
            0x88800023,   // lbz r4, 0x23(r0)
            0x98A00030}); // stb r5, 0x30(r0)
        Cpu cpu;
        cpu.gpr[5] = 0x1234;
        step(cpu, memory);
        step(cpu, memory);
        check(cpu.gpr[3] == 0x11 && cpu.gpr[4] == 0x44, "lbz : big-endian, premier et dernier octet");
        memory.write32(0x30, 0xAABBCCDD);
        step(cpu, memory);
        check(memory.read32(0x30) == 0x34BBCCDD, "stb : seul le premier octet change (0x34)");
    }
    {   // lbz hors memoire
        Memory memory(8);
        memory.write32(0, 0x88600100);   // lbz r3, 0x100(r0)
        Cpu cpu;
        cpu.gpr[3] = 5;
        check(step(cpu, memory) == Status::OutOfBounds && cpu.gpr[3] == 5, "lbz hors memoire");
    }
    {   // erreurs : rien ne change
        Memory memory(8);
        memory.write32(0, 0x12345678);
        Cpu cpu;
        check(step(cpu, memory) == Status::InvalidInstruction && cpu.pc == 0, "instruction inconnue");

        Memory m2(8);
        m2.write32(0, 0x80600100);   // lwz r3, 256(0) : hors de la memoire
        Cpu c2;
        c2.gpr[3] = 77;
        check(step(c2, m2) == Status::OutOfBounds, "lwz hors memoire : OutOfBounds");
        check(c2.gpr[3] == 77 && c2.pc == 0, "lwz hors memoire : rien ne change");

        Cpu c3;
        c3.pc = 100;   // pc hors de la memoire
        check(step(c3, m2) == Status::OutOfBounds, "pc hors memoire : OutOfBounds");
    }

    if (failures == 0) {
        std::cout << "Tous les tests du CPU PowerPC passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
