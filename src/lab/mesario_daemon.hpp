// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

// Em build não-laboratório, HABILITA responde ERR not_lab_build.

#include <cstdint>
#include <functional>
#include <string>

#include "core/roles.hpp"

namespace nexusvote::mesario {

inline constexpr std::size_t kMaxLine = 4096;

class MesarioDaemon {
  public:
    MesarioDaemon(const std::string& workdir, bool lab_test_mode, roles::ClockFn clock = {});
    std::string socketPath() const {
        return workdir_ + "/mesario.sock";
    }

    std::string handleLine(const std::string& line);

    int serve(bool oneshot);

    std::string phc() const {
        return phc_;
    }

  private:
    static std::string loadPhc(const std::string& workdir);
    bool checkPin(const std::string& pin);
    bool rateLimited(std::uint64_t now);

    std::string workdir_;
    bool lab_test_mode_ = false;
    roles::Habilitador habilitador_;
    std::string phc_;
    std::uint64_t fail_window_start_ = 0;
    unsigned fail_count_ = 0;
};

} // namespace nexusvote::mesario
