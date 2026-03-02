/**
 * @file DatabaseManager.h
 * @brief Менеджер для работы с базой данных SQLite3
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <mutex>
#include <atomic>

struct sqlite3;
struct sqlite3_stmt;

namespace FinancialAudit {

/**
 * @struct PaymentDetail
 * @brief Детали платежа (связь с КОСГУ, договорами, накладными)
 */
struct PaymentDetail {
    int id = 0;
    int paymentId = 0;
    std::optional<int> kosguId;
    std::optional<int> contractId;
    std::optional<int> invoiceId;
    double amount = 0.0;
};

/**
 * @struct Payment
 * @brief Платёж
 */
struct Payment {
    int id = 0;
    std::string date;
    std::string docNumber;
    int type = 0;  // 0 - расход, 1 - доход
    double amount = 0.0;
    std::string recipient;
    std::string description;
    std::optional<int> counterpartyId;
    std::string note;
    std::vector<PaymentDetail> details;
};

/**
 * @struct Counterparty
 * @brief Контрагент
 */
struct Counterparty {
    int id = 0;
    std::string name;
    std::optional<std::string> inn;
    bool isContractOptional = false;
};

/**
 * @struct Contract
 * @brief Договор
 */
struct Contract {
    int id = 0;
    std::string number;
    std::string date;
    std::optional<int> counterpartyId;
    double contractAmount = 0.0;
    std::string endDate;
    std::string procurementCode;  // ИКЗ
    std::string note;
    bool isForChecking = false;
    bool isForSpecialControl = false;
    bool isFound = false;
};

/**
 * @struct Invoice
 * @brief Накладная
 */
struct Invoice {
    int id = 0;
    std::string number;
    std::string date;
    std::optional<int> contractId;
    double totalAmount = 0.0;
};

/**
 * @struct Kosgu
 * @brief Запись КОСГУ
 */
struct Kosgu {
    int id = 0;
    std::string code;
    std::string name;
    std::string note;
};

/**
 * @struct RegexEntry
 * @brief Правило регулярного выражения
 */
struct RegexEntry {
    int id = 0;
    std::string name;
    std::string pattern;
};

/**
 * @struct SuspiciousWord
 * @brief Подозрительное слово
 */
struct SuspiciousWord {
    int id = 0;
    std::string word;
};

/**
 * @struct Settings
 * @brief Настройки приложения
 */
struct Settings {
    std::string organizationName;
    std::string periodStartDate;
    std::string periodEndDate;
    std::string note;
    int importPreviewLines = 100;
    int theme = 0;  // 0 - Dark, 1 - Light, 2 - Classic
    int fontSize = 16;
};

/**
 * @class DatabaseManager
 * @brief Класс для управления базой данных SQLite3
 */
class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    // Инициализация и подключение
    bool initialize();
    bool createDatabase(const std::string& path);
    bool openDatabase(const std::string& path);
    void closeDatabase();
    bool isDatabaseOpen() const { return db_ != nullptr; }
    std::string getDatabasePath() const { return dbPath_; }
    std::string getLastError() const { return lastError_; }

    // Резервное копирование
    bool backupTo(const std::string& path);

    // CRUD операции для КОСГУ
    std::vector<Kosgu> getAllKosgu();
    std::optional<Kosgu> getKosguById(int id);
    bool createKosgu(const Kosgu& kosgu);
    bool updateKosgu(const Kosgu& kosgu);
    bool deleteKosgu(int id);
    std::vector<Kosgu> searchKosgu(const std::string& query);

    // CRUD операции для платежей
    std::vector<Payment> getAllPayments();
    std::optional<Payment> getPaymentById(int id);
    bool createPayment(Payment& payment);
    bool updatePayment(const Payment& payment);
    bool deletePayment(int id);
    std::vector<Payment> searchPayments(const std::string& query);
    
    // Детали платежей
    std::vector<PaymentDetail> getPaymentDetails(int paymentId);
    bool updatePaymentDetails(int paymentId, const std::vector<PaymentDetail>& details);

    // CRUD операции для контрагентов
    std::vector<Counterparty> getAllCounterparties();
    std::optional<Counterparty> getCounterpartyById(int id);
    std::optional<Counterparty> getCounterpartyByInn(const std::string& inn);
    std::optional<Counterparty> getCounterpartyByName(const std::string& name);
    bool createCounterparty(Counterparty& counterparty);
    bool updateCounterparty(const Counterparty& counterparty);
    bool deleteCounterparty(int id);
    
    // CRUD операции для договоров
    std::vector<Contract> getAllContracts();
    std::optional<Contract> getContractById(int id);
    std::vector<Contract> getContractsWithIcz();  // С ИКЗ
    bool createContract(Contract& contract);
    bool updateContract(const Contract& contract);
    bool deleteContract(int id);
    bool transferContractDetails(int fromId, int toId);

    // CRUD операции для накладных
    std::vector<Invoice> getAllInvoices();
    std::optional<Invoice> getInvoiceById(int id);
    std::vector<Invoice> getInvoicesByContractId(int contractId);
    bool createInvoice(Invoice& invoice);
    bool updateInvoice(const Invoice& invoice);
    bool deleteInvoice(int id);

    // CRUD операции для регулярных выражений
    std::vector<RegexEntry> getAllRegexes();
    bool createRegex(const RegexEntry& regex);
    bool updateRegex(const RegexEntry& regex);
    bool deleteRegex(int id);

    // CRUD операции для подозрительных слов
    std::vector<SuspiciousWord> getAllSuspiciousWords();
    bool createSuspiciousWord(const SuspiciousWord& word);
    bool updateSuspiciousWord(const SuspiciousWord& word);
    bool deleteSuspiciousWord(int id);

    // Настройки
    Settings getSettings();
    void saveSettings(const Settings& settings);
    std::string getSetting(const std::string& key, const std::string& defaultValue = "");
    int getSetting(const std::string& key, int defaultValue);
    void loadSettings();

    // Недавние файлы
    std::vector<std::string> getRecentFiles();
    void addRecentFile(const std::string& path);

    // Групповые операции
    bool bulkUpdatePayments(const std::vector<int>& ids, const std::map<std::string, std::string>& fields);
    bool bulkDeletePayments(const std::vector<int>& ids);
    bool bulkUpdateCounterparties(const std::vector<int>& ids, const std::map<std::string, std::string>& fields);
    bool bulkUpdateContracts(const std::vector<int>& ids, const std::map<std::string, std::string>& fields);

    // Очистка
    bool clearPayments();
    bool clearCounterparties();
    bool clearContracts();
    bool clearInvoices();
    bool clearOrphanedDetails();
    bool clearRecentFiles();  // Очистка списка недавних файлов

    // Импорт/экспорт ИКЗ
    bool importIczFromCsv(const std::string& path);
    bool exportIczToCsv(const std::string& path);

    // Выполнение произвольного SQL запроса
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> executeQuery(const std::string& sql);

    // Флаги прерывания
    void setCancelFlag(std::atomic<bool>* flag) { cancelFlag_ = flag; }
    bool isCancelled() const { return cancelFlag_ && cancelFlag_->load(); }

private:
    sqlite3* db_ = nullptr;
    std::string dbPath_;
    std::string lastError_;
    std::atomic<bool>* cancelFlag_ = nullptr;
    mutable std::mutex mutex_;

    bool executeStatement(const std::string& sql, std::vector<std::string>* columns = nullptr,
                          std::vector<std::vector<std::string>>* rows = nullptr);
    std::string escapeString(const std::string& str);
    void createTables();
    void initDefaultRegexes();
    void migrateDatabase();
};

} // namespace FinancialAudit
