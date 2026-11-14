#include "nerith/log.h"

#include <iostream>

namespace nerith {

namespace {

LogLevel g_level = LogLevel::Warning;
std::ostream* g_stream = nullptr;   // nullptr : on utilise std::cerr

const char* level_name(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:   return "debug";
        case LogLevel::Info:    return "info";
        case LogLevel::Warning: return "warning";
        case LogLevel::Error:   return "error";
        case LogLevel::Off:     return "off";
    }
    return "?";
}

}  // namespace

void set_log_level(LogLevel level) {
    g_level = level;
}

LogLevel log_level() {
    return g_level;
}

void set_log_stream(std::ostream& stream) {
    g_stream = &stream;
}

void log(LogLevel level, const std::string& message) {
    // Off n'est pas un niveau de message : il sert seulement de seuil
    if (level == LogLevel::Off || level < g_level) {
        return;
    }
    std::ostream& out = (g_stream != nullptr) ? *g_stream : std::cerr;
    out << "[" << level_name(level) << "] " << message << "\n";
}

}  // namespace nerith
