// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

#include "../src/core/config_loader.hpp"
#include "../src/core/session.hpp"
#include "test_framework.hpp"

namespace {

std::string tmpdir(const std::string& name) {
    std::string p = std::string(TMP_BASE) + "/" + name;
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
    std::filesystem::create_directories(p, ec);
    return p;
}

std::string demoPath() {
    return std::string(CONFIG_DIR) + "/demo-election.json";
}

} // namespace

TEST(SessionLifecycle, RejectsBadConfig) {
    nexusvote::VotingSession s(tmpdir("badcfg"), true);
    auto r = s.loadConfig("/caminho/inexistente.json");
    EXPECT_FALSE(r.ok);
}

TEST(SessionLifecycle, OpenIsIdempotent) {
    auto d = tmpdir("idem");
    nexusvote::VotingSession s(d, true);
    EXPECT_TRUE(s.loadConfig(demoPath()).ok);
    auto a = s.open();
    EXPECT_TRUE(a.ok);
    auto b = s.open();
    EXPECT_TRUE(b.ok);
    EXPECT_EQ(s.recordCount(), 0u);
}

TEST(ZeroReport, AllZeroAfterOpen) {
    auto d = tmpdir("zero");
    nexusvote::VotingSession s(d, true);
    EXPECT_TRUE(s.loadConfig(demoPath()).ok);
    EXPECT_TRUE(s.open().ok);
    std::ifstream f(d + "/lab-zeresima.json", std::ios::binary);
    EXPECT_TRUE(static_cast<bool>(f));
    std::stringstream ss;
    ss << f.rdbuf();
    std::string z = ss.str();
    EXPECT_TRUE(z.find("all_zero") != std::string::npos);
    EXPECT_TRUE(z.find(s.sessionId()) != std::string::npos);
    EXPECT_TRUE(z.find("NÃO OFICIAL") != std::string::npos);
}

TEST(SessionLifecycle, ResumeAcrossProcesses) {
    auto d = tmpdir("resume");
    {
        nexusvote::VotingSession s(d, true);
        EXPECT_TRUE(s.loadConfig(demoPath()).ok);
        EXPECT_TRUE(s.open().ok);
        std::string tok = s.beginConfirm("alcaide");
        EXPECT_TRUE(s.confirm("alcaide", "11", false, tok).ok);
    }
    {
        nexusvote::VotingSession s2(d, true);
        EXPECT_TRUE(s2.loadConfig(demoPath()).ok);
        auto rr = s2.resume();
        EXPECT_TRUE(rr.ok);
        EXPECT_EQ(s2.recordCount(), 1u);
    }
}

TEST(SessionLifecycle, NoVotesAfterClose) {
    auto d = tmpdir("noclose");
    nexusvote::VotingSession s(d, true);
    EXPECT_TRUE(s.loadConfig(demoPath()).ok);
    EXPECT_TRUE(s.open().ok);
    EXPECT_TRUE(s.close("").ok);
    std::string tok = s.beginConfirm("alcaide");
    auto r = s.confirm("alcaide", "11", false, tok);
    EXPECT_FALSE(r.ok);
}

TEST(SessionLifecycle, ConfigImmutableAfterLoad) {
    auto d = tmpdir("immut");
    nexusvote::VotingSession s(d, true);
    EXPECT_TRUE(s.loadConfig(demoPath()).ok);
    auto r = s.loadConfig(demoPath());
    EXPECT_FALSE(r.ok);
}

TEST(VotingFlow, SingleRecordPerConfirm) {
    auto d = tmpdir("single");
    nexusvote::VotingSession s(d, true);
    EXPECT_TRUE(s.loadConfig(demoPath()).ok);
    EXPECT_TRUE(s.open().ok);
    std::string tok = s.beginConfirm("alcaide");
    EXPECT_TRUE(s.confirm("alcaide", "11", false, tok).ok);
    EXPECT_EQ(s.recordCount(), 1u);
}

