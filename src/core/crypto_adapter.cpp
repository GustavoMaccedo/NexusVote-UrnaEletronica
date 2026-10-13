// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "crypto_adapter.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <cstdio>
#include <sstream>

namespace nexusvote::crypto {

std::string toHex(const std::uint8_t* p, std::size_t n) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(n * 2);
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(digits[p[i] >> 4]);
        out.push_back(digits[p[i] & 0xF]);
    }
    return out;
}

bool fromHex(const std::string& h, std::vector<std::uint8_t>& out) {
    if (h.size() % 2 != 0)
        return false;
    out.clear();
    out.reserve(h.size() / 2);
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < h.size(); i += 2) {
        int hi = val(h[i]);
        int lo = val(h[i + 1]);
        if (hi < 0 || lo < 0)
            return false;
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return true;
}

std::string sha256Hex(const std::string& data) {
    std::array<std::uint8_t, EVP_MAX_MD_SIZE> d{};
    unsigned int len = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    bool ok = ctx != nullptr && EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1 &&
              EVP_DigestUpdate(ctx, data.data(), data.size()) == 1 &&
              EVP_DigestFinal_ex(ctx, d.data(), &len) == 1;
    EVP_MD_CTX_free(ctx);
    if (!ok)
        return "";
    return toHex(d.data(), len);
}

std::string sha256FileHex(const std::string& path, std::string& err) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        err = "nao foi possivel abrir: " + path;
        return "";
    }
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    bool ok = ctx != nullptr && EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1;
    std::array<unsigned char, 65536> buf{};
    std::size_t n = 0;
    while (ok && (n = std::fread(buf.data(), 1, buf.size(), f)) > 0) {
        ok = EVP_DigestUpdate(ctx, buf.data(), n) == 1;
    }
    if (std::ferror(f))
        ok = false;
    std::fclose(f);
    std::array<std::uint8_t, EVP_MAX_MD_SIZE> d{};
    unsigned int len = 0;
    ok = ok && EVP_DigestFinal_ex(ctx, d.data(), &len) == 1;
    EVP_MD_CTX_free(ctx);
    if (!ok) {
        err = "erro ao calcular hash: " + path;
        return "";
    }
    return toHex(d.data(), len);
}

bool randomBytes(std::uint8_t* out, std::size_t n) {
    return RAND_bytes(out, static_cast<int>(n)) == 1;
}

std::string randomHex(std::size_t nbytes, bool& ok) {
    std::vector<std::uint8_t> b(nbytes);
    ok = randomBytes(b.data(), b.size());
    if (!ok)
        return "";
    return toHex(b.data(), b.size());
}

bool edGenerate(EdKeypair& kp, std::string& err) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
    if (ctx == nullptr) {
        err = "EVP_PKEY_CTX_new_id falhou";
        return false;
    }
    EVP_PKEY* key = nullptr;
    bool ok = false;
    if (EVP_PKEY_keygen_init(ctx) == 1 && EVP_PKEY_keygen(ctx, &key) == 1) {
        std::size_t publen = 32;
        std::size_t privlen = 32;
        kp.pub.resize(32);
        kp.priv.resize(32);
        if (EVP_PKEY_get_raw_public_key(key, kp.pub.data(), &publen) == 1 &&
            EVP_PKEY_get_raw_private_key(key, kp.priv.data(), &privlen) == 1) {
            kp.pub.resize(publen);
            kp.priv.resize(privlen);
            ok = true;
        } else {
            err = "extracao de chave bruta falhou";
        }
    } else {
        err = "geracao Ed25519 falhou";
    }
    EVP_PKEY_free(key);
    EVP_PKEY_CTX_free(ctx);
    return ok;
}

bool edSign(const EdKeypair& kp, const std::string& msg, std::vector<std::uint8_t>& sig,
            std::string& err) {
    EVP_PKEY* key =
        EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, kp.priv.data(), kp.priv.size());
    if (key == nullptr) {
        err = "chave privada invalida";
        return false;
    }
    EVP_MD_CTX* md = EVP_MD_CTX_new();
    bool ok = false;
    std::size_t slen = 0;
    if (md != nullptr && EVP_DigestSignInit(md, nullptr, nullptr, nullptr, key) == 1 &&
        EVP_DigestSign(md, nullptr, &slen, reinterpret_cast<const unsigned char*>(msg.data()),
                       msg.size()) == 1) {
        sig.resize(slen);
        if (EVP_DigestSign(md, sig.data(), &slen,
                           reinterpret_cast<const unsigned char*>(msg.data()), msg.size()) == 1) {
            sig.resize(slen);
            ok = true;
        } else {
            err = "assinatura falhou";
        }
    } else {
        err = "DigestSignInit falhou";
    }
    EVP_MD_CTX_free(md);
    EVP_PKEY_free(key);
    return ok;
}

bool edVerify(const std::uint8_t* pub32, const std::string& msg, const std::uint8_t* sig64,
              std::string& err) {
    EVP_PKEY* key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, pub32, 32);
    if (key == nullptr) {
        err = "chave publica invalida";
        return false;
    }
    EVP_MD_CTX* md = EVP_MD_CTX_new();
    bool ok = false;
    if (md != nullptr && EVP_DigestVerifyInit(md, nullptr, nullptr, nullptr, key) == 1 &&
        EVP_DigestVerify(md, sig64, 64, reinterpret_cast<const unsigned char*>(msg.data()),
                         msg.size()) == 1) {
        ok = true;
    } else {
        err = "assinatura invalida";
    }
    EVP_MD_CTX_free(md);
    EVP_PKEY_free(key);
    return ok;
}

} // namespace nexusvote::crypto
