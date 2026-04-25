@echo off
:: ===========================================================================
:: jlink_debug.bat
:: 启动 J-Link GDB Server 并进入交互式 GDB 调试会话
:: 适用：STM32F103C8T6 / SWD 接口
::
:: 用法：
::   jlink_debug.bat            -- 进入交互式 GDB 命令行
::   jlink_debug.bat read       -- 读取关键变量（非交互，打印后退出）
::   jlink_debug.bat modbus     -- 读取 Modbus 统计+寄存器状态
::   jlink_debug.bat lamp       -- 读取灯管管理器状态
::   jlink_debug.bat stop       -- 仅关闭 GDB Server
:: ===========================================================================
setlocal EnableDelayedExpansion

set "JLINK_SERVER=D:\jlink\JLinkGDBServerCL.exe"
set "GDB=C:\Users\Administrator.DESKTOP-T0UF4T8\gcc-arm\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin\arm-none-eabi-gdb.exe"
set "ELF=build\output\lighting_ultra.elf"
set "DEVICE=STM32F103C8"
set "GDB_PORT=2331"

:: ---------- stop 模式 -------------------------------------------------------
if /I "%1"=="stop" (
    echo [STOP] 关闭 J-Link GDB Server...
    taskkill /IM JLinkGDBServerCL.exe /F >nul 2>&1
    echo [STOP] 完成。
    goto :EOF
)

:: ---------- 检查 ELF --------------------------------------------------------
if not exist "%ELF%" (
    echo [ERROR] 找不到 ELF: %ELF%
    echo         请先运行 jlink_flash.bat 编译并烧录固件。
    exit /b 1
)

:: ---------- 启动 GDB Server（后台，常驻）------------------------------------
echo [DEBUG] 启动 J-Link GDB Server (设备: %DEVICE%, SWD)...
start "JLinkGDBServer" "%JLINK_SERVER%" -device %DEVICE% -if SWD -speed 4000 -port %GDB_PORT% -nogui
timeout /t 2 /nobreak >nul

:: ===========================================================================
if /I "%1"=="read" goto :READ_VARS
if /I "%1"=="modbus" goto :READ_MODBUS
if /I "%1"=="lamp" goto :READ_LAMP
goto :INTERACTIVE

:: ---------- 模式: 读通用关键变量 -------------------------------------------
:READ_VARS
echo [READ] 读取关键运行时变量...
(
echo target remote localhost:%GDB_PORT%
echo monitor halt
echo echo === 系统心跳 ===\n
echo print heartbeat
echo echo === 传感器 ===\n
echo print pressureData
echo print waterLevel
echo echo === 错误状态 ===\n
echo print errorGetActiveMask()
echo monitor go
echo quit
) > GCC\gdb_read_vars.txt
"%GDB%" --batch -x GCC\gdb_read_vars.txt "%ELF%"
goto :CLEANUP

:: ---------- 模式: 读 Modbus 统计 -------------------------------------------
:READ_MODBUS
echo [READ] 读取 Modbus 通信统计...
(
echo target remote localhost:%GDB_PORT%
echo monitor halt
echo echo === UART1 Modbus 统计 ===\n
echo print modbusUart1.stats
echo echo === UART2 Modbus 统计 ===\n
echo print modbusUart2.stats
echo echo === UART1 状态机 ===\n
echo print modbusUart1.state
echo echo === UART1 Holding Regs [0..15] ===\n
echo x/16xh &uart1_holdingRegs
echo echo === UART1 Input  Regs [0..11] ===\n
echo x/12xh &uart1_inputRegs
echo monitor go
echo quit
) > GCC\gdb_read_modbus.txt
"%GDB%" --batch -x GCC\gdb_read_modbus.txt "%ELF%"
goto :CLEANUP

:: ---------- 模式: 读灯管状态 ------------------------------------------------
:READ_LAMP
echo [READ] 读取灯管管理器 + 调度器状态...
(
echo target remote localhost:%GDB_PORT%
echo monitor halt
echo echo === 灯管管理器内部状态 ===\n
echo print s_mode
echo print s_activeScene
echo print s_lastCommittedBits
echo print s_overrideActive
echo print s_rejectCount
echo echo === 调度器状态 ===\n
echo print s_curHHMM
echo print s_curDow
echo print s_enableMask
echo print s_activeEntry
echo print s_timeSynced
echo echo === 继电器实际状态 ===\n
echo x/1xb 0x40010C10
echo monitor go
echo quit
) > GCC\gdb_read_lamp.txt
"%GDB%" --batch -x GCC\gdb_read_lamp.txt "%ELF%"
goto :CLEANUP

:: ---------- 模式: 交互式 GDB ------------------------------------------------
:INTERACTIVE
echo.
echo ============================================================
echo  J-Link GDB 交互式调试会话
echo  ELF: %ELF%
echo  连接: localhost:%GDB_PORT%
echo.
echo  常用命令:
echo    target remote localhost:%GDB_PORT%  -- 连接
echo    monitor halt                         -- 暂停 CPU
echo    monitor go                           -- 继续运行
echo    monitor reset                        -- 复位
echo    print modbusUart1.stats              -- 打印变量
echo    x/4xw 0x20000000                     -- 读内存
echo    break main                           -- 设断点
echo    continue                             -- 断点后继续
echo    quit                                 -- 退出（GDB Server 继续运行）
echo    (完成后运行: jlink_debug.bat stop)
echo ============================================================
echo.
"%GDB%" "%ELF%"
goto :CLEANUP

:CLEANUP
echo.
echo [INFO] 调试完成。GDB Server 仍在后台运行。
echo        运行 jlink_debug.bat stop 关闭它。
endlocal
