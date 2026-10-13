// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include <iostream>

#include "test_framework.hpp"

int main() {
    int passed = 0;
    int failed = 0;
    for (auto& c : nvtest::registry()) {
        nvtest::current = c.suite + "." + c.name;
        int before = nvtest::failures;
        try {
            c.fn();
        } catch (const std::exception& e) {
            ++nvtest::failures;
            std::cout << "  EXCECAO [" << nvtest::current << "]: " << e.what() << "\n";
        } catch (...) {
            ++nvtest::failures;
            std::cout << "  EXCECAO desconhecida [" << nvtest::current << "]\n";
        }
        if (nvtest::failures == before) {
            ++passed;
            std::cout << "  PASS " << nvtest::current << "\n";
        } else {
            ++failed;
        }
    }
    std::cout << "==== nvtests: " << passed << " PASS, " << failed << " FAIL, " << nvtest::checks
              << " checagens ====\n";
    return failed == 0 ? 0 : 1;
}
