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
    }
    return name;
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

        case 20:   // rlwimi (Rc = 0) : insere des bits tournes dans rA ; rd est rS
        case 23:   // rlwnm (Rc = 0) : comme rlwinm, mais la rotation vient de rB
            if ((word & 0x1) == 0) {
                result.kind = opcode_of(word) == 20 ? Kind::Rlwimi : Kind::Rlwnm;
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

        case 30:   // rotations 64 bits (forme MD) : rldicl, rldicr, rldic, rldimi ; Rc = 0
            if ((word & 0x1) == 0 && ((word >> 2) & 0x7) < 4) {
                static const Kind kKinds[] = {Kind::Rldicl, Kind::Rldicr, Kind::Rldic, Kind::Rldimi};
                result.kind = kKinds[(word >> 2) & 0x7];
                result.rd = field_6_10(word);
                result.ra = field_11_15(word);
                // sh : 6 bits, le bit de poids fort est tout a droite (bit 30) ; mb/me : 6 bits "retournes"
                result.sh = static_cast<std::uint8_t>((((word >> 1) & 0x1) << 5) | field_16_20(word));
                const unsigned raw = (word >> 5) & 0x3F;
                const std::uint8_t mask_bit = static_cast<std::uint8_t>(((raw & 1u) << 5) | (raw >> 1));
                if (result.kind == Kind::Rldicr) {
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

        case 16:   // bc : beq/bne/blt/bgt/ble/bge (BO = 12 ou 4) et bdnz/bdz (BO = 16 ou 18, BI = 0), sans AA ni LK
            if ((word & 0x3) == 0 &&
                (field_6_10(word) == 12 || field_6_10(word) == 4 ||
                 ((field_6_10(word) == 16 || field_6_10(word) == 18) && field_11_15(word) == 0))) {
                result.kind = Kind::Bc;
                result.bo = field_6_10(word);
                result.bi = field_11_15(word);
                result.imm = static_cast<std::int32_t>(static_cast<std::int16_t>(word & 0xFFFC));
            }
            break;

        case 21:   // rlwinm : seulement la forme Rc = 0 ; rd est rS, ra est la destination
            if ((word & 0x1) == 0) {
                result.kind = Kind::Rlwinm;
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

        case 19:   // blr, blrl, bctr, bctrl : seulement les formes "toujours" (BO = 20, BI = 0)
            if ((word & 0x03FFFFFE) == 0x02800020) {         // XO = 16 : saute a LR
                result.kind = Kind::Blr;
                result.link = (word & 0x1) != 0;
            } else if ((word & 0x03FFFFFE) == 0x02800420) {  // XO = 528 : saute a CTR
                result.kind = Kind::Bctr;
                result.link = (word & 0x1) != 0;
            }
            break;

        case 31:   // instructions "registre, registre" : seulement les formes simples (Rc = 0)
            if ((word & 0x1) == 0) {
                result.rd = field_6_10(word);
                result.ra = field_11_15(word);
                result.rb = field_16_20(word);
                switch ((word >> 1) & 0x3FF) {   // champ XO (avec le bit OE : on refuse OE = 1)
                    case 266: result.kind = Kind::Add;   break;
                    case 40:  result.kind = Kind::Sub;   break;
                    case 235: result.kind = Kind::Mullw; break;
                    case 444: result.kind = Kind::Or;    break;
                    case 23:  result.kind = Kind::Lwzx;  break;
                    case 151: result.kind = Kind::Stwx;  break;
                    case 87:  result.kind = Kind::Lbzx;  break;
                    case 215: result.kind = Kind::Stbx;  break;
                    case 339:   // mfspr : seulement LR (numero 8) et CTR (numero 9)
                    case 467:   // mtspr : seulement LR et CTR
                        {
                            const std::uint32_t field = (word >> 11) & 0x3FF;   // moities inversees
                            const std::uint32_t spr = ((field & 0x1F) << 5) | (field >> 5);
                            const bool read = ((word >> 1) & 0x3FF) == 339;
                            if (spr == 8) {
                                result.kind = read ? Kind::Mflr : Kind::Mtlr;
                            } else if (spr == 9) {
                                result.kind = read ? Kind::Mfctr : Kind::Mtctr;
                            }
                            result.ra = 0;
                            result.rb = 0;
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
            }
            break;

        default:
            break;
    }

    // Les instructions de la table (optable.cpp) : on lit les champs selon leur disposition.
    if (result.kind == Kind::Unknown) {
        if (const OpInfo* op = find_op_for_word(word)) {
            const bool needs_zero_rb = op->layout == Layout::RaRs || op->layout == Layout::RtRa;
            if (!(needs_zero_rb && field_16_20(word) != 0)) {
                result = Instruction{};
                result.word = word;
                result.kind = op->kind;
                result.rd = field_6_10(word);
                result.ra = field_11_15(word);
                switch (op->layout) {
                    case Layout::RtRaRb:
                    case Layout::RaRsRb:
                    case Layout::MemX:
                        result.rb = field_16_20(word);
                        break;
                    case Layout::RtRaSimm:
                    case Layout::MemD:
                        result.imm = simm16(word);
                        break;
                    case Layout::MemDS:   // les 2 bits de poids faible sont un code, pas un deplacement
                        result.imm = simm16(word & 0xFFFCu);
                        break;
                    case Layout::RaRsUimm:
                        result.imm = static_cast<std::int32_t>(word & 0xFFFF);
                        break;
                    case Layout::RaRs:
                    case Layout::RtRa:
                        break;
                }
            }
        }
    }
    return result;
}

std::string to_text(const Instruction& i) {
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
        case Kind::Rldicr: {
            const std::string a = reg_name(i.ra) + ", " + reg_name(i.rd) + ", ";
            if (i.sh + i.me == 63) {
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

        case Kind::Sub:   // rD = rB - rA dans le mot : on ecrit "sub rD, rB, rA"
            return "sub " + reg_name(i.rd) + ", " + reg_name(i.rb) + ", " + reg_name(i.ra);

        case Kind::Mullw:
            return "mullw " + reg_name(i.rd) + ", " + reg_name(i.ra) + ", " + reg_name(i.rb);

        case Kind::Rlwinm: {
            // formes courantes, dans le meme ordre de priorite que llvm ; sinon la forme generale
            const std::string a = reg_name(i.ra) + ", " + reg_name(i.rd) + ", ";
            if (i.sh == 0 && i.me == 31 && i.mb > 0) {
                return "clrlwi " + a + std::to_string(i.mb);
            }
            if (i.mb == 0 && i.me == 31 - i.sh) {
                return "slwi " + a + std::to_string(i.sh);
            }
            if (i.me == 31 && i.mb > 0 && i.sh == 32 - i.mb) {
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

        case Kind::Bc: {
            if (i.bo == 16 || i.bo == 18) {   // selon CTR, pas selon CR
                return std::string(i.bo == 16 ? "bdnz " : "bdz ") + signed_offset(i.imm);
            }
            // bo = 12 : saute si le bit vaut 1 (lt, gt, eq) ; bo = 4 : saute s'il vaut 0 (ge, le, ne)
            static const char* const kTrue[]  = {"blt", "bgt", "beq", "bso"};
            static const char* const kFalse[] = {"bge", "ble", "bne", "bns"};
            std::string name = (i.bo == 12 ? kTrue : kFalse)[i.bi % 4];
            if (i.bi >= 4) {
                name += " cr" + std::to_string(static_cast<int>(i.bi / 4)) + ",";
            }
            return name + " " + signed_offset(i.imm);
        }

        case Kind::Blr:
            return i.link ? "blrl" : "blr";

        case Kind::Bctr:
            return i.link ? "bctrl" : "bctr";

        default:   // Unknown, ou une instruction de la table deja traitee plus haut
            break;
    }

    std::ostringstream out;
    out << ".long 0x" << std::hex << std::setw(8) << std::setfill('0') << i.word;
    return out.str();
}

}  // namespace nerith::ppc
