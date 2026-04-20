/**
 * @file uart_echo_test.h  
 * @brief UART回环测试头文件
 */

#ifndef UART_ECHO_TEST_H
#define UART_ECHO_TEST_H

#include <stdint.h>

/**
 * @brief 初始化回环测试
 */
void uartEchoInit(void);

/**
 * @brief 回环测试主循环处理
 */
void uartEchoPoll(void);

/**
 * @brief 处理UART1 IDLE中断
 */
void uartEchoHandleIdle(void);

/**
 * @brief 获取统计信息
 */
uint32_t uartEchoGetCount(void);

#endif

















