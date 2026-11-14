#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "nerith/memory/memory.h"
#include "nerith/toy/cpu.h"
#include "nerith/toy/disassembler.h"

namespace {

int failures = 0;

void check(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "ECHEC : " << name << "\n";
        ++failures;
    }
}

// Charge une seule instruction a l'adresse 0 et la desassemble.
std::string one(std::uint8_t opcode, std::uint8_t reg, std::uint8_t value) {
    nerith::Memory memory(16);
    memory.load(0, {opcode, reg, value});
    return nerith::disassemble(memory, 0);
}

}  // namespace

int main() {
    using namespace nerith;

    check(one(code(Op::Set), 0, 5) == "Set r0, 5", "Set");
    check(one(code(Op::Add), 1, 0) == "Add r1, r0", "Add");
    check(one(code(Op::Sub), 2, 3) == "Sub r2, r3", "Sub");
    check(one(code(Op::Mul), 0, 1) == "Mul r0, r1", "Mul");
    check(one(code(Op::Inc), 2, 0) == "Inc r2", "Inc");
    check(one(code(Op::Dec), 3, 0) == "Dec r3", "Dec");
    check(one(code(Op::Load), 1, 32) == "Load r1, [32]", "Load");
    check(one(code(Op::Store), 1, 32) == "Store r1, [32]", "Store");
    check(one(code(Op::Jmp), 0, 6) == "Jmp 6", "Jmp");
    check(one(code(Op::Jnz), 0, 6) == "Jnz r0, 6", "Jnz");
    check(one(code(Op::Halt), 0, 0) == "Halt", "Halt");
    check(one(0x77, 0, 0) == "??? 0x77", "opcode inconnu");

    {   // plusieurs instructions, avec les adresses
        Memory memory(64);
        memory.load(0, {
            code(Op::Set),  0, 5,
            code(Op::Dec),  0, 0,
            code(Op::Halt), 0, 0});
        const std::string expected =
            "0000  Set r0, 5\n"
            "0003  Dec r0\n"
            "0006  Halt\n";
        check(disassemble_range(memory, 0, 3) == expected, "disassemble_range");
    }
    {   // lecture hors de la memoire : refusee
        Memory memory(4);
        bool thrown = false;
        try {
            disassemble(memory, 3);   // il manque des octets
        } catch (const std::out_of_range&) {
            thrown = true;
        }
        check(thrown, "disassemble hors limites est refuse");
    }

    if (failures == 0) {
        std::cout << "Tous les tests du desassembleur passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
