/**
 * @file CustomWidgets.cpp
 * @brief Реализация пользовательских виджетов
 */

#include "CustomWidgets.h"

#include <imgui_internal.h>
#include <regex>
#include <algorithm>

namespace FinancialAudit {

bool CustomWidgets::InputText(const char* label, std::string& str, ImGuiInputTextFlags flags) {
    char buffer[1024];
    strncpy(buffer, str.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    if (ImGui::InputText(label, buffer, sizeof(buffer), flags)) {
        str = buffer;
        return true;
    }
    return false;
}

bool CustomWidgets::InputTextMultiline(const char* label, std::string& str, const ImVec2& size, ImGuiInputTextFlags flags) {
    char buffer[4096];
    strncpy(buffer, str.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    if (ImGui::InputTextMultiline(label, buffer, sizeof(buffer), size, flags)) {
        str = buffer;
        return true;
    }
    return false;
}

bool CustomWidgets::InputDate(const char* label, std::string& date) {
    char buffer[32];
    strncpy(buffer, date.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    // Валидация формата YYYY-MM-DD
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputText(label, buffer, sizeof(buffer))) {
        // Простая валидация
        std::string newDate = buffer;
        std::regex dateRegex(R"(\d{4}-\d{2}-\d{2})");
        
        if (std::regex_match(newDate, dateRegex) || newDate.empty()) {
            date = newDate;
            return true;
        }
    }
    return false;
}

bool CustomWidgets::AmountInput(const char* label, double& amount) {
    if (ImGui::InputDouble(label, &amount, 0.01, 1.0, "%.2f")) {
        if (amount < 0) amount = 0;
        return true;
    }
    return false;
}

bool CustomWidgets::ComboWithFilter(const char* label, int& currentItem,
                                    const std::vector<std::string>& items,
                                    const std::string& filterPlaceholder) {
    bool changed = false;
    
    if (ImGui::BeginCombo(label, currentItem >= 0 && currentItem < static_cast<int>(items.size()) 
                                     ? items[currentItem].c_str() : "")) {
        // Фильтр
        static char filterBuffer[256] = "";
        ImGui::InputTextWithHint("##filter", filterPlaceholder.c_str(), filterBuffer, sizeof(filterBuffer));
        
        for (size_t i = 0; i < items.size(); ++i) {
            std::string filter = filterBuffer;
            std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
            
            std::string itemLower = items[i];
            std::transform(itemLower.begin(), itemLower.end(), itemLower.begin(), ::tolower);
            
            if (filter.empty() || itemLower.find(filter) != std::string::npos) {
                bool isSelected = (static_cast<int>(i) == currentItem);
                if (ImGui::Selectable(items[i].c_str(), isSelected)) {
                    currentItem = static_cast<int>(i);
                    changed = true;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }
        
        ImGui::EndCombo();
    }
    
    return changed;
}

bool CustomWidgets::ComboWithFilter(const char* label, std::string& selectedValue,
                                    const std::vector<std::string>& items,
                                    bool allowEmpty) {
    bool changed = false;
    
    const char* previewValue = selectedValue.empty() ? "" : selectedValue.c_str();
    
    if (ImGui::BeginCombo(label, previewValue)) {
        if (allowEmpty) {
            if (ImGui::Selectable("", selectedValue.empty())) {
                selectedValue.clear();
                changed = true;
            }
        }
        
        // Фильтр
        static char filterBuffer[256] = "";
        ImGui::InputTextWithHint("##filter", "Фильтр...", filterBuffer, sizeof(filterBuffer));
        
        for (const auto& item : items) {
            std::string filter = filterBuffer;
            std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
            
            std::string itemLower = item;
            std::transform(itemLower.begin(), itemLower.end(), itemLower.begin(), ::tolower);
            
            if (filter.empty() || itemLower.find(filter) != std::string::npos) {
                bool isSelected = (item == selectedValue);
                if (ImGui::Selectable(item.c_str(), isSelected)) {
                    selectedValue = item;
                    changed = true;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }
        
        ImGui::EndCombo();
    }
    
    return changed;
}

bool CustomWidgets::ConfirmationModal(const char* title, const char* message,
                                      const char* confirmText, const char* cancelText) {
    bool result = false;
    
    ImGui::OpenPopup(title);
    
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", message);
        ImGui::Separator();
        
        if (ImGui::Button(confirmText, ImVec2(120, 0))) {
            result = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(cancelText, ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }

    return result;
}

void CustomWidgets::HorizontalSplitter(float thickness, float minSize) {
    // Используем стандартный разделитель ImGui
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
}

void CustomWidgets::VerticalSplitter(float thickness, float minSize) {
    // Используем стандартный разделитель ImGui
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
}

bool CustomWidgets::InnInput(const char* label, std::string& inn) {
    char buffer[32];
    strncpy(buffer, inn.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    // ИНН может быть 10 или 12 цифр
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputText(label, buffer, sizeof(buffer), ImGuiInputTextFlags_CharsDecimal)) {
        // Ограничиваем длину
        std::string newInn = buffer;
        if (newInn.length() <= 12) {
            inn = newInn;
            return true;
        }
    }
    return false;
}

bool CustomWidgets::CheckboxWithFlag(const char* label, bool& value) {
    return ImGui::Checkbox(label, &value);
}

bool CustomWidgets::IconButton(const char* icon, const char* tooltip) {
    bool clicked = ImGui::Button(icon);
    
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
    
    return clicked;
}

void CustomWidgets::ProgressBarWithText(float fraction, const ImVec2& sizeArg, const char* overlay) {
    ImGui::ProgressBar(fraction, sizeArg, overlay);
}

} // namespace FinancialAudit
