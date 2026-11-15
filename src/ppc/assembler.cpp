#include "nerith/ppc/assembler.h"

#include <cctype>
#include <map>

#include "nerith/ppc/optable.h"

namespace nerith::ppc {

AssemblyError::AssemblyError(std::size_t line, const std::string& message)
    : std::runtime_error("ligne " + std::to_string(line) + " : " + message), line_(line) {}

namespace {

// Une instruction lue dans le texte, pas encore transformee en mot.
struct Line {
    std::size_t number = 0;                // numero de ligne, pour les messages d'erreur
    std::size_t address = 0;               // adresse de l'instruction (4 par instruction)
    std::string mnemonic;                  // "li", "bne"... en minuscules
    std::vector<std::string> operands;     // "r3", "5", "8(r1)", "loop"...
};

bool is_space(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

bool is_digit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

bool is_word_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '.';
}

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && is_space(text[begin])) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && is_space(text[end - 1])) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::string to_lower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

// "r0" a "r31" : la lettre r suivie de chiffres
bool looks_like_register(const std::string& word) {
    if (word.size() < 2 || (word[0] != 'r' && word[0] != 'R')) {
        return false;
    }
    for (std::size_t i = 1; i < word.size(); i++) {
        if (!is_digit(word[i])) {
            return false;
        }
    }
    return true;
}

std::uint32_t parse_register(const Line& line, const std::string& text) {
    if (!looks_like_register(text)) {
        throw AssemblyError(line.number, "registre attendu (r0 a r31), trouve \"" + text + "\"");
    }
    if (text.size() > 3) {
        throw AssemblyError(line.number, "registre inexistant : " + text);
    }
    const int number = std::stoi(text.substr(1));
    if (number > 31) {
        throw AssemblyError(line.number, "registre inexistant : " + text + " (r0 a r31 seulement)");
    }
    return static_cast<std::uint32_t>(number);
}

// "f0" a "f31", ou simplement un nombre de 0 a 31 (comme l'ecrit llvm)
std::uint32_t parse_fregister(const Line& line, const std::string& text) {
    std::string digits = text;
    if (!digits.empty() && (digits[0] == 'f' || digits[0] == 'F')) {
        digits = digits.substr(1);
    }
    bool ok = !digits.empty() && digits.size() <= 2;
    for (const char c : digits) {
        ok = ok && is_digit(c);
    }
    if (!ok || std::stoi(digits) > 31) {
        throw AssemblyError(line.number, "registre flottant attendu (f0 a f31), trouve \"" + text + "\"");
    }
    return static_cast<std::uint32_t>(std::stoi(digits));
}

// Registre de base d'un acces "x" : "0" (le nombre 0) ou un registre. Dans le mot, 0 est le meme code que r0.
std::uint32_t parse_index_base(const Line& line, const std::string& text) {
    return text == "0" ? 0u : parse_register(line, text);
}

// "cr0" a "cr7"
bool looks_like_cr(const std::string& word) {
    return word.size() == 3 && word[0] == 'c' && word[1] == 'r' && word[2] >= '0' && word[2] <= '7';
}

// Nombre decimal ou hexadecimal, avec signe facultatif ("-12", "+8", "0x10").
bool parse_number(const std::string& text, long long& value) {
    std::size_t i = 0;
    bool negative = false;
    if (i < text.size() && (text[i] == '-' || text[i] == '+')) {
        negative = text[i] == '-';
        ++i;
    }
    int base = 10;
    if (i + 1 < text.size() && text[i] == '0' && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
        base = 16;
        i += 2;
    }
    if (i >= text.size() || text.size() - i > 10) {
        return false;
    }
    long long result = 0;
    for (; i < text.size(); i++) {
        const char c = text[i];
        int digit = 0;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (base == 16 && c >= 'a' && c <= 'f') {
            digit = 10 + (c - 'a');
        } else if (base == 16 && c >= 'A' && c <= 'F') {
            digit = 10 + (c - 'A');
        } else {
            return false;
        }
        result = result * base + digit;
    }
    value = negative ? -result : result;
    return true;
}

long long parse_value(const Line& line, const std::string& text, long long min, long long max) {
    long long value = 0;
    if (!parse_number(text, value)) {
        throw AssemblyError(line.number, "nombre attendu, trouve \"" + text + "\"");
    }
    if (value < min || value > max) {
        throw AssemblyError(line.number, "nombre hors limites (" + std::to_string(min) + " a " +
                                             std::to_string(max) + ") : " + text);
    }
    return value;
}

// "cr3" ou "3" : numero d'un champ de CR (0 a 7)
std::uint32_t parse_cr_field(const Line& line, const std::string& text) {
    const std::string lower = to_lower(text);
    if (looks_like_cr(lower)) {
        return static_cast<std::uint32_t>(lower[2] - '0');
    }
    return static_cast<std::uint32_t>(parse_value(line, text, 0, 7));
}

void expect_operands(const Line& line, std::size_t count) {
    if (line.operands.size() != count) {
        throw AssemblyError(line.number, "\"" + line.mnemonic + "\" attend " + std::to_string(count) +
                                             " operande(s), " + std::to_string(line.operands.size()) +
                                             " trouve(s)");
    }
}

// "8(r1)" -> deplacement 8 et registre r1
void parse_memory_operand(const Line& line, const std::string& text, long long& offset,
                          std::uint32_t& reg) {
    const std::size_t open = text.find('(');
    if (open == std::string::npos || text.back() != ')') {
        throw AssemblyError(line.number, "forme attendue : deplacement(registre), par exemple 8(r1)");
    }
    offset = parse_value(line, trim(text.substr(0, open)), -32768, 32767);
    reg = parse_register(line, trim(text.substr(open + 1, text.size() - open - 2)));
}

// Calcule le deplacement en octets d'un saut depuis son operande (etiquette ou "+8").
long long branch_offset(const Line& line, const std::string& target,
                        const std::map<std::string, std::size_t>& labels) {
    long long value = 0;
    if (!target.empty() && (target[0] == '+' || target[0] == '-' || is_digit(target[0]))) {
        if (!parse_number(target, value)) {
            throw AssemblyError(line.number, "deplacement invalide : " + target);
        }
        return value;
    }
    const auto found = labels.find(target);
    if (found == labels.end()) {
        throw AssemblyError(line.number, "etiquette inconnue : " + target);
    }
    return static_cast<long long>(found->second) - static_cast<long long>(line.address);
}

