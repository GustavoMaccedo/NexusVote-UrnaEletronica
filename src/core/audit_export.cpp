// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "audit_export.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <sstream>

#include "crypto_adapter.hpp"
#include "core/policy.hpp"
#include "minijson.hpp"

namespace nexusvote {

namespace {

bool uniformBelow(std::uint64_t n, std::uint64_t& out) {
    if (n == 0) {
        return false;
    }
    const std::uint64_t limit =
        (~static_cast<std::uint64_t>(0)) - ((~static_cast<std::uint64_t>(0)) % n);
    for (int tries = 0; tries < 64; ++tries) {
        std::array<std::uint8_t, 8> b{};
        if (!crypto::randomBytes(b.data(), b.size())) {
            return false;
        }
        std::uint64_t v = 0;
        for (std::uint8_t c : b) {
            v = (v << 8) | c;
        }
        if (v < limit) {
            out = v % n;
            return true;
        }
    }
    return false;
}

std::string shuffledRdv(const std::string& session_id, std::vector<VoteRecord> records,
                        std::string& err) {
    for (std::size_t i = records.size(); i > 1; --i) {
        std::uint64_t j = 0;
        if (!uniformBelow(i, j)) {
            err = "falha no gerador aleatorio para embaralhamento";
            return "";
        }
        std::swap(records[i - 1], records[static_cast<std::size_t>(j)]);
    }
    std::ostringstream o;
    o << "{\"schema_version\":1,\"lab_session_id\":\"" << minijson::escape(session_id)
      << "\",\"records\":[";
    bool first = true;
    for (const auto& rec : records) {
        if (!first)
            o << ",";
        first = false;
        o << "{\"record_id\":\"" << rec.record_id << "\",\"office_id\":\""
          << minijson::escape(rec.office_id) << "\",\"kind\":\"" << voteKindToString(rec.kind)
          << "\",\"ref\":\"" << minijson::escape(rec.ref) << "\"}";
    }
    o << "]}";
    return o.str();
}

bool writeAtom(const std::string& path, const std::string& data, std::string& err) {
    std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (f == nullptr) {
        err = "nao foi possivel escrever: " + tmp;
        return false;
    }
    if (!data.empty() && std::fwrite(data.data(), 1, data.size(), f) != data.size()) {
        err = "falha de escrita: " + tmp;
        std::fclose(f);
        return false;
    }
    std::fflush(f);
    int fd = fileno(f);
    if (fd >= 0)
        fsync(fd);
    std::fclose(f);
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        err = "falha ao publicar: " + path;
        return false;
    }
    return true;
}

} // namespace

std::string buildZeroReport(const std::string& session_id, const std::string& opened_at,
                            const std::string& config_sha) {
    std::ostringstream o;
    o << "{\"schema_version\":1,\"report\":\"zeresima-didatica\",\"banner\":\""
      << "PROTÓTIPO INDEPENDENTE — USO EXCLUSIVO EM LABORATÓRIO — NÃO OFICIAL"
      << "\",\"lab_session_id\":\"" << minijson::escape(session_id) << "\",\"opened_at\":\""
      << minijson::escape(opened_at) << "\",\"config_sha256\":\"" << config_sha
      << "\",\"totals\":\"all_zero\"}";
    return o.str();
}

bool exportAllArtifacts(const ElectionConfig& cfg, const std::string& session_id,
                        const std::string& opened_at, const std::string& closed_at,
                        const std::string& config_sha, const std::vector<VoteRecord>& records,
                        const SessionTally& tally, const std::string& outdir,
                        const std::vector<std::pair<std::string, std::string>>& extra_files,
                        std::string& err) {
    return exportAllArtifactsFull(cfg, session_id, opened_at, closed_at, config_sha, records, tally,
                                  0, outdir, extra_files, err);
}

