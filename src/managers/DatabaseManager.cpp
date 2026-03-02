/**
 * @file DatabaseManager.cpp
 * @brief Реализация менеджера базы данных
 */

#include "DatabaseManager.h"

#include <sqlite3.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace FinancialAudit {

DatabaseManager::DatabaseManager() = default;

DatabaseManager::~DatabaseManager() {
    closeDatabase();
}

bool DatabaseManager::initialize() {
    // SQLite уже инициализирована по умолчанию в новых версиях
    return true;
}

bool DatabaseManager::createDatabase(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    
    dbPath_ = path;
    addRecentFile(path);

    createTables();
    initDefaultRegexes();  // Инициализация REGEX выражений по умолчанию
    migrateDatabase();

    return true;
}

bool DatabaseManager::openDatabase(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    
    // Проверка целостности
    char* errMsg = nullptr;
    rc = sqlite3_exec(db_, "PRAGMA integrity_check;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        lastError_ = errMsg ? errMsg : "Database integrity check failed";
        sqlite3_free(errMsg);
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    
    dbPath_ = path;
    addRecentFile(path);
    loadSettings();
    
    return true;
}

void DatabaseManager::closeDatabase() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    dbPath_.clear();
}

bool DatabaseManager::backupTo(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) {
        lastError_ = "Database not open";
        return false;
    }
    
    sqlite3* backupDb;
    int rc = sqlite3_open(path.c_str(), &backupDb);
    if (rc != SQLITE_OK) {
        lastError_ = sqlite3_errmsg(backupDb);
        sqlite3_close(backupDb);
        return false;
    }
    
    sqlite3_backup* backup = sqlite3_backup_init(backupDb, "main", db_, "main");
    if (!backup) {
        lastError_ = sqlite3_errmsg(backupDb);
        sqlite3_close(backupDb);
        return false;
    }
    
    rc = sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);
    
    sqlite3_close(backupDb);
    
    if (rc != SQLITE_DONE) {
        lastError_ = "Backup failed";
        return false;
    }
    
    return true;
}

void DatabaseManager::createTables() {
    const char* tablesSql = R"(
        -- Справочник КОСГУ
        CREATE TABLE IF NOT EXISTS KOSGU (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            code TEXT NOT NULL UNIQUE,
            name TEXT NOT NULL,
            note TEXT
        );

        -- Справочник контрагентов
        CREATE TABLE IF NOT EXISTS Counterparties (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            inn TEXT UNIQUE,
            is_contract_optional INTEGER DEFAULT 0
        );

        -- Справочник договоров
        CREATE TABLE IF NOT EXISTS Contracts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            number TEXT NOT NULL,
            date TEXT NOT NULL,
            counterparty_id INTEGER,
            contract_amount REAL DEFAULT 0.0,
            end_date TEXT,
            procurement_code TEXT,
            note TEXT,
            is_for_checking INTEGER DEFAULT 0,
            is_for_special_control INTEGER DEFAULT 0,
            is_found INTEGER DEFAULT 0,
            FOREIGN KEY(counterparty_id) REFERENCES Counterparties(id)
        );

        -- Справочник платежей
        CREATE TABLE IF NOT EXISTS Payments (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            date TEXT NOT NULL,
            doc_number TEXT,
            type INTEGER NOT NULL DEFAULT 0,
            amount REAL NOT NULL,
            recipient TEXT,
            description TEXT,
            counterparty_id INTEGER,
            note TEXT,
            FOREIGN KEY(counterparty_id) REFERENCES Counterparties(id)
        );

        -- Справочник накладных
        CREATE TABLE IF NOT EXISTS Invoices (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            number TEXT NOT NULL,
            date TEXT NOT NULL,
            contract_id INTEGER,
            total_amount REAL DEFAULT 0.0,
            FOREIGN KEY(contract_id) REFERENCES Contracts(id)
        );

        -- Детали платежей
        CREATE TABLE IF NOT EXISTS PaymentDetails (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            payment_id INTEGER,
            kosgu_id INTEGER,
            contract_id INTEGER,
            invoice_id INTEGER,
            amount REAL,
            FOREIGN KEY(payment_id) REFERENCES Payments(id),
            FOREIGN KEY(kosgu_id) REFERENCES KOSGU(id),
            FOREIGN KEY(contract_id) REFERENCES Contracts(id),
            FOREIGN KEY(invoice_id) REFERENCES Invoices(id)
        );

        -- Регулярные выражения
        CREATE TABLE IF NOT EXISTS Regexes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            pattern TEXT NOT NULL
        );

        -- Подозрительные слова
        CREATE TABLE IF NOT EXISTS SuspiciousWords (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            word TEXT NOT NULL
        );

        -- Настройки
        CREATE TABLE IF NOT EXISTS Settings (
            id INTEGER PRIMARY KEY,
            organization_name TEXT,
            period_start_date TEXT,
            period_end_date TEXT,
            note TEXT,
            import_preview_lines INTEGER,
            theme INTEGER,
            font_size INTEGER
        );

        -- Недавние файлы
        CREATE TABLE IF NOT EXISTS RecentFiles (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            path TEXT NOT NULL,
            opened_at TEXT DEFAULT CURRENT_TIMESTAMP
        );
    )";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, tablesSql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        lastError_ = errMsg ? errMsg : "Failed to create tables";
        sqlite3_free(errMsg);
    }
}