std::uint32_t encode_branch(const Line& line, long long offset, std::uint32_t link) {
    if (offset % 4 != 0) {
        throw AssemblyError(line.number, "le deplacement d'un saut doit etre un multiple de 4");
    }
    if (offset < -33554432 || offset > 33554428) {
        throw AssemblyError(line.number, "saut trop loin");
    }
    return (18u << 26) | (static_cast<std::uint32_t>(offset) & 0x03FFFFFCu) | link;
}

// Les "noms de base" des sauts conditionnels. Un nom complet = base + [lr | ctr] + [l] + [a], par exemple
// "beq", "beql", "beqlr", "beqctrl", "bdnzt", "bcla".
struct BranchBase {
    const char* name;
    int bo;          // valeur de BO, ou -1 pour "bc" (BO est alors le premier operande)
    int bit;         // bit dans le champ de CR (lt = 0, gt = 1, eq = 2, so = 3), ou -1 s'il n'y en a pas
    bool explicit_bi;   // le numero de bit BI est le premier operande
};

constexpr BranchBase kBranchBases[] = {
    {"blt", 12, 0, false}, {"bgt", 12, 1, false}, {"beq", 12, 2, false}, {"bso", 12, 3, false}, {"bun", 12, 3, false},
    {"bge", 4, 0, false},  {"bnl", 4, 0, false},  {"ble", 4, 1, false},  {"bng", 4, 1, false},
    {"bne", 4, 2, false},  {"bns", 4, 3, false},  {"bnu", 4, 3, false},
    {"bdnz", 16, -1, false}, {"bdz", 18, -1, false},
    {"bdnzt", 8, -1, true},  {"bdnzf", 0, -1, true}, {"bdzt", 10, -1, true}, {"bdzf", 2, -1, true},
    {"bt", 12, -1, true},    {"bf", 4, -1, true},
    {"bc", -1, -1, true},
};

// Decoupe un mnemonique de saut conditionnel. target : 0 = bc (vers une adresse), 1 = vers LR, 2 = vers CTR.
bool split_branch_mnemonic(const std::string& m, const BranchBase*& base, int& target, bool& link, bool& absolute) {
    for (const BranchBase& candidate : kBranchBases) {
        const std::string name = candidate.name;
        if (m.compare(0, name.size(), name) != 0) {
            continue;
        }
        const std::string rest = m.substr(name.size());
        base = &candidate;
        if (rest.empty() || rest == "l" || rest == "a" || rest == "la") {
            target = 0;
            link = rest.find('l') != std::string::npos;
            absolute = rest.find('a') != std::string::npos;
            return true;
        }
        if (rest == "lr" || rest == "lrl") {
            target = 1;
            link = rest.size() == 3;
            absolute = false;
            return true;
        }
        if (rest == "ctr" || rest == "ctrl") {
            target = 2;
            link = rest.size() == 4;
            absolute = false;
            return true;
        }
    }
    return false;
}

// rlwinm rA, rS, SH, MB, ME
std::uint32_t rlwinm_word(std::uint32_t ra, std::uint32_t rs, std::uint32_t sh, std::uint32_t mb,
                          std::uint32_t me) {
    return (21u << 26) | (rs << 21) | (ra << 16) | (sh << 11) | (mb << 6) | (me << 1);
}

// "[crN,]" au debut des operandes : renvoie le numero du champ et dit combien d'operandes il a pris.
std::uint32_t parse_optional_cr(const Line& line, std::size_t& first) {
    first = 0;
    if (!line.operands.empty() && looks_like_cr(to_lower(line.operands[0]))) {
        first = 1;
        return static_cast<std::uint32_t>(line.operands[0][2] - '0');
    }
    return 0;
}

// Rotations 64 bits (forme MD) : rldicl, rldicr, rldic, rldimi.
// Les 6 bits de sh sont coupes en deux (5 bits + 1 bit a droite), et ceux de mb/me sont "retournes".
std::uint32_t rld_word(std::uint32_t xo, std::uint32_t ra, std::uint32_t rs, std::uint32_t sh, std::uint32_t mask_bit) {
    const std::uint32_t raw = ((mask_bit & 0x1Fu) << 1) | (mask_bit >> 5);
    return (30u << 26) | (rs << 21) | (ra << 16) | ((sh & 0x1Fu) << 11) | (raw << 5) | (xo << 2) |
           ((sh >> 5) << 1);
}

