/**
 * @file ExportManager.cpp
 * @brief Реализация менеджера экспорта
 */

#include "ExportManager.h"

#include "DatabaseManager.h"
#include <fstream>
#include <sstream>

namespace FinancialAudit {

ExportManager::ExportManager(DatabaseManager* dbManager)
    : dbManager_(dbManager)
{
}

ExportManager::~ExportManager() = default;

std::string ExportManager::escapeCsvField(const std::string& field) {
    std::string result;
    bool needsQuotes = false;
    
    for (char c : field) {
        if (c == '"' || c == ',' || c == '\n' || c == '\r') {
            needsQuotes = true;
        }
        if (c == '"') {
            result += "\"\"";
        } else {
            result += c;
        }
    }
    
    if (needsQuotes) {
        return "\"" + result + "\"";
    }
    
    return result;
}

bool ExportManager::exportDataToCsv(const std::string& path,
                                     const std::vector<std::string>& columns,
                                     const std::vector<std::vector<std::string>>& rows) {
    std::ofstream file(path);
    if (!file.is_open()) {
        lastError_ = "Cannot create file: " + path;
        return false;
    }
    
    // Заголовок
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) file << ",";
        file << escapeCsvField(columns[i]);
    }
    file << "\n";
    
    // Строки
    for (const auto& row : rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) file << ",";
            file << escapeCsvField(row[i]);
        }
        file << "\n";
    }
    
    return true;
}

bool ExportManager::exportKosguToCsv(const std::string& path) {
    if (!dbManager_) return false;
    
    std::string actualPath = path.empty() ? "kosgu_export.csv" : path;
    
    auto kosguList = dbManager_->getAllKosgu();
    
    std::vector<std::string> columns = {"id", "code", "name", "note"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& k : kosguList) {
        rows.push_back({
            std::to_string(k.id),
            k.code,
            k.name,
            k.note
        });
    }
    
    return exportDataToCsv(actualPath, columns, rows);
}

bool ExportManager::exportSuspiciousWordsToCsv(const std::string& path) {
    if (!dbManager_) return false;
    
    std::string actualPath = path.empty() ? "suspicious_words_export.csv" : path;
    
    auto words = dbManager_->getAllSuspiciousWords();
    
    std::vector<std::string> columns = {"id", "word"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& w : words) {
        rows.push_back({
            std::to_string(w.id),
            w.word
        });
    }
    
    return exportDataToCsv(actualPath, columns, rows);
}

bool ExportManager::exportRegexesToCsv(const std::string& path) {
    if (!dbManager_) return false;
    
    std::string actualPath = path.empty() ? "regexes_export.csv" : path;
    
    auto regexes = dbManager_->getAllRegexes();
    
    std::vector<std::string> columns = {"id", "name", "pattern"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& r : regexes) {
        rows.push_back({
            std::to_string(r.id),
            r.name,
            r.pattern
        });
    }
    
    return exportDataToCsv(actualPath, columns, rows);
}

bool ExportManager::exportPaymentsToCsv(const std::string& path) {
    if (!dbManager_) return false;
    
    std::string actualPath = path.empty() ? "payments_export.csv" : path;
    
    auto payments = dbManager_->getAllPayments();
    
    std::vector<std::string> columns = {"id", "date", "doc_number", "type", "amount", 
                                         "recipient", "description", "note"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& p : payments) {
        rows.push_back({
            std::to_string(p.id),
            p.date,
            p.docNumber,
            std::to_string(p.type),
            std::to_string(p.amount),
            p.recipient,
            p.description,
            p.note
        });
    }
    
    return exportDataToCsv(actualPath, columns, rows);
}

bool ExportManager::exportCounterpartiesToCsv(const std::string& path) {
    if (!dbManager_) return false;
    
    std::string actualPath = path.empty() ? "counterparties_export.csv" : path;
    
    auto cps = dbManager_->getAllCounterparties();
    
    std::vector<std::string> columns = {"id", "name", "inn", "is_contract_optional"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& c : cps) {
        rows.push_back({
            std::to_string(c.id),
            c.name,
            c.inn.value_or(""),
            c.isContractOptional ? "1" : "0"
        });
    }
    
    return exportDataToCsv(actualPath, columns, rows);
}

bool ExportManager::exportContractsToCsv(const std::string& path) {
    if (!dbManager_) return false;
    
    std::string actualPath = path.empty() ? "contracts_export.csv" : path;
    
    auto contracts = dbManager_->getAllContracts();
    
    std::vector<std::string> columns = {"id", "number", "date", "counterparty_id", 
                                         "contract_amount", "end_date", "procurement_code", 
                                         "note", "is_for_checking"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& c : contracts) {
        rows.push_back({
            std::to_string(c.id),
            c.number,
            c.date,
            c.counterpartyId.has_value() ? std::to_string(*c.counterpartyId) : "",
            std::to_string(c.contractAmount),
            c.endDate,
            c.procurementCode,
            c.note,
            c.isForChecking ? "1" : "0"
        });
    }
    
    return exportDataToCsv(actualPath, columns, rows);
}

bool ExportManager::exportInvoicesToCsv(const std::string& path) {
    if (!dbManager_) return false;
    
    std::string actualPath = path.empty() ? "invoices_export.csv" : path;
    
    auto invoices = dbManager_->getAllInvoices();
    
    std::vector<std::string> columns = {"id", "number", "date", "contract_id", "total_amount"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& i : invoices) {
        rows.push_back({
            std::to_string(i.id),
            i.number,
            i.date,
            i.contractId.has_value() ? std::to_string(*i.contractId) : "",
            std::to_string(i.totalAmount)
        });
    }
    
    return exportDataToCsv(actualPath, columns, rows);
}

bool ExportManager::exportContractsForChecking(const std::string& path) {
    if (!dbManager_) return false;
    
    std::string actualPath = path.empty() ? "contracts_for_checking.pdf" : path;
    
    // TODO: Реализовать экспорт через PdfReporter
    auto contracts = dbManager_->getContractsWithIcz();
    
    // Пока просто экспортируем в CSV
    std::vector<std::string> columns = {"number", "date", "counterparty", "amount", "icz", "note"};
    std::vector<std::vector<std::string>> rows;
    
    for (const auto& c : contracts) {
        std::string counterpartyName;
        if (c.counterpartyId) {
            auto cp = dbManager_->getCounterpartyById(*c.counterpartyId);
            if (cp) {
                counterpartyName = cp->name;
            }
        }
        
        rows.push_back({
            c.number,
            c.date,
            counterpartyName,
            std::to_string(c.contractAmount),
            c.procurementCode,
            c.note
        });
    }
    
    std::string csvPath = actualPath;
    size_t pos = csvPath.rfind('.');
    if (pos != std::string::npos) {
        csvPath = csvPath.substr(0, pos) + "_checking.csv";
    }
    
    return exportDataToCsv(csvPath, columns, rows);
}

} // namespace FinancialAudit
