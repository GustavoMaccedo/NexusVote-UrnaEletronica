// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "paper_escpos.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>

namespace nexusvote::paper {
namespace {

std::string latin1(const std::string& utf8) {
    std::string out;
    out.reserve(utf8.size());
    for (size_t i = 0; i < utf8.size();) {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
            ++i;
        } else if (c == 0xE2 && i + 2 < utf8.size()) {
            unsigned char c2 = static_cast<unsigned char>(utf8[i + 1]);
            unsigned char c3 = static_cast<unsigned char>(utf8[i + 2]);
            if (c2 == 0x80 && (c3 == 0x94 || c3 == 0x93)) {
                out.push_back('-');
            } else if (c2 == 0x80 && (c3 == 0x9C || c3 == 0x9D)) {
                out.push_back('"');
            } else if (c2 == 0x80 && (c3 == 0x98 || c3 == 0x99)) {
                out.push_back('\'');
            } else {
                out.push_back('?');
            }
            i += 3;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < utf8.size()) {
            unsigned char c2 = static_cast<unsigned char>(utf8[i + 1]);
            unsigned b =
                ((static_cast<unsigned>(c) & 0x1F) << 6) | (static_cast<unsigned>(c2) & 0x3F);
            out.push_back(static_cast<char>(b <= 0xFF ? b : '?'));
            i += 2;
        } else {
            out.push_back('?');
            ++i;
        }
    }
    return out;
}

std::string fit(const std::string& s) {
    std::string b = latin1(s);
    if ((int)b.size() > kPaperCols)
        b.resize(kPaperCols);
    return b;
}

std::string center(const std::string& s) {
    std::string t = s;
    size_t a = t.find_first_not_of(" \t");
    size_t z = t.find_last_not_of(" \t");
    std::string inner = (a == std::string::npos) ? "" : t.substr(a, z - a + 1);
    std::string b = fit(inner);
    if ((int)b.size() >= kPaperCols)
        return b;
    return std::string((kPaperCols - b.size()) / 2, ' ') + b;
}

} // namespace

std::string printerPath() {
    if (const char* e = std::getenv("NV_PRINTER_DEV")) {
        if (e[0] != '\0')
            return e;
    }
    return "/dev/usb/lp0";
}

std::string buildJob(const std::string& title, const std::string& sessao,
                     const std::vector<std::string>& linhas, const std::string& qr_payload) {
    std::string j;
    auto raw = [&j](std::initializer_list<unsigned char> bs) {
        for (unsigned char b : bs)
            j.push_back(static_cast<char>(b));
    };
    raw({0x1B, 0x40});
    raw({0x1B, 0x74, 0x13});
    raw({0x1B, 0x61, 0x01});
    raw({0x1B, 0x45, 0x01});
    j += center("PROTOTIPO INDEPENDENTE") + "\x0A";
    j += center("USO EM LABORATORIO") + "\x0A";
    j += center("NAO OFICIAL - SEM VALOR") + "\x0A";
    raw({0x1B, 0x45, 0x00});
    raw({0x1B, 0x61, 0x01});
    j += center(title) + "\x0A";
    raw({0x1B, 0x61, 0x00});
    j += fit("SESSAO: " + sessao) + "\x0A";
    j += fit("SISTEMA: NEXUSVOTE-LAB") + "\x0A";
    for (const auto& l : linhas)
        j += fit(l) + "\x0A";
    raw({0x1B, 0x61, 0x01});
    raw({0x1D, 0x28, 0x6B, 0x04, 0x00, 0x31, 0x41, 0x32, 0x00});
    raw({0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x43, 0x05});
    raw({0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x45, 0x31});
    size_t n = qr_payload.size() + 3;
    raw({0x1D, 0x28, 0x6B, (unsigned char)(n % 256), (unsigned char)(n / 256), 0x31, 0x50, 0x30});
    j += qr_payload;
    raw({0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x51, 0x30});
    raw({0x0A, 0x0A, 0x0A, 0x1D, 0x56, 0x42, 0x00});
    return j;
}

PrintResult printRaw(const std::string& dev, const std::string& job) {
    PrintResult r;
    FILE* probe = std::fopen(dev.c_str(), "rb");
    if (probe == nullptr) {
        r.outcome = PrintOutcome::Ausente;
        r.detail = dev + " inexistente (cabo/usblp)";
        return r;
    }
    std::fclose(probe);
    int fd = ::open(dev.c_str(), O_WRONLY);
    if (fd < 0) {
        r.outcome = PrintOutcome::Ausente;
        r.detail = "open " + dev + " (grupo lp?)";
        return r;
    }
    size_t off = 0;
    while (off < job.size()) {
        ssize_t w = ::write(fd, job.data() + off, job.size() - off);
        if (w <= 0) {
            ::close(fd);
            r.outcome = PrintOutcome::Falha;
            r.detail = "write @" + std::to_string(off) + " (jam/queda?)";
            return r;
        }
        off += static_cast<size_t>(w);
    }
    if (::fsync(fd) != 0) {
        ::close(fd);
        r.outcome = PrintOutcome::Falha;
        r.detail = "fsync (dreno URB falhou)";
        return r;
    }
    ::close(fd);
    r.outcome = PrintOutcome::Ok;
    r.detail = "via impressa";
    return r;
}

} // namespace nexusvote::paper
