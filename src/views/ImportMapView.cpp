/**
 * @file ImportMapView.cpp
 * @brief Реализация представления импорта TSV
 */

#include "ImportMapView.h"

#include <imgui_internal.h>
#include <ImGuiFileDialog.h>
#include <algorithm>
#include <iostream>
#include <cstring>

#include "managers/DatabaseManager.h"
#include "managers/ImportManager.h"
#include "widgets/CustomWidgets.h"

#define ICON_FA_UPLOAD "\xEF\x80\x93"
#define ICON_FA_PLAY "\xEF\x81\x8B"
#define ICON_FA_STOP "\xEF\x81\x8D"
#define ICON_FA_CHECK "\xEF\x80\x8C"
#define ICON_FA_TIMES "\xEF\x80\x8D"
#define ICON_FA_MAGIC "\xEF\x83\x90"
#define ICON_FA_TRASH "\xEF\x87\xB8"

namespace FinancialAudit {

ImportMapView::ImportMapView()
    : importManager_(nullptr)
    , selectedContractRegexIdx_(-1)
    , selectedKosguRegexIdx_(-1)
    , selectedInvoiceRegexIdx_(-1) {
    title_ = "Импорт из TSV";
    cancelFlag_.store(false);
}

ImportMapView::~ImportMapView() {
    cancelImport();
    if (importThread_.joinable()) {
        importThread_.join();
    }
    delete importManager_;
}

void ImportMapView::loadRegexes() {
    if (!importManager_ && dbManager_) {
        importManager_ = new ImportManager(dbManager_);
    }
    if (!importManager_) return;
    
    regexes_ = importManager_->getAllRegexes();
    
    // Находим индексы для текущих REGEX
    selectedContractRegexIdx_ = -1;
    selectedKosguRegexIdx_ = -1;
    selectedInvoiceRegexIdx_ = -1;
    
    for (size_t i = 0; i < regexes_.size(); ++i) {
        if (regexes_[i].name == "Договоры") {
            selectedContractRegexIdx_ = static_cast<int>(i);
            if (config_.contractRegex.empty()) {
                config_.contractRegex = regexes_[i].pattern;
            }
        } else if (regexes_[i].name == "КОСГУ") {
            selectedKosguRegexIdx_ = static_cast<int>(i);
            if (config_.kosguRegex.empty()) {
                config_.kosguRegex = regexes_[i].pattern;
            }
        } else if (regexes_[i].name == "Накладные") {
            selectedInvoiceRegexIdx_ = static_cast<int>(i);
            if (config_.invoiceRegex.empty()) {
                config_.invoiceRegex = regexes_[i].pattern;
            }
        }
    }
}

void ImportMapView::loadFile(const std::string& path) {
    if (!importManager_ && dbManager_) {
        importManager_ = new ImportManager(dbManager_);
    }
    if (!importManager_) return;
    
    if (importManager_->loadFile(path)) {
        filePath_ = path;
        preview_ = importManager_->getPreview(config_.previewLines);
        columnMapping_.clear();
        fileLoaded_ = true;
        statusMessage_ = "Файл загружен: " + path;
        errorMessage_.clear();
        
        // Авто-маппинг колонок
        autoMapColumns();
    } else {
        errorMessage_ = importManager_->getLastError();
        fileLoaded_ = false;
    }
}

void ImportMapView::startImport() {
    if (!importManager_ || !dbManager_ || !dbManager_->isDatabaseOpen()) {
        errorMessage_ = "База данных не открыта";
        return;
    }
    
    isImporting_ = true;
    cancelFlag_.store(false);
    importProgress_ = 0;
    importTotal_ = preview_.totalLines - 1; // Без заголовка
    
    importThread_ = std::thread([this]() {
        importManager_->setColumnMapping(columnMapping_);
        importManager_->setConfig(config_);
        
        bool success = importManager_->executeImport(&cancelFlag_, 
            [this](int current, int /* total */) {
                importProgress_ = current;
                importedCount_ = importManager_->getImportedCount();
                skippedCount_ = importManager_->getSkippedCount();
            });
        
        if (success) {
            statusMessage_ = "Импорт завершён. Записей: " + std::to_string(importedCount_);
        } else if (cancelFlag_.load()) {
            statusMessage_ = "Импорт отменён";
        } else {
            errorMessage_ = importManager_->getLastError();
        }
        
        isImporting_ = false;
    });
}

void ImportMapView::cancelImport() {
    cancelFlag_.store(true);
}

void ImportMapView::onImportComplete() {
    fileLoaded_ = false;
    filePath_.clear();
    preview_ = ImportPreview{};
    columnMapping_.clear();
}

void ImportMapView::clearMapping() {
    columnMapping_.clear();
}

void ImportMapView::autoMapColumns() {
    if (preview_.columns.empty()) return;
    
    // Попытка автоматического маппинга по именам колонок
    for (size_t i = 0; i < preview_.columns.size(); ++i) {
        std::string colName = preview_.columns[i];
        std::transform(colName.begin(), colName.end(), colName.begin(), ::tolower);
        
        if (colName.find("date") != std::string::npos || colName.find("дата") != std::string::npos) {
            columnMapping_["date"] = static_cast<int>(i);
        } else if (colName.find("doc") != std::string::npos || colName.find("номер") != std::string::npos || 
                   colName.find("document") != std::string::npos) {
            columnMapping_["doc_number"] = static_cast<int>(i);
        } else if (colName.find("amount") != std::string::npos || colName.find("сумм") != std::string::npos) {
            columnMapping_["amount"] = static_cast<int>(i);
        } else if (colName.find("recipient") != std::string::npos || colName.find("получатель") != std::string::npos) {
            columnMapping_["recipient"] = static_cast<int>(i);
        } else if (colName.find("desc") != std::string::npos || colName.find("описание") != std::string::npos ||
                   colName.find("назнач") != std::string::npos) {
            columnMapping_["description"] = static_cast<int>(i);
        } else if (colName.find("type") != std::string::npos || colName.find("тип") != std::string::npos ||
                   colName.find("вид") != std::string::npos) {
            columnMapping_["type"] = static_cast<int>(i);
        }
    }
}

void ImportMapView::renderFileSelection() {
    ImGui::Text("Выберите файл для импорта:");
    ImGui::SameLine();
    
    if (ImGui::Button(ICON_FA_UPLOAD " Выбрать файл")) {
        IGFD::FileDialogConfig config;
        config.path = ".";
        config.flags = ImGuiFileDialogFlags_None;
        ImGuiFileDialog::Instance()->OpenDialog("ImportTSVDialog", "Импорт из TSV",
            ".tsv,.csv,.txt", config);
    }
    
    if (!filePath_.empty()) {
        ImGui::SameLine();
        ImGui::Text("%s", filePath_.c_str());
    }
    
    // Обработка диалога
    if (ImGuiFileDialog::Instance()->Display("ImportTSVDialog")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
            loadFile(path);
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

void ImportMapView::renderPreview() {
    if (!fileLoaded_) return;
    
    ImGui::Separator();
    ImGui::Text("Предпросмотр (%d строк показано из %d):", 
                static_cast<int>(preview_.rows.size()), preview_.totalLines - 1);
    
    if (ImGui::BeginTable("PreviewTable", preview_.columns.size(),
        ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        
        for (const auto& col : preview_.columns) {
            ImGui::TableSetupColumn(col.c_str());
        }
        ImGui::TableHeadersRow();
        
        int displayCount = std::min(10, static_cast<int>(preview_.rows.size()));
        for (int i = 0; i < displayCount; ++i) {
            ImGui::TableNextRow();
            for (size_t j = 0; j < preview_.columns.size(); ++j) {
                ImGui::TableSetColumnIndex(static_cast<int>(j));
                if (j < preview_.rows[i].size()) {
                    ImGui::Text("%s", preview_.rows[i][j].c_str());
                }
            }
        }
        
        ImGui::EndTable();
    }
}

void ImportMapView::renderColumnMapping() {
    if (!fileLoaded_) return;
    
    ImGui::Separator();
    ImGui::Text("Сопоставление колонок:");
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_MAGIC " Авто")) {
        autoMapColumns();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_TRASH " Очистить")) {
        clearMapping();
    }
    
    if (ImGui::BeginTable("MappingTable", 2,
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        
        ImGui::TableSetupColumn("Поле БД", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Колонка файла", ImGuiTableColumnFlags_WidthStretch, 0);
        ImGui::TableHeadersRow();

        const char* fieldNames[] = {"date", "doc_number", "amount", "recipient", "description", "type"};
        const char* fieldLabels[] = {"Дата", "Номер документа", "Сумма", "Получатель", "Описание", "Тип операции"};

        for (int i = 0; i < 6; ++i) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", fieldLabels[i]);

            ImGui::TableSetColumnIndex(1);
            int currentMapping = -1;
            auto it = columnMapping_.find(fieldNames[i]);
            if (it != columnMapping_.end()) {
                currentMapping = it->second;
            }

            // Формируем список для combo
            std::vector<const char*> items;
            items.push_back("(не выбрано)");
            for (const auto& col : preview_.columns) {
                items.push_back(col.c_str());
            }

            int selected = currentMapping + 1;
            ImGui::SetNextItemWidth(300);
            if (ImGui::Combo(("##" + std::string(fieldNames[i])).c_str(), &selected,
                items.data(), static_cast<int>(items.size()))) {
                if (selected > 0) {
                    columnMapping_[fieldNames[i]] = selected - 1;
                } else {
                    columnMapping_.erase(fieldNames[i]);
                }
            }
        }

        ImGui::EndTable();
    }
}

void ImportMapView::renderConfig() {
    if (!fileLoaded_) return;

    ImGui::Separator();
    ImGui::Text("Настройки импорта:");
    
    // Загружаем REGEX при первом открытии настроек
    if (regexes_.empty() && dbManager_ && dbManager_->isDatabaseOpen()) {
        loadRegexes();
    }

    // ComboBox для выбора REGEX из базы
    ImGui::Text("REGEX из базы:");
    
    // Договоры
    ImGui::SetNextItemWidth(250);
    std::vector<const char*> contractRegexItems;
    contractRegexItems.push_back("(не выбрано)");
    for (const auto& r : regexes_) {
        contractRegexItems.push_back(r.name.c_str());
    }
    if (ImGui::Combo("##ContractRegex", &selectedContractRegexIdx_, 
        contractRegexItems.data(), static_cast<int>(contractRegexItems.size()))) {
        if (selectedContractRegexIdx_ > 0 && selectedContractRegexIdx_ - 1 < static_cast<int>(regexes_.size())) {
            config_.contractRegex = regexes_[selectedContractRegexIdx_ - 1].pattern;
        } else {
            config_.contractRegex.clear();
        }
    }
    ImGui::SameLine();
    ImGui::Text("Договоры");
    
    // КОСГУ
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20);
    ImGui::SetNextItemWidth(250);
    std::vector<const char*> kosguRegexItems;
    kosguRegexItems.push_back("(не выбрано)");
    for (const auto& r : regexes_) {
        kosguRegexItems.push_back(r.name.c_str());
    }
    if (ImGui::Combo("##KosguRegex", &selectedKosguRegexIdx_, 
        kosguRegexItems.data(), static_cast<int>(kosguRegexItems.size()))) {
        if (selectedKosguRegexIdx_ > 0 && selectedKosguRegexIdx_ - 1 < static_cast<int>(regexes_.size())) {
            config_.kosguRegex = regexes_[selectedKosguRegexIdx_ - 1].pattern;
        } else {
            config_.kosguRegex.clear();
        }
    }
    ImGui::SameLine();
    ImGui::Text("КОСГУ");
    
    // Накладные
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20);
    ImGui::SetNextItemWidth(250);
    std::vector<const char*> invoiceRegexItems;
    invoiceRegexItems.push_back("(не выбрано)");
    for (const auto& r : regexes_) {
        invoiceRegexItems.push_back(r.name.c_str());
    }
    if (ImGui::Combo("##InvoiceRegex", &selectedInvoiceRegexIdx_, 
        invoiceRegexItems.data(), static_cast<int>(invoiceRegexItems.size()))) {
        if (selectedInvoiceRegexIdx_ > 0 && selectedInvoiceRegexIdx_ - 1 < static_cast<int>(regexes_.size())) {
            config_.invoiceRegex = regexes_[selectedInvoiceRegexIdx_ - 1].pattern;
        } else {
            config_.invoiceRegex.clear();
        }
    }
    ImGui::SameLine();
    ImGui::Text("Накладные");

