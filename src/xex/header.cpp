#include "nerith/xex/header.h"

#include <iomanip>
#include <sstream>

namespace nerith::xex {

namespace {

constexpr std::size_t kMainHeaderSize = 24;      // l'en-tete principal
constexpr std::size_t kOptionalEntrySize = 8;    // un en-tete optionnel : identifiant + valeur

// Lit 4 octets a cette position, en big-endian (octet de poids fort en premier).
std::uint32_t read32(const std::vector<std::uint8_t>& bytes, std::size_t position) {
    return (static_cast<std::uint32_t>(bytes[position]) << 24) |
           (static_cast<std::uint32_t>(bytes[position + 1]) << 16) |
           (static_cast<std::uint32_t>(bytes[position + 2]) << 8) |
           static_cast<std::uint32_t>(bytes[position + 3]);
}

std::string hex(std::uint32_t value, int digits) {
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << std::setw(digits) << std::setfill('0') << value;
    return out.str();
}

struct Name {
    std::uint32_t id;
    const char* text;
};

// Liste de Free60 (noms en francais).
const Name kNames[] = {
    {0x000002FF, "Informations de ressources"},
    {0x000003FF, "Format du fichier de base"},
    {0x00000405, "Reference de base"},
    {0x000005FF, "Descripteur de patch delta"},
    {0x000080FF, "Chemin limite"},
    {0x00008105, "Identifiant de peripherique"},
    {0x00010001, "Adresse de base d'origine"},
    {0x00010100, "Point d'entree"},
    {0x00010201, "Adresse de base de l'image"},
    {0x000103FF, "Bibliotheques importees"},
    {0x00018002, "Date de la somme de controle"},
    {0x00018102, "Active pour callcap"},
    {0x00018200, "Active pour fastcap"},
    {0x000183FF, "Nom d'origine du PE"},
    {0x000200FF, "Bibliotheques statiques"},
    {0x00020104, "Informations TLS"},
    {0x00020200, "Taille de pile par defaut"},
    {0x00020301, "Taille par defaut du cache de fichiers"},
    {0x00020401, "Taille par defaut du tas"},
    {0x00028002, "Taille et drapeaux du tas de pages"},
    {0x00030000, "Drapeaux systeme"},
    {0x00040006, "Identifiant d'execution"},
    {0x000401FF, "Liste des identifiants de service"},
    {0x00040201, "Taille de l'espace de travail du titre"},
    {0x00040310, "Classifications du jeu"},
    {0x00040404, "Cle LAN"},
    {0x000405FF, "Logo Xbox 360"},
    {0x000406FF, "Identifiants de supports multi-disques"},
    {0x000407FF, "Identifiants de titre alternatifs"},
    {0x00040801, "Memoire supplementaire du titre"},
    {0x00E10402, "Exports par nom"},
};

// Drapeaux du module, du bit 0 (poids faible) au bit 7 (Free60).
const char* const kFlagNames[] = {
    "module de titre",   "exporte vers le titre", "debogueur systeme", "DLL",
    "patch de module",   "patch complet",         "patch delta",       "mode utilisateur",
};

}  // namespace

ValueKind value_kind(std::uint32_t id) {
    const std::uint32_t low = id & 0xFF;
    if (low == 0x00 || low == 0x01) {
        return ValueKind::Inline;
    }
    if (low == 0xFF) {
        return ValueKind::Variable;
    }
    return ValueKind::Offset;
}

const char* optional_header_name(std::uint32_t id) {
    for (const Name& name : kNames) {
        if (name.id == id) {
            return name.text;
        }
    }
    return nullptr;
}

Header parse_header(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < kMainHeaderSize) {
        throw XexError("fichier trop court pour un en-tete XEX (" + std::to_string(bytes.size()) +
                       " octets, 24 attendus)");
    }

    // signature : 4 caracteres ASCII
    const std::string magic(bytes.begin(), bytes.begin() + 4);
    if (magic != "XEX2") {
        if (magic.compare(0, 3, "XEX") == 0) {
            throw XexError("version de XEX non geree : \"" + magic + "\" (seul XEX2 est gere)");
        }
        throw XexError("signature invalide : ce n'est pas un fichier XEX");
    }

    Header header;
    header.module_flags = read32(bytes, 0x04);
    header.pe_offset = read32(bytes, 0x08);
    // 0x0C : champ reserve
    header.security_offset = read32(bytes, 0x10);
    const std::uint32_t count = read32(bytes, 0x14);

    // la liste des en-tetes optionnels doit tenir dans les octets fournis
    const std::uint64_t needed = kMainHeaderSize + static_cast<std::uint64_t>(count) * kOptionalEntrySize;
    if (needed > bytes.size()) {
        throw XexError("la liste de " + std::to_string(count) + " en-tetes optionnels depasse les " +
                       std::to_string(bytes.size()) + " octets fournis");
    }

    for (std::uint32_t i = 0; i < count; i++) {
        const std::size_t position = kMainHeaderSize + static_cast<std::size_t>(i) * kOptionalEntrySize;
        OptionalHeader entry;
        entry.id = read32(bytes, position);
        entry.value = read32(bytes, position + 4);
        header.optional.push_back(entry);
    }
    return header;
}

std::optional<std::uint32_t> find_value(const Header& header, std::uint32_t id) {
    for (const OptionalHeader& entry : header.optional) {
        if (entry.id == id) {
            return entry.value;
        }
    }
    return std::nullopt;
}

std::string describe(const Header& header) {
    std::ostringstream out;
    out << "Signature : XEX2\n";

    out << "Drapeaux du module : " << hex(header.module_flags, 8);
    bool first = true;
    for (int bit = 0; bit < 8; bit++) {
        if ((header.module_flags >> bit) & 1u) {
            out << (first ? " (" : ", ") << kFlagNames[bit];
            first = false;
        }
    }
    out << (first ? "" : ")") << "\n";

    out << "Donnees PE : decalage " << hex(header.pe_offset, 8) << " (" << header.pe_offset << ")\n";
    out << "Informations de securite : decalage " << hex(header.security_offset, 8) << " ("
        << header.security_offset << ")\n";
    out << "En-tetes optionnels : " << header.optional.size() << "\n";

    for (const OptionalHeader& entry : header.optional) {
        const char* name = optional_header_name(entry.id);
        out << "  " << hex(entry.id, 8) << "  " << std::left << std::setw(40)
            << (name != nullptr ? name : "(identifiant inconnu)") << std::right << " ";
        switch (value_kind(entry.id)) {
            case ValueKind::Inline:
                out << "valeur " << hex(entry.value, 8);
                break;
            case ValueKind::Variable:
                out << "donnee a " << hex(entry.value, 8) << " (taille variable)";
                break;
            case ValueKind::Offset:
                out << "donnee a " << hex(entry.value, 8) << " (" << (entry.id & 0xFF) * 4 << " octets)";
                break;
        }
        out << "\n";
    }

    if (const auto entry = find_value(header, kEntryPoint)) {
        out << "Point d'entree : " << hex(*entry, 8) << "\n";
    }
    if (const auto base = find_value(header, kImageBaseAddress)) {
        out << "Adresse de base de l'image : " << hex(*base, 8) << "\n";
    }
    return out.str();
}

}  // namespace nerith::xex
