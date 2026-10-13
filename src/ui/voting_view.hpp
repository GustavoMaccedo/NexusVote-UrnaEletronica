// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "core/domain.hpp"

namespace nexusvote::ui {

struct KeyEvent {
    enum class Key {
        Digit0,
        Digit1,
        Digit2,
        Digit3,
        Digit4,
        Digit5,
        Digit6,
        Digit7,
        Digit8,
        Digit9,
        Blank,
        Correct,
        Confirm,
        Cancel
    };
    Key key = Key::Cancel;
    char digit = 0;
};

struct ViewMessages {
    std::string banner = kNonOfficialBanner;
    std::string title;
    std::string typed_digits;
    std::string preview_text;
    std::string status_text;
    bool confirm_enabled = false;
};

class IVotingView {
  public:
    virtual ~IVotingView() = default;
    virtual void show(const ViewMessages& msg) = 0;
    virtual void focusConfirm(bool enabled) = 0;
    virtual void beep() = 0;
    virtual void beepConfirm() = 0;
    virtual void beepFim() = 0;
    virtual void setLargeFont(bool on) = 0;
    virtual void setHighContrast(bool on) = 0;
};

} // namespace nexusvote::ui
