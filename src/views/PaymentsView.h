/**
 * @file PaymentsView.h
 * @brief Представление платежей
 */

#pragma once

#include "views/BaseView.h"
#include "managers/DatabaseManager.h"

#include <string>
#include <vector>
#include <set>
#include <imgui.h>

namespace FinancialAudit {

/**
 * @class PaymentsView
 * @brief Представление для работы с платежами
 */
class PaymentsView : public BaseView {
public:
    PaymentsView();
    ~PaymentsView() override = default;

    void Render() override;
    
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override;

private:
    std::vector<Payment> payments_;
    std::string filterText_;
    std::string filterDateFrom_;
    std::string filterDateTo_;
    std::string filterAmountFrom_;
    std::string filterAmountTo_;
    int filterType_ = -1;  // -1 = все, 0 = расход, 1 = доход
    bool needRefresh_ = true;
    
    // Для редактирования/создания
    bool isEditing_ = false;
    bool isCreating_ = false;
    Payment currentItem_;
    
    // Для групповых операций
    std::set<int> selectedIds_;
    bool showGroupOperation_ = false;
    std::string groupOperationType_;  // "update" или "delete"
    
    // Для подтверждения удаления
    bool showDeleteConfirm_ = false;
    int deleteItemId_ = -1;
    
    // Сортировка
    int sortColumn_ = -1;
    bool sortAscending_ = true;
    
    // Развёрнутые детали
    std::set<int> expandedPaymentIds_;
    
    void refreshData();
    void renderToolbar();
    void renderFilters();
    void renderTable();
    void renderEditDialog();
    void renderCreateDialog();
    void renderDeleteConfirm();
    void renderGroupOperationDialog();
    
    void sortData();
    void toggleSelection(int id);
    void selectAll();
    void clearSelection();
};

} // namespace FinancialAudit