void DatabaseManager::initDefaultRegexes() {
    // Проверяем, есть ли уже REGEX записи (прямой SQL запрос без захвата мьютекса)
    const char* checkSql = "SELECT COUNT(*) FROM Regexes";
    sqlite3_stmt* stmt;
    int count = 0;
    
    if (sqlite3_prepare_v2(db_, checkSql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    if (count > 0) {
        return;  // REGEX уже существуют, не добавляем
    }

    // Добавляем REGEX выражения по умолчанию согласно ТЗ (прямой SQL без захвата мьютекса)
    const char* insertSql = "INSERT INTO Regexes (name, pattern) VALUES (?, ?)";
    
    struct {
        const char* name;
        const char* pattern;
    } defaultRegexes[] = {
        {"Договоры", "(?:по контракту|по контр|Контракт|дог\\.|К-т)(?:№)?\\s*([^\\s,]+)\\s*(?:от\\s*)?(\\d{2}\\.\\d{2}\\.(?:\\d{4}|\\d{2}))"},
        {"КОСГУ", "К(\\d{3})"},
        {"Накладные", "(?:акт|сч\\.?|сч-ф|счет на оплату|№)\\s*([^\\s,]+)\\s*(?:от\\s*)?(\\d{2}\\.\\d{2}\\.(?:\\d{4}|\\d{2}))"}
    };
    
    if (sqlite3_prepare_v2(db_, insertSql, -1, &stmt, nullptr) == SQLITE_OK) {
        for (const auto& regex : defaultRegexes) {
            sqlite3_bind_text(stmt, 1, regex.name, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, regex.pattern, -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
    }
}

void DatabaseManager::migrateDatabase() {
    // Миграции для будущих версий
    // Добавление новых колонок и таблиц
}

// ==================== КОСГУ ====================

std::vector<Kosgu> DatabaseManager::getAllKosgu() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Kosgu> result;
    
    if (!db_) return result;
    
    const char* sql = "SELECT id, code, name, note FROM KOSGU ORDER BY code";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Kosgu kosgu;
            kosgu.id = sqlite3_column_int(stmt, 0);
            kosgu.code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            kosgu.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            kosgu.note = sqlite3_column_text(stmt, 3) 
                ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)) 
                : "";
            result.push_back(kosgu);
        }
        sqlite3_finalize(stmt);
    }
    
    return result;
}

std::optional<Kosgu> DatabaseManager::getKosguById(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return std::nullopt;
    
    const char* sql = "SELECT id, code, name, note FROM KOSGU WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            Kosgu kosgu;
            kosgu.id = sqlite3_column_int(stmt, 0);
            kosgu.code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            kosgu.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            kosgu.note = sqlite3_column_text(stmt, 3) 
                ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)) 
                : "";
            sqlite3_finalize(stmt);
            return kosgu;
        }
        sqlite3_finalize(stmt);
    }
    
    return std::nullopt;
}

bool DatabaseManager::createKosgu(const Kosgu& kosgu) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = "INSERT INTO KOSGU (code, name, note) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, kosgu.code.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, kosgu.name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, kosgu.note.c_str(), -1, SQLITE_STATIC);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

bool DatabaseManager::updateKosgu(const Kosgu& kosgu) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = "UPDATE KOSGU SET code = ?, name = ?, note = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, kosgu.code.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, kosgu.name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, kosgu.note.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 4, kosgu.id);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

bool DatabaseManager::deleteKosgu(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = "DELETE FROM KOSGU WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

std::vector<Kosgu> DatabaseManager::searchKosgu(const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Kosgu> result;
    
    if (!db_) return result;
    
    std::string sql = "SELECT id, code, name, note FROM KOSGU "
                      "WHERE code LIKE ? OR name LIKE ? OR note LIKE ? "
                      "ORDER BY code";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        std::string pattern = "%" + query + "%";
        sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, pattern.c_str(), -1, SQLITE_STATIC);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Kosgu kosgu;
            kosgu.id = sqlite3_column_int(stmt, 0);
            kosgu.code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            kosgu.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            kosgu.note = sqlite3_column_text(stmt, 3) 
                ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)) 
                : "";
            result.push_back(kosgu);
        }
        sqlite3_finalize(stmt);
    }
    
    return result;
}

// ==================== Платежи ====================

std::vector<Payment> DatabaseManager::getAllPayments() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Payment> result;
    
    if (!db_) return result;
    
    const char* sql = R"(
        SELECT id, date, doc_number, type, amount, recipient, description, 
               counterparty_id, note 
        FROM Payments 
        ORDER BY date DESC, id DESC
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Payment payment;
            payment.id = sqlite3_column_int(stmt, 0);
            payment.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            payment.docNumber = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            payment.type = sqlite3_column_int(stmt, 3);
            payment.amount = sqlite3_column_double(stmt, 4);
            payment.recipient = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            payment.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            payment.counterpartyId = sqlite3_column_text(stmt, 7) 
                ? std::optional<int>(sqlite3_column_int(stmt, 7)) 
                : std::nullopt;
            payment.note = sqlite3_column_text(stmt, 8) 
                ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8)) 
                : "";
            result.push_back(payment);
        }
        sqlite3_finalize(stmt);
    }
    
    return result;
}

std::optional<Payment> DatabaseManager::getPaymentById(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return std::nullopt;
    
    const char* sql = R"(
        SELECT id, date, doc_number, type, amount, recipient, description, 
               counterparty_id, note 
        FROM Payments 
        WHERE id = ?
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            Payment payment;
            payment.id = sqlite3_column_int(stmt, 0);
            payment.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            payment.docNumber = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            payment.type = sqlite3_column_int(stmt, 3);
            payment.amount = sqlite3_column_double(stmt, 4);
            payment.recipient = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            payment.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            payment.counterpartyId = sqlite3_column_text(stmt, 7) 
                ? std::optional<int>(sqlite3_column_int(stmt, 7)) 
                : std::nullopt;
            payment.note = sqlite3_column_text(stmt, 8) 
                ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8)) 
                : "";
            sqlite3_finalize(stmt);
            return payment;
        }
        sqlite3_finalize(stmt);
    }
    
    return std::nullopt;
}

