/**
 * @file BaseView.h
 * @brief Базовый класс для всех представлений
 */

#pragma once

#include <string>
#include <vector>
#include <utility>

namespace FinancialAudit {

class DatabaseManager;
class PdfReporter;
class UIManager;

/**
 * @class BaseView
 * @brief Базовый класс для всех представлений приложения
 */
class BaseView {
public:
    virtual ~BaseView() = default;

    virtual void Render() = 0;
    
    virtual void SetDatabaseManager(DatabaseManager* dbManager) { dbManager_ = dbManager; }
    virtual void SetPdfReporter(PdfReporter* pdfReporter) { pdfReporter_ = pdfReporter; }
    virtual void SetUIManager(UIManager* uiManager) { uiManager_ = uiManager; }
    
    /**
     * @brief Получить данные в виде строк для экспорта
     * @return Пары (колонки, строки)
     */
    virtual std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> GetDataAsStrings() {
        return {{}, {}};
    }
    
    /**
     * @brief Проверить, открыто ли окно
     */
    virtual bool IsOpen() const { return isOpen_; }

protected:
    DatabaseManager* dbManager_ = nullptr;
    PdfReporter* pdfReporter_ = nullptr;
    UIManager* uiManager_ = nullptr;
    
    std::string title_;
    bool isOpen_ = true;
};

} // namespace FinancialAudit
