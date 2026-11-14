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

        case 31:   // add : seulement la forme simple (XO = 266, OE = 0, Rc = 0)
            if (((word >> 1) & 0x3FF) == 266 && (word & 0x1) == 0) {
                result.kind = Kind::Add;
                result.rd = field_6_10(word);
                result.ra = field_11_15(word);
                result.rb = field_16_20(word);
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
