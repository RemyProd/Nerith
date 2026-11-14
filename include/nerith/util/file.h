#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nerith {

// Lit tout un fichier texte et renvoie son contenu.
// Leve std::runtime_error si le fichier ne peut pas etre ouvert.
std::string read_text_file(const std::string& path);

// Lit un fichier en octets, sans aucune transformation, et renvoie son contenu.
// Ne lit que les "max_bytes" premiers octets (par defaut : tout le fichier).
// Leve std::runtime_error si le fichier ne peut pas etre ouvert.
std::vector<std::uint8_t> read_binary_file(const std::string& path, std::size_t max_bytes = SIZE_MAX);

}  // namespace nerith
