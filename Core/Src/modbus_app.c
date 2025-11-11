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
    
    /* PB1只与DO1绑定：仅当操作DO1且状态真正改变时才更新PB1 */
    RelayState_e oldState = relayGetState((RelayChannel_e)index);
    RelayState_e newState = on ? RELAY_STATE_ON : RELAY_STATE_OFF;
    
    /* 通过relay驱动统一管理（高电平有效）*/
    relaySetState((RelayChannel_e)index, newState);
    
    /* PB1同步指示DO1状态（低电平点亮），仅当DO1状态改变时更新 */
    if (index == 0 && oldState != newState) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
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
    /* 初始化继电器驱动 */
    relayInit();
    
    /* ========== 初始化UART1 Modbus ========== */
    /* 设置从站地址 */
    ModbusRTU_SetSlaveAddr(&modbusUart1, 0x01);
    
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
    /* 设置从站地址 */
    ModbusRTU_SetSlaveAddr(&modbusUart2, 0x02);
    
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
 * @brief 更新传感器数据示例
 * @note 可以在定时器中断或主循环中调用
 */
void ModbusApp_UpdateSensorData(void)
{
    static uint16_t counter = 0;
    counter++;
    
    /* 更新UART1输入寄存器（模拟传感器数据） */
    uart1_inputRegs[0] = counter;  /* 计数器 */
    uart1_inputRegs[1] = 250 + (counter % 100);  /* 模拟温度 */
    uart1_inputRegs[2] = 500 + (counter % 50);   /* 模拟湿度 */
    
    /* 更新UART2输入寄存器 */
    uart2_inputRegs[0] = counter * 2;
    uart2_inputRegs[1] = 300 + (counter % 80);
    uart2_inputRegs[2] = 600 + (counter % 40);
    
    /* 更新离散输入（模拟开关状态） */
    if (counter % 100 == 0) {
        uart1_discreteInputs[0] = ~uart1_discreteInputs[0];
        uart2_discreteInputs[0] = ~uart2_discreteInputs[0];
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
