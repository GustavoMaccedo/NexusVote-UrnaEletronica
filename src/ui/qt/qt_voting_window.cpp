// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "qt_voting_window.hpp"

#include <QApplication>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace nexusvote::ui {

QtVotingWindow::QtVotingWindow(QWidget* parent) : QMainWindow(parent) {
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    banner_ = new QLabel(QString::fromUtf8(kNonOfficialBanner), central);
    title_ = new QLabel(central);
    digits_ = new QLabel("-", central);
    preview_ = new QLabel(central);
    preview_->setWordWrap(true);
    status_ = new QLabel(central);
    status_->setWordWrap(true);
    QFont big = digits_->font();
    big.setPointSize(28);
    big.setBold(true);
    digits_->setFont(big);
    digits_->setFocusPolicy(Qt::StrongFocus);
    root->addWidget(banner_);
    root->addWidget(title_);
    root->addWidget(digits_);
    root->addWidget(preview_);
    banner_->setAccessibleName("Aviso de laboratorio");
    banner_->setAccessibleDescription(
        "Prototipo independente para laboratorio. Nao oficial, sem validade eleitoral.");
    title_->setAccessibleName("Cargo em votacao");
    digits_->setAccessibleName("Numero digitado");
    digits_->setAccessibleDescription("Digitos inseridos para o cargo atual.");
    preview_->setAccessibleName("Revisao do voto");
    preview_->setAccessibleDescription("Descricao da opcao antes da confirmacao.");
    status_->setAccessibleName("Estado");
    auto* grid = new QGridLayout();
    const char* keys = "123456789";
    for (int i = 0; i < 9; ++i) {
        auto* b = new QPushButton(QString(QChar(keys[i])), central);
        b->setMinimumSize(64, 56);
        b->setAccessibleName(QString("Digito %1").arg(keys[i]));
        b->setAccessibleDescription("Adiciona o digito ao numero do voto.");
        int d = keys[i] - '0';
        connect(b, &QPushButton::clicked, this, [this, d] {
            emitKey(d == 0 ? KeyEvent::Key::Digit0
                           : static_cast<KeyEvent::Key>(static_cast<int>(KeyEvent::Key::Digit1) +
                                                        d - 1),
                    static_cast<char>('0' + d));
        });
        grid->addWidget(b, i / 3, i % 3);
    }
    auto* zero = new QPushButton("0", central);
    zero->setMinimumSize(64, 56);
    zero->setAccessibleName("Digito 0");
    zero->setAccessibleDescription("Adiciona o digito ao numero do voto.");
    connect(zero, &QPushButton::clicked, this, [this] { emitKey(KeyEvent::Key::Digit0, '0'); });
    grid->addWidget(zero, 3, 1);
    root->addLayout(grid);
    auto* row = new QHBoxLayout();
    auto* blank = new QPushButton("BRANCO", central);
    auto* correct = new QPushButton("CORRIGE", central);
    confirm_btn_ = new QPushButton("CONFIRMA", central);
    blank->setAccessibleName("Voto em branco");
    blank->setAccessibleDescription("Marca intencao de voto em branco. Exige confirmacao.");
    correct->setAccessibleName("Corrige");
    correct->setAccessibleDescription("Apaga os digitos antes da confirmacao.");
    confirm_btn_->setAccessibleName("Confirma");
    confirm_btn_->setAccessibleDescription("Registra o voto revisado. Acao unica e irreversivel.");
    confirm_btn_->setEnabled(false);
    connect(blank, &QPushButton::clicked, this, [this] { emitKey(KeyEvent::Key::Blank); });
    connect(correct, &QPushButton::clicked, this, [this] { emitKey(KeyEvent::Key::Correct); });
    connect(confirm_btn_, &QPushButton::clicked, this, [this] { emitKey(KeyEvent::Key::Confirm); });
    row->addWidget(blank);
    row->addWidget(correct);
    row->addWidget(confirm_btn_);
    root->addLayout(row);
    root->addWidget(status_);
    setCentralWidget(central);
    setWindowTitle("NexusVote Lab — NÃO OFICIAL");
    resize(420, 640);
}

void QtVotingWindow::emitKey(KeyEvent::Key k, char digit) {
    if (key_cb_) {
        KeyEvent e;
        e.key = k;
        e.digit = digit;
        key_cb_(e);
    }
}