bool DatabaseManager::createPayment(Payment& payment) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = R"(
        INSERT INTO Payments (date, doc_number, type, amount, recipient, description, 
                              counterparty_id, note) 
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, payment.date.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, payment.docNumber.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, payment.type);
        sqlite3_bind_double(stmt, 4, payment.amount);
        sqlite3_bind_text(stmt, 5, payment.recipient.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, payment.description.c_str(), -1, SQLITE_STATIC);
        
        if (payment.counterpartyId) {
            sqlite3_bind_int(stmt, 7, *payment.counterpartyId);
        } else {
            sqlite3_bind_null(stmt, 7);
        }
        
        sqlite3_bind_text(stmt, 8, payment.note.c_str(), -1, SQLITE_STATIC);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) {
            payment.id = static_cast<int>(sqlite3_last_insert_rowid(db_));
            return true;
        }
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

bool DatabaseManager::updatePayment(const Payment& payment) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = R"(
        UPDATE Payments 
        SET date = ?, doc_number = ?, type = ?, amount = ?, 
            recipient = ?, description = ?, counterparty_id = ?, note = ? 
        WHERE id = ?
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, payment.date.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, payment.docNumber.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, payment.type);
        sqlite3_bind_double(stmt, 4, payment.amount);
        sqlite3_bind_text(stmt, 5, payment.recipient.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, payment.description.c_str(), -1, SQLITE_STATIC);
        
        if (payment.counterpartyId) {
            sqlite3_bind_int(stmt, 7, *payment.counterpartyId);
        } else {
            sqlite3_bind_null(stmt, 7);
        }
        
        sqlite3_bind_text(stmt, 8, payment.note.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 9, payment.id);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

bool DatabaseManager::deletePayment(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    // Сначала удаляем детали
    const char* deleteDetailsSql = "DELETE FROM PaymentDetails WHERE payment_id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, deleteDetailsSql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    // Затем удаляем платёж
    const char* sql = "DELETE FROM Payments WHERE id = ?";
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

std::vector<Payment> DatabaseManager::searchPayments(const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Payment> result;
    
    if (!db_) return result;
    
    std::string sql = R"(
        SELECT id, date, doc_number, type, amount, recipient, description, 
               counterparty_id, note 
        FROM Payments 
        WHERE date LIKE ? OR doc_number LIKE ? OR recipient LIKE ? 
           OR description LIKE ? OR note LIKE ?
        ORDER BY date DESC, id DESC
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        std::string pattern = "%" + query + "%";
        sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, pattern.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, pattern.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, pattern.c_str(), -1, SQLITE_STATIC);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Payment payment;
            payment.id = sqlite3_column_int(stmt, 0);
            payment.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            payment.docNumber = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            payment.type = sqlite3_column_int(stmt, 3);
            payment.amount = sqlite3_column_double(stmt, 4);
            payment.recipient = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            payment.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            payment.counterpartyId = sqlite3_column_text(stmt, 7) 
                ? std::optional<int>(sqlite3_column_int(stmt, 7)) 
                : std::nullopt;
            payment.note = sqlite3_column_text(stmt, 8) 
                ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8)) 
                : "";
            result.push_back(payment);
        }
        sqlite3_finalize(stmt);
    }
    
    return result;
}

std::vector<PaymentDetail> DatabaseManager::getPaymentDetails(int paymentId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PaymentDetail> result;
    
    if (!db_) return result;
    
    const char* sql = R"(
        SELECT id, payment_id, kosgu_id, contract_id, invoice_id, amount 
        FROM PaymentDetails 
        WHERE payment_id = ?
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, paymentId);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            PaymentDetail detail;
            detail.id = sqlite3_column_int(stmt, 0);
            detail.paymentId = sqlite3_column_int(stmt, 1);
            detail.kosguId = sqlite3_column_text(stmt, 2) 
                ? std::optional<int>(sqlite3_column_int(stmt, 2)) 
                : std::nullopt;
            detail.contractId = sqlite3_column_text(stmt, 3) 
                ? std::optional<int>(sqlite3_column_int(stmt, 3)) 
                : std::nullopt;
            detail.invoiceId = sqlite3_column_text(stmt, 4) 
                ? std::optional<int>(sqlite3_column_int(stmt, 4)) 
                : std::nullopt;
            detail.amount = sqlite3_column_double(stmt, 5);
            result.push_back(detail);
        }
        sqlite3_finalize(stmt);
    }
    
    return result;
}

bool DatabaseManager::updatePaymentDetails(int paymentId, const std::vector<PaymentDetail>& details) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    // Начинаем транзакцию
    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    
    // Удаляем старые детали
    const char* deleteSql = "DELETE FROM PaymentDetails WHERE payment_id = ?";
    sqlite3_stmt* deleteStmt;
    
    if (sqlite3_prepare_v2(db_, deleteSql, -1, &deleteStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(deleteStmt, 1, paymentId);
        sqlite3_step(deleteStmt);
        sqlite3_finalize(deleteStmt);
    }
    
    // Вставляем новые детали
    const char* insertSql = R"(
        INSERT INTO PaymentDetails (payment_id, kosgu_id, contract_id, invoice_id, amount) 
        VALUES (?, ?, ?, ?, ?)
    )";
    sqlite3_stmt* insertStmt;
    
    bool success = true;
    if (sqlite3_prepare_v2(db_, insertSql, -1, &insertStmt, nullptr) == SQLITE_OK) {
        for (const auto& detail : details) {
            sqlite3_bind_int(insertStmt, 1, paymentId);
            
            if (detail.kosguId) {
                sqlite3_bind_int(insertStmt, 2, *detail.kosguId);
            } else {
                sqlite3_bind_null(insertStmt, 2);
            }
            
            if (detail.contractId) {
                sqlite3_bind_int(insertStmt, 3, *detail.contractId);
            } else {
                sqlite3_bind_null(insertStmt, 3);
            }
            
            if (detail.invoiceId) {
                sqlite3_bind_int(insertStmt, 4, *detail.invoiceId);
            } else {
                sqlite3_bind_null(insertStmt, 4);
            }
            
            sqlite3_bind_double(insertStmt, 5, detail.amount);
            
            if (sqlite3_step(insertStmt) != SQLITE_DONE) {
                success = false;
                break;
            }
            sqlite3_reset(insertStmt);
        }
        sqlite3_finalize(insertStmt);
    }
    
    if (success) {
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    } else {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return success;
}