bool exportAllArtifactsFull(const ElectionConfig& cfg, const std::string& session_id,
                            const std::string& opened_at, const std::string& closed_at,
                            const std::string& config_sha, const std::vector<VoteRecord>& records,
                            const SessionTally& tally, std::uint64_t justificativas,
                            const std::string& outdir,
                            const std::vector<std::pair<std::string, std::string>>& extra_files,
                            std::string& err) {
    std::string rdv = shuffledRdv(session_id, records, err);
    if (rdv.empty() && !records.empty())
        return false;
    if (records.empty()) {
        rdv = "{\"schema_version\":1,\"lab_session_id\":\"" + minijson::escape(session_id) +
              "\",\"records\":[]}";
    }
    std::ostringstream b;
    b << "{\"schema_version\":1,\"banner\":\""
      << "NÃO OFICIAL — BOLETIM DE DEMONSTRAÇÃO. Sem validade eleitoral.\","
      << "\"lab_session_id\":\"" << minijson::escape(session_id) << "\",\"software\":\"nexusvote "
      << kSoftwareVersion << "\",\"rules_profile\":\"" << minijson::escape(cfg.rules_profile)
      << "\",\"opened_at\":\"" << minijson::escape(opened_at) << "\",\"closed_at\":\""
      << minijson::escape(closed_at) << "\",\"config_sha256\":\"" << config_sha
      << "\",\"turn\":" << cfg.turn << ",\"justificativas\":" << justificativas << ",\"offices\":{";
    bool first_off = true;
    for (const auto& o : cfg.offices) {
        if (!first_off)
            b << ",";
        first_off = false;
        auto it = tally.by_office.find(o.id);
        const OfficeTally& t = (it != tally.by_office.end()) ? it->second : OfficeTally{};
        b << "\"" << minijson::escape(o.id) << "\":{\"nominais\":{";
        bool first = true;
        for (const auto& [cid, n] : t.nominal) {
            if (!first)
                b << ",";
            first = false;
            b << "\"" << minijson::escape(cid) << "\":" << n;
        }
        b << "},\"legendas\":{";
        first = true;
        for (const auto& [pn, n] : t.legend) {
            if (!first)
                b << ",";
            first = false;
            b << "\"" << minijson::escape(pn) << "\":" << n;
        }
        b << "},\"brancos\":" << t.blank << ",\"nulos\":" << t.null << ",\"total\":" << t.total
          << "}";
    }
    b << "},\"software_version\":\"" << kSoftwareVersion << "\"}";
    std::string boletim = b.str();

    if (!writeAtom(outdir + "/lab-rdv.json", rdv, err))
        return false;
    if (!writeAtom(outdir + "/lab-boletim.json", boletim, err))
        return false;

    crypto::EdKeypair kp;
    std::string kerr;
    std::string pubhex;
    std::string sighex;
    if (crypto::edGenerate(kp, kerr)) {
        std::vector<std::uint8_t> sig;
        if (crypto::edSign(kp, boletim, sig, kerr)) {
            pubhex = crypto::toHex(kp.pub.data(), kp.pub.size());
            sighex = crypto::toHex(sig.data(), sig.size());
        }
    }
    if ((pubhex.empty() || sighex.empty()) && policy::terminalStrict()) {
        err = "terminal exige assinatura de cerimonia (demo indisponivel)";
        return false;
    }
    std::ostringstream sigj;
    sigj << "{\"schema_version\":1,\"alg\":\"Ed25519-demo\",\"key_id\":\"lab-demo\","
         << "\"pubkey\":\"" << pubhex << "\",\"sig_boletim\":\"" << sighex
         << "\",\"note\":\"chave de demonstracao gerada no dispositivo; sem valor eleitoral\"}";
    if (!writeAtom(outdir + "/lab-signature.json", sigj.str(), err))
        return false;
    if (!writeAtom(outdir + "/lab-pubkey.hex", pubhex + "\n", err))
        return false;
    std::vector<std::string> manifest_names = {"lab-rdv.json", "lab-boletim.json",
                                               "lab-signature.json", "lab-pubkey.hex"};
    for (const auto& [name, content] : extra_files) {
        if (!writeAtom(outdir + "/" + name, content, err))
            return false;
        manifest_names.push_back(name);
    }

    std::string herr;
    std::ostringstream m;
    for (const auto& name : manifest_names) {
        std::string h = crypto::sha256FileHex(outdir + "/" + name, herr);
        if (h.empty()) {
            err = "falha ao calcular hash de: " + name;
            return false;
        }
        m << h << "  " << name << "\n";
    }
    if (!writeAtom(outdir + "/manifest.sha256", m.str(), err))
        return false;
    return true;
}

} // namespace nexusvote