void QtVotingWindow::show(const ViewMessages& msg) {
    title_->setText(QString::fromStdString(msg.title));
    digits_->setText(
        QString::fromStdString(msg.typed_digits.empty() ? std::string("-") : msg.typed_digits));
    preview_->setText(QString::fromStdString(msg.preview_text));
    status_->setText(QString::fromStdString(msg.status_text));
    confirm_btn_->setEnabled(msg.confirm_enabled);
    if (msg.confirm_enabled)
        confirm_btn_->setFocus();
}

void QtVotingWindow::focusConfirm(bool enabled) {
    confirm_btn_->setEnabled(enabled);
    if (enabled)
        confirm_btn_->setFocus();
}

void QtVotingWindow::beep() {
    QProcess::startDetached("aplay",
                            {"-D", "plughw:0,0", "/opt/nexusvote-lab/assets/sounds/confirma.wav"});
}

namespace {
void pwmWriteQt(const char* p, const QByteArray& v) {
    QFile f(p);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(v);
    }
}
void pwmPulseQt(int n, int freq = 2000, int durMs = 120, int gapMs = 180) {
    if (!QFile::exists("/sys/class/pwm/pwmchip0/pwm0") &&
        !QFile::exists("/sys/class/pwm/pwmchip0/export"))
        return;
    if (!QFile::exists("/sys/class/pwm/pwmchip0/pwm0")) {
        pwmWriteQt("/sys/class/pwm/pwmchip0/export", "0");
    }
    int period = 1000000000 / (freq > 0 ? freq : 2000);
    pwmWriteQt("/sys/class/pwm/pwmchip0/pwm0/period", QByteArray::number(period));
    pwmWriteQt("/sys/class/pwm/pwmchip0/pwm0/duty_cycle", QByteArray::number(period / 2));
    for (int i = 0; i < n; ++i) {
        QTimer::singleShot(i * (durMs + gapMs), [=] {
            pwmWriteQt("/sys/class/pwm/pwmchip0/pwm0/enable", "1");
            QTimer::singleShot(durMs,
                               [] { pwmWriteQt("/sys/class/pwm/pwmchip0/pwm0/enable", "0"); });
        });
    }
}
void hdmiPlayQt(const char* wav) {
    QString w = QString::fromUtf8(wav);
    if (!QFile::exists(w)) {
        w = "assets/sounds/confirma.wav";
        if (!QFile::exists(w))
            return;
    }
    QProcess::startDetached("aplay", {"-D", "plughw:0,0", w});
}
} // namespace

void QtVotingWindow::beepConfirm() {
    hdmiPlayQt("/opt/nexusvote-lab/assets/sounds/confirma.wav");
    pwmPulseQt(2, 2000, 120, 180);
}

void QtVotingWindow::beepFim() {
    QString w = "/opt/nexusvote-lab/assets/sounds/fim.wav";
    if (!QFile::exists(w))
        w = "assets/sounds/fim.wav";
    if (QFile::exists(w))
        QProcess::startDetached("aplay", {"-D", "plughw:0,0", w});
    pwmPulseQt(3, 2000, 250, 260);
}

void QtVotingWindow::keyPressEvent(QKeyEvent* ev) {
    int k = ev->key();
    if (k >= Qt::Key_0 && k <= Qt::Key_9) {
        char d = static_cast<char>('0' + (k - Qt::Key_0));
        emitKey(d == '0' ? KeyEvent::Key::Digit0
                         : static_cast<KeyEvent::Key>(static_cast<int>(KeyEvent::Key::Digit1) +
                                                      (d - '1')),
                d);
        ev->accept();
        return;
    }
    if (k == Qt::Key_B) {
        emitKey(KeyEvent::Key::Blank);
    } else if (k == Qt::Key_Backspace || k == Qt::Key_C) {
        emitKey(KeyEvent::Key::Correct);
    } else if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        emitKey(KeyEvent::Key::Confirm);
    } else if (k == Qt::Key_Escape) {
        emitKey(KeyEvent::Key::Cancel);
    } else {
        QMainWindow::keyPressEvent(ev);
    }
}

void QtVotingWindow::setLargeFont(bool on) {
    large_font_ = on;
    QFont f = font();
    f.setPointSize(on ? 16 : 10);
    setFont(f);
}

void QtVotingWindow::setHighContrast(bool on) {
    if (on) {
        setStyleSheet("QWidget { background: black; color: yellow; font-weight: bold; }"
                      "QPushButton { border: 3px solid yellow; padding: 8px; }"
                      "QPushButton:focus { background: yellow; color: black; }");
    } else {
        setStyleSheet("");
    }
}

} // namespace nexusvote::ui
