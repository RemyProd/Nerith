#pragma once

#include "nerith/ppc/cpu.h"
#include "nerith/ppc/decoder.h"

namespace nerith::ppc {

// Execute une instruction Kind::Vector (AltiVec ou VMX128). Meme contrat que les autres :
// en cas d'erreur (InvalidInstruction, OutOfBounds) rien n'est modifie ; sinon pc avance de 4.
Status execute_vector(const Instruction& instruction, Cpu& cpu, Memory& memory);

}  // namespace nerith::ppc
