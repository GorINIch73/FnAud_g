/**
 * @file ServiceView.h
 * @brief Представление сервиса
 */

#pragma once

#include "views/BaseView.h"
#include <imgui.h>

namespace FinancialAudit {

class ServiceView : public BaseView {
public:
    ServiceView() { title_ = "Сервис"; }
    void Render() override {
        ImGui::Begin(title_.c_str(), &isOpen_);
        ImGui::Text("Представление сервиса (заглушка)");
        ImGui::End();
    }
};

} // namespace FinancialAudit
