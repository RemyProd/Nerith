#include <cstdint>
#include <iostream>
#include <string>

#include "nerith/ppc/decoder.h"

namespace {

int failures = 0;

void check(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "ECHEC : " << name << "\n";
        ++failures;
    }
}

std::string text(std::uint32_t word) {
    return nerith::ppc::to_text(nerith::ppc::decode(word));
}

}  // namespace

int main() {
    using namespace nerith::ppc;

    {   // les trois mots de l'exercice de la note "PowerPC et Xenon"
        const Instruction a = decode(0x38600005);
        check(a.kind == Kind::Addi && a.rd == 3 && a.ra == 0 && a.imm == 5, "0x38600005 : addi r3, 0, 5");
        const Instruction b = decode(0x80610008);
        check(b.kind == Kind::Lwz && b.rd == 3 && b.ra == 1 && b.imm == 8, "0x80610008 : lwz r3, 8(r1)");
        const Instruction c = decode(0x60000000);
        check(c.kind == Kind::Ori && c.rd == 0 && c.ra == 0 && c.imm == 0, "0x60000000 : ori 0, 0, 0");
    }

    check(text(0x38600005) == "li r3, 5", "texte : li");
    check(text(0x80610008) == "lwz r3, 8(r1)", "texte : lwz");
    check(text(0x60000000) == "nop", "texte : nop");

    // addi avec un nombre negatif : 0x3821FFF0 = addi r1, r1, -16 (reserve de la place sur la pile)
    check(text(0x3821FFF0) == "addi r1, r1, -16", "addi : valeur negative");
    // stw : 0x90610008 = stw r3, 8(r1)
    check(text(0x90610008) == "stw r3, 8(r1)", "stw");
    // add : 0x7C632214 = add r3, r3, r4
    check(text(0x7C632214) == "add r3, r3, r4", "add");
    // ori : 0x60630001 = ori r3, r3, 1
    check(text(0x60630001) == "ori r3, r3, 1", "ori");

    // sauts
    check(text(0x48000008) == "b +8", "b vers l'avant");
    check(text(0x4BFFFFFC) == "b -4", "b vers l'arriere");
    check(text(0x48000009) == "bl +8", "bl");
    check(text(0x4E800020) == "blr", "blr");

    // comparaison et sauts conditionnels
    check(text(0x2C030000) == "cmpwi r3, 0", "cmpwi cr0");
    check(text(0x2C8300FF) == "cmpwi cr1, r3, 255", "cmpwi cr1");
    check(text(0x4182000C) == "beq +12", "beq");
    check(text(0x4082FFF8) == "bne -8", "bne");
    check(text(0x4180FFFC) == "blt -4", "blt");
    check(text(0x40850008) == "ble cr1, +8", "ble cr1");
    check(decode(0x2C230000).kind == Kind::Unknown, "cmpdi (64 bits) pas encore gere");

    // mots non reconnus : jamais d'exception
    check(decode(0x00000000).kind == Kind::Unknown, "0 : inconnu");
    check(text(0x12345678) == ".long 0x12345678", "texte : inconnu");
    check(decode(0x7C632215).kind == Kind::Unknown, "add. (Rc = 1) pas encore gere");
    check(decode(0x4E800021).kind == Kind::Unknown, "blrl pas encore gere");

    if (failures == 0) {
        std::cout << "Tous les tests du decodeur PowerPC passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
