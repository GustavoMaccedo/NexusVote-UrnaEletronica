// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "vote_store.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>

#include "minijson.hpp"

namespace nexusvote {

namespace {

bool validHex32(const std::string& s) {
    if (s.size() != 32)
        return false;
    for (char c : s) {
        bool h = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!h)
            return false;
    }
    return true;
}

} // namespace

std::string VoteStore::serialize(const VoteRecord& rec) {
    return std::string("{\"schema_version\":1,\"record_id\":\"") + rec.record_id +
           "\",\"office_id\":\"" + minijson::escape(rec.office_id) + "\",\"kind\":\"" +
           voteKindToString(rec.kind) + "\",\"ref\":\"" + minijson::escape(rec.ref) + "\"}";
}

bool VoteStore::deserialize(const std::string& line, VoteRecord& rec, std::string& err) {
    auto pr = minijson::parse(line);
    if (!pr.ok) {
        err = "linha invalida: " + pr.error;
        return false;
    }
    const auto& o = pr.root->obj;
    auto get = [&](const char* k, std::string& out) -> bool {
        auto it = o.find(k);
        if (it == o.end() || it->second->type != minijson::Value::Type::Str) {
            err = std::string("campo ausente: ") + k;
            return false;
        }
        out = it->second->str;
        return true;
    };
    std::string kind;
    if (!get("record_id", rec.record_id) || !get("office_id", rec.office_id) ||
        !get("kind", kind) || !get("ref", rec.ref)) {
        return false;
    }
    if (!validHex32(rec.record_id)) {
        err = "record_id invalido";
        return false;
    }
    if (kind == "candidate") {
        rec.kind = VoteKind::Candidate;
    } else if (kind == "party") {
        rec.kind = VoteKind::Party;
    } else if (kind == "blank") {
        rec.kind = VoteKind::Blank;
    } else if (kind == "null") {
        rec.kind = VoteKind::Null;
    } else {
        err = "kind invalido";
        return false;
    }
    if (rec.office_id.empty()) {
        err = "office_id vazio";
        return false;
    }
    return true;
}

StoreResult VoteStore::open() {
    StoreResult r;
    records_.clear();
    FILE* f = std::fopen(path_.c_str(), "rb");
    if (f == nullptr) {
        r.ok = true;
        return r;
    }
    std::string content;
    char buf[8192];
    std::size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        content.append(buf, n);
    }
    std::fclose(f);
    bool ends_nl = content.empty() || content.back() == '\n';
    std::size_t pos = 0;
    while (pos < content.size()) {
        std::size_t nl = content.find('\n', pos);
        if (nl == std::string::npos)
            break;
        std::string line = content.substr(pos, nl - pos);
        pos = nl + 1;
        if (line.empty())
            continue;
        VoteRecord rec;
        std::string err;
        if (!deserialize(line, rec, err)) {
            r.ok = false;
            r.error = "journal corrompido: " + err;
            records_.clear();
            return r;
        }
        records_.push_back(rec);
    }
    if (!ends_nl && content.find('\n') != std::string::npos) {
        r.recovered_partial = true;
    } else if (!ends_nl && !content.empty()) {
        VoteRecord rec;
        std::string err;
        if (!deserialize(content.substr(pos), rec, err)) {
            r.recovered_partial = true;
        } else {
            records_.push_back(rec);
        }
    }
    r.ok = true;
    return r;
}

StoreResult VoteStore::append(const VoteRecord& rec) {
    StoreResult r;
    int fd = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
    if (fd < 0) {
        r.error = "nao foi possivel abrir journal para escrita";
        return r;
    }
    std::string line = serialize(rec) + "\n";
    const char* p = line.data();
    std::size_t left = line.size();
    while (left > 0) {
        ssize_t w = ::write(fd, p, left);
        if (w <= 0) {
            r.error = "falha de escrita no journal (voto NAO confirmado)";
            ::close(fd);
            return r;
        }
        p += w;
        left -= static_cast<std::size_t>(w);
    }
    if (::fsync(fd) != 0) {
        r.error = "falha de fsync no journal (voto NAO confirmado)";
        ::close(fd);
        return r;
    }
    ::close(fd);
    records_.push_back(rec);
    r.ok = true;
    return r;
}

} // namespace nexusvote
