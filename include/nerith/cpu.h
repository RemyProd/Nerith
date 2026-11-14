#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "nerith/memory.h"

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
    Load = 0x08,   // registre = mot de 32 bits lu en memoire a l'adresse "valeur"
    Store = 0x09,  // ecrit le registre en memoire (32 bits) a l'adresse "valeur"
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
    OutOfBounds           // registre inexistant, ou lecture hors de la memoire
};

struct Cpu {
    std::array<std::uint32_t, 4> registers{};
    std::size_t pc = 0;   // program counter : adresse de la prochaine instruction
};

// Execute les instructions lues en memoire, a partir de cpu.pc,
// jusqu'a Halt ou jusqu'a une erreur. Store peut modifier la memoire.
Status run(Cpu& cpu, Memory& memory);

}  // namespace nerith
