// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "core/roles.hpp"

#include <cstdio>
#include <filesystem>
#include <system_error>
#include <unistd.h>

#include "core/crypto_adapter.hpp"

namespace nexusvote::roles {

std::string roleName(Role r) {
    switch (r) {
    case Role::Voter:
        return "VOTER";
    case Role::PollWorker:
        return "POLL_WORKER";
    case Role::Auditor:
        return "AUDITOR";
    }
    return "VOTER";
}

Habilitador::Habilitador(ClockFn clock, const std::string& file)
    : clock_(std::move(clock)), file_(file) {
    load();
}

void Habilitador::load() {
    if (file_.empty()) {
        return;
    }
    FILE* f = std::fopen(file_.c_str(), "r");
    if (f == nullptr) {
        return;
    }
    char id[65];
    unsigned long long issued = 0;
    int used = 0;
    while (std::fscanf(f, "%64s %llu %d", id, &issued, &used) == 3) {
        EnableToken t;
        t.id = id;
        t.role = Role::Voter;
        t.issued_epoch = static_cast<std::uint64_t>(issued);
        t.used = (used != 0);
        if (t.id.size() == 32) {
            tokens_[t.id] = t;
        }
    }
    std::fclose(f);
}

bool Habilitador::persist() {
    if (file_.empty()) {
        return true;
    }
    const std::string::size_type slash = file_.rfind('/');
    if (slash != std::string::npos) {
        std::error_code ec;
        std::filesystem::create_directories(file_.substr(0, slash), ec);
        if (ec) {
            return false;
        }
    }
    const std::string tmp = file_ + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "w");
    if (f == nullptr) {
        return false;
    }
    for (const auto& kv : tokens_) {
        std::fprintf(f, "%s %llu %d\n", kv.second.id.c_str(),
                     static_cast<unsigned long long>(kv.second.issued_epoch),
                     kv.second.used ? 1 : 0);
    }
    std::fflush(f);
    const int fd = fileno(f);
    if (fd >= 0) {
        fsync(fd);
    }
    std::fclose(f);
    return std::rename(tmp.c_str(), file_.c_str()) == 0;
}

EnableToken Habilitador::issue(Role role, std::string& err) {
    bool ok = false;
    const std::string id = crypto::randomHex(16, ok);
    if (!ok) {
        err = "RNG indisponível";
        return EnableToken{};
    }
    EnableToken t;
    t.id = id;
    t.role = role;
    t.issued_epoch = clock_();
    tokens_[id] = t;
    if (!persist()) {
        tokens_.erase(id);
        err = "falha ao persistir token";
        return EnableToken{};
    }
    return t;
}

bool Habilitador::consume(const std::string& id) {
    const auto it = tokens_.find(id);
    if (it == tokens_.end() || it->second.used) {
        return false;
    }
    const std::uint64_t now = clock_();
    if (now < it->second.issued_epoch || now - it->second.issued_epoch > kEnableTtlSecs) {
        tokens_.erase(it);
        return false;
    }
    it->second.used = true;
    persist();
    return true;
}

std::size_t Habilitador::pending() {
    const std::uint64_t now = clock_();
    std::size_t n = 0;
    for (auto it = tokens_.begin(); it != tokens_.end();) {
        const bool expired =
            now < it->second.issued_epoch || now - it->second.issued_epoch > kEnableTtlSecs;
        if (expired && !it->second.used) {
            it = tokens_.erase(it);
        } else {
            if (!it->second.used) {
                ++n;
            }
            ++it;
        }
    }
    return n;
}

} // namespace nexusvote::roles
