// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "lab/keypad_evdev.hpp"
#include "paper/paper_escpos.hpp"
#include "test_framework.hpp"

using namespace nexusvote;

TEST(KeypadEvdev, Digitos) {
    keypad::Press p;
    EXPECT_TRUE(keypad::codeToKey(2, p));
    EXPECT_TRUE(p.has && p.digit == '1');
    EXPECT_TRUE(keypad::codeToKey(11, p));
    EXPECT_TRUE(p.has && p.digit == '0');
}

TEST(KeypadEvdev, Funcoes) {
    keypad::Press p;
    EXPECT_TRUE(keypad::codeToKey(30, p));
    EXPECT_TRUE(p.has && p.key == keypad::Key::Branco);
    EXPECT_TRUE(keypad::codeToKey(48, p));
    EXPECT_TRUE(p.has && p.key == keypad::Key::Corrige);
    EXPECT_TRUE(keypad::codeToKey(46, p));
    EXPECT_TRUE(p.has && p.key == keypad::Key::Confirma);
    EXPECT_TRUE(keypad::codeToKey(28, p));
    EXPECT_TRUE(p.has && p.key == keypad::Key::Confirma);
    EXPECT_TRUE(keypad::codeToKey(55, p));
    EXPECT_TRUE(p.has && p.key == keypad::Key::Cancela);
    EXPECT_TRUE(!keypad::codeToKey(32, p));
}

TEST(KeypadEvdev, SemHwFalha) {
    setenv("NV_KEYPAD_DEV", "/dev/input/nv-inexistente-xyz", 1);
    keypad::EvdevKeypad k;
    std::string err;
    EXPECT_TRUE(!k.open(err));
    EXPECT_TRUE(!err.empty());
    unsetenv("NV_KEYPAD_DEV");
}

TEST(PapelEscpos, JobTemInitQrCorte) {
    std::string job = paper::buildJob("ZERESIMA", "LAB-01", {"TOTAL: 0"}, "{\"s\":\"X\"}");
    EXPECT_TRUE(job.size() > 16);
    EXPECT_TRUE(job[0] == '\x1B' && job[1] == '\x40');
    std::string qrprint("\x1D\x28\x6B\x03\x00\x31\x51\x30", 8);
    EXPECT_TRUE(job.find(qrprint) != std::string::npos);
    std::string corte("\x1D\x56\x42\x00", 4);
    EXPECT_TRUE(job.size() >= 4 && job.substr(job.size() - 4) == corte);
}

TEST(PapelEscpos, Ausente) {
    auto r = paper::printRaw("/dev/usb/lp-inexistente-xyz", "\x0A");
    EXPECT_TRUE(r.outcome == paper::PrintOutcome::Ausente);
    EXPECT_TRUE(!r.detail.empty());
}
