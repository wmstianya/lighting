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
