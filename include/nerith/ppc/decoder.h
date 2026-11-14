#pragma once

#include <cstdint>
#include <string>

namespace nerith::ppc {

// Les instructions PowerPC que le decodeur reconnait pour l'instant.
enum class Kind {
    Unknown,   // mot de 32 bits non reconnu (ou forme pas encore geree)
    Addi,      // addi rD, rA, SIMM   (rD = rA + SIMM ; si rA vaut 0, on utilise la valeur 0)
    Addis,     // addis rD, rA, SIMM  (rA + SIMM * 65536 ; "lis rD, nombre" quand rA vaut 0)
    Add,       // add rD, rA, rB
    Sub,       // sub rD, rA, rB      (rD = rA - rB ; dans le mot, les champs rA et rB sont inverses)
    Mullw,     // mullw rD, rA, rB    (multiplication avec signe des 32 bits de poids faible)
    Rlwinm,    // rlwinm rA, rS, SH, MB, ME (rotation de 32 bits puis masque ; slwi, srwi, clrlwi...)
    Or,        // or rA, rS, rB       ("mr rA, rS" quand rS et rB sont identiques)
    Cmpw,      // cmpw crfD, rA, rB   (compare deux registres, resultat dans un champ de CR)
    Ori,       // ori rA, rS, UIMM    (nop = ori 0, 0, 0)
    Cmpwi,     // cmpwi crfD, rA, SIMM (compare rA a un nombre, resultat dans un champ de CR)
    Lwz,       // lwz rD, d(rA)       (lire un mot de 32 bits)
    Stw,       // stw rS, d(rA)       (ecrire un mot de 32 bits)
    Stwu,      // stwu rS, d(rA)      (comme stw, puis rA = adresse ecrite : sert a creer une pile)
    Mflr,      // mflr rD             (rD = LR)
    Mtlr,      // mtlr rS             (LR = rS)
    Mfctr,     // mfctr rD            (rD = CTR, le registre compteur)
    Mtctr,     // mtctr rS            (CTR = rS)
    Lbz,       // lbz rD, d(rA)       (lire un octet, complete par des 0)
    Stb,       // stb rS, d(rA)       (ecrire un octet : les 8 bits de poids faible de rS)
    B,         // b / bl / ba / bla   (saut ; bl sauvegarde le retour dans LR)
    Bc,        // saut conditionnel : beq, bne, blt, bgt, ble, bge (selon un bit de CR) ; bdnz, bdz (selon CTR)
    Blr,       // blr / blrl          (retour de fonction : saute a LR ; blrl sauvegarde ensuite le retour dans LR)
    Bctr,      // bctr / bctrl        (saut vers l'adresse dans CTR ; bctrl sauvegarde le retour dans LR)
    Lwzx,      // lwzx rD, rA, rB     (comme lwz, mais l'adresse est rA + rB ; rA = 0 veut dire le nombre 0)
    Stwx,      // stwx rS, rA, rB     (comme stw, adresse rA + rB)
    Lbzx,      // lbzx rD, rA, rB     (comme lbz, adresse rA + rB)
    Cmpdi,     // cmpdi crfD, rA, SIMM (comme cmpwi, mais compare les 64 bits de rA)
    Cmpd,      // cmpd crfD, rA, rB   (comme cmpw, mais compare les 64 bits)
    Ld,        // ld rD, ds(rA)       (lire un mot de 64 bits ; ds est un multiple de 4)
    Std,       // std rS, ds(rA)      (ecrire un mot de 64 bits)
    Stbx,      // stbx rS, rA, rB     (comme stb, adresse rA + rB)
    // --- Instructions decrites dans la table (optable.h) ---
    // Entier : multiplication, division, logique, decalages, extensions
    Mulli, Mulhw, Mulhwu, Mulld, Divw, Divwu, Divd, Divdu, Neg,
    And, Andc, Xor, Nor, Nand, Orc, Eqv,
    Slw, Srw, Sld, Srd, Cntlzw, Cntlzd, Extsb, Extsh, Extsw,
    Oris, Xori, Xoris, AndiDot, AndisDot,
    // Memoire : demi-mots (16 bits), instructions "avec mise a jour" (u), indexees (x), inversees (br)
    Lwzu, Lbzu, Stbu, Lhz, Lhzu, Lha, Lhau, Sth, Sthu,
    Lwa, Ldu, Stdu,
    Lhzx, Lhax, Sthx, Lwzux, Lbzux, Lhzux, Lhaux, Stwux, Stbux, Sthux, Lwax, Lwaux,
    Ldx, Stdx, Ldux, Stdux, Lwbrx, Stwbrx, Lhbrx, Sthbrx, Ldbrx, Stdbrx,
    // Comparaisons sans signe et rotations (ecrites a la main dans le decodeur)
    Cmplwi, Cmpldi, Cmplw, Cmpld,
    Rlwimi, Rlwnm, Rldicl, Rldicr, Rldic, Rldimi,
    // Retenue (bit CA du registre XER) : addition/soustraction avec retenue, decalages arithmetiques
    Addic, AddicDot, Subfic, Addc, Adde, Subfc, Subfe, Addze, Addme, Subfze, Subfme,
    Sraw, Srad, Srawi, Sradi, Mfxer, Mtxer,
    // Registre de conditions (CR) : copie de champ, operations sur un bit, lecture/ecriture complete
    Mcrf, Crand, Crnand, Cror, Crxor, Crnor, Creqv, Crandc, Crorc,
    Mfcr, Mfocrf, Mtcrf, Mtocrf,
    // Sauts conditionnels indirects (vers LR ou CTR), appel systeme, barrieres memoire, verrous atomiques
    Bclr, Bcctr, Sc, Sync, Eieio, Isync,
    Lwarx, Ldarx, Stwcx, Stdcx,
    // Flottants : lecture/ecriture (s = 32 bits, d = 64 bits, u = avec mise a jour, x = indexe)
    Lfs, Lfsu, Lfd, Lfdu, Stfs, Stfsu, Stfd, Stfdu,
    Lfsx, Lfsux, Lfdx, Lfdux, Stfsx, Stfsux, Stfdx, Stfdux, Stfiwx,
    // Flottants : calculs (la lettre s a la fin = resultat arrondi a 32 bits)
    Fadd, Fadds, Fsub, Fsubs, Fmul, Fmuls, Fdiv, Fdivs, Fsqrt, Fsqrts, Fres, Frsqrte, Fsel,
    Fmadd, Fmadds, Fmsub, Fmsubs, Fnmadd, Fnmadds, Fnmsub, Fnmsubs,
    // Flottants : copies, conversions, comparaisons
    Fmr, Fneg, Fabs, Fnabs, Frsp, Fctiw, Fctiwz, Fctid, Fctidz, Fcfid, Fcmpu, Fcmpo,
    // Registre d'etat des flottants (FPSCR)
    Mffs, Mtfsf, Mtfsb0, Mtfsb1, Mtfsfi, Mcrfs,

