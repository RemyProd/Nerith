#pragma once

#include <cstdint>
#include <string>

namespace nerith::ppc {

// Les instructions PowerPC que le decodeur reconnait pour l'instant.
enum class Kind {
    Unknown,   // mot de 32 bits non reconnu (ou forme pas encore geree)
    Addi,      // addi rD, rA, SIMM   (rD = rA + SIMM ; si rA vaut 0, on utilise la valeur 0)
    Addis,     // addis rD, rA, SIMM  (rA + SIMM * 65536 ; "lis rD, nombre" quand rA vaut 0)
    Add,       // add rD, rA, rB
    Sub,       // sub rD, rA, rB      (rD = rA - rB ; dans le mot, les champs rA et rB sont inverses)
    Mullw,     // mullw rD, rA, rB    (multiplication avec signe des 32 bits de poids faible)
    Or,        // or rA, rS, rB       ("mr rA, rS" quand rS et rB sont identiques)
    Cmpw,      // cmpw crfD, rA, rB   (compare deux registres, resultat dans un champ de CR)
    Ori,       // ori rA, rS, UIMM    (nop = ori 0, 0, 0)
    Cmpwi,     // cmpwi crfD, rA, SIMM (compare rA a un nombre, resultat dans un champ de CR)
    Lwz,       // lwz rD, d(rA)       (lire un mot de 32 bits)
    Stw,       // stw rS, d(rA)       (ecrire un mot de 32 bits)
    Stwu,      // stwu rS, d(rA)      (comme stw, puis rA = adresse ecrite : sert a creer une pile)
    Mflr,      // mflr rD             (rD = LR)
    Mtlr,      // mtlr rS             (LR = rS)
    Lbz,       // lbz rD, d(rA)       (lire un octet, complete par des 0)
    Stb,       // stb rS, d(rA)       (ecrire un octet : les 8 bits de poids faible de rS)
    B,         // b / bl / ba / bla   (saut ; bl sauvegarde le retour dans LR)
    Bc,        // saut conditionnel : beq, bne, blt, bgt, ble, bge (selon un bit de CR)
    Blr,       // blr                 (retour de fonction : saute a LR)
};

// Le resultat du decodage d'un mot de 32 bits.
// Selon l'instruction, certains champs ne servent pas (ils valent 0).
struct Instruction {
    Kind kind = Kind::Unknown;
    std::uint32_t word = 0;   // le mot d'origine
    std::uint8_t rd = 0;      // registre destination (ou source pour stw et ori : champ rS)
    std::uint8_t ra = 0;
    std::uint8_t rb = 0;
    std::int32_t imm = 0;     // valeur immediate (avec signe) ou deplacement du saut, en octets
    bool absolute = false;    // B : bit AA (adresse absolue au lieu de relative)
    bool link = false;        // B : bit LK (sauvegarde l'adresse de retour dans LR)
    std::uint8_t crf = 0;     // Cmpwi : numero du champ de CR (0 a 7)
    std::uint8_t bo = 0;      // Bc : 12 = saute si le bit est a 1, 4 = saute si le bit est a 0
    std::uint8_t bi = 0;      // Bc : numero du bit de CR teste (0 a 31 ; champ = bi / 4)
};

// Decode un mot de 32 bits (deja lu en big-endian, par exemple avec Memory::read32).
// Ne leve jamais d'exception : un mot inconnu donne Kind::Unknown.
Instruction decode(std::uint32_t word);

// Ecrit l'instruction en texte, par exemple "li r3, 5" ou "lwz r3, 8(r1)".
// Un mot inconnu donne ".long 0x12345678".
std::string to_text(const Instruction& instruction);

}  // namespace nerith::ppc
