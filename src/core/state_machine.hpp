// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

// Máquina de estados explícita da sessão de laboratório.

#include <string>

namespace nexusvote {

enum class SessionState {
    Unconfigured,
    Ready,
    Open,
    Voting,
    Closing,
    Closed,
    Error,
    RecoveryRequired
};

std::string stateToString(SessionState s);
bool canTransition(SessionState from, SessionState to);

} // namespace nexusvote
