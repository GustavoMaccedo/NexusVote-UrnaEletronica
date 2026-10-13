// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

#include <cstdlib>
#include <string>

namespace nexusvote::policy {

inline bool terminalStrict() {
    const char* v = std::getenv("NEXUSVOTE_TERMINAL");
    return v != nullptr && std::string(v) == "1";
}

} // namespace nexusvote::policy
