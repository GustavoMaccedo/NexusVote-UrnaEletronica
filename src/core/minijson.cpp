// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "minijson.hpp"

namespace nexusvote::minijson {

namespace {

struct Cursor {
    const char* p = nullptr;
    const char* end = nullptr;
    std::string error;
    bool fail(const std::string& msg) {
        error = msg;
        return false;
    }
    void skipWs() {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
            ++p;
    }
    bool consume(char c) {
        if (p < end && *p == c) {
            ++p;
            return true;
        }
        return false;
    }
};

std::shared_ptr<Value> parseValue(Cursor& c);

bool parseLiteral(Cursor& c, const char* word) {
    for (const char* w = word; *w; ++w) {
        if (c.p >= c.end || *c.p != *w)
            return false;
        ++c.p;
    }
    return true;
}

std::shared_ptr<Value> parseString(Cursor& c) {
    auto v = std::make_shared<Value>();
    v->type = Value::Type::Str;
    if (!c.consume('"')) {
        c.fail("string deve comecar com aspas");
        return nullptr;
    }
    std::string out;
    while (c.p < c.end) {
        char ch = *c.p++;
        if (ch == '"') {
            v->str = out;
            return v;
        }
        if (ch == '\\') {
            if (c.p >= c.end)
                break;
            char e = *c.p++;
            switch (e) {
            case '"':
                out.push_back('"');
                break;
            case '\\':
                out.push_back('\\');
                break;
            case '/':
                out.push_back('/');
                break;
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 't':
                out.push_back('\t');
                break;
            default:
                c.fail("escape invalido em string");
                return nullptr;
            }
        } else {
            out.push_back(ch);
        }
    }
    c.fail("string nao terminada");
    return nullptr;
}

std::shared_ptr<Value> parseNumber(Cursor& c) {
    const char* start = c.p;
    if (c.p < c.end && (*c.p == '-' || *c.p == '+'))
        ++c.p;
    bool any = false;
    while (c.p < c.end && *c.p >= '0' && *c.p <= '9') {
        ++c.p;
        any = true;
    }
    if (!any || (c.p < c.end && (*c.p == '.' || *c.p == 'e' || *c.p == 'E'))) {
        c.fail("apenas inteiros sao suportados");
        return nullptr;
    }
    auto v = std::make_shared<Value>();
    v->type = Value::Type::Int;
    try {
        v->integer = std::stoll(std::string(start, c.p));
    } catch (...) {
        c.fail("inteiro invalido");
        return nullptr;
    }
    return v;
}

std::shared_ptr<Value> parseArray(Cursor& c) {
    auto v = std::make_shared<Value>();
    v->type = Value::Type::Arr;
    c.consume('[');
    c.skipWs();
    if (c.consume(']'))
        return v;
    while (true) {
        c.skipWs();
        auto item = parseValue(c);
        if (item == nullptr)
            return nullptr;
        v->arr.push_back(item);
        c.skipWs();
        if (c.consume(']'))
            return v;
        if (!c.consume(',')) {
            c.fail("esperado ',' ou ']' em array");
            return nullptr;
        }
    }
}

std::shared_ptr<Value> parseObject(Cursor& c) {
    auto v = std::make_shared<Value>();
    v->type = Value::Type::Obj;
    c.consume('{');
    c.skipWs();
    if (c.consume('}'))
        return v;
    while (true) {
        c.skipWs();
        if (c.p >= c.end || *c.p != '"') {
            c.fail("chave de objeto deve ser string");
            return nullptr;
        }
        auto key = parseString(c);
        if (key == nullptr)
            return nullptr;
        c.skipWs();
        if (!c.consume(':')) {
            c.fail("esperado ':' em objeto");
            return nullptr;
        }
        c.skipWs();
        auto val = parseValue(c);
        if (val == nullptr)
            return nullptr;
        v->obj[key->str] = val;
        c.skipWs();
        if (c.consume('}'))
            return v;
        if (!c.consume(',')) {
            c.fail("esperado ',' ou '}' em objeto");
            return nullptr;
        }
    }
}

std::shared_ptr<Value> parseValue(Cursor& c) {
    c.skipWs();
    if (c.p >= c.end) {
        c.fail("fim inesperado");
        return nullptr;
    }
    char ch = *c.p;
    if (ch == '"')
        return parseString(c);
    if (ch == '{')
        return parseObject(c);
    if (ch == '[')
        return parseArray(c);
    if (ch == 't') {
        if (!parseLiteral(c, "true")) {
            c.fail("literal invalido");
            return nullptr;
        }
        auto v = std::make_shared<Value>();
        v->type = Value::Type::Bool;
        v->boolean = true;
        return v;
    }
    if (ch == 'f') {
        if (!parseLiteral(c, "false")) {
            c.fail("literal invalido");
            return nullptr;
        }
        auto v = std::make_shared<Value>();
        v->type = Value::Type::Bool;
        v->boolean = false;
        return v;
    }
    if (ch == 'n') {
        if (!parseLiteral(c, "null")) {
            c.fail("literal invalido");
            return nullptr;
        }
        return std::make_shared<Value>();
    }
    if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '+')
        return parseNumber(c);
    c.fail("valor invalido");
    return nullptr;
}

} // namespace

ParseResult parse(const std::string& text) {
    ParseResult r;
    Cursor c;
    c.p = text.data();
    c.end = text.data() + text.size();
    auto v = parseValue(c);
    if (v == nullptr) {
        r.error = c.error;
        return r;
    }
    c.skipWs();
    if (c.p != c.end) {
        r.error = "conteudo apos o valor raiz";
        return r;
    }
    r.root = v;
    r.ok = true;
    return r;
}

std::string escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char ch : s) {
        switch (ch) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    return out;
}

} // namespace nexusvote::minijson
