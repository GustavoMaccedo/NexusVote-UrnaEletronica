// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "session.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

#include "audit_export.hpp"
#include "config_loader.hpp"
#include "core/clock.hpp"
#include "core/policy.hpp"
#include "crypto_adapter.hpp"
#include "minijson.hpp"
#include "secure/secure_session.hpp"

namespace nexusvote {

namespace {

std::string nowIso() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
    gmtime_r(&t, &tmv);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmv);
    return std::string(buf);
}

bool writeFile(const std::string& path, const std::string& data, std::string& err) {
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
    if (fd >= 0) {
        fchmod(fd, 0600);
        fsync(fd);
    }
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

VotingSession::VotingSession(const std::string& workdir, bool lab_test_mode)
    : workdir_(workdir), lab_test_mode_(lab_test_mode),
      store_(workdir + "/lab-vote-records.jsonl") {}

OpResult VotingSession::setState(SessionState to) {
    OpResult r;
    if (!canTransition(state_, to)) {
        r.error = "transicao invalida: " + stateToString(state_) + " -> " + stateToString(to);
        return r;
    }
    state_ = to;
    r.ok = true;
    return r;
}

OpResult VotingSession::logEvent(const std::string& line) {
    OpResult r;
    FILE* f = std::fopen((workdir_ + "/events.log").c_str(), "ab");
    if (f == nullptr) {
        r.error = "nao foi possivel abrir events.log";
        return r;
    }
    std::string full = nowIso() + " " + line + "\n";
    std::fwrite(full.data(), 1, full.size(), f);
    std::fflush(f);
    int fd = fileno(f);
    if (fd >= 0) {
        fchmod(fd, 0600);
        fsync(fd);
    }
    std::fclose(f);
    r.ok = true;
    return r;
}

bool VotingSession::persistSessionState(const std::string& state_str) {
    std::ostringstream sj;
    sj << "{\"schema_version\":1,\"lab_session_id\":\"" << session_id_ << "\",\"opened_at\":\""
       << opened_at_ << "\",\"config_sha256\":\"" << config_sha_ << "\",\"state\":\"" << state_str
       << "\",\"completed_rounds\":" << completed_rounds_
       << ",\"round_confirms\":" << round_confirms_ << ",\"closed_at\":\"" << closed_at_
       << "\",\"clock_trusted\":" << (clock_trusted_ ? 1 : 0) << ",\"last_token\":\"" << last_token_
       << "\",\"token_used\":" << (token_used_ ? 1 : 0)
       << ",\"justificativas\":" << justificativas_;
    sj << ",\"round_refs\":{";
    bool first_rr = true;
    for (const auto& kv : round_refs_) {
        if (!first_rr) {
            sj << ",";
        }
        first_rr = false;
        sj << "\"" << kv.first << "\":[";
        bool first_r = true;
        for (const auto& ref : kv.second) {
            if (!first_r) {
                sj << ",";
            }
            first_r = false;
            sj << "\"" << ref << "\"";
        }
        sj << "]";
    }
    sj << "}}";
    std::string werr;
    if (!writeFile(workdir_ + "/session.json", sj.str(), werr)) {
        logEvent("FALHA persist_session_state: " + werr);
        return false;
    }
    return true;
}

OpResult VotingSession::loadConfig(const std::string& config_path) {
    OpResult r;
    if (state_ != SessionState::Unconfigured) {
        r.error = "configuracao imutavel apos preparacao (estado " + stateToString(state_) + ")";
        return r;
    }
    LoadResult lr = loadConfigFromFile(config_path);
    if (!lr.ok) {
        setState(SessionState::Error);
        r.error = lr.error;
        return r;
    }
    std::string hash_err;
    std::string h = crypto::sha256FileHex(config_path, hash_err);
    if (h.empty()) {
        setState(SessionState::Error);
        r.error = hash_err;
        return r;
    }
    config_ = lr.config;
    config_sha_ = h;
    rules_.version = 1;
    rules_.legend_enabled = config_.legend_enabled;
    std::error_code ec;
    std::filesystem::create_directories(workdir_, ec);
    if (ec) {
        setState(SessionState::Error);
        r.error = "nao foi possivel criar diretorio de trabalho";
        return r;
    }
    ::chmod(workdir_.c_str(), 0700);
    config_path_ = config_path;
    std::string werr;
    if (!writeFile(workdir_ + "/config.sha256", h + "\n", werr)) {
        setState(SessionState::Error);
        r.error = werr;
        return r;
    }
    OpResult s = setState(SessionState::Ready);
    if (!s.ok)
        return s;
    r.ok = true;
    r.info = "Eleição fictícia pronta (NÃO OFICIAL): " + config_.display_name;
    return r;
}

OpResult VotingSession::open() {
    OpResult r;
    if (state_ == SessionState::Open || state_ == SessionState::Voting) {
        r.ok = true;
        r.info = "sessao ja aberta: " + session_id_;
        return r;
    }
    if (state_ != SessionState::Ready) {
        r.error = "abertura exige estado READY (atual " + stateToString(state_) + ")";
        return r;
    }
    {
        std::ifstream sf(workdir_ + "/session.json", std::ios::binary);
        if (sf) {
            OpResult rr = resume();
            if (rr.ok) {
                r.ok = true;
                r.info = "sessao readotada: " + session_id_;
                return r;
            }
        }
    }
    std::string lock = workdir_ + "/.lock";
    FILE* lf = std::fopen(lock.c_str(), "wx");
    if (lf == nullptr) {
        setState(SessionState::Error);
        r.error = "sessao concorrente detectada (lock existente); abertura recusada";
        return r;
    }
    std::fclose(lf);
    StoreResult sr = store_.open();
    if (!sr.ok) {
        setState(SessionState::Error);
        r.error = sr.error;
        return r;
    }
    if (!store_.all().empty()) {
        setState(SessionState::Error);
        r.error = "journal nao vazio em nova sessao; evidencia preservada";
        return r;
    }
    bool ok = false;
    session_id_ = "LAB-" + crypto::randomHex(8, ok);
    if (!ok) {
        setState(SessionState::Error);
        r.error = "falha no gerador aleatorio";
        return r;
    }
    opened_at_ = nowIso();
    hw::TimeReading tr = hw::now();
    clock_trusted_ = tr.trusted;
    OpResult s = setState(SessionState::Open);
    if (!s.ok)
        return s;
    logEvent("session_opened id=" + session_id_ + " config_sha=" + config_sha_);
    logEvent(std::string("clock trusted=") + (clock_trusted_ ? "1" : "0"));
    if (sr.recovered_partial) {
        logEvent("recovery sufixo_parcial_descartado");
    }
    std::string err;
    std::string zer = buildZeroReport(session_id_, opened_at_, config_sha_);
    if (!writeFile(workdir_ + "/lab-zeresima.json", zer, err)) {
        setState(SessionState::Error);
        r.error = err;
        return r;
    }
    if (policy::terminalStrict()) {
        if (!secure::isEnabled()) {
            setState(SessionState::Error);
            r.error = "terminal exige canal selado (WITH_RUST_SECURE=ON)";
            return r;
        }
        if (!clock_trusted_) {
            setState(SessionState::Error);
            r.error = "terminal exige RTC autoritativo (clock_trusted=0; ver ata)";
            return r;
        }
        std::string serr;
        if (!secure::initSealedSession(serr)) {
            setState(SessionState::Error);
            r.error = "falha ao iniciar sessão selada: " + serr;
            return r;
        }
    }
    if (!persistSessionState("OPEN")) {
        setState(SessionState::Error);
        r.error = "falha ao persistir session.json (estado divergente)";
        return r;
    }
    r.ok = true;
    r.info = "sessao aberta: " + session_id_;
    return r;
}

OpResult VotingSession::resume() {
    OpResult r;
    std::ifstream f(workdir_ + "/session.json", std::ios::binary);
    if (!f) {
        r.error = "sem sessao persistida";
        return r;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    auto pr = minijson::parse(ss.str());
    if (!pr.ok) {
        setState(SessionState::Error);
        r.error = "session.json corrompido: " + pr.error;
        return r;
    }
    const auto& o = pr.root->obj;
    auto get = [&](const char* k, std::string& out) -> bool {
        auto it = o.find(k);
        if (it == o.end() || it->second->type != minijson::Value::Type::Str)
            return false;
        out = it->second->str;
        return true;
    };
    std::string sid, oat, csha, st, cat;
    if (!get("lab_session_id", sid) || !get("opened_at", oat) || !get("config_sha256", csha) ||
        !get("state", st)) {
        setState(SessionState::Error);
        r.error = "session.json incompleto";
        return r;
    }
    get("closed_at", cat);
    auto getInt = [&](const char* k, std::uint64_t& out) -> bool {
        auto it = o.find(k);
        if (it == o.end() || it->second->type != minijson::Value::Type::Int)
            return false;
        out = static_cast<std::uint64_t>(it->second->integer);
        return true;
    };
    getInt("completed_rounds", completed_rounds_);
    getInt("round_confirms", round_confirms_);
    std::uint64_t clock_trusted = 1;
    if (getInt("clock_trusted", clock_trusted)) {
        clock_trusted_ = (clock_trusted != 0);
    }
    if (csha != config_sha_) {
        setState(SessionState::Error);
        r.error = "configuracao divergente da sessao persistida; alteracao exige nova sessao";
        return r;
    }
    StoreResult sr = store_.open();
    if (!sr.ok) {
        setState(SessionState::Error);
        r.error = sr.error;
        return r;
    }
    session_id_ = sid;
    opened_at_ = oat;
    closed_at_ = cat;
    std::string ltok;
    if (get("last_token", ltok)) {
        last_token_ = ltok;
    }
    std::uint64_t tused = 1;
    if (getInt("token_used", tused)) {
        token_used_ = (tused != 0);
    }
    std::uint64_t just = 0;
    if (getInt("justificativas", just)) {
        justificativas_ = just;
    }
    {
        auto itrr = o.find("round_refs");
        if (itrr != o.end() && itrr->second->type != minijson::Value::Type::Null &&
            itrr->second->type == minijson::Value::Type::Obj) {
            for (const auto& kv : itrr->second->obj) {
                if (kv.second->type != minijson::Value::Type::Arr) {
                    continue;
                }
                for (const auto& v : kv.second->arr) {
                    if (v->type == minijson::Value::Type::Str && !v->str.empty()) {
                        round_refs_[kv.first].push_back(v->str);
                    }
                }
            }
        }
    }
    state_ = SessionState::Open;
    if (st == "VOTING" || (!store_.all().empty() && st == "OPEN")) {
        state_ = SessionState::Voting;
    } else if (st == "CLOSED") {
        state_ = SessionState::Closed;
        TallyResult tr = tallyVotes(config_, store_.all());
        if (!tr.ok) {
            state_ = SessionState::Error;
            r.error = tr.error;
            return r;
        }
        tally_ = tr.tally;
    } else if (st != "OPEN") {
        state_ = SessionState::Error;
        r.error = "estado persistido desconhecido: " + st;
        return r;
    }
    if (sr.recovered_partial)
        logEvent("recovery sufixo_parcial_descartado");
    r.ok = true;
    r.info = "sessao readotada: " + session_id_;
    return r;
}

std::vector<std::string> VotingSession::officeOrder() const {
    std::vector<std::string> out;
    for (const auto& o : config_.offices)
        out.push_back(o.id);
    return out;
}

BallotPreview VotingSession::preview(const std::string& office_id, const std::string& digits,
                                     bool blank_requested) const {
    return classifyBallot(config_, rules_, office_id, digits, blank_requested);
}

std::string VotingSession::beginConfirm(const std::string& office_id) {
    ++confirm_counter_;
    bool ok = false;
    last_token_ =
        office_id + ":" + std::to_string(confirm_counter_) + ":" + crypto::randomHex(8, ok);
    token_used_ = false;
    if (!ok) {
        last_token_.clear();
        return "";
    }
    return last_token_;
}

OpResult VotingSession::confirm(const std::string& office_id, const std::string& digits,
                                bool blank_requested, const std::string& token) {
    OpResult r;
    if (state_ != SessionState::Open && state_ != SessionState::Voting) {
        r.error = "confirmacao exige sessao aberta (atual " + stateToString(state_) + ")";
        return r;
    }
    if (token.empty() || token != last_token_ || token_used_) {
        r.error = "token de confirmacao invalido ou reutilizado (anti-duplo)";
        return r;
    }
    if (!config_.test_until_iso.empty() && nowIso() >= config_.test_until_iso) {
        token_used_ = true;
        r.error = "ensaio encerrado pelo prazo de teste (test_until_iso)";
        return r;
    }
    if (config_.test_max_voters > 0 && round_confirms_ == 0 &&
        completed_rounds_ >= static_cast<std::uint64_t>(config_.test_max_voters)) {
        token_used_ = true;
        r.error = "limite de eleitores ficticios atingido (test_max_voters)";
        return r;
    }
    if (config_.electorate_size > 0 && round_confirms_ == 0 &&
        completed_rounds_ >= config_.electorate_size) {
        token_used_ = true;
        r.error = "eleitorado esgotado (electorate_size)";
        return r;
    }
    BallotPreview pv = preview(office_id, digits, blank_requested);
    if (pv.type != PreviewType::Nominal && pv.type != PreviewType::Legend &&
        pv.type != PreviewType::Blank && pv.type != PreviewType::Null) {
        r.error = "confirmacao recusada: " + pv.message_pt;
        return r;
    }
    VoteRecord rec;
    bool ok = false;
    rec.record_id = crypto::randomHex(16, ok);
    if (!ok) {
        r.error = "falha no gerador aleatorio (voto NAO confirmado)";
        return r;
    }
    rec.office_id = office_id;
    bool repetido_anulado = false;
    if (pv.type == PreviewType::Nominal) {
        rec.kind = VoteKind::Candidate;
        rec.ref = pv.candidate_id;
        auto& refs = round_refs_[office_id];
        if (std::find(refs.begin(), refs.end(), rec.ref) != refs.end()) {
            rec.kind = VoteKind::Null;
            rec.ref = "";
            repetido_anulado = true;
        } else {
            refs.push_back(rec.ref);
        }
    } else if (pv.type == PreviewType::Legend) {
        rec.kind = VoteKind::Party;
        rec.ref = pv.party_number;
    } else if (pv.type == PreviewType::Blank) {
        rec.kind = VoteKind::Blank;
    } else {
        rec.kind = VoteKind::Null;
    }
    StoreResult sr = store_.append(rec);
    if (!sr.ok) {
        token_used_ = true;
        r.error = sr.error;
        return r;
    }
    token_used_ = true;
    if (state_ == SessionState::Open)
        setState(SessionState::Voting);
    std::uint64_t round_size = 0;
    for (const auto& o : config_.offices) {
        round_size += static_cast<std::uint64_t>(o.seats > 0 ? o.seats : 1);
    }
    if (!config_.offices.empty()) {
        ++round_confirms_;
        if (round_size > 0 && round_confirms_ >= round_size) {
            round_confirms_ = 0;
            ++completed_rounds_;
            round_refs_.clear();
        }
    }
    if (!persistSessionState("VOTING")) {
        state_dirty_ = true;
        setState(SessionState::Error);
        r.error = "voto gravado, mas session.json falhou (estado divergente)";
        return r;
    }
    logEvent("office_vote_confirmed office_id=" + office_id);
    r.ok = true;
    r.info = "voto confirmado (" + previewTypeToString(pv.type) + ")" +
             (repetido_anulado ? " [repetido na vaga: anulado]" : "");
    return r;
}

OpResult VotingSession::close(const std::string& closed_at_iso) {
    OpResult r;
    if (state_ != SessionState::Open && state_ != SessionState::Voting) {
        r.error = "encerramento exige sessao aberta (atual " + stateToString(state_) + ")";
        return r;
    }
    OpResult s = setState(SessionState::Closing);
    if (!s.ok)
        return s;
    closed_at_ = closed_at_iso.empty() ? nowIso() : closed_at_iso;
    TallyResult tr = tallyVotes(config_, store_.all());
    if (!tr.ok) {
        setState(SessionState::Error);
        r.error = tr.error;
        return r;
    }
    tally_ = tr.tally;
    logEvent("session_closed id=" + session_id_);
    if (!persistSessionState("CLOSED")) {
        setState(SessionState::Error);
        r.error = "falha ao persistir session.json (estado divergente)";
        return r;
    }
    s = setState(SessionState::Closed);
    if (!s.ok)
        return s;
    r.ok = true;
    r.info = "sessao encerrada; novos votos bloqueados";
    return r;
}

std::size_t VotingSession::recordCount() const {
    return store_.size();
}

OpResult VotingSession::exportAll(const std::string& outdir, const std::string& build_info_path) {
    OpResult r;
    if (state_ != SessionState::Closed) {
        r.error = "exportacao exige sessao CLOSED";
        return r;
    }
    if (state_dirty_) {
        r.error = "estado divergente (persist falhou); rode resume antes de exportar";
        return r;
    }
    std::error_code ec;
    std::filesystem::create_directories(outdir, ec);
    if (ec) {
        r.error = "nao foi possivel criar diretorio de saida";
        return r;
    }
    auto readBytes = [](const std::string& path, std::string& out) -> bool {
        std::ifstream f(path, std::ios::binary);
        if (!f)
            return false;
        std::stringstream ss;
        ss << f.rdbuf();
        out = ss.str();
        return true;
    };
    std::string cfg_text;
    if (!readBytes(config_path_, cfg_text)) {
        r.error = "config de origem inacessivel para o bundle: " + config_path_;
        return r;
    }
    if (crypto::sha256Hex(cfg_text) != config_sha_) {
        r.error = "config de origem divergente da sessao; bundle recusado";
        return r;
    }
    std::vector<std::pair<std::string, std::string>> extras;
    extras.emplace_back("election-config.json", cfg_text);
    std::string zer;
    if (readBytes(workdir_ + "/lab-zeresima.json", zer))
        extras.emplace_back("lab-zeresima.json", zer);
    std::string events;
    if (readBytes(workdir_ + "/events.log", events))
        extras.emplace_back("events.log", events);
    if (!build_info_path.empty()) {
        std::string bi;
        if (readBytes(build_info_path, bi))
            extras.emplace_back("build-info.json", bi);
    }
    std::string err;
    if (!exportAllArtifactsFull(config_, session_id_, opened_at_, closed_at_, config_sha_,
                                store_.all(), tally_, justificativas_, outdir, extras, err)) {
        r.error = err;
        return r;
    }
    r.ok = true;
    r.info = "artefatos exportados em " + outdir;
    return r;
}

OpResult VotingSession::justifica(const std::string& motivo) {
    OpResult r;
    if (state_ != SessionState::Open && state_ != SessionState::Voting) {
        r.error = "justificativa exige sessao aberta (atual " + stateToString(state_) + ")";
        return r;
    }
    if (motivo.empty() || motivo.size() > 256) {
        r.error = "motivo exigido (1..256 caracteres)";
        return r;
    }
    ++justificativas_;
    logEvent("justificativa registrada total=" + std::to_string(justificativas_));
    if (!persistSessionState(state_ == SessionState::Voting ? "VOTING" : "OPEN")) {
        setState(SessionState::Error);
        r.error = "falha ao persistir session.json (estado divergente)";
        return r;
    }
    r.ok = true;
    r.info = "justificativa registrada (" + std::to_string(justificativas_) + ")";
    return r;
}

OpResult VotingSession::checkLabCommand(const std::string& cmd) const {
    OpResult r;
    (void)cmd;
    if (!lab_test_mode_) {
        r.error = "comando de teste rejeitado: build nao-laboratorio (not_lab_build)";
        return r;
    }
    r.ok = true;
    return r;
}

} // namespace nexusvote
