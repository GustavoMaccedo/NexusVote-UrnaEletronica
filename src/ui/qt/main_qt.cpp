// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
// Uso (laboratório): nexusvote-qt <config.json> <workdir> [outdir]

#include <QApplication>
#include <QEventLoop>
#include <QTimer>

#include <cstdlib>
#include <iostream>

#include "../../core/config_loader.hpp"
#include "../../core/session.hpp"
#include "qt_controller.hpp"
#include "qt_voting_window.hpp"

namespace {

bool confirmable(nexusvote::PreviewType t) {
    return t == nexusvote::PreviewType::Nominal || t == nexusvote::PreviewType::Legend ||
           t == nexusvote::PreviewType::Blank || t == nexusvote::PreviewType::Null;
}

int runOffice(nexusvote::VotingSession& session, nexusvote::ui::QtVotingWindow& win,
              const std::string& office_id) {
    std::string title = office_id;
    for (const auto& o : session.config().offices) {
        if (o.id == office_id) {
            title = o.title;
            break;
        }
    }
    std::string digits;
    bool blank = false;
    QEventLoop loop;
    bool done = false;
    bool ok = false;
    auto refresh = [&] {
        nexusvote::BallotPreview pv = session.preview(office_id, digits, blank);
        win.show(nexusvote::ui::buildOfficeView(office_id, title, digits, blank, pv));
    };
    win.onKey([&](const nexusvote::ui::KeyEvent& e) {
        using K = nexusvote::ui::KeyEvent::Key;
        if (done)
            return;
        switch (e.key) {
        case K::Blank:
            blank = true;
            digits.clear();
            break;
        case K::Correct:
            blank = false;
            digits.clear();
            break;
        case K::Confirm: {
            nexusvote::BallotPreview pv = session.preview(office_id, digits, blank);
            if (!confirmable(pv.type)) {
                refresh();
                return;
            }
            std::string tok = session.beginConfirm(office_id);
            nexusvote::OpResult r = session.confirm(office_id, digits, blank, tok);
            if (!r.ok) {
                nexusvote::ui::ViewMessages m;
                m.title = "ERRO";
                m.preview_text = r.error + " (voto NÃO confirmado)";
                m.confirm_enabled = false;
                win.show(m);
                return;
            }
            win.show(nexusvote::ui::neutralMessage(office_id));
            win.beepConfirm();
            ok = true;
            done = true;
            loop.quit();
            return;
        }
        default:
            if (e.digit >= '0' && e.digit <= '9') {
                blank = false;
                digits.push_back(e.digit);
            }
            break;
        }
        refresh();
    });
    refresh();
    win.showMaximized();
    loop.exec();
    return ok ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "PROTÓTIPO INDEPENDENTE — USO EXCLUSIVO EM LABORATÓRIO — NÃO OFICIAL\n"
                  << "uso: nexusvote-qt <config.json> <workdir> [outdir]\n";
        return 2;
    }
    QApplication app(argc, argv);
    std::string config_path = argv[1];
    std::string workdir = argv[2];
    std::string outdir = (argc >= 4) ? argv[3] : (workdir + "/qt-export");
    nexusvote::VotingSession session(workdir, true);
    nexusvote::OpResult lr = session.loadConfig(config_path);
    if (!lr.ok) {
        std::cerr << "ERRO: " << lr.error << "\n";
        return 1;
    }
    {
        nexusvote::OpResult rr = session.open();
        if (!rr.ok) {
            std::cerr << "ERRO: " << rr.error << "\n";
            return 1;
        }
    }
    nexusvote::ui::QtVotingWindow win;
    if (std::getenv("NEXUSVOTE_LARGE_FONT") != nullptr)
        win.setLargeFont(true);
    if (std::getenv("NEXUSVOTE_HIGH_CONTRAST") != nullptr)
        win.setHighContrast(true);
    int rc = 0;
    for (const auto& office : session.officeOrder()) {
        rc = runOffice(session, win, office);
        if (rc != 0)
            break;
    }
    if (rc == 0) {
        nexusvote::OpResult cr = session.close("");
        if (!cr.ok) {
            std::cerr << "ERRO: " << cr.error << "\n";
            return 1;
        }
        win.beepFim();
        nexusvote::OpResult er = session.exportAll(outdir);
        std::cout << (er.ok ? er.info : std::string("ERRO: ") + er.error) << "\n";
        return er.ok ? 0 : 1;
    }
    std::cerr << "votação interrompida; nada além do confirmado foi registrado\n";
    return rc;
}
