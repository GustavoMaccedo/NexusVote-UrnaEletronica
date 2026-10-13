// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "rules.hpp"

namespace nexusvote {

namespace {

bool isDigits(const std::string& s) {
    if (s.empty())
        return false;
    for (char c : s) {
        if (c < '0' || c > '9')
            return false;
    }
    return true;
}

const Office* findOffice(const ElectionConfig& cfg, const std::string& id) {
    for (const auto& o : cfg.offices) {
        if (o.id == id)
            return &o;
    }
    return nullptr;
}

const Candidate* findCandidate(const ElectionConfig& cfg, const std::string& office_id,
                               const std::string& number) {
    for (const auto& c : cfg.candidates) {
        if (c.office_id == office_id && c.number == number && c.eligible)
            return &c;
    }
    return nullptr;
}

bool partyExists(const ElectionConfig& cfg, const std::string& num) {
    for (const auto& p : cfg.parties) {
        if (p.number == num)
            return true;
    }
    return false;
}

} // namespace

BallotPreview classifyBallot(const ElectionConfig& cfg, const ElectionRules& rules,
                             const std::string& office_id, const std::string& digits,
                             bool blank_requested) {
    BallotPreview pv;
    pv.office_id = office_id;
    pv.digits = digits;
    const Office* off = findOffice(cfg, office_id);
    if (off == nullptr) {
        pv.type = PreviewType::Invalid;
        pv.message_pt = "Cargo desconhecido na configuração.";
        return pv;
    }
    if (blank_requested) {
        pv.type = PreviewType::Blank;
        pv.message_pt = "VOTO EM BRANCO para " + off->title + ". Confirme para registrar.";
        return pv;
    }
    if (!isDigits(digits)) {
        pv.type = PreviewType::Invalid;
        pv.message_pt = "Entrada inválida: use apenas dígitos.";
        return pv;
    }
    if (static_cast<int>(digits.size()) != off->number_length) {
        pv.type = PreviewType::Incomplete;
        pv.message_pt = "Número incompleto: " + std::to_string(digits.size()) + " de " +
                        std::to_string(off->number_length) + " dígitos.";
        return pv;
    }
    const Candidate* cand = findCandidate(cfg, office_id, digits);
    if (cand != nullptr) {
        pv.type = PreviewType::Nominal;
        pv.candidate_id = cand->id;
        pv.message_pt = "Candidato: " + cand->name + " (" + digits + "). Confirme para registrar.";
        return pv;
    }
    if (rules.legend_enabled && off->proportional && off->number_length > 2) {
        std::string prefix = digits.substr(0, 2);
        if (partyExists(cfg, prefix)) {
            pv.type = PreviewType::Legend;
            pv.party_number = prefix;
            pv.message_pt =
                "VOTO DE LEGENDA para o partido " + prefix + ". Confirme para registrar.";
            return pv;
        }
    }
    pv.type = PreviewType::Null;
    pv.message_pt = "NÚMERO NÃO CORRESPONDE A CANDIDATO — o voto será NULO se confirmado.";
    return pv;
}

std::string voteKindToString(VoteKind k) {
    switch (k) {
    case VoteKind::Candidate:
        return "candidate";
    case VoteKind::Party:
        return "party";
    case VoteKind::Blank:
        return "blank";
    case VoteKind::Null:
        return "null";
    }
    return "null";
}

std::string previewTypeToString(PreviewType t) {
    switch (t) {
    case PreviewType::Nominal:
        return "nominal";
    case PreviewType::Legend:
        return "legenda";
    case PreviewType::Blank:
        return "branco";
    case PreviewType::Null:
        return "nulo";
    case PreviewType::Incomplete:
        return "incompleto";
    case PreviewType::Invalid:
        return "invalido";
    }
    return "invalido";
}

} // namespace nexusvote