// Les instructions de la table (optable.cpp) : mot de base + champs selon leur disposition.
std::uint32_t encode_table_op(const Line& line, const OpInfo& info) {
    const auto& op = line.operands;
    std::uint32_t word = info.opcode << 26;
    if (info.opcode == 31 || info.opcode == 19 || info.opcode == 59 || info.opcode == 63) {
        word |= static_cast<std::uint32_t>(info.xo) << 1;
    } else if (info.opcode == 58 || info.opcode == 62) {
        word |= static_cast<std::uint32_t>(info.xo);
    }
    switch (info.layout) {
        case Layout::RtRaRb:
            expect_operands(line, 3);
            return word | (parse_register(line, op[0]) << 21) | (parse_register(line, op[1]) << 16) |
                   (parse_register(line, op[2]) << 11);
        case Layout::RaRsRb:   // rA, rS, rB : la destination est rA
            expect_operands(line, 3);
            return word | (parse_register(line, op[1]) << 21) | (parse_register(line, op[0]) << 16) |
                   (parse_register(line, op[2]) << 11);
        case Layout::RaRs:
            expect_operands(line, 2);
            return word | (parse_register(line, op[1]) << 21) | (parse_register(line, op[0]) << 16);
        case Layout::RaRsSh:   // rA, rS, SH (0 a 31)
            expect_operands(line, 3);
            return word | (parse_register(line, op[1]) << 21) | (parse_register(line, op[0]) << 16) |
                   (static_cast<std::uint32_t>(parse_value(line, op[2], 0, 31)) << 11);
        case Layout::RtRbRa:   // "subc rD, rB, rA" : les deux derniers registres sont dans l'ordre inverse du mot
            expect_operands(line, 3);
            return word | (parse_register(line, op[0]) << 21) | (parse_register(line, op[2]) << 16) |
                   (parse_register(line, op[1]) << 11);
        case Layout::RtRa:
            expect_operands(line, 2);
            return word | (parse_register(line, op[0]) << 21) | (parse_register(line, op[1]) << 16);
        case Layout::RtRaSimm:
            expect_operands(line, 3);
            return word | (parse_register(line, op[0]) << 21) | (parse_register(line, op[1]) << 16) |
                   (static_cast<std::uint32_t>(parse_value(line, op[2], -32768, 32767)) & 0xFFFFu);
        case Layout::RaRsUimm:
            expect_operands(line, 3);
            return word | (parse_register(line, op[1]) << 21) | (parse_register(line, op[0]) << 16) |
                   static_cast<std::uint32_t>(parse_value(line, op[2], 0, 65535));
        case Layout::MemD:
        case Layout::MemDS: {
            expect_operands(line, 2);
            long long offset = 0;
            std::uint32_t base = 0;
            parse_memory_operand(line, op[1], offset, base);
            if (info.layout == Layout::MemDS && offset % 4 != 0) {
                throw AssemblyError(line.number, "le deplacement de " + line.mnemonic + " doit etre un multiple de 4");
            }
            return word | (parse_register(line, op[0]) << 21) | (base << 16) |
                   (static_cast<std::uint32_t>(offset) & (info.layout == Layout::MemDS ? 0xFFFCu : 0xFFFFu));
        }
        case Layout::MemX:   // rD, rA (ou 0), rB
            expect_operands(line, 3);
            return word | (parse_register(line, op[0]) << 21) | (parse_index_base(line, op[1]) << 16) |
                   (parse_register(line, op[2]) << 11);
        case Layout::FMemD: {   // fD, d(rA)
            expect_operands(line, 2);
            long long offset = 0;
            std::uint32_t base = 0;
            parse_memory_operand(line, op[1], offset, base);
            return word | (parse_fregister(line, op[0]) << 21) | (base << 16) |
                   (static_cast<std::uint32_t>(offset) & 0xFFFFu);
        }
        case Layout::FMemX:   // fD, rA (ou 0), rB
            expect_operands(line, 3);
            return word | (parse_fregister(line, op[0]) << 21) | (parse_index_base(line, op[1]) << 16) |
                   (parse_register(line, op[2]) << 11);
        case Layout::FA3:   // fD, fA, fB
            expect_operands(line, 3);
            return word | (parse_fregister(line, op[0]) << 21) | (parse_fregister(line, op[1]) << 16) |
                   (parse_fregister(line, op[2]) << 11);
        case Layout::FA3c:   // fD, fA, fC
            expect_operands(line, 3);
            return word | (parse_fregister(line, op[0]) << 21) | (parse_fregister(line, op[1]) << 16) |
                   (parse_fregister(line, op[2]) << 6);
        case Layout::FA4:   // fD, fA, fC, fB
            expect_operands(line, 4);
            return word | (parse_fregister(line, op[0]) << 21) | (parse_fregister(line, op[1]) << 16) |
                   (parse_fregister(line, op[2]) << 6) | (parse_fregister(line, op[3]) << 11);
        case Layout::FA2:
        case Layout::FX2:   // fD, fB
            expect_operands(line, 2);
            return word | (parse_fregister(line, op[0]) << 21) | (parse_fregister(line, op[1]) << 11);
        case Layout::FCmp: {   // [crD,] fA, fB
            std::size_t first = 0;
            const std::uint32_t crf = parse_optional_cr(line, first);
            if (op.size() != first + 2) {
                throw AssemblyError(line.number, "forme attendue : " + line.mnemonic + " [crN,] fA, fB");
            }
            return word | (crf << 23) | (parse_fregister(line, op[first]) << 16) |
                   (parse_fregister(line, op[first + 1]) << 11);
        }
        case Layout::FrD:
            expect_operands(line, 1);
            return word | (parse_fregister(line, op[0]) << 21);
        case Layout::BtOnly:
            expect_operands(line, 1);
            return word | (static_cast<std::uint32_t>(parse_value(line, op[0], 0, 31)) << 21);
        case Layout::FmFrb:   // FM, fB
            expect_operands(line, 2);
            return word | (static_cast<std::uint32_t>(parse_value(line, op[0], 0, 255)) << 17) |
                   (parse_fregister(line, op[1]) << 11);
        case Layout::CrfImm:   // crD, U
            expect_operands(line, 2);
            return word | (parse_cr_field(line, op[0]) << 23) |
                   (static_cast<std::uint32_t>(parse_value(line, op[1], 0, 15)) << 12);
        case Layout::CrfCrf:   // crD, crS
            expect_operands(line, 2);
            return word | (parse_cr_field(line, op[0]) << 23) | (parse_cr_field(line, op[1]) << 18);
        case Layout::CrBits:   // BT, BA, BB : numeros de bits de CR
            expect_operands(line, 3);
            return word | (static_cast<std::uint32_t>(parse_value(line, op[0], 0, 31)) << 21) |
                   (static_cast<std::uint32_t>(parse_value(line, op[1], 0, 31)) << 16) |
                   (static_cast<std::uint32_t>(parse_value(line, op[2], 0, 31)) << 11);
    }
    throw AssemblyError(line.number, "instruction inconnue : " + line.mnemonic);
}

