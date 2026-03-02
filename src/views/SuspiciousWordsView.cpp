/**
 * @file SuspiciousWordsView.cpp
 * @brief Реализация представления подозрительных слов
 */

#include "SuspiciousWordsView.h"

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
#define ICON_FA_EXCLAMATION "\xEF\x81\xB1"

namespace FinancialAudit {

SuspiciousWordsView::SuspiciousWordsView() {
    title_ = "Подозрительные слова";
}

SuspiciousWordsView::~SuspiciousWordsView() = default;

void SuspiciousWordsView::refreshData() {
    if (!dbManager_) return;
    words_ = dbManager_->getAllSuspiciousWords();
    filteredWords_ = words_;
    applyFilter();
    needRefresh_ = false;
}

void SuspiciousWordsView::applyFilter() {
    filteredWords_.clear();
    for (const auto& w : words_) {
        bool matches = true;
        if (!filterWord_.empty() && w.word.find(filterWord_) == std::string::npos) {
            matches = false;
        }
        if (matches) {
            filteredWords_.push_back(w);
        }
    }
}

void SuspiciousWordsView::sortData() {
    if (sortColumn_ < 0) return;
    
    std::sort(filteredWords_.begin(), filteredWords_.end(),
        [this](const SuspiciousWord& a, const SuspiciousWord& b) {
            bool result = false;
            switch (sortColumn_) {
                case 0: result = a.word < b.word; break;
            }
            return sortAscending_ ? result : !result;
        });
}

void SuspiciousWordsView::renderFilters() {
    ImGui::Text("Фильтры:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(250);
    if (ImGui::InputText("##FilterWord", &filterWord_[0], filterWord_.capacity() + 1)) {
        filterWord_.resize(std::strlen(filterWord_.c_str()));
        applyFilter();
    }
    ImGui::SameLine();
    if (ImGui::Button("Очистить")) {
        clearFilters();
    }
}

void SuspiciousWordsView::renderToolbar() {
    if (ImGui::Button(ICON_FA_PLUS " Создать")) {
        currentItem_ = SuspiciousWord{};
        editingId_ = -1;
        isCreating_ = true;
    }
    ImGui::SameLine();
    
    if (ImGui::Button(ICON_FA_FILE_CSV " Экспорт CSV")) {
        auto data = GetDataAsStrings();
        ExportManager exportMgr(dbManager_);
        exportMgr.exportDataToCsv("suspicious_words_export.csv", data.first, data.second);
    }
    
    ImGui::SameLine();
    ImGui::Text("Всего: %zu", filteredWords_.size());
}

void SuspiciousWordsView::renderTable() {
    if (ImGui::BeginTable("SuspiciousWordsTable", 2, 
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Слово", ImGuiTableColumnFlags_WidthStretch, 0);
        
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
        
        for (const auto& w : filteredWords_) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", w.id);
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", w.word.c_str());
            
            if (ImGui::BeginPopupContextItem("WordContext")) {
                if (ImGui::MenuItem("Редактировать")) {
                    currentItem_ = w;
                    editingId_ = w.id;
                    isEditing_ = true;
                }
                if (ImGui::MenuItem("Удалить")) {
                    deleteItemId_ = w.id;
                    showDeleteConfirm_ = true;
                }
                ImGui::EndPopup();
            }
        }
        
        ImGui::EndTable();
    }
}

void SuspiciousWordsView::Render() {
    ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_FirstUseEver);

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
    if (isCreating_) ImGui::OpenPopup("Создать слово");
    if (isEditing_) ImGui::OpenPopup("Редактировать слово");
    if (showDeleteConfirm_) ImGui::OpenPopup("Подтверждение удаления");

    // Создание
    if (ImGui::BeginPopupModal("Создать слово", &isCreating_, ImGuiWindowFlags_AlwaysAutoResize)) {
        char wordBuffer[128] = "";

        strncpy(wordBuffer, currentItem_.word.c_str(), sizeof(wordBuffer) - 1);

        ImGui::InputText("Слово*", wordBuffer, sizeof(wordBuffer));
        
        ImGui::Separator();

        if (ImGui::Button("Создать", ImVec2(100, 0))) {
            if (strlen(wordBuffer) > 0) {
                SuspiciousWord word;
                word.word = wordBuffer;
                
                if (dbManager_->createSuspiciousWord(word)) {
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
    if (ImGui::BeginPopupModal("Редактировать слово", &isEditing_, ImGuiWindowFlags_AlwaysAutoResize)) {
        char wordBuffer[128] = "";

        strncpy(wordBuffer, currentItem_.word.c_str(), sizeof(wordBuffer) - 1);

        ImGui::InputText("Слово*", wordBuffer, sizeof(wordBuffer));
        
        ImGui::Separator();

        if (ImGui::Button("Сохранить", ImVec2(100, 0))) {
            if (strlen(wordBuffer) > 0) {
                SuspiciousWord word = currentItem_;
                word.word = wordBuffer;
                
                if (dbManager_->updateSuspiciousWord(word)) {
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
        ImGui::Text("Вы действительно хотите удалить это слово?");
        ImGui::Separator();

        if (ImGui::Button("Удалить", ImVec2(100, 0))) {
            if (dbManager_->deleteSuspiciousWord(deleteItemId_)) {
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

void SuspiciousWordsView::clearFilters() {
    filterWord_.clear();
    applyFilter();
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> SuspiciousWordsView::GetDataAsStrings() {
    std::vector<std::string> columns = {"ID", "Word"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& w : filteredWords_) {
        rows.push_back({
            std::to_string(w.id),
            w.word
        });
    }
    
    return {columns, rows};
}

} // namespace FinancialAudit
