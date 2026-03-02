/**
 * @file RegexesView.h
 * @brief Представление регулярных выражений
 */

#pragma once

#include "views/BaseView.h"
#include "managers/DatabaseManager.h"
#include <imgui.h>
#include <string>
#include <vector>

namespace FinancialAudit {

/**
 * @class RegexesView
 * @brief Форма управления регулярными выражениями (CRUD, экспорт/импорт CSV)
 */
class RegexesView : public BaseView {
public:
    RegexesView();
    ~RegexesView() override;

    void Render() override;

private:
    std::vector<RegexEntry> regexes_;
    std::vector<RegexEntry> filteredRegexes_;
    
    RegexEntry currentItem_;
    int editingId_ = -1;
    int deleteItemId_ = -1;
    
    bool isCreating_ = false;
    bool isEditing_ = false;
    bool showDeleteConfirm_ = false;
    
    std::string filterName_;
    std::string filterPattern_;
    
    bool needRefresh_ = true;
    
    int sortColumn_ = -1;
    bool sortAscending_ = true;

    void refreshData();
    void applyFilter();
    void sortData();
    
    void renderFilters();
    void renderToolbar();
    void renderTable();
    
    void clearFilters();
    
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override;
};

} // namespace FinancialAudit
