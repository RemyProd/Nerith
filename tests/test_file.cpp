#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "nerith/util/file.h"

namespace {

int failures = 0;

void check(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "ECHEC : " << name << "\n";
        ++failures;
    }
}

// Ecrit un petit fichier de test, le relit avec read_text_file, puis le supprime.
std::string write_and_read(const std::string& content) {
    const std::string path = "test_file_tmp.txt";
    {
        std::ofstream out(path);   // les accolades referment le fichier avant de le relire
        out << content;
    }
    const std::string result = nerith::read_text_file(path);
    std::filesystem::remove(path);
    return result;
}

// Ecrit des octets tels quels, les relit avec read_binary_file, puis supprime le fichier.
std::vector<std::uint8_t> write_and_read_binary(const std::vector<std::uint8_t>& content, std::size_t max_bytes) {
    const std::string path = "test_file_tmp.bin";
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
    }
    const std::vector<std::uint8_t> result = nerith::read_binary_file(path, max_bytes);
    std::filesystem::remove(path);
    return result;
}

}  // namespace

int main() {
    check(write_and_read("Set r0, 5\nHalt\n") == "Set r0, 5\nHalt\n", "le contenu est relu a l'identique");
    check(write_and_read("").empty(), "un fichier vide donne un texte vide");
    check(write_and_read("sans retour a la ligne") == "sans retour a la ligne",
          "derniere ligne sans retour a la ligne");

    bool thrown = false;
    try {
        nerith::read_text_file("ce_fichier_n_existe_pas.asm");
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    check(thrown, "un fichier inexistant leve une exception");

    {   // fichier binaire : les octets 0x0D 0x0A 0x00 0x1A ne doivent pas etre modifies
        const std::vector<std::uint8_t> bytes = {0x58, 0x45, 0x58, 0x32, 0x0D, 0x0A, 0x00, 0x1A, 0xFF};
        check(write_and_read_binary(bytes, SIZE_MAX) == bytes, "binaire : octets relus a l'identique");
        check(write_and_read_binary({}, SIZE_MAX).empty(), "binaire : fichier vide");

        std::vector<std::uint8_t> big(10000);
        for (std::size_t i = 0; i < big.size(); i++) {
            big[i] = static_cast<std::uint8_t>(i * 7);
        }
        check(write_and_read_binary(big, SIZE_MAX) == big, "binaire : plus que le tampon de 4096 octets");
        const std::vector<std::uint8_t> start = write_and_read_binary(big, 5000);
        check(start.size() == 5000 && start[4999] == big[4999], "binaire : max_bytes limite la lecture");
        check(write_and_read_binary(bytes, 100) == bytes, "binaire : max_bytes plus grand que le fichier");

        bool thrown_binary = false;
        try {
            nerith::read_binary_file("ce_fichier_n_existe_pas.xex");
        } catch (const std::runtime_error&) {
            thrown_binary = true;
        }
        check(thrown_binary, "binaire : un fichier inexistant leve une exception");
    }

    if (failures == 0) {
        std::cout << "Tous les tests passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
