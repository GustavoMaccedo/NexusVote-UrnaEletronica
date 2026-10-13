// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

#include <string>

#include "domain.hpp"

namespace nexusvote {

struct TallyResult {
    SessionTally tally;
    bool ok = false;
    std::string error;
};

TallyResult tallyVotes(const ElectionConfig& cfg, const std::vector<VoteRecord>& records);

} // namespace nexusvote
