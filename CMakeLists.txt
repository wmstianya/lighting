cmake_minimum_required(VERSION 3.20)

# ─── Toolchain must be set BEFORE project() ───────────────────────────────
# Run CMake as:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-toolchain.cmake \
#         -DCMAKE_BUILD_TYPE=MinSizeRel \
#         -G Ninja  ..
# ──────────────────────────────────────────────────────────────────────────

project(lighting_ultra
    VERSION     2.0.0
    DESCRIPTION "STM32F103C8 Dual-UART Modbus RTU Lamp Control Firmware"
    LANGUAGES   C ASM
)

# ─── Build type defaults ──────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ─── Output directory ─────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/output)
set(ELF_FILE ${PROJECT_NAME}.elf)
set(HEX_FILE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}.hex)
set(BIN_FILE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}.bin)
set(MAP_FILE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}.map)

# ─── Linker script ────────────────────────────────────────────────────────
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/GCC/STM32F103C8TX.ld)

# ─── Compile definitions ──────────────────────────────────────────────────
set(COMPILE_DEFS
    USE_HAL_DRIVER
    STM32F103xB
)

# ─── Include paths ────────────────────────────────────────────────────────
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F1xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)

# ─── Source files ─────────────────────────────────────────────────────────

# Startup (GAS format)
set(STARTUP_SRC
    ${CMAKE_SOURCE_DIR}/GCC/startup_stm32f103xb.s
)

# Application layer
set(APP_SRC
    ${CMAKE_SOURCE_DIR}/Core/Src/main.c
    ${CMAKE_SOURCE_DIR}/Core/Src/system_stm32f1xx.c
    ${CMAKE_SOURCE_DIR}/Core/Src/stm32f1xx_it.c
    ${CMAKE_SOURCE_DIR}/Core/Src/stm32f1xx_hal_msp.c
    # Peripherals
    ${CMAKE_SOURCE_DIR}/Core/Src/relay.c
    ${CMAKE_SOURCE_DIR}/Core/Src/beep.c
    ${CMAKE_SOURCE_DIR}/Core/Src/led.c
    ${CMAKE_SOURCE_DIR}/Core/Src/watchdog.c
    ${CMAKE_SOURCE_DIR}/Core/Src/kalman.c
    ${CMAKE_SOURCE_DIR}/Core/Src/filter.c
    ${CMAKE_SOURCE_DIR}/Core/Src/pressure_sensor.c
    ${CMAKE_SOURCE_DIR}/Core/Src/water_level.c
    ${CMAKE_SOURCE_DIR}/Core/Src/config_manager.c
    ${CMAKE_SOURCE_DIR}/Core/Src/error_handler.c
    ${CMAKE_SOURCE_DIR}/Core/Src/relay_test.c
    # Modbus stack
    ${CMAKE_SOURCE_DIR}/Core/Src/modbus_rtu_core.c
    ${CMAKE_SOURCE_DIR}/Core/Src/modbus_port_uart1.c
    ${CMAKE_SOURCE_DIR}/Core/Src/modbus_port_uart2.c
    ${CMAKE_SOURCE_DIR}/Core/Src/modbus_app.c
    # Lamp control (scene engine + scheduler)
    ${CMAKE_SOURCE_DIR}/Core/Src/lamp_manager.c
    ${CMAKE_SOURCE_DIR}/Core/Src/schedule.c
    # Echo test (compiled but unused at RUN_MODE=10 — gc-sections removes dead code)
    ${CMAKE_SOURCE_DIR}/Core/Src/usart2_echo_test.c
)

# STM32F1xx HAL Driver
set(HAL_SRC
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc_ex.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_iwdg.c
)

# ─── Compiler / Assembler flags ───────────────────────────────────────────
set(COMMON_FLAGS
    -mcpu=cortex-m3
    -mthumb
    -mfloat-abi=soft
    -ffunction-sections
    -fdata-sections
    -fno-common
    -fmessage-length=0
    -Wall
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-unused-function
    -Wno-implicit-function-declaration
    -Wno-parentheses
    -Wno-pointer-to-int-cast
    -Wno-int-to-pointer-cast
    -Wno-misleading-indentation
)

set(C_LANGUAGE_FLAGS
    ${COMMON_FLAGS}
    -std=c11
)

set(ASM_FLAGS
    ${COMMON_FLAGS}
    -x assembler-with-cpp
)

# Build-type specific optimisation
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND C_LANGUAGE_FLAGS -Og -g3 -DDEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    list(APPEND C_LANGUAGE_FLAGS -Os)
else()
    list(APPEND C_LANGUAGE_FLAGS -O1)
endif()

# ─── Linker flags ─────────────────────────────────────────────────────────
set(LINKER_FLAGS
    -mcpu=cortex-m3
    -mthumb
    -mfloat-abi=soft
    -specs=nosys.specs
    -specs=nano.specs
    -T${LINKER_SCRIPT}
    -Wl,--gc-sections
    -Wl,-Map=${MAP_FILE},--cref
    -lc -lm -lnosys
)

# ─── Target definition ────────────────────────────────────────────────────
add_executable(${ELF_FILE}
    ${STARTUP_SRC}
    ${APP_SRC}
    ${HAL_SRC}
)

target_compile_definitions(${ELF_FILE} PRIVATE ${COMPILE_DEFS})
target_include_directories(${ELF_FILE} PRIVATE ${INCLUDE_DIRS})

target_compile_options(${ELF_FILE} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:${C_LANGUAGE_FLAGS}>
    $<$<COMPILE_LANGUAGE:ASM>:${ASM_FLAGS}>
)

# Join linker flags from list → string (CMake target_link_options needs list)
target_link_options(${ELF_FILE} PRIVATE ${LINKER_FLAGS})

# ─── Post-build: generate HEX and BIN ─────────────────────────────────────
find_program(CMAKE_OBJCOPY
    NAMES arm-none-eabi-objcopy
    HINTS "C:/Users/Administrator.DESKTOP-T0UF4T8/gcc-arm/xpack-arm-none-eabi-gcc-13.2.1-1.1/bin"
    REQUIRED
)
find_program(CMAKE_SIZE
    NAMES arm-none-eabi-size
    HINTS "C:/Users/Administrator.DESKTOP-T0UF4T8/gcc-arm/xpack-arm-none-eabi-gcc-13.2.1-1.1/bin"
    REQUIRED
)

add_custom_command(TARGET ${ELF_FILE} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${ELF_FILE}> ${HEX_FILE}
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${ELF_FILE}> ${BIN_FILE}
    COMMAND ${CMAKE_SIZE}    --format=berkeley $<TARGET_FILE:${ELF_FILE}>
    COMMENT "==> Generating HEX / BIN / SIZE report"
    VERBATIM
)

# ─── Summary ──────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "=== lighting_ultra build configuration ===")
message(STATUS "  Project     : ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "  Build type  : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler    : ${CMAKE_C_COMPILER}")
message(STATUS "  Linker      : ${LINKER_SCRIPT}")
message(STATUS "  Output ELF  : build/output/${ELF_FILE}")
message(STATUS "  Output HEX  : ${HEX_FILE}")
message(STATUS "  Output BIN  : ${BIN_FILE}")
message(STATUS "==========================================")
message(STATUS "")
