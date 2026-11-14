#pragma once

#include <cstddef>
#include <string>

#include "nerith/memory/memory.h"

namespace nerith::ppc {

// Desassemble "count" instructions PowerPC a la suite, une par ligne, a partir de "address".
// Chaque instruction fait 4 octets. Format : adresse, mot en hexadecimal, texte :
//   00000000  38600005  li r3, 5
// Leve std::out_of_range si les octets ne tiennent pas dans la memoire.
std::string disassemble_range(const Memory& memory, std::size_t address, std::size_t count);

}  // namespace nerith::ppc
