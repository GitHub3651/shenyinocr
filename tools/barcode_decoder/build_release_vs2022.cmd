@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "SOURCE_DIR=%SCRIPT_DIR:~0,-1%"
set "BUILD_DIR=%SOURCE_DIR%\build-vs2022"

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] CMake was not found. Install the C++ CMake tools in Visual Studio 2022.
    pause
    exit /b 1
)

cmake -S "%SOURCE_DIR%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b 1
)

cmake --build "%BUILD_DIR%" --config Release --target BarcodeDecoder
if errorlevel 1 (
    echo [ERROR] BarcodeDecoder build failed.
    pause
    exit /b 1
)

echo.
echo Build completed:
echo %BUILD_DIR%\dist\Release\BarcodeDecoder.dll
pause
