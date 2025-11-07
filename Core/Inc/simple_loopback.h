/**
 * @file simple_loopback.h
 * @brief 简单串口回环测试头文件
 * @details
 * 提供最简单的串口回环测试，验证UART+DMA+RS485硬件功能
 * 
 * @author Lighting Ultra Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#ifndef SIMPLE_LOOPBACK_H
#define SIMPLE_LOOPBACK_H

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// 公共函数声明 (Public Function Prototypes)
//=============================================================================

/**
 * @brief 初始化简单回环测试
 */
void simpleLoopbackInit(void);

/**
 * @brief 简单回环测试主循环
 */
void simpleLoopbackPoll(void);

/**
 * @brief 处理UART1 IDLE中断（在中断函数中调用）
 */
void simpleLoopbackHandleIdle1(void);

/**
 * @brief 获取回环测试统计信息
 * @param packets 输出总包数
 * @param errors 输出错误计数
 */
void simpleLoopbackGetStats(uint32_t* packets, uint32_t* errors);

/**
 * @brief 重置统计信息
 */
void simpleLoopbackResetStats(void);

#endif // SIMPLE_LOOPBACK_H

