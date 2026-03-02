/**
 * @file main.cpp
 * @brief Точка входа приложения Financial Audit
 *
 * Инициализация GLFW, OpenGL, Dear ImGui и главный цикл приложения
 */

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <fstream>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "App.h"

// Глобальные переменные для обработки ошибок
static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main(int argc, char* argv[]) {
    // Установка обработчика ошибок GLFW
    glfwSetErrorCallback(glfw_error_callback);

    // Инициализация GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return EXIT_FAILURE;
    }

    // Настройка OpenGL версии
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Создание окна
    GLFWwindow* window = glfwCreateWindow(
        1280, 720,
        "Financial Audit",
        nullptr,
        nullptr
    );

    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // V-Sync

    // Инициализация Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = false;
    io.ConfigDockingAlwaysTabBar = false;
    io.ConfigDockingTransparentPayload = false;

    // Настройка стиля
    ImGui::StyleColorsDark();

    // Инициализация бэкендов
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Загрузка шрифтов: основной + иконки Font Awesome
    ImFontConfig font_config;
    font_config.MergeMode = false;
    font_config.PixelSnapH = false;
    font_config.OversampleH = 3;
    font_config.OversampleV = 3;

    // Основной шрифт с поддержкой кириллицы
    static const ImWchar* glyph_ranges = io.Fonts->GetGlyphRangesCyrillic();
    const char* fontPath = "resources/DejaVuSans.ttf";
    std::ifstream fontFile(fontPath);
    if (fontFile.good()) {
        fontFile.close();
        io.Fonts->AddFontFromFileTTF(fontPath, 16.0f, &font_config, glyph_ranges);
        std::cout << "Loaded custom font: " << fontPath << std::endl;
    } else {
        io.Fonts->AddFontDefault(&font_config);
        std::cout << "Using default font (with Cyrillic support)" << std::endl;
    }

    // Шрифт с иконками Font Awesome (объединяем с основным)
    ImFontConfig icon_config;
    icon_config.MergeMode = true;
    icon_config.PixelSnapH = true;
    icon_config.GlyphMinAdvanceX = 14.0f;
    icon_config.GlyphOffset.y = 1.0f;  // Коррекция позиции иконок
    // Диапазон иконок Font Awesome
    static const ImWchar icon_ranges[] = { 0xf000, 0xf8ff, 0 };
    
    const char* iconFontPath = "resources/fa-solid-900.ttf";
    std::ifstream iconFontFile(iconFontPath);
    if (iconFontFile.good()) {
        iconFontFile.close();
        io.Fonts->AddFontFromFileTTF(iconFontPath, 16.0f, &icon_config, icon_ranges);
        std::cout << "Loaded icon font: " << iconFontPath << std::endl;
    } else {
        std::cout << "Icon font not found, icons will not be displayed" << std::endl;
    }
    
    io.Fonts->Build();

    // Создание и инициализация приложения
    FinancialAudit::App app(window);
    
    if (!app.initialize()) {
        std::cerr << "Failed to initialize application" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // Главный цикл приложения
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Начало кадра ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Рендеринг приложения
        app.render();

        // Отображение
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Очистка ресурсов
    app.shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}
