// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "lab/mesario_daemon.hpp"

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "../core/policy.hpp"
#include "../secure/secure_session.hpp"
#include "peercred.hpp"

namespace nexusvote::mesario {

namespace {
std::uint64_t epochNow() {
    const std::time_t t = std::time(nullptr);
    return t < 0 ? 0 : static_cast<std::uint64_t>(t);
}

std::string trimCr(const std::string& s) {
    if (!s.empty() && s.back() == '\r') {
        return s.substr(0, s.size() - 1);
    }
    return s;
}

bool readLine(int fd, std::string& out) {
    out.clear();
    char c = 0;
    while (out.size() < kMaxLine) {
        const ssize_t n = ::read(fd, &c, 1);
        if (n <= 0) {
            return false;
        }
        if (c == '\n') {
            return true;
        }
        out.push_back(c);
    }
    return false;
}

bool writeAll(int fd, const char* p, std::size_t n) {
    while (n > 0) {
        const ssize_t w = ::write(fd, p, n);
        if (w <= 0) {
            return false;
        }
        p += w;
        n -= static_cast<std::size_t>(w);
    }
    return true;
}
} // namespace

MesarioDaemon::MesarioDaemon(const std::string& workdir, bool lab_test_mode, roles::ClockFn clock)
    : workdir_(workdir), lab_test_mode_(lab_test_mode),
      habilitador_(clock ? std::move(clock) : roles::ClockFn(epochNow), workdir + "/tokens.txt"),
      phc_(loadPhc(workdir)) {}

std::string MesarioDaemon::loadPhc(const std::string& workdir) {
    std::ifstream f(workdir + "/mesario.phc", std::ios::binary);
    if (!f) {
        return {};
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) {
        s.pop_back();
    }
    if (s.size() > 512 || s.rfind("$argon2", 0) != 0) {
        return {};
    }
    return s;
}

bool MesarioDaemon::rateLimited(std::uint64_t now) {
    if (fail_count_ >= 5 && now - fail_window_start_ < 60) {
        return true;
    }
    if (now - fail_window_start_ >= 60) {
        fail_count_ = 0;
        fail_window_start_ = now;
    }
    return false;
}

bool MesarioDaemon::checkPin(const std::string& pin) {
    if (phc_.empty()) {
        return lab_test_mode_;
    }
    if (!secure::isEnabled()) {
        return false;
    }
    std::string err;
    return secure::verifyPin(pin, phc_, err);
}

std::string MesarioDaemon::handleLine(const std::string& raw) {
    const std::string line = trimCr(raw);
    if (line == "HELLO mesario-v1") {
        return std::string("OK mesario-v1 ") + (lab_test_mode_ ? "lab" : "prod");
    }
    if (line == "STATUS") {
        return "OK pendentes=" + std::to_string(habilitador_.pending());
    }
    if (line == "SAIR") {
        return "OK bye";
    }
    if (line == "HABILITA ELEITOR" || line.rfind("HABILITA ELEITOR ", 0) == 0) {
        if (!lab_test_mode_ && phc_.empty()) {
            return "ERR not_lab_build";
        }
        const std::string pin = line.size() > 17 ? line.substr(17) : std::string();
        const std::uint64_t now_s = static_cast<std::uint64_t>(std::time(nullptr));
        if (rateLimited(now_s)) {
            return "ERR bloqueado";
        }
        if (!checkPin(pin)) {
            if (fail_count_ == 0) {
                fail_window_start_ = now_s;
            }
            ++fail_count_;
            return "ERR pin";
        }
        fail_count_ = 0;
        std::string err;
        roles::EnableToken t = habilitador_.issue(roles::Role::Voter, err);
        if (t.id.empty()) {
            return "ERR rng";
        }
        return "OK token=" + t.id + " ttl=300";
    }
    return "ERR comando";
}

int MesarioDaemon::serve(bool oneshot) {
    std::error_code ec;
    std::filesystem::create_directories(workdir_, ec);
    if (ec) {
        return 2;
    }
    ::chmod(workdir_.c_str(), 0700);
    std::string path = socketPath();
    ::unlink(path.c_str());
    int srv = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv < 0) {
        return 2;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (::bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        ::listen(srv, 4) != 0) {
        ::close(srv);
        return 2;
    }
    ::chmod(path.c_str(), 0700);
    int handled = 0;
    for (;;) {
        pollfd p{};
        p.fd = srv;
        p.events = POLLIN;
        if (poll(&p, 1, oneshot && handled > 0 ? 1000 : -1) <= 0) {
            break;
        }
        int fd = ::accept(srv, nullptr, nullptr);
        if (fd < 0) {
            break;
        }
        lab::PeerCred cred = lab::peerCred(fd);
        if (!lab::peerAllowed(cred, policy::terminalStrict())) {
            ::close(fd);
            if (++handled > 0 && oneshot) {
                break;
            }
            continue;
        }
        std::string line;
        std::string resp = readLine(fd, line) ? handleLine(line) : "ERR linha";
        resp.push_back('\n');
        writeAll(fd, resp.data(), resp.size());
        ::close(fd);
        if (++handled > 0 && oneshot) {
            break;
        }
    }
    ::close(srv);
    ::unlink(path.c_str());
    return 0;
}

} // namespace nexusvote::mesario