// Raccourcis et formes a plusieurs operandes qui ne rentrent pas dans la table.
// Renvoie false si le mnemonique n'est pas l'un d'eux.
bool encode_extra(const Line& line, std::uint32_t& word) {
    const std::string& m = line.mnemonic;
    const auto& op = line.operands;

    if (m == "not") {   // not rA, rS = nor rA, rS, rS
        expect_operands(line, 2);
        const std::uint32_t rs = parse_register(line, op[1]);
        word = (31u << 26) | (124u << 1) | (rs << 21) | (parse_register(line, op[0]) << 16) | (rs << 11);
        return true;
    }
    if (m == "sradi") {   // sradi rA, rS, SH (0 a 63) : le bit de poids fort de SH est a droite du mot
        expect_operands(line, 3);
        const std::uint32_t sh = static_cast<std::uint32_t>(parse_value(line, op[2], 0, 63));
        word = (31u << 26) | (parse_register(line, op[1]) << 21) | (parse_register(line, op[0]) << 16) |
               ((sh & 0x1Fu) << 11) | (413u << 2) | ((sh >> 5) << 1);
        return true;
    }
    if (m == "mcrf") {   // mcrf crD, crS (avec ou sans le mot "cr")
        expect_operands(line, 2);
        word = 0x4C000000u | (parse_cr_field(line, op[0]) << 23) | (parse_cr_field(line, op[1]) << 18);
        return true;
    }
    if (m == "crclr" || m == "crset") {   // crxor / creqv sur le meme bit
        expect_operands(line, 1);
        const std::uint32_t bit = static_cast<std::uint32_t>(parse_value(line, op[0], 0, 31));
        word = 0x4C000000u | (bit << 21) | (bit << 16) | (bit << 11) | ((m == "crclr" ? 193u : 289u) << 1);
        return true;
    }
    if (m == "crnot" || m == "crmove") {   // crnor / cror avec BB = BA
        expect_operands(line, 2);
        const std::uint32_t bt = static_cast<std::uint32_t>(parse_value(line, op[0], 0, 31));
        const std::uint32_t ba = static_cast<std::uint32_t>(parse_value(line, op[1], 0, 31));
        word = 0x4C000000u | (bt << 21) | (ba << 16) | (ba << 11) | ((m == "crnot" ? 33u : 449u) << 1);
        return true;
    }
    if (m == "mfcr" || m == "mtcr") {
        expect_operands(line, 1);
        word = (m == "mfcr" ? 0x7C000026u : 0x7C0FF120u) | (parse_register(line, op[0]) << 21);
        return true;
    }
    if (m == "mtcrf" || m == "mtocrf") {   // mtcrf FXM, rS
        expect_operands(line, 2);
        word = (m == "mtcrf" ? 0x7C000120u : 0x7C100120u) | (parse_register(line, op[1]) << 21) |
               (static_cast<std::uint32_t>(parse_value(line, op[0], 0, 255)) << 12);
        return true;
    }
    if (m == "mfocrf") {   // mfocrf rD, FXM
        expect_operands(line, 2);
        word = 0x7C100026u | (parse_register(line, op[0]) << 21) |
               (static_cast<std::uint32_t>(parse_value(line, op[1], 0, 255)) << 12);
        return true;
    }
    if (m == "sc" || m == "sync" || m == "lwsync" || m == "ptesync" || m == "eieio" || m == "isync") {
        expect_operands(line, 0);
        word = m == "sc" ? 0x44000002u : (m == "sync" ? 0x7C0004ACu : (m == "lwsync" ? 0x7C2004ACu :
               (m == "ptesync" ? 0x7C4004ACu : (m == "eieio" ? 0x7C0006ACu : 0x4C00012Cu))));
        return true;
    }
    if (m == "stwcx." || m == "stdcx.") {   // stwcx. rS, rA, rB (le point fait partie du nom : Rc vaut toujours 1)
        expect_operands(line, 3);
        word = (m == "stwcx." ? 0x7C00012Du : 0x7C0001ADu) | (parse_register(line, op[0]) << 21) |
               (parse_index_base(line, op[1]) << 16) | (parse_register(line, op[2]) << 11);
        return true;
    }
    if (m == "dcbt" || m == "dcbtst" || m == "dcbst" || m == "dcbf" || m == "icbi" ||
        m == "dcbz" || m == "dcbzl" || m == "dcbz128") {   // dcbz128 : le nom Xenon de dcbzl
        expect_operands(line, 2);
        const std::uint32_t xo = m == "dcbt" ? 278u : (m == "dcbtst" ? 246u : (m == "dcbst" ? 54u :
                                 (m == "dcbf" ? 86u : (m == "icbi" ? 982u : 1014u))));
        const std::uint32_t rt = (m == "dcbzl" || m == "dcbz128") ? 1u : 0u;
        word = (31u << 26) | (rt << 21) | (parse_index_base(line, op[0]) << 16) |
               (parse_register(line, op[1]) << 11) | (xo << 1);
        return true;
    }
    if (m == "mfmsr") {
        expect_operands(line, 1);
        word = (31u << 26) | (parse_register(line, op[0]) << 21) | (83u << 1);
        return true;
    }
    if (m == "mtmsr" || m == "mtmsrd") {   // mtmsr rS ou mtmsr rS, L
        if (op.size() != 1) {
            expect_operands(line, 2);
        }
        const std::uint32_t l = op.size() == 2 ? static_cast<std::uint32_t>(parse_value(line, op[1], 0, 1)) : 0u;
        word = (31u << 26) | (parse_register(line, op[0]) << 21) | (l << 16) |
               ((m == "mtmsr" ? 146u : 178u) << 1);
        return true;
    }
    if (m == "mfdec" || m == "mfsrr0" || m == "mfsrr1" || m == "mfpvr" ||
        m == "mtdec" || m == "mtsrr0" || m == "mtsrr1") {
        expect_operands(line, 1);
        const std::uint32_t spr = (m == "mfdec" || m == "mtdec") ? 22u :
                                  (m == "mfpvr" ? 287u : ((m == "mfsrr0" || m == "mtsrr0") ? 26u : 27u));
        const std::uint32_t xo = m[1] == 'f' ? 339u : 467u;
        word = (31u << 26) | (parse_register(line, op[0]) << 21) |
               (((spr & 0x1Fu) << 5 | (spr >> 5)) << 11) | (xo << 1);
        return true;
    }
    if (m == "mfspr" || m == "mtspr") {   // mfspr rD, numero / mtspr numero, rS
        expect_operands(line, 2);
        const bool read = m == "mfspr";
        const std::uint32_t spr = static_cast<std::uint32_t>(parse_value(line, op[read ? 1 : 0], 0, 1023));
        word = (31u << 26) | (parse_register(line, op[read ? 0 : 1]) << 21) |
               (((spr & 0x1Fu) << 5 | (spr >> 5)) << 11) | ((read ? 339u : 467u) << 1);
        return true;
    }
    if (m == "mftb" || m == "mftbu") {   // mftb rD ou mftb rD, numero
        if (op.size() != 1) {
            expect_operands(line, 2);
        }
        const std::uint32_t spr = op.size() == 2 ?
            static_cast<std::uint32_t>(parse_value(line, op[1], 0, 1023)) : (m == "mftb" ? 268u : 269u);
        word = (31u << 26) | (parse_register(line, op[0]) << 21) |
               (((spr & 0x1Fu) << 5 | (spr >> 5)) << 11) | (371u << 1);
        return true;
    }
    if (m == "mfxer" || m == "mtxer") {
        expect_operands(line, 1);
        word = (m == "mfxer" ? 0x7C0102A6u : 0x7C0103A6u) | (parse_register(line, op[0]) << 21);
        return true;
    }
    if (m == "rotlw") {   // rotlw rA, rS, rB = rlwnm rA, rS, rB, 0, 31
        expect_operands(line, 3);
        word = (23u << 26) | (parse_register(line, op[1]) << 21) | (parse_register(line, op[0]) << 16) |
               (parse_register(line, op[2]) << 11) | (31u << 1);
        return true;
    }
    if (m == "rlwimi" || m == "rlwnm") {   // rlwimi rA, rS, SH, MB, ME  /  rlwnm rA, rS, rB, MB, ME
        expect_operands(line, 5);
        const bool imm = m == "rlwimi";
        const std::uint32_t third = imm ? static_cast<std::uint32_t>(parse_value(line, op[2], 0, 31))
                                        : parse_register(line, op[2]);
        word = ((imm ? 20u : 23u) << 26) | (parse_register(line, op[1]) << 21) | (parse_register(line, op[0]) << 16) |
               (third << 11) | (static_cast<std::uint32_t>(parse_value(line, op[3], 0, 31)) << 6) |
               (static_cast<std::uint32_t>(parse_value(line, op[4], 0, 31)) << 1);
        return true;
    }
    if (m == "rldicl" || m == "rldicr" || m == "rldic" || m == "rldimi") {   // rA, rS, SH, MB (ME pour rldicr)
        expect_operands(line, 4);
        const std::uint32_t xo = m == "rldicl" ? 0u : (m == "rldicr" ? 1u : (m == "rldic" ? 2u : 3u));
        word = rld_word(xo, parse_register(line, op[0]), parse_register(line, op[1]),
                        static_cast<std::uint32_t>(parse_value(line, op[2], 0, 63)),
                        static_cast<std::uint32_t>(parse_value(line, op[3], 0, 63)));
        return true;
    }
    if (m == "subf" || m == "subfc") {   // dans l'ordre du mot : subf rD, rA, rB = rB - rA (sub rD, rB, rA)
        expect_operands(line, 3);
        word = (31u << 26) | ((m == "subf" ? 40u : 8u) << 1) | (parse_register(line, op[0]) << 21) |
               (parse_register(line, op[1]) << 16) | (parse_register(line, op[2]) << 11);
        return true;
    }
    if (m == "rldcl" || m == "rldcr" || m == "rotld") {   // rldcl rA, rS, rB, MB ; rldcr rA, rS, rB, ME ; rotld rA, rS, rB
        expect_operands(line, m == "rotld" ? 3 : 4);
        const std::uint32_t mask_bit = m == "rotld" ? 0u : static_cast<std::uint32_t>(parse_value(line, op[3], 0, 63));
        const std::uint32_t raw = ((mask_bit & 0x1Fu) << 1) | (mask_bit >> 5);
        word = (30u << 26) | (parse_register(line, op[1]) << 21) | (parse_register(line, op[0]) << 16) |
               (parse_register(line, op[2]) << 11) | (raw << 5) | ((m == "rldcr" ? 9u : 8u) << 1);
        return true;
    }
    if (m == "sldi" || m == "srdi" || m == "clrldi" || m == "rotldi" || m == "clrrdi") {
        expect_operands(line, 3);
        const std::uint32_t ra = parse_register(line, op[0]);
        const std::uint32_t rs = parse_register(line, op[1]);
        const std::uint32_t n = static_cast<std::uint32_t>(parse_value(line, op[2], 0, 63));
        if (m == "sldi")   { word = rld_word(1, ra, rs, n, 63 - n); return true; }
        if (m == "srdi")   { word = rld_word(0, ra, rs, (64 - n) & 63u, n); return true; }
        if (m == "clrldi") { word = rld_word(0, ra, rs, 0, n); return true; }
        if (m == "clrrdi") { word = rld_word(1, ra, rs, 0, 63 - n); return true; }
        word = rld_word(0, ra, rs, n, 0);   // rotldi
        return true;
    }
    if (m == "cmplwi" || m == "cmpldi") {   // [crN,] rA, nombre sans signe
        std::size_t first = 0;
        const std::uint32_t crf = parse_optional_cr(line, first);
        if (op.size() != first + 2) {
            throw AssemblyError(line.number, "forme attendue : " + m + " [crN,] rA, nombre");
        }
        word = (10u << 26) | (m == "cmpldi" ? (1u << 21) : 0u) | (crf << 23) |
               (parse_register(line, op[first]) << 16) |
               static_cast<std::uint32_t>(parse_value(line, op[first + 1], 0, 65535));
        return true;
    }
    if (m == "cmplw" || m == "cmpld") {   // [crN,] rA, rB
        std::size_t first = 0;
        const std::uint32_t crf = parse_optional_cr(line, first);
        if (op.size() != first + 2) {
            throw AssemblyError(line.number, "forme attendue : " + m + " [crN,] rA, rB");
        }
        word = (31u << 26) | (32u << 1) | (m == "cmpld" ? (1u << 21) : 0u) | (crf << 23) |
               (parse_register(line, op[first]) << 16) | (parse_register(line, op[first + 1]) << 11);
        return true;
    }
    return false;
}

