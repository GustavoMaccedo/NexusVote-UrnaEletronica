// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

#include <string>
#include <vector>

#include "domain.hpp"
#include "tally.hpp"

namespace nexusvote {

std::string buildZeroReport(const std::string& session_id, const std::string& opened_at,
                            const std::string& config_sha);

bool exportAllArtifacts(const ElectionConfig& cfg, const std::string& session_id,
                        const std::string& opened_at, const std::string& closed_at,
                        const std::string& config_sha, const std::vector<VoteRecord>& records,
                        const SessionTally& tally, const std::string& outdir,
                        const std::vector<std::pair<std::string, std::string>>& extra_files,
                        std::string& err);
bool exportAllArtifactsFull(const ElectionConfig& cfg, const std::string& session_id,
                            const std::string& opened_at, const std::string& closed_at,
                            const std::string& config_sha, const std::vector<VoteRecord>& records,
                            const SessionTally& tally, std::uint64_t justificativas,
                            const std::string& outdir,
                            const std::vector<std::pair<std::string, std::string>>& extra_files,
                            std::string& err);

} // namespace nexusvote
