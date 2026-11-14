#pragma once

#include <cstddef>
#include <string>

#include "nerith/memory/memory.h"

namespace nerith {

// Transforme l'instruction de 3 octets situee a cette adresse en une ligne de texte,
// par exemple "Add r1, r0". Un opcode inconnu donne "??? 0x77".
// Leve std::out_of_range si les 3 octets ne tiennent pas dans la memoire.
std::string disassemble(const Memory& memory, std::size_t address);

// Desassemble "count" instructions a la suite, une par ligne, avec leur adresse :
//   0006  Add r1, r0
std::string disassemble_range(const Memory& memory, std::size_t address, std::size_t count);

}  // namespace nerith
