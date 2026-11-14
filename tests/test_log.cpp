#include <iostream>
#include <sstream>
#include <string>

#include "nerith/log.h"

namespace {

int failures = 0;

void check(bool condition, const char* name) {
    if (!condition) {
        std::cerr << "ECHEC : " << name << "\n";
        ++failures;
    }
}

}  // namespace

int main() {
    using namespace nerith;

    std::ostringstream out;
    set_log_stream(out);

    {   // niveau par defaut : Warning
        check(log_level() == LogLevel::Warning, "le niveau par defaut est Warning");
        log_debug("d");
        log_info("i");
        check(out.str().empty(), "Debug et Info sont masques par defaut");
        log_warning("attention");
        check(out.str() == "[warning] attention\n", "Warning est affiche, avec son format");
        log_error("oups");
        check(out.str() == "[warning] attention\n[error] oups\n", "Error est affiche aussi");
    }
    {   // niveau Debug : tout passe
        out.str("");
        set_log_level(LogLevel::Debug);
        log_debug("a");
        log_info("b");
        check(out.str() == "[debug] a\n[info] b\n", "niveau Debug : tout est affiche");
    }
    {   // niveau Error : seulement les erreurs
        out.str("");
        set_log_level(LogLevel::Error);
        log_warning("w");
        log_error("e");
        check(out.str() == "[error] e\n", "niveau Error : seules les erreurs passent");
    }
    {   // niveau Off : rien du tout
        out.str("");
        set_log_level(LogLevel::Off);
        log_error("e");
        log(LogLevel::Off, "x");
        check(out.str().empty(), "niveau Off : aucun message");
    }

    set_log_level(LogLevel::Warning);   // on remet la valeur par defaut

    if (failures == 0) {
        std::cout << "Tous les tests passent\n";
        return 0;
    }
    std::cerr << failures << " test(s) en echec\n";
    return 1;
}
