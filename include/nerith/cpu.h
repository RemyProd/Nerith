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
    Jmp  = 0x0A,   // saute a l'adresse "valeur" (le registre est ignore)
    Jnz  = 0x0B,   // saute a l'adresse "valeur" si le registre n'est pas zero
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
    OutOfBounds,          // registre inexistant, ou lecture hors de la memoire
    StepLimit             // trop d'instructions : boucle infinie probable
};

struct Cpu {
    std::array<std::uint32_t, 4> registers{};
    std::size_t pc = 0;   // program counter : adresse de la prochaine instruction
    std::size_t instructions_executed = 0;   // nombre d'instructions lues et executees
};

// Execute les instructions lues en memoire, a partir de cpu.pc,
// jusqu'a Halt ou jusqu'a une erreur. Store peut modifier la memoire.
// Si le programme depasse max_steps instructions, on s'arrete avec StepLimit.
Status run(Cpu& cpu, Memory& memory, std::size_t max_steps = 1000000);

}  // namespace nerith
