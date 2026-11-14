#include <cstdint>
#include <iostream>
#include <vector>

#include "nerith/cpu.h"
#include "nerith/memory.h"

namespace {

int failures = 0;

void check(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "ECHEC : " << name << "\n";
        ++failures;
    }
}

// Charge le programme en memoire (juste assez grande pour lui) et l'execute.
nerith::Status run_program(nerith::Cpu& cpu, const std::vector<std::uint8_t>& program) {
    nerith::Memory memory(program.size());
    memory.load(0, program);
    return nerith::run(cpu, memory);
}

}  // namespace

int main() {
    using namespace nerith;

    {   // Set puis Halt
        Cpu cpu;
        const Status st = run_program(cpu, {code(Op::Set), 0, 5, code(Op::Halt), 0, 0});
        check(st == Status::Halted, "Set : le programme s'arrete avec Halt");
        check(cpu.registers[0] == 5, "Set : r0 vaut 5");
    }
    {   // Sub : r1 = 20 - 5
        Cpu cpu;
        const Status st = run_program(cpu, {
            code(Op::Set), 0, 5,
            code(Op::Set), 1, 20,
            code(Op::Sub), 1, 0,
            code(Op::Halt), 0, 0});
        check(st == Status::Halted, "Sub : arret normal");
        check(cpu.registers[1] == 15, "Sub : r1 vaut 15");
    }
    {   // Inc
        Cpu cpu;
        run_program(cpu, {code(Op::Set), 2, 9, code(Op::Inc), 2, 0, code(Op::Halt), 0, 0});
        check(cpu.registers[2] == 10, "Inc : r2 vaut 10");
    }
    {   // Dec : r0 = 3 puis r0 - 1
        Cpu cpu;
        run_program(cpu, {code(Op::Set), 0, 3, code(Op::Dec), 0, 0, code(Op::Halt), 0, 0});
        check(cpu.registers[0] == 2, "Dec : r0 vaut 2");
    }
    {   // opcode inconnu
        Cpu cpu;
        const Status st = run_program(cpu, {0x77, 0, 0});
        check(st == Status::InvalidInstruction, "opcode inconnu detecte");
    }
    {   // registre inexistant
        Cpu cpu;
        const Status st = run_program(cpu, {code(Op::Set), 9, 1, code(Op::Halt), 0, 0});
        check(st == Status::OutOfBounds, "registre 9 hors limites detecte");
    }
    {   // programme sans Halt dans une memoire juste assez grande : lecture hors limites
        Cpu cpu;
        const Status st = run_program(cpu, {code(Op::Set), 0, 1});
        check(st == Status::OutOfBounds, "programme sans Halt detecte");
    }

    // --- Load et Store : le CPU lit et ecrit la memoire ---

    {   // Store ecrit le registre en memoire, en big-endian
        Memory memory(64);
        memory.load(0, {code(Op::Set), 0, 0xAB, code(Op::Store), 0, 40, code(Op::Halt), 0, 0});
        Cpu cpu;
        const Status st = run(cpu, memory);
        check(st == Status::Halted, "Store : arret normal");
        check(memory.read32(40) == 0xAB, "Store : le mot 32 bits vaut 0xAB");
        check(memory.read8(43) == 0xAB, "Store : l'octet de poids faible est a l'adresse 43");
        check(memory.read8(40) == 0, "Store : l'octet de poids fort vaut 0");
    }
    {   // Load lit un mot de 32 bits depuis la memoire
        Memory memory(64);
        memory.write32(20, 0x12345678);
        memory.load(0, {code(Op::Load), 1, 20, code(Op::Halt), 0, 0});
        Cpu cpu;
        const Status st = run(cpu, memory);
        check(st == Status::Halted, "Load : arret normal");
        check(cpu.registers[1] == 0x12345678, "Load : r1 vaut 0x12345678");
    }
    {   // Store puis Load : on retrouve la valeur
        Memory memory(64);
        memory.load(0, {
            code(Op::Set),   0, 99,
            code(Op::Store), 0, 32,
            code(Op::Load),  3, 32,
            code(Op::Halt),  0, 0});
        Cpu cpu;
        run(cpu, memory);
        check(cpu.registers[3] == 99, "Store puis Load : r3 vaut 99");
    }
    {   // Store hors limites : refuse, et rien n'est ecrit
        Memory memory(64);
        memory.load(0, {code(Op::Set), 0, 7, code(Op::Store), 0, 62, code(Op::Halt), 0, 0});
        Cpu cpu;
        const Status st = run(cpu, memory);
        check(st == Status::OutOfBounds, "Store a l'adresse 62 hors limites detecte");
        check(memory.read8(62) == 0, "Store refuse : l'octet 62 reste a 0");
        check(memory.read8(63) == 0, "Store refuse : l'octet 63 reste a 0");
    }

    // --- Nouveaux tests : le CPU lit son programme en memoire ---

    {   // pc avance de 3 octets par instruction
        Cpu cpu;
        run_program(cpu, {code(Op::Set), 0, 5, code(Op::Halt), 0, 0});
        check(cpu.pc == 6, "pc vaut 6 apres 2 instructions");
    }
    {   // un programme charge ailleurs qu'a l'adresse 0
        Memory memory(64);
        memory.load(8, {code(Op::Set), 1, 7, code(Op::Halt), 0, 0});
        Cpu cpu;
        cpu.pc = 8;
        const Status st = run(cpu, memory);
        check(st == Status::Halted, "programme charge a l'adresse 8 : arret normal");
        check(cpu.registers[1] == 7, "programme charge a l'adresse 8 : r1 vaut 7");
        check(cpu.pc == 14, "programme charge a l'adresse 8 : pc vaut 14");
    }
    {   // sans Halt, le CPU tombe sur des zeros : 0x00 n'est pas une instruction
        Memory memory(64);
        memory.load(0, {code(Op::Set), 0, 1});
        Cpu cpu;
        const Status st = run(cpu, memory);
        check(st == Status::InvalidInstruction, "memoire a zero : instruction invalide");
        check(cpu.registers[0] == 1, "l'instruction Set a bien ete executee avant");
    }

    if (failures == 0) {
        std::cout << "Tous les tests passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