TEST(VotingFlow, UnconfirmedExcluded) {
    auto d = tmpdir("unconf");
    nexusvote::VotingSession s(d, true);
    EXPECT_TRUE(s.loadConfig(demoPath()).ok);
    EXPECT_TRUE(s.open().ok);
    auto pv = s.preview("alcaide", "11", false);
    EXPECT_TRUE(pv.type == nexusvote::PreviewType::Nominal);
    EXPECT_EQ(s.recordCount(), 0u);
}

TEST(DoubleConfirm, TokenCannotBeReused) {
    auto d = tmpdir("dbl");
    nexusvote::VotingSession s(d, true);
    EXPECT_TRUE(s.loadConfig(demoPath()).ok);
    EXPECT_TRUE(s.open().ok);
    std::string tok = s.beginConfirm("alcaide");
    EXPECT_TRUE(s.confirm("alcaide", "11", false, tok).ok);
    auto r2 = s.confirm("alcaide", "22", false, tok);
    EXPECT_FALSE(r2.ok);
    EXPECT_EQ(s.recordCount(), 1u);
}

TEST(DoubleConfirm, EmptyTokenRejected) {
    auto d = tmpdir("emptytok");
    nexusvote::VotingSession s(d, true);
    EXPECT_TRUE(s.loadConfig(demoPath()).ok);
    EXPECT_TRUE(s.open().ok);
    auto r = s.confirm("alcaide", "11", false, "");
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(s.recordCount(), 0u);
}

TEST(LabModeGate, NonLabBuildRejects) {
    auto d = tmpdir("gate");
    nexusvote::VotingSession s(d, false);
    auto g = s.checkLabCommand("RUN_TEST_SUITE");
    EXPECT_FALSE(g.ok);
}

TEST(LabModeGate, LabBuildAllows) {
    auto d = tmpdir("gate2");
    nexusvote::VotingSession s(d, true);
    EXPECT_TRUE(s.checkLabCommand("RUN_TEST_SUITE").ok);
}

TEST(Permissions, WorkdirIs0700AndJournal0600) {
    auto d = tmpdir("perms");
    nexusvote::VotingSession s(d, true);
    EXPECT_TRUE(s.loadConfig(demoPath()).ok);
    struct stat st{};
    EXPECT_TRUE(::stat(d.c_str(), &st) == 0);
    if ((st.st_mode & 0777) != 0700) {
        std::puts("  SKIPPED Permissions (FS nao honra modo)");
        return;
    }
    EXPECT_TRUE(s.open().ok);
    std::string tok = s.beginConfirm("alcaide");
    EXPECT_TRUE(s.confirm("alcaide", "11", false, tok).ok);
    struct stat jf{};
    EXPECT_TRUE(::stat((d + "/lab-vote-records.jsonl").c_str(), &jf) == 0);
    EXPECT_EQ(jf.st_mode & 0777, 0600u);
}

TEST(Version, SingleVersionAcrossCore) {
    EXPECT_EQ(std::string(nexusvote::kSoftwareVersion), "1.2.0-lab");
}

