[CmdletBinding()]
param(
    [string]$QtPath = "C:/Qt/6.6.0/msvc2019_64",
    [string]$VcpkgPath = "C:/vcpkg",
    [switch]$Clean,
    [switch]$SkipVcpkg
)

function Step { param($msg) Write-Host "[STEP] $msg" }
function Info { param($msg) Write-Host "  [INFO] $msg" }
function Error { param($msg) Write-Host "  [ERROR] $msg" -ForegroundColor Red }

Write-Host "[START] NSQCuT build script"

# Проверка путей
Step "Checking paths"
if (-not (Test-Path $QtPath)) { Error "Qt not found: $QtPath"; exit 1 }
Info "Qt: $QtPath"
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { Error "CMake not in PATH"; exit 1 }
Info "CMake: $((Get-Command cmake).Version)"
if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Error "Git not in PATH"; exit 1 }
Info "Git: $((Get-Command git).Version)"

# vcpkg
if (-not $SkipVcpkg) {
    Step "Setting up vcpkg"
    if (-not (Test-Path "$VcpkgPath/vcpkg.exe")) {
        Info "Cloning vcpkg to $VcpkgPath"
        if (Test-Path $VcpkgPath) { Remove-Item -Recurse -Force $VcpkgPath }
        git clone https://github.com/microsoft/vcpkg.git $VcpkgPath
        if ($LASTEXITCODE -ne 0) { Error "vcpkg clone failed"; exit 1 }
        Push-Location $VcpkgPath
        .\bootstrap-vcpkg.bat
        if ($LASTEXITCODE -ne 0) { Error "vcpkg bootstrap failed"; exit 1 }
        Pop-Location
    }
    Info "Installing quazip[qt6]"
    Push-Location $VcpkgPath
    .\vcpkg install "quazip[qt6]:x64-windows"
    if ($LASTEXITCODE -ne 0) { Error "vcpkg install failed"; exit 1 }
    Pop-Location
}

# Очистка
if ($Clean -and (Test-Path "build")) {
    Step "Cleaning build directory"
    Remove-Item -Recurse -Force "build"
}

# CMake configure
Step "Configuring CMake"
if (-not (Test-Path "build")) { New-Item -ItemType Directory -Path "build" | Out-Null }
Push-Location build
cmake -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_TOOLCHAIN_FILE="$VcpkgPath/scripts/buildsystems/vcpkg.cmake" `
    -DCMAKE_PREFIX_PATH="$QtPath" `
    -DVCPKG_TARGET_TRIPLET=x64-windows `
    .. 2>&1
if ($LASTEXITCODE -ne 0) { Error "CMake configure failed"; Pop-Location; exit 1 }
Pop-Location
Info "CMake configure OK"

# Build
Step "Building project"
Push-Location build
cmake --build . --config Release --verbose 2>&1
if ($LASTEXITCODE -ne 0) { Error "Build failed"; Pop-Location; exit 1 }
Pop-Location
Info "Build OK"

# Проверка артефакта
$exe = "build/Release/nsqcut.exe"
if (Test-Path $exe) {
    Info "Executable: $exe"
    Info "Size: $((Get-Item $exe).Length / 1MB) MB"
} else {
    Error "Executable not found: $exe"
    exit 1
}

# windeployqt
Step "Deploying Qt dependencies"
$wdqt = "$QtPath/bin/windeployqt.exe"
if (Test-Path $wdqt) {
    & $wdqt --release --dir build/deploy $exe 2>&1
    if ($LASTEXITCODE -ne 0) { Error "windeployqt failed" } else { Info "windeployqt OK" }
} else {
    Info "windeployqt not found, skipping"
}

Write-Host "[DONE] Build completed"