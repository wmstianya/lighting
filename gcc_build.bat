@echo off
:: ===========================================================================
:: gcc_build.bat
:: Direct GCC build for lighting_ultra (STM32F103C8T6)
:: No CMake required — runs arm-none-eabi-gcc directly.
::
:: Output:
::   build\output\lighting_ultra.elf
::   build\output\lighting_ultra.hex
::   build\output\lighting_ultra.bin
::   build\output\lighting_ultra.map
::
:: Usage:
::   Double-click OR run from command line:  gcc_build.bat
::   Clean build:                            gcc_build.bat clean
:: ===========================================================================
setlocal EnableDelayedExpansion

:: ---------- Toolchain -------------------------------------------------------
set "GCC_BIN=C:\Users\Administrator.DESKTOP-T0UF4T8\gcc-arm\xpack-arm-none-eabi-gcc-13.2.1-1.1\bin"
set "GCC=%GCC_BIN%\arm-none-eabi-gcc.exe"
set "OBJCOPY=%GCC_BIN%\arm-none-eabi-objcopy.exe"
set "SIZE=%GCC_BIN%\arm-none-eabi-size.exe"

:: ---------- Output directory ------------------------------------------------
set "OUTDIR=build\output"
set "ELF=%OUTDIR%\lighting_ultra.elf"
set "HEX=%OUTDIR%\lighting_ultra.hex"
set "BIN=%OUTDIR%\lighting_ultra.bin"
set "MAP=%OUTDIR%\lighting_ultra.map"

:: ---------- Clean target ----------------------------------------------------
if /I "%1"=="clean" (
    echo [CLEAN] Removing %OUTDIR% ...
    if exist "%OUTDIR%" rmdir /S /Q "%OUTDIR%"
    echo [CLEAN] Done.
    goto :EOF
)

:: ---------- Check toolchain -------------------------------------------------
if not exist "%GCC%" (
    echo [ERROR] arm-none-eabi-gcc not found at:
    echo         %GCC%
    echo         Please update GCC_BIN in this script.
    exit /b 1
)

:: ---------- Create output dir -----------------------------------------------
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

:: ---------- Compile flags ---------------------------------------------------
set "CPU=-mcpu=cortex-m3 -mthumb -mfloat-abi=soft"
set "CFLAGS=%CPU% -O1 -g -std=c11 -ffunction-sections -fdata-sections -fno-common"
set "CFLAGS=%CFLAGS% -Wall -Wno-unused-variable -Wno-unused-but-set-variable"
set "CFLAGS=%CFLAGS% -Wno-unused-function -Wno-implicit-function-declaration"
set "CFLAGS=%CFLAGS% -Wno-parentheses -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast"
set "CFLAGS=%CFLAGS% -Wno-misleading-indentation"

:: ---------- Compile definitions ---------------------------------------------
set "DEFS=-DUSE_HAL_DRIVER -DSTM32F103xB"

:: ---------- Include paths ---------------------------------------------------
set "INCS=-ICore\Inc"
set "INCS=%INCS% -IDrivers\STM32F1xx_HAL_Driver\Inc"
set "INCS=%INCS% -IDrivers\STM32F1xx_HAL_Driver\Inc\Legacy"
set "INCS=%INCS% -IDrivers\CMSIS\Device\ST\STM32F1xx\Include"
set "INCS=%INCS% -IDrivers\CMSIS\Include"

:: ---------- Linker flags ----------------------------------------------------
set "LDFLAGS=%CPU% -specs=nosys.specs -specs=nano.specs"
set "LDFLAGS=%LDFLAGS% -TGCC\STM32F103C8TX.ld"
set "LDFLAGS=%LDFLAGS% -Wl,--gc-sections -Wl,-Map=%MAP%,--cref"
set "LDFLAGS=%LDFLAGS% -lc -lm -lnosys"

