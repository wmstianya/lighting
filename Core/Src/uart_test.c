/**
 * @file uart_test.c
 * @brief 串口综合测试模块实现
 * @details
 * 提供串口基础功能测试和Modbus RTU通信测试功能实现
 * 
 * @author Lighting Ultra Team
 * @date 2025-01-20
 * @version 2.0.0
 */

#include "uart_test.h"
#include "relay.h"
#include "../../MDK-ARM/modbus_rtu_slave.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//=============================================================================
// 私有定义 (Private Definitions)
//=============================================================================

#define TEST_BUFFER_SIZE        512
#define DEFAULT_TIMEOUT         1000   // 默认超时1秒
#define RS485_SWITCH_DELAY      10     // RS485切换延时

//=============================================================================
// 私有变量 (Private Variables)
//=============================================================================

// 测试缓冲区
static uint8_t testRxBuffer[TEST_BUFFER_SIZE];
static uint8_t testTxBuffer[TEST_BUFFER_SIZE];
static uint8_t testVerifyBuffer[TEST_BUFFER_SIZE];

// 测试状态
static volatile bool testDataReady = false;
static volatile uint16_t testRxLength = 0;
static volatile bool testInProgress = false;

// 测试配置
static UartTestConfig_t currentConfig;

// 测试统计
static UartTestStats_t testStats = {0};

// 外部变量引用
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;

// Modbus测试实例
static ModbusRTU_Slave* testModbusSlave = NULL;

//=============================================================================
// 私有函数声明 (Private Function Prototypes)
//=============================================================================

static void rs485SetMode(bool transmit);
static void generateTestData(uint8_t* buffer, uint16_t length, uint8_t pattern);
static bool verifyTestData(uint8_t* expected, uint8_t* actual, uint16_t length);
static uint32_t getMicroseconds(void);
static void updateStatistics(bool success, uint32_t responseTime, uint16_t dataLength);

//=============================================================================
// 基础功能测试实现 (Basic Function Test Implementation)
//=============================================================================

/**
 * @brief 初始化UART测试模块
 */
HAL_StatusTypeDef uartTestInit(UartTestConfig_t* config)
{
    if (config == NULL || config->huart == NULL) {
        return HAL_ERROR;
    }
    
    // 保存配置
    memcpy(&currentConfig, config, sizeof(UartTestConfig_t));
    
    // 清空缓冲区
    memset(testRxBuffer, 0, TEST_BUFFER_SIZE);
    memset(testTxBuffer, 0, TEST_BUFFER_SIZE);
    memset(testVerifyBuffer, 0, TEST_BUFFER_SIZE);
    
    // 重置统计
    uartTestResetStats();
    
    // 设置波特率
    currentConfig.huart->Init.BaudRate = config->baudRate;
    if (HAL_UART_Init(currentConfig.huart) != HAL_OK) {
        return HAL_ERROR;
    }
    
    // RS485模式配置
    if (config->useRS485) {
        rs485SetMode(false); // 默认接收模式
    }
    
    // 启用IDLE中断
    __HAL_UART_ENABLE_IT(currentConfig.huart, UART_IT_IDLE);
    
    // 启动DMA接收
    HAL_UART_Receive_DMA(currentConfig.huart, testRxBuffer, TEST_BUFFER_SIZE);
    
    // 设置LED状态为待机
    uartTestSetLedStatus(0);
    
    return HAL_OK;
}

/**
 * @brief 执行环回测试
 */
