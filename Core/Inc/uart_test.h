/**
 * @file uart_test.h
 * @brief 串口综合测试模块头文件
 * @details
 * 提供串口基础功能测试和Modbus RTU通信测试功能
 * 包括环回测试、性能测试、协议测试等多种测试模式
 * 
 * @author Lighting Ultra Team
 * @date 2025-01-20
 * @version 2.0.0
 */

#ifndef UART_TEST_H
#define UART_TEST_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

//=============================================================================
// 测试模式定义 (Test Mode Definitions)
//=============================================================================

/**
 * @brief 测试模式枚举
 */
typedef enum {
    TEST_MODE_LOOPBACK = 0,     /**< 简单环回测试 */
    TEST_MODE_ECHO_DELAY,       /**< 延时回显测试 */
    TEST_MODE_PATTERN,          /**< 固定模式测试 */
    TEST_MODE_STRESS,           /**< 压力测试 */
    TEST_MODE_MODBUS_SLAVE,     /**< Modbus从站测试 */
    TEST_MODE_MODBUS_MONITOR,   /**< Modbus监控模式 */
    TEST_MODE_COUNT             /**< 测试模式总数 */
} UartTestMode_e;

/**
 * @brief 测试结果枚举
 */
typedef enum {
    TEST_RESULT_PASS = 0,        /**< 测试通过 */
    TEST_RESULT_FAIL,            /**< 测试失败 */
    TEST_RESULT_TIMEOUT,         /**< 测试超时 */
    TEST_RESULT_ERROR            /**< 测试错误 */
} TestResult_e;

/**
 * @brief 测试配置结构体
 */
typedef struct {
    UART_HandleTypeDef* huart;  /**< 要测试的UART句柄 */
    uint32_t baudRate;           /**< 波特率 */
    uint32_t timeout;            /**< 超时时间(ms) */
    UartTestMode_e mode;        /**< 测试模式 */
    bool useRS485;              /**< 是否使用RS485 */
    GPIO_TypeDef* rs485Port;    /**< RS485控制端口 */
    uint16_t rs485Pin;          /**< RS485控制引脚 */
} UartTestConfig_t;

/**
 * @brief 测试统计信息结构体
 */
typedef struct {
    uint32_t totalPackets;      /**< 总数据包数 */
    uint32_t successPackets;    /**< 成功包数 */
    uint32_t errorPackets;       /**< 错误包数 */
    uint32_t timeoutCount;      /**< 超时次数 */
    uint32_t crcErrors;         /**< CRC错误数 */
    uint32_t totalBytes;        /**< 总字节数 */
    uint32_t avgResponseTime;   /**< 平均响应时间(us) */
    uint32_t maxResponseTime;   /**< 最大响应时间(us) */
    uint32_t minResponseTime;   /**< 最小响应时间(us) */
    float successRate;          /**< 成功率 */
} UartTestStats_t;

//=============================================================================
// 基础功能测试 API (Basic Function Test API)
//=============================================================================

/**
 * @brief 初始化UART测试模块
 * @param config 测试配置
 * @return HAL_StatusTypeDef 初始化状态
 */
HAL_StatusTypeDef uartTestInit(UartTestConfig_t* config);

/**
 * @brief 执行环回测试
 * @param data 测试数据
 * @param length 数据长度
 * @param timeout 超时时间(ms)
 * @return TestResult_e 测试结果
 */
TestResult_e uartTestLoopback(uint8_t* data, uint16_t length, uint32_t timeout);

/**
 * @brief 执行模式测试（发送固定模式数据）
 * @param pattern 测试模式(0x00, 0xFF, 0x55, 0xAA等)
 * @param length 数据长度
 * @param iterations 测试次数
 * @return TestResult_e 测试结果
 */
TestResult_e uartTestPattern(uint8_t pattern, uint16_t length, uint16_t iterations);

/**
 * @brief 执行压力测试
 * @param minSize 最小包大小
 * @param maxSize 最大包大小
 * @param duration 测试持续时间(秒)
 * @return TestResult_e 测试结果
 */
TestResult_e uartTestStress(uint16_t minSize, uint16_t maxSize, uint32_t duration);

/**
 * @brief 测试波特率范围
 * @param minBaud 最小波特率
 * @param maxBaud 最大波特率
 * @return TestResult_e 测试结果
 */
TestResult_e uartTestBaudRateRange(uint32_t minBaud, uint32_t maxBaud);

//=============================================================================
// Modbus测试 API (Modbus Test API)
//=============================================================================

/**
 * @brief Modbus从站测试初始化
 * @param slaveAddr 从站地址
 * @return HAL_StatusTypeDef 初始化状态
 */
HAL_StatusTypeDef modbusTestSlaveInit(uint8_t slaveAddr);

/**
 * @brief Modbus读保持寄存器测试
 * @param slaveAddr 从站地址
 * @param regAddr 寄存器起始地址
 * @param regCount 寄存器数量
 * @param expectedValues 期望值数组(可选)
 * @return TestResult_e 测试结果
 */
TestResult_e modbusTestReadHoldingRegs(uint8_t slaveAddr, uint16_t regAddr, 
                                       uint16_t regCount, uint16_t* expectedValues);

/**
 * @brief Modbus写单个寄存器测试
 * @param slaveAddr 从站地址
 * @param regAddr 寄存器地址
 * @param value 写入值
 * @return TestResult_e 测试结果
 */
TestResult_e modbusTestWriteSingleReg(uint8_t slaveAddr, uint16_t regAddr, uint16_t value);

/**
 * @brief Modbus写多个寄存器测试
 * @param slaveAddr 从站地址
 * @param regAddr 寄存器起始地址
 * @param regCount 寄存器数量
 * @param values 值数组
 * @return TestResult_e 测试结果
 */
TestResult_e modbusTestWriteMultipleRegs(uint8_t slaveAddr, uint16_t regAddr, 
                                         uint16_t regCount, uint16_t* values);

/**
 * @brief Modbus继电器控制测试
 * @param slaveAddr 从站地址
 * @param relayMask 继电器掩码
 * @return TestResult_e 测试结果
 */
TestResult_e modbusTestRelayControl(uint8_t slaveAddr, uint8_t relayMask);

/**
 * @brief Modbus响应时间测试
 * @param slaveAddr 从站地址
 * @param iterations 测试次数
 * @return uint32_t 平均响应时间(微秒)
 */
uint32_t modbusTestResponseTime(uint8_t slaveAddr, uint16_t iterations);

//=============================================================================
// 测试辅助函数 (Test Utility Functions)
//=============================================================================

/**
 * @brief 获取测试统计信息
 * @return UartTestStats_t* 统计信息指针
 */
UartTestStats_t* uartTestGetStats(void);

/**
 * @brief 重置测试统计
 */
void uartTestResetStats(void);

/**
 * @brief 打印测试报告
 * @param huart 用于输出的UART句柄
 */
void uartTestPrintReport(UART_HandleTypeDef* huart);

/**
 * @brief 运行完整测试套件
 * @param testMask 测试项掩码(bit0=环回, bit1=模式, bit2=压力, bit3=Modbus)
 * @return TestResult_e 综合测试结果
 */
TestResult_e uartTestRunSuite(uint8_t testMask);

/**
 * @brief 设置测试LED指示
 * @param testStatus 测试状态(0=待机, 1=测试中, 2=通过, 3=失败)
 */
void uartTestSetLedStatus(uint8_t testStatus);

/**
 * @brief 获取测试模式名称
 * @param mode 测试模式
 * @return const char* 模式名称字符串
 */
const char* uartTestGetModeName(UartTestMode_e mode);

#endif // UART_TEST_H