// Sauts conditionnels : beq, bne, bdnz, bdnzt, bt, bf, bc, et leurs variantes vers LR / CTR, avec l (LK) et a (AA).
// Renvoie false si le mnemonique n'est pas un saut conditionnel.
bool encode_conditional_branch(const Line& line, const std::map<std::string, std::size_t>& labels,
                               std::uint32_t& word) {
    const BranchBase* base = nullptr;
    int target_kind = 0;
    bool link = false;
    bool absolute = false;
    if (!split_branch_mnemonic(line.mnemonic, base, target_kind, link, absolute)) {
        return false;
    }
    const auto& op = line.operands;
    std::size_t next = 0;   // premier operande pas encore lu
    std::uint32_t bo = 0;
    std::uint32_t bi = 0;

    if (base->bo < 0) {   // bc BO, BI, ...
        if (op.size() < 2) {
            throw AssemblyError(line.number, "\"" + line.mnemonic + "\" attend BO, BI" + (target_kind == 0 ? ", cible" : ""));
        }
        bo = static_cast<std::uint32_t>(parse_value(line, op[0], 0, 31));
        bi = static_cast<std::uint32_t>(parse_value(line, op[1], 0, 31));
        next = 2;
    } else {
        bo = static_cast<std::uint32_t>(base->bo);
        if (base->explicit_bi) {   // bdnzt, bt... : BI est le premier operande
            if (op.empty()) {
                throw AssemblyError(line.number, "\"" + line.mnemonic + "\" attend BI en premier operande");
            }
            bi = static_cast<std::uint32_t>(parse_value(line, op[0], 0, 31));
            next = 1;
        } else if (base->bit >= 0) {   // beq, bne... : [crN,] puis la cible
            std::uint32_t crf = 0;
            if (!op.empty() && looks_like_cr(to_lower(op[0])) && (target_kind != 0 || op.size() == 2)) {
                crf = static_cast<std::uint32_t>(op[0][2] - '0');
                next = 1;
            }
            bi = crf * 4 + static_cast<std::uint32_t>(base->bit);
        }
        // bdnz, bdz : ni BI ni champ de CR (BI vaut 0)
    }
    if (target_kind == 2 && (bo & 4u) == 0) {
        throw AssemblyError(line.number, "bcctr ne peut pas decrementer CTR (BO doit avoir le bit 4 : \"" + line.mnemonic + "\")");
    }

    if (target_kind != 0) {   // vers LR ou CTR : plus aucun operande
        if (op.size() != next) {
            throw AssemblyError(line.number, "trop d'operandes pour \"" + line.mnemonic + "\"");
        }
        word = (19u << 26) | (bo << 21) | (bi << 16) | ((target_kind == 1 ? 16u : 528u) << 1) | (link ? 1u : 0u);
        return true;
    }

    if (op.size() != next + 1) {
        throw AssemblyError(line.number, "\"" + line.mnemonic + "\" attend une cible (etiquette ou deplacement)");
    }
    long long offset = 0;
    if (absolute) {   // "a" : la cible est une adresse, pas un deplacement
        long long value = 0;
        const auto found = labels.find(op[next]);
        if (found != labels.end()) {
            value = static_cast<long long>(found->second);
        } else {
            value = parse_value(line, op[next], -32768, 32764);
        }
        offset = value;
    } else {
        offset = branch_offset(line, op[next], labels);
    }
    if (offset % 4 != 0) {
        throw AssemblyError(line.number, "le deplacement d'un saut doit etre un multiple de 4");
    }
    if (offset < -32768 || offset > 32764) {
        throw AssemblyError(line.number, "saut conditionnel trop loin (32 Ko maximum)");
    }
    word = (16u << 26) | (bo << 21) | (bi << 16) | (static_cast<std::uint32_t>(offset) & 0xFFFCu) |
           (absolute ? 2u : 0u) | (link ? 1u : 0u);
    return true;
}

