/**
 * @file CounterpartiesView.cpp
 * @brief Реализация представления контрагентов
 */

#include "CounterpartiesView.h"

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
#define ICON_FA_USERS "\xEF\x83\x80"
#define ICON_FA_BULK "\xEF\x83\x80"

namespace FinancialAudit {

CounterpartiesView::CounterpartiesView() {
    title_ = "Контрагенты";
}

CounterpartiesView::~CounterpartiesView() = default;

void CounterpartiesView::refreshData() {
    if (!dbManager_) return;
    counterparties_ = dbManager_->getAllCounterparties();
    filteredCounterparties_ = counterparties_;
    applyFilter();
    needRefresh_ = false;
}

void CounterpartiesView::applyFilter() {
    filteredCounterparties_.clear();
    for (const auto& c : counterparties_) {
        bool matches = true;
        if (!filterName_.empty() && c.name.find(filterName_) == std::string::npos) {
            matches = false;
        }
        if (!filterInn_.empty() && c.inn.value_or("").find(filterInn_) == std::string::npos) {
            matches = false;
        }
        if (matches) {
            filteredCounterparties_.push_back(c);
        }
    }
}

void CounterpartiesView::sortData() {
    if (sortColumn_ < 0) return;
    
    std::sort(filteredCounterparties_.begin(), filteredCounterparties_.end(),
        [this](const Counterparty& a, const Counterparty& b) {
            bool result = false;
            switch (sortColumn_) {
                case 0: result = a.name < b.name; break;
                case 1: result = a.inn.value_or("") < b.inn.value_or(""); break;
                case 2: result = a.isContractOptional < b.isContractOptional; break;
            }
            return sortAscending_ ? result : !result;
        });
}

void CounterpartiesView::renderFilters() {
    ImGui::Text("Фильтры:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##FilterName", &filterName_[0], filterName_.capacity() + 1)) {
        filterName_.resize(std::strlen(filterName_.c_str()));
        applyFilter();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputText("##FilterInn", &filterInn_[0], filterInn_.capacity() + 1)) {
        filterInn_.resize(std::strlen(filterInn_.c_str()));
        applyFilter();
    }
    ImGui::SameLine();
    if (ImGui::Button("Очистить")) {
        clearFilters();
    }
}

void CounterpartiesView::renderToolbar() {
    if (ImGui::Button(ICON_FA_PLUS " Создать")) {
        currentItem_ = Counterparty{};
        editingId_ = -1;
        isCreating_ = true;
    }
    ImGui::SameLine();
    
    if (ImGui::Button(ICON_FA_FILE_CSV " Экспорт CSV")) {
        auto data = GetDataAsStrings();
        ExportManager exportMgr(dbManager_);
        exportMgr.exportDataToCsv("counterparties_export.csv", data.first, data.second);
    }
    ImGui::SameLine();
    
    if (!selectedIds_.empty()) {
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_BULK " Групповая операция")) {
            showGroupOperation_ = true;
        }
    }
    
    ImGui::SameLine();
    ImGui::Text("Всего: %zu", filteredCounterparties_.size());
}

