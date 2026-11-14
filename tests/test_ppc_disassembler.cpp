#include <iostream>
#include <stdexcept>
#include <string>

#include "nerith/memory/memory.h"
#include "nerith/ppc/disassembler.h"

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

    {   // petit programme : li r3, 5 / lwz r3, 8(r1) / nop / blr
        Memory memory(64);
        memory.write32(0x10, 0x38600005);
        memory.write32(0x14, 0x80610008);
        memory.write32(0x18, 0x60000000);
        memory.write32(0x1C, 0x4E800020);
        const std::string expected =
            "00000010  38600005  li r3, 5\n"
            "00000014  80610008  lwz r3, 8(r1)\n"
            "00000018  60000000  nop\n"
            "0000001c  4e800020  blr\n";
        check(ppc::disassemble_range(memory, 0x10, 4) == expected, "disassemble_range : 4 instructions");
    }
    {   // mot inconnu
        Memory memory(8);
        memory.write32(0, 0x12345678);
        check(ppc::disassemble_range(memory, 0, 1) == "00000000  12345678  .long 0x12345678\n",
              "disassemble_range : mot inconnu");
    }
    {   // lecture hors de la memoire : refusee
        Memory memory(6);
        bool thrown = false;
        try {
            ppc::disassemble_range(memory, 4, 1);   // il manque des octets
        } catch (const std::out_of_range&) {
            thrown = true;
        }
        check(thrown, "disassemble_range hors limites est refuse");
    }

    if (failures == 0) {
        std::cout << "Tous les tests du desassembleur PowerPC passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
