/**
 * @file ContractsView.cpp
 * @brief Реализация представления договоров
 */

#include "ContractsView.h"

#include <imgui_internal.h>
#include <algorithm>
#include <cstring>

#include "managers/DatabaseManager.h"
#include "managers/UIManager.h"
#include "managers/PdfReporter.h"
#include "managers/ExportManager.h"
#include "widgets/CustomWidgets.h"

#define ICON_FA_PLUS "\xEF\x81\xA7"
#define ICON_FA_EDIT "\xEF\x81\x84"
#define ICON_FA_TRASH "\xEF\x87\xB8"
#define ICON_FA_SAVE "\xEF\x83\x87"
#define ICON_FA_SEARCH "\xEF\x80\x82"
#define ICON_FA_FILE_CSV "\xEF\x96\xB3"
#define ICON_FA_FILE_PDF "\xEF\x87\x81"
#define ICON_FA_EXCHANGE "\xEF\x33\xB2"
#define ICON_FA_BULK "\xEF\x83\x80"

namespace FinancialAudit {

ContractsView::ContractsView() {
    title_ = "Договоры";
}

ContractsView::~ContractsView() = default;

void ContractsView::refreshData() {
    if (!dbManager_) return;
    contracts_ = dbManager_->getAllContracts();
    filteredContracts_ = contracts_;
    applyFilter();
    needRefresh_ = false;
}

void ContractsView::applyFilter() {
    filteredContracts_.clear();
    for (const auto& c : contracts_) {
        bool matches = true;
        if (!filterNumber_.empty() && c.number.find(filterNumber_) == std::string::npos) {
            matches = false;
        }
        if (!filterIcz_.empty() && c.procurementCode.find(filterIcz_) == std::string::npos) {
            matches = false;
        }
        if (showOnlyWithIcz_ && c.procurementCode.empty()) {
            matches = false;
        }
        if (matches) {
            filteredContracts_.push_back(c);
        }
    }
}

void ContractsView::sortData() {
    if (sortColumn_ < 0) return;
    
    std::sort(filteredContracts_.begin(), filteredContracts_.end(),
        [this](const Contract& a, const Contract& b) {
            bool result = false;
            switch (sortColumn_) {
                case 0: result = a.number < b.number; break;
                case 1: result = a.date < b.date; break;
                case 2: result = a.contractAmount < b.contractAmount; break;
                case 3: result = a.procurementCode < b.procurementCode; break;
            }
            return sortAscending_ ? result : !result;
        });
}

void ContractsView::renderFilters() {
    ImGui::Text("Фильтры:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputText("##FilterNumber", &filterNumber_[0], filterNumber_.capacity() + 1)) {
        filterNumber_.resize(std::strlen(filterNumber_.c_str()));
        applyFilter();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##FilterIcz", &filterIcz_[0], filterIcz_.capacity() + 1)) {
        filterIcz_.resize(std::strlen(filterIcz_.c_str()));
        applyFilter();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("С ИКЗ", &showOnlyWithIcz_)) {
        applyFilter();
    }
    ImGui::SameLine();
    if (ImGui::Button("Очистить")) {
        clearFilters();
    }
}

void ContractsView::renderToolbar() {
    if (ImGui::Button(ICON_FA_PLUS " Создать")) {
        currentItem_ = Contract{};
        editingId_ = -1;
        isCreating_ = true;
    }
    ImGui::SameLine();
    
    if (ImGui::Button(ICON_FA_FILE_CSV " Экспорт CSV")) {
        auto data = GetDataAsStrings();
        ExportManager exportMgr(dbManager_);
        exportMgr.exportDataToCsv("contracts_export.csv", data.first, data.second);
    }
    ImGui::SameLine();
    
    if (ImGui::Button(ICON_FA_FILE_PDF " PDF")) {
        if (pdfReporter_) {
            pdfReporter_->generateContractsReport("contracts_report.pdf");
        }
    }
    ImGui::SameLine();
    
    if (!selectedIds_.empty()) {
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_BULK " Групповая")) {
            showGroupOperation_ = true;
        }
    }
    
    if (!selectedIds_.empty() && selectedIds_.size() == 2) {
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_EXCHANGE " Перенести детали")) {
            showTransferDialog_ = true;
            auto it = selectedIds_.begin();
            transferFromId_ = *it;
            transferToId_ = *++it;
        }
    }
    
    ImGui::SameLine();
    ImGui::Text("Всего: %zu", filteredContracts_.size());
}