TestResult_e uartTestLoopback(uint8_t* data, uint16_t length, uint32_t timeout)
{
    if (data == NULL || length == 0 || length > TEST_BUFFER_SIZE) {
        return TEST_RESULT_ERROR;
    }
    
    testInProgress = true;
    testDataReady = false;
    testRxLength = 0;
    
    uint32_t startTime = HAL_GetTick();
    uint32_t startMicros = getMicroseconds();
    
    // 设置LED为测试中
    uartTestSetLedStatus(1);
    
    // 复制测试数据
    memcpy(testTxBuffer, data, length);
    memcpy(testVerifyBuffer, data, length); // 保存用于验证
    
    // RS485发送模式
    if (currentConfig.useRS485) {
        rs485SetMode(true);
        HAL_Delay(RS485_SWITCH_DELAY);
    }
    
    // 发送数据
    if (HAL_UART_Transmit_DMA(currentConfig.huart, testTxBuffer, length) != HAL_OK) {
        testInProgress = false;
        uartTestSetLedStatus(3); // 失败
        return TEST_RESULT_ERROR;
    }
    
    // 等待发送完成
    while (HAL_UART_GetState(currentConfig.huart) == HAL_UART_STATE_BUSY_TX) {
        if ((HAL_GetTick() - startTime) > timeout) {
            testInProgress = false;
            uartTestSetLedStatus(3);
            testStats.timeoutCount++;
            return TEST_RESULT_TIMEOUT;
        }
    }
    
    // RS485接收模式
    if (currentConfig.useRS485) {
        HAL_Delay(RS485_SWITCH_DELAY);
        rs485SetMode(false);
    }
    
    // 重启DMA接收
    HAL_UART_Receive_DMA(currentConfig.huart, testRxBuffer, TEST_BUFFER_SIZE);
    
    // 等待接收数据
    while (!testDataReady) {
        if ((HAL_GetTick() - startTime) > timeout) {
            testInProgress = false;
            uartTestSetLedStatus(3);
            testStats.timeoutCount++;
            return TEST_RESULT_TIMEOUT;
        }
    }
    
    uint32_t responseTime = getMicroseconds() - startMicros;
    
    // 验证数据
    bool success = (testRxLength == length) && 
                   verifyTestData(testVerifyBuffer, testRxBuffer, length);
    
    // 更新统计
    updateStatistics(success, responseTime, length);
    
    testInProgress = false;
    
    if (success) {
        uartTestSetLedStatus(2); // 通过
        return TEST_RESULT_PASS;
    } else {
        uartTestSetLedStatus(3); // 失败
        return TEST_RESULT_FAIL;
    }
}

/**
 * @brief 执行模式测试
 */
TestResult_e uartTestPattern(uint8_t pattern, uint16_t length, uint16_t iterations)
{
    if (length > TEST_BUFFER_SIZE || iterations == 0) {
        return TEST_RESULT_ERROR;
    }
    
    // 生成测试数据
    generateTestData(testTxBuffer, length, pattern);
    
    uint16_t passCount = 0;
    
    for (uint16_t i = 0; i < iterations; i++) {
        TestResult_e result = uartTestLoopback(testTxBuffer, length, DEFAULT_TIMEOUT);
        
        if (result == TEST_RESULT_PASS) {
            passCount++;
        } else if (result == TEST_RESULT_ERROR) {
            return TEST_RESULT_ERROR;
        }
        
        HAL_Delay(10); // 测试间隔
    }
    
    // 计算成功率
    float successRate = (float)passCount / iterations * 100.0f;
    
    if (successRate >= 95.0f) {
        return TEST_RESULT_PASS;
    } else if (successRate >= 80.0f) {
        return TEST_RESULT_FAIL;
    } else {
        return TEST_RESULT_ERROR;
    }
}

/**
 * @brief 执行压力测试
 */
TestResult_e uartTestStress(uint16_t minSize, uint16_t maxSize, uint32_t duration)
{
    if (minSize > maxSize || maxSize > TEST_BUFFER_SIZE) {
        return TEST_RESULT_ERROR;
    }
    
    uint32_t startTime = HAL_GetTick();
    uint32_t endTime = startTime + (duration * 1000);
    uint32_t testCount = 0;
    uint32_t passCount = 0;
    
    while (HAL_GetTick() < endTime) {
        // 生成随机大小
        uint16_t size = minSize + (rand() % (maxSize - minSize + 1));
        
        // 生成随机数据
        for (uint16_t i = 0; i < size; i++) {
            testTxBuffer[i] = rand() & 0xFF;
        }
        
        // 执行测试
        TestResult_e result = uartTestLoopback(testTxBuffer, size, DEFAULT_TIMEOUT);
        testCount++;
        
        if (result == TEST_RESULT_PASS) {
            passCount++;
        }
        
        // 动态调整测试间隔
        if (size < 50) {
            HAL_Delay(5);
        } else if (size < 200) {
            HAL_Delay(10);
        } else {
            HAL_Delay(20);
        }
    }
    
    // 计算成功率
    float successRate = (testCount > 0) ? 
                       ((float)passCount / testCount * 100.0f) : 0.0f;
    
    if (successRate >= 90.0f && testCount >= 100) {
        return TEST_RESULT_PASS;
    } else if (successRate >= 70.0f) {
        return TEST_RESULT_FAIL;
    } else {
        return TEST_RESULT_ERROR;
    }
}

