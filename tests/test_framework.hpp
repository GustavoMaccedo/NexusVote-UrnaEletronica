// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace nvtest {

struct Case {
    std::string suite;
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> r;
    return r;
}

inline int failures = 0;
inline int checks = 0;
inline std::string current;

inline void check(bool cond, const char* expr, const char* file, int line) {
    ++checks;
    if (!cond) {
        ++failures;
        std::cout << "  FALHA [" << current << "] " << file << ":" << line << ": " << expr << "\n";
    }
}

struct Registrar {
    Registrar(const std::string& suite, const std::string& name, std::function<void()> fn) {
        registry().push_back({suite, name, fn});
    }
};

} // namespace nvtest

#define TEST(suite, name)                                                                          \
    static void nvtest_fn_##suite##_##name();                                                      \
    static nvtest::Registrar nvtest_reg_##suite##_##name(#suite, #name,                            \
                                                         nvtest_fn_##suite##_##name);              \
    static void nvtest_fn_##suite##_##name()

#define EXPECT_TRUE(x) nvtest::check((x), #x, __FILE__, __LINE__)
#define EXPECT_FALSE(x) nvtest::check(!(x), "!(" #x ")", __FILE__, __LINE__)
#define EXPECT_EQ(a, b) nvtest::check((a) == (b), #a " == " #b, __FILE__, __LINE__)
