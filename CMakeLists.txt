cmake_minimum_required(VERSION 3.20)
project(FinancialAudit VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Опции сборки
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Позиционно-независимый код
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ============================================
# Поиск зависимостей
# ============================================

# GLFW
find_package(glfw3 3.3 REQUIRED)

# OpenGL
find_package(OpenGL REQUIRED)

# SQLite3
find_package(SQLite3 REQUIRED)

# ============================================
# Dear ImGui - используем системную версию или заголовки
# ============================================

# Пытаемся найти системную ImGui или используем заголовочные файлы
set(IMGUI_FOUND FALSE)

# Проверяем наличие external/imgui
if(EXISTS "${CMAKE_SOURCE_DIR}/external/imgui/imgui.cpp")
    set(IMGUI_FOUND TRUE)
    set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/external/imgui")
    set(IMGUI_BACKENDS_DIR "${IMGUI_DIR}/backends")

    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
        ${IMGUI_BACKENDS_DIR}/imgui_impl_glfw.cpp
        ${IMGUI_BACKENDS_DIR}/imgui_impl_opengl3.cpp
    )
else()
    # Используем заголовки из системы
    message(STATUS "External ImGui not found, will use headers only")
endif()

# ============================================
# ImGuiFileDialog
# ============================================

set(IMGFILED_FOUND FALSE)

if(EXISTS "${CMAKE_SOURCE_DIR}/external/ImGuiFileDialog/ImGuiFileDialog.cpp")
    set(IMGFILED_FOUND TRUE)
    set(IMGFILED_DIR "${CMAKE_SOURCE_DIR}/external/ImGuiFileDialog")
    
    set(IMGFILED_SOURCES
        ${IMGFILED_DIR}/ImGuiFileDialog.cpp
    )
    
    message(STATUS "ImGuiFileDialog found")
else()
    message(STATUS "ImGuiFileDialog not found, file dialogs will not be available")
endif()

# ============================================
# Исходные файлы проекта
# ============================================

set(PROJECT_SOURCES
    src/main.cpp
    src/App.cpp
    src/managers/DatabaseManager.cpp
    src/managers/UIManager.cpp
    src/managers/ImportManager.cpp
    src/managers/ExportManager.cpp
    src/managers/PdfReporter.cpp
    src/views/BaseView.cpp
    src/views/KosguView.cpp
    src/views/PaymentsView.cpp
    src/views/CounterpartiesView.cpp
    src/views/ContractsView.cpp
    src/views/InvoicesView.cpp
    src/views/SqlQueryView.cpp
    src/views/SettingsView.cpp
    src/views/ImportMapView.cpp
    src/views/RegexesView.cpp
    src/views/SuspiciousWordsView.cpp
    src/views/SelectiveCleanView.cpp
    src/views/SpecialQueryView.cpp
    src/views/ServiceView.cpp
    src/widgets/CustomWidgets.cpp
    src/utils/StringUtils.cpp
    src/utils/FileUtils.cpp
)

set(PROJECT_HEADERS
    src/App.h
    src/managers/DatabaseManager.h
    src/managers/UIManager.h
    src/managers/ImportManager.h
    src/managers/ExportManager.h
    src/managers/PdfReporter.h
    src/views/BaseView.h
    src/views/PaymentsView.h
    src/views/KosguView.h
    src/views/CounterpartiesView.h
    src/views/ContractsView.h
    src/views/InvoicesView.h
    src/views/SqlQueryView.h
    src/views/SettingsView.h
    src/views/ImportMapView.h
    src/views/RegexesView.h
    src/views/SuspiciousWordsView.h
    src/views/SelectiveCleanView.h
    src/views/SpecialQueryView.h
    src/views/ServiceView.h
    src/widgets/CustomWidgets.h
    src/utils/StringUtils.h
    src/utils/FileUtils.h
)

# ============================================
# Создаём исполняемый файл
# ============================================

if(IMGUI_FOUND AND IMGFILED_FOUND)
    add_executable(${PROJECT_NAME}
        ${PROJECT_SOURCES}
        ${PROJECT_HEADERS}
        ${IMGUI_SOURCES}
        ${IMGFILED_SOURCES}
    )
elseif(IMGUI_FOUND)
    add_executable(${PROJECT_NAME}
        ${PROJECT_SOURCES}
        ${PROJECT_HEADERS}
        ${IMGUI_SOURCES}
    )
else()
    add_executable(${PROJECT_NAME}
        ${PROJECT_SOURCES}
        ${PROJECT_HEADERS}
    )
endif()

# ============================================
# Подключаемые директории
# ============================================

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${OPENGL_INCLUDE_DIR}
)

if(IMGUI_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/misc/cpp
        ${IMGUI_BACKENDS_DIR}
    )
endif()

if(IMGFILED_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${IMGFILED_DIR}
    )
endif()

# ============================================
# Линкуем библиотеки
# ============================================

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    OpenGL::GL
    SQLite::SQLite3
)

# ============================================
# Платформо-специфичные настройки
# ============================================

if(WIN32)
    # Windows специфичные настройки
    target_link_libraries(${PROJECT_NAME} PRIVATE imm32)
    if(MINGW)
        target_link_libraries(${PROJECT_NAME} PRIVATE imm32)
    endif()
elseif(UNIX AND NOT APPLE)
    # Linux специфичные настройки
    find_package(X11 REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${X11_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${X11_INCLUDE_DIR})
endif()

# ============================================
# Установка
# ============================================

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Копирование шрифтов и ресурсов
install(DIRECTORY resources/ DESTINATION share/${PROJECT_NAME}/resources)

# ============================================
# Компиляторные флаги
# ============================================

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX-)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ============================================
# Утилита генерации тестовых данных
# ============================================

add_executable(TestDataGenerator tools/test_data_generator.cpp)

target_link_libraries(TestDataGenerator PRIVATE)

if(MSVC)
    target_compile_options(TestDataGenerator PRIVATE /W4)
else()
    target_compile_options(TestDataGenerator PRIVATE -Wall -Wextra)
endif()

# Установка утилиты
install(TARGETS TestDataGenerator
    RUNTIME DESTINATION bin
)
