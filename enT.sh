#!/bin/bash

# Имя выходного файла
OUTPUT="output.txt"

# Переход в директорию проекта
PROJECT_DIR="$HOME/sources/C/NSQCuC"
cd "$PROJECT_DIR" || exit 1

# Очистка или создание выходного файла
> "$OUTPUT"

# Функция для добавления файла в output
add_file() {
    local file="$1"
    if [ -f "$file" ]; then
        {
            echo "========================================"
            echo "FILE: $file"
            echo "========================================"
            cat "$file"
            echo
            echo
        } >> "$OUTPUT"
    else
        echo "Warning: File not found - $file" >&2
    fi
}

# === Корневые файлы проекта ===
add_file "CMakeLists.txt"
add_file "vcpkg.json"
add_file "README.md"

# === GitHub Actions workflow ===
add_file ".github/workflows/windows-build.yml"
add_file ".github/workflows/linux-build.yml"

# === Исходный код C++ (src/) ===
add_file "src/main.cpp"
add_file "src/backend.cpp"
add_file "src/backend.h"
add_file "src/addonmanager.cpp"
add_file "src/addonmanager.h"
add_file "src/settings.cpp"
add_file "src/settings.h"
add_file "src/gamelauncher.cpp"
add_file "src/gamelauncher.h"

# === Фронтенд ресурсы (resources/) ===
add_file "resources/index.html"
add_file "resources/renderer.js"
add_file "resources/style.css"
add_file "resources/ipc-bridge.js"

# === Windows ресурсы ===
add_file "resources/app.rc"

# === Иконки (только текстовое описание, бинарники не включаем) ===
{
    echo "========================================"
    echo "FILE: icons/ (binary files - not included)"
    echo "========================================"
    if [ -d "resources/icons" ]; then
        ls -la "resources/icons/" 2>/dev/null | while read line; do
            echo "$line"
        done
    else
        echo "icons/32x32.png - (binary)"
        echo "icons/128x128.png - (binary)"
        echo "icons/icon.ico - (binary)"
    fi
    echo
    echo
} >> "$OUTPUT"

# === Конфигурация сборки (опционально) ===
if [ -f "build/CMakeCache.txt" ]; then
    add_file "build/CMakeCache.txt"
fi

# === Вывод статистики ===
{
    echo "========================================"
    echo "PROJECT STRUCTURE"
    echo "========================================"
    find . -type f \
        -not -path "./build/*" \
        -not -path "./.git/*" \
        -not -name "*.o" \
        -not -name "*.a" \
        -not -name "nsqcut" \
        -not -name "nsqcut.exe" \
        -not -name "output.txt" \
        -not -name "*.png" \
        -not -name "*.ico" \
        -not -name "*.dll" \
        -not -name "*.so" \
        -not -name "*.dylib" \
        | sort
    echo
    echo
} >> "$OUTPUT"

echo "Готово! Все файлы проекта собраны в $OUTPUT"
echo "Путь: $PROJECT_DIR/$OUTPUT"