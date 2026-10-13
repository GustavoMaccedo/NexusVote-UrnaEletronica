// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "nvlp.hpp"

#include <cstdio>
#include <unistd.h>

namespace nexusvote::nvlp {

namespace {

std::uint32_t crc_table[256];
bool crc_ready = false;

void crcInit() {
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc_table[i] = c;
    }
    crc_ready = true;
}

} // namespace

std::uint32_t crc32(const std::uint8_t* data, std::size_t n) {
    if (!crc_ready)
        crcInit();
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < n; ++i)
        c = crc_table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

EncodeResult encode(std::uint8_t type, const std::string& run_id, const std::string& body) {
    EncodeResult r;
    if (run_id.size() > 255) {
        r.error = "runId longo";
        return r;
    }
    if (body.size() > kMaxBody) {
        r.error = "corpo > 64 KiB";
        return r;
    }
    std::vector<std::uint8_t> p;
    p.reserve(1 + 1 + 1 + run_id.size() + 2 + body.size());
    p.push_back(kVersion);
    p.push_back(type);
    p.push_back(static_cast<std::uint8_t>(run_id.size()));
    p.insert(p.end(), run_id.begin(), run_id.end());
    p.push_back(static_cast<std::uint8_t>((body.size() >> 8) & 0xFF));
    p.push_back(static_cast<std::uint8_t>(body.size() & 0xFF));
    p.insert(p.end(), body.begin(), body.end());
    std::uint32_t c = crc32(p.data(), p.size());
    r.bytes = {'N', 'V', 'L', 'P'};
    r.bytes.insert(r.bytes.end(), p.begin(), p.end());
    r.bytes.push_back(static_cast<std::uint8_t>((c >> 24) & 0xFF));
    r.bytes.push_back(static_cast<std::uint8_t>((c >> 16) & 0xFF));
    r.bytes.push_back(static_cast<std::uint8_t>((c >> 8) & 0xFF));
    r.bytes.push_back(static_cast<std::uint8_t>(c & 0xFF));
    r.ok = true;
    return r;
}

DecodeResult decode(const std::uint8_t* data, std::size_t n) {
    DecodeResult r;
    auto fail = [&](DecodeError e, const std::string& d) {
        r.error = e;
        r.detail = d;
        return r;
    };
    if (n < 4 + 1 + 1 + 1 + 2 + 4)
        return fail(DecodeError::TooShort, "frame menor que o minimo");
    if (!(data[0] == 'N' && data[1] == 'V' && data[2] == 'L' && data[3] == 'P'))
        return fail(DecodeError::BadMagic, "magic invalido");
    std::size_t pos = 4;
    if (data[pos++] != kVersion)
        return fail(DecodeError::BadVersion, "versao inesperada");
    std::uint8_t type = data[pos++];
    std::size_t run_len = data[pos++];
    if (n < 4 + 1 + 1 + 1 + run_len + 2 + 4)
        return fail(DecodeError::Truncated, "cabecalho incompleto");
    std::string run_id(reinterpret_cast<const char*>(data + pos), run_len);
    pos += run_len;
    std::size_t body_len =
        (static_cast<std::size_t>(data[pos]) << 8) | static_cast<std::size_t>(data[pos + 1]);
    pos += 2;
    if (body_len > kMaxBody)
        return fail(DecodeError::Oversize, "corpo excede 64 KiB");
    std::size_t need = 4 + 1 + 1 + 1 + run_len + 2 + body_len + 4;
    if (n < need)
        return fail(DecodeError::Truncated, "frame incompleto");
    std::uint32_t got = (static_cast<std::uint32_t>(data[need - 4]) << 24) |
                        (static_cast<std::uint32_t>(data[need - 3]) << 16) |
                        (static_cast<std::uint32_t>(data[need - 2]) << 8) |
                        static_cast<std::uint32_t>(data[need - 1]);
    if (crc32(data + 4, need - 8) != got)
        return fail(DecodeError::BadCrc, "crc divergente");
    r.frame.type = type;
    r.frame.run_id = run_id;
    r.frame.body = std::string(reinterpret_cast<const char*>(data + pos), body_len);
    r.ok = true;
    return r;
}

NonceGuard::NonceGuard(const std::string& file) : file_(file) {
    if (file_.empty()) {
        return;
    }
    FILE* f = std::fopen(file_.c_str(), "r");
    if (f == nullptr) {
        return;
    }
    char buf[512];
    while (std::fgets(buf, sizeof(buf), f) != nullptr && seen_.size() < kMaxNonces) {
        std::string line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        if (!line.empty() && line.size() <= 512) {
            seen_.insert(line);
        }
    }
    std::fclose(f);
}

bool NonceGuard::appendFile(const std::string& nonce) {
    FILE* f = std::fopen(file_.c_str(), "a");
    if (f == nullptr) {
        return false;
    }
    const bool ok = std::fwrite(nonce.data(), 1, nonce.size(), f) == nonce.size() &&
                    std::fputc('\n', f) == '\n';
    std::fflush(f);
    const int fd = fileno(f);
    if (fd >= 0) {
        fsync(fd);
    }
    std::fclose(f);
    return ok;
}

bool NonceGuard::accept(const std::string& nonce) {
    if (nonce.empty() || nonce.size() > 512) {
        return false;
    }
    if (!seen_.insert(nonce).second) {
        return false;
    }
    if (seen_.size() > kMaxNonces) {
        seen_.erase(nonce);
        return false;
    }
    if (!file_.empty() && !appendFile(nonce)) {
        seen_.erase(nonce);
        return false;
    }
    return true;
}

} // namespace nexusvote::nvlp
