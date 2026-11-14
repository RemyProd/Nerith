#include <cstdint>
#include <iostream>
#include <vector>

#include "nerith/cpu.h"

namespace {

int failures = 0;

void check(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "ECHEC : " << name << "\n";
        ++failures;
    }
}

}  // namespace

int main() {
    using namespace nerith;

    {   // Set puis Halt
        Cpu cpu;
        const Status st = run(cpu, {code(Op::Set), 0, 5, code(Op::Halt), 0, 0});
        check(st == Status::Halted, "Set : le programme s'arrete avec Halt");
        check(cpu.registers[0] == 5, "Set : r0 vaut 5");
    }
    {   // Sub : r1 = 20 - 5
        Cpu cpu;
        const Status st = run(cpu, {
            code(Op::Set), 0, 5,
            code(Op::Set), 1, 20,
            code(Op::Sub), 1, 0,
            code(Op::Halt), 0, 0});
        check(st == Status::Halted, "Sub : arret normal");
        check(cpu.registers[1] == 15, "Sub : r1 vaut 15");
    }
    {   // Inc
        Cpu cpu;
        run(cpu, {code(Op::Set), 2, 9, code(Op::Inc), 2, 0, code(Op::Halt), 0, 0});
        check(cpu.registers[2] == 10, "Inc : r2 vaut 10");
    }


    {   // Dec : r0 = 3 puis r0 - 1
        Cpu cpu;
        run(cpu, {code(Op::Set), 0, 3, code(Op::Dec), 0, 0, code(Op::Halt), 0, 0});
        check(cpu.registers[0] == 2, "Dec : r0 vaut 2");
    }


    {   // opcode inconnu
        Cpu cpu;
        const Status st = run(cpu, {0x77, 0, 0});
        check(st == Status::InvalidInstruction, "opcode inconnu detecte");
    }
    {   // registre inexistant
        Cpu cpu;
        const Status st = run(cpu, {code(Op::Set), 9, 1, code(Op::Halt), 0, 0});
        check(st == Status::OutOfBounds, "registre 9 hors limites detecte");
    }
    {   // programme sans Halt : on lit apres la fin
        Cpu cpu;
        const Status st = run(cpu, {code(Op::Set), 0, 1});
        check(st == Status::OutOfBounds, "programme sans Halt detecte");
    }

    if (failures == 0) {
        std::cout << "Tous les tests passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