//=============================================================================
// Modbus测试实现 (Modbus Test Implementation)
//=============================================================================

/**
 * @brief Modbus从站测试初始化
 */
HAL_StatusTypeDef modbusTestSlaveInit(uint8_t slaveAddr)
{
    // 这里使用项目中的Modbus从站实例
    extern ModbusRTU_Slave g_mb;
    testModbusSlave = &g_mb;
    
    // 初始化测试数据
    testModbusSlave->holdingRegs[0] = 0x1234;
    testModbusSlave->holdingRegs[1] = 0x5678;
    testModbusSlave->holdingRegs[2] = 0xABCD;
    testModbusSlave->inputRegs[0] = 0x1111;
    testModbusSlave->inputRegs[1] = 0x2222;
    
    return HAL_OK;
}

/**
 * @brief Modbus继电器控制测试
 */
TestResult_e modbusTestRelayControl(uint8_t slaveAddr, uint8_t relayMask)
{
    if (testModbusSlave == NULL) {
        return TEST_RESULT_ERROR;
    }
    
    // 构建Modbus写单个寄存器命令 (功能码06)
    uint8_t request[8];
    request[0] = slaveAddr;
    request[1] = 0x06;  // 写单个寄存器
    request[2] = 0x00;  // 寄存器地址高字节
    request[3] = 0x00;  // 寄存器地址低字节（使用寄存器0控制继电器）
    request[4] = 0x00;  // 数据高字节
    request[5] = relayMask;  // 数据低字节（继电器掩码）
    
    // 计算CRC
    uint16_t crc = ModbusRTU_CRC16(request, 6);
    request[6] = crc & 0xFF;
    request[7] = (crc >> 8) & 0xFF;
    
    // 发送命令并等待响应
    TestResult_e result = uartTestLoopback(request, 8, 500);
    
    if (result == TEST_RESULT_PASS) {
        // 验证继电器状态
        uint8_t actualState = relayGetAllStates();
        if ((actualState & 0x1F) == (relayMask & 0x1F)) {
            return TEST_RESULT_PASS;
        }
    }
    
    return result;
}

/**
 * @brief Modbus响应时间测试
 */
uint32_t modbusTestResponseTime(uint8_t slaveAddr, uint16_t iterations)
{
    if (iterations == 0) return 0;
    
    uint32_t totalTime = 0;
    uint16_t successCount = 0;
    
    // 构建读保持寄存器命令
    uint8_t request[8];
    request[0] = slaveAddr;
    request[1] = 0x03;  // 读保持寄存器
    request[2] = 0x00;  // 起始地址高字节
    request[3] = 0x00;  // 起始地址低字节
    request[4] = 0x00;  // 寄存器数量高字节
    request[5] = 0x01;  // 寄存器数量低字节
    
    uint16_t crc = ModbusRTU_CRC16(request, 6);
    request[6] = crc & 0xFF;
    request[7] = (crc >> 8) & 0xFF;
    
    for (uint16_t i = 0; i < iterations; i++) {
        uint32_t startTime = getMicroseconds();
        
        if (uartTestLoopback(request, 8, 100) == TEST_RESULT_PASS) {
            uint32_t responseTime = getMicroseconds() - startTime;
            totalTime += responseTime;
            successCount++;
        }
        
        HAL_Delay(10); // 测试间隔
    }
    
    return (successCount > 0) ? (totalTime / successCount) : 0;
}

