#include "nerith/ppc/assembler.h"

#include <cctype>
#include <map>

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

// beq, bne, blt, bgt, ble, bge : sauts selon un bit de CR
bool conditional_info(const std::string& mnemonic, std::uint32_t& bo, std::uint32_t& bit) {
    if (mnemonic == "blt") { bo = 12; bit = 0; return true; }
    if (mnemonic == "bgt") { bo = 12; bit = 1; return true; }
    if (mnemonic == "beq") { bo = 12; bit = 2; return true; }
    if (mnemonic == "bge") { bo = 4;  bit = 0; return true; }
    if (mnemonic == "ble") { bo = 4;  bit = 1; return true; }
    if (mnemonic == "bne") { bo = 4;  bit = 2; return true; }
    return false;
}

std::uint32_t encode(const Line& line, const std::map<std::string, std::size_t>& labels) {
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
    if (m == "cmpw") {
        std::uint32_t crf = 0;
        std::size_t first = 0;
        if (!op.empty() && looks_like_cr(to_lower(op[0]))) {
            crf = static_cast<std::uint32_t>(op[0][2] - '0');
            first = 1;
        }
        if (op.size() != first + 2) {
            throw AssemblyError(line.number, "forme attendue : cmpw [crN,] rA, rB");
        }
        return 0x7C000000u | (crf << 23) | (parse_register(line, op[first]) << 16) |
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
    if (m == "lwz" || m == "stw" || m == "lbz" || m == "stb") {
        expect_operands(line, 2);
        long long offset = 0;
        std::uint32_t base = 0;
        parse_memory_operand(line, op[1], offset, base);
        const std::uint32_t opcode = m == "lwz" ? 32u : (m == "stw" ? 36u : (m == "lbz" ? 34u : 38u));
        return (opcode << 26) | (parse_register(line, op[0]) << 21) | (base << 16) |
               (static_cast<std::uint32_t>(offset) & 0xFFFFu);
    }
    if (m == "cmpwi") {
        std::uint32_t crf = 0;
        std::size_t first = 0;
        if (!op.empty() && looks_like_cr(to_lower(op[0]))) {
            crf = static_cast<std::uint32_t>(op[0][2] - '0');
            first = 1;
        }
        if (op.size() != first + 2) {
            throw AssemblyError(line.number, "forme attendue : cmpwi [crN,] rA, nombre");
        }
        return (11u << 26) | (crf << 23) | (parse_register(line, op[first]) << 16) |
               (static_cast<std::uint32_t>(parse_value(line, op[first + 1], -32768, 32767)) & 0xFFFFu);
    }
    if (m == "b" || m == "bl") {
        expect_operands(line, 1);
        return encode_branch(line, branch_offset(line, op[0], labels), m == "bl" ? 1u : 0u);
    }
    if (m == "blr") {
        expect_operands(line, 0);
        return 0x4E800020u;
    }
    std::uint32_t bo = 0;
    std::uint32_t bit = 0;
    if (conditional_info(m, bo, bit)) {
        std::uint32_t crf = 0;
        std::size_t first = 0;
        if (!op.empty() && looks_like_cr(to_lower(op[0]))) {
            crf = static_cast<std::uint32_t>(op[0][2] - '0');
            first = 1;
        }
        if (op.size() != first + 1) {
            throw AssemblyError(line.number, "forme attendue : " + m + " [crN,] etiquette");
        }
        const long long offset = branch_offset(line, op[first], labels);
        if (offset % 4 != 0) {
            throw AssemblyError(line.number, "le deplacement d'un saut doit etre un multiple de 4");
        }
        if (offset < -32768 || offset > 32764) {
            throw AssemblyError(line.number, "saut conditionnel trop loin (32 Ko maximum)");
        }
        return (16u << 26) | (bo << 21) | ((crf * 4 + bit) << 16) |
               (static_cast<std::uint32_t>(offset) & 0xFFFCu);
    }
    throw AssemblyError(line.number, "instruction inconnue : " + m);
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
