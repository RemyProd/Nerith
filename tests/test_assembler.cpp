#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "nerith/assembler.h"
#include "nerith/cpu.h"
#include "nerith/disassembler.h"
#include "nerith/memory.h"

namespace {

int failures = 0;

void check(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "ECHEC : " << name << "\n";
        ++failures;
    }
}

using Bytes = std::vector<std::uint8_t>;

// Verifie que ce programme est refuse, avec une erreur signalee a la bonne ligne.
void check_error(const std::string& source, std::size_t expected_line, const char* name) {
    try {
        nerith::assemble(source);
        std::cerr << "ECHEC : " << name << " (aucune erreur)\n";
        ++failures;
    } catch (const nerith::AssemblyError& error) {
        check(error.line() == expected_line, name);
    }
}

}  // namespace

int main() {
    using namespace nerith;

    {   // une instruction de chaque forme
        const Bytes expected = {
            code(Op::Set),   0, 5,
            code(Op::Add),   1, 0,
            code(Op::Sub),   1, 2,
            code(Op::Mul),   3, 1,
            code(Op::Inc),   2, 0,
            code(Op::Dec),   2, 0,
            code(Op::Load),  1, 20,
            code(Op::Store), 1, 21,
            code(Op::Jmp),   0, 3,
            code(Op::Jnz),   0, 6,
            code(Op::Halt),  0, 0};
        const Bytes bytes = assemble(
            "Set r0, 5\n"
            "Add r1, r0\n"
            "Sub r1, r2\n"
            "Mul r3, r1\n"
            "Inc r2\n"
            "Dec r2\n"
            "Load r1, [20]\n"
            "Store r1, 21\n"
            "Jmp 3\n"
            "Jnz r0, 6\n"
            "Halt\n");
        check(bytes == expected, "chaque instruction donne les bons octets");
    }
    {   // commentaires, lignes vides, espaces, majuscules, hexadecimal
        const Bytes bytes = assemble(
            "; un commentaire seul\n"
            "\n"
            "   SET   R1 ,  0x0A   ; commentaire apres l'instruction\n"
            "\thalt\n");
        check(bytes == Bytes({code(Op::Set), 1, 10, code(Op::Halt), 0, 0}),
              "commentaires, espaces, majuscules et hexadecimal");
    }
    {   // programme vide
        check(assemble("").empty(), "texte vide : aucun octet");
        check(assemble("; rien\n\n").empty(), "que des commentaires : aucun octet");
    }
    {   // etiquette en arriere (boucle) et etiquette seule sur sa ligne
        const Bytes bytes = assemble(
            "        Set r0, 5\n"
            "        Set r1, 0\n"
            "loop:   Add r1, r0\n"
            "        Dec r0\n"
            "        Jnz r0, loop\n"
            "        Halt\n");
        check(bytes.size() == 18, "boucle : 6 instructions");
        check(bytes[13] == 0 && bytes[14] == 6, "boucle : Jnz r0 vise l'adresse 6");

        const Bytes alone = assemble(
            "start:\n"
            "  Inc r0\n"
            "  Jmp start\n");
        check(alone[5] == 0, "etiquette seule : elle designe l'instruction suivante");
    }
    {   // etiquette en avant
        const Bytes bytes = assemble(
            "  Jmp end\n"
            "  Set r0, 99\n"
            "end: Halt\n");
        check(bytes[2] == 6, "etiquette en avant : end est a l'adresse 6");
    }
    {   // le programme assemble s'execute vraiment
        const Bytes bytes = assemble(
            "  Set r0, 5\n"
            "  Set r1, 0\n"
            "loop: Add r1, r0\n"
            "  Dec r0\n"
            "  Jnz r0, loop\n"
            "  Halt\n");
        Memory memory(64);
        memory.load(0, bytes);
        Cpu cpu;
        const Status status = run(cpu, memory);
        check(status == Status::Halted, "execution : arret normal");
        check(cpu.registers[1] == 15, "execution : la somme vaut 15");
        check(cpu.instructions_executed == 18, "execution : 18 instructions");
    }
    {   // aller-retour : desassembler une instruction puis la reassembler redonne les memes octets
        const Bytes program = {
            code(Op::Set),   0, 5,
            code(Op::Add),   1, 0,
            code(Op::Sub),   1, 2,
            code(Op::Mul),   3, 1,
            code(Op::Inc),   2, 0,
            code(Op::Dec),   2, 0,
            code(Op::Load),  1, 20,
            code(Op::Store), 1, 21,
            code(Op::Jmp),   0, 3,
            code(Op::Jnz),   2, 6,
            code(Op::Halt),  0, 0};
        Memory memory(64);
        memory.load(0, program);
        bool same = true;
        for (std::size_t i = 0; i < program.size() / 3; i++) {
            const Bytes again = assemble(disassemble(memory, i * 3));
            const Bytes original(program.begin() + static_cast<std::ptrdiff_t>(i * 3),
                                 program.begin() + static_cast<std::ptrdiff_t>(i * 3 + 3));
            if (again != original) {
                same = false;
            }
        }
        check(same, "aller-retour desassembleur puis assembleur");
    }

    // --- Erreurs : refusees, avec le bon numero de ligne ---

    check_error("Set r0, 1\nBlah r0\n", 2, "instruction inconnue");
    check_error("Set r0\n", 1, "operande manquant");
    check_error("Halt r0\n", 1, "operande en trop");
    check_error("Set r0, 1,\n", 1, "virgule finale");
    check_error("Set r0,, 1\n", 1, "operande vide");
    check_error("Set 5, 5\n", 1, "registre attendu");
    check_error("Add r1, 5\n", 1, "registre attendu en second operande");
    check_error("Set r0, 256\n", 1, "nombre trop grand");
    check_error("Set r0, 0xFFF\n", 1, "hexadecimal trop grand");
    check_error("Set r0, abc\n", 1, "nombre invalide");
    check_error("Set r0, 12x\n", 1, "nombre melange de lettres");
    check_error("Load r0, [5\n", 1, "crochet fermant manquant");
    check_error("\n\nJmp nulle_part\n", 3, "etiquette inconnue");
    check_error("a: Halt\na: Halt\n", 2, "etiquette en double");
    check_error("r1: Halt\n", 1, "une etiquette ne peut pas s'appeler comme un registre");
    check_error("1abc: Halt\n", 1, "nom d'etiquette invalide");

    {   // une etiquette au-dela de l'adresse 255 est refusee
        std::string source;
        for (int i = 0; i < 90; i++) {
            source += "Inc r0\n";
        }
        source += "far: Halt\n";
        source += "Jmp far\n";
        check_error(source, 92, "etiquette au dela de 255");
    }

    if (failures == 0) {
        std::cout << "Tous les tests passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
