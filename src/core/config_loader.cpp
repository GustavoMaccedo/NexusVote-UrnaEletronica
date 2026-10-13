// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "config_loader.hpp"

#include <fstream>
#include <set>
#include <sstream>

#include "minijson.hpp"

namespace nexusvote {

std::string configErrorPrefix(const std::string& field, const std::string& msg) {
    return "config invalida [" + field + "]: " + msg;
}

namespace {

using minijson::Value;

bool reqStr(const Value& obj, const char* key, std::string& out, std::string& err) {
    auto it = obj.obj.find(key);
    if (it == obj.obj.end() || it->second->type != Value::Type::Str) {
        err = configErrorPrefix(key, "campo obrigatorio do tipo string");
        return false;
    }
    out = it->second->str;
    return true;
}

bool optBool(const Value& obj, const char* key, bool dflt) {
    auto it = obj.obj.find(key);
    if (it == obj.obj.end())
        return dflt;
    if (it->second->type != Value::Type::Bool)
        return dflt;
    return it->second->boolean;
}

bool allDigits(const std::string& s) {
    if (s.empty())
        return false;
    for (char c : s) {
        if (c < '0' || c > '9')
            return false;
    }
    return true;
}

} // namespace

LoadResult loadConfigFromText(const std::string& text) {
    LoadResult r;
    auto pr = minijson::parse(text);
    if (!pr.ok) {
        r.error = configErrorPrefix("sintaxe", pr.error);
        return r;
    }
    const Value& root = *pr.root;
    if (root.type != Value::Type::Obj) {
        r.error = configErrorPrefix("raiz", "objeto esperado");
        return r;
    }
    ElectionConfig cfg;
    auto itv = root.obj.find("schema_version");
    if (itv == root.obj.end() || itv->second->type != Value::Type::Int ||
        itv->second->integer != kSchemaVersion) {
        r.error = configErrorPrefix("schema_version", "esperado 1");
        return r;
    }
    if (!reqStr(root, "election_id", cfg.election_id, r.error))
        return r;
    if (!reqStr(root, "display_name", cfg.display_name, r.error))
        return r;
    if (cfg.election_id.empty() || cfg.display_name.empty()) {
        r.error = configErrorPrefix("election_id/display_name", "nao podem ser vazios");
        return r;
    }
    auto itloc = root.obj.find("locale");
    cfg.locale = (itloc != root.obj.end() && itloc->second->type == Value::Type::Str)
                     ? itloc->second->str
                     : "pt-BR";
    auto itprof = root.obj.find("rules_profile");
    cfg.rules_profile = (itprof != root.obj.end() && itprof->second->type == Value::Type::Str)
                            ? itprof->second->str
                            : kRulesProfile;
    cfg.legend_enabled = optBool(root, "legend_enabled", false);
    auto itmax = root.obj.find("test_max_voters");
    if (itmax != root.obj.end()) {
        if (itmax->second->type != Value::Type::Int || itmax->second->integer < 0) {
            r.error = configErrorPrefix("test_max_voters", "inteiro >= 0");
            return r;
        }
        cfg.test_max_voters = static_cast<int>(itmax->second->integer);
    }
    auto ituntil = root.obj.find("test_until_iso");
    if (ituntil != root.obj.end()) {
        if (ituntil->second->type != Value::Type::Str) {
            r.error = configErrorPrefix("test_until_iso", "string no formato YYYY-MM-DDTHH:MM:SSZ");
            return r;
        }
        const std::string& u = ituntil->second->str;
        if (u.size() != 20 || u[4] != '-' || u[7] != '-' || u[10] != 'T' || u[13] != ':' ||
            u[16] != ':' || u[19] != 'Z') {
            r.error = configErrorPrefix("test_until_iso", "formato esperado YYYY-MM-DDTHH:MM:SSZ");
            return r;
        }
        cfg.test_until_iso = u;
    }

    auto itoff = root.obj.find("offices");
    if (itoff == root.obj.end() || itoff->second->type != Value::Type::Arr ||
        itoff->second->arr.empty()) {
        r.error = configErrorPrefix("offices", "lista com ao menos 1 cargo");
        return r;
    }
    std::set<std::string> officeIds;
    for (const auto& item : itoff->second->arr) {
        if (item->type != Value::Type::Obj) {
            r.error = configErrorPrefix("offices[]", "objeto esperado");
            return r;
        }
        Office o;
        if (!reqStr(*item, "office_id", o.id, r.error))
            return r;
        if (!reqStr(*item, "title", o.title, r.error))
            return r;
        if (!officeIds.insert(o.id).second) {
            r.error = configErrorPrefix("offices", "office_id duplicado: " + o.id);
            return r;
        }
        auto itnl = item->obj.find("number_length");
        if (itnl == item->obj.end() || itnl->second->type != Value::Type::Int) {
            r.error = configErrorPrefix("number_length", "inteiro obrigatorio em " + o.id);
            return r;
        }
        o.number_length = static_cast<int>(itnl->second->integer);
        if (o.number_length < 1 || o.number_length > 6) {
            r.error = configErrorPrefix("number_length", "fora do intervalo 1..6 em " + o.id);
            return r;
        }
        auto itprop = item->obj.find("proportional");
        o.proportional = (itprop != item->obj.end() && itprop->second->type == Value::Type::Bool)
                             ? itprop->second->boolean
                             : false;
        auto itord = item->obj.find("order");
        o.order = (itord != item->obj.end() && itord->second->type == Value::Type::Int)
                      ? static_cast<int>(itord->second->integer)
                      : static_cast<int>(cfg.offices.size());
        auto itseats = item->obj.find("seats");
        o.seats = (itseats != item->obj.end() && itseats->second->type == Value::Type::Int)
                      ? static_cast<int>(itseats->second->integer)
                      : 1;
        if (o.seats < 1 || o.seats > 4) {
            r.error = configErrorPrefix("seats", "fora do intervalo 1..4 em " + o.id);
            return r;
        }
        auto itmate_req = item->obj.find("requires_mate");
        o.requires_mate =
            (itmate_req != item->obj.end() && itmate_req->second->type == Value::Type::Bool)
                ? itmate_req->second->boolean
                : false;
        cfg.offices.push_back(o);
    }

    auto itpar = root.obj.find("parties");
    std::set<std::string> partyNums;
    if (itpar != root.obj.end()) {
        if (itpar->second->type != Value::Type::Arr) {
            r.error = configErrorPrefix("parties", "array esperado");
            return r;
        }
        for (const auto& item : itpar->second->arr) {
            Party p;
            if (!reqStr(*item, "party_number", p.number, r.error))
                return r;
            if (!reqStr(*item, "name", p.name, r.error))
                return r;
            if (p.number.size() != 2 || !allDigits(p.number)) {
                r.error = configErrorPrefix("party_number", "exige 2 digitos: " + p.number);
                return r;
            }
            if (!partyNums.insert(p.number).second) {
                r.error = configErrorPrefix("parties", "partido duplicado: " + p.number);
                return r;
            }
            cfg.parties.push_back(p);
        }
    }

    auto itcand = root.obj.find("candidates");
    if (itcand == root.obj.end() || itcand->second->type != Value::Type::Arr ||
        itcand->second->arr.empty()) {
        r.error = configErrorPrefix("candidates", "lista com ao menos 1 candidato");
        return r;
    }
    std::set<std::string> candIds;
    std::set<std::string> numPerOffice;
    for (const auto& item : itcand->second->arr) {
        if (item->type != Value::Type::Obj) {
            r.error = configErrorPrefix("candidates[]", "objeto esperado");
            return r;
        }
        Candidate c;
        if (!reqStr(*item, "candidate_id", c.id, r.error))
            return r;
        if (!reqStr(*item, "office_id", c.office_id, r.error))
            return r;
        if (!reqStr(*item, "number", c.number, r.error))
            return r;
        if (!reqStr(*item, "name", c.name, r.error))
            return r;
        if (!reqStr(*item, "party_number", c.party_number, r.error))
            return r;
        if (!candIds.insert(c.id).second) {
            r.error = configErrorPrefix("candidates", "candidate_id duplicado: " + c.id);
            return r;
        }
        if (officeIds.count(c.office_id) == 0) {
            r.error = configErrorPrefix("candidates", "cargo inexistente: " + c.office_id);
            return r;
        }
        const Office* off = nullptr;
        for (const auto& o : cfg.offices) {
            if (o.id == c.office_id) {
                off = &o;
                break;
            }
        }
        if (off == nullptr || !allDigits(c.number) ||
            static_cast<int>(c.number.size()) != off->number_length) {
            r.error = configErrorPrefix("candidates", "numero incompativel com o cargo: " + c.id);
            return r;
        }
        std::string key = c.office_id + "#" + c.number;
        if (!numPerOffice.insert(key).second) {
            r.error = configErrorPrefix("candidates", "numero duplicado no cargo: " + key);
            return r;
        }
        if (cfg.legend_enabled && partyNums.count(c.party_number) == 0) {
            r.error = configErrorPrefix("candidates", "partido inexistente: " + c.party_number);
            return r;
        }
        auto itel = item->obj.find("eligible");
        c.eligible = (itel == item->obj.end() || itel->second->type != Value::Type::Bool)
                         ? true
                         : itel->second->boolean;
        auto itmate = item->obj.find("running_mate");
        if (itmate != item->obj.end()) {
            if (itmate->second->type != Value::Type::Str) {
                r.error = configErrorPrefix("running_mate", "string esperada em " + c.id);
                return r;
            }
            c.running_mate = itmate->second->str;
        }
        auto itsub = item->obj.find("substitutes");
        if (itsub != item->obj.end()) {
            if (itsub->second->type != Value::Type::Arr || itsub->second->arr.size() > 2) {
                r.error =
                    configErrorPrefix("substitutes", "array com no maximo 2 nomes em " + c.id);
                return r;
            }
            for (const auto& sn : itsub->second->arr) {
                if (sn->type != Value::Type::Str || sn->str.empty()) {
                    r.error = configErrorPrefix("substitutes", "nomes nao vazios em " + c.id);
                    return r;
                }
                c.substitutes.push_back(sn->str);
            }
        }
        cfg.candidates.push_back(c);
    }
    {
        auto itturn = root.obj.find("turn");
        cfg.turn = (itturn != root.obj.end() && itturn->second->type == Value::Type::Int)
                       ? static_cast<int>(itturn->second->integer)
                       : 1;
        if (cfg.turn < 1 || cfg.turn > 2) {
            r.error = configErrorPrefix("turn", "esperado 1 ou 2");
            return r;
        }
        auto itelec = root.obj.find("electorate_size");
        if (itelec != root.obj.end()) {
            if (itelec->second->type != Value::Type::Int || itelec->second->integer < 0) {
                r.error = configErrorPrefix("electorate_size", "inteiro >= 0 esperado");
                return r;
            }
            cfg.electorate_size = static_cast<std::uint64_t>(itelec->second->integer);
        }
    }
    for (const auto& c : cfg.candidates) {
        const Office* off = nullptr;
        for (const auto& o : cfg.offices) {
            if (o.id == c.office_id) {
                off = &o;
                break;
            }
        }
        if (off != nullptr && off->requires_mate && c.running_mate.empty()) {
            r.error = configErrorPrefix("running_mate", "vice exigido em " + c.id);
            return r;
        }
    }
    r.config = cfg;
    r.ok = true;
    return r;
}

LoadResult loadConfigFromFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        LoadResult r;
        r.error = "nao foi possivel abrir: " + path;
        return r;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return loadConfigFromText(ss.str());
}

} // namespace nexusvote
