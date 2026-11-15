#include "nerith/ppc/decoder.h"

#include <iomanip>
#include <sstream>

#include "nerith/ppc/optable.h"

namespace nerith::ppc {

namespace {

// Dans les manuels PowerPC, les bits sont numerotes de 0 (poids fort) a 31 (poids faible).
// Ici on lit les champs avec des decalages classiques.
std::uint32_t opcode_of(std::uint32_t w) { return w >> 26; }                         // bits 0-5
std::uint8_t field_6_10(std::uint32_t w) { return static_cast<std::uint8_t>((w >> 21) & 0x1F); }
std::uint8_t field_11_15(std::uint32_t w) { return static_cast<std::uint8_t>((w >> 16) & 0x1F); }
std::uint8_t field_16_20(std::uint32_t w) { return static_cast<std::uint8_t>((w >> 11) & 0x1F); }

// Les 16 bits de poids faible, vus comme un nombre avec signe.
std::int32_t simm16(std::uint32_t w) {
    return static_cast<std::int32_t>(static_cast<std::int16_t>(w & 0xFFFF));
}

std::string reg_name(std::uint8_t reg) {
    return "r" + std::to_string(static_cast<int>(reg));
}

std::string freg_name(std::uint8_t reg) {
    return "f" + std::to_string(static_cast<int>(reg));
}

std::string cr_name(int crf) {
    return "cr" + std::to_string(crf);
}

std::string signed_offset(std::int32_t value) {
    return (value < 0 ? "-" : "+") + std::to_string(value < 0 ? -value : value);
}

// Texte d'une instruction decrite dans la table.
std::string table_text(const OpInfo& op, const Instruction& i) {
    const std::string name = op.name;
    const std::string rd = reg_name(i.rd);
    const std::string ra = reg_name(i.ra);
    const std::string rb = reg_name(i.rb);
    switch (op.layout) {
        case Layout::RtRaRb:
            return name + " " + rd + ", " + ra + ", " + rb;
        case Layout::RaRsRb:   // rd est rS, ra est la destination
            if (i.kind == Kind::Nor && i.rd == i.rb) {
                return "not " + ra + ", " + rd;
            }
            return name + " " + ra + ", " + rd + ", " + rb;
        case Layout::RaRs:
            return name + " " + ra + ", " + rd;
        case Layout::RaRsSh:
            return name + " " + ra + ", " + rd + ", " + std::to_string(i.sh);
        case Layout::RtRbRa:   // subc rD, rB, rA ; avec OE llvm n'a plus d'alias et ecrit subfc dans l'ordre du mot
            if (i.oe) {
                return "subfc " + rd + ", " + ra + ", " + rb;
            }
            return name + " " + rd + ", " + rb + ", " + ra;
        case Layout::RtRa:
            return name + " " + rd + ", " + ra;
        case Layout::RtRaSimm:
            return name + " " + rd + ", " + ra + ", " + std::to_string(i.imm);
        case Layout::RaRsUimm:
            return name + " " + ra + ", " + rd + ", " + std::to_string(i.imm);
        case Layout::MemD:
        case Layout::MemDS:
            return name + " " + rd + ", " + std::to_string(i.imm) + "(" + ra + ")";
        case Layout::MemX:   // rA = 0 veut dire le nombre 0 : on l'ecrit "0", comme llvm
            return name + " " + rd + ", " + (i.ra == 0 ? std::string("0") : ra) + ", " + rb;
        case Layout::FMemD:
            return name + " " + freg_name(i.rd) + ", " + std::to_string(i.imm) + "(" + ra + ")";
        case Layout::FMemX:
            return name + " " + freg_name(i.rd) + ", " + (i.ra == 0 ? std::string("0") : ra) + ", " + rb;
        case Layout::FA3:
            return name + " " + freg_name(i.rd) + ", " + freg_name(i.ra) + ", " + freg_name(i.rb);
        case Layout::FA3c:
            return name + " " + freg_name(i.rd) + ", " + freg_name(i.ra) + ", " + freg_name(i.frc);
        case Layout::FA4:
            return name + " " + freg_name(i.rd) + ", " + freg_name(i.ra) + ", " + freg_name(i.frc) + ", " +
                   freg_name(i.rb);
        case Layout::FA2:
        case Layout::FX2:
            return name + " " + freg_name(i.rd) + ", " + freg_name(i.rb);
        case Layout::FCmp:
            return name + " " + cr_name(i.crf) + ", " + freg_name(i.ra) + ", " + freg_name(i.rb);
        case Layout::FrD:
            return name + " " + freg_name(i.rd);
        case Layout::BtOnly:
            return name + " " + std::to_string(static_cast<int>(i.rd));
        case Layout::FmFrb:
            return name + " " + std::to_string(i.imm) + ", " + freg_name(i.rb);
        case Layout::CrfImm:
            return name + " " + cr_name(i.crf) + ", " + std::to_string(i.imm);
        case Layout::CrfCrf:
            return name + " " + cr_name(i.crf) + ", " + cr_name(i.bi);
        case Layout::CrBits: {
            const std::string bt = std::to_string(static_cast<int>(i.rd));
            const std::string ba = std::to_string(static_cast<int>(i.ra));
            const std::string bb = std::to_string(static_cast<int>(i.rb));
            if (i.kind == Kind::Crxor && i.rd == i.ra && i.ra == i.rb) {
                return "crclr " + bt;
            }
            if (i.kind == Kind::Creqv && i.rd == i.ra && i.ra == i.rb) {
                return "crset " + bt;
            }
            if (i.kind == Kind::Crnor && i.ra == i.rb) {
                return "crnot " + bt + ", " + ba;
            }
            if (i.kind == Kind::Cror && i.ra == i.rb) {
                return "crmove " + bt + ", " + ba;
            }
            return name + " " + bt + ", " + ba + ", " + bb;
        }
    }
    return name;
}

// Nom d'un saut conditionnel : "beq", "bdnz", "bdnzt", ou "bc" quand aucun raccourci ne convient.
// "target" vaut "" (bc), "lr" (bclr) ou "ctr" (bcctr). Le suffixe l (LK) et a (AA) est ajoute a la fin.
std::string branch_text(const Instruction& i, const std::string& target) {
    static const char* const kTrue[]  = {"blt", "bgt", "beq", "bso"};
    static const char* const kFalse[] = {"bge", "ble", "bne", "bns"};
    std::string name;
    if (i.bo == 12 || i.bo == 4) {
        name = (i.bo == 12 ? kTrue : kFalse)[i.bi % 4] + target;
    } else if ((i.bo == 16 || i.bo == 18) && i.bi == 0) {
        name = std::string(i.bo == 16 ? "bdnz" : "bdz") + target;
    } else if (i.bo == 8 || i.bo == 0 || i.bo == 10 || i.bo == 2) {
        const char* ctr_name = i.bo == 0 ? "bdnzf" : (i.bo == 2 ? "bdzf" : (i.bo == 8 ? "bdnzt" : "bdzt"));
        name = std::string(ctr_name) + target;
    } else {
        name = (target.empty() ? std::string("bc") : "bc" + target);
    }
    if (i.link) {
        name += "l";
    }
    if (i.absolute) {
        name += "a";
    }
    return name;
}

// Ce qui s'ecrit entre le nom et la cible : "cr1,", "2," ou "20, 3,". Vide pour les raccourcis simples.
std::string branch_leading(const Instruction& i) {
    if (i.bo == 12 || i.bo == 4) {
        return i.bi >= 4 ? "cr" + std::to_string(static_cast<int>(i.bi / 4)) + ", " : std::string();
    }
    if ((i.bo == 16 || i.bo == 18) && i.bi == 0) {
        return std::string();
    }
    if (i.bo == 8 || i.bo == 0 || i.bo == 10 || i.bo == 2) {
        return std::to_string(static_cast<int>(i.bi)) + ", ";
    }
    return std::to_string(static_cast<int>(i.bo)) + ", " + std::to_string(static_cast<int>(i.bi)) + ", ";
}

// Pour bclr / bcctr (pas de cible) : " cr1", " 2" ou " 20, 3". Vide pour les raccourcis simples.
std::string branch_leading_only(const Instruction& i) {
    std::string text = branch_leading(i);
    if (text.empty()) {
        return text;
    }
    text.resize(text.size() - 2);   // retire la virgule finale
    return " " + text;
}

// "cmplwi cr1, r3, 5" : le champ de CR n'est ecrit que s'il n'est pas cr0.
std::string compare_text(const std::string& name, std::uint8_t crf, const std::string& a, const std::string& b) {
    return name + " " + (crf == 0 ? std::string() : "cr" + std::to_string(static_cast<int>(crf)) + ", ") + a + ", " + b;
}

}  // namespace

Instruction decode(std::uint32_t word) {
    Instruction result;
    result.word = word;

    switch (opcode_of(word)) {
        case 11:   // cmpwi (L = 0, 32 bits) ou cmpdi (L = 1, 64 bits)
            result.kind = ((word >> 21) & 0x1) == 0 ? Kind::Cmpwi : Kind::Cmpdi;
            result.crf = static_cast<std::uint8_t>((word >> 23) & 0x7);
            result.ra = field_11_15(word);
            result.imm = simm16(word);
            break;

        case 10:   // cmplwi (L = 0, 32 bits) ou cmpldi (L = 1, 64 bits) : comparaison SANS signe
            result.kind = ((word >> 21) & 0x1) == 0 ? Kind::Cmplwi : Kind::Cmpldi;
            result.crf = static_cast<std::uint8_t>((word >> 23) & 0x7);
            result.ra = field_11_15(word);
            result.imm = static_cast<std::int32_t>(word & 0xFFFF);   // nombre sans signe
            break;

        case 20:   // rlwimi : insere des bits tournes dans rA ; rd est rS
        case 23:   // rlwnm : comme rlwinm, mais la rotation vient de rB
            {
                result.kind = opcode_of(word) == 20 ? Kind::Rlwimi : Kind::Rlwnm;
                result.rc = (word & 0x1) != 0;
                result.rd = field_6_10(word);
                result.ra = field_11_15(word);
                if (result.kind == Kind::Rlwimi) {
                    result.sh = field_16_20(word);
                } else {
                    result.rb = field_16_20(word);
                }
                result.mb = static_cast<std::uint8_t>((word >> 6) & 0x1F);
                result.me = static_cast<std::uint8_t>((word >> 1) & 0x1F);
            }
            break;

        case 30:   // rotations 64 bits : forme MD (rldicl, rldicr, rldic, rldimi) et forme MDS (rldcl, rldcr)
            if (((word >> 2) & 0x7) <= 4) {
                static const Kind kKinds[] = {Kind::Rldicl, Kind::Rldicr, Kind::Rldic, Kind::Rldimi, Kind::Rldcl};
                const unsigned form = (word >> 2) & 0x7;
                result.kind = form == 4 ? (((word >> 1) & 1u) == 0 ? Kind::Rldcl : Kind::Rldcr) : kKinds[form];
                result.rc = (word & 0x1) != 0;
                result.rd = field_6_10(word);
                result.ra = field_11_15(word);
                // mb/me : 6 bits "retournes"
                const unsigned raw = (word >> 5) & 0x3F;
                const std::uint8_t mask_bit = static_cast<std::uint8_t>(((raw & 1u) << 5) | (raw >> 1));
                if (form == 4) {   // rldcl / rldcr : la rotation vient de rB (bits 16-20)
                    result.rb = field_16_20(word);
                } else {
                    // sh : 6 bits, le bit de poids fort est tout a droite (bit 30)
                    result.sh = static_cast<std::uint8_t>((((word >> 1) & 0x1) << 5) | field_16_20(word));
                }
                if (result.kind == Kind::Rldicr || result.kind == Kind::Rldcr) {
                    result.me = mask_bit;
                } else {
                    result.mb = mask_bit;
                }
            }
            break;

        case 58:   // ld : seulement la forme de base (les 2 bits de poids faible valent 0 ; 1 = ldu, 2 = lwa)
            if ((word & 0x3) == 0) {
                result.kind = Kind::Ld;
                result.rd = field_6_10(word);
                result.ra = field_11_15(word);
                result.imm = simm16(word);
            }
            break;

        case 62:   // std : seulement la forme de base (1 = stdu)
            if ((word & 0x3) == 0) {
                result.kind = Kind::Std;
                result.rd = field_6_10(word);
                result.ra = field_11_15(word);
                result.imm = simm16(word);
            }
            break;

        case 16:   // bc : n'importe quelle combinaison BO / BI, avec AA et LK
            result.kind = Kind::Bc;
            result.bo = field_6_10(word);
            result.bi = field_11_15(word);
            result.imm = static_cast<std::int32_t>(static_cast<std::int16_t>(word & 0xFFFC));
            result.absolute = (word & 0x2) != 0;
            result.link = (word & 0x1) != 0;
            break;

        case 21:   // rlwinm ; rd est rS, ra est la destination
            {
                result.kind = Kind::Rlwinm;
                result.rc = (word & 0x1) != 0;
                result.rd = field_6_10(word);
                result.ra = field_11_15(word);
                result.sh = field_16_20(word);
                result.mb = static_cast<std::uint8_t>((word >> 6) & 0x1F);
                result.me = static_cast<std::uint8_t>((word >> 1) & 0x1F);
            }
            break;

        case 15:   // addis (et lis)
            result.kind = Kind::Addis;
            result.rd = field_6_10(word);
            result.ra = field_11_15(word);
            result.imm = simm16(word);
            break;

        case 37:   // stwu
            result.kind = Kind::Stwu;
            result.rd = field_6_10(word);
            result.ra = field_11_15(word);
            result.imm = simm16(word);
            break;

        case 34:   // lbz
            result.kind = Kind::Lbz;
            result.rd = field_6_10(word);
            result.ra = field_11_15(word);
            result.imm = simm16(word);
            break;

        case 38:   // stb
            result.kind = Kind::Stb;
            result.rd = field_6_10(word);
            result.ra = field_11_15(word);
            result.imm = simm16(word);
            break;

        case 14:   // addi
            result.kind = Kind::Addi;
            result.rd = field_6_10(word);
            result.ra = field_11_15(word);
            result.imm = simm16(word);
            break;

        case 24:   // ori : le champ 6-10 est rS, le champ 11-15 est rA (destination)
            result.kind = Kind::Ori;
            result.rd = field_6_10(word);
            result.ra = field_11_15(word);
            result.imm = static_cast<std::int32_t>(word & 0xFFFF);
            break;

        case 32:   // lwz
            result.kind = Kind::Lwz;
            result.rd = field_6_10(word);
            result.ra = field_11_15(word);
            result.imm = simm16(word);
            break;

        case 36:   // stw
            result.kind = Kind::Stw;
            result.rd = field_6_10(word);
            result.ra = field_11_15(word);
            result.imm = simm16(word);
            break;

        case 18: { // b, bl, ba, bla : 24 bits de deplacement (bits 6-29), a multiplier par 4
            result.kind = Kind::B;
            std::int32_t li = static_cast<std::int32_t>((word >> 2) & 0xFFFFFF);
            if (li & 0x800000) {
                li -= 0x1000000;   // extension du signe sur 24 bits
            }
            result.imm = li * 4;
            result.absolute = (word & 0x2) != 0;
            result.link = (word & 0x1) != 0;
            break;
        }

        case 19: {   // sauts vers LR / CTR, mcrf, isync ; les operations sur les bits de CR viennent de la table
            const std::uint32_t xo = (word >> 1) & 0x3FF;
            if (xo == 16 || xo == 528) {   // bclr / bcctr : les bits 16 a 20 (reserves, puis "BH") doivent valoir 0
                if ((word & 0xF800u) != 0) {
                    break;
                }
                const std::uint8_t bo = field_6_10(word);
                const std::uint8_t bi = field_11_15(word);
                if (xo == 528 && (bo & 4) == 0) {
                    break;   // bcctr ne peut pas decrementer CTR
                }
                result.link = (word & 0x1) != 0;
                if (bo == 20 && bi == 0) {
                    result.kind = xo == 16 ? Kind::Blr : Kind::Bctr;   // blr, blrl, bctr, bctrl
                } else {
                    result.kind = xo == 16 ? Kind::Bclr : Kind::Bcctr;
                    result.bo = bo;
                    result.bi = bi;
                }
            } else if ((word & 0x1) == 0 && xo == 0 && (word & 0x0063F800u) == 0) {   // mcrf crD, crS
                result.kind = Kind::Mcrf;
                result.crf = static_cast<std::uint8_t>((word >> 23) & 0x7);
                result.bi = static_cast<std::uint8_t>((word >> 18) & 0x7);
            } else if (word == 0x4C00012Cu) {
                result.kind = Kind::Isync;
            }
            break;
        }

        case 17:   // sc (appel systeme) : seulement la forme sans niveau
            if (word == 0x44000002u) {
                result.kind = Kind::Sc;
            }
            break;

        case 31: {   // instructions "registre, registre"
            const std::uint32_t xo10 = (word >> 1) & 0x3FF;   // champ XO ; son bit de poids fort est le bit OE
            const std::uint32_t xo9 = xo10 & 0x1FF;
            if (xo9 == 266 || xo9 == 40 || xo9 == 235 || xo10 == 444 || xo10 == 826 || xo10 == 827) {
                // add, subf, mullw (OE possible), or, sradi : le bit Rc peut valoir 1
                result.rc = (word & 0x1) != 0;
                result.rd = field_6_10(word);
                result.ra = field_11_15(word);
                result.rb = field_16_20(word);
                if (xo10 == 444) {
                    result.kind = Kind::Or;
                } else if (xo10 == 826 || xo10 == 827) {   // sradi : l'opcode etendu a 9 bits vaut 413, et le bit 30 est le bit de poids fort de SH
                    result.kind = Kind::Sradi;
                    result.sh = static_cast<std::uint8_t>(((word >> 1) & 1u) << 5 | field_16_20(word));
                    result.rb = 0;
                } else {
                    result.kind = xo9 == 266 ? Kind::Add : (xo9 == 40 ? Kind::Sub : Kind::Mullw);
                    result.oe = (xo10 & 0x200u) != 0;
                }
            } else if ((word & 0x1) == 0) {   // les autres : seulement Rc = 0
                result.rd = field_6_10(word);
                result.ra = field_11_15(word);
                result.rb = field_16_20(word);
                switch (xo10) {
                    case 23:  result.kind = Kind::Lwzx;  break;
                    case 151: result.kind = Kind::Stwx;  break;
                    case 87:  result.kind = Kind::Lbzx;  break;
                    case 215: result.kind = Kind::Stbx;  break;
                    case 339:   // mfspr
                    case 467:   // mtspr
                        {
                            const std::uint32_t field = (word >> 11) & 0x3FF;   // moities inversees
                            const std::uint32_t spr = ((field & 0x1F) << 5) | (field >> 5);
                            const bool read = ((word >> 1) & 0x3FF) == 339;
                            if (spr == 1) {
                                result.kind = read ? Kind::Mfxer : Kind::Mtxer;
                            } else if (spr == 8) {
                                result.kind = read ? Kind::Mflr : Kind::Mtlr;
                            } else if (spr == 9) {
                                result.kind = read ? Kind::Mfctr : Kind::Mtctr;
                            } else if (spr == 22 || spr == 26 || spr == 27 || (spr >= 272 && spr <= 275)) {
                                result.kind = read ? Kind::Mfspr : Kind::Mtspr;   // DEC, SRR0, SRR1, SPRG0 a SPRG3
                                result.imm = static_cast<std::int32_t>(spr);
                            } else if (read && (spr == 268 || spr == 269 || spr == 287)) {
                                result.kind = Kind::Mfspr;   // base de temps et PVR : lecture seule
                                result.imm = static_cast<std::int32_t>(spr);
                            }
                            result.ra = 0;
                            result.rb = 0;
                        }
                        break;
                    case 371:   // mftb : base de temps (268) ou sa moitie haute (269)
                        {
                            const std::uint32_t field = (word >> 11) & 0x3FF;
                            const std::uint32_t spr = ((field & 0x1F) << 5) | (field >> 5);
                            if (spr == 268 || spr == 269) {
                                result.kind = Kind::Mftb;
                                result.imm = static_cast<std::int32_t>(spr);
                            }
                            result.ra = 0;
                            result.rb = 0;
                        }
                        break;
                    case 83:    // mfmsr : les champs rA et rB doivent etre a 0
                        if (result.ra == 0 && result.rb == 0) {
                            result.kind = Kind::Mfmsr;
                        }
                        break;
                    case 146:   // mtmsr (L dans le bit de poids faible du champ rA)
                    case 178:   // mtmsrd
                        if ((result.ra & 0x1E) == 0 && result.rb == 0) {
                            result.kind = ((word >> 1) & 0x3FF) == 146 ? Kind::Mtmsr : Kind::Mtmsrd;
                            result.imm = result.ra;   // L : 1 = ne changer que les bits EE et RI
                            result.ra = 0;
                        }
                        break;
                    case 278:   // dcbt, et les autres instructions de cache : le champ rD doit etre a 0
                    case 246:   // dcbtst
                    case 54:    // dcbst
                    case 86:    // dcbf
                    case 982:   // icbi
                        if (result.rd == 0) {
                            switch ((word >> 1) & 0x3FF) {
                                case 278: result.kind = Kind::Dcbt;   break;
                                case 246: result.kind = Kind::Dcbtst; break;
                                case 54:  result.kind = Kind::Dcbst;  break;
                                case 86:  result.kind = Kind::Dcbf;   break;
                                default:  result.kind = Kind::Icbi;   break;
                            }
                        }
                        break;
                    case 1014:  // dcbz (rD = 0) ou dcbzl, alias dcbz128 (rD = 1)
                        if (result.rd == 0) {
                            result.kind = Kind::Dcbz;
                        } else if (result.rd == 1) {
                            result.kind = Kind::Dcbzl;
                            result.rd = 0;
                        }
                        break;
                    case 19:    // mfcr (FXM = 0) ou mfocrf (bit 11 = 1, FXM = champ choisi)
                    case 144:   // mtcrf (bit 11 = 0) ou mtocrf (bit 11 = 1)
                        if ((word & 0x800u) == 0) {
                            const bool one = (word & 0x00100000u) != 0;
                            const std::uint32_t fxm = (word >> 12) & 0xFF;
                            const bool single_field = (fxm & (fxm - 1)) == 0 && fxm != 0;   // mfocrf / mtocrf : un seul champ
                            if (one && !single_field) {
                                break;
                            }
                            if (((word >> 1) & 0x3FF) == 19) {
                                if (one) {
                                    result.kind = Kind::Mfocrf;
                                    result.imm = static_cast<std::int32_t>(fxm);
                                } else if (fxm == 0) {
                                    result.kind = Kind::Mfcr;
                                }
                            } else {
                                result.kind = one ? Kind::Mtocrf : Kind::Mtcrf;
                                result.imm = static_cast<std::int32_t>(fxm);
                            }
                            result.ra = 0;
                            result.rb = 0;
                        }
                        break;
                    case 598:   // sync (L = 0), lwsync (L = 1), ptesync (L = 2)
                        if (result.rd <= 2 && result.ra == 0 && result.rb == 0) {
                            result.kind = Kind::Sync;
                            result.imm = result.rd;
                            result.rd = 0;
                        }
                        break;
                    case 854:   // eieio
                        if (result.rd == 0 && result.ra == 0 && result.rb == 0) {
                            result.kind = Kind::Eieio;
                        }
                        break;
                    case 32:  // cmplw (L = 0) ou cmpld (L = 1) : comparaison sans signe
                        result.kind = ((word >> 21) & 0x1) == 0 ? Kind::Cmplw : Kind::Cmpld;
                        result.crf = static_cast<std::uint8_t>((word >> 23) & 0x7);
                        result.rd = 0;
                        break;
                    case 0:   // cmpw (L = 0, 32 bits) ou cmpd (L = 1, 64 bits)
                        result.kind = ((word >> 21) & 0x1) == 0 ? Kind::Cmpw : Kind::Cmpd;
                        result.crf = static_cast<std::uint8_t>((word >> 23) & 0x7);
                        result.rd = 0;
                        break;
                    default: break;
                }
                if (result.kind == Kind::Unknown) {
                    result = Instruction{};
                    result.word = word;
                }
            } else if (((word >> 1) & 0x3FF) == 150 || ((word >> 1) & 0x3FF) == 214) {   // stwcx. / stdcx. : Rc vaut toujours 1
                result.kind = ((word >> 1) & 0x3FF) == 150 ? Kind::Stwcx : Kind::Stdcx;
                result.rd = field_6_10(word);
                result.ra = field_11_15(word);
                result.rb = field_16_20(word);
            }
            if (result.kind == Kind::Unknown) {
                result = Instruction{};
                result.word = word;
            }
            break;
        }

        default:
            break;
    }

    // Les instructions de la table (optable.cpp) : on lit les champs selon leur disposition.
    if (result.kind == Kind::Unknown) {
        if (const OpInfo* op = find_op_for_word(word)) {
            if ((word & reserved_mask(op->layout)) == 0) {   // les champs reserves doivent valoir 0
                result = Instruction{};
                result.word = word;
                result.kind = op->kind;
                result.rc = (op->flags & kRc) != 0 && (word & 1u) != 0;
                result.oe = (op->flags & kOe) != 0 && (word & 0x400u) != 0;
                result.rd = field_6_10(word);
                result.ra = field_11_15(word);
                switch (op->layout) {
                    case Layout::RtRaRb:
                    case Layout::RaRsRb:
                    case Layout::MemX:
                    case Layout::CrBits:
                    case Layout::FMemX:
                    case Layout::FA3:
                    case Layout::FA2:
                    case Layout::FX2:
                        result.rb = field_16_20(word);
                        break;
                    case Layout::RtRaSimm:
                    case Layout::MemD:
                    case Layout::FMemD:
                        result.imm = simm16(word);
                        break;
                    case Layout::MemDS:   // les 2 bits de poids faible sont un code, pas un deplacement
                        result.imm = simm16(word & 0xFFFCu);
                        break;
                    case Layout::RaRsUimm:
                        result.imm = static_cast<std::int32_t>(word & 0xFFFF);
                        break;
                    case Layout::RtRbRa:
                        result.rb = field_16_20(word);
                        break;
                    case Layout::RaRsSh:
                        result.sh = field_16_20(word);   // le decalage est dans le champ rB
                        break;
                    case Layout::FA3c:
                        result.frc = static_cast<std::uint8_t>((word >> 6) & 0x1F);
                        break;
                    case Layout::FA4:
                        result.rb = field_16_20(word);
                        result.frc = static_cast<std::uint8_t>((word >> 6) & 0x1F);
                        break;
                    case Layout::FCmp:
                        result.crf = static_cast<std::uint8_t>((word >> 23) & 0x7);
                        result.rd = 0;
                        result.rb = field_16_20(word);
                        break;
                    case Layout::FmFrb:   // mtfsf : FM = bits 7-14
                        result.imm = static_cast<std::int32_t>((word >> 17) & 0xFF);
                        result.rd = 0;
                        result.ra = 0;
                        result.rb = field_16_20(word);
                        break;
                    case Layout::CrfImm:   // mtfsfi : BF = bits 6-8, U = bits 16-19
                        result.crf = static_cast<std::uint8_t>((word >> 23) & 0x7);
                        result.imm = static_cast<std::int32_t>((word >> 12) & 0xF);
                        result.rd = 0;
                        result.ra = 0;
                        break;
                    case Layout::CrfCrf:   // mcrfs : BF = bits 6-8, BFA = bits 11-13
                        result.crf = static_cast<std::uint8_t>((word >> 23) & 0x7);
                        result.bi = static_cast<std::uint8_t>((word >> 18) & 0x7);
                        result.rd = 0;
                        result.ra = 0;
                        break;
                    case Layout::RaRs:
                    case Layout::RtRa:
                    case Layout::FrD:
                    case Layout::BtOnly:
                        break;
                }
            }
        }
    }
    return result;
}

namespace {

std::string base_text(const Instruction& i) {
    if (const OpInfo* op = find_op(i.kind)) {
        return table_text(*op, i);
    }
    switch (i.kind) {
        case Kind::Cmplwi:
            return compare_text("cmplwi", i.crf, reg_name(i.ra), std::to_string(i.imm));
        case Kind::Cmpldi:
            return compare_text("cmpldi", i.crf, reg_name(i.ra), std::to_string(i.imm));
        case Kind::Cmplw:
            return compare_text("cmplw", i.crf, reg_name(i.ra), reg_name(i.rb));
        case Kind::Cmpld:
            return compare_text("cmpld", i.crf, reg_name(i.ra), reg_name(i.rb));

        case Kind::Sradi:
            return "sradi " + reg_name(i.ra) + ", " + reg_name(i.rd) + ", " + std::to_string(i.sh);
        case Kind::Mfxer:
            return "mfxer " + reg_name(i.rd);
        case Kind::Mtxer:
            return "mtxer " + reg_name(i.rd);

        case Kind::Dcbt:
        case Kind::Dcbtst:
        case Kind::Dcbst:
        case Kind::Dcbf:
        case Kind::Icbi:
        case Kind::Dcbz:
        case Kind::Dcbzl: {
            const char* name = i.kind == Kind::Dcbt ? "dcbt" : (i.kind == Kind::Dcbtst ? "dcbtst" :
                               (i.kind == Kind::Dcbst ? "dcbst" : (i.kind == Kind::Dcbf ? "dcbf" :
                               (i.kind == Kind::Icbi ? "icbi" : (i.kind == Kind::Dcbz ? "dcbz" : "dcbzl")))));
            return std::string(name) + " " + (i.ra == 0 ? std::string("0") : reg_name(i.ra)) + ", " + reg_name(i.rb);
        }

        case Kind::Mfmsr:
            return "mfmsr " + reg_name(i.rd);
        case Kind::Mtmsr:
        case Kind::Mtmsrd: {
            std::string out = (i.kind == Kind::Mtmsr ? "mtmsr " : "mtmsrd ") + reg_name(i.rd);
            if (i.imm != 0) {
                out += ", 1";
            }
            return out;
        }

        case Kind::Mfspr:   // les memes noms que llvm : mfdec, mfsrr0... sinon la forme generale
            switch (i.imm) {
                case 22:  return "mfdec " + reg_name(i.rd);
                case 26:  return "mfsrr0 " + reg_name(i.rd);
                case 27:  return "mfsrr1 " + reg_name(i.rd);
                case 287: return "mfpvr " + reg_name(i.rd);
                default:  return "mfspr " + reg_name(i.rd) + ", " + std::to_string(i.imm);
            }
        case Kind::Mtspr:   // attention : le numero du SPR s'ecrit en premier
            switch (i.imm) {
                case 22:  return "mtdec " + reg_name(i.rd);
                case 26:  return "mtsrr0 " + reg_name(i.rd);
                case 27:  return "mtsrr1 " + reg_name(i.rd);
                default:  return "mtspr " + std::to_string(i.imm) + ", " + reg_name(i.rd);
            }
        case Kind::Mftb:
            if (i.imm == 269) {
                return "mftbu " + reg_name(i.rd);
            }
            return "mftb " + reg_name(i.rd) + ", " + std::to_string(i.imm);

        case Kind::Rlwimi:
            return "rlwimi " + reg_name(i.ra) + ", " + reg_name(i.rd) + ", " + std::to_string(i.sh) + ", " +
                   std::to_string(i.mb) + ", " + std::to_string(i.me);
        case Kind::Rlwnm:
            if (i.mb == 0 && i.me == 31) {
                return "rotlw " + reg_name(i.ra) + ", " + reg_name(i.rd) + ", " + reg_name(i.rb);
            }
            return "rlwnm " + reg_name(i.ra) + ", " + reg_name(i.rd) + ", " + reg_name(i.rb) + ", " +
                   std::to_string(i.mb) + ", " + std::to_string(i.me);

        // rotations 64 bits : formes courantes dans le meme ordre de priorite que llvm
        case Kind::Rldicl: {
            const std::string a = reg_name(i.ra) + ", " + reg_name(i.rd) + ", ";
            if (i.mb == 0) {
                return "rotldi " + a + std::to_string(i.sh);
            }
            if (i.sh == 0) {
                return "clrldi " + a + std::to_string(i.mb);
            }
            return "rldicl " + a + std::to_string(i.sh) + ", " + std::to_string(i.mb);
        }
        case Kind::Rldcl:
        case Kind::Rldcr: {
            const std::string a = reg_name(i.ra) + ", " + reg_name(i.rd) + ", " + reg_name(i.rb);
            if (i.kind == Kind::Rldcl) {
                return i.mb == 0 ? "rotld " + a : "rldcl " + a + ", " + std::to_string(i.mb);
            }
            return "rldcr " + a + ", " + std::to_string(i.me);
        }
        case Kind::Rldicr: {
            const std::string a = reg_name(i.ra) + ", " + reg_name(i.rd) + ", ";
            if (i.sh + i.me == 63 && !i.rc) {   // pas d'alias "sldi." chez llvm
                return "sldi " + a + std::to_string(i.sh);
            }
            return "rldicr " + a + std::to_string(i.sh) + ", " + std::to_string(i.me);
        }
        case Kind::Rldic:
        case Kind::Rldimi:
            return std::string(i.kind == Kind::Rldic ? "rldic " : "rldimi ") + reg_name(i.ra) + ", " +
                   reg_name(i.rd) + ", " + std::to_string(i.sh) + ", " + std::to_string(i.mb);

        case Kind::Addi:
            if (i.ra == 0) {
                return "li " + reg_name(i.rd) + ", " + std::to_string(i.imm);
            }
            return "addi " + reg_name(i.rd) + ", " + reg_name(i.ra) + ", " + std::to_string(i.imm);

        case Kind::Addis:
            if (i.ra == 0) {
                std::ostringstream out;
                out << "lis " << reg_name(i.rd) << ", 0x" << std::hex << (static_cast<std::uint32_t>(i.imm) & 0xFFFF);
                return out.str();
            }
            return "addis " + reg_name(i.rd) + ", " + reg_name(i.ra) + ", " + std::to_string(i.imm);

        case Kind::Sub:   // rD = rB - rA dans le mot : on ecrit "sub rD, rB, rA" (avec OE : "subfo rD, rA, rB", comme llvm)
            if (i.oe) {
                return "subf " + reg_name(i.rd) + ", " + reg_name(i.ra) + ", " + reg_name(i.rb);
            }
            return "sub " + reg_name(i.rd) + ", " + reg_name(i.rb) + ", " + reg_name(i.ra);

        case Kind::Mullw:
            return "mullw " + reg_name(i.rd) + ", " + reg_name(i.ra) + ", " + reg_name(i.rb);

        case Kind::Rlwinm: {
            // formes courantes, dans le meme ordre de priorite que llvm ; sinon la forme generale
            const std::string a = reg_name(i.ra) + ", " + reg_name(i.rd) + ", ";
            if (i.sh == 0 && i.me == 31 && i.mb > 0) {
                return "clrlwi " + a + std::to_string(i.mb);
            }
            if (i.mb == 0 && i.me == 31 - i.sh && !i.rc) {   // llvm n'a pas d'alias "slwi." ni "srwi." (avec point)
                return "slwi " + a + std::to_string(i.sh);
            }
            if (i.me == 31 && i.mb > 0 && i.sh == 32 - i.mb && !i.rc) {
                return "srwi " + a + std::to_string(i.mb);
            }
            if (i.mb == 0 && i.me == 31) {
                return "rotlwi " + a + std::to_string(i.sh);
            }
            return "rlwinm " + a + std::to_string(i.sh) + ", " + std::to_string(i.mb) + ", " +
                   std::to_string(i.me);
        }

        case Kind::Or:   // rd est le champ rS, ra la destination
            if (i.rd == i.rb) {
                return "mr " + reg_name(i.ra) + ", " + reg_name(i.rd);
            }
            return "or " + reg_name(i.ra) + ", " + reg_name(i.rd) + ", " + reg_name(i.rb);

        case Kind::Cmpw:
        case Kind::Cmpd: {
            const std::string name = i.kind == Kind::Cmpw ? "cmpw" : "cmpd";
            if (i.crf == 0) {
                return name + " " + reg_name(i.ra) + ", " + reg_name(i.rb);
            }
            return name + " cr" + std::to_string(static_cast<int>(i.crf)) + ", " + reg_name(i.ra) + ", " +
                   reg_name(i.rb);
        }

        case Kind::Stwu:
            return "stwu " + reg_name(i.rd) + ", " + std::to_string(i.imm) + "(" + reg_name(i.ra) + ")";

        case Kind::Mflr:
            return "mflr " + reg_name(i.rd);

        case Kind::Mtlr:
            return "mtlr " + reg_name(i.rd);

        case Kind::Mfctr:
            return "mfctr " + reg_name(i.rd);

        case Kind::Mtctr:
            return "mtctr " + reg_name(i.rd);

        case Kind::Lbz:
            return "lbz " + reg_name(i.rd) + ", " + std::to_string(i.imm) + "(" + reg_name(i.ra) + ")";

        case Kind::Stb:
            return "stb " + reg_name(i.rd) + ", " + std::to_string(i.imm) + "(" + reg_name(i.ra) + ")";

        case Kind::Lwzx:
        case Kind::Stwx:
        case Kind::Lbzx:
        case Kind::Stbx: {
            const char* name = i.kind == Kind::Lwzx ? "lwzx " : (i.kind == Kind::Stwx ? "stwx " :
                               (i.kind == Kind::Lbzx ? "lbzx " : "stbx "));
            // rA = 0 veut dire le nombre 0 : on l'ecrit "0" (comme llvm) pour ne pas laisser croire a r0
            return std::string(name) + reg_name(i.rd) + ", " + (i.ra == 0 ? std::string("0") : reg_name(i.ra)) +
                   ", " + reg_name(i.rb);
        }

        case Kind::Add:
            return "add " + reg_name(i.rd) + ", " + reg_name(i.ra) + ", " + reg_name(i.rb);

        case Kind::Ori:
            if (i.rd == 0 && i.ra == 0 && i.imm == 0) {
                return "nop";
            }
            return "ori " + reg_name(i.ra) + ", " + reg_name(i.rd) + ", " + std::to_string(i.imm);

        case Kind::Lwz:
            return "lwz " + reg_name(i.rd) + ", " + std::to_string(i.imm) + "(" + reg_name(i.ra) + ")";

        case Kind::Stw:
            return "stw " + reg_name(i.rd) + ", " + std::to_string(i.imm) + "(" + reg_name(i.ra) + ")";

        case Kind::B: {
            std::string name = "b";
            if (i.link) name += "l";
            if (i.absolute) {
                std::ostringstream out;
                out << name << "a 0x" << std::hex << static_cast<std::uint32_t>(i.imm);
                return out.str();
            }
            return name + " " + signed_offset(i.imm);
        }

        case Kind::Cmpwi:
        case Kind::Cmpdi: {
            const std::string name = i.kind == Kind::Cmpwi ? "cmpwi" : "cmpdi";
            if (i.crf == 0) {
                return name + " " + reg_name(i.ra) + ", " + std::to_string(i.imm);
            }
            return name + " cr" + std::to_string(static_cast<int>(i.crf)) + ", " + reg_name(i.ra) +
                   ", " + std::to_string(i.imm);
        }

        case Kind::Ld:
            return "ld " + reg_name(i.rd) + ", " + std::to_string(i.imm) + "(" + reg_name(i.ra) + ")";

        case Kind::Std:
            return "std " + reg_name(i.rd) + ", " + std::to_string(i.imm) + "(" + reg_name(i.ra) + ")";

        case Kind::Bc:
            return branch_text(i, "") + " " + branch_leading(i) +
                   (i.absolute ? std::to_string(i.imm) : signed_offset(i.imm));

        case Kind::Bclr:
            return branch_text(i, "lr") + branch_leading_only(i);

        case Kind::Bcctr:
            return branch_text(i, "ctr") + branch_leading_only(i);

        case Kind::Blr:
            return i.link ? "blrl" : "blr";

        case Kind::Bctr:
            return i.link ? "bctrl" : "bctr";

        case Kind::Mcrf:
            return "mcrf cr" + std::to_string(static_cast<int>(i.crf)) + ", cr" + std::to_string(static_cast<int>(i.bi));

        case Kind::Mfcr:
            return "mfcr " + reg_name(i.rd);
        case Kind::Mfocrf:
            return "mfocrf " + reg_name(i.rd) + ", " + std::to_string(i.imm);
        case Kind::Mtcrf:
            if (i.imm == 255) {
                return "mtcr " + reg_name(i.rd);
            }
            return "mtcrf " + std::to_string(i.imm) + ", " + reg_name(i.rd);
        case Kind::Mtocrf:
            return "mtocrf " + std::to_string(i.imm) + ", " + reg_name(i.rd);

        case Kind::Sc:
            return "sc";
        case Kind::Sync:
            return i.imm == 0 ? "sync" : (i.imm == 1 ? "lwsync" : "ptesync");
        case Kind::Eieio:
            return "eieio";
        case Kind::Isync:
            return "isync";

        case Kind::Stwcx:
        case Kind::Stdcx:
            return std::string(i.kind == Kind::Stwcx ? "stwcx. " : "stdcx. ") + reg_name(i.rd) + ", " +
                   (i.ra == 0 ? std::string("0") : reg_name(i.ra)) + ", " + reg_name(i.rb);

        default:   // Unknown, ou une instruction de la table deja traitee plus haut
            break;
    }

    std::ostringstream out;
    out << ".long 0x" << std::hex << std::setw(8) << std::setfill('0') << i.word;
    return out.str();
}

}  // namespace

std::string to_text(const Instruction& i) {
    std::string text = base_text(i);
    if (i.rc || i.oe) {   // le suffixe se glisse apres le nom : "add" -> "addo." ; "mr r3, r4" -> "mr. r3, r4"
        const std::string suffix = std::string(i.oe ? "o" : "") + (i.rc ? "." : "");
        const std::size_t space = text.find(' ');
        if (space == std::string::npos) {
            text += suffix;
        } else {
            text.insert(space, suffix);
        }
    }
    return text;
}

}  // namespace nerith::ppc
