/**
 * @file ContractsView.h
 * @brief Представление договоров
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
 * @class ContractsView
 * @brief Форма управления договорами (CRUD, фильтрация, групповые операции, перенос деталей)
 */
class ContractsView : public BaseView {
public:
    ContractsView();
    ~ContractsView() override;

    void Render() override;

private:
    std::vector<Contract> contracts_;
    std::vector<Contract> filteredContracts_;
    
    Contract currentItem_;
    int editingId_ = -1;
    int deleteItemId_ = -1;
    
    bool isCreating_ = false;
    bool isEditing_ = false;
    bool showDeleteConfirm_ = false;
    bool showGroupOperation_ = false;
    bool showTransferDialog_ = false;
    
    std::string filterNumber_;
    std::string filterIcz_;
    bool showOnlyWithIcz_ = false;
    
    std::set<int> selectedIds_;
    int transferFromId_ = -1;
    int transferToId_ = -1;
    
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
