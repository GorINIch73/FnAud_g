/**
 * @file CounterpartiesView.h
 * @brief Представление контрагентов
 */

#pragma once

#include "views/BaseView.h"
#include "managers/DatabaseManager.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <set>

namespace FinancialAudit {

/**
 * @class CounterpartiesView
 * @brief Форма управления контрагентами (CRUD, фильтрация, групповые операции)
 */
class CounterpartiesView : public BaseView {
public:
    CounterpartiesView();
    ~CounterpartiesView() override;

    void Render() override;

private:
    std::vector<Counterparty> counterparties_;
    std::vector<Counterparty> filteredCounterparties_;
    
    Counterparty currentItem_;
    int editingId_ = -1;
    int deleteItemId_ = -1;
    
    bool isCreating_ = false;
    bool isEditing_ = false;
    bool showDeleteConfirm_ = false;
    bool showGroupOperation_ = false;
    
    std::string filterName_;
    std::string filterInn_;
    
    std::set<int> selectedIds_;
    
    bool needRefresh_ = true;
    
    int sortColumn_ = -1;
    bool sortAscending_ = true;

    void refreshData();
    void applyFilter();
    void sortData();
    
    void renderFilters();
    void renderToolbar();
    void renderTable();
    
    void clearSelection();
    void toggleSelection(int id);
    void selectAll();
    void clearFilters();
    
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override;
};

} // namespace FinancialAudit
