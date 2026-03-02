/**
 * @file SettingsView.h
 * @brief Представление настроек
 */

#pragma once

#include "views/BaseView.h"
#include <imgui.h>

namespace FinancialAudit {

class SettingsView : public BaseView {
public:
    SettingsView() { title_ = "Настройки"; }
    void Render() override {
        ImGui::Begin(title_.c_str(), &isOpen_);
        ImGui::Text("Представление настроек (заглушка)");
        ImGui::End();
    }
};

} // namespace FinancialAudit
