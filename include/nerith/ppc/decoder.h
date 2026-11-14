#pragma once

#include <cstdint>
#include <string>

namespace nerith::ppc {

// Les instructions PowerPC que le decodeur reconnait pour l'instant.
enum class Kind {
    Unknown,   // mot de 32 bits non reconnu (ou forme pas encore geree)
    Addi,      // addi rD, rA, SIMM   (rD = rA + SIMM ; si rA vaut 0, on utilise la valeur 0)
    Add,       // add rD, rA, rB
    Ori,       // ori rA, rS, UIMM    (nop = ori 0, 0, 0)
    Lwz,       // lwz rD, d(rA)       (lire un mot de 32 bits)
    Stw,       // stw rS, d(rA)       (ecrire un mot de 32 bits)
    B,         // b / bl / ba / bla   (saut ; bl sauvegarde le retour dans LR)
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
};

// Decode un mot de 32 bits (deja lu en big-endian, par exemple avec Memory::read32).
// Ne leve jamais d'exception : un mot inconnu donne Kind::Unknown.
Instruction decode(std::uint32_t word);

// Ecrit l'instruction en texte, par exemple "li r3, 5" ou "lwz r3, 8(r1)".
// Un mot inconnu donne ".long 0x12345678".
std::string to_text(const Instruction& instruction);

}  // namespace nerith::ppc
