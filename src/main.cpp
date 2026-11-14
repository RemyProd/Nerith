#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "nerith/assembler.h"
#include "nerith/cpu.h"
#include "nerith/disassembler.h"
#include "nerith/file.h"
#include "nerith/memory.h"

namespace {

// Programme utilise quand on lance nerith.exe sans fichier.
// "loop:" est une etiquette : elle donne un nom a l'adresse de l'instruction suivante.
const char* const kDemoProgram =
    "; somme de 5 + 4 + 3 + 2 + 1\n"
    "        Set r0, 5       ; compteur\n"
    "        Set r1, 0       ; somme\n"
    "loop:   Add r1, r0\n"
    "        Dec r0\n"
    "        Jnz r0, loop    ; tant que r0 != 0, on recommence\n"
    "        Halt\n";

const char* status_text(nerith::Status status) {
    switch (status) {
        case nerith::Status::Halted:             return "arret normal";
        case nerith::Status::InvalidInstruction: return "instruction invalide";
        case nerith::Status::OutOfBounds:        return "acces hors limites";
        case nerith::Status::StepLimit:          return "trop d'instructions (boucle infinie ?)";
    }
    return "inconnu";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc > 2) {
        std::cerr << "Usage : nerith [programme.asm]\n";
        return 2;
    }

    try {
        // le texte vient d'un fichier si on en donne un, sinon du programme de demonstration
        const std::string source = (argc == 2) ? nerith::read_text_file(argv[1]) : kDemoProgram;

        // l'assembleur transforme le texte en octets
        const std::vector<std::uint8_t> program = nerith::assemble(source);

        // le programme est charge en memoire, a l'adresse 0
        nerith::Memory memory(256);
        memory.load(0, program);

        // on affiche le programme en clair avant de l'executer
        std::cout << "Programme :\n"
                  << nerith::disassemble_range(memory, 0, program.size() / 3) << "\n";

        nerith::Cpu cpu;
        const nerith::Status status = nerith::run(cpu, memory);

        std::cout << "Fin : " << status_text(status) << "\n";
        for (std::size_t i = 0; i < cpu.registers.size(); i++) {
            std::cout << "r" << i << " = " << cpu.registers[i] << "\n";
        }
        std::cout << "pc = " << cpu.pc << "\n";
        std::cout << "instructions executees = " << cpu.instructions_executed << "\n";
        return status == nerith::Status::Halted ? 0 : 1;
    } catch (const nerith::AssemblyError& error) {
        std::cerr << "Erreur d'assemblage, " << error.what() << "\n";
    } catch (const std::exception& error) {
        std::cerr << "Erreur : " << error.what() << "\n";
    }
    return 1;
}
