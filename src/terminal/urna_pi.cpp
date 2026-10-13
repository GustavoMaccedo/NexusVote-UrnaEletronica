// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "../core/session.hpp"
#include "../paper/paper_escpos.hpp"
#include "../lab/keypad_evdev.hpp"

namespace {

std::atomic<bool> g_stop{false};
void onSig(int) {
    g_stop = true;
}

std::string nowIso() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
    gmtime_r(&t, &tmv);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmv);
    return buf;
}

void hwLog(const std::string& workdir, const std::string& evento, const std::string& detalhe) {
    std::string path = workdir + "/hw-events.jsonl";
    FILE* f = std::fopen(path.c_str(), "ab");
    if (!f)
        return;
    std::string line = "{\"ts\":\"" + nowIso() + "\",\"evento\":\"" + evento + "\",\"detalhe\":\"" +
                       detalhe + "\"}\n";
    std::fwrite(line.data(), 1, line.size(), f);
    std::fflush(f);
    int fd = fileno(f);
    if (fd >= 0)
        fsync(fd);
    std::fclose(f);
}

bool sysfsCritical() {
    try {
        namespace fs = std::filesystem;
        std::error_code ec;
        if (!fs::exists("/sys/class/power_supply", ec) || ec)
            return false;
        bool onBattery = false;
        int pct = 100;
        bool seen = false;
        fs::directory_iterator it("/sys/class/power_supply", ec);
        if (ec)
            return false;
        for (auto& e : it) {
            std::string type, online, cap, status;
            std::ifstream(e.path() / "type") >> type;
            for (auto& c : type)
                c = (char)tolower(c);
            if (type.find("mains") != std::string::npos || type.find("usb") != std::string::npos ||
                type.find("ac") != std::string::npos) {
                std::ifstream f(e.path() / "online");
                std::string v;
                f >> v;
                if (v == "0")
                    onBattery = true;
                seen = true;
            }
            if (type.find("battery") != std::string::npos ||
                type.find("ups") != std::string::npos) {
                std::ifstream f(e.path() / "capacity");
                int p = -1;
                f >> p;
                if (p >= 0 && p <= 100) {
                    pct = std::min(pct, p);
                    seen = true;
                }
                if (!seen) {
                    std::ifstream s(e.path() / "status");
                    std::string st;
                    s >> st;
                    for (auto& c : st)
                        c = (char)tolower(c);
                    if (st.find("discharg") != std::string::npos)
                        onBattery = true;
                    seen = true;
                }
            }
        }
        return seen && onBattery && pct <= 5;
    } catch (...) {
        return false;
    }
}

bool rtcTrusted() {
    std::ifstream f("/sys/class/rtc/rtc0/since_epoch");
    long long epoch = 0;
    f >> epoch;
    return epoch >= 1704067200LL;
}

void pwmWrite(const char* p, const char* v) {
    FILE* f = std::fopen(p, "wb");
    if (!f)
        return;
    std::fwrite(v, 1, strlen(v), f);
    std::fclose(f);
}

void pwmPulses(int n, int freq, int durMs, int gapMs) {
    if (access("/sys/class/pwm/pwmchip0/export", W_OK) != 0 &&
        access("/sys/class/pwm/pwmchip0/pwm0/enable", W_OK) != 0)
        return;
    if (access("/sys/class/pwm/pwmchip0/pwm0/enable", F_OK) != 0) {
        pwmWrite("/sys/class/pwm/pwmchip0/export", "0");
        usleep(120000);
    }
    char buf[32];
    int period = 1000000000 / (freq > 0 ? freq : 2000);
    snprintf(buf, sizeof(buf), "%d", period);
    pwmWrite("/sys/class/pwm/pwmchip0/pwm0/period", buf);
    snprintf(buf, sizeof(buf), "%d", period / 2);
    pwmWrite("/sys/class/pwm/pwmchip0/pwm0/duty_cycle", buf);
    for (int i = 0; i < n && !g_stop; ++i) {
        pwmWrite("/sys/class/pwm/pwmchip0/pwm0/enable", "1");
        usleep((useconds_t)(durMs * 1000));
        pwmWrite("/sys/class/pwm/pwmchip0/pwm0/enable", "0");
        if (i + 1 < n)
            usleep((useconds_t)(gapMs * 1000));
    }
}

