#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "nerith/memory/memory.h"
#include "nerith/ppc/assembler.h"
#include "nerith/ppc/cpu.h"
#include "nerith/ppc/disassembler.h"
#include "nerith/toy/assembler.h"
#include "nerith/toy/cpu.h"
#include "nerith/toy/disassembler.h"
#include "nerith/util/file.h"
#include "nerith/util/log.h"
#include "nerith/xex/header.h"

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
        case nerith::Status::Running:            return "toujours en cours";
        case nerith::Status::Halted:             return "arret normal";
        case nerith::Status::InvalidInstruction: return "instruction invalide";
        case nerith::Status::OutOfBounds:        return "acces hors limites";
        case nerith::Status::StepLimit:          return "trop d'instructions (boucle infinie ?)";
    }
    return "inconnu";
}

void print_registers(const nerith::Cpu& cpu) {
    for (std::size_t i = 0; i < cpu.registers.size(); i++) {
        std::cout << "r" << i << "=" << cpu.registers[i] << (i + 1 < cpu.registers.size() ? " " : "");
    }
}

// Comme nerith::run, mais affiche chaque instruction et les registres apres son execution.
nerith::Status run_with_trace(nerith::Cpu& cpu, nerith::Memory& memory, std::size_t max_steps) {
    std::cout << "Trace d'execution :\n";
    while (cpu.instructions_executed < max_steps) {
        const std::size_t address = cpu.pc;

        // l'instruction en clair, avant de l'executer (elle peut etre hors de la memoire)
        std::string text;
        try {
            text = nerith::disassemble(memory, address);
        } catch (const std::out_of_range&) {
            text = "(hors memoire)";
        }

        const nerith::Status status = nerith::step(cpu, memory);

        std::cout << std::setfill('0') << std::setw(4) << address << std::setfill(' ') << "  "
                  << std::left << std::setw(16) << text << std::right << " | ";
        print_registers(cpu);
        std::cout << "\n";

        if (status != nerith::Status::Running) {
            return status;
        }
    }
    return nerith::Status::StepLimit;
}

// --- Mode PowerPC (option --ppc) ---

// Programme utilise quand on lance "nerith --ppc" sans fichier.
const char* const kPpcDemoProgram =
    "; somme de 3 + 2 + 1\n"
    "        li r3, 3        # compteur\n"
    "        li r4, 0        # somme\n"
    "loop:   add r4, r4, r3\n"
    "        addi r3, r3, -1\n"
    "        cmpwi r3, 0\n"
    "        bne loop        # tant que r3 != 0, on recommence\n"
    "        blr             # lr vaut 0 : fin du programme\n";

const char* ppc_status_text(nerith::ppc::Status status) {
    switch (status) {
        case nerith::ppc::Status::Running:            return "toujours en cours";
        case nerith::ppc::Status::Halted:             return "arret normal";
        case nerith::ppc::Status::InvalidInstruction: return "instruction invalide";
        case nerith::ppc::Status::OutOfBounds:        return "acces hors limites";
        case nerith::ppc::Status::StepLimit:          return "trop d'instructions (boucle infinie ?)";
    }
    return "inconnu";
}

// Affiche les registres qui ne valent pas 0, plus lr et cr.
void print_ppc_registers(const nerith::ppc::Cpu& cpu) {
    for (std::size_t i = 0; i < cpu.gpr.size(); i++) {
        if (cpu.gpr[i] != 0) {
            std::cout << "r" << i << "=" << cpu.gpr[i] << " ";
        }
    }
    std::cout << "lr=" << cpu.lr << " cr=0x" << std::hex << std::setw(8) << std::setfill('0')
              << cpu.cr << std::dec << std::setfill(' ');
}

int run_ppc(const char* path, bool trace) {
    try {
        const std::string source = (path != nullptr) ? nerith::read_text_file(path) : kPpcDemoProgram;
        const std::vector<std::uint32_t> words = nerith::ppc::assemble(source);
        nerith::log_info("assemblage PowerPC reussi : " + std::to_string(words.size()) + " instructions");

        nerith::Memory memory(4096);
        for (std::size_t i = 0; i < words.size(); i++) {
            memory.write32(i * 4, words[i]);
        }

        std::cout << "Programme PowerPC :\n" << nerith::ppc::disassemble_range(memory, 0, words.size())
                  << "\n";

        nerith::ppc::Cpu cpu;
        nerith::ppc::Status status = nerith::ppc::Status::StepLimit;
        std::size_t executed = 0;
        const std::size_t max_steps = 1000;
        if (trace) {
            std::cout << "Trace d'execution :\n";
        }
        while (executed < max_steps) {
            std::string line;
            if (trace) {
                try {
                    line = nerith::ppc::disassemble_range(memory, static_cast<std::size_t>(cpu.pc), 1);
                    line.pop_back();   // enleve le retour a la ligne
                } catch (const std::out_of_range&) {
                    line = "(hors memoire)";
                }
            }
            status = nerith::ppc::step(cpu, memory);
            ++executed;
            if (trace) {
                std::cout << line << " | ";
                print_ppc_registers(cpu);
                std::cout << "\n";
            }
            if (status != nerith::ppc::Status::Running) {
                break;
            }
            if (executed == max_steps) {
                status = nerith::ppc::Status::StepLimit;
            }
        }

        std::cout << (trace ? "\n" : "") << "Fin : " << ppc_status_text(status) << "\n";
        print_ppc_registers(cpu);
        std::cout << "\npc = " << cpu.pc << "\ninstructions executees = " << executed << "\n";
        return status == nerith::ppc::Status::Halted ? 0 : 1;
    } catch (const nerith::ppc::AssemblyError& error) {
        nerith::log_error(std::string("Erreur d'assemblage PowerPC, ") + error.what());
    } catch (const std::exception& error) {
        nerith::log_error(std::string("Erreur : ") + error.what());
    }
    return 1;
}

