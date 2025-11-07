/**
 * @file    uart_loopback_test.c
 * @brief   双串口回环测试实现
 * @author  Senior Engineer
 * @date    2025-10-18
 * @version 1.0.0
 */

#include "uart_loopback_test.h"
#include "../../MDK-ARM/modbus_rtu_slave.h"
#include <string.h>
#include <stdio.h>

/* ========== 私有变量 ========== */
static UART_HandleTypeDef *s_huart1 = NULL;  /* 串口2 (USART1) */
static UART_HandleTypeDef *s_huart2 = NULL;  /* 串口1 (USART2) */

static uint8_t s_txBuffer[TEST_BUFFER_SIZE];
static uint8_t s_rxBuffer[TEST_BUFFER_SIZE];

static volatile bool s_rxComplete = false;
static volatile uint16_t s_rxCount = 0;

/* ========== 初始化 ========== */
void loopbackTestInit(UART_HandleTypeDef *huart1, UART_HandleTypeDef *huart2)
{
    s_huart1 = huart1;
    s_huart2 = huart2;
    
    memset(s_txBuffer, 0, sizeof(s_txBuffer));
    memset(s_rxBuffer, 0, sizeof(s_rxBuffer));
}

/* ========== 生成测试数据模式 ========== */
void loopbackTestGeneratePattern(uint8_t *buffer, uint16_t size, uint8_t pattern)
{
    switch (pattern) {
        case 0:  /* 递增模式 */
            for (uint16_t i = 0; i < size; i++) {
                buffer[i] = (uint8_t)i;
            }
            break;
            
        case 1:  /* 0xAA 模式 */
            memset(buffer, 0xAA, size);
            break;
            
        case 2:  /* 0x55 模式 */
            memset(buffer, 0x55, size);
            break;
            
        case 3:  /* 随机模式（伪随机） */
            for (uint16_t i = 0; i < size; i++) {
                buffer[i] = (uint8_t)((i * 7 + 13) & 0xFF);
            }
            break;
            
        default:
            memset(buffer, 0x00, size);
            break;
    }
}

/* ========== 验证数据 ========== */
bool loopbackTestVerifyData(const uint8_t *expected, const uint8_t *received, uint16_t size)
{
    for (uint16_t i = 0; i < size; i++) {
        if (expected[i] != received[i]) {
            return false;
        }
    }
    return true;
}

/* ========== 硬件回环测试 ========== */
bool loopbackTestHardware(UART_HandleTypeDef *huart, TestResult *result)
{
    const uint16_t testSize = 32;
    uint32_t startTick;
    bool passed = true;
    
    /* 生成测试数据 */
    loopbackTestGeneratePattern(s_txBuffer, testSize, 0);
    memset(s_rxBuffer, 0, sizeof(s_rxBuffer));
    
    /* 启动接收 */
    HAL_UART_Receive_IT(huart, s_rxBuffer, testSize);
    
    /* 发送数据 */
    HAL_StatusTypeDef txStatus = HAL_UART_Transmit(huart, s_txBuffer, testSize, TEST_TIMEOUT_MS);
    if (txStatus != HAL_OK) {
        snprintf(result->lastError, sizeof(result->lastError), 
                 "TX failed: %d", txStatus);
        result->failedTests++;
        return false;
    }
    result->txCount += testSize;
    
    /* 等待接收完成 */
    startTick = HAL_GetTick();
    while (!s_rxComplete) {
        if ((HAL_GetTick() - startTick) > TEST_TIMEOUT_MS) {
            snprintf(result->lastError, sizeof(result->lastError), 
                     "RX timeout");
            result->timeoutErrors++;
            result->failedTests++;
            return false;
        }
    }
    s_rxComplete = false;
    result->rxCount += testSize;
    
    /* 验证数据 */
    if (!loopbackTestVerifyData(s_txBuffer, s_rxBuffer, testSize)) {
        snprintf(result->lastError, sizeof(result->lastError), 
                 "Data mismatch");
        result->failedTests++;
        passed = false;
    } else {
        result->passedTests++;
    }
    
    result->totalTests++;
    return passed;
}