// ==================== Контрагенты ====================

std::vector<Counterparty> DatabaseManager::getAllCounterparties() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Counterparty> result;
    
    if (!db_) return result;
    
    const char* sql = R"(
        SELECT id, name, inn, is_contract_optional 
        FROM Counterparties 
        ORDER BY name
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Counterparty cp;
            cp.id = sqlite3_column_int(stmt, 0);
            cp.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            cp.inn = sqlite3_column_text(stmt, 2) 
                ? std::optional<std::string>(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))) 
                : std::nullopt;
            cp.isContractOptional = sqlite3_column_int(stmt, 3) != 0;
            result.push_back(cp);
        }
        sqlite3_finalize(stmt);
    }
    
    return result;
}

std::optional<Counterparty> DatabaseManager::getCounterpartyById(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return std::nullopt;
    
    const char* sql = "SELECT id, name, inn, is_contract_optional FROM Counterparties WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            Counterparty cp;
            cp.id = sqlite3_column_int(stmt, 0);
            cp.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            cp.inn = sqlite3_column_text(stmt, 2) 
                ? std::optional<std::string>(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))) 
                : std::nullopt;
            cp.isContractOptional = sqlite3_column_int(stmt, 3) != 0;
            sqlite3_finalize(stmt);
            return cp;
        }
        sqlite3_finalize(stmt);
    }
    
    return std::nullopt;
}

std::optional<Counterparty> DatabaseManager::getCounterpartyByInn(const std::string& inn) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return std::nullopt;
    
    const char* sql = "SELECT id, name, inn, is_contract_optional FROM Counterparties WHERE inn = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, inn.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            Counterparty cp;
            cp.id = sqlite3_column_int(stmt, 0);
            cp.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            cp.inn = sqlite3_column_text(stmt, 2) 
                ? std::optional<std::string>(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))) 
                : std::nullopt;
            cp.isContractOptional = sqlite3_column_int(stmt, 3) != 0;
            sqlite3_finalize(stmt);
            return cp;
        }
        sqlite3_finalize(stmt);
    }
    
    return std::nullopt;
}

std::optional<Counterparty> DatabaseManager::getCounterpartyByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return std::nullopt;
    
    const char* sql = "SELECT id, name, inn, is_contract_optional FROM Counterparties WHERE name = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            Counterparty cp;
            cp.id = sqlite3_column_int(stmt, 0);
            cp.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            cp.inn = sqlite3_column_text(stmt, 2) 
                ? std::optional<std::string>(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))) 
                : std::nullopt;
            cp.isContractOptional = sqlite3_column_int(stmt, 3) != 0;
            sqlite3_finalize(stmt);
            return cp;
        }
        sqlite3_finalize(stmt);
    }
    
    return std::nullopt;
}

bool DatabaseManager::createCounterparty(Counterparty& counterparty) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = R"(
        INSERT INTO Counterparties (name, inn, is_contract_optional) 
        VALUES (?, ?, ?)
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, counterparty.name.c_str(), -1, SQLITE_STATIC);
        
        if (counterparty.inn) {
            sqlite3_bind_text(stmt, 2, counterparty.inn->c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 2);
        }
        
        sqlite3_bind_int(stmt, 3, counterparty.isContractOptional ? 1 : 0);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) {
            counterparty.id = static_cast<int>(sqlite3_last_insert_rowid(db_));
            return true;
        }
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

bool DatabaseManager::updateCounterparty(const Counterparty& counterparty) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = R"(
        UPDATE Counterparties 
        SET name = ?, inn = ?, is_contract_optional = ? 
        WHERE id = ?
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, counterparty.name.c_str(), -1, SQLITE_STATIC);
        
        if (counterparty.inn) {
            sqlite3_bind_text(stmt, 2, counterparty.inn->c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 2);
        }
        
        sqlite3_bind_int(stmt, 3, counterparty.isContractOptional ? 1 : 0);
        sqlite3_bind_int(stmt, 4, counterparty.id);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

bool DatabaseManager::deleteCounterparty(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = "DELETE FROM Counterparties WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

// ==================== Договоры ====================

std::vector<Contract> DatabaseManager::getAllContracts() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Contract> result;
    
    if (!db_) return result;
    
    const char* sql = R"(
        SELECT id, number, date, counterparty_id, contract_amount, end_date, 
               procurement_code, note, is_for_checking, is_for_special_control, is_found 
        FROM Contracts 
        ORDER BY date DESC, number
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Contract contract;
            contract.id = sqlite3_column_int(stmt, 0);
            contract.number = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            contract.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            contract.counterpartyId = sqlite3_column_text(stmt, 3) 
                ? std::optional<int>(sqlite3_column_int(stmt, 3)) 
                : std::nullopt;
            contract.contractAmount = sqlite3_column_double(stmt, 4);
            contract.endDate = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            contract.procurementCode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            contract.note = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            contract.isForChecking = sqlite3_column_int(stmt, 8) != 0;
            contract.isForSpecialControl = sqlite3_column_int(stmt, 9) != 0;
            contract.isFound = sqlite3_column_int(stmt, 10) != 0;
            result.push_back(contract);
        }
        sqlite3_finalize(stmt);
    }
    
    return result;
}

