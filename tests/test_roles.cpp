// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "core/roles.hpp"
#include "lab/mesario_daemon.hpp"
#include "secure/secure_session.hpp"
#include "test_framework.hpp"

using nexusvote::roles::Habilitador;
using nexusvote::roles::Role;

TEST(Roles, IssueConsumeOnce) {
    std::uint64_t now = 1'000'000;
    Habilitador h([&] { return now; });
    std::string err;
    auto t = h.issue(Role::Voter, err);
    EXPECT_TRUE(!t.id.empty());
    EXPECT_TRUE(t.id.size() == 32);
    EXPECT_EQ(h.pending(), 1ULL);
    EXPECT_TRUE(h.consume(t.id));
    EXPECT_TRUE(!h.consume(t.id));
    EXPECT_EQ(h.pending(), 0ULL);
}

TEST(Roles, ExpiredRejected) {
    std::uint64_t now = 1'000'000;
    Habilitador h([&] { return now; });
    std::string err;
    auto t = h.issue(Role::PollWorker, err);
    now += nexusvote::roles::kEnableTtlSecs + 1;
    EXPECT_TRUE(!h.consume(t.id));
    EXPECT_EQ(h.pending(), 0ULL);
}

TEST(Roles, UnknownRejected) {
    std::uint64_t now = 1'000'000;
    Habilitador h([&] { return now; });
    EXPECT_TRUE(!h.consume("0123456789abcdef0123456789abcdef"));
}

namespace {

std::string mesTmp(const std::string& name) {
    std::string p = std::string(TMP_BASE) + "/" + name;
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
    std::filesystem::create_directories(p, ec);
    return p;
}

} // namespace

TEST(Mesario, HelloStatusUnknown) {
    nexusvote::mesario::MesarioDaemon d(mesTmp("hello"), true);
    EXPECT_EQ(d.handleLine("HELLO mesario-v1"), "OK mesario-v1 lab");
    EXPECT_EQ(d.handleLine("STATUS"), "OK pendentes=0");
    EXPECT_EQ(d.handleLine("XYZ"), "ERR comando");
    EXPECT_EQ(d.handleLine("SAIR"), "OK bye");
}

TEST(Mesario, HabilitaFlow) {
    nexusvote::mesario::MesarioDaemon d(mesTmp("flow"), true);
    std::string r = d.handleLine("HABILITA ELEITOR");
    EXPECT_TRUE(r.rfind("OK token=", 0) == 0);
    EXPECT_EQ(d.handleLine("STATUS"), "OK pendentes=1");
}

TEST(Mesario, NonLabBuildNaks) {
    nexusvote::mesario::MesarioDaemon d(mesTmp("nonlab"), false);
    EXPECT_EQ(d.handleLine("HABILITA ELEITOR"), "ERR not_lab_build");
    EXPECT_EQ(d.handleLine("HELLO mesario-v1"), "OK mesario-v1 prod");
}

#ifdef NEXUSVOTE_WITH_RUST_SECURE
TEST(Mesario, PinRequiredWhenPhcPresent) {
    std::string dir = "/tmp/nvmes-pin";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    std::string phc, err;
    EXPECT_TRUE(nexusvote::secure::hashPin("492817", phc, err));
    {
        std::ofstream f(dir + "/mesario.phc", std::ios::binary);
        f << phc;
    }
    nexusvote::mesario::MesarioDaemon d(dir, true);
    EXPECT_TRUE(!d.phc().empty());
    EXPECT_EQ(d.handleLine("HABILITA ELEITOR"), "ERR pin");
    EXPECT_EQ(d.handleLine("HABILITA ELEITOR 000000"), "ERR pin");
    std::string r = d.handleLine("HABILITA ELEITOR 492817");
    EXPECT_TRUE(r.rfind("OK token=", 0) == 0);
    for (int i = 0; i < 5; ++i) {
        d.handleLine("HABILITA ELEITOR 111111");
    }
    EXPECT_EQ(d.handleLine("HABILITA ELEITOR 492817"), "ERR bloqueado");
    std::filesystem::remove_all(dir, ec);
}
#endif
