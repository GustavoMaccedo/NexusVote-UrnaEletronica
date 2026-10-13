// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "secure/secure_session.hpp"

#include <array>
#include <cstdio>

namespace nexusvote::secure {

namespace {
std::string toHexLocal(const uint8_t* p, size_t n) {
    static const char* H = "0123456789abcdef";
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        s.push_back(H[p[i] >> 4]);
        s.push_back(H[p[i] & 0xF]);
    }
    return s;
}
} // namespace

#ifdef NEXUSVOTE_WITH_RUST_SECURE
#include "secure/nv_secure_ffi.h"

bool isEnabled() {
    return true;
}

bool initSealedSession(std::string& err) {
    int rc = nv_init_sealed_session();
    if (rc != 0) {
        err = "nv_init_sealed_session rc=" + std::to_string(rc);
        return false;
    }
    return true;
}

SealResult sealVote(const std::string& dataJson) {
    SealResult r;
    std::string pub = sessionPubkey();
    if (pub.empty()) {
        r.code = kUnsupported;
        return r;
    }
    std::vector<uint8_t> out(128 * 1024, 0);
    int n = nv_seal_vote(reinterpret_cast<const uint8_t*>(dataJson.data()), dataJson.size(),
                         out.data(), out.size());
    if (n < 0) {
        r.code = n;
        return r;
    }
    r.code = kOk;
    r.jsonl.assign(reinterpret_cast<const char*>(out.data()), static_cast<size_t>(n));
    r.pubkey = pub;
    return r;
}

std::string sessionPubkey() {
    std::array<uint8_t, 65> buf{};
    int n = nv_pubkey(buf.data(), buf.size());
    if (n != 64) {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(buf.data()), 64);
}

bool sha256Hex(const std::string& data, std::string& outHex, std::string& err) {
    std::array<uint8_t, 32> h{};
    int rc =
        nv_sha256(reinterpret_cast<const uint8_t*>(data.data()), data.size(), h.data(), h.size());
    if (rc != 0) {
        err = "nv_sha256 rc=" + std::to_string(rc);
        return false;
    }
    outHex = toHexLocal(h.data(), h.size());
    return true;
}

bool verifyEd25519(const std::vector<std::uint8_t>& pub32, const std::string& msg,
                   const std::vector<std::uint8_t>& sig64, std::string& err) {
    if (pub32.size() != 32 || sig64.size() != 64) {
        err = "tamanho inválido";
        return false;
    }
    int rc = nv_verify(pub32.data(), sig64.data(), reinterpret_cast<const uint8_t*>(msg.data()),
                       msg.size());
    if (rc != 0) {
        err = "assinatura inválida rc=" + std::to_string(rc);
        return false;
    }
    return true;
}

bool hashPin(const std::string& pin, std::string& outPhc, std::string& err) {
    std::array<uint8_t, 256> buf{};
    int n = nv_hash_pin(reinterpret_cast<const uint8_t*>(pin.data()), pin.size(), buf.data(),
                        buf.size());
    if (n <= 0) {
        err = "nv_hash_pin rc=" + std::to_string(n);
        return false;
    }
    outPhc.assign(reinterpret_cast<const char*>(buf.data()), static_cast<size_t>(n));
    return true;
}

bool verifyPin(const std::string& pin, const std::string& phc, std::string& err) {
    int rc = nv_verify_pin(reinterpret_cast<const uint8_t*>(pin.data()), pin.size(),
                           reinterpret_cast<const uint8_t*>(phc.data()), phc.size());
    if (rc != 0) {
        err = "PIN incorreto";
        return false;
    }
    return true;
}

#else

bool isEnabled() {
    return false;
}
bool initSealedSession(std::string& err) {
    err = "canal Rust desativado (WITH_RUST_SECURE=OFF)";
    return false;
}
SealResult sealVote(const std::string&) {
    return SealResult{};
}
std::string sessionPubkey() {
    return {};
}
bool sha256Hex(const std::string&, std::string&, std::string& err) {
    err = "canal Rust desativado";
    return false;
}
bool verifyEd25519(const std::vector<std::uint8_t>&, const std::string&,
                   const std::vector<std::uint8_t>&, std::string& err) {
    err = "canal Rust desativado";
    return false;
}
bool hashPin(const std::string&, std::string&, std::string& err) {
    err = "canal Rust desativado";
    return false;
}
bool verifyPin(const std::string&, const std::string&, std::string& err) {
    err = "canal Rust desativado";
    return false;
}

#endif

} // namespace nexusvote::secure
