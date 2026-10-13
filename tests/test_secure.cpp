// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "secure/secure_session.hpp"
#include "test_framework.hpp"

#include <string>

using nexusvote::secure::SealResult;

TEST(SecureChannel, DisabledOrEnabledIsConsistent) {
    bool en = nexusvote::secure::isEnabled();
#ifdef NEXUSVOTE_WITH_RUST_SECURE
    EXPECT_TRUE(en);
#else
    EXPECT_TRUE(!en);
#endif
}

#ifdef NEXUSVOTE_WITH_RUST_SECURE
TEST(SecureChannel, InitPubkeySealVerify) {
    std::string err;
    EXPECT_TRUE(nexusvote::secure::initSealedSession(err));
    std::string pub = nexusvote::secure::sessionPubkey();
    EXPECT_TRUE(pub.size() == 64);

    SealResult r =
        nexusvote::secure::sealVote(R"({"office":"vereador","kind":"nominal","ref":"12345"})");
    EXPECT_TRUE(r.code == 0);
    EXPECT_TRUE(!r.jsonl.empty());
    EXPECT_TRUE(r.jsonl.find("\"seq\":1") != std::string::npos);
    EXPECT_TRUE(r.jsonl.find("\"prev\"") != std::string::npos);

    SealResult r2 = nexusvote::secure::sealVote(R"({"office":"prefeito","kind":"blank"})");
    EXPECT_TRUE(r2.code == 0);
    EXPECT_TRUE(r2.jsonl.find("\"seq\":2") != std::string::npos);

    std::string outHex;
    EXPECT_TRUE(nexusvote::secure::sha256Hex("abc", outHex, err));
    EXPECT_EQ(outHex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}
#else
TEST(SecureChannel, StubsReturnUnsupported) {
    std::string err;
    EXPECT_TRUE(!nexusvote::secure::initSealedSession(err));
    SealResult r = nexusvote::secure::sealVote("{}");
    EXPECT_TRUE(r.code == nexusvote::secure::kUnsupported);
}
#endif
