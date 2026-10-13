// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

#include <string>
#include <vector>

namespace nexusvote::paper {

inline constexpr int kPaperCols = 32;

enum class PrintOutcome { Ok, Ausente, Falha };

struct PrintResult {
    PrintOutcome outcome = PrintOutcome::Falha;
    std::string detail;
    bool ok() const {
        return outcome == PrintOutcome::Ok;
    }
};

std::string printerPath();
std::string buildJob(const std::string& title, const std::string& sessao,
                     const std::vector<std::string>& linhas, const std::string& qr_payload);
PrintResult printRaw(const std::string& dev, const std::string& job);

} // namespace nexusvote::paper
