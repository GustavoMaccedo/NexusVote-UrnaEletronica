// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

// Respondedor de laboratório NVLP sobre socket Unix local (stream).
// Em build não-laboratório, responde NAK not_lab_build a tudo.

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "nvlp.hpp"

namespace nexusvote::lab {

struct RunSlot {
    std::string request_hash;
    std::vector<std::uint8_t> response;
    bool done = false;
};

class LabDaemon {
  public:
    LabDaemon(const std::string& workdir, bool lab_test_mode);
    std::string socketPath() const {
        return workdir_ + "/lab.sock";
    }

    int serve(bool oneshot);

    nvlp::Frame handleFrame(const nvlp::Frame& in);

  private:
    nvlp::Frame nak(const std::string& run_id, const std::string& reason);
    nvlp::Frame runSuite(const std::string& run_id, const std::string& body);

    std::string workdir_;
    bool lab_test_mode_ = false;
    std::map<std::string, RunSlot> runs_;
    nvlp::NonceGuard nonces_;
    std::size_t rejected_peers_ = 0;
};

} // namespace nexusvote::lab
