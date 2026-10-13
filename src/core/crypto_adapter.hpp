// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace nexusvote::crypto {

std::string sha256Hex(const std::string& data);
std::string sha256FileHex(const std::string& path, std::string& err);
bool randomBytes(std::uint8_t* out, std::size_t n);
std::string randomHex(std::size_t nbytes, bool& ok);

struct EdKeypair {
    std::vector<std::uint8_t> pub;
    std::vector<std::uint8_t> priv;
};

bool edGenerate(EdKeypair& kp, std::string& err);
bool edSign(const EdKeypair& kp, const std::string& msg, std::vector<std::uint8_t>& sig,
            std::string& err);
bool edVerify(const std::uint8_t* pub32, const std::string& msg, const std::uint8_t* sig64,
              std::string& err);
std::string toHex(const std::uint8_t* p, std::size_t n);
bool fromHex(const std::string& h, std::vector<std::uint8_t>& out);

} // namespace nexusvote::crypto
