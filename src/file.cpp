#include "nerith/file.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace nerith {

std::string read_text_file(const std::string& path) {
    // RAII : le fichier se ferme tout seul a la fin de la fonction (ou si une exception part)
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("impossible d'ouvrir le fichier : " + path);
    }
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

}  // namespace nerith