// Mnemonique inconnu : permet a encode() d'essayer ensuite les formes avec "." (Rc) et "o" (OE).
class UnknownInstruction : public AssemblyError {
public:
    UnknownInstruction(std::size_t line, const std::string& name)
        : AssemblyError(line, "instruction inconnue : " + name) {}
};

std::uint32_t encode_base(const Line& line, const std::map<std::string, std::size_t>& labels) {
    const std::string& m = line.mnemonic;
    const auto& op = line.operands;

    if (m == "li") {
        expect_operands(line, 2);
        return (14u << 26) | (parse_register(line, op[0]) << 21) |
               (static_cast<std::uint32_t>(parse_value(line, op[1], -32768, 32767)) & 0xFFFFu);
    }
    if (m == "addi") {
        expect_operands(line, 3);
        return (14u << 26) | (parse_register(line, op[0]) << 21) | (parse_register(line, op[1]) << 16) |
               (static_cast<std::uint32_t>(parse_value(line, op[2], -32768, 32767)) & 0xFFFFu);
    }
    if (m == "add") {
        expect_operands(line, 3);
        return 0x7C000214u | (parse_register(line, op[0]) << 21) | (parse_register(line, op[1]) << 16) |
               (parse_register(line, op[2]) << 11);
    }
    if (m == "lwzx" || m == "stwx" || m == "lbzx" || m == "stbx") {   // xxx rD, rA|0, rB
        expect_operands(line, 3);
        const std::uint32_t word = m == "lwzx" ? 0x7C00002Eu : (m == "stwx" ? 0x7C00012Eu :
                                   (m == "lbzx" ? 0x7C0000AEu : 0x7C0001AEu));
        return word | (parse_register(line, op[0]) << 21) | (parse_index_base(line, op[1]) << 16) |
               (parse_register(line, op[2]) << 11);
    }
    if (m == "lis") {
        expect_operands(line, 2);
        return (15u << 26) | (parse_register(line, op[0]) << 21) |
               (static_cast<std::uint32_t>(parse_value(line, op[1], -32768, 65535)) & 0xFFFFu);
    }
    if (m == "addis") {
        expect_operands(line, 3);
        return (15u << 26) | (parse_register(line, op[0]) << 21) | (parse_register(line, op[1]) << 16) |
               (static_cast<std::uint32_t>(parse_value(line, op[2], -32768, 65535)) & 0xFFFFu);
    }
    if (m == "sub") {   // sub rD, rA, rB = subf rD, rB, rA : les champs du mot sont inverses
        expect_operands(line, 3);
        return 0x7C000050u | (parse_register(line, op[0]) << 21) | (parse_register(line, op[2]) << 16) |
               (parse_register(line, op[1]) << 11);
    }
    if (m == "mullw") {
        expect_operands(line, 3);
        return 0x7C0001D6u | (parse_register(line, op[0]) << 21) | (parse_register(line, op[1]) << 16) |
               (parse_register(line, op[2]) << 11);
    }
    if (m == "rlwinm") {   // rlwinm rA, rS, SH, MB, ME
        expect_operands(line, 5);
        return rlwinm_word(parse_register(line, op[0]), parse_register(line, op[1]),
                           static_cast<std::uint32_t>(parse_value(line, op[2], 0, 31)),
                           static_cast<std::uint32_t>(parse_value(line, op[3], 0, 31)),
                           static_cast<std::uint32_t>(parse_value(line, op[4], 0, 31)));
    }
    if (m == "slwi" || m == "srwi" || m == "clrlwi" || m == "clrrwi" || m == "rotlwi") {
        expect_operands(line, 3);
        const std::uint32_t ra = parse_register(line, op[0]);
        const std::uint32_t rs = parse_register(line, op[1]);
        const std::uint32_t n = static_cast<std::uint32_t>(parse_value(line, op[2], 0, 31));
        if (m == "slwi")   return rlwinm_word(ra, rs, n, 0, 31 - n);
        if (m == "srwi")   return rlwinm_word(ra, rs, (32 - n) & 31u, n, 31);
        if (m == "clrlwi") return rlwinm_word(ra, rs, 0, n, 31);
        if (m == "clrrwi") return rlwinm_word(ra, rs, 0, 0, 31 - n);
        return rlwinm_word(ra, rs, n, 0, 31);   // rotlwi
    }
    if (m == "or") {   // or rA, rS, rB
        expect_operands(line, 3);
        return 0x7C000378u | (parse_register(line, op[1]) << 21) | (parse_register(line, op[0]) << 16) |
               (parse_register(line, op[2]) << 11);
    }
    if (m == "mr") {   // mr rA, rS = or rA, rS, rS
        expect_operands(line, 2);
        const std::uint32_t source = parse_register(line, op[1]);
        return 0x7C000378u | (source << 21) | (parse_register(line, op[0]) << 16) | (source << 11);
    }
    if (m == "cmpw" || m == "cmpd") {   // cmpd : meme mot avec L = 1 (bit 21)
        std::uint32_t crf = 0;
        std::size_t first = 0;
        if (!op.empty() && looks_like_cr(to_lower(op[0]))) {
            crf = static_cast<std::uint32_t>(op[0][2] - '0');
            first = 1;
        }
        if (op.size() != first + 2) {
            throw AssemblyError(line.number, "forme attendue : cmpw [crN,] rA, rB");
        }
        return 0x7C000000u | (m == "cmpd" ? (1u << 21) : 0u) | (crf << 23) | (parse_register(line, op[first]) << 16) |
               (parse_register(line, op[first + 1]) << 11);
    }
    if (m == "ori") {   // ori rA, rS, constante
        expect_operands(line, 3);
        return (24u << 26) | (parse_register(line, op[1]) << 21) | (parse_register(line, op[0]) << 16) |
               static_cast<std::uint32_t>(parse_value(line, op[2], 0, 65535));
    }
    if (m == "nop") {
        expect_operands(line, 0);
        return 0x60000000u;
    }
    if (m == "mfctr" || m == "mtctr") {
        expect_operands(line, 1);
        return (m == "mfctr" ? 0x7C0902A6u : 0x7C0903A6u) | (parse_register(line, op[0]) << 21);
    }
    if (m == "bctr" || m == "bctrl" || m == "blrl") {
        expect_operands(line, 0);
        return m == "bctr" ? 0x4E800420u : (m == "bctrl" ? 0x4E800421u : 0x4E800021u);
    }
    if (m == "mflr" || m == "mtlr") {
        expect_operands(line, 1);
        return (m == "mflr" ? 0x7C0802A6u : 0x7C0803A6u) | (parse_register(line, op[0]) << 21);
    }
    if (m == "lwz" || m == "stw" || m == "lbz" || m == "stb" || m == "stwu") {
        expect_operands(line, 2);
        long long offset = 0;
        std::uint32_t base = 0;
        parse_memory_operand(line, op[1], offset, base);
        const std::uint32_t opcode = m == "lwz" ? 32u : (m == "stw" ? 36u : (m == "lbz" ? 34u : (m == "stb" ? 38u : 37u)));
        return (opcode << 26) | (parse_register(line, op[0]) << 21) | (base << 16) |
               (static_cast<std::uint32_t>(offset) & 0xFFFFu);
    }
    if (m == "ld" || m == "std") {   // ld rD, ds(rA) : ds multiple de 4
        expect_operands(line, 2);
        long long offset = 0;
        std::uint32_t base = 0;
        parse_memory_operand(line, op[1], offset, base);
        if (offset % 4 != 0) {
            throw AssemblyError(line.number, "le deplacement de ld/std doit etre un multiple de 4");
        }
        return ((m == "ld" ? 58u : 62u) << 26) | (parse_register(line, op[0]) << 21) | (base << 16) |
               (static_cast<std::uint32_t>(offset) & 0xFFFCu);
    }
    if (m == "cmpwi" || m == "cmpdi") {
        std::uint32_t crf = 0;
        std::size_t first = 0;
        if (!op.empty() && looks_like_cr(to_lower(op[0]))) {
            crf = static_cast<std::uint32_t>(op[0][2] - '0');
            first = 1;
        }
        if (op.size() != first + 2) {
            throw AssemblyError(line.number, "forme attendue : cmpwi [crN,] rA, nombre");
        }
        return (11u << 26) | (m == "cmpdi" ? (1u << 21) : 0u) | (crf << 23) | (parse_register(line, op[first]) << 16) |
               (static_cast<std::uint32_t>(parse_value(line, op[first + 1], -32768, 32767)) & 0xFFFFu);
    }
    if (m == "ba" || m == "bla") {   // saut a une adresse absolue (28 bits avec signe : 0 a 0x1FFFFFC ou 0xFE000000 a 0xFFFFFFFC)
        expect_operands(line, 1);
        const auto found = labels.find(op[0]);
        const long long address = found != labels.end() ? static_cast<long long>(found->second)
                                                        : parse_value(line, op[0], -33554432, 4294967295LL);
        const std::uint32_t value = static_cast<std::uint32_t>(address);
        if (address % 4 != 0 || ((value & 0xFE000000u) != 0 && (value & 0xFE000000u) != 0xFE000000u)) {
            throw AssemblyError(line.number, "adresse absolue impossible a encoder : " + op[0]);
        }
        return (18u << 26) | (value & 0x03FFFFFCu) | 2u | (m == "bla" ? 1u : 0u);
    }
    if (m == "b" || m == "bl") {
        expect_operands(line, 1);
        return encode_branch(line, branch_offset(line, op[0], labels), m == "bl" ? 1u : 0u);
    }
    if (m == "blr") {
        expect_operands(line, 0);
        return 0x4E800020u;
    }
    {
        std::uint32_t branch_word = 0;
        if (encode_conditional_branch(line, labels, branch_word)) {
            return branch_word;
        }
    }
    if (const OpInfo* info = find_op(m)) {
        return encode_table_op(line, *info);
    }
    std::uint32_t word = 0;
    if (encode_extra(line, word)) {
        return word;
    }
    throw UnknownInstruction(line.number, m);
}

