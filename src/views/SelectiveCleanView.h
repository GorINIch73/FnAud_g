/**
 * @file SelectiveCleanView.h
 * @brief Представление очистки базы
 */

#pragma once

#include "views/BaseView.h"
#include <imgui.h>
#include <string>

namespace FinancialAudit {

class SelectiveCleanView : public BaseView {
public:
    SelectiveCleanView();
    void Render() override;
    
private:
    bool showConfirm_ = false;
    std::string confirmAction_;
    std::string confirmMessage_;
};

} // namespace FinancialAudit
