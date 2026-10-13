// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nexusvote::secure {

inline constexpr int kOk = 0;
inline constexpr int kUnsupported = -100;

struct SealResult {
    int code = kUnsupported;
    std::string jsonl;
    std::string pubkey;
};

bool initSealedSession(std::string& err);
SealResult sealVote(const std::string& dataJson);
std::string sessionPubkey();
bool sha256Hex(const std::string& data, std::string& outHex, std::string& err);
bool verifyEd25519(const std::vector<std::uint8_t>& pub32, const std::string& msg,
                   const std::vector<std::uint8_t>& sig64, std::string& err);
bool hashPin(const std::string& pin, std::string& outPhc, std::string& err);
bool verifyPin(const std::string& pin, const std::string& phc, std::string& err);

bool isEnabled();

} // namespace nexusvote::secure
