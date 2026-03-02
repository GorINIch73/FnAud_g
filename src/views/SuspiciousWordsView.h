/**
 * @file SuspiciousWordsView.h
 * @brief Представление подозрительных слов
 */

#pragma once

#include "views/BaseView.h"
#include "managers/DatabaseManager.h"
#include <imgui.h>
#include <string>
#include <vector>

namespace FinancialAudit {

/**
 * @class SuspiciousWordsView
 * @brief Форма управления подозрительными словами (CRUD, экспорт/импорт CSV)
 */
class SuspiciousWordsView : public BaseView {
public:
    SuspiciousWordsView();
    ~SuspiciousWordsView() override;

    void Render() override;

private:
    std::vector<SuspiciousWord> words_;
    std::vector<SuspiciousWord> filteredWords_;
    
    SuspiciousWord currentItem_;
    int editingId_ = -1;
    int deleteItemId_ = -1;
    
    bool isCreating_ = false;
    bool isEditing_ = false;
    bool showDeleteConfirm_ = false;
    
    std::string filterWord_;
    
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
