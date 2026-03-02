/**
 * @file PaymentsView.cpp
 * @brief Реализация представления платежей
 */

#include "PaymentsView.h"

#include <imgui_internal.h>
#include <algorithm>

#include "managers/DatabaseManager.h"
#include "managers/UIManager.h"
#include "managers/PdfReporter.h"
#include "widgets/CustomWidgets.h"

#define ICON_FA_PLUS "\xEF\x81\xA7"
#define ICON_FA_EDIT "\xEF\x81\x84"
#define ICON_FA_TRASH "\xEF\x87\xB8"
#define ICON_FA_SAVE "\xEF\x83\x87"
#define ICON_FA_SEARCH "\xEF\x80\x82"
#define ICON_FA_FILE_CSV "\xEF\x96\xB3"
#define ICON_FA_FILE_PDF "\xEF\x87\x81"
#define ICON_FA_USERS "\xEF\x83\x80"
#define ICON_FA_CARET_DOWN "\xEF\x83\x97"
#define ICON_FA_CARET_RIGHT "\xEF\x83\x9A"

namespace FinancialAudit {

PaymentsView::PaymentsView() {
    title_ = "Платежи";
}

void PaymentsView::Render() {
    ImGui::SetNextWindowSize(ImVec2(1200, 700), ImGuiCond_FirstUseEver);
    
    if (!ImGui::Begin(title_.c_str(), &isOpen_)) {
        ImGui::End();
        return;
    }
    
    if (needRefresh_) {
        refreshData();
    }
    
    renderFilters();
    ImGui::Separator();
    renderToolbar();
    ImGui::Separator();
    renderTable();
    
    // Диалоги
    if (isCreating_) {
        ImGui::OpenPopup("Создать платёж");
    }
    
    if (isEditing_) {
        ImGui::OpenPopup("Редактировать платёж");
    }
    
    if (showDeleteConfirm_) {
        ImGui::OpenPopup("Подтверждение удаления");
    }
    
    if (showGroupOperation_) {
        ImGui::OpenPopup("Групповая операция");
    }
    
    // Модальное окно создания
    if (ImGui::BeginPopupModal("Создать платёж", &isCreating_, ImGuiWindowFlags_AlwaysAutoResize)) {
        char dateBuffer[32] = "";
        char docNumberBuffer[64] = "";
        char recipientBuffer[256] = "";
        char descriptionBuffer[512] = "";
        char noteBuffer[512] = "";
        double amount = 0.0;
        int type = 0;
        
        strncpy(dateBuffer, currentItem_.date.c_str(), sizeof(dateBuffer) - 1);
        strncpy(docNumberBuffer, currentItem_.docNumber.c_str(), sizeof(docNumberBuffer) - 1);
        strncpy(recipientBuffer, currentItem_.recipient.c_str(), sizeof(recipientBuffer) - 1);
        strncpy(descriptionBuffer, currentItem_.description.c_str(), sizeof(descriptionBuffer) - 1);
        strncpy(noteBuffer, currentItem_.note.c_str(), sizeof(noteBuffer) - 1);
        amount = currentItem_.amount;
        type = currentItem_.type;

        ImGui::InputText("Дата*", dateBuffer, sizeof(dateBuffer));
        ImGui::InputText("Номер документа", docNumberBuffer, sizeof(docNumberBuffer));
        ImGui::InputText("Получатель/Отправитель*", recipientBuffer, sizeof(recipientBuffer));
        ImGui::InputTextMultiline("Описание", descriptionBuffer, sizeof(descriptionBuffer), ImVec2(400, 80));
        
        const char* types[] = {"Расход", "Доход"};
        ImGui::Combo("Тип*", &type, types, 2);
        
        CustomWidgets::AmountInput("Сумма*", amount);
        ImGui::InputTextMultiline("Примечание", noteBuffer, sizeof(noteBuffer), ImVec2(400, 60));
        
        ImGui::Separator();
        
        if (ImGui::Button("Создать", ImVec2(100, 0))) {
            if (strlen(dateBuffer) > 0 && strlen(recipientBuffer) > 0 && amount > 0) {
                Payment newPayment;
                newPayment.date = dateBuffer;
                newPayment.docNumber = docNumberBuffer;
                newPayment.recipient = recipientBuffer;
                newPayment.description = descriptionBuffer;
                newPayment.type = type;
                newPayment.amount = amount;
                newPayment.note = noteBuffer;
                
                if (dbManager_ && dbManager_->createPayment(newPayment)) {
                    needRefresh_ = true;
                    isCreating_ = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
        }
        
        if (ImGui::Button("Отмена", ImVec2(100, 0))) {
            isCreating_ = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Модальное окно редактирования
    if (ImGui::BeginPopupModal("Редактировать платёж", &isEditing_, ImGuiWindowFlags_AlwaysAutoResize)) {
        char dateBuffer[32] = "";
        char docNumberBuffer[64] = "";
        char recipientBuffer[256] = "";
        char descriptionBuffer[512] = "";
        char noteBuffer[512] = "";
        double amount = currentItem_.amount;
        int type = currentItem_.type;
        
        strncpy(dateBuffer, currentItem_.date.c_str(), sizeof(dateBuffer) - 1);
        strncpy(docNumberBuffer, currentItem_.docNumber.c_str(), sizeof(docNumberBuffer) - 1);
        strncpy(recipientBuffer, currentItem_.recipient.c_str(), sizeof(recipientBuffer) - 1);
        strncpy(descriptionBuffer, currentItem_.description.c_str(), sizeof(descriptionBuffer) - 1);
        strncpy(noteBuffer, currentItem_.note.c_str(), sizeof(noteBuffer) - 1);

        ImGui::InputText("Дата*", dateBuffer, sizeof(dateBuffer));
        ImGui::InputText("Номер документа", docNumberBuffer, sizeof(docNumberBuffer));
        ImGui::InputText("Получатель/Отправитель*", recipientBuffer, sizeof(recipientBuffer));
        ImGui::InputTextMultiline("Описание", descriptionBuffer, sizeof(descriptionBuffer), ImVec2(400, 80));
        
        const char* types[] = {"Расход", "Доход"};
        ImGui::Combo("Тип*", &type, types, 2);
        
        CustomWidgets::AmountInput("Сумма*", amount);
        ImGui::InputTextMultiline("Примечание", noteBuffer, sizeof(noteBuffer), ImVec2(400, 60));
        
        ImGui::Separator();
        
        if (ImGui::Button("Сохранить", ImVec2(100, 0))) {
            if (strlen(dateBuffer) > 0 && strlen(recipientBuffer) > 0 && amount > 0) {
                currentItem_.date = dateBuffer;
                currentItem_.docNumber = docNumberBuffer;
                currentItem_.recipient = recipientBuffer;
                currentItem_.description = descriptionBuffer;
                currentItem_.type = type;
                currentItem_.amount = amount;
                currentItem_.note = noteBuffer;
                
                if (dbManager_ && dbManager_->updatePayment(currentItem_)) {
                    needRefresh_ = true;
                    isEditing_ = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
        }
        
        if (ImGui::Button("Отмена", ImVec2(100, 0))) {
            isEditing_ = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Модальное окно подтверждения удаления
    if (ImGui::BeginPopupModal("Подтверждение удаления", &showDeleteConfirm_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Вы действительно хотите удалить этот платёж?");
        ImGui::Separator();
        
        if (ImGui::Button("Удалить", ImVec2(100, 0))) {
            if (dbManager_ && dbManager_->deletePayment(deleteItemId_)) {
                needRefresh_ = true;
                showDeleteConfirm_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
        }
        
        if (ImGui::Button("Отмена", ImVec2(100, 0))) {
            showDeleteConfirm_ = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Модальное окно групповой операции
    if (ImGui::BeginPopupModal("Групповая операция", &showGroupOperation_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Выбрано платежей: %zu", selectedIds_.size());
        ImGui::Separator();
        
        static char updateField[64] = "";
        static char updateValue[256] = "";
        
        ImGui::InputText("Поле", updateField, sizeof(updateField));
        ImGui::InputText("Значение", updateValue, sizeof(updateValue));
        
        if (ImGui::Button("Обновить выбранные")) {
            if (dbManager_ && strlen(updateField) > 0) {
                std::map<std::string, std::string> fields;
                fields[updateField] = updateValue;
                
                std::vector<int> ids(selectedIds_.begin(), selectedIds_.end());
                dbManager_->bulkUpdatePayments(ids, fields);
                
                needRefresh_ = true;
                showGroupOperation_ = false;
                clearSelection();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Удалить выбранные")) {
            if (dbManager_) {
                std::vector<int> ids(selectedIds_.begin(), selectedIds_.end());
                dbManager_->bulkDeletePayments(ids);
                
                needRefresh_ = true;
                showGroupOperation_ = false;
                clearSelection();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Отмена", ImVec2(100, 0))) {
            showGroupOperation_ = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    ImGui::End();
}

void PaymentsView::refreshData() {
    if (!dbManager_) return;
    
    if (filterText_.empty()) {
        payments_ = dbManager_->getAllPayments();
    } else {
        payments_ = dbManager_->searchPayments(filterText_);
    }
    
    // Применяем фильтры
    std::vector<Payment> filtered;
    
    for (const auto& p : payments_) {
        // Фильтр по дате
        if (!filterDateFrom_.empty() && p.date < filterDateFrom_) continue;
        if (!filterDateTo_.empty() && p.date > filterDateTo_) continue;
        
        // Фильтр по типу
        if (filterType_ >= 0 && p.type != filterType_) continue;
        
        // Фильтр по сумме
        if (!filterAmountFrom_.empty()) {
            try {
                double from = std::stod(filterAmountFrom_);
                if (p.amount < from) continue;
            } catch (...) {}
        }
        if (!filterAmountTo_.empty()) {
            try {
                double to = std::stod(filterAmountTo_);
                if (p.amount > to) continue;
            } catch (...) {}
        }
        
        filtered.push_back(p);
    }
    
    payments_ = filtered;
    sortData();
    needRefresh_ = false;
}

void PaymentsView::renderFilters() {
    if (ImGui::CollapsingHeader("Фильтры", ImGuiTreeNodeFlags_DefaultOpen)) {
        char filterBuffer[256] = "";
        strncpy(filterBuffer, filterText_.c_str(), sizeof(filterBuffer) - 1);
        if (ImGui::InputText("Поиск", filterBuffer, sizeof(filterBuffer))) {
            filterText_ = filterBuffer;
        }
        ImGui::SameLine();

        char dateFromBuffer[32] = "";
        strncpy(dateFromBuffer, filterDateFrom_.c_str(), sizeof(dateFromBuffer) - 1);
        if (ImGui::InputText("Дата с", dateFromBuffer, sizeof(dateFromBuffer))) {
            filterDateFrom_ = dateFromBuffer;
        }
        ImGui::SameLine();

        char dateToBuffer[32] = "";
        strncpy(dateToBuffer, filterDateTo_.c_str(), sizeof(dateToBuffer) - 1);
        if (ImGui::InputText("Дата по", dateToBuffer, sizeof(dateToBuffer))) {
            filterDateTo_ = dateToBuffer;
        }
        ImGui::SameLine();

        const char* types[] = {"Все", "Расход", "Доход"};
        ImGui::Combo("Тип", &filterType_, types, 3);
        ImGui::SameLine();

        char amountFromBuffer[32] = "";
        strncpy(amountFromBuffer, filterAmountFrom_.c_str(), sizeof(amountFromBuffer) - 1);
        if (ImGui::InputText("Сумма от", amountFromBuffer, sizeof(amountFromBuffer))) {
            filterAmountFrom_ = amountFromBuffer;
        }
        ImGui::SameLine();

        char amountToBuffer[32] = "";
        strncpy(amountToBuffer, filterAmountTo_.c_str(), sizeof(amountToBuffer) - 1);
        if (ImGui::InputText("Сумма до", amountToBuffer, sizeof(amountToBuffer))) {
            filterAmountTo_ = amountToBuffer;
        }
        ImGui::SameLine();

        if (ImGui::Button("Применить")) {
            needRefresh_ = true;
        }
        ImGui::SameLine();

        if (ImGui::Button("Сбросить")) {
            filterText_.clear();
            filterDateFrom_.clear();
            filterDateTo_.clear();
            filterAmountFrom_.clear();
            filterAmountTo_.clear();
            filterType_ = -1;
            needRefresh_ = true;
        }
    }
}

void PaymentsView::renderToolbar() {
    // Кнопка создания
    if (CustomWidgets::IconButton(ICON_FA_PLUS " Создать", "Создать новый платёж")) {
        currentItem_ = Payment{};
        isCreating_ = true;
    }
    ImGui::SameLine();
    
    // Кнопка обновления
    if (CustomWidgets::IconButton(ICON_FA_SEARCH, "Обновить")) {
        needRefresh_ = true;
    }
    ImGui::SameLine();
    
    // Групповые операции
    if (!selectedIds_.empty()) {
        if (CustomWidgets::IconButton(ICON_FA_USERS " Групповая операция", "Групповые операции")) {
            showGroupOperation_ = true;
        }
        ImGui::SameLine();
        ImGui::Text("Выбрано: %zu", selectedIds_.size());
    }
    ImGui::SameLine();
    
    // Экспорт в PDF
    if (CustomWidgets::IconButton(ICON_FA_FILE_PDF, "Экспорт в PDF")) {
        if (pdfReporter_) {
            pdfReporter_->generatePaymentsReport("payments_report.pdf", filterText_);
        }
    }
    ImGui::SameLine();
    
    // Выделить все
    if (ImGui::Button("Выделить все")) {
        selectAll();
    }
    ImGui::SameLine();
    
    // Снять выделение
    if (ImGui::Button("Снять выделение")) {
        clearSelection();
    }
    
    ImGui::SameLine();
    ImGui::Text("Всего: %zu", payments_.size());
}

void PaymentsView::renderTable() {
    if (ImGui::BeginTable("PaymentsTable", 8, ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
                                              ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders |
                                              ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30);  // Чекбокс
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 25);  // Развёрнуть
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Дата", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Документ", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Тип", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Сумма", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Получатель", ImGuiTableColumnFlags_WidthStretch, 0);
        
        ImGui::TableSetupScrollFreeze(0, 2);
        ImGui::TableHeadersRow();
        
        // Обработка сортировки
        if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
            if (sortSpecs->SpecsDirty) {
                sortColumn_ = sortSpecs->Specs->ColumnIndex;
                sortAscending_ = sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending;
                sortData();
                sortSpecs->SpecsDirty = false;
            }
        }
        
        for (const auto& payment : payments_) {
            ImGui::TableNextRow();
            
            // Чекбокс выделения
            ImGui::TableSetColumnIndex(0);
            bool isSelected = selectedIds_.count(payment.id) > 0;
            if (ImGui::Checkbox("", &isSelected)) {
                toggleSelection(payment.id);
            }
            
            // Кнопка развёртывания
            ImGui::TableSetColumnIndex(1);
            bool isExpanded = expandedPaymentIds_.count(payment.id) > 0;
            if (ImGui::Button(isExpanded ? ICON_FA_CARET_DOWN : ICON_FA_CARET_RIGHT)) {
                if (isExpanded) {
                    expandedPaymentIds_.erase(payment.id);
                } else {
                    expandedPaymentIds_.insert(payment.id);
                }
            }
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d", payment.id);
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", payment.date.c_str());
            
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%s", payment.docNumber.c_str());
            
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%s", payment.type == 0 ? "Расход" : "Доход");
            
            ImGui::TableSetColumnIndex(6);
            ImGui::Text("%.2f", payment.amount);
            
            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%s", payment.recipient.c_str());
            
            // Контекстное меню
            if (ImGui::BeginPopupContextItem("PaymentContext")) {
                if (ImGui::MenuItem("Редактировать")) {
                    currentItem_ = payment;
                    isEditing_ = true;
                }
                if (ImGui::MenuItem("Удалить")) {
                    deleteItemId_ = payment.id;
                    showDeleteConfirm_ = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Выделить")) {
                    toggleSelection(payment.id);
                }
                ImGui::EndPopup();
            }
            
            // Развёрнутые детали
            if (isExpanded) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Indent(20);
                ImGui::Text("Описание: %s", payment.description.c_str());
                ImGui::Text("Примечание: %s", payment.note.c_str());
                
                // Детали платежа
                if (dbManager_) {
                    auto details = dbManager_->getPaymentDetails(payment.id);
                    if (!details.empty()) {
                        ImGui::Text("Детали:");
                        for (const auto& d : details) {
                            ImGui::Indent(20);
                            ImGui::Text("- КОСГУ: %d, Договор: %d, Накладная: %d, Сумма: %.2f",
                                       d.kosguId.value_or(0),
                                       d.contractId.value_or(0),
                                       d.invoiceId.value_or(0),
                                       d.amount);
                            ImGui::Unindent(20);
                        }
                    }
                }
                
                ImGui::Unindent(20);
            }
        }
        
        ImGui::EndTable();
    }
}

void PaymentsView::sortData() {
    if (sortColumn_ < 0) return;
    
    std::sort(payments_.begin(), payments_.end(), [this](const Payment& a, const Payment& b) {
        bool result = false;
        
        switch (sortColumn_) {
            case 2: result = a.id < b.id; break;
            case 3: result = a.date < b.date; break;
            case 4: result = a.docNumber < b.docNumber; break;
            case 5: result = a.type < b.type; break;
            case 6: result = a.amount < b.amount; break;
            case 7: result = a.recipient < b.recipient; break;
        }
        
        return sortAscending_ ? result : !result;
    });
}

void PaymentsView::toggleSelection(int id) {
    if (selectedIds_.count(id) > 0) {
        selectedIds_.erase(id);
    } else {
        selectedIds_.insert(id);
    }
}

void PaymentsView::selectAll() {
    for (const auto& p : payments_) {
        selectedIds_.insert(p.id);
    }
}

void PaymentsView::clearSelection() {
    selectedIds_.clear();
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> PaymentsView::GetDataAsStrings() {
    std::vector<std::string> columns = {"ID", "Date", "DocNumber", "Type", "Amount", "Recipient", "Description", "Note"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& p : payments_) {
        rows.push_back({
            std::to_string(p.id),
            p.date,
            p.docNumber,
            std::to_string(p.type),
            std::to_string(p.amount),
            p.recipient,
            p.description,
            p.note
        });
    }
    
    return {columns, rows};
}

} // namespace FinancialAudit
