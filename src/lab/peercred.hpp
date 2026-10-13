// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>

namespace nexusvote::lab {

struct PeerCred {
    std::uint32_t uid = 0;
    std::uint32_t gid = 0;
    std::int32_t pid = 0;
    bool ok = false;
};

inline PeerCred peerCred(int fd) {
    PeerCred c;
#ifdef SO_PEERCRED
    struct ucred u{};
    socklen_t len = sizeof(u);
    if (::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &u, &len) == 0 && len >= sizeof(u)) {
        c.uid = u.uid;
        c.gid = u.gid;
        c.pid = u.pid;
        c.ok = true;
    }
#endif
    return c;
}

inline bool peerAllowed(const PeerCred& c, bool terminal_strict) {
    if (!terminal_strict) {
        return true;
    }
    if (!c.ok) {
        return false;
    }
    return c.uid == static_cast<std::uint32_t>(::geteuid());
}

} // namespace nexusvote::lab
