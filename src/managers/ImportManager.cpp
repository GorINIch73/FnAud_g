/**
 * @file ImportManager.cpp
 * @brief Реализация менеджера импорта
 */

#include "ImportManager.h"

#include "DatabaseManager.h"
#include <fstream>
#include <sstream>
#include <regex>

namespace FinancialAudit {

ImportManager::ImportManager(DatabaseManager* dbManager)
    : dbManager_(dbManager)
{
}

ImportManager::~ImportManager() = default;

bool ImportManager::loadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        lastError_ = "Cannot open file: " + path;
        return false;
    }
    
    filePath_ = path;
    rawLines_.clear();
    
    std::string line;
    while (std::getline(file, line)) {
        rawLines_.push_back(line);
    }
    
    return true;
}

ImportPreview ImportManager::getPreview(int lines) {
    ImportPreview preview;
    preview.totalLines = static_cast<int>(rawLines_.size());
    
    if (rawLines_.empty()) return preview;
    
    // Парсим первую строку как заголовок
    preview.columns = parseLine(rawLines_[0]);
    
    // Получаем строки для предпросмотра
    int count = std::min(lines, static_cast<int>(rawLines_.size()) - 1);
    for (int i = 1; i <= count; ++i) {
        preview.rows.push_back(parseLine(rawLines_[i]));
    }
    
    return preview;
}

bool ImportManager::executeImport(std::atomic<bool>* cancelFlag,
                                   std::function<void(int, int)> progressCallback) {
    if (!dbManager_ || !dbManager_->isDatabaseOpen()) {
        lastError_ = "Database not open";
        return false;
    }
    
    if (rawLines_.empty()) {
        lastError_ = "No data to import";
        return false;
    }
    
    importedCount_ = 0;
    skippedCount_ = 0;
    
    dbManager_->setCancelFlag(cancelFlag);
    
    // Пропускаем заголовок
    for (size_t i = 1; i < rawLines_.size(); ++i) {
        if (cancelFlag && cancelFlag->load()) {
            lastError_ = "Import cancelled";
            return false;
        }
        
        auto fields = parseLine(rawLines_[i]);
        if (fields.empty()) {
            skippedCount_++;
            continue;
        }
        
        // Создаём платёж
        Payment payment;
        
        // Заполняем поля согласно маппингу
        for (const auto& [fieldName, colIndex] : columnMapping_) {
            if (colIndex >= 0 && colIndex < static_cast<int>(fields.size())) {
                const std::string& value = fields[colIndex];
                
                if (fieldName == "date") {
                    payment.date = value;
                } else if (fieldName == "doc_number") {
                    payment.docNumber = value;
                } else if (fieldName == "amount") {
                    try {
                        payment.amount = std::stod(value);
                    } catch (...) {
                        payment.amount = 0.0;
                    }
                } else if (fieldName == "recipient") {
                    payment.recipient = value;
                } else if (fieldName == "description") {
                    payment.description = value;
                } else if (fieldName == "type") {
                    if (config_.forceOperationType >= 0) {
                        payment.type = config_.forceOperationType;
                    } else {
                        try {
                            payment.type = std::stoi(value);
                        } catch (...) {
                            payment.type = 0;
                        }
                    }
                }
            }
        }
        
        payment.note = config_.customNote;
        
        // Извлекаем договор из описания
        std::string contractNumber = extractContractNumber(payment.description);
        if (!contractNumber.empty()) {
            int counterpartyId = findOrCreateCounterparty(payment.recipient);
            int contractId = findOrCreateContract(contractNumber, payment.date, counterpartyId);
            
            PaymentDetail detail;
            detail.paymentId = payment.id;
            detail.contractId = contractId;
            detail.amount = payment.amount;
            payment.details.push_back(detail);
        }
        
        // Извлекаем КОСГУ
        std::string kosguCode = extractKosguCode(payment.description);
        if (!kosguCode.empty()) {
            int kosguId = findOrCreateKosgu(kosguCode);
            
            // Ищем существующую деталь или создаём новую
            if (payment.details.empty()) {
                PaymentDetail detail;
                detail.paymentId = payment.id;
                detail.kosguId = kosguId;
                detail.amount = payment.amount;
                payment.details.push_back(detail);
            } else {
                payment.details[0].kosguId = kosguId;
            }
        }
        
        // Создаём платёж в БД
        if (dbManager_->createPayment(payment)) {
            importedCount_++;
            
            // Сохраняем детали
            if (!payment.details.empty()) {
                dbManager_->updatePaymentDetails(payment.id, payment.details);
            }
        } else {
            skippedCount_++;
        }
        
        if (progressCallback) {
            progressCallback(static_cast<int>(i), static_cast<int>(rawLines_.size()));
        }
    }
    
    return true;
}

