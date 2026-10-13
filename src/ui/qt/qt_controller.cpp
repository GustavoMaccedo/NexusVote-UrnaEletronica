// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC

#include "qt_controller.hpp"

namespace nexusvote::ui {

ViewMessages buildOfficeView(const std::string& office_id, const std::string& office_title,
                             const std::string& digits, bool blank, const BallotPreview& pv) {
    ViewMessages m;
    m.title = "Cargo: " + office_id + " - " + office_title;
    m.typed_digits = blank ? "(BRANCO)" : digits;
    m.preview_text = pv.message_pt;
    m.status_text = "BRANCO=branco C=CORRIGE ENTER=confirma (após revisão)";
    m.confirm_enabled = (pv.type == PreviewType::Nominal || pv.type == PreviewType::Legend ||
                         pv.type == PreviewType::Blank || pv.type == PreviewType::Null);
    return m;
}

ViewMessages neutralMessage(const std::string& office_id) {
    ViewMessages m;
    m.title = "Cargo: " + office_id;
    m.typed_digits = "-";
    m.preview_text = "Voto registrado. Nada da escolha anterior permanece na tela.";
    m.status_text = "Aguardando próximo cargo…";
    m.confirm_enabled = false;
    return m;
}

} // namespace nexusvote::ui
