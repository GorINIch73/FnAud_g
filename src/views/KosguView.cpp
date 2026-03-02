/**
 * @file KosguView.cpp
 * @brief Реализация представления справочника КОСГУ
 */

#include "KosguView.h"

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
#define ICON_FA_UPLOAD "\xEF\x80\x93"
#define ICON_FA_DOWNLOAD "\xEF\x80\x99"
#define ICON_FA_TIMES "\xEF\x80\x8D"

namespace FinancialAudit {

KosguView::KosguView() {
    title_ = "Справочник КОСГУ";
}

void KosguView::Render() {
    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    
    if (!ImGui::Begin(title_.c_str(), &isOpen_)) {
        ImGui::End();
        return;
    }
    
    if (needRefresh_) {
        refreshData();
    }
    
    renderToolbar();
    ImGui::Separator();
    renderTable();
    renderImportExport();
    
    // Диалоги
    if (isCreating_) {
        ImGui::OpenPopup("Создать запись КОСГУ");
    }
    
    if (isEditing_) {
        ImGui::OpenPopup("Редактировать запись КОСГУ");
    }
    
    if (showDeleteConfirm_) {
        ImGui::OpenPopup("Подтверждение удаления");
    }
    
    // Модальное окно создания
    if (ImGui::BeginPopupModal("Создать запись КОСГУ", &isCreating_, ImGuiWindowFlags_AlwaysAutoResize)) {
        char codeBuffer[64] = "";
        char nameBuffer[256] = "";
        char noteBuffer[512] = "";
        
        strncpy(codeBuffer, currentItem_.code.c_str(), sizeof(codeBuffer) - 1);
        strncpy(nameBuffer, currentItem_.name.c_str(), sizeof(nameBuffer) - 1);
        strncpy(noteBuffer, currentItem_.note.c_str(), sizeof(noteBuffer) - 1);
        
        ImGui::InputText("Код*", codeBuffer, sizeof(codeBuffer));
        ImGui::InputText("Наименование*", nameBuffer, sizeof(nameBuffer));
        ImGui::InputTextMultiline("Примечание", noteBuffer, sizeof(noteBuffer), ImVec2(400, 100));
        
        ImGui::Separator();
        
        if (ImGui::Button("Создать", ImVec2(100, 0))) {
            if (strlen(codeBuffer) > 0 && strlen(nameBuffer) > 0) {
                Kosgu newKosgu;
                newKosgu.code = codeBuffer;
                newKosgu.name = nameBuffer;
                newKosgu.note = noteBuffer;
                
                if (dbManager_ && dbManager_->createKosgu(newKosgu)) {
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
    if (ImGui::BeginPopupModal("Редактировать запись КОСГУ", &isEditing_, ImGuiWindowFlags_AlwaysAutoResize)) {
        char codeBuffer[64] = "";
        char nameBuffer[256] = "";
        char noteBuffer[512] = "";
        
        strncpy(codeBuffer, currentItem_.code.c_str(), sizeof(codeBuffer) - 1);
        strncpy(nameBuffer, currentItem_.name.c_str(), sizeof(nameBuffer) - 1);
        strncpy(noteBuffer, currentItem_.note.c_str(), sizeof(noteBuffer) - 1);
        
        ImGui::InputText("Код*", codeBuffer, sizeof(codeBuffer));
        ImGui::InputText("Наименование*", nameBuffer, sizeof(nameBuffer));
        ImGui::InputTextMultiline("Примечание", noteBuffer, sizeof(noteBuffer), ImVec2(400, 100));
        
        ImGui::Separator();
        
        if (ImGui::Button("Сохранить", ImVec2(100, 0))) {
            if (strlen(codeBuffer) > 0 && strlen(nameBuffer) > 0) {
                currentItem_.code = codeBuffer;
                currentItem_.name = nameBuffer;
                currentItem_.note = noteBuffer;
                
                if (dbManager_ && dbManager_->updateKosgu(currentItem_)) {
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
        ImGui::Text("Вы действительно хотите удалить эту запись КОСГУ?");
        ImGui::Separator();
        
        if (ImGui::Button("Удалить", ImVec2(100, 0))) {
            if (dbManager_ && dbManager_->deleteKosgu(deleteItemId_)) {
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
    
    ImGui::End();
}

void KosguView::refreshData() {
    if (!dbManager_) return;
    
    if (filterText_.empty()) {
        kosguList_ = dbManager_->getAllKosgu();
    } else {
        kosguList_ = dbManager_->searchKosgu(filterText_);
    }
    
    sortData();
    needRefresh_ = false;
}

void KosguView::renderToolbar() {
    // Поиск
    ImGui::SetNextItemWidth(200);
    if (CustomWidgets::InputText("##filter", filterText_, ImGuiInputTextFlags_EnterReturnsTrue)) {
        needRefresh_ = true;
    }
    ImGui::SameLine();
    
    // Кнопка создания
    if (CustomWidgets::IconButton(ICON_FA_PLUS " Создать", "Создать новую запись")) {
        currentItem_ = Kosgu{};
        isCreating_ = true;
    }
    ImGui::SameLine();
    
    // Кнопка обновления
    if (CustomWidgets::IconButton(ICON_FA_SEARCH, "Обновить")) {
        needRefresh_ = true;
    }
    ImGui::SameLine();
    
    // Экспорт в PDF
    if (CustomWidgets::IconButton(ICON_FA_FILE_PDF, "Экспорт в PDF")) {
        if (pdfReporter_) {
            pdfReporter_->generateKosguReport("kosgu_report.pdf");
        }
    }
    ImGui::SameLine();
    
    // Экспорт в CSV
    if (CustomWidgets::IconButton(ICON_FA_FILE_CSV, "Экспорт в CSV")) {
        // TODO: Экспорт в CSV
    }
    
    ImGui::SameLine();
    ImGui::Text("Записей: %zu", kosguList_.size());
}

void KosguView::renderTable() {
    if (ImGui::BeginTable("KosguTable", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | 
                                           ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Код", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Наименование", ImGuiTableColumnFlags_WidthStretch, 0);
        ImGui::TableSetupColumn("Примечание", ImGuiTableColumnFlags_WidthFixed, 200);
        
        ImGui::TableSetupScrollFreeze(0, 1);
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
        
        for (const auto& kosgu : kosguList_) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", kosgu.id);
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", kosgu.code.c_str());
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", kosgu.name.c_str());
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", kosgu.note.c_str());
            
            // Контекстное меню
            if (ImGui::BeginPopupContextItem("KosguContext")) {
                if (ImGui::MenuItem("Редактировать")) {
                    currentItem_ = kosgu;
                    isEditing_ = true;
                }
                if (ImGui::MenuItem("Удалить")) {
                    deleteItemId_ = kosgu.id;
                    showDeleteConfirm_ = true;
                }
                ImGui::EndPopup();
            }
        }
        
        ImGui::EndTable();
    }
}

void KosguView::renderImportExport() {
    // Панель импорта/экспорта (свёрнутая по умолчанию)
    if (ImGui::CollapsingHeader("Импорт/Экспорт")) {
        if (ImGui::Button("Импорт из CSV")) {
            // TODO: Импорт
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Экспорт в CSV")) {
            // TODO: Экспорт
        }
    }
}

void KosguView::sortData() {
    if (sortColumn_ < 0) return;
    
    std::sort(kosguList_.begin(), kosguList_.end(), [this](const Kosgu& a, const Kosgu& b) {
        bool result = false;
        
        switch (sortColumn_) {
            case 0: result = a.id < b.id; break;
            case 1: result = a.code < b.code; break;
            case 2: result = a.name < b.name; break;
            case 3: result = a.note < b.note; break;
        }
        
        return sortAscending_ ? result : !result;
    });
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> KosguView::GetDataAsStrings() {
    std::vector<std::string> columns = {"ID", "Code", "Name", "Note"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& k : kosguList_) {
        rows.push_back({
            std::to_string(k.id),
            k.code,
            k.name,
            k.note
        });
    }
    
    return {columns, rows};
}

} // namespace FinancialAudit
