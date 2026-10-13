// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include <set>

#include "../src/core/crypto_adapter.hpp"
#include "test_framework.hpp"

TEST(Crypto, Sha256KnownVector) {
    EXPECT_EQ(nexusvote::crypto::sha256Hex("abc"),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Crypto, RandomDiffers) {
    bool ok1 = false;
    bool ok2 = false;
    std::string a = nexusvote::crypto::randomHex(16, ok1);
    std::string b = nexusvote::crypto::randomHex(16, ok2);
    EXPECT_TRUE(ok1 && ok2);
    EXPECT_EQ(a.size(), 32u);
    EXPECT_TRUE(a != b);
}

TEST(Crypto, EdDemoSignVerify) {
    nexusvote::crypto::EdKeypair kp;
    std::string err;
    EXPECT_TRUE(nexusvote::crypto::edGenerate(kp, err));
    std::vector<std::uint8_t> sig;
    EXPECT_TRUE(nexusvote::crypto::edSign(kp, "boletim-demo", sig, err));
    EXPECT_EQ(sig.size(), 64u);
    EXPECT_TRUE(nexusvote::crypto::edVerify(kp.pub.data(), "boletim-demo", sig.data(), err));
}

TEST(Crypto, EdDemoWrongKeyFails) {
    nexusvote::crypto::EdKeypair a;
    nexusvote::crypto::EdKeypair b;
    std::string err;
    EXPECT_TRUE(nexusvote::crypto::edGenerate(a, err));
    EXPECT_TRUE(nexusvote::crypto::edGenerate(b, err));
    std::vector<std::uint8_t> sig;
    EXPECT_TRUE(nexusvote::crypto::edSign(a, "msg", sig, err));
    EXPECT_FALSE(nexusvote::crypto::edVerify(b.pub.data(), "msg", sig.data(), err));
}

TEST(Crypto, EdDemoTamperedFails) {
    nexusvote::crypto::EdKeypair kp;
    std::string err;
    EXPECT_TRUE(nexusvote::crypto::edGenerate(kp, err));
    std::vector<std::uint8_t> sig;
    EXPECT_TRUE(nexusvote::crypto::edSign(kp, "original", sig, err));
    EXPECT_FALSE(nexusvote::crypto::edVerify(kp.pub.data(), "adulterado", sig.data(), err));
}
