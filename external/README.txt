# Инструкция по установке Dear ImGui

Dear ImGui требуется для сборки проекта.

## Способ 1: Git clone (рекомендуется)

```bash
cd external
git clone --branch docking https://github.com/ocornut/imgui.git
```

## Способ 2: Скачать архив

1. Перейдите на https://github.com/ocornut/imgui/tree/docking
2. Скачайте архив (Download ZIP)
3. Распакуйте в папку `external/imgui`

## После установки

Проверьте, что файлы существуют:
- `external/imgui/imgui.cpp`
- `external/imgui/backends/imgui_impl_glfw.cpp`
- `external/imgui/backends/imgui_impl_opengl3.cpp`
- `external/imgui/misc/cpp/imgui_stdlib.cpp`

Затем запустите сборку:

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
```
