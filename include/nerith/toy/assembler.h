#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace nerith {

// Erreur dans un programme source. line() donne le numero de la ligne (a partir de 1).
class AssemblyError : public std::runtime_error {
public:
    AssemblyError(std::size_t line, const std::string& message);

    std::size_t line() const { return line_; }

private:
    std::size_t line_;
};

// Transforme un programme ecrit en texte en octets, prets pour Memory::load.
//
// Syntaxe, une instruction par ligne :
//   Set r0, 5          registre = nombre
//   Add r1, r0         Add, Sub, Mul : deux registres
//   Inc r0             Inc, Dec : un registre
//   Load r1, [20]      Load, Store : un registre et une adresse (crochets facultatifs)
//   Jmp loop           saut vers une etiquette ou une adresse
//   Jnz r0, loop       saut si le registre n'est pas zero
//   Halt
//
// - Les nombres sont en decimal (12) ou en hexadecimal (0x0C), de 0 a 255.
// - Une etiquette se declare avec ":" ("loop:") seule ou devant une instruction.
// - Un ";" commence un commentaire. Les majuscules et minuscules sont equivalentes
//   pour les instructions et les registres, mais pas pour les etiquettes.
//
// Leve AssemblyError si le programme est invalide.
std::vector<std::uint8_t> assemble(const std::string& source);

}  // namespace nerith
