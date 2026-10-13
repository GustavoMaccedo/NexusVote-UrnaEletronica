// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

#include <string>

namespace nexusvote::keypad {

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
    Branco,
    Corrige,
    Confirma,
    Cancela,
    Ignorada
};

struct Press {
    bool has = false;
    Key key = Key::Ignorada;
    char digit = 0;
};

std::string devicePath();
bool codeToKey(int code, Press& out);

class EvdevKeypad {
  public:
    EvdevKeypad() = default;
    ~EvdevKeypad();
    EvdevKeypad(const EvdevKeypad&) = delete;
    EvdevKeypad& operator=(const EvdevKeypad&) = delete;
    bool open(std::string& err);
    bool readPress(Press& out, std::string& err);
    bool isOpen() const {
        return fd_ >= 0;
    }

  private:
    int fd_ = -1;
};

} // namespace nexusvote::keypad
