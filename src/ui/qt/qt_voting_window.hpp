// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

#include <QMainWindow>

#include "../voting_view.hpp"

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
QT_END_NAMESPACE

namespace nexusvote::ui {

class QtVotingWindow : public QMainWindow, public IVotingView {
    Q_OBJECT
  public:
    using KeyCallback = std::function<void(const KeyEvent&)>;
    explicit QtVotingWindow(QWidget* parent = nullptr);
    void onKey(KeyCallback cb) {
        key_cb_ = std::move(cb);
    }

    void show(const ViewMessages& msg) override;
    void focusConfirm(bool enabled) override;
    void beep() override;
    void beepConfirm() override;
    void beepFim() override;
    void setLargeFont(bool on) override;
    void setHighContrast(bool on) override;

  protected:
    void keyPressEvent(QKeyEvent* ev) override;

  private:
    void emitKey(KeyEvent::Key k, char digit = 0);
    QLabel* banner_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* digits_ = nullptr;
    QLabel* preview_ = nullptr;
    QLabel* status_ = nullptr;
    QPushButton* confirm_btn_ = nullptr;
    KeyCallback key_cb_;
    bool large_font_ = false;
};

} // namespace nexusvote::ui
