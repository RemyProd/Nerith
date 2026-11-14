#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "nerith/cpu.h"
#include "nerith/memory.h"

int main() {
    using nerith::code;
    using nerith::Op;

    // r0 = 5 ; r1 = 20 ; r1 = r1 - r0 ; r1 est ecrit en memoire a l'adresse 32,
    // puis relu dans r2 ; arret
    const std::vector<std::uint8_t> program = {
        code(Op::Set),  0, 5,
        code(Op::Set),  1, 20,
        code(Op::Sub),  1, 0,
        code(Op::Store), 1, 32,
        code(Op::Load),  2, 32,
        code(Op::Halt), 0, 0
    };

    // le programme est charge en memoire, a l'adresse 0
    nerith::Memory memory(64);
    memory.load(0, program);

    nerith::Cpu cpu;
    const nerith::Status status = nerith::run(cpu, memory);

    if (status != nerith::Status::Halted) {
        std::cerr << "Erreur d'execution\n";
        return 1;
    }

    for (std::size_t i = 0; i < cpu.registers.size(); i++) {
        std::cout << "r" << i << " = " << cpu.registers[i] << "\n";
    }

    std::cout << "memoire[32..35] = " << memory.read32(32) << "\n";
    return 0;
}
