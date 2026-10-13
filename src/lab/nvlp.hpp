// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

// NVLP v1 — enquadramento do protocolo de laboratório (espelho de LabProtocol.kt).

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace nexusvote::nvlp {

inline constexpr std::uint8_t kVersion = 1;
inline constexpr std::size_t kMaxBody = 65535;
inline constexpr std::size_t kMaxFrame = 4 + 1 + 1 + 1 + 255 + 2 + kMaxBody + 4;

inline constexpr std::uint8_t HELLO = 1;
inline constexpr std::uint8_t HELLO_ACK = 2;
inline constexpr std::uint8_t LOAD_TEST_FIXTURE = 3;
inline constexpr std::uint8_t ACK = 4;
inline constexpr std::uint8_t NAK = 5;
inline constexpr std::uint8_t RUN_TEST_SUITE = 6;
inline constexpr std::uint8_t GET_STATUS = 7;
inline constexpr std::uint8_t STATUS = 8;
inline constexpr std::uint8_t FETCH_RESULT = 9;
inline constexpr std::uint8_t RESULT = 10;
inline constexpr std::uint8_t CANCEL_TEST = 11;

struct Frame {
    std::uint8_t type = 0;
    std::string run_id;
    std::string body;
};

struct EncodeResult {
    std::vector<std::uint8_t> bytes;
    bool ok = false;
    std::string error;
};

enum class DecodeError { Ok, TooShort, BadMagic, BadVersion, Truncated, Oversize, BadCrc };

struct DecodeResult {
    Frame frame;
    bool ok = false;
    DecodeError error = DecodeError::Ok;
    std::string detail;
};

EncodeResult encode(std::uint8_t type, const std::string& run_id, const std::string& body);
DecodeResult decode(const std::uint8_t* data, std::size_t n);
std::uint32_t crc32(const std::uint8_t* data, std::size_t n);

class NonceGuard {
  public:
    NonceGuard() = default;
    explicit NonceGuard(const std::string& file);
    bool accept(const std::string& nonce);
    std::size_t size() const {
        return seen_.size();
    }

  private:
    static constexpr std::size_t kMaxNonces = 100000;
    bool appendFile(const std::string& nonce);
    std::set<std::string> seen_;
    std::string file_;
};

} // namespace nexusvote::nvlp
