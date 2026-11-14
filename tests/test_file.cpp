#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

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

    if (failures == 0) {
        std::cout << "Tous les tests passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
