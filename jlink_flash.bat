@echo off
:: ===========================================================================
:: jlink_flash.bat
:: 一键编译 + 烧录 lighting_ultra 到 STM32F103C8T6
:: 硬件：J-Link SWD 接口
::
:: 用法：
::   jlink_flash.bat          -- 先编译再烧录
::   jlink_flash.bat norebuild -- 跳过编译，直接烧录已有 ELF
:: ===========================================================================
setlocal EnableDelayedExpansion

:: ---------- 路径配置 --------------------------------------------------------
set "JLINK_SERVER=D:\jlink\JLinkGDBServerCL.exe"
set "GDB=C:\Users\Administrator.DESKTOP-T0UF4T8\gcc-arm\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin\arm-none-eabi-gdb.exe"
set "ELF=build\output\lighting_ultra.elf"
set "DEVICE=STM32F103C8"
set "GDB_PORT=2331"
set "GDB_SCRIPT=GCC\gdb_flash.txt"

:: ---------- 编译 ------------------------------------------------------------
if /I not "%1"=="norebuild" (
    echo [BUILD] 编译固件...
    call gcc_build.bat
    if errorlevel 1 (
        echo [FAIL] 编译失败，终止烧录。
        exit /b 1
    )
)

:: ---------- 检查 ELF --------------------------------------------------------
if not exist "%ELF%" (
    echo [ERROR] 找不到 ELF 文件: %ELF%
    echo         请先运行 gcc_build.bat 生成固件。
    exit /b 1
)

:: ---------- 生成 GDB Flash 脚本 --------------------------------------------
(
echo target remote localhost:%GDB_PORT%
echo monitor reset
echo monitor halt
echo load
echo monitor reset
echo monitor go
echo quit
) > "%GDB_SCRIPT%"

:: ---------- 启动 J-Link GDB Server（后台） ---------------------------------
echo [FLASH] 启动 J-Link GDB Server (设备: %DEVICE%, SWD)...
start "JLinkGDBServer" "%JLINK_SERVER%" -device %DEVICE% -if SWD -speed 4000 -port %GDB_PORT% -nogui -singlerun

:: 等待 GDB Server 就绪
timeout /t 2 /nobreak >nul

:: ---------- GDB 批量烧录 ----------------------------------------------------
echo [FLASH] 烧录中...
"%GDB%" --batch -x "%GDB_SCRIPT%" "%ELF%"

if errorlevel 1 (
    echo.
    echo [FAIL] 烧录失败！请检查：
    echo   1. J-Link 是否已插好，SWD 接线（SWDIO/SWDCLK/GND/VCC）是否正确
    echo   2. 目标板已上电
    echo   3. 设备名称是否匹配（当前: %DEVICE%）
    taskkill /IM JLinkGDBServerCL.exe /F >nul 2>&1
    exit /b 1
)

echo.
echo ============================================================
echo  烧录成功！固件已写入 STM32F103C8T6
echo  ELF: %ELF%
echo ============================================================

taskkill /IM JLinkGDBServerCL.exe /F >nul 2>&1
endlocal