:: ---------- Startup (ASM) ---------------------------------------------------
set "STARTUP=GCC\startup_stm32f103xb.s"

:: ---------- Application source files ----------------------------------------
set "APP_SRC="
set "APP_SRC=%APP_SRC% Core\Src\main.c"
set "APP_SRC=%APP_SRC% Core\Src\system_stm32f1xx.c"
set "APP_SRC=%APP_SRC% Core\Src\stm32f1xx_it.c"
set "APP_SRC=%APP_SRC% Core\Src\stm32f1xx_hal_msp.c"
set "APP_SRC=%APP_SRC% Core\Src\relay.c"
set "APP_SRC=%APP_SRC% Core\Src\beep.c"
set "APP_SRC=%APP_SRC% Core\Src\led.c"
set "APP_SRC=%APP_SRC% Core\Src\watchdog.c"
set "APP_SRC=%APP_SRC% Core\Src\kalman.c"
set "APP_SRC=%APP_SRC% Core\Src\filter.c"
set "APP_SRC=%APP_SRC% Core\Src\pressure_sensor.c"
set "APP_SRC=%APP_SRC% Core\Src\water_level.c"
set "APP_SRC=%APP_SRC% Core\Src\config_manager.c"
set "APP_SRC=%APP_SRC% Core\Src\error_handler.c"
set "APP_SRC=%APP_SRC% Core\Src\relay_test.c"
set "APP_SRC=%APP_SRC% Core\Src\modbus_rtu_core.c"
set "APP_SRC=%APP_SRC% Core\Src\modbus_port_uart1.c"
set "APP_SRC=%APP_SRC% Core\Src\modbus_port_uart2.c"
set "APP_SRC=%APP_SRC% Core\Src\modbus_app.c"
set "APP_SRC=%APP_SRC% Core\Src\lamp_manager.c"
set "APP_SRC=%APP_SRC% Core\Src\schedule.c"
set "APP_SRC=%APP_SRC% Core\Src\usart2_echo_test.c"

:: ---------- HAL driver source files -----------------------------------------
set "HAL_SRC="
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal.c"
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_rcc.c"
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_rcc_ex.c"
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_gpio.c"
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_gpio_ex.c"
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_dma.c"
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_cortex.c"
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_pwr.c"
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_flash.c"
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_flash_ex.c"
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_exti.c"
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_uart.c"
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_tim.c"
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_tim_ex.c"
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_adc.c"
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_adc_ex.c"
set "HAL_SRC=%HAL_SRC% Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_iwdg.c"

:: ---------- Build -----------------------------------------------------------
echo.
echo ============================================================
echo  lighting_ultra GCC Build  (STM32F103C8T6)
echo ============================================================
echo  GCC      : %GCC%
echo  Output   : %ELF%
echo ============================================================
echo.

:: Assemble + Compile + Link in one pass (simpler, no intermediate .o files)
echo [BUILD] Compiling and linking ...
"%GCC%" %CFLAGS% %DEFS% %INCS% ^
  %STARTUP% ^
  %APP_SRC% ^
  %HAL_SRC% ^
  %LDFLAGS% ^
  -o "%ELF%"

if errorlevel 1 (
    echo.
    echo [FAIL] Build failed.  See errors above.
    exit /b 1
)

:: ---------- Post-build: HEX + BIN + SIZE ------------------------------------
echo [HEX ] Generating %HEX% ...
"%OBJCOPY%" -O ihex   "%ELF%" "%HEX%"

echo [BIN ] Generating %BIN% ...
"%OBJCOPY%" -O binary "%ELF%" "%BIN%"

echo.
echo [SIZE] Memory usage:
"%SIZE%" --format=berkeley "%ELF%"

echo.
echo ============================================================
echo  Build successful!
echo  ELF : %ELF%
echo  HEX : %HEX%
echo  BIN : %BIN%
echo  MAP : %MAP%
echo ============================================================
echo.

endlocal
