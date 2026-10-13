// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "keypad_evdev.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>

#ifdef __linux__
#include <linux/input-event-codes.h>
#include <linux/input.h>
#else
#define EV_KEY 0x01
#define KEY_1 2
#define KEY_2 3
#define KEY_3 4
#define KEY_4 5
#define KEY_5 6
#define KEY_6 7
#define KEY_7 8
#define KEY_8 9
#define KEY_9 10
#define KEY_0 11
#define KEY_ENTER 28
#define KEY_A 30
#define KEY_D 32
#define KEY_C 46
#define KEY_B 48
#define KEY_KPASTERISK 55
#define KEY_KPSLASH 98
struct input_event {
    struct timeval time;
    unsigned short type, code;
    int value;
};
#endif

namespace nexusvote::keypad {

std::string devicePath() {
    if (const char* e = std::getenv("NV_KEYPAD_DEV")) {
        if (e[0] != '\0')
            return e;
    }
    return "/dev/input/nv-keypad";
}

bool codeToKey(int code, Press& out) {
    out = Press{};
    switch (code) {
    case KEY_1:
        out = {true, Key::Digit1, '1'};
        return true;
    case KEY_2:
        out = {true, Key::Digit2, '2'};
        return true;
    case KEY_3:
        out = {true, Key::Digit3, '3'};
        return true;
    case KEY_4:
        out = {true, Key::Digit4, '4'};
        return true;
    case KEY_5:
        out = {true, Key::Digit5, '5'};
        return true;
    case KEY_6:
        out = {true, Key::Digit6, '6'};
        return true;
    case KEY_7:
        out = {true, Key::Digit7, '7'};
        return true;
    case KEY_8:
        out = {true, Key::Digit8, '8'};
        return true;
    case KEY_9:
        out = {true, Key::Digit9, '9'};
        return true;
    case KEY_0:
        out = {true, Key::Digit0, '0'};
        return true;
    case KEY_A:
        out = {true, Key::Branco, 0};
        return true;
    case KEY_B:
        out = {true, Key::Corrige, 0};
        return true;
    case KEY_C:
    case KEY_ENTER:
    case KEY_KPSLASH:
        out = {true, Key::Confirma, 0};
        return true;
    case KEY_KPASTERISK:
        out = {true, Key::Cancela, 0};
        return true;
    default:
        return false;
    }
}

EvdevKeypad::~EvdevKeypad() {
    if (fd_ >= 0)
        ::close(fd_);
}

bool EvdevKeypad::open(std::string& err) {
    std::string dev = devicePath();
    if (!std::filesystem::exists(dev)) {
        for (int n = 0; n < 32; ++n) {
            std::string sys = "/sys/class/input/event" + std::to_string(n) + "/device/name";
            FILE* f = std::fopen(sys.c_str(), "r");
            if (!f)
                continue;
            char name[128] = {0};
            size_t r = std::fread(name, 1, sizeof(name) - 1, f);
            std::fclose(f);
            std::string s(name, r);
            for (auto& c : s)
                c = (char)tolower(c);
            if (s.find("matrix") != std::string::npos || s.find("keypad") != std::string::npos) {
                dev = "/dev/input/event" + std::to_string(n);
                break;
            }
        }
    }
    fd_ = ::open(dev.c_str(), O_RDONLY);
    if (fd_ < 0) {
        err = "teclado matricial ausente (" + dev + "; dtoverlay=nv-keypad-4x4 + reboot)";
        return false;
    }
    return true;
}

bool EvdevKeypad::readPress(Press& out, std::string& err) {
    if (fd_ < 0) {
        err = "teclado fechado";
        return false;
    }
    for (;;) {
        struct input_event ev{};
        ssize_t got = 0;
        char* p = reinterpret_cast<char*>(&ev);
        size_t need = sizeof(ev);
        while (need > 0) {
            ssize_t r = ::read(fd_, p + got, need);
            if (r == 0) {
                err = "teclado EOF (desconectado?)";
                return false;
            }
            if (r < 0) {
                err = "evdev read (IO)";
                return false;
            }
            got += r;
            need -= (size_t)r;
        }
        if (ev.type != EV_KEY || ev.value != 1)
            continue;
        Press pr;
        if (codeToKey(ev.code, pr)) {
            out = pr;
            return true;
        }
    }
}

} // namespace nexusvote::keypad
