/**
 * @file error_handler.h
 * @brief 统一错误处理框架头文件
 * @details 错误报告、追踪、恢复和Flash日志记录
 * @author Lighting Ultra Team
 * @date 2025-11-11
 * 
 * 功能：
 * - 统一的错误代码定义
 * - 错误计数和状态追踪
 * - Flash循环日志存储（最近100条）
 * - 错误恢复策略
 */

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

/* ==================== Flash日志配置 ==================== */
#define ERROR_LOG_FLASH_BASE_ADDR   0x0800F000  /* C8T6倒数第二页（64KB） */
#define ERROR_LOG_MAX_ENTRIES       62          /* 最大日志条数（2KB/32B=64，留余量62） */
#define ERROR_LOG_ENTRY_SIZE        32          /* 每条日志32字节 */

/* ==================== 错误代码定义 ==================== */
/**
 * @brief 错误代码枚举
 */
typedef enum {
    /* 系统错误 (0x00-0x0F) */
    ERROR_NONE = 0x00,
    ERROR_SYSTEM_INIT_FAILED = 0x01,
    ERROR_WATCHDOG_TIMEOUT = 0x02,
    ERROR_MEMORY_OVERFLOW = 0x03,
    
    /* 通信错误 (0x10-0x1F) */
    ERROR_MODBUS_TIMEOUT = 0x10,
    ERROR_MODBUS_CRC_ERROR = 0x11,
    ERROR_UART_TX_FAILED = 0x12,
    ERROR_UART_RX_OVERFLOW = 0x13,
    
    /* ADC/传感器错误 (0x20-0x2F) */
    ERROR_ADC_INIT_FAILED = 0x20,
    ERROR_ADC_CONVERSION_TIMEOUT = 0x21,
    ERROR_PRESSURE_SENSOR_FAULT = 0x22,
    ERROR_PRESSURE_OUT_OF_RANGE = 0x23,
    
    /* 水位错误 (0x30-0x3F) */
    ERROR_WATER_LEVEL_ABNORMAL = 0x30,
    ERROR_WATER_LOW_BUT_MID_HIGH = 0x31,
    ERROR_WATER_DISCONTINUOUS = 0x32,
    
    /* 硬件错误 (0x40-0x4F) */
    ERROR_RELAY_FAULT = 0x40,
    ERROR_BEEP_INIT_FAILED = 0x41,
    ERROR_GPIO_INIT_FAILED = 0x42,
    
    /* 配置错误 (0x50-0x5F) */
    ERROR_CONFIG_INVALID = 0x50,
    ERROR_CONFIG_CHECKSUM_FAILED = 0x51,
    ERROR_FLASH_WRITE_FAILED = 0x52,
    ERROR_FLASH_ERASE_FAILED = 0x53,
    
    ERROR_CODE_MAX = 0xFF
} ErrorCode_e;

/**
 * @brief 错误级别枚举
 */
typedef enum {
    ERROR_LEVEL_INFO = 0,    /**< 信息 */
    ERROR_LEVEL_WARNING,     /**< 警告 */
    ERROR_LEVEL_ERROR,       /**< 错误 */
    ERROR_LEVEL_CRITICAL     /**< 严重错误 */
} ErrorLevel_e;

/* ==================== 错误日志结构 ==================== */
/**
 * @brief Flash日志条目结构（32字节）
 */
typedef struct {
    uint32_t timestamp;      /**< 时间戳（ms） */
    ErrorCode_e errorCode;   /**< 错误代码 */
    ErrorLevel_e level;      /**< 错误级别 */
    uint16_t lineNumber;     /**< 源码行号 */
    char message[20];        /**< 错误信息 */
} ErrorLogEntry_t;

/**
 * @brief 错误统计结构
 */
typedef struct {
    uint32_t errorCount;     /**< 错误总次数 */
    uint32_t lastErrorTime;  /**< 上次错误时间 */
    ErrorCode_e lastError;   /**< 上次错误代码 */
    bool isActive;           /**< 错误是否激活 */
} ErrorStats_t;

/* ==================== API函数 ==================== */
/**
 * @brief 初始化错误处理模块
 * @return HAL_StatusTypeDef HAL状态码
 */
HAL_StatusTypeDef errorHandlerInit(void);

/**
 * @brief 报告错误
 * @param code 错误代码
 * @param level 错误级别
 * @param message 错误信息（最多19字符）
 * @param line 源码行号
 */
void errorReport(ErrorCode_e code, ErrorLevel_e level, const char* message, uint16_t line);

/**
 * @brief 清除错误
 * @param code 错误代码
 */
void errorClear(ErrorCode_e code);

/**
 * @brief 获取错误统计
 * @param code 错误代码
 * @return ErrorStats_t 错误统计
 */
ErrorStats_t errorGetStats(ErrorCode_e code);

/**
 * @brief 检查错误是否激活
 * @param code 错误代码
 * @return bool 是否激活
 */
bool errorIsActive(ErrorCode_e code);

/**
 * @brief 获取所有激活的错误掩码
 * @return uint32_t 错误掩码（bit对应错误代码）
 */
uint32_t errorGetActiveMask(void);

/**
 * @brief 清除所有错误
 */
void errorClearAll(void);

/* ==================== Flash日志API ==================== */
/**
 * @brief 写入日志到Flash
 * @param code 错误代码
 * @param level 错误级别
 * @param message 日志信息
 * @return HAL_StatusTypeDef HAL状态码
 */
HAL_StatusTypeDef errorLogWrite(ErrorCode_e code, ErrorLevel_e level, const char* message);

/**
 * @brief 读取最近N条日志
 * @param buffer 日志缓冲区
 * @param count 读取条数
 * @return uint16_t 实际读取条数
 */
uint16_t errorLogRead(ErrorLogEntry_t* buffer, uint16_t count);

/**
 * @brief 获取日志总数
 * @return uint16_t 日志条数
 */
uint16_t errorLogGetCount(void);

/**
 * @brief 清除所有日志
 * @return HAL_StatusTypeDef HAL状态码
 */
HAL_StatusTypeDef errorLogClear(void);

/**
 * @brief 获取错误代码描述字符串
 * @param code 错误代码
 * @return const char* 错误描述
 */
const char* errorGetDescription(ErrorCode_e code);

/* ==================== 错误报告宏 ==================== */
#define ERROR_REPORT_INFO(code, msg)     errorReport(code, ERROR_LEVEL_INFO, msg, __LINE__)
#define ERROR_REPORT_WARNING(code, msg)  errorReport(code, ERROR_LEVEL_WARNING, msg, __LINE__)
#define ERROR_REPORT_ERROR(code, msg)    errorReport(code, ERROR_LEVEL_ERROR, msg, __LINE__)
#define ERROR_REPORT_CRITICAL(code, msg) errorReport(code, ERROR_LEVEL_CRITICAL, msg, __LINE__)

#endif /* ERROR_HANDLER_H */

