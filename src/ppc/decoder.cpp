#include "nerith/ppc/decoder.h"

#include <iomanip>
#include <sstream>

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

}  // namespace

Instruction decode(std::uint32_t word) {
    Instruction result;
    result.word = word;

    switch (opcode_of(word)) {
        case 11:   // cmpwi : seulement la forme 32 bits (L = 0)
            if (((word >> 21) & 0x1) == 0) {
                result.kind = Kind::Cmpwi;
                result.crf = static_cast<std::uint8_t>((word >> 23) & 0x7);
                result.ra = field_11_15(word);
                result.imm = simm16(word);
            }
            break;

        case 16:   // bc : seulement beq/bne/blt/bgt/ble/bge, sans AA ni LK
            if ((word & 0x3) == 0 && (field_6_10(word) == 12 || field_6_10(word) == 4)) {
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

        case 19:   // blr : seulement la forme exacte "toujours, sans sauvegarde de LR"
            if ((word & 0x03FFFFFF) == 0x02800020) {   // BO = 20, BI = 0, XO = 16, LK = 0
                result.kind = Kind::Blr;
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
                    case 339:   // mfspr : seulement LR (numero 8)
                    case 467:   // mtspr : seulement LR
                        {
                            const std::uint32_t field = (word >> 11) & 0x3FF;   // moities inversees
                            const std::uint32_t spr = ((field & 0x1F) << 5) | (field >> 5);
                            if (spr == 8) {
                                result.kind = ((word >> 1) & 0x3FF) == 339 ? Kind::Mflr : Kind::Mtlr;
                                result.ra = 0;
                                result.rb = 0;
                            }
                        }
                        break;
                    case 0:   // cmpw : seulement la forme 32 bits (L = 0)
                        if (((word >> 21) & 0x1) == 0) {
                            result.kind = Kind::Cmpw;
                            result.crf = static_cast<std::uint8_t>((word >> 23) & 0x7);
                            result.rd = 0;
                        }
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
    return result;
}

std::string to_text(const Instruction& i) {
    switch (i.kind) {
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
            if (i.crf == 0) {
                return "cmpw " + reg_name(i.ra) + ", " + reg_name(i.rb);
            }
            return "cmpw cr" + std::to_string(static_cast<int>(i.crf)) + ", " + reg_name(i.ra) + ", " +
                   reg_name(i.rb);

        case Kind::Stwu:
            return "stwu " + reg_name(i.rd) + ", " + std::to_string(i.imm) + "(" + reg_name(i.ra) + ")";

        case Kind::Mflr:
            return "mflr " + reg_name(i.rd);

        case Kind::Mtlr:
            return "mtlr " + reg_name(i.rd);

        case Kind::Lbz:
            return "lbz " + reg_name(i.rd) + ", " + std::to_string(i.imm) + "(" + reg_name(i.ra) + ")";

        case Kind::Stb:
            return "stb " + reg_name(i.rd) + ", " + std::to_string(i.imm) + "(" + reg_name(i.ra) + ")";

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
            if (i.crf == 0) {
                return "cmpwi " + reg_name(i.ra) + ", " + std::to_string(i.imm);
            }
            return "cmpwi cr" + std::to_string(static_cast<int>(i.crf)) + ", " + reg_name(i.ra) +
                   ", " + std::to_string(i.imm);

        case Kind::Bc: {
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
            return "blr";

        case Kind::Unknown:
            break;
    }

    std::ostringstream out;
    out << ".long 0x" << std::hex << std::setw(8) << std::setfill('0') << i.word;
    return out.str();
}

}  // namespace nerith::ppc
