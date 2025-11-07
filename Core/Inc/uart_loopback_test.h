/**
 * @file    uart_loopback_test.h
 * @brief   双串口回环测试模块
 * @author  Senior Engineer
 * @date    2025-10-18
 * @version 1.0.0
 * 
 * @details
 * 提供三种测试模式：
 * 1. 硬件回环测试 - 验证UART+DMA基础功能
 * 2. RS485方向控制测试 - 验证DE/RE切换
 * 3. Modbus双串口互测 - 验证完整通信链路
 */

#ifndef __UART_LOOPBACK_TEST_H
#define __UART_LOOPBACK_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ========== 测试配置 ========== */
#define TEST_BUFFER_SIZE        256U
#define TEST_TIMEOUT_MS         1000U
#define TEST_PATTERN_SIZE       16U

/* ========== 测试结果结构体 ========== */
typedef struct {
    uint32_t totalTests;
    uint32_t passedTests;
    uint32_t failedTests;
    uint32_t timeoutErrors;
    uint32_t crcErrors;
    uint32_t txCount;
    uint32_t rxCount;
    char     lastError[64];
} TestResult;

/* ========== 测试模式枚举 ========== */
typedef enum {
    TEST_MODE_HARDWARE_LOOPBACK = 0,  /* 硬件回环（TX-RX短接） */
    TEST_MODE_RS485_DIRECTION,         /* RS485方向控制测试 */
    TEST_MODE_DUAL_UART_CROSSOVER,     /* 双串口交叉测试 */
    TEST_MODE_MODBUS_LOOPBACK,         /* Modbus回环测试 */
    TEST_MODE_STRESS_TEST              /* 压力测试 */
} TestMode;

/* ========== API 函数声明 ========== */

/**
 * @brief  初始化测试模块
 * @param  huart1  串口2句柄 (USART1: PA9/PA10)
 * @param  huart2  串口1句柄 (USART2: PA2/PA3)
 * @retval None
 */
void loopbackTestInit(UART_HandleTypeDef *huart1, UART_HandleTypeDef *huart2);

/**
 * @brief  执行单次回环测试
 * @param  mode    测试模式
 * @param  result  测试结果输出
 * @retval true=通过, false=失败
 */
bool loopbackTestRun(TestMode mode, TestResult *result);

/**
 * @brief  执行完整测试套件
 * @retval 通过的测试数量
 */
uint32_t loopbackTestRunAll(void);

/**
 * @brief  打印测试结果
 * @param  result  测试结果
 * @retval None
 */
void loopbackTestPrintResult(const TestResult *result);

/**
 * @brief  硬件回环测试 - 验证UART基础功能
 * @param  huart   要测试的串口
 * @param  result  测试结果
 * @retval true=通过
 * 
 * @note   需要将TX和RX引脚短接
 */
bool loopbackTestHardware(UART_HandleTypeDef *huart, TestResult *result);

/**
 * @brief  RS485方向控制测试
 * @param  huart   要测试的串口
 * @param  result  测试结果
 * @retval true=通过
 * 
 * @note   验证PA4/PA8的DE/RE切换
 */
bool loopbackTestRS485Direction(UART_HandleTypeDef *huart, TestResult *result);

/**
 * @brief  双串口交叉测试
 * @param  result  测试结果
 * @retval true=通过
 * 
 * @note   串口1 TX -> 串口2 RX, 串口2 TX -> 串口1 RX
 */
bool loopbackTestDualCrossover(TestResult *result);

/**
 * @brief  Modbus功能回环测试
 * @param  result  测试结果
 * @retval true=通过
 * 
 * @note   测试Modbus读写寄存器功能
 */
bool loopbackTestModbus(TestResult *result);

/**
 * @brief  压力测试 - 连续发送接收
 * @param  count   测试次数
 * @param  result  测试结果
 * @retval true=通过
 */
bool loopbackTestStress(uint32_t count, TestResult *result);

/**
 * @brief  生成测试数据模式
 * @param  buffer  输出缓冲区
 * @param  size    缓冲区大小
 * @param  pattern 模式类型 (0=递增, 1=0xAA, 2=随机)
 * @retval None
 */
void loopbackTestGeneratePattern(uint8_t *buffer, uint16_t size, uint8_t pattern);

/**
 * @brief  验证接收数据
 * @param  expected  期望数据
 * @param  received  接收数据
 * @param  size      数据长度
 * @retval true=匹配
 */
bool loopbackTestVerifyData(const uint8_t *expected, const uint8_t *received, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif /* __UART_LOOPBACK_TEST_H */

