/**
 * @file usart2_modbus_rtu.h
 * @brief USART2 Modbus RTU从站头文件
 * @details Modbus RTU协议接口定义
 * @author Lighting Ultra Team
 * @date 2025-11-08
 */

#ifndef __USART2_MODBUS_RTU_H
#define __USART2_MODBUS_RTU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* Modbus统计信息结构体 */
typedef struct {
    uint32_t rxFrameCount;      /* 接收帧计数 */
    uint32_t txFrameCount;      /* 发送帧计数 */
    uint32_t errorCount;        /* 错误计数 */
    uint32_t crcErrorCount;     /* CRC错误计数 */
} ModbusStats;

/* ==================== 主要接口函数 ==================== */
/* Modbus RTU初始化和运行 */
void modbusRtuInit(void);
void modbusRtuProcess(void);
void modbusRtuRun(void);

/* 中断处理回调 */
void modbusHandleIdle(void);
void modbusTxCallback(UART_HandleTypeDef *huart);

/* 寄存器操作 */
uint16_t modbusReadReg(uint16_t addr);
void modbusWriteReg(uint16_t addr, uint16_t value);

/* 获取统计信息 */
ModbusStats* modbusGetStats(void);

/* ==================== 兼容性接口 ==================== */
/* 保留原有函数名，实际调用Modbus功能 */
void usart2EchoTestInit(void);
void usart2EchoHandleIdle(void);
void usart2EchoProcess(void);
void usart2EchoTxCallback(UART_HandleTypeDef *huart);
void usart2EchoTestRun(void);

#ifdef __cplusplus
}
#endif

#endif /* __USART2_MODBUS_RTU_H */