void ContractsView::renderTable() {
    if (ImGui::BeginTable("ContractsTable", 6, 
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::TableSetupColumn("Номер", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Дата", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Сумма", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("ИКЗ", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Примечание", ImGuiTableColumnFlags_WidthStretch, 0);
        
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
        
        for (const auto& c : filteredContracts_) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            bool isSelected = selectedIds_.count(c.id) > 0;
            if (ImGui::Checkbox("", &isSelected)) {
                toggleSelection(c.id);
            }
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", c.number.c_str());
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", c.date.c_str());
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f", c.contractAmount);
            
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%s", c.procurementCode.empty() ? "-" : c.procurementCode.c_str());
            
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%s", c.note.empty() ? "-" : c.note.c_str());
            
            if (ImGui::BeginPopupContextItem("ContractContext")) {
                if (ImGui::MenuItem("Редактировать")) {
                    currentItem_ = c;
                    editingId_ = c.id;
                    isEditing_ = true;
                }
                if (ImGui::MenuItem("Удалить")) {
                    deleteItemId_ = c.id;
                    showDeleteConfirm_ = true;
                }
                ImGui::EndPopup();
            }
        }
        
        ImGui::EndTable();
    }
}

void ContractsView::Render() {
    ImGui::SetNextWindowSize(ImVec2(1000, 650), ImGuiCond_FirstUseEver);

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
    if (isCreating_) ImGui::OpenPopup("Создать договор");
    if (isEditing_) ImGui::OpenPopup("Редактировать договор");
    if (showDeleteConfirm_) ImGui::OpenPopup("Подтверждение удаления");
    if (showGroupOperation_) ImGui::OpenPopup("Групповая операция");
    if (showTransferDialog_) ImGui::OpenPopup("Перенос деталей");

    // Создание
    if (ImGui::BeginPopupModal("Создать договор", &isCreating_, ImGuiWindowFlags_AlwaysAutoResize)) {
        char numberBuffer[64] = "";
        char dateBuffer[32] = "";
        char endDateBuffer[32] = "";
        char iczBuffer[64] = "";
        char noteBuffer[512] = "";
        double amount = 0.0;
        bool isForChecking = false;
        bool isForSpecialControl = false;

        strncpy(numberBuffer, currentItem_.number.c_str(), sizeof(numberBuffer) - 1);
        strncpy(dateBuffer, currentItem_.date.c_str(), sizeof(dateBuffer) - 1);
        strncpy(endDateBuffer, currentItem_.endDate.c_str(), sizeof(endDateBuffer) - 1);
        strncpy(iczBuffer, currentItem_.procurementCode.c_str(), sizeof(iczBuffer) - 1);
        strncpy(noteBuffer, currentItem_.note.c_str(), sizeof(noteBuffer) - 1);
        amount = currentItem_.contractAmount;
        isForChecking = currentItem_.isForChecking;
        isForSpecialControl = currentItem_.isForSpecialControl;

        ImGui::InputText("Номер*", numberBuffer, sizeof(numberBuffer));
        ImGui::InputText("Дата*", dateBuffer, sizeof(dateBuffer));
        ImGui::InputText("Дата окончания", endDateBuffer, sizeof(endDateBuffer));
        ImGui::InputText("ИКЗ", iczBuffer, sizeof(iczBuffer));
        CustomWidgets::AmountInput("Сумма", amount);
        ImGui::InputTextMultiline("Примечание", noteBuffer, sizeof(noteBuffer), ImVec2(400, 60));
        ImGui::Checkbox("Для проверки", &isForChecking);
        ImGui::Checkbox("Спецконтроль", &isForSpecialControl);

        ImGui::Separator();

        if (ImGui::Button("Создать", ImVec2(100, 0))) {
            if (strlen(numberBuffer) > 0 && strlen(dateBuffer) > 0) {
                Contract contract;
                contract.number = numberBuffer;
                contract.date = dateBuffer;
                contract.endDate = endDateBuffer;
                contract.procurementCode = iczBuffer;
                contract.note = noteBuffer;
                contract.contractAmount = amount;
                contract.isForChecking = isForChecking;
                contract.isForSpecialControl = isForSpecialControl;
                
                if (dbManager_->createContract(contract)) {
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
    if (ImGui::BeginPopupModal("Редактировать договор", &isEditing_, ImGuiWindowFlags_AlwaysAutoResize)) {
        char numberBuffer[64] = "";
        char dateBuffer[32] = "";
        char endDateBuffer[32] = "";
        char iczBuffer[64] = "";
        char noteBuffer[512] = "";
        double amount = 0.0;
        bool isForChecking = false;
        bool isForSpecialControl = false;

        strncpy(numberBuffer, currentItem_.number.c_str(), sizeof(numberBuffer) - 1);
        strncpy(dateBuffer, currentItem_.date.c_str(), sizeof(dateBuffer) - 1);
        strncpy(endDateBuffer, currentItem_.endDate.c_str(), sizeof(endDateBuffer) - 1);
        strncpy(iczBuffer, currentItem_.procurementCode.c_str(), sizeof(iczBuffer) - 1);
        strncpy(noteBuffer, currentItem_.note.c_str(), sizeof(noteBuffer) - 1);
        amount = currentItem_.contractAmount;
        isForChecking = currentItem_.isForChecking;
        isForSpecialControl = currentItem_.isForSpecialControl;

        ImGui::InputText("Номер*", numberBuffer, sizeof(numberBuffer));
        ImGui::InputText("Дата*", dateBuffer, sizeof(dateBuffer));
        ImGui::InputText("Дата окончания", endDateBuffer, sizeof(endDateBuffer));
        ImGui::InputText("ИКЗ", iczBuffer, sizeof(iczBuffer));
        CustomWidgets::AmountInput("Сумма", amount);
        ImGui::InputTextMultiline("Примечание", noteBuffer, sizeof(noteBuffer), ImVec2(400, 60));
        ImGui::Checkbox("Для проверки", &isForChecking);
        ImGui::Checkbox("Спецконтроль", &isForSpecialControl);

        ImGui::Separator();

        if (ImGui::Button("Сохранить", ImVec2(100, 0))) {
            if (strlen(numberBuffer) > 0 && strlen(dateBuffer) > 0) {
                Contract contract = currentItem_;
                contract.number = numberBuffer;
                contract.date = dateBuffer;
                contract.endDate = endDateBuffer;
                contract.procurementCode = iczBuffer;
                contract.note = noteBuffer;
                contract.contractAmount = amount;
                contract.isForChecking = isForChecking;
                contract.isForSpecialControl = isForSpecialControl;
                
                if (dbManager_->updateContract(contract)) {
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
        ImGui::Text("Вы действительно хотите удалить этот договор?");
        ImGui::Separator();

        if (ImGui::Button("Удалить", ImVec2(100, 0))) {
            if (dbManager_->deleteContract(deleteItemId_)) {
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

    // Групповая операция
    if (ImGui::BeginPopupModal("Групповая операция", &showGroupOperation_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Выбрано: %zu договоров", selectedIds_.size());
        ImGui::Separator();
        
        if (ImGui::Button("Удалить выбранные", ImVec2(200, 0))) {
            std::vector<int> ids(selectedIds_.begin(), selectedIds_.end());
            for (int id : ids) {
                dbManager_->deleteContract(id);
            }
            clearSelection();
            needRefresh_ = true;
            showGroupOperation_ = false;
        }
        
        ImGui::Separator();
        if (ImGui::Button("Отмена", ImVec2(100, 0))) {
            showGroupOperation_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Перенос деталей
    if (ImGui::BeginPopupModal("Перенос деталей", &showTransferDialog_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Перенести детали с договора %d на договор %d?", transferFromId_, transferToId_);
        ImGui::Separator();
        
        if (ImGui::Button("Перенести", ImVec2(120, 0))) {
            if (dbManager_->transferContractDetails(transferFromId_, transferToId_)) {
                needRefresh_ = true;
            }
            clearSelection();
            showTransferDialog_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена", ImVec2(100, 0))) {
            showTransferDialog_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

void ContractsView::clearSelection() {
    selectedIds_.clear();
}

void ContractsView::toggleSelection(int id) {
    if (selectedIds_.count(id)) {
        selectedIds_.erase(id);
    } else {
        selectedIds_.insert(id);
    }
}

void ContractsView::clearFilters() {
    filterNumber_.clear();
    filterIcz_.clear();
    showOnlyWithIcz_ = false;
    applyFilter();
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> ContractsView::GetDataAsStrings() {
    std::vector<std::string> columns = {"ID", "Number", "Date", "Amount", "ICZ", "Note"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& c : filteredContracts_) {
        rows.push_back({
            std::to_string(c.id),
            c.number,
            c.date,
            std::to_string(static_cast<long long>(c.contractAmount)),
            c.procurementCode.empty() ? "" : c.procurementCode,
            c.note.empty() ? "" : c.note
        });
    }
    
    return {columns, rows};
}

} // namespace FinancialAudit