void CounterpartiesView::renderTable() {
    if (ImGui::BeginTable("CounterpartiesTable", 4, 
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::TableSetupColumn("Наименование", ImGuiTableColumnFlags_WidthStretch, 0);
        ImGui::TableSetupColumn("ИНН", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Договор не обяз.", ImGuiTableColumnFlags_WidthFixed, 120);
        
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
        
        for (const auto& c : filteredCounterparties_) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            bool isSelected = selectedIds_.count(c.id) > 0;
            if (ImGui::Checkbox("", &isSelected)) {
                toggleSelection(c.id);
            }
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", c.name.c_str());
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", c.inn.value_or("N/A").c_str());
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", c.isContractOptional ? "Да" : "Нет");
            
            if (ImGui::BeginPopupContextItem("CounterpartyContext")) {
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

void CounterpartiesView::Render() {
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

    // Модальное окно создания
    if (isCreating_) {
        ImGui::OpenPopup("Создать контрагента");
    }

    if (isEditing_) {
        ImGui::OpenPopup("Редактировать контрагента");
    }

    if (showDeleteConfirm_) {
        ImGui::OpenPopup("Подтверждение удаления");
    }

    if (showGroupOperation_) {
        ImGui::OpenPopup("Групповая операция");
    }

    // Создание
    if (ImGui::BeginPopupModal("Создать контрагента", &isCreating_, ImGuiWindowFlags_AlwaysAutoResize)) {
        char nameBuffer[512] = "";
        char innBuffer[32] = "";
        bool isContractOptional = false;

        strncpy(nameBuffer, currentItem_.name.c_str(), sizeof(nameBuffer) - 1);
        strncpy(innBuffer, currentItem_.inn.value_or("").c_str(), sizeof(innBuffer) - 1);
        isContractOptional = currentItem_.isContractOptional;

        ImGui::InputText("Наименование*", nameBuffer, sizeof(nameBuffer));
        ImGui::InputText("ИНН", innBuffer, sizeof(innBuffer));
        ImGui::Checkbox("Договор не обязателен", &isContractOptional);

        ImGui::Separator();

        if (ImGui::Button("Создать", ImVec2(100, 0))) {
            if (strlen(nameBuffer) > 0) {
                Counterparty cp;
                cp.name = nameBuffer;
                if (strlen(innBuffer) > 0) {
                    cp.inn = innBuffer;
                }
                cp.isContractOptional = isContractOptional;
                
                if (dbManager_->createCounterparty(cp)) {
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
    if (ImGui::BeginPopupModal("Редактировать контрагента", &isEditing_, ImGuiWindowFlags_AlwaysAutoResize)) {
        char nameBuffer[512] = "";
        char innBuffer[32] = "";
        bool isContractOptional = false;

        strncpy(nameBuffer, currentItem_.name.c_str(), sizeof(nameBuffer) - 1);
        strncpy(innBuffer, currentItem_.inn.value_or("").c_str(), sizeof(innBuffer) - 1);
        isContractOptional = currentItem_.isContractOptional;

        ImGui::InputText("Наименование*", nameBuffer, sizeof(nameBuffer));
        ImGui::InputText("ИНН", innBuffer, sizeof(innBuffer));
        ImGui::Checkbox("Договор не обязателен", &isContractOptional);

        ImGui::Separator();

        if (ImGui::Button("Сохранить", ImVec2(100, 0))) {
            if (strlen(nameBuffer) > 0) {
                Counterparty cp = currentItem_;
                cp.name = nameBuffer;
                if (strlen(innBuffer) > 0) {
                    cp.inn = innBuffer;
                } else {
                    cp.inn = std::nullopt;
                }
                cp.isContractOptional = isContractOptional;
                
                if (dbManager_->updateCounterparty(cp)) {
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
        ImGui::Text("Вы действительно хотите удалить этого контрагента?");
        ImGui::Separator();

        if (ImGui::Button("Удалить", ImVec2(100, 0))) {
            if (dbManager_->deleteCounterparty(deleteItemId_)) {
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
        ImGui::Text("Выбрано: %zu контрагентов", selectedIds_.size());
        ImGui::Separator();
        
        if (ImGui::Button("Удалить выбранные", ImVec2(200, 0))) {
            std::vector<int> ids(selectedIds_.begin(), selectedIds_.end());
            dbManager_->bulkDeletePayments(ids); // TODO: добавить bulkDeleteCounterparties
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

    ImGui::End();
}

void CounterpartiesView::clearSelection() {
    selectedIds_.clear();
}

void CounterpartiesView::toggleSelection(int id) {
    if (selectedIds_.count(id)) {
        selectedIds_.erase(id);
    } else {
        selectedIds_.insert(id);
    }
}

void CounterpartiesView::clearFilters() {
    filterName_.clear();
    filterInn_.clear();
    applyFilter();
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> CounterpartiesView::GetDataAsStrings() {
    std::vector<std::string> columns = {"ID", "Name", "INN", "ContractOptional"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& c : filteredCounterparties_) {
        rows.push_back({
            std::to_string(c.id),
            c.name,
            c.inn.value_or("N/A"),
            c.isContractOptional ? "Yes" : "No"
        });
    }
    
    return {columns, rows};
}

} // namespace FinancialAudit
