/**
 * @file modbus_app.c
 * @brief Modbus RTU应用层实现
 * @details 管理多个Modbus RTU实例的应用层
 * @author Lighting Ultra Team
 * @date 2025-11-08
 */

#include "modbus_app.h"
#include "modbus_port.h"
#include "main.h"     /* HAL GPIO */
#include "relay.h"  /* 继电器驱动 */
#include "led.h"    /* LED指示灯驱动 */
#include "pressure_sensor.h"  /* 压力传感器驱动 */
#include "water_level.h"  /* 水位检测驱动 */
#include "config_manager.h"  /* 配置管理 */
#include "error_handler.h"   /* 错误处理 */

/* ==================== Modbus实例 ==================== */
/* UART1 Modbus实例 */
ModbusRTU_t modbusUart1;

/* UART2 Modbus实例 */
ModbusRTU_t modbusUart2;

/* ==================== 数据存储 ==================== */
/* UART1数据存储 */
static uint16_t uart1_holdingRegs[100];
static uint16_t uart1_inputRegs[50];
static uint8_t uart1_coils[10];  /* 80 bits */
static uint8_t uart1_discreteInputs[5];  /* 40 bits */

/* UART2数据存储 */
static uint16_t uart2_holdingRegs[100];
static uint16_t uart2_inputRegs[50];
static uint8_t uart2_coils[10];  /* 80 bits */
static uint8_t uart2_discreteInputs[5];  /* 40 bits */

/* ==================== 回调函数 ==================== */
/**
 * @brief UART1线圈改变回调
 * @details 可以用于控制继电器
 */
static void setDoByIndex(uint16_t index, uint8_t on)
{
    if (index >= RELAY_CHANNEL_COUNT) return;
    
    /* 通过relay驱动统一管理继电器（高电平有效）*/
    RelayState_e newState = on ? RELAY_STATE_ON : RELAY_STATE_OFF;
    relaySetState((RelayChannel_e)index, newState);
    
    /* LED指示灯同步DO状态：LED1-LED4对应DO1-DO4（低电平点亮） */
    /* LED模块内部已有状态检查，只在状态改变时才操作GPIO */
    if (index < LED_CHANNEL_COUNT) {
        LedState_e ledState = on ? LED_STATE_ON : LED_STATE_OFF;
        ledSetState((LedChannel_e)index, ledState);
    }
}

static void uart1_onCoilChanged(uint16_t addr, uint8_t value)
{
    if (addr < 5) {
        setDoByIndex(addr, value ? 1 : 0);
        // 如需同步继电器驱动，可在此调用 relaySetState
    }
}

/**
 * @brief UART1寄存器改变回调
 */
static void uart1_onRegChanged(uint16_t addr, uint16_t value)
{
    /* 寄存器0的bit0..4控制DO1..DO5 */
    if (addr == 0) {
        for (uint16_t i = 0; i < 5; i++) {
            setDoByIndex(i, (value & (1 << i)) ? 1 : 0);
        }
    }
}

/**
 * @brief UART2线圈改变回调
 */
static void uart2_onCoilChanged(uint16_t addr, uint8_t value)
{
    if (addr < 5) {
        setDoByIndex(addr, value ? 1 : 0);
    }
}

/**
 * @brief UART2寄存器改变回调
 */
static void uart2_onRegChanged(uint16_t addr, uint16_t value)
{
    /* 寄存器0的bit0..4控制DO1..DO5 */
    if (addr == 0) {
        for (uint16_t i = 0; i < 5; i++) {
            setDoByIndex(i, (value & (1 << i)) ? 1 : 0);
        }
    }
}

/* ==================== 初始化函数 ==================== */
/**
 * @brief 初始化Modbus应用
 */
