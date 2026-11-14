#pragma once

#include <iosfwd>
#include <string>

namespace nerith {

// Niveaux de log, du plus bavard au plus grave. Off coupe tout.
enum class LogLevel {
    Debug,     // details pour comprendre ce qui se passe
    Info,      // etapes importantes
    Warning,   // quelque chose d'inattendu, mais le programme continue
    Error,     // quelque chose a rate
    Off        // aucun message
};

// Seuil : un message est affiche seulement s'il est au moins de ce niveau.
// Par defaut : Warning (on ne voit que les avertissements et les erreurs).
void set_log_level(LogLevel level);
LogLevel log_level();

// Ou vont les messages. Par defaut : std::cerr.
// Le flux doit rester valide tant qu'il est utilise (utile surtout pour les tests).
void set_log_stream(std::ostream& stream);

// Ecrit "[niveau] message" sur le flux, si le niveau passe le seuil.
void log(LogLevel level, const std::string& message);

inline void log_debug(const std::string& message)   { log(LogLevel::Debug, message); }
inline void log_info(const std::string& message)    { log(LogLevel::Info, message); }
inline void log_warning(const std::string& message) { log(LogLevel::Warning, message); }
inline void log_error(const std::string& message)   { log(LogLevel::Error, message); }

}  // namespace nerith
