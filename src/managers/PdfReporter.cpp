/**
 * @file PdfReporter.cpp
 * @brief Реализация генератора PDF-отчётов
 */

#include "PdfReporter.h"

#include "DatabaseManager.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace FinancialAudit {

PdfReporter::PdfReporter(DatabaseManager* dbManager)
    : dbManager_(dbManager)
{
}

PdfReporter::~PdfReporter() = default;

bool PdfReporter::writePdfFile(const std::string& path,
                                const std::string& title,
                                const std::vector<std::string>& columns,
                                const std::vector<std::vector<std::string>>& rows) {
    // Простая реализация генерации PDF
    // В реальном проекте здесь будет использоваться pdfgen.c
    
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        lastError_ = "Cannot create PDF file: " + path;
        return false;
    }
    
    // Минимальный PDF документ (упрощённо)
    file << "%PDF-1.4\n";
    file << "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";
    file << "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n";
    file << "3 0 obj\n<< /Type /Page /Parent 2 0 R /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>\nendobj\n";
    file << "4 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n";
    
    // Генерируем содержимое страницы
    std::ostringstream content;
    content << "BT\n/F1 16 Tf 50 750 Td (" << title << ") Tj\nET\n";
    content << "BT\n/F1 10 Tf 50 720 Td (" << "Generated: " << std::time(nullptr) << ") Tj\nET\n";
    
    // Добавляем заголовки колонок
    float yPos = 680.0f;
    float xPos = 50.0f;
    
    content << "BT\n/F1 10 Tf\n";
    for (const auto& col : columns) {
        content << xPos << " " << yPos << " Td (" << col << ") Tj\n";
        xPos += 100.0f;
    }
    content << "ET\n";
    
    // Добавляем строки (максимум 20 строк для демо)
    yPos -= 30.0f;
    int rowCount = 0;
    for (const auto& row : rows) {
        if (rowCount >= 20) break;  // Ограничение для демо
        
        xPos = 50.0f;
        content << "BT\n/F1 8 Tf\n";
        for (const auto& cell : row) {
            // Экранируем специальные символы
            std::string escaped;
            for (char c : cell) {
                if (c == '(' || c == ')' || c == '\\') {
                    escaped += '\\';
                }
                escaped += c;
            }
            content << xPos << " " << yPos << " Td (" << escaped << ") Tj\n";
            xPos += 100.0f;
        }
        content << "ET\n";
        yPos -= 15.0f;
        rowCount++;
    }
    
    file << "5 0 obj\n<< /Length " << content.str().length() << " >>\nstream\n" 
         << content.str() << "\nendstream\nendobj\n";
    
    file << "xref\n0 6\n";
    file << "0000000000 65535 f \n";
    file << "0000000009 00000 n \n";
    file << "0000000058 00000 n \n";
    file << "0000000115 00000 n \n";
    file << "0000000214 00000 n \n";
    file << "0000000281 00000 n \n";
    file << "trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n";
    file << content.str().length() + 400 << "\n%%EOF\n";
    
    return true;
}

bool PdfReporter::generatePaymentsReport(const std::string& path, const std::string& filter) {
    if (!dbManager_) return false;
    
    std::vector<std::string> columns = {"ID", "Date", "Doc#", "Type", "Amount", "Recipient"};
    std::vector<std::vector<std::string>> rows;
    
    auto payments = filter.empty() ? dbManager_->getAllPayments() : dbManager_->searchPayments(filter);
    
    for (const auto& p : payments) {
        rows.push_back({
            std::to_string(p.id),
            p.date,
            p.docNumber,
            p.type == 0 ? "Расход" : "Доход",
            std::to_string(p.amount),
            p.recipient
        });
    }
    
    return writePdfFile(path, "Отчёт по платежам", columns, rows);
}

bool PdfReporter::generateKosguReport(const std::string& path) {
    if (!dbManager_) return false;
    
    auto kosguList = dbManager_->getAllKosgu();
    
    std::vector<std::string> columns = {"Code", "Name", "Note"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& k : kosguList) {
        rows.push_back({k.code, k.name, k.note});
    }
    
    return writePdfFile(path, "Справочник КОСГУ", columns, rows);
}

bool PdfReporter::generateContractsReport(const std::string& path) {
    if (!dbManager_) return false;
    
    auto contracts = dbManager_->getAllContracts();
    
    std::vector<std::string> columns = {"Number", "Date", "Amount", "ICZ", "Note"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& c : contracts) {
        rows.push_back({
            c.number,
            c.date,
            std::to_string(c.contractAmount),
            c.procurementCode,
            c.note
        });
    }
    
    return writePdfFile(path, "Реестр договоров", columns, rows);
}

bool PdfReporter::generateCounterpartiesReport(const std::string& path) {
    if (!dbManager_) return false;
    
    auto cps = dbManager_->getAllCounterparties();
    
    std::vector<std::string> columns = {"Name", "INN", "Optional Contract"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& c : cps) {
        rows.push_back({
            c.name,
            c.inn.value_or("N/A"),
            c.isContractOptional ? "Yes" : "No"
        });
    }
    
    return writePdfFile(path, "Реестр контрагентов", columns, rows);
}

bool PdfReporter::generateSqlQueryReport(const std::string& path, const std::string& query) {
    if (!dbManager_) return false;
    
    auto result = dbManager_->executeQuery(query);
    
    return writePdfFile(path, "SQL Query Result", result.first, result.second);
}

bool PdfReporter::generateCustomReport(const std::string& path,
                                        const std::string& title,
                                        const std::vector<std::string>& columns,
                                        const std::vector<std::vector<std::string>>& rows) {
    return writePdfFile(path, title, columns, rows);
}

} // namespace FinancialAudit
