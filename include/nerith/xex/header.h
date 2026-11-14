#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace nerith::xex {

// Erreur de lecture d'un fichier XEX (trop court, mauvaise signature, version non geree...).
class XexError : public std::runtime_error {
public:
    explicit XexError(const std::string& message) : std::runtime_error(message) {}
};

// Un en-tete optionnel : un identifiant et une valeur brute de 32 bits (lue en big-endian).
// Ce que la valeur signifie depend de l'identifiant : voir value_kind.
struct OptionalHeader {
    std::uint32_t id = 0;
    std::uint32_t value = 0;
};

// Comment lire la valeur d'un en-tete optionnel, d'apres l'octet de poids faible de son identifiant
// (source : Free60, voir la note "12 - Format XEX" du coffre) :
//   Inline   : la valeur est la donnee elle-meme (par exemple une adresse)
//   Variable : la donnee est ailleurs dans le fichier, a ce decalage, et sa taille est au debut (octet 0xFF)
//   Offset   : la donnee est a ce decalage, et l'octet de poids faible donne sa taille en mots de 4 octets
enum class ValueKind { Inline, Variable, Offset };

// [a verifier] Free60 ne dit "valeur directe" que pour 0x01 ; ici 0x00 est aussi traite comme direct
// (le point d'entree 0x10100 se termine par 0x00 et semble etre une valeur directe).
ValueKind value_kind(std::uint32_t id);

// Identifiants utiles (liste de Free60).
constexpr std::uint32_t kBaseFileFormat = 0x000003FF;
constexpr std::uint32_t kEntryPoint = 0x00010100;
constexpr std::uint32_t kImageBaseAddress = 0x00010201;
constexpr std::uint32_t kImportLibraries = 0x000103FF;
constexpr std::uint32_t kTlsInfo = 0x00020104;
constexpr std::uint32_t kDefaultStackSize = 0x00020200;
constexpr std::uint32_t kExecutionId = 0x00040006;

// Nom d'un identifiant connu (en francais), ou nullptr s'il n'est pas dans la liste.
const char* optional_header_name(std::uint32_t id);

// L'en-tete principal de 24 octets et la liste des en-tetes optionnels qui le suit.
struct Header {
    std::uint32_t module_flags = 0;
    std::uint32_t pe_offset = 0;         // ou commencent les donnees (l'image PE) dans le fichier
    std::uint32_t security_offset = 0;   // [a verifier] decalage des informations de securite
    std::vector<OptionalHeader> optional;
};

// Lit l'en-tete au debut de "bytes" (les octets du debut d'un fichier .xex).
// Ne lit que l'en-tete de 24 octets et la liste des en-tetes optionnels, sans rien dechiffrer
// ni decompresser. Leve XexError si les octets ne decrivent pas un XEX2 lisible.
Header parse_header(const std::vector<std::uint8_t>& bytes);

// Valeur brute de l'en-tete optionnel "id", s'il est present.
std::optional<std::uint32_t> find_value(const Header& header, std::uint32_t id);

// Description lisible de l'en-tete, une information par ligne.
std::string describe(const Header& header);

}  // namespace nerith::xex
