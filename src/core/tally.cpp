// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "tally.hpp"

#include <limits>

namespace nexusvote {

namespace {

bool addChecked(std::uint64_t a, std::uint64_t b, std::uint64_t& out) {
    if (a > std::numeric_limits<std::uint64_t>::max() - b)
        return false;
    out = a + b;
    return true;
}

} // namespace

TallyResult tallyVotes(const ElectionConfig& cfg, const std::vector<VoteRecord>& records) {
    TallyResult r;
    for (const auto& o : cfg.offices) {
        r.tally.by_office[o.id] = OfficeTally{};
    }
    for (const auto& rec : records) {
        auto it = r.tally.by_office.find(rec.office_id);
        if (it == r.tally.by_office.end()) {
            r.error = "registro com cargo desconhecido: " + rec.office_id;
            return r;
        }
        OfficeTally& t = it->second;
        std::uint64_t* slot = nullptr;
        if (rec.kind == VoteKind::Blank) {
            slot = &t.blank;
        } else if (rec.kind == VoteKind::Null) {
            slot = &t.null;
        } else if (rec.kind == VoteKind::Party) {
            t.legend[rec.ref] += 0;
            slot = &t.legend[rec.ref];
        } else {
            t.nominal[rec.ref] += 0;
            slot = &t.nominal[rec.ref];
        }
        std::uint64_t nv = 0;
        if (!addChecked(*slot, 1, nv)) {
            r.error = "overflow na contagem";
            return r;
        }
        *slot = nv;
    }
    for (auto& [id, t] : r.tally.by_office) {
        std::uint64_t sum = 0;
        std::uint64_t nv = 0;
        for (const auto& [k, v] : t.nominal) {
            if (!addChecked(sum, v, nv)) {
                r.error = "overflow na soma nominal";
                return r;
            }
            sum = nv;
        }
        for (const auto& [k, v] : t.legend) {
            if (!addChecked(sum, v, nv)) {
                r.error = "overflow na soma legenda";
                return r;
            }
            sum = nv;
        }
        if (!addChecked(sum, t.blank, nv)) {
            r.error = "overflow na soma branco";
            return r;
        }
        sum = nv;
        if (!addChecked(sum, t.null, nv)) {
            r.error = "overflow na soma nulo";
            return r;
        }
        t.total = nv;
    }
    r.ok = true;
    return r;
}

} // namespace nexusvote
