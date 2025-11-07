/**
 * @file usart2_echo_test.h
 * @brief USART2串口回环测试头文件
 * @details 用于验证串口2 (PA2/PA3) DMA收发功能
 * @author Lighting Ultra Team
 * @date 2025-11-05
 */

#ifndef USART2_ECHO_TEST_H
#define USART2_ECHO_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32f1xx_hal.h"

/* TIM3句柄（用于非阻塞延迟） - 暂时不使用 */
// extern TIM_HandleTypeDef htim3;

/**
 * @brief 初始化串口2回环测试
 */
void usart2EchoTestInit(void);

/**
 * @brief IDLE中断处理（在中断中调用）
 */
void usart2EchoHandleIdle(void);

/**
 * @brief 处理回环逻辑（在主循环中调用）
 * @note 纯数据回环，无调试信息
 */
void usart2EchoProcess(void);

/**
 * @brief 处理回环逻辑（带调试信息版本）
 * @note 会打印HEX和ASCII格式，用于调试
 */
void usart2EchoProcessDebug(void);

/**
 * @brief DMA发送完成回调
 */
void usart2EchoTxCallback(UART_HandleTypeDef *huart);

/**
 * @brief TIM3中断回调（RS485延迟切换） - 暂时不使用
 */
// void usart2EchoTim3Callback(void);

/**
 * @brief 运行完整回环测试
 */
void usart2EchoTestRun(void);

/**
 * @brief 获取诊断计数器（用于调试）
 */
void usart2EchoGetDiagnostics(uint32_t *idle, uint32_t *process, uint32_t *txCplt, uint32_t *tim3, uint32_t *dmaTx);

#ifdef __cplusplus
}
#endif

#endif /* USART2_ECHO_TEST_H */

