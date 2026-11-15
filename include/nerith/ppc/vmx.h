#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nerith::ppc {

// Instructions vectorielles : AltiVec standard (registres v0-v31) et VMX128 de la Xbox 360 (v0-v127).
// Un registre vectoriel fait 128 bits = 4 mots de 32 bits ; le mot 0 est celui de poids FORT (le premier en memoire).
//
// Chaque instruction est decrite par une ligne de la table (vmx.cpp) : nom, motif du mot, masque, disposition des champs.
// Les formes "standard" et "128" d'une meme operation partagent le meme VecOp (le calcul est identique).
enum class VecOp : std::uint8_t {
    None,
    // lectures et ecritures (adresse = (rA|0) + rB)
    Lvebx, Lvehx, Lvewx, Lvsl, Lvsr, Lvx, Lvxl,
    Stvebx, Stvehx, Stvewx, Stvx, Stvxl,
    Lvlx, Lvlxl, Lvrx, Lvrxl, Stvlx, Stvlxl, Stvrx, Stvrxl,   // gauche/droite : propres au Xenon et au Cell
    // flottants (4 flottants de 32 bits par registre)
    Vaddfp, Vsubfp, Vmulfp128, Vmaddfp, Vnmsubfp, Vmaddfp128, Vmaddcfp128, Vnmsubfp128, Vmsum3fp128, Vmsum4fp128,
    Vmaxfp, Vminfp, Vrefp, Vrsqrtefp, Vexptefp, Vlogefp, Vrfin, Vrfiz, Vrfip, Vrfim,
    Vcfux, Vcfsx, Vctuxs, Vctsxs,
    Vcmpeqfp, Vcmpgefp, Vcmpgtfp, Vcmpbfp, Vcmpequw,
    // logique, selection
    Vand, Vandc, Vor, Vnor, Vxor, Vsel, Vsel128,
    // entiers 32 bits
    Vadduwm, Vsubuwm, Vrlw, Vslw, Vsrw, Vsraw,
    // permutations, melanges, copies d'un mot
    Vperm, Vsldoi, Vmrghw, Vmrglw, Vspltw, Vspltisw, Vslo, Vsro, Vpermwi128, Vrlimi128,
    // conversions de taille
    Vpkuhum, Vpkuwum, Vpkuhus, Vpkuwus, Vpkshus, Vpkswus, Vpkshss, Vpkswss, Vupkhsb, Vupklsb,
    Vpkd3d128, Vupkd3d128,   // formats Direct3D : reconnues, pas encore executees
};

// Disposition des champs (qui est ou dans le mot, et dans quel ordre on ecrit les operandes).
enum class VForm : std::uint8_t {
    Mem,         // vD, rA, rB                  lvx, stvx...  (rA = 0 veut dire le nombre 0)
    Ab,          // vD, vA, vB
    AbC,         // vD, vA, vB, vC              vperm, vsel (standard)
    AcB,         // vD, vA, vC, vB              vmaddfp, vnmsubfp (standard : comme l'ecrit llvm)
    AbShb,       // vD, vA, vB, SHB             vsldoi (decalage de 0 a 15 octets)
    B,           // vD, vB                      vrefp...
    BUimm,       // vD, vB, UIMM                vcfsx, vctsxs... (UIMM de 5 bits, 0 a 31)
    BSplat,      // vD, vB, UIMM                vspltw (UIMM de 2 bits dans la forme standard)
    Simm,        // vD, SIMM                    vspltisw standard (vB doit valoir 0)
    // formes 128 : numeros de registre sur 7 bits, repartis dans plusieurs morceaux du mot
    AbVd,        // vD, vA, vB, vD              vmaddfp128, vnmsubfp128, vsel128 (le dernier operande est vD lui-meme)
    AdB,         // vD, vA, vD, vB              vmaddcfp128
    Perm128,     // vD, vA, vB, vC              vperm128 (vC : v0 a v7 seulement)
    Pwi128,      // vD, vB, PERM                vpermwi128 (PERM de 8 bits)
    BSimm128,    // vD, vB, SIMM                vspltisw128 (vB sans effet)
    Rlimi128,    // vD, vB, UIMM, Z             vrlimi128
    Pkd3d128,    // vD, vB, X, Y, Z             vpkd3d128
};

struct VecInfo {
    VecOp op;
    const char* name;
    std::uint32_t match;   // valeur du mot sur les bits du masque
    std::uint32_t mask;    // bits qui identifient l'instruction
    VForm form;
    bool is128;            // forme VMX128 (registres sur 7 bits)
    bool rc;               // forme "point" des comparaisons : CR6 est mis a jour
};

const VecInfo* vec_table();
std::size_t vec_table_size();

// Cherche une ligne de la table pour un mot (nullptr si rien ne correspond ou si un bit reserve n'est pas 0).
const VecInfo* find_vec_for_word(std::uint32_t word);
const VecInfo* find_vec_by_name(const std::string& name);

// Champs "scindes" de VMX128 : les numeros de registre (0 a 127) sont repartis dans le mot.
inline unsigned vmx_vd128(std::uint32_t w) { return ((w >> 21) & 0x1Fu) | ((w << 3) & 0x60u); }
inline unsigned vmx_va128(std::uint32_t w) { return ((w >> 16) & 0x1Fu) | (w & 0x20u) | ((w >> 4) & 0x40u); }
inline unsigned vmx_vb128(std::uint32_t w) { return ((w >> 11) & 0x1Fu) | ((w << 5) & 0x60u); }
inline std::uint32_t vmx_put_vd128(unsigned v) { return ((v & 0x1Fu) << 21) | ((v & 0x60u) >> 3); }
inline std::uint32_t vmx_put_va128(unsigned v) { return ((v & 0x1Fu) << 16) | (v & 0x20u) | ((v & 0x40u) << 4); }
inline std::uint32_t vmx_put_vb128(unsigned v) { return ((v & 0x1Fu) << 11) | ((v & 0x60u) >> 5); }

}  // namespace nerith::ppc