    ImGui::Separator();
    ImGui::Text("Ручное редактирование REGEX:");

    char contractRegexBuffer[512] = "";
    char kosguRegexBuffer[512] = "";
    char invoiceRegexBuffer[512] = "";
    char customNoteBuffer[512] = "";

    strncpy(contractRegexBuffer, config_.contractRegex.c_str(), sizeof(contractRegexBuffer) - 1);
    strncpy(kosguRegexBuffer, config_.kosguRegex.c_str(), sizeof(kosguRegexBuffer) - 1);
    strncpy(invoiceRegexBuffer, config_.invoiceRegex.c_str(), sizeof(invoiceRegexBuffer) - 1);
    strncpy(customNoteBuffer, config_.customNote.c_str(), sizeof(customNoteBuffer) - 1);

    if (ImGui::InputText("Regex для договора", contractRegexBuffer, sizeof(contractRegexBuffer))) {
        config_.contractRegex = contractRegexBuffer;
        selectedContractRegexIdx_ = -1;  // Сбрасываем выбор при ручном редактировании
    }

    if (ImGui::InputText("Regex для КОСГУ", kosguRegexBuffer, sizeof(kosguRegexBuffer))) {
        config_.kosguRegex = kosguRegexBuffer;
        selectedKosguRegexIdx_ = -1;
    }