void ModbusApp_Init(void)
{
    uint16_t i;
    const SystemConfig_t* config = configGet();
    
    /* 初始化继电器驱动 */
    relayInit();
    
    /* ========== 初始化UART1 Modbus ========== */
    /* 设置从站地址（从配置读取） */
    ModbusRTU_SetSlaveAddr(&modbusUart1, config->modbus1SlaveAddr);
    
    /* 设置数据存储 */
    ModbusRTU_SetHoldingRegs(&modbusUart1, uart1_holdingRegs, 100);
    ModbusRTU_SetInputRegs(&modbusUart1, uart1_inputRegs, 50);
    ModbusRTU_SetCoils(&modbusUart1, uart1_coils, 80);
    ModbusRTU_SetDiscreteInputs(&modbusUart1, uart1_discreteInputs, 40);
    
    /* 设置回调函数 */
    modbusUart1.onCoilChanged = uart1_onCoilChanged;
    modbusUart1.onRegChanged = uart1_onRegChanged;
    
    /* 初始化测试数据 */
    for (i = 0; i < 100; i++) {
        uart1_holdingRegs[i] = 1000 + i;
    }
    for (i = 0; i < 50; i++) {
        uart1_inputRegs[i] = 2000 + i;
    }
    /* 前8个线圈设为ON */
    uart1_coils[0] = 0xFF;
    /* 离散输入测试模式 */
    uart1_discreteInputs[0] = 0xAA;
    
    /* 初始化硬件端口 */
    ModbusPort_UART1_Init(&modbusUart1);
    
    /* 初始化Modbus核心 */
    ModbusRTU_Init(&modbusUart1);
    
    /* ========== 初始化UART2 Modbus ========== */
    /* 设置从站地址（从配置读取） */
    ModbusRTU_SetSlaveAddr(&modbusUart2, config->modbus2SlaveAddr);
    
    /* 设置数据存储 */
    ModbusRTU_SetHoldingRegs(&modbusUart2, uart2_holdingRegs, 100);
    ModbusRTU_SetInputRegs(&modbusUart2, uart2_inputRegs, 50);
    ModbusRTU_SetCoils(&modbusUart2, uart2_coils, 80);
    ModbusRTU_SetDiscreteInputs(&modbusUart2, uart2_discreteInputs, 40);
    
    /* 设置回调函数 */
    modbusUart2.onCoilChanged = uart2_onCoilChanged;
    modbusUart2.onRegChanged = uart2_onRegChanged;
    
    /* 初始化测试数据 */
    for (i = 0; i < 100; i++) {
        uart2_holdingRegs[i] = 3000 + i;
    }
    for (i = 0; i < 50; i++) {
        uart2_inputRegs[i] = 4000 + i;
    }
    /* 前4个线圈设为ON */
    uart2_coils[0] = 0x0F;
    /* 离散输入测试模式 */
    uart2_discreteInputs[0] = 0x55;
    
    /* 初始化硬件端口 */
    ModbusPort_UART2_Init(&modbusUart2);
    
    /* 初始化Modbus核心 */
    ModbusRTU_Init(&modbusUart2);
}

/**
 * @brief Modbus应用主循环处理
 * @note 在main.c的主循环中调用
 */
void ModbusApp_Process(void)
{
    /* 处理UART1 Modbus */
    ModbusRTU_Process(&modbusUart1);
    
    /* 处理UART2 Modbus */
    ModbusRTU_Process(&modbusUart2);
}

/**
 * @brief 更新传感器数据
 * @note 在主循环中每1秒调用一次
 */