TEST(SessionLimits, MaxVotersEnforcedPerRound) {
    auto d = tmpdir("maxv");
    {
        std::ifstream f(demoPath(), std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        std::string t = ss.str();
        auto pos = t.find("\"legend_enabled\": true");
        t.insert(pos, "\"test_max_voters\": 1, ");
        std::ofstream o(d + "/cfg.json", std::ios::binary);
        o << t;
    }
    nexusvote::VotingSession s(d, true);
    EXPECT_TRUE(s.loadConfig(d + "/cfg.json").ok);
    EXPECT_TRUE(s.open().ok);
    std::string t1 = s.beginConfirm("vereador");
    EXPECT_TRUE(s.confirm("vereador", "11111", false, t1).ok);
    std::string t2 = s.beginConfirm("alcaide");
    EXPECT_TRUE(s.confirm("alcaide", "11", false, t2).ok);
    EXPECT_EQ(s.completedRounds(), 1u);
    std::string t3 = s.beginConfirm("vereador");
    auto r = s.confirm("vereador", "11111", false, t3);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(s.recordCount(), 2u);
}

TEST(SessionLimits, UntilPastRefusesVotes) {
    auto d = tmpdir("until");
    {
        std::ifstream f(demoPath(), std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        std::string t = ss.str();
        auto pos = t.find("\"legend_enabled\": true");
        t.insert(pos, "\"test_until_iso\": \"2000-01-01T00:00:00Z\", ");
        std::ofstream o(d + "/cfg.json", std::ios::binary);
        o << t;
    }
    nexusvote::VotingSession s(d, true);
    EXPECT_TRUE(s.loadConfig(d + "/cfg.json").ok);
    EXPECT_TRUE(s.open().ok);
    std::string tok = s.beginConfirm("alcaide");
    EXPECT_FALSE(s.confirm("alcaide", "11", false, tok).ok);
    EXPECT_EQ(s.recordCount(), 0u);
}

TEST(SessionExport, BundleContainsExtrasAndManifest) {
    auto d = tmpdir("bundle");
    nexusvote::VotingSession s(d, true);
    EXPECT_TRUE(s.loadConfig(demoPath()).ok);
    EXPECT_TRUE(s.open().ok);
    std::string tok = s.beginConfirm("alcaide");
    EXPECT_TRUE(s.confirm("alcaide", "11", false, tok).ok);
    EXPECT_TRUE(s.close("").ok);
    std::string out = d + "/out";
    EXPECT_TRUE(s.exportAll(out, "").ok);
    const char* names[] = {"lab-rdv.json",   "lab-boletim.json",     "lab-signature.json",
                           "lab-pubkey.hex", "election-config.json", "lab-zeresima.json",
                           "events.log",     "manifest.sha256"};
    for (const char* n : names) {
        std::ifstream f(out + "/" + n, std::ios::binary);
        EXPECT_TRUE(static_cast<bool>(f));
    }
    std::ifstream mf(out + "/manifest.sha256", std::ios::binary);
    std::stringstream ss;
    ss << mf.rdbuf();
    std::string man = ss.str();
    for (const char* n : names) {
        if (std::string(n) == "manifest.sha256")
            continue;
        EXPECT_TRUE(man.find(n) != std::string::npos);
    }
}

TEST(StrictTerminal, RefusesWithoutSealedOrRtc) {
    auto d = tmpdir("strict");
    nexusvote::VotingSession s(d, true);
    EXPECT_TRUE(s.loadConfig(demoPath()).ok);
    ::setenv("NEXUSVOTE_TERMINAL", "1", 1);
    auto r = s.open();
    EXPECT_FALSE(r.ok);
    ::unsetenv("NEXUSVOTE_TERMINAL");
    nexusvote::VotingSession s2(tmpdir("strict-lab"), true);
    EXPECT_TRUE(s2.loadConfig(demoPath()).ok);
    EXPECT_TRUE(s2.open().ok);
}

TEST(DoubleConfirm, TokenSurvivesResume) {
    auto d = tmpdir("tokresume");
    std::string tok;
    {
        nexusvote::VotingSession s(d, true);
        EXPECT_TRUE(s.loadConfig(demoPath()).ok);
        EXPECT_TRUE(s.open().ok);
        tok = s.beginConfirm("alcaide");
        EXPECT_TRUE(s.confirm("alcaide", "11", false, tok).ok);
    }
    nexusvote::VotingSession s2(d, true);
    EXPECT_TRUE(s2.loadConfig(demoPath()).ok);
    EXPECT_TRUE(s2.resume().ok);
    auto r = s2.confirm("alcaide", "22", false, tok);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(s2.recordCount(), 1u);
}

TEST(MultiSeat, DuplicateNominalAnnulled) {
    auto d = tmpdir("seats");
    nexusvote::VotingSession s(d, true);
    EXPECT_TRUE(s.loadConfig(demoPath()).ok);
    EXPECT_TRUE(s.open().ok);
    std::string t1 = s.beginConfirm("alcaide");
    EXPECT_TRUE(s.confirm("alcaide", "11", false, t1).ok);
    std::string t2 = s.beginConfirm("alcaide");
    auto r = s.confirm("alcaide", "11", false, t2);
    EXPECT_TRUE(r.ok);
    EXPECT_TRUE(r.info.find("anulado") != std::string::npos);
    EXPECT_EQ(s.recordCount(), 2u);
}

TEST(ElectionMeta, TurnAndElectorate) {
    nexusvote::LoadResult lr = nexusvote::loadConfigFromFile(demoPath());
    EXPECT_TRUE(lr.ok);
    EXPECT_EQ(lr.config.turn, 1);
    EXPECT_EQ(lr.config.electorate_size, 0ULL);
    auto bad = nexusvote::loadConfigFromText(
        "{\"schema_version\":1,\"election_id\":\"x\",\"display_name\":\"x\",\"turn\":3,"
        "\"offices\":[{\"office_id\":\"a\",\"title\":\"A\",\"number_length\":2}],"
        "\"candidates\":[{\"candidate_id\":\"c\",\"office_id\":\"a\",\"number\":\"11\","
        "\"name\":\"N\",\"party_number\":\"11\"}]}");
    EXPECT_FALSE(bad.ok);
}

TEST(Justificativa, CountedPersistedExported) {
    auto d = tmpdir("just");
    nexusvote::VotingSession s(d, true);
    EXPECT_TRUE(s.loadConfig(demoPath()).ok);
    EXPECT_TRUE(s.open().ok);
    EXPECT_TRUE(s.justifica("viagem").ok);
    EXPECT_TRUE(s.justifica("doenca").ok);
    EXPECT_EQ(s.justificativas(), 2ULL);
    EXPECT_TRUE(s.close("").ok);
    std::string out = d + "/out";
    EXPECT_TRUE(s.exportAll(out, "").ok);
    std::ifstream f(out + "/lab-boletim.json", std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    EXPECT_TRUE(ss.str().find("\"justificativas\":2") != std::string::npos);
    nexusvote::VotingSession s2(d, true);
    EXPECT_TRUE(s2.loadConfig(demoPath()).ok);
    EXPECT_TRUE(s2.resume().ok);
    EXPECT_EQ(s2.justificativas(), 2ULL);
}

TEST(MultiSeat, SeatsParsedAndMateRequired) {
    auto ok2 = nexusvote::loadConfigFromText(
        "{\"schema_version\":1,\"election_id\":\"x\",\"display_name\":\"x\","
        "\"offices\":[{\"office_id\":\"sen\",\"title\":\"Senador\",\"number_length\":3,\"seats\":2}"
        "],"
        "\"candidates\":[{\"candidate_id\":\"c1\",\"office_id\":\"sen\",\"number\":\"111\","
        "\"name\":\"N1\",\"party_number\":\"11\"},"
        "{\"candidate_id\":\"c2\",\"office_id\":\"sen\",\"number\":\"222\","
        "\"name\":\"N2\",\"party_number\":\"11\"}]}");
    EXPECT_TRUE(ok2.ok);
    EXPECT_EQ(ok2.config.offices[0].seats, 2);
    auto badseats = nexusvote::loadConfigFromText(
        "{\"schema_version\":1,\"election_id\":\"x\",\"display_name\":\"x\","
        "\"offices\":[{\"office_id\":\"sen\",\"title\":\"Senador\",\"number_length\":3,\"seats\":9}"
        "],"
        "\"candidates\":[{\"candidate_id\":\"c1\",\"office_id\":\"sen\",\"number\":\"111\","
        "\"name\":\"N1\",\"party_number\":\"11\"}]}");
    EXPECT_FALSE(badseats.ok);
    auto nomate = nexusvote::loadConfigFromText(
        "{\"schema_version\":1,\"election_id\":\"x\",\"display_name\":\"x\","
        "\"offices\":[{\"office_id\":\"pref\",\"title\":\"Prefeito\",\"number_length\":2,"
        "\"requires_mate\":true}],"
        "\"candidates\":[{\"candidate_id\":\"c1\",\"office_id\":\"pref\",\"number\":\"11\","
        "\"name\":\"N1\",\"party_number\":\"11\"}]}");
    EXPECT_FALSE(nomate.ok);
}
