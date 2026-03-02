/**
 * @file SpecialQueryView.cpp
 * @brief Реализация представления специальных запросов
 */

#include "SpecialQueryView.h"
#include "managers/DatabaseManager.h"
#include "managers/PdfReporter.h"
#include "managers/UIManager.h"
#include "managers/ExportManager.h"
#include <imgui.h>
#include <ImGuiFileDialog.h>
#include <algorithm>
#include <sstream>
#include <cstring>

namespace FinancialAudit {

SpecialQueryView::SpecialQueryView() {
    title_ = "Форма анализа и экспорта";
}

SpecialQueryView::~SpecialQueryView() = default;

void SpecialQueryView::SetDatabaseManager(DatabaseManager* dbManager) {
    dbManager_ = dbManager;
}

void SpecialQueryView::SetPdfReporter(PdfReporter* reporter) {
    pdfReporter_ = reporter;
}

void SpecialQueryView::SetUIManager(UIManager* manager) {
    uiManager_ = manager;
}

void SpecialQueryView::Initialize(const std::string& title, const std::string& query) {
    title_ = title;
    query_ = query;
    dataLoaded_ = false;
    filterText_.clear();
    sortColumn_ = -1;
    sortAscending_ = true;
}

void SpecialQueryView::loadData() {
    if (!dbManager_ || !dbManager_->isDatabaseOpen()) {
        lastError_ = "База данных не открыта";
        return;
    }

    if (query_.empty()) {
        lastError_ = "Запрос не задан";
        return;
    }

    auto result = dbManager_->executeQuery(query_);
    columns_ = result.first;
    rows_ = result.second;
    filteredRows_.clear();
    
    // Инициализируем filteredRows_ индексами строк
    for (size_t i = 0; i < rows_.size(); ++i) {
        filteredRows_.push_back(std::to_string(i));
    }

    dataLoaded_ = true;
    lastError_.clear();
}

void SpecialQueryView::applyFilter() {
    if (!dataLoaded_) return;

    filteredRows_.clear();

    if (filterText_.empty()) {
        // Нет фильтра - показываем все строки
        for (size_t i = 0; i < rows_.size(); ++i) {
            filteredRows_.push_back(std::to_string(i));
        }
        return;
    }

    // Фильтрация по всем колонкам
    for (size_t rowIndex = 0; rowIndex < rows_.size(); ++rowIndex) {
        const auto& row = rows_[rowIndex];
        bool matches = false;
        for (const auto& cell : row) {
            if (cell.find(filterText_) != std::string::npos) {
                matches = true;
                break;
            }
        }
        if (matches) {
            filteredRows_.push_back(std::to_string(rowIndex));
        }
    }
}

void SpecialQueryView::applySorting() {
    if (!dataLoaded_ || sortColumn_ < 0 || sortColumn_ >= static_cast<int>(columns_.size())) {
        return;
    }

    std::sort(filteredRows_.begin(), filteredRows_.end(),
        [this](const std::string& a, const std::string& b) {
            size_t idxA = std::stoul(a);
            size_t idxB = std::stoul(b);
            const std::string& valA = rows_[idxA][sortColumn_];
            const std::string& valB = rows_[idxB][sortColumn_];

            // Попытка числового сравнения
            double numA, numB;
            bool isNumA = std::sscanf(valA.c_str(), "%lf", &numA) == 1;
            bool isNumB = std::sscanf(valB.c_str(), "%lf", &numB) == 1;

            if (isNumA && isNumB) {
                return sortAscending_ ? (numA < numB) : (numA > numB);
            }

            // Строковое сравнение
            return sortAscending_ ? (valA < valB) : (valA > valB);
        });
}

void SpecialQueryView::exportToCsv() {
    if (!dbManager_ || !dataLoaded_) return;

    // Открываем диалог сохранения
    IGFD::FileDialogConfig config;
    config.path = ".";
    config.fileName = "export.csv";
    config.flags = ImGuiFileDialogFlags_ConfirmOverwrite;
    ImGuiFileDialog::Instance()->OpenDialog("SpecialQueryExportDialog",
        "Экспорт в CSV", ".csv", config);
}

void SpecialQueryView::Render() {
    if (!isOpen_) return;

    ImGui::SetNextWindowSize(ImVec2(900, 500), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(title_.c_str(), &isOpen_)) {
        ImGui::End();
        return;
    }

    // Загрузка данных при первом открытии
    if (!dataLoaded_ && dbManager_ && dbManager_->isDatabaseOpen()) {
        loadData();
        applyFilter();
        if (sortColumn_ >= 0) {
            applySorting();
        }
    }

    // Панель инструментов
    if (ImGui::Button("Обновить")) {
        dataLoaded_ = false;
        loadData();
        applyFilter();
    }
    ImGui::SameLine();

    if (ImGui::Button("Экспорт в CSV")) {
        exportToCsv();
    }
    ImGui::SameLine();

    // Фильтр
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20);
    ImGui::Text("Фильтр:");
    ImGui::SameLine();
    if (ImGui::InputText("##Filter", &filterText_[0], filterText_.capacity() + 1)) {
        filterText_.resize(std::strlen(filterText_.c_str()));
        applyFilter();
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        applyFilter();
    }

    // Отображение ошибки
    if (!lastError_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::Text("Ошибка: %s", lastError_.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Separator();

    // Таблица результатов
    if (dataLoaded_) {
        ImGui::Text("Строк: %zu (из %zu)", filteredRows_.size(), rows_.size());

        if (ImGui::BeginTable("QueryResults", columns_.size(),
            ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
            ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter |
            ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg)) {

            // Заголовки
            for (size_t i = 0; i < columns_.size(); ++i) {
                ImGui::TableSetupColumn(columns_[i].c_str(), ImGuiTableColumnFlags_None);
            }
            ImGui::TableHeadersRow();

            // Обработка сортировки по клику на заголовок
            if (ImGui::TableGetHoveredColumn() >= 0) {
                if (ImGui::IsMouseClicked(0)) {
                    int clickedColumn = ImGui::TableGetHoveredColumn();
                    if (sortColumn_ == clickedColumn) {
                        sortAscending_ = !sortAscending_;
                    } else {
                        sortColumn_ = clickedColumn;
                        sortAscending_ = true;
                    }
                    applySorting();
                }
            }

            // Строки данных
            for (const auto& rowIdx : filteredRows_) {
                size_t idx = std::stoul(rowIdx);
                ImGui::TableNextRow();
                for (size_t col = 0; col < columns_.size(); ++col) {
                    ImGui::TableSetColumnIndex(static_cast<int>(col));
                    if (col < rows_[idx].size()) {
                        ImGui::TextUnformatted(rows_[idx][col].c_str());
                    }
                }
            }

            ImGui::EndTable();
        }
    } else {
        ImGui::Text("Нет данных для отображения. Откройте базу данных и нажмите 'Обновить'.");
    }

    // Обработка экспорта
    if (ImGuiFileDialog::Instance()->Display("SpecialQueryExportDialog")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
            
            // Формируем данные для экспорта
            std::vector<std::vector<std::string>> exportRows;
            for (const auto& rowIdx : filteredRows_) {
                size_t idx = std::stoul(rowIdx);
                exportRows.push_back(rows_[idx]);
            }

            ExportManager exportMgr(dbManager_);
            if (!exportMgr.exportDataToCsv(filePath, columns_, exportRows)) {
                lastError_ = exportMgr.getLastError();
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::End();
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> SpecialQueryView::GetDataAsStrings() {
    return {columns_, rows_};
}

} // namespace FinancialAudit
