// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "state_machine.hpp"

namespace nexusvote {

std::string stateToString(SessionState s) {
    switch (s) {
    case SessionState::Unconfigured:
        return "UNCONFIGURED";
    case SessionState::Ready:
        return "READY";
    case SessionState::Open:
        return "OPEN";
    case SessionState::Voting:
        return "VOTING";
    case SessionState::Closing:
        return "CLOSING";
    case SessionState::Closed:
        return "CLOSED";
    case SessionState::Error:
        return "ERROR";
    case SessionState::RecoveryRequired:
        return "RECOVERY_REQUIRED";
    }
    return "ERROR";
}

bool canTransition(SessionState from, SessionState to) {
    switch (from) {
    case SessionState::Unconfigured:
        return to == SessionState::Ready || to == SessionState::Error;
    case SessionState::Ready:
        return to == SessionState::Open || to == SessionState::Error;
    case SessionState::Open:
        return to == SessionState::Voting || to == SessionState::Closing ||
               to == SessionState::Error || to == SessionState::Open;
    case SessionState::Voting:
        return to == SessionState::Voting || to == SessionState::Open ||
               to == SessionState::Closing || to == SessionState::Error ||
               to == SessionState::RecoveryRequired;
    case SessionState::Closing:
        return to == SessionState::Closed || to == SessionState::Error;
    case SessionState::Closed:
        return to == SessionState::Error;
    case SessionState::Error:
        return to == SessionState::RecoveryRequired || to == SessionState::Error;
    case SessionState::RecoveryRequired:
        return to == SessionState::Open || to == SessionState::Error;
    }
    return false;
}

} // namespace nexusvote
