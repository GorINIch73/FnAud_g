/**
 * @file App.cpp
 * @brief Реализация главного класса приложения
 */

#include "App.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <cstdlib>

// Иконки Font Awesome 6 (используем Unicode символы)
#define ICON_FA_FILE "\xEF\x87\x9C"
#define ICON_FA_BOOK "\xEF\x80\xAD"
#define ICON_FA_TABLE "\xEF\x80\x8E"
#define ICON_FA_USERS "\xEF\x83\x80"
#define ICON_FA_FILE_CONTRACT "\xEF\x95\x9C"
#define ICON_FA_TRUCK "\xEF\x91\x91"
#define ICON_FA_CHART_BAR "\xEF\x82\x80"
#define ICON_FA_FILE_PDF "\xEF\x87\x81"
#define ICON_FA_COG "\xEF\x80\x93"
#define ICON_FA_DOWNLOAD "\xEF\x80\x99"
#define ICON_FA_UPLOAD "\xEF\x80\x93"
#define ICON_FA_TRASH "\xEF\x87\xB8"
#define ICON_FA_SEARCH "\xEF\x80\x82"
#define ICON_FA_EXCLAMATION_TRIANGLE "\xEF\x81\xB1"

namespace FinancialAudit {

App::App(GLFWwindow* window)
    : window_(window)
    , dbManager_()
    , uiManager_(&dbManager_)
    , importManager_(&dbManager_)
    , exportManager_(&dbManager_)
    , pdfReporter_(&dbManager_)
{
    uiManager_.setPdfReporter(&pdfReporter_);
}

App::~App() = default;

bool App::initialize() {
    // Инициализация менеджеров
    if (!dbManager_.initialize()) {
        return false;
    }
    
    // Загрузка настроек
    dbManager_.loadSettings();
    
    // Применение настроек темы
    int theme = dbManager_.getSetting("theme", 0);
    switch (theme) {
        case 0: ImGui::StyleColorsDark(); break;
        case 1: ImGui::StyleColorsLight(); break;
        case 2: ImGui::StyleColorsClassic(); break;
    }
    
    return true;
}

void App::render() {
    // Главное докспейс
    ImGuiViewport* viewport = ImGui::GetMainViewport();
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
    
    ImGui::Begin("MainWindow", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    
    // Рендеринг главного меню
    renderMainMenu();
    
    // Рендеринг представлений через UIManager
    uiManager_.render();
    
    // Рендеринг статус бара
    renderStatusBar();
    
    ImGui::End();
}

void App::renderMainMenu() {
    if (ImGui::BeginMenuBar()) {
        // Меню "Файл"
        if (ImGui::BeginMenu(ICON_FA_FILE " Файл")) {
            handleFileMenu();
            ImGui::EndMenu();
        }
        
        // Меню "Справочники"
        if (ImGui::BeginMenu(ICON_FA_BOOK " Справочники")) {
            handleDirectoriesMenu();
            ImGui::EndMenu();
        }
        
        // Меню "Отчёты"
        if (ImGui::BeginMenu(ICON_FA_CHART_BAR " Отчёты")) {
            handleReportsMenu();
            ImGui::EndMenu();
        }
        
        // Меню "Сервис"
        if (ImGui::BeginMenu(ICON_FA_COG " Сервис")) {
            handleServiceMenu();
            ImGui::EndMenu();
        }
        
        ImGui::EndMenuBar();
    }
}

void App::handleFileMenu() {
    if (ImGui::MenuItem(ICON_FA_FILE " Создать новую базу", "Ctrl+N")) {
        uiManager_.showNewDatabaseDialog();
    }
    
    if (ImGui::MenuItem(ICON_FA_FILE " Открыть базу данных", "Ctrl+O")) {
        uiManager_.showOpenDatabaseDialog();
    }
    
    if (ImGui::MenuItem("Сохранить базу как...", "Ctrl+S")) {
        uiManager_.showSaveDatabaseDialog();
    }
    
    ImGui::Separator();
    
    // Недавние файлы
    const auto& recentFiles = dbManager_.getRecentFiles();
    if (!recentFiles.empty()) {
        ImGui::Text("Недавние файлы:");
        for (const auto& file : recentFiles) {
            if (ImGui::MenuItem(file.c_str())) {
                dbManager_.openDatabase(file);
            }
        }
        ImGui::Separator();
    }
    
    if (ImGui::MenuItem("Выход", "Alt+F4")) {
        glfwSetWindowShouldClose(window_, true);
    }
}

void App::handleDirectoriesMenu() {
    if (ImGui::MenuItem(ICON_FA_TABLE " КОСГУ")) {
        uiManager_.openKosguView();
    }
    
    if (ImGui::MenuItem(ICON_FA_TABLE " Банк (Платежи)")) {
        uiManager_.openPaymentsView();
    }
    
    if (ImGui::MenuItem(ICON_FA_USERS " Контрагенты")) {
        uiManager_.openCounterpartiesView();
    }
    
    if (ImGui::MenuItem(ICON_FA_FILE_CONTRACT " Договоры")) {
        uiManager_.openContractsView();
    }
    
    if (ImGui::MenuItem(ICON_FA_TRUCK " Накладные")) {
        uiManager_.openInvoicesView();
    }
}

void App::handleReportsMenu() {
    if (ImGui::MenuItem(ICON_FA_SEARCH " SQL Запрос")) {
        uiManager_.openSqlQueryView();
    }
    
    if (ImGui::MenuItem(ICON_FA_FILE_PDF " Экспорт в PDF")) {
        uiManager_.showPdfExportDialog();
    }
    
    if (ImGui::MenuItem("Договоры для проверки (PDF)")) {
        exportManager_.exportContractsForChecking();
    }
}

void App::handleServiceMenu() {
    if (ImGui::MenuItem(ICON_FA_DOWNLOAD " Импорт из TSV")) {
        uiManager_.openImportMapView();
    }
    
    ImGui::Separator();
    
    if (ImGui::MenuItem("Экспорт/Импорт ИКЗ")) {
        uiManager_.showIczImportExportDialog();
    }
    
    ImGui::Separator();
    
    if (ImGui::MenuItem("Экспорт справочников")) {
        // Подменю экспорта
        if (ImGui::BeginMenu("Экспорт")) {
            if (ImGui::MenuItem("КОСГУ в CSV")) {
                exportManager_.exportKosguToCsv();
            }
            if (ImGui::MenuItem("Подозрительные слова в CSV")) {
                exportManager_.exportSuspiciousWordsToCsv();
            }
            if (ImGui::MenuItem("REGEX выражения в CSV")) {
                exportManager_.exportRegexesToCsv();
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Импорт")) {
            if (ImGui::MenuItem("КОСГУ из CSV")) {
                uiManager_.showImportKosguDialog();
            }
            if (ImGui::MenuItem("Подозрительные слова из CSV")) {
                uiManager_.showImportSuspiciousWordsDialog();
            }
            if (ImGui::MenuItem("REGEX выражения из CSV")) {
                uiManager_.showImportRegexesDialog();
            }
            ImGui::EndMenu();
        }
    }
    
    ImGui::Separator();
    
    if (ImGui::MenuItem(ICON_FA_TABLE " Регулярные выражения")) {
        uiManager_.openRegexesView();
    }
    
    if (ImGui::MenuItem(ICON_FA_EXCLAMATION_TRIANGLE " Подозрительные слова")) {
        uiManager_.openSuspiciousWordsView();
    }
    
    if (ImGui::MenuItem(ICON_FA_TRASH " Очистка базы")) {
        uiManager_.openSelectiveCleanView();
    }
    
    ImGui::Separator();
    
    if (ImGui::MenuItem(ICON_FA_COG " Настройки")) {
        uiManager_.openSettingsView();
    }
}

void App::renderStatusBar() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - 30));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, 30));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | 
                             ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 30));
    
    if (ImGui::Begin("StatusBar", nullptr, flags)) {
        ImGui::PopStyleVar(2);
        
        if (dbManager_.isDatabaseOpen()) {
            ImGui::Text("БД: %s", dbManager_.getDatabasePath().c_str());
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            ImGui::Text("Статус: подключено");
        } else {
            ImGui::Text("БД не открыта");
        }
        
        // Панель ошибок (скрытая по умолчанию)
        const std::string& lastError = dbManager_.getLastError();
        if (!lastError.empty()) {
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::Text("Ошибка: %s", lastError.c_str());
            ImGui::PopStyleColor();
        }
    } else {
        ImGui::PopStyleVar(2);
    }
    ImGui::End();
}

void App::shutdown() {
    dbManager_.closeDatabase();
}

} // namespace FinancialAudit
