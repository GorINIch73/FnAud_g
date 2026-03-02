# Financial Audit

Приложение для финансового аудита организаций государственного сектора.

## Описание

**Financial Audit** — настольное приложение для:
- Ведения базы данных финансовых операций
- Управления справочниками (КОСГУ, контрагенты, договоры, накладные)
- Импорта данных из TSV-файлов
- Экспорта данных и формирования отчётности (PDF, CSV)
- Выполнения SQL-запросов к базе данных
- Выявления подозрительных операций

## Технологии

- **Язык:** C++17
- **Графический интерфейс:** Dear ImGui (docking branch)
- **Оконная система:** GLFW 3.3+
- **Графика:** OpenGL 3.3 Core
- **База данных:** SQLite3
- **Система сборки:** CMake 3.20+

## Требования

### Аппаратные
- Оперативная память: от 2 ГБ
- Свободное место на диске: от 100 МБ
- Видеокарта с поддержкой OpenGL 3.3

### Программные
- Компилятор с поддержкой C++17 (GCC 7+, MSVC 2017+, Clang 5+)
- CMake 3.20+
- GLFW 3.3+
- OpenGL 3.3+
- SQLite3
- Git (для загрузки Dear ImGui)

## Быстрый старт (MSYS2 Windows)

```bash
# 1. Установка зависимостей
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc \
        mingw-w64-x86_64-glfw mingw-w64-x86_64-mesa \
        mingw-w64-x86_64-sqlite3 git

# 2. Установка Dear ImGui
chmod +x install_imgui.sh
./install_imgui.sh

# 3. Сборка
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
cmake --build .

# 4. Запуск
./FinancialAudit.exe
```

## Сборка

### Linux (Ubuntu/Debian)

```bash
# Установка зависимостей
sudo apt-get install cmake build-essential libglfw3-dev \
    libgl1-mesa-dev libsqlite3-dev git

# Установка Dear ImGui
chmod +x install_imgui.sh
./install_imgui.sh

# Сборка
mkdir build && cd build
cmake ..
cmake --build .

# Запуск
./FinancialAudit
```

### MSYS2 (Windows)

```bash
# Установка зависимостей
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc \
        mingw-w64-x86_64-glfw mingw-w64-x86_64-mesa \
        mingw-w64-x86_64-sqlite3 git

# Установка Dear ImGui
./install_imgui.sh

# Сборка
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
cmake --build .

# Запуск
./FinancialAudit.exe
```

### macOS

```bash
# Установка зависимостей через Homebrew
brew install cmake glfw sqlite git

# Установка Dear ImGui
chmod +x install_imgui.sh
./install_imgui.sh

# Сборка
mkdir build && cd build
cmake ..
cmake --build .

# Запуск
./FinancialAudit
```

## Структура проекта

```
fa_test/
├── CMakeLists.txt              # Файл сборки CMake
├── README.md                   # Этот файл
├── ТЕХНИЧЕСКОЕ_ЗАДАНИЕ.md      # Техническое задание
├── install_imgui.sh            # Скрипт установки Dear ImGui
├── src/
│   ├── main.cpp                # Точка входа
│   ├── App.h/cpp               # Главный класс приложения
│   ├── managers/               # Менеджеры
│   │   ├── DatabaseManager.h/cpp  # Работа с БД
│   │   ├── UIManager.h/cpp        # Управление UI
│   │   ├── ImportManager.h/cpp    # Импорт данных
│   │   ├── ExportManager.h/cpp    # Экспорт данных
│   │   └── PdfReporter.h/cpp      # Генерация PDF
│   ├── views/                  # Представления (окна)
│   │   ├── BaseView.h/cpp
│   │   ├── PaymentsView.h/cpp     # Платежи
│   │   ├── KosguView.h/cpp        # КОСГУ
│   │   ├── CounterpartiesView.h/cpp  # Контрагенты
│   │   ├── ContractsView.h/cpp    # Договоры
│   │   ├── InvoicesView.h/cpp     # Накладные
│   │   ├── SqlQueryView.h/cpp     # SQL запросы
│   │   ├── SettingsView.h/cpp     # Настройки
│   │   └── ...
│   ├── widgets/                # Пользовательские виджеты
│   │   └── CustomWidgets.h/cpp
│   └── utils/                  # Утилиты
│       ├── StringUtils.h/cpp
│       └── FileUtils.h/cpp
├── external/
│   └── imgui/                  # Dear ImGui (устанавливается скриптом)
└── resources/
    └── NotoSans-Regular.ttf    # Шрифт с поддержкой кириллицы
```

## Использование

### Главное меню

**Файл:**
- Создать новую базу
- Открыть базу данных
- Сохранить базу как...
- Недавние файлы
- Выход

**Справочники:**
- КОСГУ
- Банк (Платежи)
- Контрагенты
- Договоры
- Накладные

**Отчёты:**
- SQL Запрос
- Экспорт в PDF
- Договоры для проверки