void playWavDetached(const std::string& wav) {
    if (access(wav.c_str(), R_OK) != 0)
        return;
    pid_t pid = fork();
    if (pid == 0) {
        execlp("aplay", "aplay", "-D", "plughw:0,0", wav.c_str(), (char*)nullptr);
        _exit(127);
    }
}

std::string assetsDir() {
    if (const char* e = std::getenv("NV_ASSETS_DIR"))
        return e;
    for (const char* c : {"/opt/nexusvote-lab/assets/sounds", "assets/sounds"}) {
        if (access(c, F_OK) == 0)
            return c;
    }
    return "assets/sounds";
}

void soundConfirma() {
    playWavDetached(assetsDir() + "/confirma.wav");
    pwmPulses(2, 2000, 120, 180);
}

void soundFim() {
    playWavDetached(assetsDir() + "/fim.wav");
    pwmPulses(3, 2000, 250, 260);
}

struct KeySource {
    FILE* fifo = nullptr;
    nexusvote::keypad::EvdevKeypad evdev;
    bool useEvdev = false;
    bool labStdin = false;
};

bool openInput(const std::string& workdir, KeySource& ks, bool labStdin, std::string& err) {
    ks.labStdin = labStdin;
    if (labStdin)
        return true;
    std::string fifo = workdir + "/hw-keys.fifo";
    int fd = ::open(fifo.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd >= 0) {
        ::close(fd);
        fd = ::open(fifo.c_str(), O_RDONLY);
        if (fd >= 0) {
            ks.fifo = fdopen(fd, "r");
            if (ks.fifo)
                return true;
            ::close(fd);
        }
    }
    std::string e2;
    if (ks.evdev.open(e2)) {
        ks.useEvdev = true;
        return true;
    }
    err = "sem entrada física (fifo " + fifo + " ausente e " + e2 + ")";
    return false;
}

