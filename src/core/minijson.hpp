// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace nexusvote::minijson {

struct Value;
using Object = std::map<std::string, std::shared_ptr<Value>>;
using Array = std::vector<std::shared_ptr<Value>>;

struct Value {
    enum class Type { Null, Bool, Int, Str, Arr, Obj };
    Type type = Type::Null;
    bool boolean = false;
    std::int64_t integer = 0;
    std::string str;
    Array arr;
    Object obj;
};

struct ParseResult {
    std::shared_ptr<Value> root;
    bool ok = false;
    std::string error;
};

ParseResult parse(const std::string& text);
std::string escape(const std::string& s);

} // namespace nexusvote::minijson
