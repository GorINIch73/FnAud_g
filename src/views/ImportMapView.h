/**
 * @file ImportMapView.h
 * @brief Представление импорта TSV
 */

#pragma once

#include "views/BaseView.h"
#include "managers/ImportManager.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <thread>

namespace FinancialAudit {

/**
 * @class ImportMapView
 * @brief Форма импорта данных из TSV файлов с предпросмотром и маппингом колонок
 */
class ImportMapView : public BaseView {
public:
    ImportMapView();
    ~ImportMapView() override;

    void Render() override;

private:
    ImportManager* importManager_;
    
    std::string filePath_;
    ImportPreview preview_;
    ImportConfig config_;
    std::map<std::string, int> columnMapping_;
    
    bool fileLoaded_ = false;
    bool isImporting_ = false;
    int importProgress_ = 0;
    int importTotal_ = 0;
    int importedCount_ = 0;
    int skippedCount_ = 0;
    
    std::atomic<bool> cancelFlag_;
    std::thread importThread_;
    
    std::string statusMessage_;
    std::string errorMessage_;

    void loadFile(const std::string& path);
    void startImport();
    void cancelImport();
    void onImportComplete();
    
    void renderFileSelection();
    void renderPreview();
    void renderColumnMapping();
    void renderConfig();
    void renderImportControls();
    void renderProgress();

    void clearMapping();
    void autoMapColumns();
    void loadRegexes();
    
    std::vector<RegexEntry> regexes_;
    int selectedContractRegexIdx_;
    int selectedKosguRegexIdx_;
    int selectedInvoiceRegexIdx_;
};

} // namespace FinancialAudit