//=============================================================================
// 辅助函数实现 (Utility Function Implementation)
//=============================================================================

/**
 * @brief 获取测试统计信息
 */
UartTestStats_t* uartTestGetStats(void)
{
    // 计算成功率
    if (testStats.totalPackets > 0) {
        testStats.successRate = (float)testStats.successPackets / 
                                testStats.totalPackets * 100.0f;
    }
    
    return &testStats;
}

/**
 * @brief 重置测试统计
 */
void uartTestResetStats(void)
{
    memset(&testStats, 0, sizeof(UartTestStats_t));
    testStats.minResponseTime = 0xFFFFFFFF;
}

/**
 * @brief 打印测试报告
 */
void uartTestPrintReport(UART_HandleTypeDef* huart)
{
    char buffer[256];
    
    sprintf(buffer, "\r\n========== UART测试报告 ==========\r\n");
    HAL_UART_Transmit(huart, (uint8_t*)buffer, strlen(buffer), 100);
    
    sprintf(buffer, "总数据包: %lu\r\n", testStats.totalPackets);
    HAL_UART_Transmit(huart, (uint8_t*)buffer, strlen(buffer), 100);
    
    sprintf(buffer, "成功: %lu | 错误: %lu | 超时: %lu\r\n", 
            testStats.successPackets, testStats.errorPackets, testStats.timeoutCount);
    HAL_UART_Transmit(huart, (uint8_t*)buffer, strlen(buffer), 100);
    
    sprintf(buffer, "成功率: %.2f%%\r\n", testStats.successRate);
    HAL_UART_Transmit(huart, (uint8_t*)buffer, strlen(buffer), 100);
    
    sprintf(buffer, "总字节数: %lu\r\n", testStats.totalBytes);
    HAL_UART_Transmit(huart, (uint8_t*)buffer, strlen(buffer), 100);
    
    sprintf(buffer, "响应时间(us) - 平均: %lu | 最小: %lu | 最大: %lu\r\n",
            testStats.avgResponseTime, testStats.minResponseTime, testStats.maxResponseTime);
    HAL_UART_Transmit(huart, (uint8_t*)buffer, strlen(buffer), 100);
    
    if (testStats.crcErrors > 0) {
        sprintf(buffer, "CRC错误: %lu\r\n", testStats.crcErrors);
        HAL_UART_Transmit(huart, (uint8_t*)buffer, strlen(buffer), 100);
    }
    
    sprintf(buffer, "===================================\r\n");
    HAL_UART_Transmit(huart, (uint8_t*)buffer, strlen(buffer), 100);
}

/**
 * @brief 运行完整测试套件
 */
TestResult_e uartTestRunSuite(uint8_t testMask)
{
    TestResult_e overallResult = TEST_RESULT_PASS;
    
    // 环回测试
    if (testMask & 0x01) {
        uint8_t testData[] = "Hello UART Test!";
        TestResult_e result = uartTestLoopback(testData, sizeof(testData) - 1, 1000);
        if (result != TEST_RESULT_PASS) {
            overallResult = TEST_RESULT_FAIL;
        }
    }
    
    // 模式测试
    if (testMask & 0x02) {
        TestResult_e result = uartTestPattern(0x55, 64, 10);
        if (result != TEST_RESULT_PASS) {
            overallResult = TEST_RESULT_FAIL;
        }
        
        result = uartTestPattern(0xAA, 64, 10);
        if (result != TEST_RESULT_PASS) {
            overallResult = TEST_RESULT_FAIL;
        }
    }
    
    // 压力测试
    if (testMask & 0x04) {
        TestResult_e result = uartTestStress(10, 256, 10); // 10秒压力测试
        if (result != TEST_RESULT_PASS) {
            overallResult = TEST_RESULT_FAIL;
        }
    }
    
    // Modbus测试
    if (testMask & 0x08) {
        modbusTestSlaveInit(0x01);
        uint32_t avgTime = modbusTestResponseTime(0x01, 20);
        if (avgTime == 0 || avgTime > 50000) { // 响应时间应小于50ms
            overallResult = TEST_RESULT_FAIL;
        }
    }
    
    return overallResult;
}