bool readKey(KeySource& ks, nexusvote::keypad::Press& pr, std::string& err) {
    using namespace nexusvote::keypad;
    if (ks.labStdin) {
        std::string line;
        if (!std::getline(std::cin, line)) {
            err = "stdin EOF";
            return false;
        }
        if (line == "B" || line == "b" || line == "A" || line == "a") {
            pr = {true, Key::Branco, 0};
            return true;
        }
        if (line == "C" || line == "c") {
            pr = {true, Key::Corrige, 0};
            return true;
        }
        if (line.empty() || line == "ENTER") {
            pr = {true, Key::Confirma, 0};
            return true;
        }
        if (line == "*") {
            pr = {true, Key::Cancela, 0};
            return true;
        }
        if (line.size() == 1 && isdigit(line[0])) {
            pr = {true, line[0] == '0' ? Key::Digit0 : Key::Digit1, line[0]};
            if (line[0] != '0')
                pr.key = (Key)((int)Key::Digit1 + (line[0] - '1'));
            return true;
        }
        pr = {false, Key::Ignorada, 0};
        return true;
    }
    if (ks.useEvdev)
        return ks.evdev.readPress(pr, err);
    char* raw = nullptr;
    size_t cap = 0;
    ssize_t n = getline(&raw, &cap, ks.fifo);
    if (n <= 0) {
        free(raw);
        err = "fifo EOF (bridge caiu?)";
        return false;
    }
    std::string s(raw, (size_t)n);
    free(raw);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
        s.pop_back();
    if (s == "BRANCO") {
        pr = {true, Key::Branco, 0};
        return true;
    }
    if (s == "CORRIGE") {
        pr = {true, Key::Corrige, 0};
        return true;
    }
    if (s == "CONFIRMA") {
        pr = {true, Key::Confirma, 0};
        return true;
    }
    if (s == "CANCELA") {
        pr = {true, Key::Cancela, 0};
        return true;
    }
    if (s.size() == 1 && isdigit(s[0])) {
        pr = {true, s[0] == '0' ? Key::Digit0 : Key::Digit1, s[0]};
        if (s[0] != '0')
            pr.key = (Key)((int)Key::Digit1 + (s[0] - '1'));
        return true;
    }
    pr = {false, Key::Ignorada, 0};
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, onSig);
    std::signal(SIGTERM, onSig);
    std::cout << "==============================================================\n"
              << "PROTÓTIPO INDEPENDENTE - USO EXCLUSIVO EM LABORATÓRIO - NÃO OFICIAL\n"
              << "NexusVote " << nexusvote::kSoftwareVersion << " urna-pi (Pi 4B).\n"
              << "==============================================================\n";
    if (argc < 2) {
        std::cerr << "uso: urna-pi <workdir> [--lab-stdin]\n";
        return 2;
    }
    std::string workdir = argv[1];
    bool labStdin = (argc >= 3 && std::string(argv[2]) == "--lab-stdin");
    const char* cfg = std::getenv("NEXUSVOTE_CONFIG");
    if (!cfg) {
        std::cerr << "NEXUSVOTE_CONFIG não definido\n";
        return 2;
    }
    bool lab = true;
    nexusvote::VotingSession s(workdir, lab);
    auto lr = s.loadConfig(cfg);
    if (!lr.ok) {
        std::cerr << "ERRO config: " << lr.error << "\n";
        return 1;
    }
    {
        std::ifstream sf(workdir + "/session.json", std::ios::binary);
        if (sf) {
            auto rr = s.resume();
            if (!rr.ok) {
                std::cerr << "ERRO resume: " << rr.error << "\n";
                return 1;
            }
        }
    }
    if (s.state() == nexusvote::SessionState::Ready) {
        auto r = s.open();
        if (!r.ok) {
            std::cerr << "ERRO open: " << r.error << "\n";
            return 1;
        }
        std::cout << r.info << "\n";
    }
    hwLog(workdir, "boot", std::string("urna-pi rtc_trusted=") + (rtcTrusted() ? "1" : "0"));
    if (!rtcTrusted())
        hwLog(workdir, "rtc_untrusted", "tempo do sistema (sem DS3231?)");
    if (labStdin)
        hwLog(workdir, "input_lab_stdin", "MODO DEV: stdin (não é tecla física)");

    {
        std::vector<std::string> linhas;
        for (auto& o : s.config().offices)
            linhas.push_back(o.title + ": 0");
        std::string job =
            nexusvote::paper::buildJob("ZERESIMA", s.sessionId(), linhas, "{\"zeresima\":1}");
        auto pr = nexusvote::paper::printRaw(nexusvote::paper::printerPath(), job);
        hwLog(workdir, pr.ok() ? "print_ok" : "print_fail", "zeresima: " + pr.detail);
        std::cout << (pr.ok() ? "Zerésima impressa.\n"
                              : "Zerésima NÃO impressa (" + pr.detail + ") - segue QR/ata.\n");
    }

    KeySource ks;
    std::string ierr;
    if (!openInput(workdir, ks, labStdin, ierr)) {
        std::cerr << "ERRO: " << ierr << "\n";
        hwLog(workdir, "input_fail", ierr);
        return 1;
    }

    uint64_t eleitores = 0;
    while (!g_stop) {
        if (sysfsCritical()) {
            std::cout << "Energia crítica: fechando sessão com segurança.\n";
            hwLog(workdir, "power_critical", "quiesce: fecha sessão");
            auto rc = s.close("");
            hwLog(workdir, "quiesce_done", rc.ok ? rc.info : rc.error);
            return 2;
        }
        bool eleitorCompleto = true;
        for (const auto& office : s.officeOrder()) {
            std::string digits;
            bool blank = false;
            int seats = 1;
            for (auto& o : s.config().offices)
                if (o.id == office)
                    seats = std::max(1, o.seats);
            for (int seat = 0; seat < seats && !g_stop; ++seat) {
                for (;;) {
                    if (sysfsCritical()) {
                        hwLog(workdir, "power_critical", "quiesce no meio do voto");
                        s.close("");
                        return 2;
                    }
                    auto pv = s.preview(office, digits, blank);
                    std::cout << "\n[" << office
                              << (seats > 1 ? " vaga " + std::to_string(seat + 1) + "/" +
                                                  std::to_string(seats)
                                            : "")
                              << "] " << (blank ? "(BRANCO)" : (digits.empty() ? "-" : digits))
                              << "\n  " << pv.message_pt << "\n> ";
                    std::cout.flush();
                    nexusvote::keypad::Press pr;
                    std::string re;
                    if (!readKey(ks, pr, re)) {
                        std::cerr << "entrada encerrada: " << re << " (voto NÃO confirmado)\n";
                        eleitorCompleto = false;
                        break;
                    }
                    if (!pr.has)
                        continue;
                    using K = nexusvote::keypad::Key;
                    if (pr.key == K::Cancela || pr.key == K::Corrige) {
                        digits.clear();
                        blank = false;
                        continue;
                    }
                    if (pr.key == K::Branco) {
                        blank = true;
                        digits.clear();
                        continue;
                    }
                    if (pr.key >= K::Digit0 && pr.key <= K::Digit9) {
                        if (blank)
                            continue;
                        int lim = 2;
                        for (auto& o : s.config().offices)
                            if (o.id == office)
                                lim = o.number_length;
                        if ((int)digits.size() >= lim)
                            continue;
                        blank = false;
                        digits.push_back(pr.digit);
                        continue;
                    }
                    if (pr.key == K::Confirma) {
                        if (pv.type != nexusvote::PreviewType::Nominal &&
                            pv.type != nexusvote::PreviewType::Legend &&
                            pv.type != nexusvote::PreviewType::Blank &&
                            pv.type != nexusvote::PreviewType::Null) {
                            std::cout << "Nada a confirmar. Digite o número ou BRANCO.\n";
                            continue;
                        }
                        std::string tok = s.beginConfirm(office);
                        auto r = s.confirm(office, digits, blank, tok);
                        if (!r.ok) {
                            std::cerr << "ERRO: " << r.error << " (voto NÃO confirmado)\n";
                            eleitorCompleto = false;
                            break;
                        }
                        digits.clear();
                        blank = false;
                        soundConfirma();
                        std::cout << "Voto registrado.\n----------------------------------------\n";
                        break;
                    }
                }
                if (!eleitorCompleto)
                    break;
            }
            if (!eleitorCompleto)
                break;
        }
        if (!eleitorCompleto)
            break;
        ++eleitores;
        soundFim();
        std::cout << "FIM. Obrigado. (eleitor " << eleitores << ")\n";
        hwLog(workdir, "voter_done", "eleitor " + std::to_string(eleitores));
    }

    std::cout << "Encerrando terminal (eleitores: " << eleitores << ")...\n";
    auto rc = s.close("");
    std::cout << (rc.ok ? rc.info : "ERRO: " + rc.error) << "\n";
    {
        std::vector<std::string> linhas;
        std::string qr = "{\"s\":\"NEXUSVOTE-LAB\",\"f\":\"BU-LAB-v1\",\"sessao\":\"" +
                         s.sessionId() + "\",\"cargos\":{";
        bool first = true;
        for (auto& [oid, t] : s.tally().by_office) {
            linhas.push_back(oid + " TOT:" + std::to_string(t.total) +
                             " B:" + std::to_string(t.blank) + " N:" + std::to_string(t.null));
            if (!first)
                qr += ",";
            first = false;
            qr += "\"" + oid + "\":" + std::to_string(t.total);
        }
        qr += "}}";
        std::string job = nexusvote::paper::buildJob("BU", s.sessionId(), linhas, qr);
        auto pr = nexusvote::paper::printRaw(nexusvote::paper::printerPath(), job);
        hwLog(workdir, pr.ok() ? "print_ok" : "print_fail", "BU: " + pr.detail);
        std::cout << (pr.ok() ? "BU impresso (via 1; conferir legibilidade p/ vias 2-5).\n"
                              : "BU NÃO impresso (" + pr.detail + ") - QR/ata.\n");
    }
    return 0;
}
