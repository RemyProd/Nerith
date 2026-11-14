#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "nerith/cpu.h"
#include "nerith/disassembler.h"
#include "nerith/memory.h"

int main() {
    using nerith::code;
    using nerith::Op;

    // Somme de 5 + 4 + 3 + 2 + 1 avec une boucle :
    //   r0 = 5 (compteur) ; r1 = 0 (somme)
    //   adresse 6 : r1 += r0 ; r0 -= 1 ; si r0 != 0, retour a l'adresse 6
    const std::vector<std::uint8_t> program = {
        code(Op::Set),  0, 5,    // adresse 0
        code(Op::Set),  1, 0,    // adresse 3
        code(Op::Add),  1, 0,    // adresse 6  : debut de la boucle
        code(Op::Dec),  0, 0,    // adresse 9
        code(Op::Jnz),  0, 6,    // adresse 12 : si r0 != 0, saute a l'adresse 6
        code(Op::Halt), 0, 0     // adresse 15
    };

    // le programme est charge en memoire, a l'adresse 0
    nerith::Memory memory(64);
    memory.load(0, program);

    // on affiche le programme en clair avant de l'executer
    std::cout << "Programme :\n" << nerith::disassemble_range(memory, 0, 6) << "\n";

    nerith::Cpu cpu;
    const nerith::Status status = nerith::run(cpu, memory);

    if (status != nerith::Status::Halted) {
        std::cerr << "Erreur d'execution\n";
        return 1;
    }

    for (std::size_t i = 0; i < cpu.registers.size(); i++) {
        std::cout << "r" << i << " = " << cpu.registers[i] << "\n";
    }
    std::cout << "instructions executees = " << cpu.instructions_executed << "\n";
    return 0;
}
