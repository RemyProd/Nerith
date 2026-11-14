#include "nerith/assembler.h"

#include <cctype>
#include <map>
#include <sstream>

#include "nerith/cpu.h"

namespace nerith {

AssemblyError::AssemblyError(std::size_t line, const std::string& message)
    : std::runtime_error("ligne " + std::to_string(line) + " : " + message), line_(line) {}

namespace {

// Une instruction lue dans le texte, pas encore transformee en octets.
struct Line {
    std::size_t number = 0;                // numero de ligne, pour les messages d'erreur
    std::string mnemonic;                  // "set", "jnz"... en minuscules
    std::vector<std::string> operands;     // "r0", "5", "loop"...
};

bool is_space(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

bool is_digit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
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

// "r0", "R12"... : la lettre r suivie d'au moins un chiffre
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

bool is_identifier(const std::string& word) {
    if (word.empty() || is_digit(word[0])) {
        return false;
    }
    for (char c : word) {
        if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '_') {
            return false;
        }
    }
    return true;
}

int digit_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Un nombre de 0 a 255, en decimal ou en hexadecimal (0x...).
std::uint8_t parse_byte(const std::string& text, std::size_t line) {
    if (text.empty()) {
        throw AssemblyError(line, "nombre manquant");
    }

    int base = 10;
    std::size_t start = 0;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        start = 2;
    }

    unsigned long value = 0;
    for (std::size_t i = start; i < text.size(); i++) {
        const int digit = digit_value(text[i]);
        if (digit < 0 || digit >= base) {
            throw AssemblyError(line, "nombre invalide : " + text);
        }
        value = value * static_cast<unsigned long>(base) + static_cast<unsigned long>(digit);
        if (value > 255) {
            throw AssemblyError(line, "nombre trop grand (maximum 255) : " + text);
        }
    }
    return static_cast<std::uint8_t>(value);
}

std::uint8_t parse_register(const std::string& text, std::size_t line) {
    if (!looks_like_register(text)) {
        throw AssemblyError(line, "registre attendu (par exemple r0) : " + text);
    }
    // on saute le "r" et on lit le numero
    return parse_byte(text.substr(1), line);
}

// Adresse d'un Load ou d'un Store : "20" ou "[20]".
std::uint8_t parse_address(const std::string& text, std::size_t line) {
    if (!text.empty() && text.front() == '[') {
        if (text.back() != ']') {
            throw AssemblyError(line, "crochet fermant manquant : " + text);
        }
        return parse_byte(trim(text.substr(1, text.size() - 2)), line);
    }
    return parse_byte(text, line);
}

// Cible d'un saut : un nombre, ou le nom d'une etiquette.
std::uint8_t parse_target(const std::string& text, const std::map<std::string, std::size_t>& labels,
                          std::size_t line) {
    if (!text.empty() && is_digit(text[0])) {
        return parse_byte(text, line);
    }
    const auto found = labels.find(text);
    if (found == labels.end()) {
        throw AssemblyError(line, "etiquette inconnue : " + text);
    }
    if (found->second > 255) {
        throw AssemblyError(line, "l'etiquette " + text + " est a l'adresse " +
                                      std::to_string(found->second) + ", au dela de 255");
    }
    return static_cast<std::uint8_t>(found->second);
}

void expect_operands(const Line& line, std::size_t count) {
    if (line.operands.size() != count) {
        throw AssemblyError(line.number, line.mnemonic + " attend " + std::to_string(count) +
                                             " operande(s), pas " +
                                             std::to_string(line.operands.size()));
    }
}

// Decoupe "r1, [20]" en {"r1", "[20]"}.
std::vector<std::string> split_operands(const std::string& text, std::size_t line) {
    std::vector<std::string> operands;
    if (trim(text).empty()) {
        return operands;
    }
    std::istringstream in(text);
    std::string part;
    while (std::getline(in, part, ',')) {
        part = trim(part);
        if (part.empty()) {
            throw AssemblyError(line, "operande vide");
        }
        operands.push_back(part);
    }
    // getline ne signale pas une virgule finale ("Set r0,") : on la cherche nous-memes
    if (trim(text).back() == ',') {
        throw AssemblyError(line, "operande vide");
    }
    return operands;
}

}  // namespace

