/**
 * @file ExportManager.h
 * @brief Менеджер экспорта данных
 */

#pragma once

#include <string>
#include <vector>

namespace FinancialAudit {

class DatabaseManager;

/**
 * @class ExportManager
 * @brief Управление экспортом данных в различные форматы
 */
class ExportManager {
public:
    explicit ExportManager(DatabaseManager* dbManager);
    ~ExportManager();

    // Экспорт в CSV
    bool exportKosguToCsv(const std::string& path = "");
    bool exportSuspiciousWordsToCsv(const std::string& path = "");
    bool exportRegexesToCsv(const std::string& path = "");
    bool exportPaymentsToCsv(const std::string& path = "");
    bool exportCounterpartiesToCsv(const std::string& path = "");
    bool exportContractsToCsv(const std::string& path = "");
    bool exportInvoicesToCsv(const std::string& path = "");
    
    // Экспорт договоров для проверки (PDF)
    bool exportContractsForChecking(const std::string& path = "");
    
    // Экспорт произвольных данных
    bool exportDataToCsv(const std::string& path,
                         const std::vector<std::string>& columns,
                         const std::vector<std::vector<std::string>>& rows);
    
    std::string getLastError() const { return lastError_; }

private:
    DatabaseManager* dbManager_;
    std::string lastError_;
    
    std::string escapeCsvField(const std::string& field);
};

} // namespace FinancialAudit
