@echo off
REM Build script for StockMarketTracker CMake project (Windows)

setlocal enabledelayedexpansion

REM Default build type
set BUILD_TYPE=Release

REM Accept optional first argument for build type
if not "%1"=="" (
    set BUILD_TYPE=%1
)

echo [*] Building StockMarketTracker with CMake
echo [*] Build type: %BUILD_TYPE%

REM Configure
echo [*] Configuring CMake...
cmake -B build -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 (
    echo [ERROR] CMake configuration failed
    exit /b 1
)

REM Build
echo [*] Building...
cmake --build build --config %BUILD_TYPE% -j
if errorlevel 1 (
    echo [ERROR] Build failed
    exit /b 1
)

echo [OK] Build succeeded
echo [*] Output directory: build\
exit /b 0
