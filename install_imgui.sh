#!/bin/bash
# Скрипт для установки Dear ImGui

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== Установка Dear ImGui ==="

if [ -d "external/imgui" ]; then
    echo "Dear ImGui уже установлен. Обновление..."
    cd external/imgui
    git pull
else
    echo "Клонирование Dear ImGui..."
    mkdir -p external
    cd external
    git clone --branch docking https://github.com/ocornut/imgui.git
fi

echo "=== Проверка файлов ==="
cd "$SCRIPT_DIR"
if [ -f "external/imgui/imgui.cpp" ]; then
    echo "OK: imgui.cpp найден"
else
    echo "ERROR: imgui.cpp не найден"
    exit 1
fi

if [ -f "external/imgui/backends/imgui_impl_glfw.cpp" ]; then
    echo "OK: imgui_impl_glfw.cpp найден"
else
    echo "ERROR: imgui_impl_glfw.cpp не найден"
    exit 1
fi

echo "=== Dear ImGui успешно установлен ==="
echo ""
echo "Теперь вы можете собрать проект:"
echo "  mkdir build && cd build"
echo "  cmake -G \"MinGW Makefiles\" .."
echo "  cmake --build ."
