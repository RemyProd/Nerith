#include "nerith/util/file.h"

#include <algorithm>
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

std::vector<std::uint8_t> read_binary_file(const std::string& path, std::size_t max_bytes) {
    // std::ios::binary : sur Windows, sans lui, les fins de ligne seraient modifiees
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("impossible d'ouvrir le fichier : " + path);
    }
    std::vector<std::uint8_t> bytes;
    char buffer[4096];
    while (bytes.size() < max_bytes && file) {
        const std::size_t wanted = std::min(sizeof(buffer), max_bytes - bytes.size());
        file.read(buffer, static_cast<std::streamsize>(wanted));
        const std::size_t got = static_cast<std::size_t>(file.gcount());
        bytes.insert(bytes.end(), buffer, buffer + got);
        if (got < wanted) {
            break;
        }
    }
    return bytes;
}

}  // namespace nerith
