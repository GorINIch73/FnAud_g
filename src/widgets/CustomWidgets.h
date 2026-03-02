/**
 * @file CustomWidgets.h
 * @brief Пользовательские виджеты для интерфейса
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <optional>

#include <imgui.h>

namespace FinancialAudit {

/**
 * @class CustomWidgets
 * @brief Коллекция пользовательских виджетов
 */
class CustomWidgets {
public:
    /**
     * @brief Поле ввода текста с поддержкой UTF-8
     */
    static bool InputText(const char* label, std::string& str, ImGuiInputTextFlags flags = 0);
    
    /**
     * @brief Многострочное поле ввода
     */
    static bool InputTextMultiline(const char* label, std::string& str, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0);
    
    /**
     * @brief Поле ввода даты (формат YYYY-MM-DD)
     */
    static bool InputDate(const char* label, std::string& date);
    
    /**
     * @brief Поле ввода суммы
     */
    static bool AmountInput(const char* label, double& amount);
    
    /**
     * @brief Выпадающий список с поиском/фильтрацией
     */
    static bool ComboWithFilter(const char* label, int& currentItem, 
                                const std::vector<std::string>& items,
                                const std::string& filterPlaceholder = "Фильтр...");
    
    /**
     * @brief Выпадающий список с поиском для строковых значений
     */
    static bool ComboWithFilter(const char* label, std::string& selectedValue,
                                const std::vector<std::string>& items,
                                bool allowEmpty = true);
    
    /**
     * @brief Модальное окно подтверждения
     */
    static bool ConfirmationModal(const char* title, const char* message, 
                                  const char* confirmText = "Подтвердить",
                                  const char* cancelText = "Отмена");
    
    /**
     * @brief Горизонтальный разделитель с изменяемым размером
     */
    static void HorizontalSplitter(float thickness = 4.0f, float minSize = 20.0f);
    
    /**
     * @brief Вертикальный разделитель с изменяемым размером
     */
    static void VerticalSplitter(float thickness = 4.0f, float minSize = 20.0f);
    
    /**
     * @brief Поле ввода ИНН
     */
    static bool InnInput(const char* label, std::string& inn);
    
    /**
     * @brief Чекбокс с флагом
     */
    static bool CheckboxWithFlag(const char* label, bool& value);
    
    /**
     * @brief Кнопка с иконкой
     */
    static bool IconButton(const char* icon, const char* tooltip = nullptr);
    
    /**
     * @brief Прогресс бар с текстом
     */
    static void ProgressBarWithText(float fraction, const ImVec2& sizeArg = ImVec2(-1, 0), const char* overlay = nullptr);
};

} // namespace FinancialAudit