std::vector<std::uint8_t> assemble(const std::string& source) {
    std::vector<Line> lines;
    std::map<std::string, std::size_t> labels;   // nom -> adresse

    // Passe 1 : lire le texte, noter les etiquettes et garder les instructions.
    std::istringstream in(source);
    std::string text;
    std::size_t number = 0;
    while (std::getline(in, text)) {
        ++number;

        const std::size_t comment = text.find(';');
        if (comment != std::string::npos) {
            text.erase(comment);
        }
        text = trim(text);

        // une etiquette eventuelle, avant le ":"
        const std::size_t colon = text.find(':');
        if (colon != std::string::npos) {
            const std::string name = trim(text.substr(0, colon));
            if (!is_identifier(name) || looks_like_register(name)) {
                throw AssemblyError(number, "nom d'etiquette invalide : " + name);
            }
            if (labels.count(name) != 0) {
                throw AssemblyError(number, "etiquette deja definie : " + name);
            }
            // l'etiquette designe la prochaine instruction (3 octets chacune)
            labels[name] = lines.size() * 3;
            text = trim(text.substr(colon + 1));
        }

        if (text.empty()) {
            continue;
        }

        // le mot d'instruction, puis les operandes
        std::size_t end = 0;
        while (end < text.size() && !is_space(text[end])) {
            ++end;
        }
        Line line;
        line.number = number;
        line.mnemonic = to_lower(text.substr(0, end));
        line.operands = split_operands(text.substr(end), number);
        lines.push_back(line);
    }

    // Passe 2 : transformer chaque instruction en 3 octets.
    // (deux passes, pour qu'un saut puisse viser une etiquette definie plus bas)
    std::vector<std::uint8_t> bytes;
    for (const Line& line : lines) {
        Op op;
        std::uint8_t reg = 0;
        std::uint8_t value = 0;

        if (line.mnemonic == "set") {
            expect_operands(line, 2);
            op = Op::Set;
            reg = parse_register(line.operands[0], line.number);
            value = parse_byte(line.operands[1], line.number);
        } else if (line.mnemonic == "add" || line.mnemonic == "sub" || line.mnemonic == "mul") {
            expect_operands(line, 2);
            op = line.mnemonic == "add" ? Op::Add : (line.mnemonic == "sub" ? Op::Sub : Op::Mul);
            reg = parse_register(line.operands[0], line.number);
            value = parse_register(line.operands[1], line.number);
        } else if (line.mnemonic == "inc" || line.mnemonic == "dec") {
            expect_operands(line, 1);
            op = line.mnemonic == "inc" ? Op::Inc : Op::Dec;
            reg = parse_register(line.operands[0], line.number);
        } else if (line.mnemonic == "load" || line.mnemonic == "store") {
            expect_operands(line, 2);
            op = line.mnemonic == "load" ? Op::Load : Op::Store;
            reg = parse_register(line.operands[0], line.number);
            value = parse_address(line.operands[1], line.number);
        } else if (line.mnemonic == "jmp") {
            expect_operands(line, 1);
            op = Op::Jmp;
            value = parse_target(line.operands[0], labels, line.number);
        } else if (line.mnemonic == "jnz") {
            expect_operands(line, 2);
            op = Op::Jnz;
            reg = parse_register(line.operands[0], line.number);
            value = parse_target(line.operands[1], labels, line.number);
        } else if (line.mnemonic == "halt") {
            expect_operands(line, 0);
            op = Op::Halt;
        } else {
            throw AssemblyError(line.number, "instruction inconnue : " + line.mnemonic);
        }

        bytes.push_back(code(op));
        bytes.push_back(reg);
        bytes.push_back(value);
    }
    return bytes;
}

}  // namespace nerith
