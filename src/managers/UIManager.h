/**
 * @file UIManager.h
 * @brief Менеджер пользовательского интерфейса
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

#include <imgui.h>
#include <ImGuiFileDialog.h>

class GLFWwindow;

namespace FinancialAudit {

class DatabaseManager;
class PdfReporter;
class BaseView;

/**
 * @class UIManager
 * @brief Управление окнами представлений и диалогами
 */
class UIManager {
public:
    explicit UIManager(DatabaseManager* dbManager);
    ~UIManager();

    void setPdfReporter(PdfReporter* reporter) { pdfReporter_ = reporter; }

    void render();

    // Открытие представлений
    void openKosguView();
    void openPaymentsView();
    void openCounterpartiesView();
    void openContractsView();
    void openInvoicesView();
    void openSqlQueryView();
    void openSettingsView();
    void openImportMapView();
    void openRegexesView();
    void openSuspiciousWordsView();
    void openSelectiveCleanView();
    void openSpecialQueryView(const std::string& title, const std::string& query);

    // Диалоги с использованием ImGuiFileDialog
    void showNewDatabaseDialog();
    void showOpenDatabaseDialog();
    void showSaveDatabaseDialog();
    void showPdfExportDialog();
    void showIczImportExportDialog();
    void showImportKosguDialog();
    void showImportSuspiciousWordsDialog();
    void showImportRegexesDialog();

    // Управление окнами
    void closeView(const std::string& name);
    bool isViewOpen(const std::string& name) const;
    
    // Обработка результатов диалогов ImGuiFileDialog
    void handleFileDialogResults();

private:
    DatabaseManager* dbManager_;
    PdfReporter* pdfReporter_ = nullptr;

    std::unordered_map<std::string, std::unique_ptr<BaseView>> views_;
    std::unordered_map<std::string, bool> viewOpenFlags_;

    // Буферы для путей файлов
    std::string currentDbPath_;
    std::string currentCsvPath_;
    
    // Флаги активных диалогов
    bool newDbDialogActive_ = false;
    bool openDbDialogActive_ = false;
    bool saveDbDialogActive_ = false;
    bool pdfExportDialogActive_ = false;
    bool iczDialogActive_ = false;
    bool importKosguDialogActive_ = false;
    bool importSuspiciousWordsDialogActive_ = false;
    bool importRegexesDialogActive_ = false;

    void renderView(const std::string& name, BaseView* view);
};

} // namespace FinancialAudit