    if (ImGui::InputText("Regex для накладной", invoiceRegexBuffer, sizeof(invoiceRegexBuffer))) {
        config_.invoiceRegex = invoiceRegexBuffer;
        selectedInvoiceRegexIdx_ = -1;
    }
    
    const char* typeOptions[] = {"Авто", "Расход (0)", "Доход (1)"};
    int typeIndex = config_.forceOperationType + 1;
    if (ImGui::Combo("Тип операции", &typeIndex, typeOptions, 3)) {
        config_.forceOperationType = typeIndex - 1;
    }
    
    if (ImGui::InputText("Примечание", customNoteBuffer, sizeof(customNoteBuffer))) {
        config_.customNote = customNoteBuffer;
    }
    
    int previewLines = config_.previewLines;
    if (ImGui::InputInt("Строк предпросмотра", &previewLines)) {
        if (previewLines < 10) previewLines = 10;
        if (previewLines > 1000) previewLines = 1000;
        config_.previewLines = previewLines;
    }
}

void ImportMapView::renderImportControls() {
    if (!fileLoaded_) return;
    
    ImGui::Separator();
    
    if (isImporting_) {
        renderProgress();
        
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_STOP " Отмена")) {
            cancelImport();
        }
    } else {
        if (ImGui::Button(ICON_FA_PLAY " Начать импорт", ImVec2(150, 0))) {
            startImport();
        }
        
        if (!statusMessage_.empty()) {
            ImGui::SameLine();
            if (errorMessage_.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            }
            ImGui::Text("%s", statusMessage_.c_str());
            ImGui::PopStyleColor();
        }
        
        if (importedCount_ > 0 || skippedCount_ > 0) {
            ImGui::Text("Импортировано: %d, Пропущено: %d", importedCount_, skippedCount_);
        }
    }
}

void ImportMapView::renderProgress() {
    float progress = importTotal_ > 0 ? static_cast<float>(importProgress_) / importTotal_ : 0.0f;
    ImGui::ProgressBar(progress, ImVec2(200, 0));
    ImGui::SameLine();
    ImGui::Text("%d/%d", importProgress_, importTotal_);
}

void ImportMapView::Render() {
    ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(title_.c_str(), &isOpen_)) {
        ImGui::End();
        return;
    }

    if (!dbManager_ || !dbManager_->isDatabaseOpen()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::Text("Ошибка: База данных не открыта. Откройте базу данных перед импортом.");
        ImGui::PopStyleColor();
        ImGui::End();
        return;
    }

    renderFileSelection();
    renderPreview();
    renderColumnMapping();
    renderConfig();
    renderImportControls();

    if (!errorMessage_.empty() && statusMessage_.find("Ошибка") != std::string::npos) {
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::Text("Ошибка: %s", errorMessage_.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::End();
}

} // namespace FinancialAudit
