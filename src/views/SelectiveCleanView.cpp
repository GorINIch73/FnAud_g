/**
 * @file SelectiveCleanView.cpp
 * @brief Реализация представления очистки базы
 */

#include "SelectiveCleanView.h"

#include "managers/DatabaseManager.h"
#include "widgets/CustomWidgets.h"

#include <iostream>

#define ICON_FA_TRASH "\xEF\x87\xB8"

namespace FinancialAudit {

SelectiveCleanView::SelectiveCleanView() {
    title_ = "Очистка базы";
}

void SelectiveCleanView::Render() {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    
    if (!ImGui::Begin(title_.c_str(), &isOpen_)) {
        ImGui::End();
        return;
    }
    
    ImGui::Text("Выберите данные для очистки:");
    ImGui::Separator();
    
    // Кнопки очистки
    if (ImGui::Button(ICON_FA_TRASH " Очистить платежи", ImVec2(200, 0))) {
        showConfirm_ = true;
        confirmAction_ = "clear_payments";
        confirmMessage_ = "Вы действительно хотите удалить ВСЕ платежи?";
    }
    
    if (ImGui::Button(ICON_FA_TRASH " Очистить контрагентов", ImVec2(200, 0))) {
        showConfirm_ = true;
        confirmAction_ = "clear_counterparties";
        confirmMessage_ = "Вы действительно хотите удалить ВСЕХ контрагентов?";
    }
    
    if (ImGui::Button(ICON_FA_TRASH " Очистить договоры", ImVec2(200, 0))) {
        showConfirm_ = true;
        confirmAction_ = "clear_contracts";
        confirmMessage_ = "Вы действительно хотите удалить ВСЕ договоры?";
    }
    
    if (ImGui::Button(ICON_FA_TRASH " Очистить накладные", ImVec2(200, 0))) {
        showConfirm_ = true;
        confirmAction_ = "clear_invoices";
        confirmMessage_ = "Вы действительно хотите удалить ВСЕ накладные?";
    }
    
    if (ImGui::Button(ICON_FA_TRASH " Очистить сироты", ImVec2(200, 0))) {
        showConfirm_ = true;
        confirmAction_ = "clear_orphans";
        confirmMessage_ = "Вы действительно хотите удалить детали платежей без родительских записей?";
    }
    
    if (ImGui::Button(ICON_FA_TRASH " Очистить недавние файлы", ImVec2(200, 0))) {
        showConfirm_ = true;
        confirmAction_ = "clear_recent";
        confirmMessage_ = "Вы действительно хотите очистить список недавних файлов?";
    }
    
    ImGui::Separator();
    
    if (ImGui::Button("Закрыть")) {
        isOpen_ = false;
    }
    
    // Модальное окно подтверждения
    if (showConfirm_) {
        ImGui::OpenPopup("Подтверждение очистки");
    }
    
    if (ImGui::BeginPopupModal("Подтверждение очистки", &showConfirm_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", confirmMessage_.c_str());
        ImGui::Separator();
        
        if (ImGui::Button("Очистить", ImVec2(100, 0))) {
            if (dbManager_) {
                bool success = false;
                
                if (confirmAction_ == "clear_payments") {
                    success = dbManager_->clearPayments();
                } else if (confirmAction_ == "clear_counterparties") {
                    success = dbManager_->clearCounterparties();
                } else if (confirmAction_ == "clear_contracts") {
                    success = dbManager_->clearContracts();
                } else if (confirmAction_ == "clear_invoices") {
                    success = dbManager_->clearInvoices();
                } else if (confirmAction_ == "clear_orphans") {
                    success = dbManager_->clearOrphanedDetails();
                } else if (confirmAction_ == "clear_recent") {
                    success = dbManager_->clearRecentFiles();
                }
                
                if (success) {
                    std::cout << "Очистка выполнена успешно" << std::endl;
                } else {
                    std::cerr << "Ошибка очистки: " << dbManager_->getLastError() << std::endl;
                }
            }
            showConfirm_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена", ImVec2(100, 0))) {
            showConfirm_ = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    ImGui::End();
}

} // namespace FinancialAudit
