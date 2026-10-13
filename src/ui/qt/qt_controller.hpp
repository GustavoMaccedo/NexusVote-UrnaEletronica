// Universidade de Brasilia (UnB) - Gustavo Maccedo - TCC
#pragma once

#include <string>

#include "../voting_view.hpp"
#include "../../core/domain.hpp"

namespace nexusvote::ui {

ViewMessages buildOfficeView(const std::string& office_id, const std::string& office_title,
                             const std::string& digits, bool blank, const BallotPreview& pv);
ViewMessages neutralMessage(const std::string& office_id);

} // namespace nexusvote::ui
