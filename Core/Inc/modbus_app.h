/**
 * @file modbus_app.h
 * @brief Modbus RTU应用层头文件
 * @details 管理多个Modbus RTU实例的应用层接口
 * @author Lighting Ultra Team
 * @date 2025-11-08
 */

#ifndef __MODBUS_APP_H
#define __MODBUS_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "modbus_rtu_core.h"

/* ==================== 功能开关 ==================== */
/**
 * @brief 双串口虚拟寄存器同步开关
 * @details 
 * - 设置为1：UART1和UART2的线圈/寄存器相互同步，状态一致
 * - 设置为0：UART1和UART2的线圈/寄存器独立，互不影响
 * @note 
 * - 启用同步：两个串口看到的DO状态永远一致
 * - 性能影响：<0.3μs/次操作，可忽略不计
 * - 适用场景：两个串口控制同一组继电器
 */
#define ENABLE_DUAL_UART_SYNC   1   /* 0=独立模式, 1=同步模式 */

/* ==================== 全局Modbus实例 ==================== */
extern ModbusRTU_t modbusUart1;
extern ModbusRTU_t modbusUart2;

/* ==================== API函数 ==================== */
/* 初始化和处理 */
void ModbusApp_Init(void);
void ModbusApp_Process(void);

/* 数据更新 */
void ModbusApp_UpdateSensorData(void);

/* 获取实例 */
ModbusRTU_t* ModbusApp_GetUart1Instance(void);
ModbusRTU_t* ModbusApp_GetUart2Instance(void);

#ifdef __cplusplus
}
#endif

#endif /* __MODBUS_APP_H */
