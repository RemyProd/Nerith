#pragma once

#include <string>
#include <vector>

#include "nerith/ppc/decoder.h"
#include "nerith/ppc/vmx.h"

namespace nerith::ppc {

// Decode un mot vectoriel (AltiVec ou VMX128). Renvoie false (et ne touche pas "out") si le mot n'en est pas un.
bool decode_vector(std::uint32_t word, Instruction& out);

// Texte d'une instruction vectorielle, par exemple "vaddfp v3, v4, v5" ou "lvx128 v100, 0, r5".
std::string vector_text(const Instruction& instruction);

// Assemble une ligne vectorielle. Renvoie false si le mnemonique n'est pas vectoriel.
// Leve std::invalid_argument (avec un message en francais) si les operandes sont faux.
bool assemble_vector(const std::string& mnemonic, const std::vector<std::string>& operands, std::uint32_t& word);

}  // namespace nerith::ppc
