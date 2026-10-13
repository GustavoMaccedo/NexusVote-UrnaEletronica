// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

// Papéis e interlock mesário→eleitor (espelho do terminal duplo oficial).

#include <cstdint>
#include <functional>
#include <map>
#include <string>

namespace nexusvote::roles {

enum class Role { Voter, PollWorker, Auditor };

inline constexpr std::uint64_t kEnableTtlSecs = 300;

struct EnableToken {
    std::string id;
    Role role = Role::Voter;
    std::uint64_t issued_epoch = 0;
    bool used = false;
};

using ClockFn = std::function<std::uint64_t()>;

class Habilitador {
  public:
    explicit Habilitador(ClockFn clock, const std::string& file = "");

    EnableToken issue(Role role, std::string& err);

    bool consume(const std::string& id);

    std::size_t pending();

  private:
    bool persist();
    void load();

    ClockFn clock_;
    std::string file_;
    std::map<std::string, EnableToken> tokens_;
};

std::string roleName(Role r);

} // namespace nexusvote::roles
