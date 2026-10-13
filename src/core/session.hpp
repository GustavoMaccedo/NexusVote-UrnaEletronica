// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

// Sessão de laboratório: única fachada do núcleo (CLI, TUI, Qt, app usam esta).

#include <cstdint>
#include <string>
#include <vector>

#include "domain.hpp"
#include "rules.hpp"
#include "state_machine.hpp"
#include "tally.hpp"
#include "vote_store.hpp"

namespace nexusvote {

struct OpResult {
    bool ok = false;
    std::string error;
    std::string info;
};

class VotingSession {
  public:
    VotingSession(const std::string& workdir, bool lab_test_mode);

    OpResult loadConfig(const std::string& config_path);
    OpResult open();
    OpResult resume();

    std::vector<std::string> officeOrder() const;
    BallotPreview preview(const std::string& office_id, const std::string& digits,
                          bool blank_requested) const;
    std::string beginConfirm(const std::string& office_id);
    OpResult confirm(const std::string& office_id, const std::string& digits, bool blank_requested,
                     const std::string& token);

    OpResult close(const std::string& closed_at_iso);
    const SessionTally& tally() const {
        return tally_;
    }
    OpResult exportAll(const std::string& outdir, const std::string& build_info_path = "");

    SessionState state() const {
        return state_;
    }
    const ElectionConfig& config() const {
        return config_;
    }
    std::string sessionId() const {
        return session_id_;
    }
    std::string configPath() const {
        return config_path_;
    }
    std::string workdir() const {
        return workdir_;
    }
    std::size_t recordCount() const;
    std::uint64_t completedRounds() const {
        return completed_rounds_;
    }
    std::uint64_t justificativas() const {
        return justificativas_;
    }
    OpResult justifica(const std::string& motivo);

    // Portão de modo laboratório: comandos de teste só com LAB_TEST_MODE=ON.
    bool labTestMode() const {
        return lab_test_mode_;
    }
    OpResult checkLabCommand(const std::string& cmd) const;

  private:
    OpResult setState(SessionState to);
    OpResult logEvent(const std::string& line);
    bool persistSessionState(const std::string& state_str);

    std::string workdir_;
    bool lab_test_mode_ = false;
    SessionState state_ = SessionState::Unconfigured;
    ElectionConfig config_;
    ElectionRules rules_;
    std::string config_path_;
    std::string config_sha_;
    std::string session_id_;
    std::string opened_at_;
    std::string closed_at_;
    VoteStore store_{""};
    SessionTally tally_;
    std::uint64_t confirm_counter_ = 0;
    std::uint64_t completed_rounds_ = 0;
    std::uint64_t round_confirms_ = 0;
    std::uint64_t justificativas_ = 0;
    std::map<std::string, std::vector<std::string>> round_refs_;
    std::string last_token_;
    bool token_used_ = true;
    bool clock_trusted_ = true;
    bool state_dirty_ = false;
};

} // namespace nexusvote
