#pragma once

#include <cstddef>
#include <string>

#include "nerith/ppc/decoder.h"

namespace nerith::ppc {

// Table des instructions "simples" : celles qui se decrivent par un code (opcode, et parfois XO)
// et par la place de leurs champs. Le decodeur, l'assembleur et le texte s'en servent tous les trois :
// ajouter une ligne ici suffit pour reconnaitre, ecrire et afficher une nouvelle instruction.
// (Il faut quand meme ecrire ce qu'elle FAIT, dans cpu.cpp.)

// Ou sont les registres et les nombres dans le mot de 32 bits.
// "rS" et "rD" occupent les bits 6-10 (champ rd de Instruction), rA les bits 11-15, rB les bits 16-20.
enum class Layout {
    RtRaRb,     // name rD, rA, rB
    RaRsRb,     // name rA, rS, rB        (instructions logiques : la destination est rA)
    RaRs,       // name rA, rS            (rB doit valoir 0)
    RaRsSh,     // name rA, rS, SH       (le champ rB contient le decalage SH, de 0 a 31)
    RtRbRa,     // name rD, rB, rA        (ordre inverse, comme llvm l'ecrit pour subc)
    RtRa,       // name rD, rA            (rB doit valoir 0)
    RtRaSimm,   // name rD, rA, SIMM      (nombre avec signe, 16 bits)
    RaRsUimm,   // name rA, rS, UIMM      (nombre sans signe, 16 bits)
    MemD,       // name rD, d(rA)         (deplacement de 16 bits avec signe)
    MemDS,      // name rD, ds(rA)        (deplacement multiple de 4 : les 2 bits de poids faible servent de code)
    MemX,       // name rD, rA, rB        (adresse rA + rB ; rA = 0 veut dire le nombre 0)
};

struct OpInfo {
    Kind kind;
    const char* name;   // "mulli", "andi."...
    unsigned opcode;    // bits 0-5 du mot
    int xo;             // opcode 31 : champ XO (bits 21-30) ; opcode 58 et 62 : les 2 bits de poids faible ; sinon -1
    Layout layout;
};

// Toutes les lignes de la table.
const OpInfo* op_table();
std::size_t op_table_size();

// Recherche par type, par nom ou par mot de 32 bits (nullptr si rien ne correspond).
const OpInfo* find_op(Kind kind);
const OpInfo* find_op(const std::string& name);
const OpInfo* find_op_for_word(std::uint32_t word);

}  // namespace nerith::ppc
