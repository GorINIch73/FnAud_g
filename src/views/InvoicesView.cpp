/**
 * @file InvoicesView.cpp
 * @brief Реализация представления накладных
 */

#include "InvoicesView.h"

#include <imgui_internal.h>
#include <algorithm>
#include <cstring>

#include "managers/DatabaseManager.h"
#include "managers/UIManager.h"
#include "managers/ExportManager.h"
#include "widgets/CustomWidgets.h"

#define ICON_FA_PLUS "\xEF\x81\xA7"
#define ICON_FA_EDIT "\xEF\x81\x84"
#define ICON_FA_TRASH "\xEF\x87\xB8"
#define ICON_FA_FILE_CSV "\xEF\x96\xB3"
#define ICON_FA_LINK "\xEF\x83\x81"

namespace FinancialAudit {

InvoicesView::InvoicesView() {
    title_ = "Накладные";
}

InvoicesView::~InvoicesView() = default;

void InvoicesView::refreshData() {
    if (!dbManager_) return;
    invoices_ = dbManager_->getAllInvoices();
    filteredInvoices_ = invoices_;
    applyFilter();
    needRefresh_ = false;
}

void InvoicesView::applyFilter() {
    filteredInvoices_.clear();
    for (const auto& inv : invoices_) {
        bool matches = true;
        if (!filterNumber_.empty() && inv.number.find(filterNumber_) == std::string::npos) {
            matches = false;
        }
        if (filterContractId_ >= 0 && inv.contractId.value_or(-1) != filterContractId_) {
            matches = false;
        }
        if (matches) {
            filteredInvoices_.push_back(inv);
        }
    }
}

void InvoicesView::sortData() {
    if (sortColumn_ < 0) return;
    
    std::sort(filteredInvoices_.begin(), filteredInvoices_.end(),
        [this](const Invoice& a, const Invoice& b) {
            bool result = false;
            switch (sortColumn_) {
                case 0: result = a.number < b.number; break;
                case 1: result = a.date < b.date; break;
                case 2: result = a.totalAmount < b.totalAmount; break;
            }
            return sortAscending_ ? result : !result;
        });
}

void InvoicesView::renderFilters() {
    ImGui::Text("Фильтры:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputText("##FilterNumber", &filterNumber_[0], filterNumber_.capacity() + 1)) {
        filterNumber_.resize(std::strlen(filterNumber_.c_str()));
        applyFilter();
    }
    ImGui::SameLine();
    
    // Фильтр по договору
    ImGui::SetNextItemWidth(200);
    if (ImGui::BeginCombo("##ContractFilter", filterContractId_ >= 0 ? "Договор выбран" : "Все договоры")) {
        if (ImGui::Selectable("Все договоры", filterContractId_ < 0)) {
            filterContractId_ = -1;
            applyFilter();
        }
        if (dbManager_) {
            auto contracts = dbManager_->getAllContracts();
            for (const auto& c : contracts) {
                char label[256];
                snprintf(label, sizeof(label), "%s от %s", c.number.c_str(), c.date.c_str());
                bool isSelected = (filterContractId_ == c.id);
                if (ImGui::Selectable(label, isSelected)) {
                    filterContractId_ = c.id;
                    applyFilter();
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Очистить")) {
        clearFilters();
    }
}

void InvoicesView::renderToolbar() {
    if (ImGui::Button(ICON_FA_PLUS " Создать")) {
        currentItem_ = Invoice{};
        editingId_ = -1;
        isCreating_ = true;
    }
    ImGui::SameLine();
    
    if (ImGui::Button(ICON_FA_FILE_CSV " Экспорт CSV")) {
        auto data = GetDataAsStrings();
        ExportManager exportMgr(dbManager_);
        exportMgr.exportDataToCsv("invoices_export.csv", data.first, data.second);
    }
    
    ImGui::SameLine();
    ImGui::Text("Всего: %zu", filteredInvoices_.size());
}

void InvoicesView::renderTable() {
    if (ImGui::BeginTable("InvoicesTable", 5, 
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::TableSetupColumn("Номер", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Дата", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Договор", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Сумма", ImGuiTableColumnFlags_WidthFixed, 100);
        
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        
        if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
            if (sortSpecs->SpecsDirty) {
                sortColumn_ = sortSpecs->Specs->ColumnIndex;
                sortAscending_ = sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending;
                sortData();
                sortSpecs->SpecsDirty = false;
            }
        }
        
        // Получаем список договоров для отображения
        std::vector<Contract> contracts;
        if (dbManager_) {
            contracts = dbManager_->getAllContracts();
        }
        
        for (const auto& inv : filteredInvoices_) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            bool isSelected = selectedIds_.count(inv.id) > 0;
            if (ImGui::Checkbox("", &isSelected)) {
                toggleSelection(inv.id);
            }
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", inv.number.c_str());
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", inv.date.c_str());
            
            ImGui::TableSetColumnIndex(3);
            if (inv.contractId.has_value()) {
                // Ищем договор по ID
                std::string contractStr = "-";
                for (const auto& c : contracts) {
                    if (c.id == inv.contractId.value()) {
                        char buf[256];
                        snprintf(buf, sizeof(buf), "%s от %s", c.number.c_str(), c.date.c_str());
                        contractStr = buf;
                        break;
                    }
                }
                ImGui::Text("%s", contractStr.c_str());
            } else {
                ImGui::Text("-");
            }
            
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.2f", inv.totalAmount);
            
            if (ImGui::BeginPopupContextItem("InvoiceContext")) {
                if (ImGui::MenuItem("Редактировать")) {
                    currentItem_ = inv;
                    editingId_ = inv.id;
                    isEditing_ = true;
                }
                if (ImGui::MenuItem("Удалить")) {
                    deleteItemId_ = inv.id;
                    showDeleteConfirm_ = true;
                }
                ImGui::EndPopup();
            }
        }
        
        ImGui::EndTable();
    }
}

void InvoicesView::Render() {
    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(title_.c_str(), &isOpen_)) {
        ImGui::End();
        return;
    }

    if (needRefresh_ && dbManager_ && dbManager_->isDatabaseOpen()) {
        refreshData();
    }

    renderFilters();
    ImGui::Separator();
    renderToolbar();
    ImGui::Separator();
    renderTable();

    // Модальные окна
    if (isCreating_) ImGui::OpenPopup("Создать накладную");
    if (isEditing_) ImGui::OpenPopup("Редактировать накладную");
    if (showDeleteConfirm_) ImGui::OpenPopup("Подтверждение удаления");

    // Создание
    if (ImGui::BeginPopupModal("Создать накладную", &isCreating_, ImGuiWindowFlags_AlwaysAutoResize)) {
        char numberBuffer[64] = "";
        char dateBuffer[32] = "";
        double amount = 0.0;
        int selectedContractId = -1;

        strncpy(numberBuffer, currentItem_.number.c_str(), sizeof(numberBuffer) - 1);
        strncpy(dateBuffer, currentItem_.date.c_str(), sizeof(dateBuffer) - 1);
        amount = currentItem_.totalAmount;
        if (currentItem_.contractId.has_value()) {
            selectedContractId = currentItem_.contractId.value();
        }

        ImGui::InputText("Номер*", numberBuffer, sizeof(numberBuffer));
        ImGui::InputText("Дата*", dateBuffer, sizeof(dateBuffer));
        CustomWidgets::AmountInput("Сумма", amount);
        
        // Выбор договора
        ImGui::Text("Договор:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(250);
        if (dbManager_) {
            auto contracts = dbManager_->getAllContracts();
            std::vector<const char*> contractItems;
            std::vector<int> contractIds;
            contractItems.push_back("Без договора");
            contractIds.push_back(-1);
            
            for (const auto& c : contracts) {
                char* label = new char[256];
                snprintf(label, 256, "%s от %s", c.number.c_str(), c.date.c_str());
                contractItems.push_back(label);
                contractIds.push_back(c.id);
                if (c.id == selectedContractId) {
                    selectedContractId = static_cast<int>(contractItems.size() - 1);
                }
            }
            
            if (ImGui::Combo("##Contract", &selectedContractId, contractItems.data(), contractItems.size())) {
                // Выбран новый договор
            }
            
            // Освобождаем память
            for (size_t i = 1; i < contractItems.size(); ++i) {
                delete[] contractItems[i];
            }
        }

        ImGui::Separator();

        if (ImGui::Button("Создать", ImVec2(100, 0))) {
            if (strlen(numberBuffer) > 0 && strlen(dateBuffer) > 0) {
                Invoice invoice;
                invoice.number = numberBuffer;
                invoice.date = dateBuffer;
                invoice.totalAmount = amount;
                if (selectedContractId > 0 && dbManager_) {
                    auto contracts = dbManager_->getAllContracts();
                    int idx = 0;
                    for (const auto& c : contracts) {
                        if (idx == selectedContractId - 1) {
                            invoice.contractId = c.id;
                            break;
                        }
                        idx++;
                    }
                }
                
                if (dbManager_->createInvoice(invoice)) {
                    needRefresh_ = true;
                    isCreating_ = false;
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена", ImVec2(100, 0))) {
            isCreating_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Редактирование
    if (ImGui::BeginPopupModal("Редактировать накладную", &isEditing_, ImGuiWindowFlags_AlwaysAutoResize)) {
        char numberBuffer[64] = "";
        char dateBuffer[32] = "";
        double amount = 0.0;
        int selectedContractId = -1;

        strncpy(numberBuffer, currentItem_.number.c_str(), sizeof(numberBuffer) - 1);
        strncpy(dateBuffer, currentItem_.date.c_str(), sizeof(dateBuffer) - 1);
        amount = currentItem_.totalAmount;
        if (currentItem_.contractId.has_value()) {
            selectedContractId = currentItem_.contractId.value();
        }

        ImGui::InputText("Номер*", numberBuffer, sizeof(numberBuffer));
        ImGui::InputText("Дата*", dateBuffer, sizeof(dateBuffer));
        CustomWidgets::AmountInput("Сумма", amount);
        
        // Выбор договора
        ImGui::Text("Договор:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(250);
        if (dbManager_) {
            auto contracts = dbManager_->getAllContracts();
            std::vector<const char*> contractItems;
            std::vector<int> contractIds;
            contractItems.push_back("Без договора");
            contractIds.push_back(-1);
            
            for (const auto& c : contracts) {
                char* label = new char[256];
                snprintf(label, 256, "%s от %s", c.number.c_str(), c.date.c_str());
                contractItems.push_back(label);
                contractIds.push_back(c.id);
            }
            
            // Находим текущий индекс
            int currentIdx = 0;
            for (size_t i = 0; i < contractIds.size(); ++i) {
                if (contractIds[i] == selectedContractId || 
                    (selectedContractId == -1 && i == 0)) {
                    currentIdx = static_cast<int>(i);
                    break;
                }
            }
            
            if (ImGui::Combo("##ContractEdit", &currentIdx, contractItems.data(), contractItems.size())) {
                selectedContractId = contractIds[currentIdx];
            }
            
            for (size_t i = 1; i < contractItems.size(); ++i) {
                delete[] contractItems[i];
            }
        }

        ImGui::Separator();

        if (ImGui::Button("Сохранить", ImVec2(100, 0))) {
            if (strlen(numberBuffer) > 0 && strlen(dateBuffer) > 0) {
                Invoice invoice = currentItem_;
                invoice.number = numberBuffer;
                invoice.date = dateBuffer;
                invoice.totalAmount = amount;
                
                if (selectedContractId > 0 && dbManager_) {
                    auto contracts = dbManager_->getAllContracts();
                    int idx = 0;
                    for (const auto& c : contracts) {
                        if (idx == selectedContractId - 1) {
                            invoice.contractId = c.id;
                            break;
                        }
                        idx++;
                    }
                } else {
                    invoice.contractId = std::nullopt;
                }
                
                if (dbManager_->updateInvoice(invoice)) {
                    needRefresh_ = true;
                    isEditing_ = false;
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена", ImVec2(100, 0))) {
            isEditing_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Удаление
    if (ImGui::BeginPopupModal("Подтверждение удаления", &showDeleteConfirm_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Вы действительно хотите удалить эту накладную?");
        ImGui::Separator();

        if (ImGui::Button("Удалить", ImVec2(100, 0))) {
            if (dbManager_->deleteInvoice(deleteItemId_)) {
                needRefresh_ = true;
            }
            showDeleteConfirm_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена", ImVec2(100, 0))) {
            showDeleteConfirm_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

void InvoicesView::clearSelection() {
    selectedIds_.clear();
}

void InvoicesView::toggleSelection(int id) {
    if (selectedIds_.count(id)) {
        selectedIds_.erase(id);
    } else {
        selectedIds_.insert(id);
    }
}

void InvoicesView::clearFilters() {
    filterNumber_.clear();
    filterContractId_ = -1;
    applyFilter();
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> InvoicesView::GetDataAsStrings() {
    std::vector<std::string> columns = {"ID", "Number", "Date", "ContractID", "Amount"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& inv : filteredInvoices_) {
        rows.push_back({
            std::to_string(inv.id),
            inv.number,
            inv.date,
            inv.contractId.has_value() ? std::to_string(inv.contractId.value()) : "",
            std::to_string(static_cast<long long>(inv.totalAmount))
        });
    }
    
    return {columns, rows};
}

} // namespace FinancialAudit
