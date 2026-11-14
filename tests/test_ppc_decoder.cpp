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
    check(text(0x2C230000) == "cmpdi r3, 0", "cmpdi");
    check(text(0x2CA300FF) == "cmpdi cr1, r3, 255", "cmpdi cr1");

    // instructions ajoutees ensuite (mots verifies avec llvm-mc)
    check(text(0x7C652050) == "sub r3, r4, r5", "sub");
    check(text(0x7C6429D6) == "mullw r3, r4, r5", "mullw");
    check(text(0x7C032000) == "cmpw r3, r4", "cmpw");
    check(text(0x7C842800) == "cmpw cr1, r4, r5", "cmpw cr1");
    check(text(0x7C832378) == "mr r3, r4", "mr");
    check(text(0x7C832B78) == "or r3, r4, r5", "or");
    check(text(0x88610008) == "lbz r3, 8(r1)", "lbz");
    check(text(0x98610008) == "stb r3, 8(r1)", "stb");
    check(text(0x7C64282E) == "lwzx r3, r4, r5", "lwzx");
    check(text(0x7CC7412E) == "stwx r6, r7, r8", "stwx");
    check(text(0x7C6028AE) == "lbzx r3, 0, r5", "lbzx avec rA = 0");
    check(text(0x7D2A59AE) == "stbx r9, r10, r11", "stbx");
    check(decode(0x7C64282F).kind == Kind::Unknown, "lwzx. (Rc = 1) n'existe pas");
    check(text(0x3C608200) == "lis r3, 0x8200", "lis");
    check(text(0x3C640001) == "addis r3, r4, 1", "addis");
    check(decode(0x7C652051).kind == Kind::Unknown, "sub. (Rc = 1) pas encore gere");
    check(text(0x7C232000) == "cmpd r3, r4", "cmpd");
    check(text(0x7CA42800) == "cmpd cr1, r4, r5", "cmpd cr1");
    check(text(0xE8610008) == "ld r3, 8(r1)", "ld");
    check(text(0xE861FFF8) == "ld r3, -8(r1)", "ld negatif");
    check(text(0xF8610008) == "std r3, 8(r1)", "std");
    check(decode(0xE8610009).kind == Kind::Unknown, "ldu (bits 01) pas encore gere");
    check(decode(0xE8610002).kind == Kind::Unknown, "lwa (bits 10) pas encore gere");
    check(decode(0xF8610001).kind == Kind::Unknown, "stdu (bits 01) pas encore gere");

    // pile et lr (mots verifies avec llvm-mc)
    check(text(0x7C0802A6) == "mflr r0", "mflr");
    check(text(0x7D8803A6) == "mtlr r12", "mtlr");
    check(text(0x9421FFF0) == "stwu r1, -16(r1)", "stwu");
    check(decode(0x7C0A02A6).kind == Kind::Unknown, "mfspr d'un autre registre (spr 10) pas encore gere");

    // rotation et masque (mots verifies avec llvm-mc)
    check(text(0x5483298E) == "rlwinm r3, r4, 5, 6, 7", "rlwinm general");
    check(text(0x54830706) == "rlwinm r3, r4, 0, 28, 3", "rlwinm avec masque qui fait le tour");
    check(text(0x5483402E) == "slwi r3, r4, 8", "slwi");
    check(text(0x5483C23E) == "srwi r3, r4, 8", "srwi");
    check(text(0x5483043E) == "clrlwi r3, r4, 16", "clrlwi");
    check(text(0x5483403E) == "rotlwi r3, r4, 8", "rotlwi");
    check(text(0x54830036) == "rlwinm r3, r4, 0, 0, 27", "clrrwi s'affiche en rlwinm (comme llvm)");
    {
        const Instruction r = decode(0x5483298E);
        check(r.kind == Kind::Rlwinm && r.rd == 4 && r.ra == 3 && r.sh == 5 && r.mb == 6 && r.me == 7,
              "rlwinm : champs (rS = 4, rA = 3)");
    }
    check(decode(0x5483402F).kind == Kind::Unknown, "rlwinm. (Rc = 1) pas encore gere");

    // CTR, boucles et sauts indirects (mots verifies avec llvm-mc)
    check(text(0x7C6903A6) == "mtctr r3", "mtctr");
    check(text(0x7C6902A6) == "mfctr r3", "mfctr");
    check(text(0x4200FFF8) == "bdnz -8", "bdnz");
    check(text(0x4240000C) == "bdz +12", "bdz");
    check(text(0x4E800420) == "bctr", "bctr");
    check(text(0x4E800421) == "bctrl", "bctrl");
    check(text(0x4E800021) == "blrl", "blrl");
    check(decode(0x42010008).kind == Kind::Unknown, "bdnz avec BI different de 0 : pas gere");
    check(decode(0x7C6A02A6).kind == Kind::Unknown, "mfspr d'un autre registre (spr 10) pas gere");

    // mots non reconnus : jamais d'exception
    check(decode(0x00000000).kind == Kind::Unknown, "0 : inconnu");
    check(text(0x12345678) == ".long 0x12345678", "texte : inconnu");
    check(decode(0x7C632215).kind == Kind::Unknown, "add. (Rc = 1) pas encore gere");

    if (failures == 0) {
        std::cout << "Tous les tests du decodeur PowerPC passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
