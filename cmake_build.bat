@echo off
:: ===========================================================================
:: cmake_build.bat
:: CMake-based build for lighting_ultra (STM32F103C8T6)
::
:: Prerequisites:
::   1. CMake >= 3.20   → https://cmake.org/download/
::   2. Ninja           → https://github.com/ninja-build/ninja/releases
::      (or use "MinGW Makefiles" generator by changing -G below)
::   3. arm-none-eabi-gcc (xPack 13.2)
::
:: First-time setup (run as Administrator):
::   winget install Kitware.CMake
::   winget install Ninja-build.Ninja
::
:: Usage:
::   cmake_build.bat              → Release build
::   cmake_build.bat Debug        → Debug build (-Og -g3)
::   cmake_build.bat MinSizeRel   → Size-optimized build (-Os)
::   cmake_build.bat clean        → Delete build directory
:: ===========================================================================
setlocal EnableDelayedExpansion

set "BUILD_TYPE=%~1"
if "%BUILD_TYPE%"=="" set "BUILD_TYPE=Release"

set "BUILD_DIR=build\cmake_%BUILD_TYPE%"
set "OUTDIR=build\output"
set "TOOLCHAIN_FILE=cmake\arm-none-eabi-toolchain.cmake"

:: ---------- Clean target ----------------------------------------------------
if /I "%BUILD_TYPE%"=="clean" (
    echo [CLEAN] Removing build directory ...
    if exist "build" rmdir /S /Q "build"
    echo [CLEAN] Done.
    goto :EOF
)

:: ---------- Check prerequisites ---------------------------------------------
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake not found on PATH.
    echo.
    echo  Install via winget (run as Administrator):
    echo    winget install Kitware.CMake
    echo    winget install Ninja-build.Ninja
    echo.
    echo  Or download from: https://cmake.org/download/
    echo.
    echo  Alternatively, use gcc_build.bat (no CMake required).
    exit /b 1
)

where ninja >nul 2>&1
if errorlevel 1 (
    echo [WARN] Ninja not found. Falling back to -G "MinGW Makefiles".
    echo        Install Ninja for faster builds: winget install Ninja-build.Ninja
    set "GENERATOR=MinGW Makefiles"
) else (
    set "GENERATOR=Ninja"
)

:: ---------- Create build dir ------------------------------------------------
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%OUTDIR%"   mkdir "%OUTDIR%"

echo.
echo ============================================================
echo  lighting_ultra CMake Build
echo  Type      : %BUILD_TYPE%
echo  Generator : %GENERATOR%
echo  Build dir : %BUILD_DIR%
echo ============================================================
echo.

:: ---------- Configure -------------------------------------------------------
echo [CMAKE] Configuring ...
cmake -S . -B "%BUILD_DIR%" ^
    -G "%GENERATOR%" ^
    -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN_FILE%" ^
    -DCMAKE_BUILD_TYPE="%BUILD_TYPE%" ^
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="%CD%\%OUTDIR%"

if errorlevel 1 (
    echo [FAIL] CMake configure failed.
    exit /b 1
)

:: ---------- Build -----------------------------------------------------------
echo.
echo [CMAKE] Building ...
cmake --build "%BUILD_DIR%" --parallel

if errorlevel 1 (
    echo.
    echo [FAIL] Build failed.  See errors above.
    exit /b 1
)

echo.
echo ============================================================
echo  Build successful!
echo  Artifacts are in:  %OUTDIR%\
echo    lighting_ultra.elf
echo    lighting_ultra.hex
echo    lighting_ultra.bin
echo    lighting_ultra.map
echo ============================================================
echo.

endlocal