std::optional<Contract> DatabaseManager::getContractById(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return std::nullopt;
    
    const char* sql = R"(
        SELECT id, number, date, counterparty_id, contract_amount, end_date, 
               procurement_code, note, is_for_checking, is_for_special_control, is_found 
        FROM Contracts 
        WHERE id = ?
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            Contract contract;
            contract.id = sqlite3_column_int(stmt, 0);
            contract.number = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            contract.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            contract.counterpartyId = sqlite3_column_text(stmt, 3) 
                ? std::optional<int>(sqlite3_column_int(stmt, 3)) 
                : std::nullopt;
            contract.contractAmount = sqlite3_column_double(stmt, 4);
            contract.endDate = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            contract.procurementCode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            contract.note = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            contract.isForChecking = sqlite3_column_int(stmt, 8) != 0;
            contract.isForSpecialControl = sqlite3_column_int(stmt, 9) != 0;
            contract.isFound = sqlite3_column_int(stmt, 10) != 0;
            sqlite3_finalize(stmt);
            return contract;
        }
        sqlite3_finalize(stmt);
    }
    
    return std::nullopt;
}

std::vector<Contract> DatabaseManager::getContractsWithIcz() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Contract> result;
    
    if (!db_) return result;
    
    const char* sql = R"(
        SELECT id, number, date, counterparty_id, contract_amount, end_date, 
               procurement_code, note, is_for_checking, is_for_special_control, is_found 
        FROM Contracts 
        WHERE procurement_code IS NOT NULL AND procurement_code != '' 
        ORDER BY date DESC, number
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Contract contract;
            contract.id = sqlite3_column_int(stmt, 0);
            contract.number = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            contract.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            contract.counterpartyId = sqlite3_column_text(stmt, 3) 
                ? std::optional<int>(sqlite3_column_int(stmt, 3)) 
                : std::nullopt;
            contract.contractAmount = sqlite3_column_double(stmt, 4);
            contract.endDate = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            contract.procurementCode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            contract.note = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            contract.isForChecking = sqlite3_column_int(stmt, 8) != 0;
            contract.isForSpecialControl = sqlite3_column_int(stmt, 9) != 0;
            contract.isFound = sqlite3_column_int(stmt, 10) != 0;
            result.push_back(contract);
        }
        sqlite3_finalize(stmt);
    }
    
    return result;
}

bool DatabaseManager::createContract(Contract& contract) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = R"(
        INSERT INTO Contracts (number, date, counterparty_id, contract_amount, end_date, 
                               procurement_code, note, is_for_checking, is_for_special_control, is_found) 
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, contract.number.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, contract.date.c_str(), -1, SQLITE_STATIC);
        
        if (contract.counterpartyId) {
            sqlite3_bind_int(stmt, 3, *contract.counterpartyId);
        } else {
            sqlite3_bind_null(stmt, 3);
        }
        
        sqlite3_bind_double(stmt, 4, contract.contractAmount);
        sqlite3_bind_text(stmt, 5, contract.endDate.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, contract.procurementCode.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, contract.note.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 8, contract.isForChecking ? 1 : 0);
        sqlite3_bind_int(stmt, 9, contract.isForSpecialControl ? 1 : 0);
        sqlite3_bind_int(stmt, 10, contract.isFound ? 1 : 0);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) {
            contract.id = static_cast<int>(sqlite3_last_insert_rowid(db_));
            return true;
        }
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

bool DatabaseManager::updateContract(const Contract& contract) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = R"(
        UPDATE Contracts 
        SET number = ?, date = ?, counterparty_id = ?, contract_amount = ?, 
            end_date = ?, procurement_code = ?, note = ?, is_for_checking = ?, 
            is_for_special_control = ?, is_found = ? 
        WHERE id = ?
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, contract.number.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, contract.date.c_str(), -1, SQLITE_STATIC);
        
        if (contract.counterpartyId) {
            sqlite3_bind_int(stmt, 3, *contract.counterpartyId);
        } else {
            sqlite3_bind_null(stmt, 3);
        }
        
        sqlite3_bind_double(stmt, 4, contract.contractAmount);
        sqlite3_bind_text(stmt, 5, contract.endDate.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, contract.procurementCode.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, contract.note.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 8, contract.isForChecking ? 1 : 0);
        sqlite3_bind_int(stmt, 9, contract.isForSpecialControl ? 1 : 0);
        sqlite3_bind_int(stmt, 10, contract.isFound ? 1 : 0);
        sqlite3_bind_int(stmt, 11, contract.id);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

bool DatabaseManager::deleteContract(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = "DELETE FROM Contracts WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

bool DatabaseManager::transferContractDetails(int fromId, int toId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = "UPDATE PaymentDetails SET contract_id = ? WHERE contract_id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, toId);
        sqlite3_bind_int(stmt, 2, fromId);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

// ==================== Накладные ====================

std::vector<Invoice> DatabaseManager::getAllInvoices() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Invoice> result;
    
    if (!db_) return result;
    
    const char* sql = R"(
        SELECT id, number, date, contract_id, total_amount 
        FROM Invoices 
        ORDER BY date DESC, number
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Invoice invoice;
            invoice.id = sqlite3_column_int(stmt, 0);
            invoice.number = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            invoice.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            invoice.contractId = sqlite3_column_text(stmt, 3) 
                ? std::optional<int>(sqlite3_column_int(stmt, 3)) 
                : std::nullopt;
            invoice.totalAmount = sqlite3_column_double(stmt, 4);
            result.push_back(invoice);
        }
        sqlite3_finalize(stmt);
    }
    
    return result;
}

std::optional<Invoice> DatabaseManager::getInvoiceById(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return std::nullopt;
    
    const char* sql = "SELECT id, number, date, contract_id, total_amount FROM Invoices WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            Invoice invoice;
            invoice.id = sqlite3_column_int(stmt, 0);
            invoice.number = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            invoice.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            invoice.contractId = sqlite3_column_text(stmt, 3) 
                ? std::optional<int>(sqlite3_column_int(stmt, 3)) 
                : std::nullopt;
            invoice.totalAmount = sqlite3_column_double(stmt, 4);
            sqlite3_finalize(stmt);
            return invoice;
        }
        sqlite3_finalize(stmt);
    }
    
    return std::nullopt;
}

