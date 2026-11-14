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
