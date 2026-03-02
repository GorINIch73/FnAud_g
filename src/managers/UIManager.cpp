/**
 * @file UIManager.cpp
 * @brief Реализация менеджера пользовательского интерфейса
 */

#define IMGUI_DEFINE_MATH_OPERATORS

#include "UIManager.h"

#include <imgui_internal.h>
#include <filesystem>
#include <iostream>

#include "DatabaseManager.h"
#include "PdfReporter.h"
#include "ExportManager.h"
#include "views/BaseView.h"
#include "views/KosguView.h"
#include "views/PaymentsView.h"
#include "views/CounterpartiesView.h"
#include "views/ContractsView.h"
#include "views/InvoicesView.h"
#include "views/SqlQueryView.h"
#include "views/SettingsView.h"
#include "views/ImportMapView.h"
#include "views/RegexesView.h"
#include "views/SuspiciousWordsView.h"
#include "views/SelectiveCleanView.h"
#include "views/SpecialQueryView.h"

namespace fs = std::filesystem;

namespace FinancialAudit {

UIManager::UIManager(DatabaseManager* dbManager)
    : dbManager_(dbManager)
{
}

UIManager::~UIManager() = default;

void UIManager::render() {
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    
    // Создаем докспейс
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    
    ImGui::End();
    
    // Рендеринг всех открытых представлений и проверка закрытия
    std::vector<std::string> viewsToClose;
    for (auto& [name, view] : views_) {
        if (viewOpenFlags_[name]) {
            renderView(name, view.get());
            // Проверяем, не закрыл ли пользователь окно крестиком
            if (!view->IsOpen()) {
                viewsToClose.push_back(name);
            }
        }
    }
    
    // Закрываем окна, которые пользователь закрыл крестиком
    for (const auto& name : viewsToClose) {
        closeView(name);
    }
    
    // Обработка диалогов ImGuiFileDialog
    handleFileDialogResults();
}

void UIManager::renderView(const std::string& /* name */, BaseView* view) {
    if (!view) return;
    view->Render();
}

void UIManager::handleFileDialogResults() {
    // Диалог создания новой БД - только если активен
    if (newDbDialogActive_) {
        std::cout << "[DEBUG] NewDbDialog: Display() called, active=" << newDbDialogActive_ << std::endl;
        if (ImGuiFileDialog::Instance()->Display("NewDbDialog", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
            std::cout << "[DEBUG] NewDbDialog: Display() returned true" << std::endl;
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::cout << "[DEBUG] NewDbDialog: IsOk()=true" << std::endl;
                std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
                std::cout << "Creating database: " << filePath << std::endl;
                if (dbManager_->createDatabase(filePath)) {
                    currentDbPath_ = filePath;
                    std::cout << "Database created successfully" << std::endl;
                } else {
                    std::cerr << "Failed: " << dbManager_->getLastError() << std::endl;
                }
            } else {
                std::cout << "[DEBUG] NewDbDialog: IsOk()=false (cancelled)" << std::endl;
            }
            newDbDialogActive_ = false;
            std::cout << "[DEBUG] NewDbDialog: Closing and setting active=false" << std::endl;
            ImGuiFileDialog::Instance()->Close();
        }
    }
    
    // Диалог открытия БД - только если активен
    if (openDbDialogActive_) {
        if (ImGuiFileDialog::Instance()->Display("OpenDbDialog", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
            std::cout << "[DEBUG] OpenDbDialog: Display() returned true" << std::endl;
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::cout << "[DEBUG] OpenDbDialog: IsOk()=true" << std::endl;
                std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
                std::cout << "Opening database: " << filePath << std::endl;
                if (dbManager_->openDatabase(filePath)) {
                    currentDbPath_ = filePath;
                    std::cout << "Database opened successfully" << std::endl;
                } else {
                    std::cerr << "Failed: " << dbManager_->getLastError() << std::endl;
                }
            } else {
                std::cout << "[DEBUG] OpenDbDialog: IsOk()=false (cancelled)" << std::endl;
            }
            openDbDialogActive_ = false;
            std::cout << "[DEBUG] OpenDbDialog: Closing and setting active=false" << std::endl;
            ImGuiFileDialog::Instance()->Close();
        }
    }
    
    // Диалог сохранения БД - только если активен
    if (saveDbDialogActive_) {
        if (ImGuiFileDialog::Instance()->Display("SaveDbDialog", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
            std::cout << "[DEBUG] SaveDbDialog: Display() returned true" << std::endl;
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::cout << "[DEBUG] SaveDbDialog: IsOk()=true" << std::endl;
                std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
                std::cout << "Saving to: " << filePath << std::endl;
                if (dbManager_->backupTo(filePath)) {
                    currentDbPath_ = filePath;
                    std::cout << "Database saved successfully" << std::endl;
                } else {
                    std::cerr << "Failed: " << dbManager_->getLastError() << std::endl;
                }
            } else {
                std::cout << "[DEBUG] SaveDbDialog: IsOk()=false (cancelled)" << std::endl;
            }
            saveDbDialogActive_ = false;
            std::cout << "[DEBUG] SaveDbDialog: Closing and setting active=false" << std::endl;
            ImGuiFileDialog::Instance()->Close();
        }
    }
    
    // Диалог экспорта PDF - только если активен
    if (pdfExportDialogActive_) {
        if (ImGuiFileDialog::Instance()->Display("PdfExportDialog", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                currentCsvPath_ = ImGuiFileDialog::Instance()->GetFilePathName();
                std::cout << "PDF export: " << currentCsvPath_ << std::endl;
            }
            pdfExportDialogActive_ = false;
            ImGuiFileDialog::Instance()->Close();
        }
    }
    
    // Диалог ИКЗ - только если активен
    if (iczDialogActive_) {
        if (ImGuiFileDialog::Instance()->Display("IczDialog", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                currentCsvPath_ = ImGuiFileDialog::Instance()->GetFilePathName();
                std::cout << "ICZ: " << currentCsvPath_ << std::endl;
                
                // Определяем тип операции по расширению файла
                if (currentCsvPath_.find(".csv") != std::string::npos || 
                    currentCsvPath_.find(".tsv") != std::string::npos) {
                    // Импорт/экспорт ИКЗ
                    if (dbManager_ && dbManager_->isDatabaseOpen()) {
                        // Простая эвристика: если файл существует - импорт, иначе - экспорт
                        std::ifstream file(currentCsvPath_);
                        if (file.good()) {
                            file.close();
                            // Импорт
                            if (!dbManager_->importIczFromCsv(currentCsvPath_)) {
                                std::cerr << "Import ICZ failed: " << dbManager_->getLastError() << std::endl;
                            }
                        } else {
                            // Экспорт
                            if (!dbManager_->exportIczToCsv(currentCsvPath_)) {
                                std::cerr << "Export ICZ failed: " << dbManager_->getLastError() << std::endl;
                            }
                        }
                    }
                }
            }
            iczDialogActive_ = false;
            ImGuiFileDialog::Instance()->Close();
        }
    }

    // Диалоги импорта - только если активны
    if (importKosguDialogActive_) {
        if (ImGuiFileDialog::Instance()->Display("ImportKosguDialog", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
                std::cout << "Import KOSGU: " << filePath << std::endl;
                
                // Импорт КОСГУ из CSV
                if (dbManager_ && dbManager_->isDatabaseOpen()) {
                    std::ifstream file(filePath);
                    if (file.is_open()) {
                        std::string line;
                        bool headerSkipped = false;
                        
                        while (std::getline(file, line)) {
                            if (!headerSkipped) {
                                headerSkipped = true;
                                continue;
                            }
                            
                            // Парсинг CSV (упрощенно)
                            std::vector<std::string> fields;
                            std::stringstream ss(line);
                            std::string field;
                            while (std::getline(ss, field, ',')) {
                                fields.push_back(field);
                            }
                            
                            if (fields.size() >= 2) {
                                Kosgu kosgu;
                                kosgu.code = fields[0];
                                kosgu.name = fields[1];
                                if (fields.size() >= 3) {
                                    kosgu.note = fields[2];
                                }
                                dbManager_->createKosgu(kosgu);
                            }
                        }
                        file.close();
                    }
                }
            }
            importKosguDialogActive_ = false;
            ImGuiFileDialog::Instance()->Close();
        }
    }

    if (importSuspiciousWordsDialogActive_) {
        if (ImGuiFileDialog::Instance()->Display("ImportSuspiciousWordsDialog", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
                std::cout << "Import Suspicious Words: " << filePath << std::endl;
                
                // Импорт подозрительных слов из CSV
                if (dbManager_ && dbManager_->isDatabaseOpen()) {
                    std::ifstream file(filePath);
                    if (file.is_open()) {
                        std::string line;
                        bool headerSkipped = false;
                        
                        while (std::getline(file, line)) {
                            if (!headerSkipped) {
                                headerSkipped = true;
                                continue;
                            }
                            
                            SuspiciousWord word;
                            word.word = line;
                            dbManager_->createSuspiciousWord(word);
                        }
                        file.close();
                    }
                }
            }
            importSuspiciousWordsDialogActive_ = false;
            ImGuiFileDialog::Instance()->Close();
        }
    }

    if (importRegexesDialogActive_) {
        if (ImGuiFileDialog::Instance()->Display("ImportRegexesDialog", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
                std::cout << "Import Regexes: " << filePath << std::endl;
                
                // Импорт REGEX из CSV
                if (dbManager_ && dbManager_->isDatabaseOpen()) {
                    std::ifstream file(filePath);
                    if (file.is_open()) {
                        std::string line;
                        bool headerSkipped = false;
                        
                        while (std::getline(file, line)) {
                            if (!headerSkipped) {
                                headerSkipped = true;
                                continue;
                            }
                            
                            // Парсинг CSV (name,pattern)
                            size_t commaPos = line.find(',');
                            if (commaPos != std::string::npos) {
                                RegexEntry regex;
                                regex.name = line.substr(0, commaPos);
                                regex.pattern = line.substr(commaPos + 1);
                                dbManager_->createRegex(regex);
                            }
                        }
                        file.close();
                    }
                }
            }
            importRegexesDialogActive_ = false;
            ImGuiFileDialog::Instance()->Close();
        }
    }
}

void UIManager::showNewDatabaseDialog() {
    std::cout << "[DEBUG] showNewDatabaseDialog() called" << std::endl;
    newDbDialogActive_ = true;
    IGFD::FileDialogConfig config;
    config.path = ".";
    config.fileName = "financial_audit.db";
    config.flags = ImGuiFileDialogFlags_ConfirmOverwrite;
    std::cout << "[DEBUG] Calling OpenDialog for NewDbDialog" << std::endl;
    ImGuiFileDialog::Instance()->OpenDialog("NewDbDialog", "Создать новую базу данных",
        ".db,.sqlite,.sqlite3", config);
}

void UIManager::showOpenDatabaseDialog() {
    std::cout << "[DEBUG] showOpenDatabaseDialog() called" << std::endl;
    openDbDialogActive_ = true;
    IGFD::FileDialogConfig config;
    config.path = ".";
    config.flags = ImGuiFileDialogFlags_None;
    std::cout << "[DEBUG] Calling OpenDialog for OpenDbDialog" << std::endl;
    ImGuiFileDialog::Instance()->OpenDialog("OpenDbDialog", "Открыть базу данных",
        ".db,.sqlite,.sqlite3", config);
}

void UIManager::showSaveDatabaseDialog() {
    std::cout << "[DEBUG] showSaveDatabaseDialog() called" << std::endl;
    saveDbDialogActive_ = true;
    IGFD::FileDialogConfig config;
    config.path = ".";
    config.fileName = "backup.db";
    config.flags = ImGuiFileDialogFlags_ConfirmOverwrite;
    std::cout << "[DEBUG] Calling OpenDialog for SaveDbDialog" << std::endl;
    ImGuiFileDialog::Instance()->OpenDialog("SaveDbDialog", "Сохранить базу как...",
        ".db,.sqlite,.sqlite3", config);
}

void UIManager::showPdfExportDialog() {
    pdfExportDialogActive_ = true;
    IGFD::FileDialogConfig config;
    config.path = ".";
    config.fileName = "report.pdf";
    config.flags = ImGuiFileDialogFlags_ConfirmOverwrite;
    ImGuiFileDialog::Instance()->OpenDialog("PdfExportDialog", "Экспорт в PDF",
        ".pdf", config);
}

void UIManager::showIczImportExportDialog() {
    iczDialogActive_ = true;
    IGFD::FileDialogConfig config;
    config.path = ".";
    config.flags = ImGuiFileDialogFlags_None;
    ImGuiFileDialog::Instance()->OpenDialog("IczDialog", "Импорт/Экспорт ИКЗ",
        ".csv,.tsv", config);
}

void UIManager::showImportKosguDialog() {
    importKosguDialogActive_ = true;
    IGFD::FileDialogConfig config;
    config.path = ".";
    config.flags = ImGuiFileDialogFlags_None;
    ImGuiFileDialog::Instance()->OpenDialog("ImportKosguDialog", "Импорт КОСГУ из CSV",
        ".csv,.tsv", config);
}

void UIManager::showImportSuspiciousWordsDialog() {
    importSuspiciousWordsDialogActive_ = true;
    IGFD::FileDialogConfig config;
    config.path = ".";
    config.flags = ImGuiFileDialogFlags_None;
    ImGuiFileDialog::Instance()->OpenDialog("ImportSuspiciousWordsDialog", "Импорт подозрительных слов из CSV",
        ".csv,.tsv", config);
}

void UIManager::showImportRegexesDialog() {
    importRegexesDialogActive_ = true;
    IGFD::FileDialogConfig config;
    config.path = ".";
    config.flags = ImGuiFileDialogFlags_None;
    ImGuiFileDialog::Instance()->OpenDialog("ImportRegexesDialog", "Импорт REGEX из CSV",
        ".csv,.tsv", config);
}

void UIManager::openKosguView() {
    if (!isViewOpen("KosguView")) {
        auto view = std::make_unique<KosguView>();
        view->SetDatabaseManager(dbManager_);
        view->SetPdfReporter(pdfReporter_);
        view->SetUIManager(this);
        views_["KosguView"] = std::move(view);
        viewOpenFlags_["KosguView"] = true;
    }
}

void UIManager::openPaymentsView() {
    if (!isViewOpen("PaymentsView")) {
        auto view = std::make_unique<PaymentsView>();
        view->SetDatabaseManager(dbManager_);
        view->SetPdfReporter(pdfReporter_);
        view->SetUIManager(this);
        views_["PaymentsView"] = std::move(view);
        viewOpenFlags_["PaymentsView"] = true;
    }
}

void UIManager::openCounterpartiesView() {
    if (!isViewOpen("CounterpartiesView")) {
        auto view = std::make_unique<CounterpartiesView>();
        view->SetDatabaseManager(dbManager_);
        view->SetPdfReporter(pdfReporter_);
        view->SetUIManager(this);
        views_["CounterpartiesView"] = std::move(view);
        viewOpenFlags_["CounterpartiesView"] = true;
    }
}

void UIManager::openContractsView() {
    if (!isViewOpen("ContractsView")) {
        auto view = std::make_unique<ContractsView>();
        view->SetDatabaseManager(dbManager_);
        view->SetPdfReporter(pdfReporter_);
        view->SetUIManager(this);
        views_["ContractsView"] = std::move(view);
        viewOpenFlags_["ContractsView"] = true;
    }
}

void UIManager::openInvoicesView() {
    if (!isViewOpen("InvoicesView")) {
        auto view = std::make_unique<InvoicesView>();
        view->SetDatabaseManager(dbManager_);
        view->SetPdfReporter(pdfReporter_);
        view->SetUIManager(this);
        views_["InvoicesView"] = std::move(view);
        viewOpenFlags_["InvoicesView"] = true;
    }
}

void UIManager::openSqlQueryView() {
    if (!isViewOpen("SqlQueryView")) {
        auto view = std::make_unique<SqlQueryView>();
        view->SetDatabaseManager(dbManager_);
        view->SetPdfReporter(pdfReporter_);
        view->SetUIManager(this);
        views_["SqlQueryView"] = std::move(view);
        viewOpenFlags_["SqlQueryView"] = true;
    }
}

void UIManager::openSettingsView() {
    if (!isViewOpen("SettingsView")) {
        auto view = std::make_unique<SettingsView>();
        view->SetDatabaseManager(dbManager_);
        view->SetPdfReporter(pdfReporter_);
        view->SetUIManager(this);
        views_["SettingsView"] = std::move(view);
        viewOpenFlags_["SettingsView"] = true;
    }
}

void UIManager::openImportMapView() {
    if (!isViewOpen("ImportMapView")) {
        auto view = std::make_unique<ImportMapView>();
        view->SetDatabaseManager(dbManager_);
        view->SetPdfReporter(pdfReporter_);
        view->SetUIManager(this);
        views_["ImportMapView"] = std::move(view);
        viewOpenFlags_["ImportMapView"] = true;
    }
}

void UIManager::openRegexesView() {
    if (!isViewOpen("RegexesView")) {
        auto view = std::make_unique<RegexesView>();
        view->SetDatabaseManager(dbManager_);
        view->SetPdfReporter(pdfReporter_);
        view->SetUIManager(this);
        views_["RegexesView"] = std::move(view);
        viewOpenFlags_["RegexesView"] = true;
    }
}

void UIManager::openSuspiciousWordsView() {
    if (!isViewOpen("SuspiciousWordsView")) {
        auto view = std::make_unique<SuspiciousWordsView>();
        view->SetDatabaseManager(dbManager_);
        view->SetPdfReporter(pdfReporter_);
        view->SetUIManager(this);
        views_["SuspiciousWordsView"] = std::move(view);
        viewOpenFlags_["SuspiciousWordsView"] = true;
    }
}

void UIManager::openSelectiveCleanView() {
    if (!isViewOpen("SelectiveCleanView")) {
        auto view = std::make_unique<SelectiveCleanView>();
        view->SetDatabaseManager(dbManager_);
        view->SetPdfReporter(pdfReporter_);
        view->SetUIManager(this);
        views_["SelectiveCleanView"] = std::move(view);
        viewOpenFlags_["SelectiveCleanView"] = true;
    }
}

void UIManager::openSpecialQueryView(const std::string& title, const std::string& query) {
    if (!isViewOpen("SpecialQueryView")) {
        auto view = std::make_unique<SpecialQueryView>();
        view->SetDatabaseManager(dbManager_);
        view->SetPdfReporter(pdfReporter_);
        view->SetUIManager(this);
        
        // Инициализация запроса
        view->Initialize(title, query);
        
        views_["SpecialQueryView"] = std::move(view);
        viewOpenFlags_["SpecialQueryView"] = true;
    }
}

void UIManager::closeView(const std::string& name) {
    viewOpenFlags_[name] = false;
    views_.erase(name);
}

bool UIManager::isViewOpen(const std::string& name) const {
    auto it = viewOpenFlags_.find(name);
    return it != viewOpenFlags_.end() && it->second;
}

} // namespace FinancialAudit