std::vector<Invoice> DatabaseManager::getInvoicesByContractId(int contractId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Invoice> result;
    
    if (!db_) return result;
    
    const char* sql = R"(
        SELECT id, number, date, contract_id, total_amount 
        FROM Invoices 
        WHERE contract_id = ? 
        ORDER BY date DESC, number
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, contractId);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Invoice invoice;
            invoice.id = sqlite3_column_int(stmt, 0);
            invoice.number = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            invoice.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            invoice.contractId = sqlite3_column_text(stmt, 3) 
                ? std::optional<int>(sqlite3_column_int(stmt, 3)) 
                : std::nullopt;
            invoice.totalAmount = sqlite3_column_double(stmt, 4);
            result.push_back(invoice);
        }
        sqlite3_finalize(stmt);
    }
    
    return result;
}

bool DatabaseManager::createInvoice(Invoice& invoice) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = R"(
        INSERT INTO Invoices (number, date, contract_id, total_amount) 
        VALUES (?, ?, ?, ?)
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, invoice.number.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, invoice.date.c_str(), -1, SQLITE_STATIC);
        
        if (invoice.contractId) {
            sqlite3_bind_int(stmt, 3, *invoice.contractId);
        } else {
            sqlite3_bind_null(stmt, 3);
        }
        
        sqlite3_bind_double(stmt, 4, invoice.totalAmount);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) {
            invoice.id = static_cast<int>(sqlite3_last_insert_rowid(db_));
            return true;
        }
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

bool DatabaseManager::updateInvoice(const Invoice& invoice) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = R"(
        UPDATE Invoices 
        SET number = ?, date = ?, contract_id = ?, total_amount = ? 
        WHERE id = ?
    )";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, invoice.number.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, invoice.date.c_str(), -1, SQLITE_STATIC);
        
        if (invoice.contractId) {
            sqlite3_bind_int(stmt, 3, *invoice.contractId);
        } else {
            sqlite3_bind_null(stmt, 3);
        }
        
        sqlite3_bind_double(stmt, 4, invoice.totalAmount);
        sqlite3_bind_int(stmt, 5, invoice.id);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

bool DatabaseManager::deleteInvoice(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = "DELETE FROM Invoices WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

// ==================== Регулярные выражения ====================

std::vector<RegexEntry> DatabaseManager::getAllRegexes() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RegexEntry> result;
    
    if (!db_) return result;
    
    const char* sql = "SELECT id, name, pattern FROM Regexes ORDER BY name";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            RegexEntry entry;
            entry.id = sqlite3_column_int(stmt, 0);
            entry.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            entry.pattern = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            result.push_back(entry);
        }
        sqlite3_finalize(stmt);
    }
    
    return result;
}

bool DatabaseManager::createRegex(const RegexEntry& regex) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = "INSERT INTO Regexes (name, pattern) VALUES (?, ?)";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, regex.name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, regex.pattern.c_str(), -1, SQLITE_STATIC);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

bool DatabaseManager::updateRegex(const RegexEntry& regex) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = "UPDATE Regexes SET name = ?, pattern = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, regex.name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, regex.pattern.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, regex.id);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

bool DatabaseManager::deleteRegex(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = "DELETE FROM Regexes WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

// ==================== Подозрительные слова ====================

std::vector<SuspiciousWord> DatabaseManager::getAllSuspiciousWords() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SuspiciousWord> result;
    
    if (!db_) return result;
    
    const char* sql = "SELECT id, word FROM SuspiciousWords ORDER BY word";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            SuspiciousWord sw;
            sw.id = sqlite3_column_int(stmt, 0);
            sw.word = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            result.push_back(sw);
        }
        sqlite3_finalize(stmt);
    }
    
    return result;
}

bool DatabaseManager::createSuspiciousWord(const SuspiciousWord& word) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = "INSERT INTO SuspiciousWords (word) VALUES (?)";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, word.word.c_str(), -1, SQLITE_STATIC);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

bool DatabaseManager::updateSuspiciousWord(const SuspiciousWord& word) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = "UPDATE SuspiciousWords SET word = ? WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, word.word.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, word.id);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

bool DatabaseManager::deleteSuspiciousWord(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    const char* sql = "DELETE FROM SuspiciousWords WHERE id = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc == SQLITE_DONE) return true;
        
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return false;
}

// ==================== Настройки ====================

Settings DatabaseManager::getSettings() {
    // mutex уже должен быть взят в вызывающей функции!
    // std::lock_guard<std::mutex> lock(mutex_);  // ЗАКОММЕНТИРОВАНО - избегаем deadlock
    Settings settings;

    if (!db_) return settings;

    const char* sql = "SELECT organization_name, period_start_date, period_end_date, "
                      "note, import_preview_lines, theme, font_size "
                      "FROM Settings WHERE id = 1";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            settings.organizationName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            settings.periodStartDate = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            settings.periodEndDate = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            settings.note = sqlite3_column_text(stmt, 3)
                ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))
                : "";
            settings.importPreviewLines = sqlite3_column_int(stmt, 4);
            settings.theme = sqlite3_column_int(stmt, 5);
            settings.fontSize = sqlite3_column_int(stmt, 6);
        }
        sqlite3_finalize(stmt);
    }

    return settings;
}

