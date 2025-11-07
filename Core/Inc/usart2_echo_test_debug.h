/**
 * @file usart2_echo_test_debug.h
 * @brief USART2调试版本头文件
 */

#ifndef USART2_ECHO_TEST_DEBUG_H
#define USART2_ECHO_TEST_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* TIM3句柄（调试版本） */
extern TIM_HandleTypeDef htim3_debug;

void usart2DebugTestInit(void);
void usart2DebugHandleIdle(void);
void usart2DebugProcess(void);
void usart2DebugTxCallback(UART_HandleTypeDef *huart);
void usart2DebugTim3Callback(void);
void usart2DebugTestRun(void);

#ifdef __cplusplus
}
#endif

#endif /* USART2_ECHO_TEST_DEBUG_H */