**Сервис:**
- Импорт из TSV
- Экспорт/Импорт ИКЗ
- Экспорт справочников
- Регулярные выражения
- Подозрительные слова
- Очистка базы
- Настройки

### Работа с базой данных

1. **Создание новой БД:** Файл → Создать новую базу
2. **Открытие существующей:** Файл → Открыть базу данных
3. **Резервное копирование:** Файл → Сохранить базу как...

### Импорт данных

1. Сервис → Импорт из TSV
2. Выберите файл TSV
3. Настройте маппинг колонок
4. Задайте regex для извлечения договоров и КОСГУ
5. Нажмите "Импорт"

### Экспорт данных

- **CSV:** Кнопка "Экспорт в CSV" в представлениях
- **PDF:** Кнопка "Экспорт в PDF" или Отчёты → Экспорт в PDF

## База данных

### Таблицы

- **KOSGU** — справочник КОСГУ
- **Counterparties** — контрагенты
- **Contracts** — договоры
- **Payments** — платежи
- **Invoices** — накладные
- **PaymentDetails** — детали платежей
- **Regexes** — регулярные выражения
- **SuspiciousWords** — подозрительные слова
- **Settings** — настройки приложения
- **RecentFiles** — недавние файлы

## Настройки

- Наименование организации
- Период аудита
- Количество строк предпросмотра импорта
- Тема оформления (Тёмная/Светлая/Классическая)
- Размер шрифта

## Горячие клавиши

- `Ctrl+N` — Создать базу
- `Ctrl+O` — Открыть базу
- `Ctrl+S` — Сохранить базу
- `Alt+F4` — Выход

## Утилита генерации тестовых данных

В комплект входит утилита **TestDataGenerator** для генерации тестовых данных (имитация выгрузки банка).

### Сборка утилиты

Утилита собирается автоматически вместе с основным проектом:

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

После сборки в папке `build/` появится исполняемый файл `TestDataGenerator.exe`.

### Использование

```bash
TestDataGenerator <количество_записей> [имя_файла] [seed]
```

**Аргументы:**
- `количество_записей` — число записей для генерации (от 1 до 100000)
- `имя_файла` — имя выходного TSV файла (по умолчанию: `bank_export.tsv`)
- `seed` — seed для генератора случайных чисел (по умолчанию: время системы)

### Примеры

```bash
# Сгенерировать 1000 записей
TestDataGenerator 1000

# Сгенерировать 5000 записей в файл test_data.tsv
TestDataGenerator 5000 test_data.tsv

# Сгенерировать 10000 записей с фиксированным seed (для воспроизводимости)
TestDataGenerator 10000 data.tsv 42

# Показать справку
TestDataGenerator --help
```

### Формат выходного файла

TSV (Tab-Separated Values) с колонками:

| Колонка | Описание |
|---------|----------|
| `doc_number` | номер платежного поручения (6 цифр) |
| `date` | дата платежа (ДД.ММ.ГГГГ) |
| `recipient` | наименование контрагента |
| `inn` | ИНН контрагента (10 цифр) |
| `amount` | сумма платежа (рубли, 2 знака после запятой) |
| `description` | назначение платежа |

### Содержание описания платежа

Поле `description` содержит информацию для тестирования импорта:

- **Номер и дату договора** (контракта) — форматы: `по контракту №XXX`, `договор №XXX`, `Контракт XXX`
- **Номер и дату накладной/акта** — форматы: `акт №XXX`, `накладная №XXX`, `счет-фактура №XXX`
- **Код КОСГУ** — формат `КXXX` (например, `К100`, `К262`)
- **Тип операции** — оплата, аванс, возврат и т.д.

**Пример описания:**
```
Оплата по договору №Д-1234 от 15.01.2024. Основание: акт №00567 от 20.01.2024. КОСГУ К262
```

### Производительность

- 1 000 записей: ~0.01 сек
- 10 000 записей: ~0.1 сек
- 100 000 записей: ~1 сек

Размер файла: ~200 КБ на 1000 записей.

## Решение проблем

### Ошибка: "Cannot find source file: external/imgui/..."

Запустите скрипт установки Dear ImGui:

```bash
./install_imgui.sh
```

### Ошибка: "GLFW not found"

Установите GLFW:

**Linux:** `sudo apt-get install libglfw3-dev`
**MSYS2:** `pacman -S mingw-w64-x86_64-glfw`
**macOS:** `brew install glfw`

### Ошибка: "OpenGL not found"

Установите OpenGL:

**Linux:** `sudo apt-get install libgl1-mesa-dev`
**MSYS2:** `pacman -S mingw-w64-x86_64-mesa`
**macOS:** Входит в состав Xcode

### Ошибка компиляции C++17

Убедитесь, что компилятор поддерживает C++17:

```bash
g++ --version  # Должна быть версия 7+ или clang 5+
```

## Лицензия

Проект разработан в рамках системы Qwen Code для задач финансового аудита.

## Контакты

ggorinich@gmail.com
