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
};

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
