/**
 * @file RegexesView.cpp
 * @brief Реализация представления регулярных выражений
 */

#include "RegexesView.h"

#include <imgui_internal.h>
#include <algorithm>
#include <cstring>

#include "managers/DatabaseManager.h"
#include "managers/ExportManager.h"
#include "widgets/CustomWidgets.h"

#define ICON_FA_PLUS "\xEF\x81\xA7"
#define ICON_FA_EDIT "\xEF\x81\x84"
#define ICON_FA_TRASH "\xEF\x87\xB8"
#define ICON_FA_FILE_CSV "\xEF\x96\xB3"
#define ICON_FA_SEARCH "\xEF\x80\x82"

namespace FinancialAudit {

RegexesView::RegexesView() {
    title_ = "Регулярные выражения";
}

RegexesView::~RegexesView() = default;

void RegexesView::refreshData() {
    if (!dbManager_) return;
    regexes_ = dbManager_->getAllRegexes();
    filteredRegexes_ = regexes_;
    applyFilter();
    needRefresh_ = false;
}

void RegexesView::applyFilter() {
    filteredRegexes_.clear();
    for (const auto& r : regexes_) {
        bool matches = true;
        if (!filterName_.empty() && r.name.find(filterName_) == std::string::npos) {
            matches = false;
        }
        if (!filterPattern_.empty() && r.pattern.find(filterPattern_) == std::string::npos) {
            matches = false;
        }
        if (matches) {
            filteredRegexes_.push_back(r);
        }
    }
}

void RegexesView::sortData() {
    if (sortColumn_ < 0) return;
    
    std::sort(filteredRegexes_.begin(), filteredRegexes_.end(),
        [this](const RegexEntry& a, const RegexEntry& b) {
            bool result = false;
            switch (sortColumn_) {
                case 0: result = a.name < b.name; break;
                case 1: result = a.pattern < b.pattern; break;
            }
            return sortAscending_ ? result : !result;
        });
}

void RegexesView::renderFilters() {
    ImGui::Text("Фильтры:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##FilterName", &filterName_[0], filterName_.capacity() + 1)) {
        filterName_.resize(std::strlen(filterName_.c_str()));
        applyFilter();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(250);
    if (ImGui::InputText("##FilterPattern", &filterPattern_[0], filterPattern_.capacity() + 1)) {
        filterPattern_.resize(std::strlen(filterPattern_.c_str()));
        applyFilter();
    }
    ImGui::SameLine();
    if (ImGui::Button("Очистить")) {
        clearFilters();
    }
}

void RegexesView::renderToolbar() {
    if (ImGui::Button(ICON_FA_PLUS " Создать")) {
        currentItem_ = RegexEntry{};
        editingId_ = -1;
        isCreating_ = true;
    }
    ImGui::SameLine();
    
    if (ImGui::Button(ICON_FA_FILE_CSV " Экспорт CSV")) {
        auto data = GetDataAsStrings();
        ExportManager exportMgr(dbManager_);
        exportMgr.exportDataToCsv("regexes_export.csv", data.first, data.second);
    }
    
    ImGui::SameLine();
    ImGui::Text("Всего: %zu", filteredRegexes_.size());
}

void RegexesView::renderTable() {
    if (ImGui::BeginTable("RegexesTable", 3, 
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Имя", ImGuiTableColumnFlags_WidthFixed, 200);
        ImGui::TableSetupColumn("Паттерн", ImGuiTableColumnFlags_WidthStretch, 0);
        
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
        
        for (const auto& r : filteredRegexes_) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", r.id);
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", r.name.c_str());
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", r.pattern.c_str());
            
            if (ImGui::BeginPopupContextItem("RegexContext")) {
                if (ImGui::MenuItem("Редактировать")) {
                    currentItem_ = r;
                    editingId_ = r.id;
                    isEditing_ = true;
                }
                if (ImGui::MenuItem("Удалить")) {
                    deleteItemId_ = r.id;
                    showDeleteConfirm_ = true;
                }
                ImGui::EndPopup();
            }
        }
        
        ImGui::EndTable();
    }
}

void RegexesView::Render() {
    ImGui::SetNextWindowSize(ImVec2(800, 500), ImGuiCond_FirstUseEver);

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
    if (isCreating_) ImGui::OpenPopup("Создать правило");
    if (isEditing_) ImGui::OpenPopup("Редактировать правило");
    if (showDeleteConfirm_) ImGui::OpenPopup("Подтверждение удаления");

    // Создание
    if (ImGui::BeginPopupModal("Создать правило", &isCreating_, ImGuiWindowFlags_AlwaysAutoResize)) {
        char nameBuffer[128] = "";
        char patternBuffer[512] = "";

        strncpy(nameBuffer, currentItem_.name.c_str(), sizeof(nameBuffer) - 1);
        strncpy(patternBuffer, currentItem_.pattern.c_str(), sizeof(patternBuffer) - 1);

        ImGui::InputText("Имя*", nameBuffer, sizeof(nameBuffer));
        ImGui::InputText("Паттерн*", patternBuffer, sizeof(patternBuffer));
        
        ImGui::Separator();

        if (ImGui::Button("Создать", ImVec2(100, 0))) {
            if (strlen(nameBuffer) > 0 && strlen(patternBuffer) > 0) {
                RegexEntry regex;
                regex.name = nameBuffer;
                regex.pattern = patternBuffer;
                
                if (dbManager_->createRegex(regex)) {
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
    if (ImGui::BeginPopupModal("Редактировать правило", &isEditing_, ImGuiWindowFlags_AlwaysAutoResize)) {
        char nameBuffer[128] = "";
        char patternBuffer[512] = "";

        strncpy(nameBuffer, currentItem_.name.c_str(), sizeof(nameBuffer) - 1);
        strncpy(patternBuffer, currentItem_.pattern.c_str(), sizeof(patternBuffer) - 1);

        ImGui::InputText("Имя*", nameBuffer, sizeof(nameBuffer));
        ImGui::InputText("Паттерн*", patternBuffer, sizeof(patternBuffer));
        
        ImGui::Separator();

        if (ImGui::Button("Сохранить", ImVec2(100, 0))) {
            if (strlen(nameBuffer) > 0 && strlen(patternBuffer) > 0) {
                RegexEntry regex = currentItem_;
                regex.name = nameBuffer;
                regex.pattern = patternBuffer;
                
                if (dbManager_->updateRegex(regex)) {
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
        ImGui::Text("Вы действительно хотите удалить это правило?");
        ImGui::Separator();

        if (ImGui::Button("Удалить", ImVec2(100, 0))) {
            if (dbManager_->deleteRegex(deleteItemId_)) {
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

void RegexesView::clearFilters() {
    filterName_.clear();
    filterPattern_.clear();
    applyFilter();
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> RegexesView::GetDataAsStrings() {
    std::vector<std::string> columns = {"ID", "Name", "Pattern"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& r : filteredRegexes_) {
        rows.push_back({
            std::to_string(r.id),
            r.name,
            r.pattern
        });
    }
    
    return {columns, rows};
}

} // namespace FinancialAudit