// Encode une ligne. Si le mnemonique est inconnu, essaie la forme avec point (Rc = 1) et/ou "o" (OE = 1) :
// "add." = add + bit 31, "addo" = add + bit 21, "addo." = les deux. Le decodeur dit si cette variante existe.
std::uint32_t encode(const Line& line, const std::map<std::string, std::size_t>& labels) {
    try {
        return encode_base(line, labels);
    } catch (const UnknownInstruction&) {
        // on continue ci-dessous
    }
    std::string stem = line.mnemonic;
    const bool rc = stem.size() > 1 && stem.back() == '.';
    if (rc) {
        stem.pop_back();
    }
    for (const bool oe : {false, true}) {
        if (!rc && !oe) {
            continue;   // sans point ni "o", il n'y a rien d'autre a essayer
        }
        std::string name = stem;
        if (oe) {
            if (name.size() < 2 || name.back() != 'o') {
                continue;
            }
            name.pop_back();
        }
        Line base = line;
        base.mnemonic = name;
        std::uint32_t word = 0;
        try {
            word = encode_base(base, labels);
        } catch (const UnknownInstruction&) {
            continue;
        }
        word |= (rc ? 1u : 0u) | (oe ? 0x400u : 0u);
        const Instruction decoded = decode(word);
        if (decoded.kind == Kind::Unknown || decoded.rc != rc || decoded.oe != oe) {
            throw AssemblyError(line.number, "\"" + name + "\" n'existe pas avec " +
                                             std::string(oe ? "le suffixe o" : "") + (oe && rc ? " et " : "") +
                                             std::string(rc ? "le point" : "") + " : " + line.mnemonic);
        }
        return word;
    }
    throw UnknownInstruction(line.number, line.mnemonic);
}

}  // namespace

