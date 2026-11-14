// Tests de ce que FONT les instructions de la table (optable.cpp) : logique, decalages, multiplications,
// divisions, comparaisons sans signe, rotations, acces memoire de 16 bits, indexes, avec mise a jour, inverses.
// Les valeurs attendues sont calculees a la main (le manuel PowerPC donne les formules).
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <string>
#include <utility>

#include "nerith/memory/memory.h"
#include "nerith/ppc/assembler.h"
#include "nerith/ppc/cpu.h"
#include "nerith/ppc/decoder.h"
#include "nerith/ppc/optable.h"

namespace {

using nerith::Memory;
using nerith::ppc::Cpu;
using nerith::ppc::Status;

int failures = 0;

void check(bool condition, const std::string& name) {
    if (!condition) {
        std::cerr << "ECHEC : " << name << "\n";
        ++failures;
    }
}

void check_value(std::uint64_t actual, std::uint64_t expected, const std::string& name) {
    if (actual != expected) {
        std::cerr << "ECHEC : " << name << " : obtenu 0x" << std::hex << actual << " au lieu de 0x" << expected
                  << std::dec << "\n";
        ++failures;
    }
}

using Regs = std::initializer_list<std::pair<int, std::uint64_t>>;

// Execute UNE instruction (a l'adresse 0) apres avoir donne leur valeur a quelques registres.
// La memoire fait 256 octets ; "memory" peut etre preparee a l'avance (l'instruction est ecrite en 0).
Cpu exec(const std::string& line, Regs regs, Memory& memory, Status* status = nullptr) {
    memory.write32(0, nerith::ppc::assemble(line).at(0));
    Cpu cpu;
    for (const auto& [number, value] : regs) {
        cpu.gpr[static_cast<std::size_t>(number)] = value;
    }
    const Status result = nerith::ppc::step(cpu, memory);
    if (status != nullptr) {
        *status = result;
    } else {
        check(result == Status::Running, line + " : doit s'executer");
    }
    return cpu;
}

Cpu exec(const std::string& line, Regs regs) {
    Memory memory(256);
    return exec(line, regs, memory);
}

// Pareil, avec une valeur de depart pour XER (par exemple le bit CA deja pose).
Cpu execx(const std::string& line, Regs regs, std::uint32_t xer) {
    Memory memory(256);
    memory.write32(0, nerith::ppc::assemble(line).at(0));
    Cpu cpu;
    cpu.xer = xer;
    for (const auto& [number, value] : regs) {
        cpu.gpr[static_cast<std::size_t>(number)] = value;
    }
    check(nerith::ppc::step(cpu, memory) == Status::Running, line + " : doit s'executer");
    return cpu;
}

// Execute plusieurs instructions (le programme se termine par blr).
Cpu run_program(const std::string& source, Regs regs) {
    Memory memory(256);
    const auto words = nerith::ppc::assemble(source + "\nblr\n");
    for (std::size_t n = 0; n < words.size(); n++) {
        memory.write32(n * 4, words[n]);
    }
    Cpu cpu;
    for (const auto& [number, value] : regs) {
        cpu.gpr[static_cast<std::size_t>(number)] = value;
    }
    check(nerith::ppc::run(cpu, memory, 1000) == Status::Halted, "programme : doit se terminer");
    return cpu;
}

constexpr std::uint32_t kCa = 0x20000000;   // bit CA (retenue) de XER

}  // namespace

