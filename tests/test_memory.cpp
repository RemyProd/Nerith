#include <cstdint>
#include <iostream>
#include <stdexcept>

#include "nerith/memory.h"

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
    using nerith::Memory;

    {   // la memoire commence remplie de zeros
        Memory mem(16);
        check(mem.size() == 16, "taille de 16 octets");
        check(mem.read8(0) == 0, "octet 0 initialise a 0");
        check(mem.read8(15) == 0, "octet 15 initialise a 0");
    }
    {   // ecriture puis lecture d'un octet
        Memory mem(16);
        mem.write8(3, 0xAB);
        check(mem.read8(3) == 0xAB, "write8 puis read8");
        check(mem.read8(2) == 0, "l'octet voisin n'est pas modifie");
    }
    {   // lecture d'un mot de 32 bits en big-endian
        Memory mem(16);
        mem.write8(0, 0xAB);
        mem.write8(1, 0xCD);
        mem.write8(2, 0xEF);
        mem.write8(3, 0x01);
        check(mem.read32(0) == 0xABCDEF01, "read32 big-endian");
    }
    {   // write32 ecrit 4 octets, du poids fort au poids faible
        Memory mem(16);
        mem.write32(4, 0x12345678);
        check(mem.read8(4) == 0x12, "write32 : octet 4 vaut 0x12");
        check(mem.read8(5) == 0x34, "write32 : octet 5 vaut 0x34");
        check(mem.read8(6) == 0x56, "write32 : octet 6 vaut 0x56");
        check(mem.read8(7) == 0x78, "write32 : octet 7 vaut 0x78");
        check(mem.read32(4) == 0x12345678, "write32 puis read32");
    }
    {   // write32 qui depasse la fin : rien ne doit etre ecrit
        Memory mem(16);
        bool thrown = false;
        try {
            mem.write32(14, 0xFFFFFFFF);
        } catch (const std::out_of_range&) {
            thrown = true;
        }
        check(thrown, "write32 a l'adresse 14 est refuse");
        check(mem.read8(14) == 0, "write32 refuse : l'octet 14 reste a 0");
        check(mem.read8(15) == 0, "write32 refuse : l'octet 15 reste a 0");
    }
    {   // load copie des octets a partir d'une adresse
        Memory mem(16);
        mem.load(2, {0x11, 0x22, 0x33});
        check(mem.read8(1) == 0, "load : l'octet avant reste a 0");
        check(mem.read8(2) == 0x11, "load : octet 2");
        check(mem.read8(3) == 0x22, "load : octet 3");
        check(mem.read8(4) == 0x33, "load : octet 4");
        check(mem.read8(5) == 0, "load : l'octet apres reste a 0");
    }
    {   // load qui ne tient pas : rien n'est copie
        Memory mem(4);
        bool thrown = false;
        try {
            mem.load(2, {0x11, 0x22, 0x33});
        } catch (const std::out_of_range&) {
            thrown = true;
        }
        check(thrown, "load trop grand est refuse");
        check(mem.read8(2) == 0, "load refuse : rien n'est copie");
        check(mem.read8(3) == 0, "load refuse : rien n'est copie (suite)");
    }
    {   // adresse invalide : lecture
        Memory mem(16);
        bool thrown = false;
        try {
            mem.read8(16);
        } catch (const std::out_of_range&) {
            thrown = true;
        }
        check(thrown, "read8 a l'adresse 16 est refuse");
    }
    {   // adresse invalide : ecriture
        Memory mem(16);
        bool thrown = false;
        try {
            mem.write8(100, 1);
        } catch (const std::out_of_range&) {
            thrown = true;
        }
        check(thrown, "write8 a l'adresse 100 est refuse");
    }
    {   // read32 qui depasse la fin
        Memory mem(16);
        bool thrown = false;
        try {
            mem.read32(14);
        } catch (const std::out_of_range&) {
            thrown = true;
        }
        check(thrown, "read32 a l'adresse 14 est refuse");
    }

    if (failures == 0) {
        std::cout << "Tous les tests de Memory passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
