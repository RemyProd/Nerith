#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "nerith/memory/memory.h"

namespace nerith::ppc {

// Etat du processeur PowerPC (version tres reduite pour commencer).
struct Cpu {
    std::array<std::uint64_t, 32> gpr{};   // r0 a r31, 64 bits chacun
    std::uint64_t pc = 0;                  // adresse de la prochaine instruction
    std::uint64_t lr = 0;                  // registre de lien : adresse de retour (bl, blr)
    std::uint64_t ctr = 0;                 // registre compteur : boucles (bdnz) et sauts indirects (bctr)
    std::uint32_t cr = 0;                  // registre de conditions : 8 champs de 4 bits (LT, GT, EQ, SO)
};

// Dans CR, le champ n (0 a 7) occupe 4 bits ; le champ 0 est tout a gauche (bits de poids fort).
// Dans un champ : LT = 8, GT = 4, EQ = 2, SO = 1. Exemple : "egal dans le champ 0" donne cr = 0x20000000.

// Ce qui s'est passe pendant l'execution.
enum class Status {
    Running,              // l'instruction a ete executee, on peut continuer
    Halted,               // blr vers l'adresse 0 : le programme est termine
    InvalidInstruction,   // mot inconnu ou pas encore gere
    OutOfBounds,          // acces en dehors de la memoire
    StepLimit,            // run : trop d'instructions (protege des boucles infinies)
};

// Execute UNE instruction a l'adresse cpu.pc.
// Ne leve pas d'exception. En cas d'erreur (InvalidInstruction, OutOfBounds),
// le processeur et la memoire ne sont pas modifies.
// Convention : "blr" avec lr == 0 termine le programme (Status::Halted).
Status step(Cpu& cpu, Memory& memory);

// Execute des instructions jusqu'a l'arret, une erreur ou max_steps instructions.
Status run(Cpu& cpu, Memory& memory, std::size_t max_steps);

}  // namespace nerith::ppc