void DatabaseManager::saveSettings(const Settings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return;
    
    // Проверяем, есть ли запись
    const char* checkSql = "SELECT COUNT(*) FROM Settings WHERE id = 1";
    sqlite3_stmt* checkStmt;
    bool exists = false;
    
    if (sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(checkStmt) == SQLITE_ROW && sqlite3_column_int(checkStmt, 0) > 0) {
            exists = true;
        }
        sqlite3_finalize(checkStmt);
    }
    
    sqlite3_stmt* stmt;
    if (exists) {
        const char* sql = R"(
            UPDATE Settings 
            SET organization_name = ?, period_start_date = ?, period_end_date = ?, 
                note = ?, import_preview_lines = ?, theme = ?, font_size = ? 
            WHERE id = 1
        )";
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, settings.organizationName.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, settings.periodStartDate.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, settings.periodEndDate.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, settings.note.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 5, settings.importPreviewLines);
            sqlite3_bind_int(stmt, 6, settings.theme);
            sqlite3_bind_int(stmt, 7, settings.fontSize);
            
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    } else {
        const char* sql = R"(
            INSERT INTO Settings (id, organization_name, period_start_date, period_end_date, 
                                  note, import_preview_lines, theme, font_size) 
            VALUES (1, ?, ?, ?, ?, ?, ?, ?)
        )";
        
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, settings.organizationName.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, settings.periodStartDate.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, settings.periodEndDate.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, settings.note.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 5, settings.importPreviewLines);
            sqlite3_bind_int(stmt, 6, settings.theme);
            sqlite3_bind_int(stmt, 7, settings.fontSize);
            
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
}

void DatabaseManager::loadSettings() {
    // Загружаем настройки (вызывается при открытии БД)
    getSettings();
}

std::string DatabaseManager::getSetting(const std::string& key, const std::string& defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);  // Публичный API - берём mutex
    Settings s = getSettings();  // getSettings() уже не берёт mutex

    if (key == "organization_name") return s.organizationName;
    if (key == "period_start_date") return s.periodStartDate;
    if (key == "period_end_date") return s.periodEndDate;
    if (key == "note") return s.note;

    return defaultValue;
}

int DatabaseManager::getSetting(const std::string& key, int defaultValue) {
    std::lock_guard<std::mutex> lock(mutex_);  // Публичный API - берём mutex
    Settings s = getSettings();  // getSettings() уже не берёт mutex

    if (key == "import_preview_lines") return s.importPreviewLines;
    if (key == "theme") return s.theme;
    if (key == "font_size") return s.fontSize;

    return defaultValue;
}

// ==================== Недавние файлы ====================

std::vector<std::string> DatabaseManager::getRecentFiles() {
    std::vector<std::string> result;
    
    // Путь к файлу recent_files.txt в директории приложения
    std::string recentFilePath = "recent_files.txt";
    
    std::ifstream file(recentFilePath);
    if (!file.is_open()) {
        return result;  // Файл не существует или не открывается
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Пропускаем пустые строки
        if (!line.empty()) {
            result.push_back(line);
        }
    }
    
    file.close();
    return result;
}

void DatabaseManager::addRecentFile(const std::string& path) {
    // Читаем текущий список
    std::vector<std::string> recentFiles = getRecentFiles();
    
    // Удаляем если уже есть (чтобы переместить в начало)
    auto it = std::find(recentFiles.begin(), recentFiles.end(), path);
    if (it != recentFiles.end()) {
        recentFiles.erase(it);
    }
    
    // Добавляем в начало
    recentFiles.insert(recentFiles.begin(), path);
    
    // Оставляем только 10 последних
    if (recentFiles.size() > 10) {
        recentFiles.resize(10);
    }
    
    // Записываем обратно в файл
    std::string recentFilePath = "recent_files.txt";
    std::ofstream file(recentFilePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open recent_files.txt for writing" << std::endl;
        return;
    }
    
    for (const auto& filePath : recentFiles) {
        file << filePath << "\n";
    }
    
    file.close();
}

// ==================== Групповые операции ====================

bool DatabaseManager::bulkUpdatePayments(const std::vector<int>& ids, const std::map<std::string, std::string>& fields) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_ || ids.empty()) return false;
    
    // Проверяем флаг отмены
    if (isCancelled()) return false;
    
    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    
    bool success = true;
    
    for (int id : ids) {
        if (isCancelled()) {
            success = false;
            break;
        }
        
        std::ostringstream sql;
        sql << "UPDATE Payments SET ";
        
        std::vector<std::string> updates;
        for (const auto& [field, value] : fields) {
            updates.push_back(field + " = '" + escapeString(value) + "'");
        }
        
        for (size_t i = 0; i < updates.size(); ++i) {
            if (i > 0) sql << ", ";
            sql << updates[i];
        }
        
        sql << " WHERE id = " << id;
        
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql.str().c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            lastError_ = errMsg ? errMsg : "Bulk update failed";
            sqlite3_free(errMsg);
            success = false;
            break;
        }
    }
    
    if (success) {
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    } else {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
    
    return success;
}

bool DatabaseManager::bulkDeletePayments(const std::vector<int>& ids) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_ || ids.empty()) return false;
    
    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    
    bool success = true;
    
    for (int id : ids) {
        if (isCancelled()) {
            success = false;
            break;
        }
        
        if (!deletePayment(id)) {
            success = false;
            break;
        }
    }
    
    if (success) {
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    } else {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
    
    return success;
}

bool DatabaseManager::bulkUpdateCounterparties(const std::vector<int>& ids, const std::map<std::string, std::string>& fields) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_ || ids.empty()) return false;
    
    if (isCancelled()) return false;
    
    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    
    bool success = true;
    
    for (int id : ids) {
        if (isCancelled()) {
            success = false;
            break;
        }
        
        std::ostringstream sql;
        sql << "UPDATE Counterparties SET ";
        
        std::vector<std::string> updates;
        for (const auto& [field, value] : fields) {
            updates.push_back(field + " = '" + escapeString(value) + "'");
        }
        
        for (size_t i = 0; i < updates.size(); ++i) {
            if (i > 0) sql << ", ";
            sql << updates[i];
        }
        
        sql << " WHERE id = " << id;
        
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql.str().c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            lastError_ = errMsg ? errMsg : "Bulk update failed";
            sqlite3_free(errMsg);
            success = false;
            break;
        }
    }
    
    if (success) {
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    } else {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
    
    return success;
}

