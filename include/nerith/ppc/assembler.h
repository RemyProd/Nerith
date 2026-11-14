#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace nerith::ppc {

// Erreur dans un programme source PowerPC. line() donne le numero de la ligne (a partir de 1).
class AssemblyError : public std::runtime_error {
public:
    AssemblyError(std::size_t line, const std::string& message);

    std::size_t line() const { return line_; }

private:
    std::size_t line_;
};

// Transforme un programme PowerPC ecrit en texte en mots de 32 bits
// (le premier mot est a l'adresse 0, le suivant a l'adresse 4, etc.).
// Pour le mettre en memoire : memory.write32(4 * i, words[i]).
//
// Syntaxe, une instruction par ligne :
//   li r3, 5            addi r3, r4, -1       add r5, r3, r4
//   ori r3, r4, 0x10    nop                   lis r3, 0x8200      addis r3, r4, 1
//   sub r3, r4, r5      (r3 = r4 - r5)        mullw r3, r4, r5    mr r3, r4     or r3, r4, r5
//   cmpw r3, r4         cmpw cr1, r3, r4
//   mflr r0             mtlr r0               stwu r1, -16(r1)    (pile : r1 = r1 - 16, puis ecrit)
//   lwz r3, 8(r1)       stw r3, 8(r1)         lbz r3, 8(r1)       stb r3, 8(r1)
//   (r0 entre parentheses = base 0)
//   cmpwi r3, 0         cmpwi cr1, r3, 0
//   b loop              bl func               blr
//   beq loop   bne loop   blt loop   bgt loop   ble loop   bge loop   (ou "bne cr1, loop")
//
// - Les nombres sont en decimal (-12) ou en hexadecimal (0x0C).
// - Une etiquette se declare avec ":" ("loop:"), seule ou devant une instruction.
// - Un saut accepte une etiquette ou un deplacement en octets avec signe ("+8", "-4").
// - Un ";" ou un "#" commence un commentaire. Les mnemoniques et registres ne
//   tiennent pas compte des majuscules, mais les etiquettes oui.
//
// Leve AssemblyError si le programme est invalide.
std::vector<std::uint32_t> assemble(const std::string& source);

}  // namespace nerith::ppc
