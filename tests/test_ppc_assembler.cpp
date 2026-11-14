#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "nerith/memory/memory.h"
#include "nerith/ppc/assembler.h"
#include "nerith/ppc/cpu.h"
#include "nerith/ppc/decoder.h"

namespace {

int failures = 0;

void check(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "ECHEC : " << name << "\n";
        ++failures;
    }
}

// Assemble une seule instruction et renvoie son mot.
std::uint32_t one(const std::string& line) {
    return nerith::ppc::assemble(line).at(0);
}

// Vrai si le programme est refuse, a la ligne attendue.
bool fails_at(const std::string& source, std::size_t line) {
    try {
        nerith::ppc::assemble(source);
    } catch (const nerith::ppc::AssemblyError& error) {
        return error.line() == line;
    }
    return false;
}

}  // namespace

int main() {
    using namespace nerith;

    // les mots connus des autres tests
    check(one("li r3, 5") == 0x38600005, "li");
    check(one("addi r1, r1, -16") == 0x3821FFF0, "addi negatif");
    check(one("add r5, r3, r4") == 0x7CA32214, "add");
    check(one("ori r3, r3, 1") == 0x60630001, "ori");
    check(one("ori r3, r4, 5") == 0x60830005, "ori : rA est la destination");
    check(one("nop") == 0x60000000, "nop");
    check(one("lwz r3, 8(r1)") == 0x80610008, "lwz");
    check(one("stw r3, 8(r1)") == 0x90610008, "stw");
    check(one("stw r5, 64(r0)") == 0x90A00040, "stw avec base r0");
    check(one("cmpwi r3, 0") == 0x2C030000, "cmpwi");
    check(one("cmpwi cr1, r3, 255") == 0x2C8300FF, "cmpwi cr1");
    check(one("blr") == 0x4E800020, "blr");
    check(one("b +8") == 0x48000008, "b +8");
    check(one("b -4") == 0x4BFFFFFC, "b -4");
    check(one("bl +8") == 0x48000009, "bl");
    check(one("beq +12") == 0x4182000C, "beq");
    check(one("bne -8") == 0x4082FFF8, "bne");
    check(one("ble cr1, +8") == 0x40850008, "ble cr1");

    // instructions ajoutees ensuite (mots verifies avec llvm-mc)
    check(one("sub r3, r4, r5") == 0x7C652050, "sub");
    check(one("mullw r3, r4, r5") == 0x7C6429D6, "mullw");
    check(one("cmpw r3, r4") == 0x7C032000, "cmpw");
    check(one("cmpw cr1, r4, r5") == 0x7C842800, "cmpw cr1");
    check(one("mr r3, r4") == 0x7C832378, "mr");
    check(one("or r3, r4, r5") == 0x7C832B78, "or");
    check(one("lbz r3, 8(r1)") == 0x88610008, "lbz");
    check(one("stb r3, 8(r1)") == 0x98610008, "stb");
    check(one("lis r3, 0x8200") == 0x3C608200, "lis");
    check(one("lis r3, -1") == 0x3C60FFFF, "lis negatif");
    check(one("addis r3, r4, -1") == 0x3C64FFFF, "addis");
    check(ppc::to_text(ppc::decode(one("sub r3, r4, r5"))) == "sub r3, r4, r5", "aller-retour sub");
    check(ppc::to_text(ppc::decode(one("mr r31, r0"))) == "mr r31, r0", "aller-retour mr");
    check(fails_at("sub r3, r4\n", 1), "sub : operande manquante");

    check(one("mflr r0") == 0x7C0802A6, "mflr");
    check(one("mtlr r12") == 0x7D8803A6, "mtlr");
    check(one("stwu r1, -16(r1)") == 0x9421FFF0, "stwu");
    check(one("stwu r1, -32(r1)") == 0x9421FFE0, "stwu -32");

    // rotation et masque (mots verifies avec llvm-mc)
    check(one("rlwinm r3, r4, 5, 6, 7") == 0x5483298E, "rlwinm");
    check(one("rlwinm r3, r4, 0, 28, 3") == 0x54830706, "rlwinm avec masque qui fait le tour");
    check(one("slwi r3, r4, 8") == 0x5483402E, "slwi");
    check(one("srwi r3, r4, 8") == 0x5483C23E, "srwi");
    check(one("srwi r3, r4, 0") == 0x5483003E, "srwi 0");
    check(one("clrlwi r3, r4, 16") == 0x5483043E, "clrlwi");
    check(one("clrrwi r3, r4, 4") == 0x54830036, "clrrwi");
    check(one("rotlwi r3, r4, 8") == 0x5483403E, "rotlwi");
    check(fails_at("slwi r3, r4, 32\n", 1), "slwi : decalage trop grand");
    check(fails_at("rlwinm r3, r4, 1, 2\n", 1), "rlwinm : operande manquante");

    {   // decoder un mot avec un programme PowerPC (examples/bits_ppc.s)
        const std::vector<std::uint32_t> words = ppc::assemble(
            "lis r3, 0x3860\n"
            "ori r3, r3, 5\n"
            "srwi r4, r3, 26\n"
            "rlwinm r5, r3, 11, 27, 31\n"
            "rlwinm r6, r3, 16, 27, 31\n"
            "clrlwi r7, r3, 16\n"
            "blr\n");
        Memory memory(64);
        for (std::size_t i = 0; i < words.size(); i++) {
            memory.write32(i * 4, words[i]);
        }
        ppc::Cpu cpu;
        check(ppc::run(cpu, memory, 100) == ppc::Status::Halted, "champs : se termine");
        check(cpu.gpr[4] == 14 && cpu.gpr[5] == 3 && cpu.gpr[6] == 0 && cpu.gpr[7] == 5,
              "champs de 0x38600005 : opcode 14, rD 3, rA 0, d 5");
    }

    check(one("mtctr r3") == 0x7C6903A6, "mtctr");
    check(one("mfctr r3") == 0x7C6902A6, "mfctr");
    check(one("mtctr r31") == 0x7FE903A6, "mtctr r31");
    check(one("bdnz -8") == 0x4200FFF8, "bdnz");
    check(one("bdnz +8") == 0x42000008, "bdnz +8");
    check(one("bdz +12") == 0x4240000C, "bdz");
    check(one("bctr") == 0x4E800420, "bctr");
    check(one("bctrl") == 0x4E800421, "bctrl");
    check(one("blrl") == 0x4E800021, "blrl");

    // majuscules, hexadecimal, commentaires
    check(one("LI R3, 0x10 ; commentaire") == 0x38600010, "majuscules, hexa, commentaire");
    check(one("li r3, 5 # autre commentaire") == 0x38600005, "commentaire avec #");

    // etiquettes : l'assembleur calcule les deplacements
    {
        const std::vector<std::uint32_t> words = ppc::assemble(
            "        li r3, 3\n"
            "        li r4, 0\n"
            "loop:   add r4, r4, r3\n"
            "        addi r3, r3, -1\n"
            "        cmpwi r3, 0\n"
            "        bne loop\n"
            "        blr\n");
        check(words.size() == 7, "etiquettes : 7 instructions");
        check(words.at(5) == 0x4082FFF4, "etiquettes : bne loop = bne -12");

        // le programme assemble tourne vraiment
        Memory memory(64);
        for (std::size_t i = 0; i < words.size(); i++) {
            memory.write32(i * 4, words[i]);
        }
        ppc::Cpu cpu;
        check(ppc::run(cpu, memory, 100) == ppc::Status::Halted, "etiquettes : se termine");
        check(cpu.gpr[4] == 6, "etiquettes : 3 + 2 + 1 = 6");
    }
    {   // etiquette vers l'avant, etiquette seule sur sa ligne, bl
        const std::vector<std::uint32_t> words = ppc::assemble(
            "        bl func\n"
            "        blr\n"
            "func:\n"
            "        li r3, 9\n"
            "        blr\n");
        check(words.at(0) == 0x48000009, "bl vers l'avant : +8");
    }

    {   // factorielle recursive : appels, pile, lr
        const std::vector<std::uint32_t> words = ppc::assemble(
            "main:   mflr r12\n"
            "        li r1, 0x800\n"
            "        li r3, 5\n"
            "        bl fact\n"
            "        mtlr r12\n"
            "        blr\n"
            "fact:   cmpwi r3, 1\n"
            "        ble base\n"
            "        mflr r0\n"
            "        stwu r1, -16(r1)\n"
            "        stw r0, 20(r1)\n"
            "        stw r3, 8(r1)\n"
            "        addi r3, r3, -1\n"
            "        bl fact\n"
            "        lwz r4, 8(r1)\n"
            "        mullw r3, r3, r4\n"
            "        lwz r0, 20(r1)\n"
            "        addi r1, r1, 16\n"
            "        mtlr r0\n"
            "        blr\n"
            "base:   li r3, 1\n"
            "        blr\n");
        Memory memory(4096);
        for (std::size_t i = 0; i < words.size(); i++) {
            memory.write32(i * 4, words[i]);
        }
        ppc::Cpu cpu;
        check(ppc::run(cpu, memory, 1000) == ppc::Status::Halted, "recursion : se termine");
        check(cpu.gpr[3] == 120, "recursion : fact(5) = 120");
        check(cpu.gpr[1] == 0x800, "recursion : la pile est revenue a sa place");
    }

    // le decodeur relit ce que l'assembleur a ecrit
    check(ppc::to_text(ppc::decode(one("lwz r3, 8(r1)"))) == "lwz r3, 8(r1)", "aller-retour lwz");
    check(ppc::to_text(ppc::decode(one("bne cr1, -8"))) == "bne cr1, -8", "aller-retour bne cr1");

    // erreurs, avec le bon numero de ligne
    check(fails_at("li r3, 5\nfoo r1\n", 2), "instruction inconnue");
    check(fails_at("li r32, 5\n", 1), "registre r32");
    check(fails_at("li r3, 40000\n", 1), "nombre trop grand");
    check(fails_at("li r3\n", 1), "operande manquante");
    check(fails_at("\n\nb nulle_part\n", 3), "etiquette inconnue");
    check(fails_at("a: nop\na: nop\n", 2), "etiquette en double");
    check(fails_at("lwz r3, 8\n", 1), "lwz sans parentheses");
    check(fails_at("b +3\n", 1), "deplacement pas multiple de 4");
    check(fails_at("ori r3, r4, -1\n", 1), "ori refuse un nombre negatif");

    if (failures == 0) {
        std::cout << "Tous les tests de l'assembleur PowerPC passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