bool DatabaseManager::bulkUpdateContracts(const std::vector<int>& ids, const std::map<std::string, std::string>& fields) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_ || ids.empty()) return false;
    
    if (isCancelled()) return false;
    
    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    
    bool success = true;
    
    for (int id : ids) {
        if (isCancelled()) {
            success = false;
            break;
        }
        
        std::ostringstream sql;
        sql << "UPDATE Contracts SET ";
        
        std::vector<std::string> updates;
        for (const auto& [field, value] : fields) {
            updates.push_back(field + " = '" + escapeString(value) + "'");
        }
        
        for (size_t i = 0; i < updates.size(); ++i) {
            if (i > 0) sql << ", ";
            sql << updates[i];
        }
        
        sql << " WHERE id = " << id;
        
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql.str().c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            lastError_ = errMsg ? errMsg : "Bulk update failed";
            sqlite3_free(errMsg);
            success = false;
            break;
        }
    }
    
    if (success) {
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    } else {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
    
    return success;
}

// ==================== Очистка ====================

bool DatabaseManager::clearPayments() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, "DELETE FROM Payments;", nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        lastError_ = errMsg ? errMsg : "Failed to clear payments";
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

bool DatabaseManager::clearCounterparties() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, "DELETE FROM Counterparties;", nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        lastError_ = errMsg ? errMsg : "Failed to clear counterparties";
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

bool DatabaseManager::clearContracts() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, "DELETE FROM Contracts;", nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        lastError_ = errMsg ? errMsg : "Failed to clear contracts";
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

bool DatabaseManager::clearInvoices() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, "DELETE FROM Invoices;", nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        lastError_ = errMsg ? errMsg : "Failed to clear invoices";
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

bool DatabaseManager::clearOrphanedDetails() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!db_) return false;
    
    char* errMsg = nullptr;
    
    // Удаляем детали без родительских записей
    const char* sql = R"(
        DELETE FROM PaymentDetails 
        WHERE payment_id NOT IN (SELECT id FROM Payments)
           OR kosgu_id NOT IN (SELECT id FROM KOSGU)
           OR contract_id NOT IN (SELECT id FROM Contracts)
           OR invoice_id NOT IN (SELECT id FROM Invoices)
    )";
    
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg);
    
    if (rc != SQLITE_OK) {
        lastError_ = errMsg ? errMsg : "Failed to clear orphaned details";
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

bool DatabaseManager::clearRecentFiles() {
    // Удаляем файл recent_files.txt
    std::string recentFilePath = "recent_files.txt";
    
    try {
        std::remove(recentFilePath.c_str());
        return true;
    } catch (...) {
        lastError_ = "Failed to clear recent files";
        return false;
    }
}

// ==================== Импорт/экспорт ИКЗ ====================

bool DatabaseManager::importIczFromCsv(const std::string& path) {
    // Реализация импорта ИКЗ из CSV
    std::ifstream file(path);
    if (!file.is_open()) {
        lastError_ = "Cannot open file: " + path;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    
    std::string line;
    bool headerSkipped = false;
    
    while (std::getline(file, line)) {
        if (isCancelled()) {
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }
        
        if (!headerSkipped) {
            headerSkipped = true;
            continue;
        }
        
        // Парсинг CSV (упрощённо)
        // Формат: number,date,procurement_code
        size_t pos1 = line.find('\t');
        size_t pos2 = line.find('\t', pos1 + 1);
        
        if (pos1 != std::string::npos && pos2 != std::string::npos) {
            std::string number = line.substr(0, pos1);
            std::string date = line.substr(pos1 + 1, pos2 - pos1 - 1);
            std::string icz = line.substr(pos2 + 1);
            
            // Обновляем договор с ИКЗ
            std::ostringstream sql;
            sql << "UPDATE Contracts SET procurement_code = '" 
                << escapeString(icz) << "' WHERE number = '" 
                << escapeString(number) << "' AND date = '" 
                << escapeString(date) << "'";
            
            sqlite3_exec(db_, sql.str().c_str(), nullptr, nullptr, nullptr);
        }
    }
    
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}

bool DatabaseManager::exportIczToCsv(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        lastError_ = "Cannot create file: " + path;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    const char* sql = R"(
        SELECT number, date, procurement_code 
        FROM Contracts 
        WHERE procurement_code IS NOT NULL AND procurement_code != ''
    )";
    sqlite3_stmt* stmt;
    
    // Заголовок
    file << "number\tdate\tprocurement_code\n";
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            file << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)) << "\t";
            file << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)) << "\t";
            file << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)) << "\n";
        }
        sqlite3_finalize(stmt);
    }
    
    return true;
}

// ==================== Произвольный SQL запрос ====================

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> 
DatabaseManager::executeQuery(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> result;
    
    if (!db_) return result;
    
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        // Получаем имена колонок
        int colCount = sqlite3_column_count(stmt);
        for (int i = 0; i < colCount; ++i) {
            result.first.push_back(sqlite3_column_name(stmt, i));
        }
        
        // Получаем строки
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::vector<std::string> row;
            for (int i = 0; i < colCount; ++i) {
                const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                row.push_back(text ? text : "");
            }
            result.second.push_back(row);
        }
        
        sqlite3_finalize(stmt);
    } else {
        lastError_ = sqlite3_errmsg(db_);
    }
    
    return result;
}

// ==================== Вспомогательные методы ====================

std::string DatabaseManager::escapeString(const std::string& str) {
    std::string result;
    result.reserve(str.size() + 10);
    
    for (char c : str) {
        switch (c) {
            case '\'': result += "''"; break;
            case '\\': result += "\\\\"; break;
            default: result += c;
        }
    }
    
    return result;
}

} // namespace FinancialAudit
