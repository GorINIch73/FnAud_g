/**
 * @file PdfReporter.h
 * @brief Генератор PDF-отчётов
 */

#pragma once

#include <string>
#include <vector>

namespace FinancialAudit {

class DatabaseManager;

/**
 * @class PdfReporter
 * @brief Генерация PDF-отчётов с использованием pdfgen.c
 */
class PdfReporter {
public:
    explicit PdfReporter(DatabaseManager* dbManager);
    ~PdfReporter();

    // Генерация отчётов
    bool generatePaymentsReport(const std::string& path, const std::string& filter = "");
    bool generateKosguReport(const std::string& path);
    bool generateContractsReport(const std::string& path);
    bool generateCounterpartiesReport(const std::string& path);
    bool generateSqlQueryReport(const std::string& path, const std::string& query);
    
    // Низкоуровневая генерация
    bool generateCustomReport(const std::string& path,
                              const std::string& title,
                              const std::vector<std::string>& columns,
                              const std::vector<std::vector<std::string>>& rows);
    
    std::string getLastError() const { return lastError_; }

private:
    DatabaseManager* dbManager_;
    std::string lastError_;
    
    // Внутренняя функция генерации PDF
    bool writePdfFile(const std::string& path,
                      const std::string& title,
                      const std::vector<std::string>& columns,
                      const std::vector<std::vector<std::string>>& rows);
};

} // namespace FinancialAudit
