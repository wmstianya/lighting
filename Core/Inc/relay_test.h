/**
 * @file relay_test.h
 * @brief 继电器功能测试模块头文件
 * @details
 * 提供继电器系统的测试和验证功能，包括单路测试、批量测试等。
 * 
 * @author Lighting Ultra Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#ifndef RELAY_TEST_H
#define RELAY_TEST_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

//=============================================================================
// 测试功能函数声明 (Test Function Prototypes)
//=============================================================================

/**
 * @brief 继电器系统自检测试
 * @details 逐个测试每个继电器的开关功能，每个继电器开启500ms后关闭
 * @param None
 * @return bool 测试结果
 * @retval true 所有继电器测试通过
 * @retval false 存在继电器测试失败
 */
bool relaySystemSelfTest(void);

/**
 * @brief 继电器流水灯测试
 * @details 按顺序开启继电器，形成流水灯效果
 * @param delayMs 每个继电器的点亮时间 (毫秒)
 * @param cycles 循环次数
 * @return HAL_StatusTypeDef HAL状态码
 */
HAL_StatusTypeDef relayRunningLightTest(uint32_t delayMs, uint8_t cycles);

/**
 * @brief 继电器同步闪烁测试
 * @details 所有继电器同时开启和关闭
 * @param flashCount 闪烁次数
 * @param onTimeMs 开启时间 (毫秒)
 * @param offTimeMs 关闭时间 (毫秒)  
 * @return HAL_StatusTypeDef HAL状态码
 */
HAL_StatusTypeDef relaySyncFlashTest(uint8_t flashCount, uint32_t onTimeMs, uint32_t offTimeMs);

/**
 * @brief 测试指定继电器的响应时间
 * @param channel 继电器通道
 * @param testCount 测试次数
 * @return uint32_t 平均响应时间 (微秒)
 */
//uint32_t relayResponseTimeTest(RelayChannel_e channel, uint8_t testCount);

/**
 * @brief 打印继电器测试报告
 * @details 通过调试串口输出测试结果 (需要实现printf重定向)
 * @param None
 */
void relayPrintTestReport(void);

#endif // RELAY_TEST_H
