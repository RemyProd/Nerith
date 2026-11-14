#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace nerith {

// Instructions du CPU pedagogique. Chaque instruction fait 3 octets :
// [opcode, registre, valeur]
enum class Op : std::uint8_t {
    Set  = 0x01,   // registre = valeur
    Add  = 0x02,   // registre += registre numero "valeur"
    Sub  = 0x04,   // registre -= registre numero "valeur"
    Mul  = 0x05,   // registre *= registre numero "valeur"
    Inc  = 0x06,   // registre += 1 (la valeur est ignoree)
    Dec  = 0x07,   // registre -= 1 (la valeur est ignoree)
    Halt = 0xFF    // arret propre
};

// Donne le numero d'un opcode, pour ecrire un programme en octets.
constexpr std::uint8_t code(Op op) {
    return static_cast<std::uint8_t>(op);
}

// Resultat de l'execution d'un programme.
enum class Status {
    Halted,               // le programme s'est termine avec Halt
    InvalidInstruction,   // opcode inconnu
    OutOfBounds           // registre inexistant, ou lecture apres la fin du programme
};

struct Cpu {
    std::array<std::uint32_t, 4> registers{};
};

// Execute le programme depuis le debut jusqu'a Halt ou jusqu'a une erreur.
Status run(Cpu& cpu, const std::vector<std::uint8_t>& program);

}  // namespace nerith
