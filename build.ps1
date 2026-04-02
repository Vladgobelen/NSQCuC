[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    
    [switch]$Clean,
    
    [switch]$SkipVcpkg,
    
    [string]$QtPath = "C:/Qt/6.6.0/msvc2019_64",
    
    [string]$VcpkgPath = "C:/vcpkg"
)

# 🔥 Цветной вывод для наглядности
function Write-Step { param([string]$Message) Write-Host "`n[✓] $Message" -ForegroundColor Cyan }
function Write-Error { param([string]$Message) Write-Host "`n[✗] $Message" -ForegroundColor Red }
function Write-Warn  { param([string]$Message) Write-Host "`n[!] $Message" -ForegroundColor Yellow }
function Write-Debug { param([string]$Message) Write-Host "    [dbg] $Message" -ForegroundColor DarkGray }

# 🔥 Проверка на ошибки
function Check-Result {
    param([string]$StepName)
    if ($LASTEXITCODE -ne 0) {
        Write-Error "$StepName failed with code $LASTEXITCODE"
        exit 1
    }
    Write-Debug "$StepName completed successfully"
}

# ==================== НАЧАЛО ====================
Write-Host "`n========================================" -ForegroundColor Green
Write-Host "  NSQCuT Build Script (Windows)" -ForegroundColor Green
Write-Host "  Config: $Configuration | Clean: $Clean | SkipVcpkg: $SkipVcpkg" -ForegroundColor Green
Write-Host "  Qt: $QtPath | vcpkg: $VcpkgPath" -ForegroundColor Green
Write-Host "========================================`n" -ForegroundColor Green

# 1️⃣ Проверка окружения
Write-Step "Checking environment..."

# Проверка Visual Studio
$vsWhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
if (Test-Path $vsWhere) {
    $vsInfo = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vsInfo) {
        Write-Debug "Visual Studio found: $vsInfo"
        # Загружаем окружение MSVC
        $vcVars = "$vsInfo\VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $vcVars) {
            Write-Debug "Loading MSVC environment from $vcVars"
            cmd /c "call `"$vcVars`" && set" | ForEach-Object {
                $name, $value = $_ -split '=', 2
                if ($name -and $value) {
                    Set-Item -Path "env:$name" -Value $value -Force
                }
            }
        }
    }
}

# Проверка Qt
if (-not (Test-Path "$QtPath/bin/qmake.exe")) {
    Write-Error "Qt not found at $QtPath"
    Write-Warn "Set correct path with -QtPath parameter"
    exit 1
}
Write-Debug "Qt found: $QtPath"

# Проверка CMake
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Write-Error "CMake not found in PATH"
    exit 1
}
Write-Debug "CMake: $($cmake.Version)"

# Проверка Git
$git = Get-Command git -ErrorAction SilentlyContinue
if (-not $git) {
    Write-Error "Git not found in PATH"
    exit 1
}
Write-Debug "Git: $($git.Version)"

# 2️⃣ Настройка vcpkg (если не пропущено)
if (-not $SkipVcpkg) {
    Write-Step "Setting up vcpkg..."
    
    if (-not (Test-Path "$VcpkgPath/vcpkg.exe")) {
        Write-Debug "Cloning vcpkg to $VcpkgPath..."
        if (Test-Path $VcpkgPath) { Remove-Item -Recurse -Force $VcpkgPath }
        git clone https://github.com/microsoft/vcpkg.git $VcpkgPath
        Check-Result "vcpkg clone"
        
        Write-Debug "Bootstrapping vcpkg..."
        Push-Location $VcpkgPath
        .\bootstrap-vcpkg.bat
        Check-Result "vcpkg bootstrap"
        Pop-Location
    }
    
    # Установка QuaZip для Qt6
    Write-Debug "Installing quazip[qt6]:x64-windows..."
    Push-Location $VcpkgPath
    .\vcpkg install "quazip[qt6]:x64-windows"
    Check-Result "vcpkg install quazip"
    Pop-Location
} else {
    Write-Warn "Skipping vcpkg setup (--SkipVcpkg)"
}

# 3️⃣ Очистка (если запрошено)
if ($Clean) {
    Write-Step "Cleaning build directory..."
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
        Write-Debug "build/ removed"
    }
}

# 4️⃣ Создание папки сборки
$BuildDir = "build"
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
    Write-Debug "Created $BuildDir/"
}

# 5️⃣ Генерация проекта через CMake
Write-Step "Configuring CMake..."
Push-Location $BuildDir

$cmakeArgs = @(
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgPath/scripts/buildsystems/vcpkg.cmake",
    "-DCMAKE_PREFIX_PATH=$QtPath",
    "-DVCPKG_TARGET_TRIPLET=x64-windows",
    "--debug-find-pkg=Qt6,QuaZip",  # 🔥 Отладка поиска пакетов
    ".."
)

Write-Debug "CMake command: cmake $($cmakeArgs -join ' ')"
cmake @cmakeArgs 2>&1 | Tee-Object -Variable cmakeLog
Check-Result "CMake configure"

# 🔥 Выводим, какие пакеты нашёл CMake
Write-Debug "=== CMake find_package results ==="
$cmakeLog | Select-String "Found.*:" | ForEach-Object { Write-Debug $_ }

Pop-Location

# 6️⃣ Сборка
Write-Step "Building project ($Configuration)..."
Push-Location $BuildDir

cmake --build . --config $Configuration --verbose 2>&1 | Tee-Object -Variable buildLog
Check-Result "CMake build"

Pop-Location

# 7️⃣ Поиск артефакта
$ExePath = "build/$Configuration/nsqcut.exe"
if (Test-Path $ExePath) {
    Write-Step "Build successful! ✓"
    Write-Host "  Executable: $ExePath" -ForegroundColor Green
    Write-Host "  Size: $((Get-Item $ExePath).Length / 1MB -as [int]) MB" -ForegroundColor Green
} else {
    Write-Warn "Executable not found at $ExePath"
    Write-Debug "Searching for .exe files in build/..."
    Get-ChildItem -Recurse -Path build -Filter "*.exe" | ForEach-Object {
        Write-Debug "  Found: $($_.FullName)"
    }
}

# 8️⃣ Деплой зависимостей Qt (опционально)
Write-Step "Deploying Qt dependencies..."
$windeployqt = "$QtPath/bin/windeployqt.exe"
if (Test-Path $windeployqt) {
    $targetExe = "build/$Configuration/nsqcut.exe"
    if (Test-Path $targetExe) {
        Write-Debug "Running windeployqt for $targetExe"
        & $windeployqt --release --dir build/deploy $targetExe
        Check-Result "windeployqt"
        Write-Debug "Qt dependencies deployed to build/deploy/"
    } else {
        Write-Warn "Executable not found, skipping windeployqt"
    }
} else {
    Write-Warn "windeployqt not found at $windeployqt"
}

# ==================== ФИНАЛ ====================
Write-Host "`n========================================" -ForegroundColor Green
Write-Host "  Build completed!" -ForegroundColor Green
Write-Host "========================================`n" -ForegroundColor Green

# 🔥 Быстрый тест запуска (если не в CI)
if ($env:CI -ne "true" -and (Test-Path $ExePath)) {
    Write-Host "  Test run: .\$ExePath" -ForegroundColor Cyan
    # & $ExePath  # Раскомментируйте для автозапуска
}