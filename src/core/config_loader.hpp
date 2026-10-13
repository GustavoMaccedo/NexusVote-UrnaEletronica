// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

#include <string>

#include "domain.hpp"

namespace nexusvote {

struct LoadResult {
    ElectionConfig config;
    bool ok = false;
    std::string error;
};

LoadResult loadConfigFromText(const std::string& text);
LoadResult loadConfigFromFile(const std::string& path);
std::string configErrorPrefix(const std::string& field, const std::string& msg);

} // namespace nexusvote
