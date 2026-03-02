/**
 * @file KosguView.h
 * @brief Представление справочника КОСГУ
 */

#pragma once

#include "views/BaseView.h"
#include "managers/DatabaseManager.h"

#include <string>
#include <vector>
#include <imgui.h>

namespace FinancialAudit {

/**
 * @class KosguView
 * @brief Представление для работы со справочником КОСГУ
 */
class KosguView : public BaseView {
public:
    KosguView();
    ~KosguView() override = default;

    void Render() override;
    
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() override;
    
    bool IsOpen() const { return isOpen_; }

private:
    std::vector<Kosgu> kosguList_;
    std::string filterText_;
    bool needRefresh_ = true;
    
    // Для редактирования/создания
    bool isEditing_ = false;
    bool isCreating_ = false;
    Kosgu currentItem_;
    
    // Для подтверждения удаления
    bool showDeleteConfirm_ = false;
    int deleteItemId_ = -1;
    
    // Сортировка
    int sortColumn_ = -1;
    bool sortAscending_ = true;
    
    void refreshData();
    void renderToolbar();
    void renderTable();
    void renderEditDialog();
    void renderCreateDialog();
    void renderDeleteConfirm();
    void renderImportExport();
    
    void sortData();
};

} // namespace FinancialAudit
