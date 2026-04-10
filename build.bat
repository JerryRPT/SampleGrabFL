@echo off
setlocal

echo ============================================
echo   SampleGrab Build Script
echo ============================================
echo.

:: ---- Step 1: CMake Configure ----
echo [1/3] Configuring CMake...
cmake -B build
if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: CMake configuration failed.
    echo Make sure CMake 3.20+ is installed and on your PATH.
    pause
    exit /b 1
)
echo.

:: ---- Step 2: CMake Build (Release) ----
echo [2/3] Building VST3 plugin (Release)...
cmake --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Build failed.
    echo Make sure Visual Studio 2022 Build Tools are installed.
    pause
    exit /b 1
)
echo.

:: ---- Step 3: Build Installer with Inno Setup ----
echo [3/3] Building installer...

:: Try common Inno Setup locations
set "ISCC="
if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" (
    set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
)
if exist "C:\Program Files\Inno Setup 6\ISCC.exe" (
    set "ISCC=C:\Program Files\Inno Setup 6\ISCC.exe"
)

if not defined ISCC (
    echo.
    echo ERROR: Inno Setup 6 not found.
    echo Install it from: https://jrsoftware.org/isdl.php
    echo Or set ISCC_PATH environment variable to ISCC.exe location.
    pause
    exit /b 1
)

"%ISCC%" installer.iss
if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Installer build failed.
    pause
    exit /b 1
)

echo.
echo ============================================
echo   Build complete!
echo   Installer: Output\SampleGrab_Windows_Installer.exe
echo ============================================
pause