// --- Mode XEX (option --xex) ---

// Affiche l'en-tete d'un fichier .xex. Ne lit que les premiers octets : rien n'est dechiffre ni decompresse.
int run_xex(const char* path) {
    if (path == nullptr) {
        std::cerr << "Usage : nerith --xex fichier.xex\n";
        return 2;
    }
    try {
        const std::size_t kMaxHeaderBytes = 65536;   // l'en-tete et la liste des en-tetes optionnels tiennent dedans
        const std::vector<std::uint8_t> bytes = nerith::read_binary_file(path, kMaxHeaderBytes);
        nerith::log_info("lu : " + std::to_string(bytes.size()) + " octets du debut du fichier");
        std::cout << nerith::xex::describe(nerith::xex::parse_header(bytes));
        return 0;
    } catch (const nerith::xex::XexError& error) {
        nerith::log_error(std::string("Erreur XEX : ") + error.what());
    } catch (const std::exception& error) {
        nerith::log_error(std::string("Erreur : ") + error.what());
    }
    return 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    // arguments : [--ppc | --xex] [--trace] [--verbose] [fichier]
    bool trace = false;
    bool ppc = false;
    bool xex = false;
    const char* path = nullptr;
    for (int i = 1; i < argc; i++) {
        const std::string argument = argv[i];
        if (argument == "--trace" && !trace) {
            trace = true;
        } else if (argument == "--ppc") {
            ppc = true;
        } else if (argument == "--xex") {
            xex = true;
        } else if (argument == "--verbose") {
            nerith::set_log_level(nerith::LogLevel::Debug);
        } else if (path == nullptr && argument.rfind("--", 0) != 0) {
            path = argv[i];
        } else {
            std::cerr << "Usage : nerith [--ppc | --xex] [--trace] [--verbose] [fichier]\n";
            return 2;
        }
    }

    if (ppc && xex) {
        std::cerr << "Usage : --ppc et --xex ne se combinent pas\n";
        return 2;
    }
    if (xex) {
        return run_xex(path);
    }
    if (ppc) {
        return run_ppc(path, trace);
    }

    try {
        // le texte vient d'un fichier si on en donne un, sinon du programme de demonstration
        if (path != nullptr) {
            nerith::log_info(std::string("lecture du fichier ") + path);
        } else {
            nerith::log_info("aucun fichier : programme de demonstration");
        }
        const std::string source = (path != nullptr) ? nerith::read_text_file(path) : kDemoProgram;

        // l'assembleur transforme le texte en octets
        const std::vector<std::uint8_t> program = nerith::assemble(source);
        nerith::log_info("assemblage reussi : " + std::to_string(program.size() / 3) +
                         " instructions, " + std::to_string(program.size()) + " octets");

        // le programme est charge en memoire, a l'adresse 0
        nerith::Memory memory(256);
        memory.load(0, program);
        nerith::log_debug("programme charge a l'adresse 0, memoire de " +
                          std::to_string(memory.size()) + " octets");

        // on affiche le programme en clair avant de l'executer
        std::cout << "Programme :\n"
                  << nerith::disassemble_range(memory, 0, program.size() / 3) << "\n";

        nerith::Cpu cpu;
        const nerith::Status status =
            trace ? run_with_trace(cpu, memory, 1000) : nerith::run(cpu, memory);

        nerith::log_info(std::string("fin de l'execution : ") + status_text(status));

        std::cout << (trace ? "\n" : "") << "Fin : " << status_text(status) << "\n";
        for (std::size_t i = 0; i < cpu.registers.size(); i++) {
            std::cout << "r" << i << " = " << cpu.registers[i] << "\n";
        }
        std::cout << "pc = " << cpu.pc << "\n";
        std::cout << "instructions executees = " << cpu.instructions_executed << "\n";
        return status == nerith::Status::Halted ? 0 : 1;
    } catch (const nerith::AssemblyError& error) {
        nerith::log_error(std::string("Erreur d'assemblage, ") + error.what());
    } catch (const std::exception& error) {
        nerith::log_error(std::string("Erreur : ") + error.what());
    }
    return 1;
}
