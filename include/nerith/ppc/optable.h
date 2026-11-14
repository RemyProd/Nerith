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
    CrBits,     // name BT, BA, BB        (numeros de bits de CR, de 0 a 31 : crand, cror, crxor...)
    // --- flottants : fN = registre flottant (frD, frA, frB, frC) ---
    FMemD,      // name fD, d(rA)         (lfs, lfd, stfs, stfd et leurs formes "u")
    FMemX,      // name fD, rA, rB        (formes indexees ; rA = 0 veut dire le nombre 0)
    FA3,        // name fD, fA, fB        (fadd, fsub, fdiv : le champ frC doit valoir 0)
    FA3c,       // name fD, fA, fC        (fmul : le champ frB doit valoir 0)
    FA4,        // name fD, fA, fC, fB    (fmadd..., fsel)
    FA2,        // name fD, fB            (fsqrt, fres, frsqrte : frA et frC doivent valoir 0)
    FX2,        // name fD, fB            (fmr, fneg, frsp, fctiw... : frA doit valoir 0)
    FCmp,       // name [crfD,] fA, fB    (fcmpu, fcmpo)
    FrD,        // name fD                (mffs)
    BtOnly,     // name BT                (mtfsb0, mtfsb1 : numero de bit de FPSCR)
    FmFrb,      // name FM, fB            (mtfsf : masque de 8 champs de FPSCR)
    CrfImm,     // name crfD, IMM         (mtfsfi : un champ de FPSCR recoit un nombre de 4 bits)
    CrfCrf,     // name crfD, crfS        (mcrfs)
};

struct OpInfo {
    Kind kind;
    const char* name;   // "mulli", "andi."...
    unsigned opcode;    // bits 0-5 du mot
    int xo;             // opcode 19, 31, 59 et 63 : champ XO (bits 21-30, ou 26-30 pour les formes "A" des flottants) ; opcode 58 et 62 : les 2 bits de poids faible ; sinon -1
    Layout layout;
};

// Toutes les lignes de la table.
const OpInfo* op_table();
std::size_t op_table_size();

// Recherche par type, par nom ou par mot de 32 bits (nullptr si rien ne correspond).
const OpInfo* find_op(Kind kind);
const OpInfo* find_op(const std::string& name);
const OpInfo* find_op_for_word(std::uint32_t word);

// Bits du mot qui doivent valoir 0 pour que la forme soit valide (champs reserves ou inutilises).
std::uint32_t reserved_mask(Layout layout);

// Vrai pour les formes "A" des flottants, dont l'opcode etendu n'a que 5 bits (bits 26-30).
bool is_a_form(Layout layout);

}  // namespace nerith::ppc