/* ========== RS485方向控制测试 ========== */
bool loopbackTestRS485Direction(UART_HandleTypeDef *huart, TestResult *result)
{
    GPIO_TypeDef *dePort;
    uint16_t dePin;
    
    /* 确定RS485控制引脚 */
    if (huart->Instance == USART1) {
        dePort = MB_USART1_RS485_DE_GPIO_Port;
        dePin = MB_USART1_RS485_DE_Pin;
    } else if (huart->Instance == USART2) {
        dePort = MB_USART2_RS485_DE_GPIO_Port;
        dePin = MB_USART2_RS485_DE_Pin;
    } else {
        snprintf(result->lastError, sizeof(result->lastError), 
                 "Unknown UART");
        result->failedTests++;
        return false;
    }
    
    /* 测试方向切换 */
    bool passed = true;
    
    /* 1. 测试发送模式 */
    HAL_GPIO_WritePin(dePort, dePin, GPIO_PIN_SET);
    HAL_Delay(1);
    GPIO_PinState state = HAL_GPIO_ReadPin(dePort, dePin);
    if (state != GPIO_PIN_SET) {
        snprintf(result->lastError, sizeof(result->lastError), 
                 "TX mode failed");
        passed = false;
    }
    
    /* 2. 测试接收模式 */
    HAL_GPIO_WritePin(dePort, dePin, GPIO_PIN_RESET);
    HAL_Delay(1);
    state = HAL_GPIO_ReadPin(dePort, dePin);
    if (state != GPIO_PIN_RESET) {
        snprintf(result->lastError, sizeof(result->lastError), 
                 "RX mode failed");
        passed = false;
    }
    
    /* 3. 快速切换测试 (100次) */
    for (int i = 0; i < 100; i++) {
        HAL_GPIO_WritePin(dePort, dePin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(dePort, dePin, GPIO_PIN_RESET);
    }
    
    /* 恢复到接收模式 */
    HAL_GPIO_WritePin(dePort, dePin, GPIO_PIN_RESET);
    
    result->totalTests++;
    if (passed) {
        result->passedTests++;
    } else {
        result->failedTests++;
    }
    
    return passed;
}

/* ========== 双串口交叉测试 ========== */
bool loopbackTestDualCrossover(TestResult *result)
{
    const uint16_t testSize = 16;
    bool passed = true;
    
    if (s_huart1 == NULL || s_huart2 == NULL) {
        snprintf(result->lastError, sizeof(result->lastError), 
                 "UART not initialized");
        result->failedTests++;
        return false;
    }
    
    /* 测试1: 串口1 -> 串口2 */
    loopbackTestGeneratePattern(s_txBuffer, testSize, 0);
    memset(s_rxBuffer, 0, sizeof(s_rxBuffer));
    
    HAL_UART_Receive_IT(s_huart2, s_rxBuffer, testSize);
    HAL_StatusTypeDef status = HAL_UART_Transmit(s_huart1, s_txBuffer, testSize, TEST_TIMEOUT_MS);
    
    if (status != HAL_OK) {
        snprintf(result->lastError, sizeof(result->lastError), 
                 "UART1->UART2 TX failed");
        passed = false;
    } else {
        HAL_Delay(100);  /* 等待接收完成 */
        if (!loopbackTestVerifyData(s_txBuffer, s_rxBuffer, testSize)) {
            snprintf(result->lastError, sizeof(result->lastError), 
                     "UART1->UART2 data mismatch");
            passed = false;
        }
    }
    
    /* 测试2: 串口2 -> 串口1 */
    if (passed) {
        loopbackTestGeneratePattern(s_txBuffer, testSize, 1);
        memset(s_rxBuffer, 0, sizeof(s_rxBuffer));
        
        HAL_UART_Receive_IT(s_huart1, s_rxBuffer, testSize);
        status = HAL_UART_Transmit(s_huart2, s_txBuffer, testSize, TEST_TIMEOUT_MS);
        
        if (status != HAL_OK) {
            snprintf(result->lastError, sizeof(result->lastError), 
                     "UART2->UART1 TX failed");
            passed = false;
        } else {
            HAL_Delay(100);
            if (!loopbackTestVerifyData(s_txBuffer, s_rxBuffer, testSize)) {
                snprintf(result->lastError, sizeof(result->lastError), 
                         "UART2->UART1 data mismatch");
                passed = false;
            }
        }
    }
    
    result->totalTests++;
    if (passed) {
        result->passedTests++;
    } else {
        result->failedTests++;
    }
    
    return passed;
}

/* ========== Modbus功能回环测试 ========== */
bool loopbackTestModbus(TestResult *result)
{
    bool passed = true;
    
    /* 构造Modbus读保持寄存器请求 (功能码0x03) */
    uint8_t modbusReq[] = {
        0x01,        /* 从站地址 */
        0x03,        /* 功能码：读保持寄存器 */
        0x00, 0x00,  /* 起始地址 */
        0x00, 0x03,  /* 数量：3个寄存器 */
        0x05, 0xCB   /* CRC16 (需要正确计算) */
    };
    
    /* 计算正确的CRC */
    uint16_t crc = ModbusRTU_CRC16(modbusReq, 6);
    modbusReq[6] = (uint8_t)(crc & 0xFF);       /* CRC LSB */
    modbusReq[7] = (uint8_t)((crc >> 8) & 0xFF); /* CRC MSB */
    
    /* 发送Modbus请求到串口1 */
    HAL_UART_Receive_IT(s_huart1, s_rxBuffer, 64);
    HAL_StatusTypeDef status = HAL_UART_Transmit(s_huart1, modbusReq, 8, TEST_TIMEOUT_MS);
    
    if (status != HAL_OK) {
        snprintf(result->lastError, sizeof(result->lastError), 
                 "Modbus TX failed");
        result->failedTests++;
        return false;
    }
    
    /* 等待响应 */
    HAL_Delay(200);
    
    /* 验证响应帧结构 */
    if (s_rxBuffer[0] == 0x01 && s_rxBuffer[1] == 0x03) {
        /* 收到正确的响应头 */
        uint8_t byteCount = s_rxBuffer[2];
        if (byteCount == 6) {  /* 3个寄存器 = 6字节 */
            result->passedTests++;
        } else {
            snprintf(result->lastError, sizeof(result->lastError), 
                     "Invalid byte count: %d", byteCount);
            result->failedTests++;
            passed = false;
        }
    } else {
        snprintf(result->lastError, sizeof(result->lastError), 
                 "Invalid Modbus response");
        result->failedTests++;
        passed = false;
    }
    
    result->totalTests++;
    return passed;
}

/* ========== 压力测试 ========== */
bool loopbackTestStress(uint32_t count, TestResult *result)
{
    uint32_t passCount = 0;
    const uint16_t testSize = 64;
    
    for (uint32_t i = 0; i < count; i++) {
        /* 交替使用不同模式 */
        uint8_t pattern = i % 4;
        loopbackTestGeneratePattern(s_txBuffer, testSize, pattern);
        memset(s_rxBuffer, 0, sizeof(s_rxBuffer));
        
        /* 发送接收 */
        HAL_UART_Receive_IT(s_huart1, s_rxBuffer, testSize);
        HAL_StatusTypeDef status = HAL_UART_Transmit(s_huart1, s_txBuffer, testSize, 500);
        
        if (status == HAL_OK) {
            HAL_Delay(50);
            if (loopbackTestVerifyData(s_txBuffer, s_rxBuffer, testSize)) {
                passCount++;
            }
        }
        
        /* 每100次显示进度 */
        if ((i + 1) % 100 == 0) {
            // 可以在这里输出进度信息
        }
    }
    
    result->totalTests += count;
    result->passedTests += passCount;
    result->failedTests += (count - passCount);
    
    float passRate = ((float)passCount / count) * 100.0f;
    if (passRate >= 99.0f) {
        return true;
    } else {
        snprintf(result->lastError, sizeof(result->lastError), 
                 "Pass rate too low: %.1f%%", passRate);
        return false;
    }
}

/* ========== 执行单次测试 ========== */
bool loopbackTestRun(TestMode mode, TestResult *result)
{
    bool passed = false;
    
    switch (mode) {
        case TEST_MODE_HARDWARE_LOOPBACK:
            passed = loopbackTestHardware(s_huart1, result);
            break;
            
        case TEST_MODE_RS485_DIRECTION:
            passed = loopbackTestRS485Direction(s_huart1, result) &&
                     loopbackTestRS485Direction(s_huart2, result);
            break;
            
        case TEST_MODE_DUAL_UART_CROSSOVER:
            passed = loopbackTestDualCrossover(result);
            break;
            
        case TEST_MODE_MODBUS_LOOPBACK:
            passed = loopbackTestModbus(result);
            break;
            
        case TEST_MODE_STRESS_TEST:
            passed = loopbackTestStress(1000, result);
            break;
            
        default:
            snprintf(result->lastError, sizeof(result->lastError), 
                     "Unknown test mode");
            break;
    }
    
    return passed;
}

/* ========== 执行完整测试套件 ========== */
uint32_t loopbackTestRunAll(void)
{
    TestResult result = {0};
    uint32_t passedCount = 0;
    
    /* 测试1: RS485方向控制 */
    if (loopbackTestRun(TEST_MODE_RS485_DIRECTION, &result)) {
        passedCount++;
    }
    loopbackTestPrintResult(&result);
    
    /* 测试2: 硬件回环（需要短接） */
    memset(&result, 0, sizeof(result));
    if (loopbackTestRun(TEST_MODE_HARDWARE_LOOPBACK, &result)) {
        passedCount++;
    }
    loopbackTestPrintResult(&result);
    
    /* 测试3: 双串口交叉 */
    memset(&result, 0, sizeof(result));
    if (loopbackTestRun(TEST_MODE_DUAL_UART_CROSSOVER, &result)) {
        passedCount++;
    }
    loopbackTestPrintResult(&result);
    
    return passedCount;
}

/* ========== 打印测试结果 ========== */
void loopbackTestPrintResult(const TestResult *result)
{
    /* 这里可以通过串口或其他方式输出结果 */
    /* 示例：通过调试器观察变量或通过printf输出 */
    
    // printf("Test Results:\n");
    // printf("  Total:   %lu\n", result->totalTests);
    // printf("  Passed:  %lu\n", result->passedTests);
    // printf("  Failed:  %lu\n", result->failedTests);
    // printf("  Timeout: %lu\n", result->timeoutErrors);
    // printf("  TX/RX:   %lu/%lu\n", result->txCount, result->rxCount);
    // if (result->lastError[0] != '\0') {
    //     printf("  Error:   %s\n", result->lastError);
    // }
    
    /* 或者通过LED指示 */
    if (result->passedTests == result->totalTests && result->totalTests > 0) {
        /* 全部通过：LED闪烁 */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
    } else {
        /* 有失败：LED常亮 */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
    }
}

/* ========== UART接收完成回调（需要在stm32f1xx_it.c中调用） ========== */
void loopbackTestRxCallback(void)
{
    s_rxComplete = true;
}