int main() {
    using namespace nerith::ppc;

    // --- logique ---
    {
        const Regs in = {{4, 0xF0F0F0F0F0F0F0F0ull}, {5, 0x00FF00FF00FF00FFull}};
        check_value(exec("and r3, r4, r5", in).gpr[3], 0x00F000F000F000F0ull, "and");
        check_value(exec("andc r3, r4, r5", in).gpr[3], 0xF000F000F000F000ull, "andc");
        check_value(exec("xor r3, r4, r5", in).gpr[3], 0xF00FF00FF00FF00Full, "xor");
        check_value(exec("nor r3, r4, r5", in).gpr[3], 0x0F000F000F000F00ull, "nor");
        check_value(exec("nand r3, r4, r5", in).gpr[3], 0xFF0FFF0FFF0FFF0Full, "nand");
        check_value(exec("orc r3, r4, r5", in).gpr[3], 0xFFF0FFF0FFF0FFF0ull, "orc");
        check_value(exec("eqv r3, r4, r5", in).gpr[3], 0x0FF00FF00FF00FF0ull, "eqv");
        check_value(exec("not r3, r4", in).gpr[3], 0x0F0F0F0F0F0F0F0Full, "not");
        check_value(exec("neg r3, r4", {{4, 5}}).gpr[3], 0xFFFFFFFFFFFFFFFBull, "neg 5");
        check_value(exec("neg r3, r4", {{4, 0}}).gpr[3], 0, "neg 0");
        // la destination est rA : rS ne change pas
        check_value(exec("and r3, r4, r5", in).gpr[4], 0xF0F0F0F0F0F0F0F0ull, "and : rS inchange");
    }

    // --- decalages ---
    {
        check_value(exec("slw r3, r4, r5", {{4, 0x180000001ull}, {5, 1}}).gpr[3], 0x2, "slw 1 : seuls les 32 bits bas comptent");
        check_value(exec("slw r3, r4, r5", {{4, 0x80000001ull}, {5, 31}}).gpr[3], 0x80000000ull, "slw 31");
        check_value(exec("slw r3, r4, r5", {{4, 0xFFFFFFFFull}, {5, 32}}).gpr[3], 0, "slw 32 : tout est perdu");
        check_value(exec("srw r3, r4, r5", {{4, 0xFFFFFFFF80000000ull}, {5, 4}}).gpr[3], 0x08000000, "srw 4");
        check_value(exec("srw r3, r4, r5", {{4, 0x80000000ull}, {5, 35}}).gpr[3], 0, "srw 35 (> 31)");
        check_value(exec("sld r3, r4, r5", {{4, 1}, {5, 63}}).gpr[3], 0x8000000000000000ull, "sld 63");
        check_value(exec("sld r3, r4, r5", {{4, 1}, {5, 64}}).gpr[3], 0, "sld 64");
        check_value(exec("srd r3, r4, r5", {{4, 0x8000000000000000ull}, {5, 63}}).gpr[3], 1, "srd 63");
        check_value(exec("srd r3, r4, r5", {{4, 0x8000000000000000ull}, {5, 65}}).gpr[3], 0, "srd 65");
        check_value(exec("cntlzw r3, r4", {{4, 0x00010000}}).gpr[3], 15, "cntlzw");
        check_value(exec("cntlzw r3, r4", {{4, 0}}).gpr[3], 32, "cntlzw 0");
        check_value(exec("cntlzw r3, r4", {{4, 0xFFFFFFFF00000001ull}}).gpr[3], 31, "cntlzw : seuls les 32 bits bas");
        check_value(exec("cntlzd r3, r4", {{4, 1}}).gpr[3], 63, "cntlzd 1");
        check_value(exec("cntlzd r3, r4", {{4, 0}}).gpr[3], 64, "cntlzd 0");
        check_value(exec("extsb r3, r4", {{4, 0x80}}).gpr[3], 0xFFFFFFFFFFFFFF80ull, "extsb negatif");
        check_value(exec("extsb r3, r4", {{4, 0x12345680}}).gpr[3], 0xFFFFFFFFFFFFFF80ull, "extsb : un seul octet compte");
        check_value(exec("extsb r3, r4", {{4, 0x7F}}).gpr[3], 0x7F, "extsb positif");
        check_value(exec("extsh r3, r4", {{4, 0x8000}}).gpr[3], 0xFFFFFFFFFFFF8000ull, "extsh");
        check_value(exec("extsw r3, r4", {{4, 0x80000000ull}}).gpr[3], 0xFFFFFFFF80000000ull, "extsw negatif");
        check_value(exec("extsw r3, r4", {{4, 0x17FFFFFFFull}}).gpr[3], 0x7FFFFFFF, "extsw positif");
    }

    // --- multiplications et divisions ---
    {
        check_value(exec("mulli r3, r4, -5", {{4, 7}}).gpr[3], 0xFFFFFFFFFFFFFFDDull, "mulli");
        check_value(exec("mulld r3, r4, r5", {{4, 3}, {5, 0xFFFFFFFFFFFFFFFCull}}).gpr[3], 0xFFFFFFFFFFFFFFF4ull, "mulld 3 * -4");
        check_value(exec("mulld r3, r4, r5", {{4, 0x100000000ull}, {5, 0x100000000ull}}).gpr[3], 0, "mulld : 2^64 boucle sur 0");
        check_value(exec("mulhw r3, r4, r5", {{4, 0x7FFFFFFF}, {5, 0x7FFFFFFF}}).gpr[3], 0x3FFFFFFF, "mulhw");
        check_value(exec("mulhw r3, r4, r5", {{4, 0xFFFFFFFF}, {5, 2}}).gpr[3], 0xFFFFFFFFFFFFFFFFull, "mulhw : -1 * 2");
        check_value(exec("mulhwu r3, r4, r5", {{4, 0xFFFFFFFF}, {5, 2}}).gpr[3], 1, "mulhwu");
        check_value(exec("divw r3, r4, r5", {{4, 7}, {5, 2}}).gpr[3], 3, "divw 7 / 2");
        check_value(exec("divw r3, r4, r5", {{4, 0x1234FFFFFFF9ull}, {5, 2}}).gpr[3], 0xFFFFFFFFFFFFFFFDull, "divw -7 / 2 (32 bits bas)");
        check_value(exec("divw r3, r4, r5", {{4, 7}, {5, 0}}).gpr[3], 0, "divw par 0 : Nerith met 0");
        check_value(exec("divw r3, r4, r5", {{4, 0x80000000ull}, {5, 0xFFFFFFFFull}}).gpr[3], 0, "divw INT_MIN / -1 : Nerith met 0");
        check_value(exec("divwu r3, r4, r5", {{4, 0xFFFFFFFEull}, {5, 2}}).gpr[3], 0x7FFFFFFF, "divwu");
        check_value(exec("divwu r3, r4, r5", {{4, 5}, {5, 0}}).gpr[3], 0, "divwu par 0");
        check_value(exec("divd r3, r4, r5", {{4, 0xFFFFFFFFFFFFFFF9ull}, {5, 2}}).gpr[3], 0xFFFFFFFFFFFFFFFDull, "divd -7 / 2");
        check_value(exec("divd r3, r4, r5", {{4, 0x8000000000000000ull}, {5, 0xFFFFFFFFFFFFFFFFull}}).gpr[3], 0, "divd INT64_MIN / -1");
        check_value(exec("divdu r3, r4, r5", {{4, 0xFFFFFFFFFFFFFFFEull}, {5, 2}}).gpr[3], 0x7FFFFFFFFFFFFFFFull, "divdu");
    }

    // --- nombres de 16 bits ---
    {
        check_value(exec("oris r3, r4, 0x1234", {{4, 0xF}}).gpr[3], 0x1234000F, "oris");
        check_value(exec("xori r3, r4, 0x0F", {{4, 0xFF}}).gpr[3], 0xF0, "xori");
        check_value(exec("xoris r3, r4, 0xFFFF", {{4, 0}}).gpr[3], 0xFFFF0000ull, "xoris");
        {
            const Cpu cpu = exec("andi. r3, r4, 0xFF", {{4, 0xFFFF1234ull}});
            check_value(cpu.gpr[3], 0x34, "andi. : resultat");
            check_value(cpu.cr, 0x40000000, "andi. : cr0 = GT");
        }
        {
            Memory memory(256);
            memory.write32(0, 0);
            const Cpu cpu = exec("andi. r3, r4, 0xFF", {{4, 0x100}}, memory);
            check_value(cpu.gpr[3], 0, "andi. zero");
            check_value(cpu.cr, 0x20000000, "andi. : cr0 = EQ");
        }
        {   // les autres champs de CR ne bougent pas
            Memory memory(256);
            memory.write32(0, nerith::ppc::assemble("andi. r3, r4, 1").at(0));
            Cpu cpu;
            cpu.cr = 0x0FFFFFFF;
            cpu.gpr[4] = 1;
            nerith::ppc::step(cpu, memory);
            check_value(cpu.cr, 0x4FFFFFFF, "andi. : seul cr0 change");
        }
        {
            const Cpu cpu = exec("andis. r3, r4, 0xFF", {{4, 0xFFFF1234ull}});
            check_value(cpu.gpr[3], 0x00FF0000, "andis. : resultat");
            check_value(cpu.cr, 0x40000000, "andis. : cr0 = GT");
        }
    }

    // --- comparaisons sans signe ---
    {
        check_value(exec("cmplwi r3, 5", {{3, 0xFFFFFFFFull}}).cr, 0x40000000, "cmplwi : 0xFFFFFFFF > 5 sans signe");
        check_value(exec("cmpwi r3, 5", {{3, 0xFFFFFFFFull}}).cr, 0x80000000, "cmpwi : -1 < 5 avec signe (pour comparer)");
        check_value(exec("cmplwi cr1, r3, 5", {{3, 5}}).cr, 0x02000000, "cmplwi cr1 : EQ");
        check_value(exec("cmpldi r3, 5", {{3, 0x100000000ull}}).cr, 0x40000000, "cmpldi : 2^32 > 5");
        check_value(exec("cmpldi r3, 5", {{3, 0}}).cr, 0x80000000, "cmpldi : 0 < 5");
        check_value(exec("cmplw r3, r4", {{3, 0x100000001ull}, {4, 2}}).cr, 0x80000000, "cmplw : 32 bits bas, 1 < 2");
        check_value(exec("cmpld r3, r4", {{3, 0x100000001ull}, {4, 2}}).cr, 0x40000000, "cmpld : 64 bits, plus grand");
        check_value(exec("cmpld r3, r4", {{3, 0xFFFFFFFFFFFFFFFFull}, {4, 0}}).cr, 0x40000000, "cmpld : sans signe");
        check_value(exec("cmpd r3, r4", {{3, 0xFFFFFFFFFFFFFFFFull}, {4, 0}}).cr, 0x80000000, "cmpd : avec signe (pour comparer)");
    }

    // --- rotations ---
    {
        check_value(exec("rlwimi r3, r4, 8, 24, 31", {{3, 0xDEADBEEF11223344ull}, {4, 0xAB000000ull}}).gpr[3],
                    0xDEADBEEF112233ABull, "rlwimi : insere un octet, le haut de rA ne bouge pas");
        check_value(exec("rlwnm r3, r4, r5, 0, 31", {{4, 0x80000001ull}, {5, 1}}).gpr[3], 3, "rlwnm : rotation de 1");
        check_value(exec("rlwnm r3, r4, r5, 0, 31", {{4, 0x80000001ull}, {5, 33}}).gpr[3], 3, "rlwnm : 5 bits de rB seulement");
        check_value(exec("rlwnm r3, r4, r5, 8, 15", {{4, 0x12345678ull}, {5, 8}}).gpr[3], 0x00560000, "rlwnm avec masque");
        const std::uint64_t value = 0x1234567890ABCDEFull;
        check_value(exec("clrldi r3, r4, 32", {{4, value}}).gpr[3], 0x90ABCDEFull, "clrldi 32");
        check_value(exec("srdi r3, r4, 8", {{4, value}}).gpr[3], 0x001234567890ABCDull, "srdi 8");
        check_value(exec("rotldi r3, r4, 4", {{4, value}}).gpr[3], 0x234567890ABCDEF1ull, "rotldi 4");
        check_value(exec("sldi r3, r4, 8", {{4, value}}).gpr[3], 0x34567890ABCDEF00ull, "sldi 8");
        check_value(exec("clrrdi r3, r4, 16", {{4, value}}).gpr[3], 0x1234567890AB0000ull, "clrrdi 16");
        check_value(exec("rldic r3, r4, 8, 16", {{4, value}}).gpr[3], 0x00007890ABCDEF00ull, "rldic");
        check_value(exec("rldimi r3, r4, 8, 16", {{3, 0xFFFFFFFFFFFFFFFFull}, {4, value}}).gpr[3], 0xFFFF7890ABCDEFFFull, "rldimi");
        check_value(exec("sldi r3, r4, 0", {{4, value}}).gpr[3], value, "sldi 0 : rien ne change");
    }

    // --- memoire : 16 bits ---
    {
        Memory memory(256);
        memory.write32(0x20, 0x80017FFF);
        check_value(exec("lhz r3, 32(r0)", {}, memory).gpr[3], 0x8001, "lhz");
        check_value(exec("lhz r3, 34(r0)", {}, memory).gpr[3], 0x7FFF, "lhz 2");
        check_value(exec("lha r3, 32(r0)", {}, memory).gpr[3], 0xFFFFFFFFFFFF8001ull, "lha negatif");
        check_value(exec("lha r3, 34(r0)", {}, memory).gpr[3], 0x7FFF, "lha positif");
        memory.write32(0x30, 0x11223344);
        exec("sth r3, 48(r0)", {{3, 0x1234ABCDull}}, memory);
        check_value(memory.read32(0x30), 0xABCD3344, "sth : 2 octets seulement, big-endian");
    }

    // --- memoire : avec mise a jour (u) ---
    {
        Memory memory(256);
        memory.write32(0x28, 0xCAFEBABE);
        const Cpu cpu = exec("lwzu r3, 8(r4)", {{4, 0x20}}, memory);
        check_value(cpu.gpr[3], 0xCAFEBABE, "lwzu : valeur");
        check_value(cpu.gpr[4], 0x28, "lwzu : rA = adresse");
        Status status = Status::Running;
        const Cpu same = exec("lwzu r4, 8(r4)", {{4, 0x20}}, memory, &status);
        check(status == Status::InvalidInstruction && same.gpr[4] == 0x20, "lwzu avec rA = rD : invalide");
        exec("lwzu r3, 8(r0)", {}, memory, &status);
        check(status == Status::InvalidInstruction, "lwzu avec rA = 0 : invalide");
        const Cpu st = exec("stbu r3, 1(r4)", {{3, 0xAA}, {4, 0x40}}, memory);
        check(memory.read8(0x41) == 0xAA && st.gpr[4] == 0x41, "stbu");
        const Cpu prologue = exec("stdu r1, -16(r1)", {{1, 0x80}}, memory);
        check(memory.read32(0x70) == 0 && memory.read32(0x74) == 0x80 && prologue.gpr[1] == 0x70, "stdu : cree une pile de 64 bits");
        memory.write32(0x28, 0x01020304);
        memory.write32(0x2C, 0x05060708);
        const Cpu load = exec("ldu r3, 8(r4)", {{4, 0x20}}, memory);
        check_value(load.gpr[3], 0x0102030405060708ull, "ldu : valeur");
        check_value(load.gpr[4], 0x28, "ldu : rA");
        memory.write32(0x20, 0x80000000);
        check_value(exec("lwa r3, 32(r0)", {}, memory).gpr[3], 0xFFFFFFFF80000000ull, "lwa : signe etendu");
    }

    // --- memoire : indexee (x) ---
    {
        Memory memory(256);
        memory.write32(0x20, 0x11223344);
        memory.write32(0x24, 0x55667788);
        check_value(exec("lhzx r3, r4, r5", {{4, 0x10}, {5, 0x10}}, memory).gpr[3], 0x1122, "lhzx");
        check_value(exec("lhax r3, r4, r5", {{4, 0x10}, {5, 0x12}}, memory).gpr[3], 0x3344, "lhax positif");
        memory.write32(0x30, 0x80000000);
        check_value(exec("lhax r3, r4, r5", {{4, 0x20}, {5, 0x10}}, memory).gpr[3], 0xFFFFFFFFFFFF8000ull, "lhax negatif");
        check_value(exec("lwax r3, 0, r5", {{5, 0x30}}, memory).gpr[3], 0xFFFFFFFF80000000ull, "lwax : rA = 0 veut dire 0");
        check_value(exec("ldx r3, r4, r5", {{4, 0x10}, {5, 0x10}}, memory).gpr[3], 0x1122334455667788ull, "ldx");
        const Cpu update = exec("lwzux r3, r4, r5", {{4, 0x10}, {5, 0x10}}, memory);
        check_value(update.gpr[3], 0x11223344, "lwzux : valeur");
        check_value(update.gpr[4], 0x20, "lwzux : rA");
        exec("stdx r3, r4, r5", {{3, 0xA1A2A3A4A5A6A7A8ull}, {4, 0x40}, {5, 0x08}}, memory);
        check(memory.read32(0x48) == 0xA1A2A3A4 && memory.read32(0x4C) == 0xA5A6A7A8, "stdx");
        exec("sthx r3, r4, r5", {{3, 0xBEEF}, {4, 0x50}, {5, 0x01}}, memory);
        check(memory.read8(0x51) == 0xBE && memory.read8(0x52) == 0xEF, "sthx : adresse impaire permise");
        const Cpu stux = exec("stwux r3, r4, r5", {{3, 0x01020304}, {4, 0x60}, {5, 0x04}}, memory);
        check(memory.read32(0x64) == 0x01020304 && stux.gpr[4] == 0x64, "stwux");
    }

    // --- memoire : octets inverses (br) ---
    {
        Memory memory(256);
        memory.write32(0x20, 0x11223344);
        memory.write32(0x24, 0x55667788);
        check_value(exec("lwbrx r3, 0, r5", {{5, 0x20}}, memory).gpr[3], 0x44332211, "lwbrx");
        check_value(exec("lhbrx r3, 0, r5", {{5, 0x20}}, memory).gpr[3], 0x2211, "lhbrx");
        check_value(exec("ldbrx r3, 0, r5", {{5, 0x20}}, memory).gpr[3], 0x8877665544332211ull, "ldbrx");
        exec("stwbrx r3, 0, r5", {{3, 0x11223344}, {5, 0x30}}, memory);
        check_value(memory.read32(0x30), 0x44332211, "stwbrx");
        exec("sthbrx r3, 0, r5", {{3, 0x1122}, {5, 0x38}}, memory);
        check(memory.read8(0x38) == 0x22 && memory.read8(0x39) == 0x11, "sthbrx");
        exec("stdbrx r3, 0, r5", {{3, 0x1122334455667788ull}, {5, 0x40}}, memory);
        check(memory.read32(0x40) == 0x88776655 && memory.read32(0x44) == 0x44332211, "stdbrx");
    }

    // --- acces qui sortent de la memoire : rien ne change ---
    {
        Memory memory(256);
        Status status = Status::Running;
        const Cpu load = exec("lhz r3, 255(r0)", {{3, 7}}, memory, &status);
        check(status == Status::OutOfBounds && load.gpr[3] == 7, "lhz a cheval sur la fin : OutOfBounds");
        memory.write8(255, 0x5A);
        exec("sth r3, 255(r0)", {{3, 0x1234}}, memory, &status);
        check(status == Status::OutOfBounds && memory.read8(255) == 0x5A, "sth a cheval : aucun octet ecrit");
        exec("stdx r3, r4, r5", {{3, 1}, {4, 200}, {5, 52}}, memory, &status);   // 252 a 259
        check(status == Status::OutOfBounds && memory.read8(252) == 0 && memory.read8(255) == 0x5A, "stdx a cheval : rien ecrit");
        const Cpu update = exec("lwzu r3, 8(r4)", {{4, 0x1000}}, memory, &status);
        check(status == Status::OutOfBounds && update.gpr[4] == 0x1000 && update.gpr[3] == 0, "lwzu hors memoire : rA inchange");
    }

    // --- chaque ligne de la table a une semantique : aucune ne doit sortir en InvalidInstruction ---
    {
        for (std::size_t n = 0; n < nerith::ppc::op_table_size(); n++) {
            const OpInfo& op = nerith::ppc::op_table()[n];
            // un mot valide pour chaque disposition : rD = 3, rA = 4, rB = 5 (les champs reserves restent a 0)
            std::uint32_t word = op.opcode << 26;
            const std::uint32_t d = 3u << 21;
            const std::uint32_t a = 4u << 16;
            const std::uint32_t b = 5u << 11;
            switch (op.layout) {
                case Layout::RtRaRb:
                case Layout::RaRsRb:
                case Layout::RtRbRa:
                case Layout::MemX:
                case Layout::CrBits:
                case Layout::FMemX:
                case Layout::FA3:
                    word |= d | a | b;
                    break;
                case Layout::RaRsSh:
                    word |= d | a | (3u << 11);
                    break;
                case Layout::RtRaSimm:
                case Layout::RaRsUimm:
                case Layout::MemD:
                case Layout::MemDS:
                case Layout::FMemD:
                    word |= d | a | 8u;
                    break;
                case Layout::RaRs:
                case Layout::RtRa:
                    word |= d | a;
                    break;
                case Layout::FA3c:
                    word |= d | a | (5u << 6);
                    break;
                case Layout::FA4:
                    word |= d | a | b | (6u << 6);
                    break;
                case Layout::FA2:
                case Layout::FX2:
                    word |= d | b;
                    break;
                case Layout::FCmp:
                    word |= (1u << 23) | a | b;
                    break;
                case Layout::FrD:
                    word |= d;
                    break;
                case Layout::BtOnly:
                    word |= 5u << 21;
                    break;
                case Layout::FmFrb:
                    word |= (0x0Fu << 17) | b;
                    break;
                case Layout::CrfImm:
                    word |= (1u << 23) | (5u << 12);
                    break;
                case Layout::CrfCrf:
                    word |= (1u << 23) | (2u << 18);
                    break;
            }
            if (op.opcode == 31 || op.opcode == 19 || op.opcode == 59 || op.opcode == 63) {
                word |= static_cast<std::uint32_t>(op.xo) << 1;
            } else if (op.opcode == 58 || op.opcode == 62) {
                word |= static_cast<std::uint32_t>(op.xo);
            }
            check(nerith::ppc::decode(word).kind == op.kind, std::string("table : decodage de ") + op.name);
            Memory memory(256);
            memory.write32(0, word);
            Cpu cpu;
            cpu.gpr[4] = 0x40;
            cpu.gpr[5] = 0x08;
            check(nerith::ppc::step(cpu, memory) == Status::Running, std::string("table : execution de ") + op.name);
        }
    }

    // --- retenue (CA) ---
    {
        const std::uint64_t all = 0xFFFFFFFFFFFFFFFFull;
        Cpu c = exec("addic r3, r4, 1", {{4, all}});
        check(c.gpr[3] == 0 && c.xer == kCa, "addic : -1 + 1 = 0 avec retenue");
        c = exec("addic r3, r4, 1", {{4, 5}});
        check(c.gpr[3] == 6 && c.xer == 0, "addic : 5 + 1, pas de retenue");
        c = execx("addic r3, r4, 1", {{4, 5}}, kCa);
        check(c.gpr[3] == 6 && c.xer == 0, "addic : la retenue est reecrite (effacee ici)");
        c = exec("addic r3, r4, -1", {{4, 1}});
        check(c.gpr[3] == 0 && c.xer == kCa, "addic : 1 + (-1) = 0 avec retenue");
        c = exec("addic r3, r4, -1", {{4, 0}});
        check(c.gpr[3] == all && c.xer == 0, "addic : 0 + (-1), pas de retenue");
        c = exec("addic r3, r0, 5", {{0, 7}});
        check(c.gpr[3] == 12, "addic : r0 est un vrai registre (pas le nombre 0)");
        c = exec("addic. r3, r4, -1", {{4, 1}});
        check(c.gpr[3] == 0 && c.xer == kCa && c.cr == 0x20000000, "addic. : zero (EQ) avec retenue");
        c = exec("addic. r3, r4, -1", {{4, 0}});
        check(c.gpr[3] == all && c.xer == 0 && c.cr == 0x80000000, "addic. : negatif (LT)");
        c = exec("subfic r3, r4, 10", {{4, 3}});
        check(c.gpr[3] == 7 && c.xer == kCa, "subfic : 10 - 3, pas d'emprunt donc CA = 1");
        c = exec("subfic r3, r4, 10", {{4, 11}});
        check(c.gpr[3] == all && c.xer == 0, "subfic : 10 - 11 = -1, emprunt donc CA = 0");

        c = exec("addc r3, r4, r5", {{4, all}, {5, 1}});
        check(c.gpr[3] == 0 && c.xer == kCa, "addc : retenue");
        c = exec("addc r3, r4, r5", {{4, 1}, {5, 2}});
        check(c.gpr[3] == 3 && c.xer == 0, "addc : pas de retenue");
        c = execx("adde r3, r4, r5", {{4, 1}, {5, 2}}, kCa);
        check(c.gpr[3] == 4 && c.xer == 0, "adde : 1 + 2 + CA");
        c = execx("adde r3, r4, r5", {{4, all}, {5, 0}}, kCa);
        check(c.gpr[3] == 0 && c.xer == kCa, "adde : retenue venue de la retenue entrante");
        c = run_program("addc r3, r4, r6\n adde r5, r7, r8", {{4, all}, {6, 1}, {7, 5}, {8, 7}});
        check(c.gpr[3] == 0 && c.gpr[5] == 13, "addition sur 128 bits : addc puis adde");

        c = exec("subc r3, r4, r5", {{4, 10}, {5, 3}});     // r3 = r4 - r5
        check(c.gpr[3] == 7 && c.xer == kCa, "subc : 10 - 3, CA = 1");
        c = exec("subc r3, r4, r5", {{4, 3}, {5, 10}});
        check(c.gpr[3] == 0xFFFFFFFFFFFFFFF9ull && c.xer == 0, "subc : 3 - 10 = -7, CA = 0");
        c = exec("subc r3, r4, r5", {{4, 6}, {5, 6}});
        check(c.gpr[3] == 0 && c.xer == kCa, "subc : egaux, CA = 1");
        c = execx("subfe r3, r4, r5", {{4, 3}, {5, 10}}, kCa);   // r3 = r5 - r4 - (1 - CA)
        check(c.gpr[3] == 7 && c.xer == kCa, "subfe avec CA = 1 : 10 - 3");
        c = execx("subfe r3, r4, r5", {{4, 3}, {5, 10}}, 0);
        check(c.gpr[3] == 6 && c.xer == kCa, "subfe avec CA = 0 : 10 - 3 - 1");
        c = execx("addze r3, r4", {{4, all}}, kCa);
        check(c.gpr[3] == 0 && c.xer == kCa, "addze : -1 + CA");
        c = execx("addze r3, r4", {{4, 9}}, 0);
        check(c.gpr[3] == 9 && c.xer == 0, "addze sans retenue");
        c = execx("addme r3, r4", {{4, 5}}, kCa);
        check(c.gpr[3] == 5 && c.xer == kCa, "addme : 5 - 1 + 1");
        c = execx("addme r3, r4", {{4, 0}}, 0);
        check(c.gpr[3] == all && c.xer == 0, "addme : 0 - 1 + 0");
        c = execx("subfze r3, r4", {{4, 5}}, 0);
        check(c.gpr[3] == 0xFFFFFFFFFFFFFFFAull && c.xer == 0, "subfze : ~5 + 0");
        c = execx("subfze r3, r4", {{4, 0}}, kCa);
        check(c.gpr[3] == 0 && c.xer == kCa, "subfze : ~0 + 1 = 0 avec retenue");
        c = execx("subfme r3, r4", {{4, 0}}, kCa);
        check(c.gpr[3] == all && c.xer == kCa, "subfme : ~0 - 1 + 1");

        c = exec("sraw r3, r4, r5", {{4, 0x80000000ull}, {5, 4}});
        check(c.gpr[3] == 0xFFFFFFFFF8000000ull && c.xer == 0, "sraw : negatif, que des 0 perdus, CA = 0");
        c = exec("sraw r3, r4, r5", {{4, 0xFFFFFFF1ull}, {5, 2}});
        check(c.gpr[3] == 0xFFFFFFFFFFFFFFFCull && c.xer == kCa, "sraw : -15 >> 2 = -4, un bit 1 perdu, CA = 1");
        c = exec("sraw r3, r4, r5", {{4, 0x7FFFFFFFull}, {5, 4}});
        check(c.gpr[3] == 0x07FFFFFF && c.xer == 0, "sraw : positif");
        c = exec("sraw r3, r4, r5", {{4, 0x80000000ull}, {5, 32}});
        check(c.gpr[3] == all && c.xer == kCa, "sraw 32 : tout est parti, resultat -1, CA = 1");
        c = exec("sraw r3, r4, r5", {{4, 0x40000000ull}, {5, 40}});
        check(c.gpr[3] == 0 && c.xer == 0, "sraw 40 positif : 0");
        c = exec("sraw r3, r4, r5", {{4, 0x80000000ull}, {5, 0}});
        check(c.gpr[3] == 0xFFFFFFFF80000000ull && c.xer == 0, "sraw 0 : etend le signe sur 64 bits");
        c = exec("srawi r3, r4, 1", {{4, 0xFFFFFFFFull}});
        check(c.gpr[3] == all && c.xer == kCa, "srawi : -1 >> 1, CA = 1");
        c = exec("srawi r3, r4, 1", {{4, 0xFFFFFFFEull}});
        check(c.gpr[3] == all && c.xer == 0, "srawi : -2 >> 1, CA = 0");
        // division par 4 en arrondissant vers zero : srawi puis addze (idiome des compilateurs)
        c = run_program("srawi r3, r4, 2\n addze r3, r3", {{4, 0xFFFFFFF9ull}});   // -7 / 4
        check(c.gpr[3] == all, "srawi + addze : -7 / 4 = -1");
        c = run_program("srawi r3, r4, 2\n addze r3, r3", {{4, 7}});
        check(c.gpr[3] == 1, "srawi + addze : 7 / 4 = 1");
        c = run_program("srawi r3, r4, 2\n addze r3, r3", {{4, 0xFFFFFFF8ull}});   // -8 / 4
        check(c.gpr[3] == 0xFFFFFFFFFFFFFFFEull, "srawi + addze : -8 / 4 = -2");
        c = exec("srad r3, r4, r5", {{4, 0x8000000000000000ull}, {5, 63}});
        check(c.gpr[3] == all && c.xer == 0, "srad 63 de INT64_MIN : -1, aucun bit 1 perdu");
        c = exec("srad r3, r4, r5", {{4, all}, {5, 63}});
        check(c.gpr[3] == all && c.xer == kCa, "srad 63 de -1 : CA = 1");
        c = exec("srad r3, r4, r5", {{4, all}, {5, 64}});
        check(c.gpr[3] == all && c.xer == kCa, "srad 64 : tout parti");
        c = exec("srad r3, r4, r5", {{4, 5}, {5, 100}});
        check(c.gpr[3] == 0 && c.xer == 0, "srad 100 positif : 0");
        c = exec("sradi r3, r4, 32", {{4, 0x8000000100000000ull}});
        check(c.gpr[3] == 0xFFFFFFFF80000001ull && c.xer == 0, "sradi 32");
        c = exec("sradi r3, r4, 4", {{4, 0xFFFFFFFFFFFFFF00ull}});
        check(c.gpr[3] == 0xFFFFFFFFFFFFFFF0ull && c.xer == 0, "sradi 4 : -256 >> 4");
        c = exec("sradi r3, r4, 4", {{4, 0xFFFFFFFFFFFFFF01ull}});
        check(c.gpr[3] == 0xFFFFFFFFFFFFFFF0ull && c.xer == kCa, "sradi 4 : -255 >> 4 = -16, CA = 1");
        c = exec("sradi r3, r4, 63", {{4, 0x7FFFFFFFFFFFFFFFull}});
        check(c.gpr[3] == 0 && c.xer == 0, "sradi 63 positif");

        c = exec("mtxer r3", {{3, all}});
        check(c.xer == 0xE000007F, "mtxer : seuls SO, OV, CA et le compteur sont gardes");
        c = execx("mfxer r4", {}, 0xE000007F);
        check(c.gpr[4] == 0xE000007F, "mfxer");
        c = execx("cmpwi r3, 0", {{3, 0}}, 0x80000000);
        check(c.cr == 0x30000000, "cmpwi : le bit SO de XER est recopie dans le champ de CR");
    }

    // --- un vrai programme : CRC32 (zip, png...). Les valeurs attendues sont les valeurs connues du CRC32 ---
    {
        const std::string program =
            "lis r6, 0xEDB8\n ori r6, r6, 0x8320\n clrldi r6, r6, 32\n"   // polynome 0xEDB88320 sur 32 bits
            "li r3, -1\n clrldi r3, r3, 32\n mtctr r5\n"                  // crc = 0xFFFFFFFF, une tour par octet
            "octet:\n lbz r7, 0(r4)\n addi r4, r4, 1\n xor r3, r3, r7\n li r8, 8\n"
            "bit:\n andi. r9, r3, 1\n srwi r3, r3, 1\n beq suite\n xor r3, r3, r6\n"
            "suite:\n addi r8, r8, -1\n cmpwi r8, 0\n bne bit\n bdnz octet\n"
            "not r3, r3\n clrldi r3, r3, 32\n blr\n";
        const std::pair<std::string, std::uint64_t> cases[] = {{"abc", 0x352441C2}, {"123456789", 0xCBF43926}};
        for (const auto& [text, expected] : cases) {
            Memory memory(1024);
            const auto words = nerith::ppc::assemble(program);
            for (std::size_t n = 0; n < words.size(); n++) {
                memory.write32(n * 4, words[n]);
            }
            for (std::size_t n = 0; n < text.size(); n++) {
                memory.write8(0x200 + n, static_cast<std::uint8_t>(text[n]));
            }
            Cpu cpu;
            cpu.gpr[4] = 0x200;
            cpu.gpr[5] = text.size();
            check(nerith::ppc::run(cpu, memory, 100000) == Status::Halted, "crc32 : le programme se termine");
            check_value(cpu.gpr[3], expected, "crc32(\"" + text + "\")");
        }
    }

    if (failures == 0) {
        std::cout << "Tous les tests des instructions de la table passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
