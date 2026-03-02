/**
 * @file InvoicesView.h
 * @brief Представление накладных
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
 * @class InvoicesView
 * @brief Форма управления накладными (CRUD, фильтрация, привязка к договору)
 */
class InvoicesView : public BaseView {
public:
    InvoicesView();
    ~InvoicesView() override;

    void Render() override;

private:
    std::vector<Invoice> invoices_;
    std::vector<Invoice> filteredInvoices_;
    
    Invoice currentItem_;
    int editingId_ = -1;
    int deleteItemId_ = -1;
    
    bool isCreating_ = false;
    bool isEditing_ = false;
    bool showDeleteConfirm_ = false;
    
    std::string filterNumber_;
    int filterContractId_ = -1;
    
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
    void clearFilters();
    
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override;
};

} // namespace FinancialAudit
