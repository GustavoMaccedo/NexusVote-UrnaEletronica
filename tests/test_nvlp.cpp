// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
// Testes NVLP v1 + respondedor de laboratório (sem socket; socket em run_lab_socket.sh).

#include <filesystem>
#include <fstream>
#include <sstream>

#include "../src/lab/lab_daemon.hpp"
#include "../src/lab/nvlp.hpp"
#include "test_framework.hpp"

namespace {

std::string demoConfigText() {
    std::ifstream f(CONFIG_DIR "/demo-election.json", std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string tmpdir(const std::string& name) {
    std::string p = std::string(TMP_BASE) + "/" + name;
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
    std::filesystem::create_directories(p, ec);
    return p;
}

} // namespace

TEST(Nvlp, RoundTrip) {
    auto e = nexusvote::nvlp::encode(nexusvote::nvlp::HELLO, "RUN-42", "{\"nonce\":\"abc\"}");
    EXPECT_TRUE(e.ok);
    auto d = nexusvote::nvlp::decode(e.bytes.data(), e.bytes.size());
    EXPECT_TRUE(d.ok);
    EXPECT_EQ(d.frame.type, nexusvote::nvlp::HELLO);
    EXPECT_EQ(d.frame.run_id, "RUN-42");
    EXPECT_EQ(d.frame.body, "{\"nonce\":\"abc\"}");
}

TEST(Nvlp, TamperFailsCrc) {
    auto e = nexusvote::nvlp::encode(nexusvote::nvlp::ACK, "R", "{}");
    EXPECT_TRUE(e.ok);
    e.bytes[e.bytes.size() - 5] ^= 0x01;
    auto d = nexusvote::nvlp::decode(e.bytes.data(), e.bytes.size());
    EXPECT_FALSE(d.ok);
    EXPECT_TRUE(d.error == nexusvote::nvlp::DecodeError::BadCrc);
}

TEST(Nvlp, TruncatedAndMagic) {
    auto e = nexusvote::nvlp::encode(nexusvote::nvlp::ACK, "RUN-LONGO-123", "{}");
    EXPECT_TRUE(e.ok);
    auto d = nexusvote::nvlp::decode(e.bytes.data(), 10);
    EXPECT_FALSE(d.ok);
    e.bytes[0] = 0x00;
    auto d2 = nexusvote::nvlp::decode(e.bytes.data(), e.bytes.size());
    EXPECT_FALSE(d2.ok);
    EXPECT_TRUE(d2.error == nexusvote::nvlp::DecodeError::BadMagic);
}

TEST(Nvlp, OversizeRejected) {
    std::string big(nexusvote::nvlp::kMaxBody + 1, 'x');
    EXPECT_FALSE(nexusvote::nvlp::encode(nexusvote::nvlp::RESULT, "R", big).ok);
}

TEST(Nvlp, NonceGuardRejectsReplay) {
    nexusvote::nvlp::NonceGuard g;
    EXPECT_TRUE(g.accept("n1"));
    EXPECT_FALSE(g.accept("n1"));
    EXPECT_TRUE(g.accept("n2"));
}

TEST(LabDaemon, NonLabBuildNaksEverything) {
    auto d = tmpdir("labgate");
    nexusvote::lab::LabDaemon daemon(d, false);
    nexusvote::nvlp::Frame in{nexusvote::nvlp::HELLO, "R", "{\"nonce\":\"x\"}"};
    auto out = daemon.handleFrame(in);
    EXPECT_TRUE(out.type == nexusvote::nvlp::NAK);
}

TEST(LabDaemon, HelloRequiresFreshNonce) {
    auto d = tmpdir("labhello");
    nexusvote::lab::LabDaemon daemon(d, true);
    nexusvote::nvlp::Frame in{nexusvote::nvlp::HELLO, "R", "{\"nonce\":\"abc\"}"};
    auto a1 = daemon.handleFrame(in);
    EXPECT_TRUE(a1.type == nexusvote::nvlp::HELLO_ACK);
    auto a2 = daemon.handleFrame(in);
    EXPECT_TRUE(a2.type == nexusvote::nvlp::NAK);
}

TEST(LabDaemon, LoadRunResultFlow) {
    auto d = tmpdir("labflow");
    nexusvote::lab::LabDaemon daemon(d, true);
    std::string cfg = demoConfigText();
    nexusvote::nvlp::Frame load{nexusvote::nvlp::LOAD_TEST_FIXTURE, "RUN-1", cfg};
    auto la = daemon.handleFrame(load);
    EXPECT_TRUE(la.type == nexusvote::nvlp::ACK);
    auto la2 = daemon.handleFrame(load);
    EXPECT_TRUE(la2.type == nexusvote::nvlp::ACK);
    EXPECT_EQ(la2.body, la.body);
    nexusvote::nvlp::Frame load3{nexusvote::nvlp::LOAD_TEST_FIXTURE, "RUN-1", "{\"other\":1}"};
    EXPECT_TRUE(daemon.handleFrame(load3).type == nexusvote::nvlp::NAK);
    std::string run_body =
        "{\"suite_id\":\"smoke\",\"seed\":\"42\",\"votes\":[\"alcaide 11\",\"vereador 11111\"]}";
    nexusvote::nvlp::Frame run{nexusvote::nvlp::RUN_TEST_SUITE, "RUN-1", run_body};
    auto ra = daemon.handleFrame(run);
    EXPECT_TRUE(ra.type == nexusvote::nvlp::ACK);
    auto ra2 = daemon.handleFrame(run);
    EXPECT_TRUE(ra2.type == nexusvote::nvlp::ACK);
    EXPECT_EQ(ra2.body, ra.body);
    nexusvote::nvlp::Frame st{nexusvote::nvlp::GET_STATUS, "RUN-1", "{}"};
    auto sa = daemon.handleFrame(st);
    EXPECT_TRUE(sa.type == nexusvote::nvlp::STATUS);
    EXPECT_TRUE(sa.body.find("true") != std::string::npos);
    nexusvote::nvlp::Frame fr{nexusvote::nvlp::FETCH_RESULT, "RUN-1", "{}"};
    auto fa = daemon.handleFrame(fr);
    EXPECT_TRUE(fa.type == nexusvote::nvlp::RESULT);
    EXPECT_TRUE(fa.body.find("TOTAL") != std::string::npos ||
                fa.body.find("total") != std::string::npos);
    nexusvote::nvlp::Frame cc{nexusvote::nvlp::CANCEL_TEST, "RUN-1", "{}"};
    EXPECT_TRUE(daemon.handleFrame(cc).type == nexusvote::nvlp::ACK);
    EXPECT_TRUE(daemon.handleFrame(fr).type == nexusvote::nvlp::NAK);
}

TEST(NonceFile, PersistsAcrossInstances) {
    std::string dir = std::string(TMP_BASE) + "/noncefile";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    std::string path = dir + "/nonces.txt";
    {
        nexusvote::nvlp::NonceGuard g(path);
        EXPECT_TRUE(g.accept("n1-livre"));
        EXPECT_TRUE(!g.accept("n1-livre"));
    }
    {
        nexusvote::nvlp::NonceGuard g2(path);
        EXPECT_TRUE(!g2.accept("n1-livre"));
        EXPECT_TRUE(g2.accept("n2-livre"));
    }
    std::filesystem::remove_all(dir, ec);
}
