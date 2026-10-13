// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

#include <string>

#include "domain.hpp"

namespace nexusvote {

BallotPreview classifyBallot(const ElectionConfig& cfg, const ElectionRules& rules,
                             const std::string& office_id, const std::string& digits,
                             bool blank_requested);

}