std::vector<std::uint32_t> assemble(const std::string& source) {
    std::vector<Line> lines;
    std::map<std::string, std::size_t> labels;   // nom -> adresse

    // premiere passe : lire le texte, noter les etiquettes
    std::size_t number = 0;
    std::size_t start = 0;
    while (start <= source.size()) {
        std::size_t end = source.find('\n', start);
        if (end == std::string::npos) {
            end = source.size();
        }
        ++number;
        std::string text = source.substr(start, end - start);
        start = end + 1;

        const std::size_t comment = text.find_first_of(";#");
        if (comment != std::string::npos) {
            text.erase(comment);
        }
        text = trim(text);

        // etiquette devant l'instruction ?
        const std::size_t colon = text.find(':');
        if (colon != std::string::npos) {
            const std::string name = trim(text.substr(0, colon));
            bool valid = !name.empty() && !is_digit(name[0]) && !looks_like_register(name);
            for (const char c : name) {
                valid = valid && is_word_char(c);
            }
            if (!valid) {
                throw AssemblyError(number, "nom d'etiquette invalide : \"" + name + "\"");
            }
            if (labels.count(name) != 0) {
                throw AssemblyError(number, "etiquette deja definie : " + name);
            }
            labels[name] = lines.size() * 4;
            text = trim(text.substr(colon + 1));
        }
        if (text.empty()) {
            continue;
        }

        Line line;
        line.number = number;
        line.address = lines.size() * 4;
        std::size_t word_end = 0;
        while (word_end < text.size() && !is_space(text[word_end])) {
            ++word_end;
        }
        line.mnemonic = to_lower(text.substr(0, word_end));

        // operandes separes par des virgules
        const std::string rest = trim(text.substr(word_end));
        if (!rest.empty()) {
            std::size_t from = 0;
            while (true) {
                const std::size_t comma = rest.find(',', from);
                const std::string operand = trim(rest.substr(from, comma == std::string::npos
                                                                       ? std::string::npos
                                                                       : comma - from));
                if (operand.empty()) {
                    throw AssemblyError(number, "operande vide");
                }
                line.operands.push_back(looks_like_register(operand) ? to_lower(operand) : operand);
                if (comma == std::string::npos) {
                    break;
                }
                from = comma + 1;
            }
        }
        lines.push_back(line);
    }

    // deuxieme passe : transformer chaque ligne en mot de 32 bits
    std::vector<std::uint32_t> words;
    words.reserve(lines.size());
    for (const Line& line : lines) {
        words.push_back(encode(line, labels));
    }
    return words;
}

}  // namespace nerith::ppc
