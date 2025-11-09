/**
 * @file modbus_port.h
 * @brief Modbus RTU硬件抽象层头文件
 * @details 提供STM32F103的UART硬件接口
 * @author Lighting Ultra Team
 * @date 2025-11-08
 */

#ifndef __MODBUS_PORT_H
#define __MODBUS_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "modbus_rtu_core.h"
#include "stm32f1xx_hal.h"

/* ==================== 端口配置结构 ==================== */
typedef struct {
    /* UART硬件 */
    UART_HandleTypeDef *huart;
    DMA_HandleTypeDef *hdma_rx;
    DMA_HandleTypeDef *hdma_tx;
    
    /* RS485控制引脚 */
    GPIO_TypeDef *rs485Port;
    uint16_t rs485Pin;
    
    /* LED指示引脚（可选） */
    GPIO_TypeDef *ledPort;
    uint16_t ledPin;
    
    /* 关联的Modbus实例 */
    ModbusRTU_t *modbusInstance;
    
} ModbusPort_t;

/* ==================== 全局端口实例声明 ==================== */
extern ModbusPort_t modbusPortUart1;
extern ModbusPort_t modbusPortUart2;

/* ==================== 端口初始化函数 ==================== */
/* UART1端口初始化 */
void ModbusPort_UART1_Init(ModbusRTU_t *mb);
void ModbusPort_UART1_DeInit(void);

/* UART2端口初始化 */
void ModbusPort_UART2_Init(ModbusRTU_t *mb);
void ModbusPort_UART2_DeInit(void);

/* ==================== 硬件操作函数 ==================== */
/* UART1硬件操作 */
void ModbusPort_UART1_SendData(void *port, uint8_t *data, uint16_t len);
void ModbusPort_UART1_SetRS485Dir(void *port, uint8_t txMode);
void ModbusPort_UART1_LedIndicate(void *port, uint8_t state);
void ModbusPort_UART1_StartReceive(void);

/* UART2硬件操作 */
void ModbusPort_UART2_SendData(void *port, uint8_t *data, uint16_t len);
void ModbusPort_UART2_SetRS485Dir(void *port, uint8_t txMode);
void ModbusPort_UART2_LedIndicate(void *port, uint8_t state);
void ModbusPort_UART2_StartReceive(void);

/* ==================== 中断处理函数 ==================== */
/* UART1中断处理 */
void ModbusPort_UART1_IdleCallback(void);
void ModbusPort_UART1_TxCpltCallback(void);

/* UART2中断处理 */
void ModbusPort_UART2_IdleCallback(void);
void ModbusPort_UART2_TxCpltCallback(void);

/* ==================== 系统函数 ==================== */
uint32_t ModbusPort_GetSysTick(void);

#ifdef __cplusplus
}
#endif

#endif /* __MODBUS_PORT_H */
