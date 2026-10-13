// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "lab_daemon.hpp"

#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "../core/config_loader.hpp"
#include "../core/crypto_adapter.hpp"
#include "../core/domain.hpp"
#include "../core/minijson.hpp"
#include "../core/policy.hpp"
#include "../core/session.hpp"
#include "peercred.hpp"

namespace nexusvote::lab {

namespace {

constexpr int kIoTimeoutMs = 5000;

bool readExact(int fd, std::uint8_t* out, std::size_t n) {
    std::size_t got = 0;
    while (got < n) {
        pollfd p{};
        p.fd = fd;
        p.events = POLLIN;
        int pr = poll(&p, 1, kIoTimeoutMs);
        if (pr <= 0)
            return false;
        ssize_t r = ::read(fd, out + got, n - got);
        if (r <= 0)
            return false;
        got += static_cast<std::size_t>(r);
    }
    return true;
}

bool writeAll(int fd, const std::uint8_t* p, std::size_t n) {
    std::size_t done = 0;
    while (done < n) {
        ssize_t w = ::write(fd, p + done, n - done);
        if (w <= 0)
            return false;
        done += static_cast<std::size_t>(w);
    }
    return true;
}

std::string jsonEscape(const std::string& s) {
    return minijson::escape(s);
}

bool getStr(const minijson::Value& obj, const char* key, std::string& out) {
    auto it = obj.obj.find(key);
    if (it == obj.obj.end())
        return false;
    if (it->second->type == minijson::Value::Type::Str) {
        out = it->second->str;
        return true;
    }
    return false;
}

} // namespace

LabDaemon::LabDaemon(const std::string& workdir, bool lab_test_mode)
    : workdir_(workdir), lab_test_mode_(lab_test_mode), nonces_(workdir + "/nonces.txt") {}

nvlp::Frame LabDaemon::nak(const std::string& run_id, const std::string& reason) {
    nvlp::Frame f;
    f.type = nvlp::NAK;
    f.run_id = run_id;
    f.body = "{\"reason\":\"" + jsonEscape(reason) + "\"}";
    return f;
}

nvlp::Frame LabDaemon::runSuite(const std::string& run_id, const std::string& body) {
    auto pr = minijson::parse(body);
    if (!pr.ok || pr.root->type != minijson::Value::Type::Obj)
        return nak(run_id, "corpo RUN_TEST_SUITE invalido");
    const auto& o = *pr.root;
    std::string suite;
    std::string seed = "42";
    getStr(o, "suite_id", suite);
    getStr(o, "seed", seed);
    std::vector<std::pair<std::string, std::string>> votes;
    auto itv = o.obj.find("votes");
    if (itv != o.obj.end() && itv->second->type == minijson::Value::Type::Arr) {
        for (const auto& item : itv->second->arr) {
            if (item->type != minijson::Value::Type::Str)
                return nak(run_id, "vote invalido na suite");
            const std::string& line = item->str;
            auto sp = line.find(' ');
            if (sp == std::string::npos)
                return nak(run_id, "vote invalido na suite");
            votes.emplace_back(line.substr(0, sp), line.substr(sp + 1));
        }
    }
    std::string rundir = workdir_ + "/runs/" + run_id;
    std::error_code ec;
    std::filesystem::create_directories(rundir, ec);
    ::chmod(rundir.c_str(), 0700);
    std::string cfg_path = rundir + "/election-config.json";
    {
        std::ifstream cf(cfg_path, std::ios::binary);
        if (!cf)
            return nak(run_id, "LOAD_TEST_FIXTURE antes de RUN_TEST_SUITE");
    }
    VotingSession s(rundir + "/work", true);
    if (!s.loadConfig(cfg_path).ok)
        return nak(run_id, "config da suite invalida");
    {
        std::ifstream sf(rundir + "/work/session.json", std::ios::binary);
        if (sf) {
            if (!s.resume().ok)
                return nak(run_id, "sessao do run corrompida");
        } else {
            if (!s.open().ok)
                return nak(run_id, "abertura da suite falhou");
        }
    }
    int confirmed = 0;
    if (s.state() != SessionState::Closed) {
        for (const auto& [office, value] : votes) {
            bool blank = (value == "BRANCO");
            BallotPreview pv = s.preview(office, blank ? "" : value, blank);
            if (pv.type != PreviewType::Nominal && pv.type != PreviewType::Legend &&
                pv.type != PreviewType::Blank && pv.type != PreviewType::Null)
                return nak(run_id, "voto recusado: " + pv.message_pt);
            std::string tok = s.beginConfirm(office);
            OpResult cr = s.confirm(office, blank ? "" : value, blank, tok);
            if (!cr.ok)
                return nak(run_id, cr.error);
            ++confirmed;
        }
        if (!s.close("").ok)
            return nak(run_id, "encerramento falhou");
    }
    std::string outdir = rundir + "/out";
    OpResult er = s.exportAll(outdir, "");
    if (!er.ok)
        return nak(run_id, er.error);
    nvlp::Frame f;
    f.type = nvlp::ACK;
    f.run_id = run_id;
    std::ostringstream b;
    b << "{\"suite_id\":\"" << jsonEscape(suite) << "\",\"seed\":\"" << jsonEscape(seed)
      << "\",\"confirmed\":" << confirmed << ",\"result_dir\":\"" << jsonEscape(outdir) << "\"}";
    f.body = b.str();
    return f;
}

nvlp::Frame LabDaemon::handleFrame(const nvlp::Frame& in) {
    if (!lab_test_mode_)
        return nak(in.run_id, "not_lab_build");
    switch (in.type) {
    case nvlp::HELLO: {
        auto pr = minijson::parse(in.body);
        std::string nonce;
        if (pr.ok && pr.root->type == minijson::Value::Type::Obj)
            getStr(*pr.root, "nonce", nonce);
        if (nonce.empty() || !nonces_.accept(nonce))
            return nak(in.run_id, "nonce ausente ou repetido");
        nvlp::Frame f;
        f.type = nvlp::HELLO_ACK;
        f.run_id = in.run_id;
        f.body = std::string("{\"proto\":1,\"core_version\":\"") + kSoftwareVersion +
                 "\",\"lab_mode\":true}";
        return f;
    }
    case nvlp::LOAD_TEST_FIXTURE: {
        std::string h = crypto::sha256Hex(in.body);
        auto it = runs_.find(in.run_id);
        if (it != runs_.end() && it->second.request_hash == "LOAD:" + h &&
            !it->second.response.empty()) {
            auto stored = nvlp::decode(it->second.response.data(), it->second.response.size());
            if (stored.ok)
                return stored.frame;
            return nak(in.run_id, "resposta armazenada corrompida");
        }
        if (it != runs_.end() && !it->second.request_hash.empty() &&
            it->second.request_hash != "LOAD:" + h)
            return nak(in.run_id, "run_id com pedido divergente");
        LoadResult lr = loadConfigFromText(in.body);
        if (!lr.ok)
            return nak(in.run_id, lr.error);
        std::string rundir = workdir_ + "/runs/" + in.run_id;
        std::error_code ec;
        std::filesystem::create_directories(rundir, ec);
        ::chmod(rundir.c_str(), 0700);
        std::string cfg_path = rundir + "/election-config.json";
        std::string tmp = cfg_path + ".tmp";
        FILE* f = std::fopen(tmp.c_str(), "wb");
        if (f == nullptr)
            return nak(in.run_id, "nao foi possivel guardar fixture");
        std::fwrite(in.body.data(), 1, in.body.size(), f);
        int fd = fileno(f);
        if (fd >= 0) {
            fchmod(fd, 0600);
            fsync(fd);
        }
        std::fclose(f);
        std::filesystem::rename(tmp, cfg_path, ec);
        if (ec)
            return nak(in.run_id, "nao foi possivel publicar fixture");
        RunSlot slot;
        slot.request_hash = "LOAD:" + h;
        auto enc = nvlp::encode(nvlp::ACK, in.run_id, "{\"stored\":true}");
        slot.response = enc.bytes;
        runs_[in.run_id] = slot;
        nvlp::Frame out;
        out.type = nvlp::ACK;
        out.run_id = in.run_id;
        out.body = "{\"stored\":true}";
        return out;
    }
    case nvlp::RUN_TEST_SUITE: {
        std::string h = crypto::sha256Hex(in.body);
        auto it = runs_.find(in.run_id);
        if (it != runs_.end() && it->second.request_hash == "RUN:" + h && it->second.done) {
            nvlp::Frame out;
            auto dec = nvlp::decode(it->second.response.data(), it->second.response.size());
            if (dec.ok)
                return dec.frame;
        }
        nvlp::Frame out = runSuite(in.run_id, in.body);
        if (out.type == nvlp::ACK) {
            auto enc = nvlp::encode(out.type, out.run_id, out.body);
            RunSlot slot = (it != runs_.end()) ? it->second : RunSlot{};
            slot.request_hash = "RUN:" + h;
            slot.response = enc.bytes;
            slot.done = true;
            runs_[in.run_id] = slot;
        }
        return out;
    }
    case nvlp::GET_STATUS: {
        nvlp::Frame f;
        f.type = nvlp::STATUS;
        f.run_id = in.run_id;
        auto it = runs_.find(in.run_id);
        bool done = (it != runs_.end() && it->second.done);
        f.body = std::string("{\"done\":") + (done ? "true" : "false") + "}";
        return f;
    }
    case nvlp::FETCH_RESULT: {
        auto it = runs_.find(in.run_id);
        if (it == runs_.end() || !it->second.done)
            return nak(in.run_id, "sem resultado para este run");
        std::string rundir = workdir_ + "/runs/" + in.run_id;
        std::ifstream bf(rundir + "/out/lab-boletim.json", std::ios::binary);
        if (!bf)
            return nak(in.run_id, "boletim ausente");
        std::stringstream ss;
        ss << bf.rdbuf();
        nvlp::Frame f;
        f.type = nvlp::RESULT;
        f.run_id = in.run_id;
        f.body = ss.str();
        return f;
    }
    case nvlp::CANCEL_TEST: {
        runs_.erase(in.run_id);
        std::error_code ec;
        std::filesystem::remove_all(workdir_ + "/runs/" + in.run_id, ec);
        nvlp::Frame f;
        f.type = nvlp::ACK;
        f.run_id = in.run_id;
        f.body = "{\"cancelled\":true}";
        return f;
    }
    default:
        return nak(in.run_id, "tipo desconhecido");
    }
}

int LabDaemon::serve(bool oneshot) {
    std::error_code ec;
    std::filesystem::create_directories(workdir_, ec);
    if (ec)
        return 2;
    ::chmod(workdir_.c_str(), 0700);
    std::string path = socketPath();
    ::unlink(path.c_str());
    int srv = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv < 0)
        return 2;
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (::bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        ::listen(srv, 4) != 0) {
        ::close(srv);
        return 2;
    }
    ::chmod(path.c_str(), 0700);
    int handled = 0;
    for (;;) {
        pollfd p{};
        p.fd = srv;
        p.events = POLLIN;
        if (poll(&p, 1, oneshot && handled > 0 ? 1000 : -1) <= 0)
            break;
        int fd = ::accept(srv, nullptr, nullptr);
        if (fd < 0)
            break;
        PeerCred cred = peerCred(fd);
        if (!peerAllowed(cred, policy::terminalStrict())) {
            ++rejected_peers_;
            ::close(fd);
            if (++handled > 0 && oneshot)
                break;
            continue;
        }
        std::uint8_t hdr[4];
        bool ok = readExact(fd, hdr, 4);
        std::uint32_t len = ok ? (static_cast<std::uint32_t>(hdr[0]) << 24) |
                                     (static_cast<std::uint32_t>(hdr[1]) << 16) |
                                     (static_cast<std::uint32_t>(hdr[2]) << 8) |
                                     static_cast<std::uint32_t>(hdr[3])
                               : 0;
        std::vector<std::uint8_t> buf;
        if (ok && len > 0 && len <= nvlp::kMaxFrame) {
            buf.resize(len);
            ok = readExact(fd, buf.data(), len);
        } else {
            ok = false;
        }
        nvlp::Frame resp;
        if (!ok) {
            resp.type = nvlp::NAK;
            resp.body = "{\"reason\":\"frame incompleto ou desconexao\"}";
        } else {
            auto dec = nvlp::decode(buf.data(), buf.size());
            if (!dec.ok) {
                resp.type = nvlp::NAK;
                resp.run_id = "";
                resp.body = "{\"reason\":\"" + minijson::escape(dec.detail) + "\"}";
            } else {
                resp = handleFrame(dec.frame);
            }
        }
        auto enc = nvlp::encode(resp.type, resp.run_id, resp.body);
        if (enc.ok) {
            std::uint32_t rl = static_cast<std::uint32_t>(enc.bytes.size());
            std::uint8_t ph[4] = {static_cast<std::uint8_t>((rl >> 24) & 0xFF),
                                  static_cast<std::uint8_t>((rl >> 16) & 0xFF),
                                  static_cast<std::uint8_t>((rl >> 8) & 0xFF),
                                  static_cast<std::uint8_t>(rl & 0xFF)};
            writeAll(fd, ph, 4);
            writeAll(fd, enc.bytes.data(), enc.bytes.size());
        }
        ::close(fd);
        if (++handled > 0 && oneshot)
            break;
    }
    ::close(srv);
    ::unlink(path.c_str());
    return 0;
}

} // namespace nexusvote::lab
