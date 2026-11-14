#pragma once

#include <string>

namespace nerith {

// Lit tout un fichier texte et renvoie son contenu.
// Leve std::runtime_error si le fichier ne peut pas etre ouvert.
std::string read_text_file(const std::string& path);

}  // namespace nerith