void ModbusApp_UpdateSensorData(void)
{
    static uint16_t counter = 0;
    counter++;
    
    /* 获取压力传感器数据 */
    PressureData_t pressureData = pressureSensorGetData();
    
    /* 更新UART1输入寄存器（实际传感器数据） */
    uart1_inputRegs[0] = counter;  /* 计数器 */
    
    /* 压力值（单位：0.001 MPa，即压力值×1000） */
    uart1_inputRegs[1] = (uint16_t)(pressureData.pressureFiltered * 1000.0f);
    
    /* 电流值（单位：0.01 mA，即电流值×100） */
    uart1_inputRegs[2] = (uint16_t)(pressureData.current * 100.0f);
    
    /* ADC原始值 */
    uart1_inputRegs[3] = pressureData.adcRaw;
    
    /* 数据有效标志 (0=无效, 1=有效) */
    uart1_inputRegs[4] = pressureData.isValid ? 1 : 0;
    
    /* 系统错误状态（位掩码） */
    uart1_inputRegs[5] = (uint16_t)(errorGetActiveMask() & 0xFFFF);
    
    /* 系统错误状态（高16位） */
    uart1_inputRegs[6] = (uint16_t)((errorGetActiveMask() >> 16) & 0xFFFF);
    
    /* 错误日志总数 */
    uart1_inputRegs[7] = errorLogGetCount();
    
    /* 更新UART2输入寄存器（可用于第二个传感器或备份） */
    uart2_inputRegs[0] = counter;
    uart2_inputRegs[1] = (uint16_t)(pressureData.pressureFiltered * 1000.0f);
    uart2_inputRegs[2] = (uint16_t)(pressureData.current * 100.0f);
    
    /* 获取水位检测数据 */
    WaterLevelState_e waterLevel = waterLevelGetLevel();
    bool lowProbe, midProbe, highProbe;
    waterLevelGetProbeStates(&lowProbe, &midProbe, &highProbe);
    
    /* 更新离散输入（字节0：压力传感器状态） */
    uart1_discreteInputs[0] = 0;
    if (pressureData.isValid) {
        uart1_discreteInputs[0] |= 0x01;  /* bit0: 压力数据有效 */
    }
    if (pressureData.pressureFiltered > 1.0f) {
        uart1_discreteInputs[0] |= 0x02;  /* bit1: 压力超过1.0 MPa */
    }
    if (pressureData.pressureFiltered > 1.4f) {
        uart1_discreteInputs[0] |= 0x04;  /* bit2: 压力超过1.4 MPa（报警） */
    }
    
    /* 更新离散输入（字节1：水位探针原始状态，0=有水） */
    uart1_discreteInputs[1] = 0;
    if (!lowProbe) uart1_discreteInputs[1] |= 0x01;  /* bit0: DI1低水位探针有水 */
    if (!midProbe) uart1_discreteInputs[1] |= 0x02;  /* bit1: DI2中水位探针有水 */
    if (!highProbe) uart1_discreteInputs[1] |= 0x04; /* bit2: DI3高水位探针有水 */
    
    /* 更新离散输入（字节2：水位状态编码） */
    uart1_discreteInputs[2] = 0;
    switch (waterLevel) {
        case WATER_LEVEL_NONE:
            uart1_discreteInputs[2] = 0x00;  /* 无水 */
            break;
        case WATER_LEVEL_LOW:
            uart1_discreteInputs[2] = 0x01;  /* 低水位 */
            break;
        case WATER_LEVEL_MID:
            uart1_discreteInputs[2] = 0x02;  /* 中水位 */
            break;
        case WATER_LEVEL_HIGH:
            uart1_discreteInputs[2] = 0x03;  /* 高水位 */
            break;
        case WATER_LEVEL_ERROR:
            uart1_discreteInputs[2] = 0xFF;  /* 异常 */
            break;
    }
    
    /* 水位状态稳定标志 */
    if (waterLevelIsStable()) {
        uart1_discreteInputs[2] |= 0x80;  /* bit7: 水位稳定 */
    }
}

/**
 * @brief 获取UART1 Modbus实例
 */
ModbusRTU_t* ModbusApp_GetUart1Instance(void)
{
    return &modbusUart1;
}

/**
 * @brief 获取UART2 Modbus实例
 */
ModbusRTU_t* ModbusApp_GetUart2Instance(void)
{
    return &modbusUart2;
}
