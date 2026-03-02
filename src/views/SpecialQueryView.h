/**
 * @file SpecialQueryView.h
 * @brief Представление специальных запросов (Форма анализа и экспорта)
 */

#pragma once

#include "views/BaseView.h"
#include <imgui.h>
#include <string>
#include <vector>

namespace FinancialAudit {

/**
 * @class SpecialQueryView
 * @brief Форма для отображения произвольного SELECT запроса с фильтрацией, сортировкой и экспортом в CSV
 */
class SpecialQueryView : public BaseView {
public:
    SpecialQueryView();
    ~SpecialQueryView() override;

    void Render() override;

    void SetDatabaseManager(DatabaseManager* dbManager) override;
    void SetPdfReporter(PdfReporter* reporter) override;
    void SetUIManager(UIManager* manager) override;

    // Инициализация запроса
    void Initialize(const std::string& title, const std::string& query);

    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override;

private:
    std::string title_;
    std::string query_;
    std::vector<std::string> columns_;
    std::vector<std::vector<std::string>> rows_;
    std::vector<std::string> filteredRows_;
    std::string filterText_;
    int sortColumn_ = -1;
    bool sortAscending_ = true;
    bool dataLoaded_ = false;
    std::string lastError_;

    void loadData();
    void applyFilter();
    void applySorting();
    void exportToCsv();
};

} // namespace FinancialAudit
