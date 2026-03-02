/**
 * @file App.h
 * @brief Главный класс приложения Financial Audit
 */

#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include <GLFW/glfw3.h>

#include "managers/DatabaseManager.h"
#include "managers/UIManager.h"
#include "managers/ImportManager.h"
#include "managers/ExportManager.h"
#include "managers/PdfReporter.h"

namespace FinancialAudit {

/**
 * @class App
 * @brief Главный класс приложения, координирующий все компоненты
 */
class App {
public:
    explicit App(GLFWwindow* window);
    ~App();

    bool initialize();
    void render();
    void shutdown();

    // Доступ к менеджерам
    DatabaseManager* getDatabaseManager() { return &dbManager_; }
    UIManager* getUIManager() { return &uiManager_; }
    ImportManager* getImportManager() { return &importManager_; }
    ExportManager* getExportManager() { return &exportManager_; }
    PdfReporter* getPdfReporter() { return &pdfReporter_; }

private:
    GLFWwindow* window_;
    
    DatabaseManager dbManager_;
    UIManager uiManager_;
    ImportManager importManager_;
    ExportManager exportManager_;
    PdfReporter pdfReporter_;

    bool showMainMenu_ = true;
    
    void renderMainMenu();
    void renderStatusBar();
    
    // Обработчики меню
    void handleFileMenu();
    void handleDirectoriesMenu();
    void handleReportsMenu();
    void handleServiceMenu();
};

} // namespace FinancialAudit
