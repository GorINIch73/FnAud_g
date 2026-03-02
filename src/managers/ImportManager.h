/**
 * @file ImportManager.h
 * @brief Менеджер импорта данных из TSV
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <functional>

namespace FinancialAudit {

class DatabaseManager;
struct RegexEntry;

/**
 * @struct ImportConfig
 * @brief Конфигурация импорта
 */
struct ImportConfig {
    std::string contractRegex;      // Regex для извлечения номера договора
    std::string kosguRegex;         // Regex для извлечения КОСГУ
    std::string invoiceRegex;       // Regex для извлечения накладной
    int forceOperationType = -1;    // -1 = авто, 0 = расход, 1 = доход
    std::string customNote;         // Пользовательское примечание
    int previewLines = 100;         // Количество строк предпросмотра
};

/**
 * @struct ImportPreview
 * @brief Данные для предпросмотра импорта
 */
struct ImportPreview {
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    int totalLines = 0;
};

/**
 * @class ImportManager
 * @brief Управление импортом данных из TSV файлов
 */
class ImportManager {
public:
    explicit ImportManager(DatabaseManager* dbManager);
    ~ImportManager();

    // Загрузка и предпросмотр
    bool loadFile(const std::string& path);
    ImportPreview getPreview(int lines = 100);
    
    // Маппинг колонок
    void setColumnMapping(const std::map<std::string, int>& mapping) { columnMapping_ = mapping; }
    std::map<std::string, int> getColumnMapping() const { return columnMapping_; }
    
    // Конфигурация
    void setConfig(const ImportConfig& config) { config_ = config; }
    ImportConfig getConfig() const { return config_; }
    
    // Выполнение импорта
    bool executeImport(std::atomic<bool>* cancelFlag = nullptr,
                       std::function<void(int, int)> progressCallback = nullptr);
    
    // Статистика
    int getImportedCount() const { return importedCount_; }
    int getSkippedCount() const { return skippedCount_; }
    std::string getLastError() const { return lastError_; }

    // Получение списка REGEX из базы
    std::vector<RegexEntry> getAllRegexes() const;

private:
    DatabaseManager* dbManager_;
    std::string filePath_;
    std::vector<std::string> rawLines_;
    std::map<std::string, int> columnMapping_;
    ImportConfig config_;
    
    int importedCount_ = 0;
    int skippedCount_ = 0;
    std::string lastError_;
    
    std::vector<std::string> parseLine(const std::string& line);
    std::string extractContractNumber(const std::string& description);
    std::string extractKosguCode(const std::string& description);
    std::string extractInvoiceNumber(const std::string& description);
    
    int findOrCreateCounterparty(const std::string& name, const std::string& inn = "");
    int findOrCreateContract(const std::string& number, const std::string& date, int counterpartyId);
    int findOrCreateKosgu(const std::string& code);
};

} // namespace FinancialAudit
