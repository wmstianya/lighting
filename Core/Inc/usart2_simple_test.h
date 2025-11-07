/**
 * @file usart2_simple_test.h
 * @brief USART2最简化测试头文件
 */

#ifndef USART2_SIMPLE_TEST_H
#define USART2_SIMPLE_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

void usart2SimpleTestInit(void);
void usart2SimpleHandleIdle(void);
void usart2SimpleTxCallback(UART_HandleTypeDef *huart);
void usart2SimpleProcess(void);
void usart2SimpleTestRun(void);

#ifdef __cplusplus
}
#endif

#endif /* USART2_SIMPLE_TEST_H */