    // caches (sans effet chez nous, sauf dcbz qui met a zero) et registres systeme
    Dcbt, Dcbtst, Dcbst, Dcbf, Icbi,
    Dcbz,    // met a zero un bloc de 32 octets
    Dcbzl,   // variante 128 octets (dcbz128 sur Xenon)
    Mfmsr, Mtmsr, Mtmsrd,
    Mfspr,   // forme generale : imm = numero du SPR (SRR0/1, SPRG0-3, DEC, PVR, base de temps)
    Mtspr,
    Mftb,    // lecture de la base de temps (imm = 268) ou de sa moitie haute (269)
};

// Le resultat du decodage d'un mot de 32 bits.
// Selon l'instruction, certains champs ne servent pas (ils valent 0).
struct Instruction {
    Kind kind = Kind::Unknown;
    std::uint32_t word = 0;   // le mot d'origine
    std::uint8_t rd = 0;      // registre destination (ou source pour stw et ori : champ rS) ; flottants : frD / frS
    std::uint8_t ra = 0;
    std::uint8_t rb = 0;
    std::uint8_t frc = 0;     // flottants : 3e registre source (champ frC, bits 21-25) : fmul, fmadd, fsel...
    std::int32_t imm = 0;     // valeur immediate (avec signe) ou deplacement du saut, en octets
    bool absolute = false;    // B, Bc : bit AA (adresse absolue au lieu de relative)
    bool link = false;        // B, Bc, Blr, Bctr, Bclr, Bcctr : bit LK (sauvegarde l'adresse de retour dans LR)
    std::uint8_t sh = 0;      // Rlwinm : rotation a gauche (0 a 31) ; Rldicl... : 0 a 63
    std::uint8_t mb = 0;      // Rlwinm : debut du masque (bit 0 = poids fort des 32 bits)
    std::uint8_t me = 0;      // Rlwinm : fin du masque
    std::uint8_t crf = 0;     // Cmpwi : numero du champ de CR (0 a 7)
    std::uint8_t bo = 0;      // Bc, Bclr, Bcctr : les 5 bits BO (16 = pas de test de CR, 8 = valeur du bit attendue,
                              //                     4 = pas de decrement de CTR, 2 = saute si CTR vaut 0)
    std::uint8_t bi = 0;      // Bc... : numero du bit de CR teste (0 a 31 ; champ = bi / 4) ; Mcrf : champ source
};

// Decode un mot de 32 bits (deja lu en big-endian, par exemple avec Memory::read32).
// Ne leve jamais d'exception : un mot inconnu donne Kind::Unknown.
Instruction decode(std::uint32_t word);

// Ecrit l'instruction en texte, par exemple "li r3, 5" ou "lwz r3, 8(r1)".
// Un mot inconnu donne ".long 0x12345678".
std::string to_text(const Instruction& instruction);

}  // namespace nerith::ppc
