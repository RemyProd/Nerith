#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "nerith/xex/header.h"

namespace {

int failures = 0;

void check(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "ECHEC : " << name << "\n";
        ++failures;
    }
}

// Ajoute un mot de 32 bits en big-endian a la fin des octets.
void put32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value >> 24));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value));
}

// Un en-tete de 24 octets avec la signature donnee.
std::vector<std::uint8_t> make_header(const std::string& magic, std::uint32_t flags, std::uint32_t pe_offset,
                                      std::uint32_t security_offset, std::uint32_t count) {
    std::vector<std::uint8_t> bytes(magic.begin(), magic.end());
    put32(bytes, flags);
    put32(bytes, pe_offset);
    put32(bytes, 0);   // champ reserve
    put32(bytes, security_offset);
    put32(bytes, count);
    return bytes;
}

// Vrai si parse_header refuse ces octets avec une XexError.
bool refused(const std::vector<std::uint8_t>& bytes) {
    try {
        nerith::xex::parse_header(bytes);
    } catch (const nerith::xex::XexError&) {
        return true;
    }
    return false;
}

}  // namespace

int main() {
    using namespace nerith::xex;

    {   // l'exercice de la note "12 - Format XEX" : 32 octets
        const std::vector<std::uint8_t> bytes = {
            0x58, 0x45, 0x58, 0x32, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x02, 0x01, 0x82, 0x00, 0x00, 0x00};
        const Header header = parse_header(bytes);
        check(header.module_flags == 1, "exercice : drapeaux");
        check(header.pe_offset == 0x2000, "exercice : decalage des donnees PE");
        check(header.security_offset == 0x180, "exercice : decalage des informations de securite");
        check(header.optional.size() == 1, "exercice : un en-tete optionnel");
        check(header.optional[0].id == kImageBaseAddress && header.optional[0].value == 0x82000000,
              "exercice : adresse de base 0x82000000");
        check(find_value(header, kImageBaseAddress) == 0x82000000u, "find_value : present");
        check(!find_value(header, kEntryPoint).has_value(), "find_value : absent");
    }
    {   // plusieurs en-tetes optionnels, de trois sortes
        std::vector<std::uint8_t> bytes = make_header("XEX2", 0x00000081, 0x3000, 0x200, 4);
        put32(bytes, kEntryPoint);          put32(bytes, 0x82001234);   // valeur directe
        put32(bytes, kImageBaseAddress);    put32(bytes, 0x82000000);   // valeur directe
        put32(bytes, kImportLibraries);     put32(bytes, 0x00000300);   // taille variable
        put32(bytes, kExecutionId);         put32(bytes, 0x00000400);   // 6 mots de 4 octets
        const Header header = parse_header(bytes);
        check(header.optional.size() == 4, "quatre en-tetes optionnels");
        check(find_value(header, kEntryPoint) == 0x82001234u, "point d'entree");
        check(value_kind(kEntryPoint) == ValueKind::Inline, "point d'entree : valeur directe");
        check(value_kind(kImageBaseAddress) == ValueKind::Inline, "adresse de base : valeur directe");
        check(value_kind(kImportLibraries) == ValueKind::Variable, "bibliotheques importees : taille variable");
        check(value_kind(kExecutionId) == ValueKind::Offset, "identifiant d'execution : decalage et taille");

        const std::string text = describe(header);
        check(text.find("Signature : XEX2") != std::string::npos, "description : signature");
        check(text.find("0x00000081 (module de titre, mode utilisateur)") != std::string::npos,
              "description : drapeaux (bits 0 et 7)");
        check(text.find("Point d'entree : 0x82001234") != std::string::npos, "description : point d'entree");
        check(text.find("Adresse de base de l'image : 0x82000000") != std::string::npos,
              "description : adresse de base");
        check(text.find("Bibliotheques importees") != std::string::npos, "description : nom d'un identifiant connu");
        check(text.find("(24 octets)") != std::string::npos, "description : taille 6 mots = 24 octets");
    }
    {   // zero en-tete optionnel, identifiant inconnu
        const Header none = parse_header(make_header("XEX2", 0, 0x2000, 0x100, 0));
        check(none.optional.empty(), "zero en-tete optionnel");
        check(describe(none).find("Drapeaux du module : 0x00000000\n") != std::string::npos,
              "description : aucun drapeau");

        std::vector<std::uint8_t> bytes = make_header("XEX2", 0, 0, 0, 1);
        put32(bytes, 0x00BADBAD);
        put32(bytes, 5);
        check(describe(parse_header(bytes)).find("(identifiant inconnu)") != std::string::npos,
              "description : identifiant inconnu");
        check(optional_header_name(0x00BADBAD) == nullptr, "nom : identifiant inconnu");
    }
    {   // des octets en plus apres la liste ne gerent pas (le reste du fichier)
        std::vector<std::uint8_t> bytes = make_header("XEX2", 1, 0x2000, 0x180, 0);
        bytes.resize(4096, 0xAB);
        check(parse_header(bytes).pe_offset == 0x2000, "octets en plus apres l'en-tete : ignores");
    }

    // erreurs
    check(refused({}), "vide : refuse");
    check(refused({0x58, 0x45, 0x58, 0x32}), "trop court : refuse");
    check(refused(make_header("MZ\x90\x01", 0, 0, 0, 0)), "mauvaise signature : refuse");
    check(refused(make_header("XEX1", 0, 0, 0, 0)), "XEX1 : version non geree");
    check(refused(make_header("XEX2", 0, 0, 0, 3)), "3 en-tetes annonces mais absents : refuse");
    check(refused(make_header("XEX2", 0, 0, 0, 0xFFFFFFFF)), "nombre enorme : refuse sans deborder");

    if (failures == 0) {
        std::cout << "Tous les tests de l'en-tete XEX passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
