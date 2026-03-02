/**
 * @file SqlQueryView.h
 * @brief Представление SQL запросов
 */

#pragma once

#include "views/BaseView.h"
#include <imgui.h>

namespace FinancialAudit {

class SqlQueryView : public BaseView {
public:
    SqlQueryView() { title_ = "SQL Запрос"; }
    void Render() override {
        ImGui::Begin(title_.c_str(), &isOpen_);
        ImGui::Text("Представление SQL запросов (заглушка)");
        ImGui::End();
    }
};

} // namespace FinancialAudit
