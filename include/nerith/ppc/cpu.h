#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "nerith/memory/memory.h"

namespace nerith::ppc {

// Etat du processeur PowerPC (version tres reduite pour commencer).
struct Cpu {
    std::array<std::uint64_t, 32> gpr{};   // r0 a r31, 64 bits chacun
    std::uint64_t pc = 0;                  // adresse de la prochaine instruction
    std::uint64_t lr = 0;                  // registre de lien : adresse de retour (bl, blr)
    std::uint64_t ctr = 0;                 // registre compteur : boucles (bdnz) et sauts indirects (bctr)
    std::uint32_t cr = 0;                  // registre de conditions : 8 champs de 4 bits (LT, GT, EQ, SO)
    std::array<std::uint64_t, 32> fpr{};   // f0 a f31 : 64 bits chacun, gardes tels quels (un double IEEE 754)
    std::uint32_t fpscr = 0;               // etat des flottants : mode d'arrondi (bits 30-31), resultat de la derniere comparaison (bits 16-19)...
    bool reserved = false;                 // une reservation existe (posee par lwarx ou ldarx, consommee par stwcx. ou stdcx.)
    std::uint64_t reserve_address = 0;     // adresse de cette reservation
    std::uint32_t xer = 0;                 // registre d'exception : SO = 0x80000000, OV = 0x40000000, CA (retenue) = 0x20000000

    // Registres systeme (acces par mfmsr/mtmsr et mfspr/mtspr).
    std::uint64_t msr = 0;                   // etat de la machine : interruptions, mode 32/64 bits... (on le garde, sans effet pour l'instant)
    std::uint64_t srr0 = 0;                  // SPR 26 : adresse de retour d'interruption
    std::uint64_t srr1 = 0;                  // SPR 27 : copie du MSR a l'interruption
    std::array<std::uint64_t, 4> sprg{};     // SPR 272 a 275 : cases libres pour le systeme
    std::uint32_t dec = 0;                   // SPR 22 : decompteur, diminue de 1 par instruction executee
    std::uint32_t pvr = 0;                   // SPR 287 : version du processeur, lecture seule (valeur du vrai Xenon a verifier)
    std::uint64_t tb = 0;                    // base de temps (mftb) : augmente de 1 par instruction executee

    // Registres vectoriels (AltiVec / VMX128) : 128 registres de 128 bits. Un registre = 4 mots de 32 bits, le mot 0 est celui de poids FORT.
    std::array<std::array<std::uint32_t, 4>, 128> vr{};
    std::uint32_t vscr = 0;                  // etat vectoriel : SAT (bit 0) = une saturation a eu lieu ; NJ (bit 16) n'est pas utilise
};

// Dans CR, le champ n (0 a 7) occupe 4 bits ; le champ 0 est tout a gauche (bits de poids fort).
// Dans un champ : LT = 8, GT = 4, EQ = 2, SO = 1. Exemple : "egal dans le champ 0" donne cr = 0x20000000.

// Ce qui s'est passe pendant l'execution.
enum class Status {
    Running,              // l'instruction a ete executee, on peut continuer
    Halted,               // blr vers l'adresse 0 : le programme est termine
    SystemCall,           // sc : l'appel systeme est a traiter par l'appelant ; pc pointe deja sur l'instruction suivante
    InvalidInstruction,   // mot inconnu ou pas encore gere
    OutOfBounds,          // acces en dehors de la memoire
    StepLimit,            // run : trop d'instructions (protege des boucles infinies)
};

// Execute UNE instruction a l'adresse cpu.pc.
// Ne leve pas d'exception. En cas d'erreur (InvalidInstruction, OutOfBounds),
// le processeur et la memoire ne sont pas modifies.
// Convention : "blr" avec lr == 0 termine le programme (Status::Halted).
Status step(Cpu& cpu, Memory& memory);

// Execute des instructions jusqu'a l'arret, une erreur ou max_steps instructions.
Status run(Cpu& cpu, Memory& memory, std::size_t max_steps);

}  // namespace nerith::ppc
