// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include <QApplication>
#include <QPushButton>
#include <QtGlobal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "../src/core/config_loader.hpp"
#include "../src/core/session.hpp"
#include "../src/ui/qt/qt_controller.hpp"
#include "../src/ui/qt/qt_voting_window.hpp"
#include "test_framework.hpp"

namespace {

int qt_argc = 1;
char qt_arg0[] = {'t', 'e', 's', 't', '\0'};
char* qt_argv[] = {qt_arg0, nullptr};

bool qtReady() {
    static bool inited = false;
    static bool ok = false;
    if (!inited) {
        inited = true;
        qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
        QApplication* app = nullptr;
        try {
            app = new QApplication(qt_argc, qt_argv);
        } catch (...) {
            app = nullptr;
        }
        ok = (app != nullptr);
    }
    return ok;
}

} // namespace

TEST(QtWindow, ContractDoesNotCrash) {
    if (!qtReady()) {
        std::puts("  SKIPPED QtWindow.ContractDoesNotCrash (sem plataforma Qt)");
        return;
    }
    nexusvote::ui::QtVotingWindow w;
    nexusvote::ui::ViewMessages m;
    m.title = "Cargo teste";
    m.typed_digits = "11";
    m.preview_text = "revisão";
    m.status_text = "estado";
    m.confirm_enabled = true;
    w.show(m);
    w.focusConfirm(true);
    w.beep();
    w.setLargeFont(true);
    w.setHighContrast(true);
    w.setLargeFont(false);
    w.setHighContrast(false);
    EXPECT_TRUE(true);
}

TEST(QtWindow, DrivesSessionThroughContract) {
    if (!qtReady()) {
        std::puts("  SKIPPED QtWindow.DrivesSessionThroughContract (sem plataforma Qt)");
        return;
    }
    std::string dir = std::string(TMP_BASE) + "/qt-drive";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    nexusvote::VotingSession s(dir, true);
    EXPECT_TRUE(s.loadConfig(std::string(CONFIG_DIR) + "/demo-election.json").ok);
    EXPECT_TRUE(s.open().ok);
    nexusvote::ui::QtVotingWindow w;
    auto pv = s.preview("alcaide", "11", false);
    EXPECT_TRUE(pv.type == nexusvote::PreviewType::Nominal);
    nexusvote::ui::ViewMessages m;
    m.title = "alcaide";
    m.typed_digits = "11";
    m.preview_text = pv.message_pt;
    m.confirm_enabled = true;
    w.show(m);
    std::string tok = s.beginConfirm("alcaide");
    EXPECT_TRUE(s.confirm("alcaide", "11", false, tok).ok);
    EXPECT_EQ(s.recordCount(), 1u);
    EXPECT_TRUE(s.close("").ok);
}

TEST(QtController, NeutralHidesVoteData) {
    nexusvote::BallotPreview pv;
    pv.type = nexusvote::PreviewType::Nominal;
    pv.message_pt = "Candidato: Ada Fictícia (11111). Confirme para registrar.";
    auto shown = nexusvote::ui::buildOfficeView("vereador", "Vereador", "11111", false, pv);
    EXPECT_TRUE(shown.preview_text.find("Ada") != std::string::npos);
    auto neutral = nexusvote::ui::neutralMessage("vereador");
    EXPECT_TRUE(neutral.typed_digits.find("11111") == std::string::npos);
    EXPECT_TRUE(neutral.preview_text.find("Ada") == std::string::npos);
    EXPECT_FALSE(neutral.confirm_enabled);
}

TEST(QtWindow, ButtonsHaveAccessibleNames) {
    if (!qtReady()) {
        std::puts("  SKIPPED QtWindow.ButtonsHaveAccessibleNames (sem plataforma Qt)");
        return;
    }
    nexusvote::ui::QtVotingWindow w;
    auto buttons = w.findChildren<QPushButton*>();
    EXPECT_TRUE(!buttons.empty());
    for (auto* b : buttons) {
        EXPECT_TRUE(!b->accessibleName().isEmpty());
        EXPECT_TRUE(!b->accessibleDescription().isEmpty());
    }
}

TEST(QtWindow, DoubleConfirmViaKeysKeepsSingleRecord) {
    if (!qtReady()) {
        std::puts("  SKIPPED QtWindow.DoubleConfirmViaKeysKeepsSingleRecord");
        return;
    }
    std::string dir = std::string(TMP_BASE) + "/qt-dbl";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    nexusvote::VotingSession s(dir, true);
    EXPECT_TRUE(s.loadConfig(std::string(CONFIG_DIR) + "/demo-election.json").ok);
    EXPECT_TRUE(s.open().ok);
    nexusvote::ui::QtVotingWindow w;
    std::string tok = s.beginConfirm("alcaide");
    EXPECT_TRUE(s.confirm("alcaide", "11", false, tok).ok);
    w.show(nexusvote::ui::neutralMessage("alcaide"));
    EXPECT_FALSE(s.confirm("alcaide", "11", false, tok).ok);
    EXPECT_EQ(s.recordCount(), 1u);
}
