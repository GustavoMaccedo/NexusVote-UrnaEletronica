// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
// NexusVote - CLI de laboratório (NÃO é interface de eleitor oficial).
// Banner NÃO OFICIAL exibido em toda execução.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "../core/audit_export.hpp"
#include "../core/config_loader.hpp"
#include "../core/domain.hpp"
#include "../core/session.hpp"
#include "../lab/lab_daemon.hpp"
#include "../lab/mesario_daemon.hpp"
#include "../secure/secure_session.hpp"

namespace {

std::string exeBuildInfoPath(const std::string& argv0) {
    char buf[4096] = {0};
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    std::string exe = (n > 0) ? std::string(buf, static_cast<std::size_t>(n)) : argv0;
    auto slash = exe.find_last_of('/');
    std::string dir = (slash == std::string::npos) ? "." : exe.substr(0, slash);
    std::string cand = dir + "/build-info.json";
    std::ifstream f(cand, std::ios::binary);
    return f ? cand : "";
}

void banner() {
    std::cout << "==============================================================\n"
              << "PROTÓTIPO INDEPENDENTE - USO EXCLUSIVO EM LABORATÓRIO - NÃO OFICIAL\n"
              << "NexusVote " << nexusvote::kSoftwareVersion
              << " (núcleo C++). Eleição e candidatos fictícios.\n"
              << "==============================================================\n";
}

bool labModeFromEnv() {
#ifdef NEXUSVOTE_LAB_TEST_MODE
    return true;
#else
    return false;
#endif
}

std::string readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int cmdVoteBatch(nexusvote::VotingSession& s, const std::string& fixture) {
    std::ifstream f(fixture);
    if (!f) {
        std::cerr << "fixture inacessível: " << fixture << "\n";
        return 2;
    }
    std::string line;
    int lineno = 0;
    int confirmed = 0;
    while (std::getline(f, line)) {
        ++lineno;
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream ls(line);
        std::string office, token_value;
        ls >> office >> token_value;
        if (office.empty() || token_value.empty()) {
            std::cerr << "linha " << lineno << " inválida (esperado: OFFICE DIGITOS|BRANCO)\n";
            return 2;
        }
        bool blank = (token_value == "BRANCO");
        auto pv = s.preview(office, blank ? "" : token_value, blank);
        std::cout << "[" << office << "] " << pv.message_pt << "\n";
        if (pv.type != nexusvote::PreviewType::Nominal &&
            pv.type != nexusvote::PreviewType::Legend && pv.type != nexusvote::PreviewType::Blank &&
            pv.type != nexusvote::PreviewType::Null) {
            std::cerr << "voto recusado na linha " << lineno << "\n";
            return 2;
        }
        std::string tok = s.beginConfirm(office);
        auto r = s.confirm(office, blank ? "" : token_value, blank, tok);
        if (!r.ok) {
            std::cerr << "falha ao confirmar linha " << lineno << ": " << r.error << "\n";
            return 2;
        }
        ++confirmed;
    }
    std::cout << "confirmados: " << confirmed << "\n";
    return 0;
}

// TUI interativa de laboratório: teclado numérico, CORRIGE/BRANCO/CONFIRMA.
int cmdVoteInteractive(nexusvote::VotingSession& s) {
    std::cout << "Modo interativo de LABORATÓRIO (dados fictícios).\n"
              << "Teclas: 0-9 dígitos | B branco | C corrige | ENTER confirma | Q sai\n";
    for (const auto& office : s.officeOrder()) {
        std::string digits;
        bool blank = false;
        while (true) {
            auto pv = s.preview(office, digits, blank);
            std::cout << "\n[" << office
                      << "] digitado: " << (blank ? "(BRANCO)" : (digits.empty() ? "-" : digits))
                      << "\n"
                      << "  " << pv.message_pt << "\n> ";
            std::string in;
            if (!std::getline(std::cin, in)) {
                std::cout << "entrada encerrada; voto NÃO confirmado.\n";
                return 1;
            }
            if (in == "Q" || in == "q") {
                std::cout << "saída; voto NÃO confirmado.\n";
                return 1;
            }
            if (in == "C" || in == "c") {
                digits.clear();
                blank = false;
                continue;
            }
            if (in == "B" || in == "b") {
                blank = true;
                digits.clear();
                continue;
            }
            if (in.empty()) {
                if (pv.type != nexusvote::PreviewType::Nominal &&
                    pv.type != nexusvote::PreviewType::Legend &&
                    pv.type != nexusvote::PreviewType::Blank &&
                    pv.type != nexusvote::PreviewType::Null) {
                    std::cout << "Nada a confirmar. Digite o número ou B.\n";
                    continue;
                }
                std::string tok = s.beginConfirm(office);
                auto r = s.confirm(office, digits, blank, tok);
                if (!r.ok) {
                    std::cerr << "ERRO: " << r.error << " (voto NÃO confirmado)\n";
                    return 2;
                }
                digits.clear();
                blank = false;
                std::cout << "Voto registrado. \a\a\n"
                          << "----------------------------------------\n";
                break;
            }
            bool alld = !in.empty();
            for (char c : in) {
                if (c < '0' || c > '9')
                    alld = false;
            }
            if (!alld) {
                std::cout << "Entrada inválida: use apenas dígitos.\n";
                continue;
            }
            blank = false;
            digits += in;
        }
    }
    std::cout << "Todos os cargos votados nesta rodada. \a\a\a\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    banner();
    if (argc < 3) {
        std::cerr << "uso: nexusvote-cli [--lab] <workdir> "
                     "<init|open|vote-batch|vote|close|export|status|lab-serve|mesario-serve|"
                     "mesario-init> [args]\n"
                  << "  init <config.json> | open | vote-batch <fixture.txt> | vote | "
                     "close | export <outdir> | status | lab-serve [--oneshot] | "
                     "mesario-serve [--oneshot] | mesario-init <pin> | justifica <motivo>\n";
        return 2;
    }
    int ai = 1;
    bool lab_flag = false;
    if (std::string(argv[ai]) == "--lab") {
        lab_flag = true;
        ++ai;
    }
    bool lab = lab_flag || labModeFromEnv();
    std::string workdir = argv[ai++];
    if (ai >= argc) {
        std::cerr << "comando ausente\n";
        return 2;
    }
    std::string cmd = argv[ai++];
    // Servidor de laboratório NVLP (socket Unix): não exige NEXUSVOTE_CONFIG;
    if (cmd == "lab-serve") {
        bool oneshot = (ai < argc && std::string(argv[ai]) == "--oneshot");
        nexusvote::lab::LabDaemon daemon(workdir, lab);
        std::cout << "lab-serve em " << daemon.socketPath() << (oneshot ? " (oneshot)" : "")
                  << "\n";
        return daemon.serve(oneshot);
    }
    if (cmd == "mesario-serve") {
        bool oneshot = (ai < argc && std::string(argv[ai]) == "--oneshot");
        nexusvote::mesario::MesarioDaemon daemon(workdir, lab);
        std::cout << "mesario-serve em " << daemon.socketPath() << (oneshot ? " (oneshot)" : "")
                  << "\n";
        return daemon.serve(oneshot);
    }
    if (cmd == "mesario-init") {
        if (ai >= argc) {
            std::cerr
                << "mesario-init exige <pin> (6+ caracteres; cuidado com histórico do shell)\n";
            return 2;
        }
        if (!nexusvote::secure::isEnabled()) {
            std::cerr << "ERRO: canal Rust desativado; PIN exige Argon2 (WITH_RUST_SECURE=ON)\n";
            return 1;
        }
        std::string phc, err;
        if (!nexusvote::secure::hashPin(argv[ai], phc, err)) {
            std::cerr << "ERRO: " << err << "\n";
            return 1;
        }
        std::error_code ec;
        std::filesystem::create_directories(workdir, ec);
        const std::string path = workdir + "/mesario.phc";
        std::string werr;
        {
            std::string tmp = path + ".tmp";
            FILE* f = std::fopen(tmp.c_str(), "wb");
            if (f == nullptr) {
                std::cerr << "ERRO: nao foi possivel escrever " << tmp << "\n";
                return 1;
            }
            const bool written =
                phc.empty() || std::fwrite(phc.data(), 1, phc.size(), f) == phc.size();
            std::fflush(f);
            int fd = fileno(f);
            if (fd >= 0) {
                fchmod(fd, 0600);
                fsync(fd);
            }
            std::fclose(f);
            if (!written) {
                std::cerr << "ERRO: falha de escrita em " << tmp << "\n";
                return 1;
            }
            std::filesystem::rename(tmp, path, ec);
            if (ec) {
                std::cerr << "ERRO: falha ao publicar " << path << "\n";
                return 1;
            }
            (void)werr;
        }
        std::cout << "mesario PIN registrado em " << path << "\n";
        return 0;
    }
    nexusvote::VotingSession s(workdir, lab);

    if (cmd == "init") {
        if (ai >= argc) {
            std::cerr << "init exige <config.json>\n";
            return 2;
        }
        auto r = s.loadConfig(argv[ai]);
        std::cout << (r.ok ? r.info : std::string("ERRO: ") + r.error) << "\n";
        return r.ok ? 0 : 1;
    }
    const char* cfg_env = std::getenv("NEXUSVOTE_CONFIG");
    if (cfg_env == nullptr) {
        std::cerr << "NEXUSVOTE_CONFIG não definido (caminho da config de origem)\n";
        return 2;
    }
    auto lr = s.loadConfig(cfg_env);
    if (!lr.ok) {
        std::cerr << "ERRO: " << lr.error << "\n";
        return 1;
    }
    {
        std::ifstream sf(workdir + "/session.json", std::ios::binary);
        if (sf) {
            auto rr = s.resume();
            if (!rr.ok) {
                std::cerr << "ERRO: " << rr.error << "\n";
                return 1;
            }
        }
    }
    if (cmd == "open") {
        auto r = s.open();
        std::cout << (r.ok ? r.info : std::string("ERRO: ") + r.error) << "\n";
        if (r.ok) {
            std::string z = readAll(workdir + "/lab-zeresima.json");
            std::cout << z << "\n";
        }
        return r.ok ? 0 : 1;
    }
    // Abertura implícita idempotente (fluxo de laboratório): só abre se READY.
    if (s.state() == nexusvote::SessionState::Ready) {
        auto r = s.open();
        if (!r.ok) {
            std::cerr << "ERRO: " << r.error << "\n";
            return 1;
        }
    }
    if (cmd == "vote-batch") {
        if (ai >= argc) {
            std::cerr << "vote-batch exige <fixture.txt>\n";
            return 2;
        }
        return cmdVoteBatch(s, argv[ai]);
    }
    if (cmd == "vote")
        return cmdVoteInteractive(s);
    if (cmd == "close") {
        if (s.state() == nexusvote::SessionState::Closed) {
            std::cout << "sessao ja encerrada; novos votos bloqueados\n";
            return 0;
        }
        auto r = s.close("");
        std::cout << (r.ok ? r.info : std::string("ERRO: ") + r.error) << "\n";
        return r.ok ? 0 : 1;
    }
    if (cmd == "export") {
        if (ai >= argc) {
            std::cerr << "export exige <outdir>\n";
            return 2;
        }
        if (s.state() != nexusvote::SessionState::Closed) {
            auto rc = s.close("");
            if (!rc.ok) {
                std::cerr << "ERRO: " << rc.error << "\n";
                return 1;
            }
        }
        auto r = s.exportAll(argv[ai], exeBuildInfoPath(argv[0]));
        std::cout << (r.ok ? r.info : std::string("ERRO: ") + r.error) << "\n";
        return r.ok ? 0 : 1;
    }
    if (cmd == "status") {
        auto g = s.checkLabCommand("GET_STATUS");
        std::cout << "estado=" << stateToString(s.state()) << " lab=" << (lab ? "ON" : "OFF")
                  << " gate=" << (g.ok ? "allow" : g.error) << "\n";
        return 0;
    }
    if (cmd == "justifica") {
        if (ai >= argc) {
            std::cerr << "justifica exige <motivo> (1..256 caracteres)\n";
            return 2;
        }
        auto r = s.justifica(argv[ai]);
        std::cout << (r.ok ? r.info : std::string("ERRO: ") + r.error) << "\n";
        return r.ok ? 0 : 1;
    }
    std::cerr << "comando desconhecido: " << cmd << "\n";
    return 2;
}