std::vector<std::string> ImportManager::parseLine(const std::string& line) {
    std::vector<std::string> result;
    std::stringstream ss(line);
    std::string field;
    
    while (std::getline(ss, field, '\t')) {
        result.push_back(field);
    }
    
    return result;
}

std::string ImportManager::extractContractNumber(const std::string& description) {
    if (config_.contractRegex.empty()) return "";
    
    try {
        std::regex re(config_.contractRegex);
        std::smatch match;
        
        if (std::regex_search(description, match, re) && match.size() > 1) {
            return match[1].str();
        }
    } catch (...) {
        // Invalid regex
    }
    
    return "";
}

std::string ImportManager::extractKosguCode(const std::string& description) {
    if (config_.kosguRegex.empty()) return "";
    
    try {
        std::regex re(config_.kosguRegex);
        std::smatch match;
        
        if (std::regex_search(description, match, re) && match.size() > 1) {
            return match[1].str();
        }
    } catch (...) {
        // Invalid regex
    }
    
    return "";
}

std::string ImportManager::extractInvoiceNumber(const std::string& description) {
    if (config_.invoiceRegex.empty()) return "";
    
    try {
        std::regex re(config_.invoiceRegex);
        std::smatch match;
        
        if (std::regex_search(description, match, re) && match.size() > 1) {
            return match[1].str();
        }
    } catch (...) {
        // Invalid regex
    }
    
    return "";
}

int ImportManager::findOrCreateCounterparty(const std::string& name, const std::string& inn) {
    // Пробуем найти по ИНН
    if (!inn.empty()) {
        auto existing = dbManager_->getCounterpartyByInn(inn);
        if (existing) return existing->id;
    }
    
    // Пробуем найти по имени
    auto existing = dbManager_->getCounterpartyByName(name);
    if (existing) return existing->id;
    
    // Создаём нового контрагента
    Counterparty cp;
    cp.name = name;
    cp.inn = inn.empty() ? std::nullopt : std::optional<std::string>(inn);
    
    if (dbManager_->createCounterparty(cp)) {
        return cp.id;
    }
    
    return 0;
}

int ImportManager::findOrCreateContract(const std::string& number, const std::string& date, int counterpartyId) {
    // Ищем существующий договор
    auto contracts = dbManager_->getAllContracts();
    for (const auto& contract : contracts) {
        if (contract.number == number && contract.date == date) {
            return contract.id;
        }
    }
    
    // Создаём новый договор
    Contract contract;
    contract.number = number;
    contract.date = date;
    contract.counterpartyId = counterpartyId > 0 ? std::optional<int>(counterpartyId) : std::nullopt;
    
    if (dbManager_->createContract(contract)) {
        return contract.id;
    }
    
    return 0;
}

int ImportManager::findOrCreateKosgu(const std::string& code) {
    // Ищем существующий КОСГУ
    auto kosguList = dbManager_->getAllKosgu();
    for (const auto& kosgu : kosguList) {
        if (kosgu.code == code) {
            return kosgu.id;
        }
    }
    
    // Создаём новый КОСГУ
    Kosgu kosgu;
    kosgu.code = code;
    kosgu.name = "Импортированный КОСГУ " + code;

    if (dbManager_->createKosgu(kosgu)) {
        return kosgu.id;
    }

    return 0;
}

std::vector<RegexEntry> ImportManager::getAllRegexes() const {
    if (!dbManager_) return {};
    return dbManager_->getAllRegexes();
}

} // namespace FinancialAudit