/**
 * @brief 设置测试LED指示
 */
void uartTestSetLedStatus(uint8_t testStatus)
{
    // 使用继电器作为状态指示
    switch (testStatus) {
        case 0: // 待机
            relayTurnOffAll();
            break;
        case 1: // 测试中
            relaySetState(RELAY_CHANNEL_SECOND, RELAY_STATE_ON); // 继电器2亮
            break;
        case 2: // 通过
            relaySetState(RELAY_CHANNEL_THIRD, RELAY_STATE_ON);  // 继电器3亮
            break;
        case 3: // 失败
            relaySetState(RELAY_CHANNEL_FOURTH, RELAY_STATE_ON); // 继电器4亮
            break;
        default:
            break;
    }
}

//=============================================================================
// 私有函数实现 (Private Function Implementation)
//=============================================================================

/**
 * @brief 设置RS485模式
 */
static void rs485SetMode(bool transmit)
{
    if (currentConfig.useRS485 && currentConfig.rs485Port != NULL) {
        HAL_GPIO_WritePin(currentConfig.rs485Port, currentConfig.rs485Pin, 
                         transmit ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

/**
 * @brief 生成测试数据
 */
static void generateTestData(uint8_t* buffer, uint16_t length, uint8_t pattern)
{
    for (uint16_t i = 0; i < length; i++) {
        if (pattern == 0xFF) {
            buffer[i] = 0xFF;
        } else if (pattern == 0x00) {
            buffer[i] = 0x00;
        } else if (pattern == 0x55) {
            buffer[i] = (i & 1) ? 0x55 : 0xAA;
        } else if (pattern == 0xAA) {
            buffer[i] = (i & 1) ? 0xAA : 0x55;
        } else {
            buffer[i] = (uint8_t)(i & 0xFF); // 递增序列
        }
    }
}

/**
 * @brief 验证测试数据
 */
static bool verifyTestData(uint8_t* expected, uint8_t* actual, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++) {
        if (expected[i] != actual[i]) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 获取微秒时间戳
 */
static uint32_t getMicroseconds(void)
{
    // 使用SysTick计数器实现微秒级计时
    uint32_t ms = HAL_GetTick();
    uint32_t st = SysTick->VAL;
    return (ms * 1000) + ((SysTick->LOAD - st) / (SystemCoreClock / 1000000));
}

/**
 * @brief 更新统计信息
 */
static void updateStatistics(bool success, uint32_t responseTime, uint16_t dataLength)
{
    testStats.totalPackets++;
    testStats.totalBytes += dataLength;
    
    if (success) {
        testStats.successPackets++;
    } else {
        testStats.errorPackets++;
    }
    
    // 更新响应时间统计
    if (responseTime < testStats.minResponseTime) {
        testStats.minResponseTime = responseTime;
    }
    if (responseTime > testStats.maxResponseTime) {
        testStats.maxResponseTime = responseTime;
    }
    
    // 计算平均响应时间（滑动平均）
    if (testStats.avgResponseTime == 0) {
        testStats.avgResponseTime = responseTime;
    } else {
        testStats.avgResponseTime = (testStats.avgResponseTime * 9 + responseTime) / 10;
    }
}

/**
 * @brief 处理UART IDLE中断（在中断中调用）
 */
void uartTestHandleIdleIRQ(UART_HandleTypeDef* huart)
{
    if (huart == currentConfig.huart && testInProgress) {
        // 停止DMA
        HAL_UART_DMAStop(huart);
        
        // 获取接收长度
        if (huart->Instance == USART1) {
            testRxLength = TEST_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
        } else if (huart->Instance == USART2) {
            testRxLength = TEST_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
        }
        
        // 设置数据准备标志
        if (testRxLength > 0) {
            testDataReady = true;
        }
    }
}

