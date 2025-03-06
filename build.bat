@echo off
echo Building Stock Market Tracker...

if not exist build mkdir build
cd build

cmake -G "Visual Studio 16 2019" -A x64 ..
if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed
    exit /b %ERRORLEVEL%
)

cmake --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo Build failed
    exit /b %ERRORLEVEL%
)

echo Build completed successfully
echo Executable location: %~dp0\build\bin\Release\stock_market_tracker.exe
echo.
echo To run the application:
echo %~dp0\build\bin\Release\stock_market_tracker.exe

cd ..
