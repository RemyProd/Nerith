#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "nerith/assembler.h"
#include "nerith/cpu.h"
#include "nerith/disassembler.h"
#include "nerith/memory.h"

int main() {
    // Somme de 5 + 4 + 3 + 2 + 1 avec une boucle, ecrite en texte.
    // "loop:" est une etiquette : elle donne un nom a l'adresse de l'instruction suivante.
    const std::string source =
        "; somme de 5 + 4 + 3 + 2 + 1\n"
        "        Set r0, 5       ; compteur\n"
        "        Set r1, 0       ; somme\n"
        "loop:   Add r1, r0\n"
        "        Dec r0\n"
        "        Jnz r0, loop    ; tant que r0 != 0, on recommence\n"
        "        Halt\n";

    // l'assembleur transforme le texte en octets
    std::vector<std::uint8_t> program;
    try {
        program = nerith::assemble(source);
    } catch (const nerith::AssemblyError& error) {
        std::cerr << "Erreur d'assemblage, " << error.what() << "\n";
        return 1;
    }

    // le programme est charge en memoire, a l'adresse 0
    nerith::Memory memory(64);
    memory.load(0, program);

    // on affiche le programme en clair avant de l'executer
    std::cout << "Programme :\n"
              << nerith::disassemble_range(memory, 0, program.size() / 3) << "\n";

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
