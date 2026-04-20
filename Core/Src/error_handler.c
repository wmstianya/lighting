/**
 * @file error_handler.c
 * @brief 统一错误处理框架实现
 * @details 错误追踪、Flash日志记录
 * @author Lighting Ultra Team
 * @date 2025-11-11
 */

#include "error_handler.h"
#include "config_manager.h"  /* 使用ENABLE_FLASH_STORAGE宏 */
#include <string.h>
#include <stdio.h>

/* ==================== 私有变量 ==================== */
static ErrorStats_t errorStats[64];  /* 支持64种错误码统计 */
static uint16_t logWriteIndex = 0;   /* 日志写入索引（循环） */
static uint16_t logCount = 0;        /* 当前日志总数 */

/* ==================== 私有函数 ==================== */
/**
 * @brief 计算Flash日志条目地址
 */
static uint32_t getLogEntryAddress(uint16_t index)
{
    return ERROR_LOG_FLASH_BASE_ADDR + (index % ERROR_LOG_MAX_ENTRIES) * ERROR_LOG_ENTRY_SIZE;
}

/**
 * @brief 擦除日志Flash页
 */
static HAL_StatusTypeDef eraseLogPage(void)
{
    HAL_FLASH_Unlock();
    
    FLASH_EraseInitTypeDef eraseInit;
    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInit.PageAddress = ERROR_LOG_FLASH_BASE_ADDR;
    eraseInit.NbPages = 1;
    
    uint32_t pageError = 0;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&eraseInit, &pageError);
    
    HAL_FLASH_Lock();
    return status;
}

/* ==================== API函数实现 ==================== */
HAL_StatusTypeDef errorHandlerInit(void)
{
    /* 清空错误统计 */
    memset(errorStats, 0, sizeof(errorStats));
    
    logCount = 0;
    logWriteIndex = 0;
    
#if ENABLE_FLASH_STORAGE
    /* Flash存储已启用：从Flash读取日志计数 */
    for (uint16_t i = 0; i < ERROR_LOG_MAX_ENTRIES; i++) {
        uint32_t* pFlash = (uint32_t*)getLogEntryAddress(i);
        if (*pFlash != 0xFFFFFFFF) {  /* Flash已写入数据 */
            logCount++;
            logWriteIndex = (i + 1) % ERROR_LOG_MAX_ENTRIES;
        }
    }
#else
    /* Flash存储已禁用：仅使用内存统计 */
    /* logCount和logWriteIndex已初始化为0 */
#endif
    
    return HAL_OK;
}

void errorReport(ErrorCode_e code, ErrorLevel_e level, const char* message, uint16_t line)
{
    uint8_t index = code & 0x3F;  /* 限制在64个统计槽内 */
    
    /* 更新统计 */
    errorStats[index].errorCount++;
    errorStats[index].lastErrorTime = HAL_GetTick();
    errorStats[index].lastError = code;
    errorStats[index].isActive = true;
    
#if ENABLE_FLASH_STORAGE
    /* Flash存储已启用：写入日志到Flash */
    errorLogWrite(code, level, message);
#else
    /* Flash存储已禁用：仅内存统计 */
    (void)level;
    (void)message;
    (void)line;
#endif
    
    /* 严重错误：触发蜂鸣器（需外部实现） */
    if (level == ERROR_LEVEL_CRITICAL) {
        /* 可以在这里触发蜂鸣器或LED报警 */
        /* 需要include beep.h并调用 beepSetTime(1000); */
    }
}

void errorClear(ErrorCode_e code)
{
    uint8_t index = code & 0x3F;
    errorStats[index].isActive = false;
}

ErrorStats_t errorGetStats(ErrorCode_e code)
{
    uint8_t index = code & 0x3F;
    return errorStats[index];
}

bool errorIsActive(ErrorCode_e code)
{
    uint8_t index = code & 0x3F;
    return errorStats[index].isActive;
}

uint32_t errorGetActiveMask(void)
{
    uint32_t mask = 0;
    
    for (uint8_t i = 0; i < 32; i++) {
        if (errorStats[i].isActive) {
            mask |= (1 << i);
        }
    }
    
    return mask;
}

void errorClearAll(void)
{
    for (uint8_t i = 0; i < 64; i++) {
        errorStats[i].isActive = false;
    }
}

/* ==================== Flash日志实现 ==================== */
HAL_StatusTypeDef errorLogWrite(ErrorCode_e code, ErrorLevel_e level, const char* message)
{
    ErrorLogEntry_t entry;
    
    /* 准备日志条目 */
    entry.timestamp = HAL_GetTick();
    entry.errorCode = code;
    entry.level = level;
    entry.lineNumber = 0;
    
    /* 复制消息（限制长度） */
    strncpy(entry.message, message, sizeof(entry.message) - 1);
    entry.message[sizeof(entry.message) - 1] = '\0';
    
    /* 计算Flash地址 */
    uint32_t address = getLogEntryAddress(logWriteIndex);
    
    /* 检查是否需要擦除（写满一圈） */
    if (logWriteIndex == 0 && logCount >= ERROR_LOG_MAX_ENTRIES) {
        if (eraseLogPage() != HAL_OK) {
            return HAL_ERROR;
        }
    }
    
    /* 写入Flash */
    HAL_FLASH_Unlock();
    
    uint32_t* pData = (uint32_t*)&entry;
    for (uint32_t i = 0; i < sizeof(ErrorLogEntry_t) / 4; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + i * 4, pData[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return HAL_ERROR;
        }
    }
    
    HAL_FLASH_Lock();
    
    /* 更新索引 */
    logWriteIndex = (logWriteIndex + 1) % ERROR_LOG_MAX_ENTRIES;
    if (logCount < ERROR_LOG_MAX_ENTRIES) {
        logCount++;
    }
    
    return HAL_OK;
}

uint16_t errorLogRead(ErrorLogEntry_t* buffer, uint16_t count)
{
    if (buffer == NULL || count == 0) {
        return 0;
    }
    
    /* 限制读取数量 */
    uint16_t readCount = (count > logCount) ? logCount : count;
    
    /* 从最新的日志开始读取 */
    for (uint16_t i = 0; i < readCount; i++) {
        uint16_t index = (logWriteIndex - 1 - i + ERROR_LOG_MAX_ENTRIES) % ERROR_LOG_MAX_ENTRIES;
        uint32_t address = getLogEntryAddress(index);
        
        /* 从Flash读取 */
        uint32_t* pFlash = (uint32_t*)address;
        uint32_t* pBuffer = (uint32_t*)&buffer[i];
        
        for (uint32_t j = 0; j < sizeof(ErrorLogEntry_t) / 4; j++) {
            pBuffer[j] = pFlash[j];
        }
    }
    
    return readCount;
}

uint16_t errorLogGetCount(void)
{
    return logCount;
}

HAL_StatusTypeDef errorLogClear(void)
{
    if (eraseLogPage() != HAL_OK) {
        return HAL_ERROR;
    }
    
    logCount = 0;
    logWriteIndex = 0;
    
    return HAL_OK;
}

const char* errorGetDescription(ErrorCode_e code)
{
    switch (code) {
        /* 系统错误 */
        case ERROR_NONE:                    return "No Error";
        case ERROR_SYSTEM_INIT_FAILED:      return "System Init Failed";
        case ERROR_WATCHDOG_TIMEOUT:        return "Watchdog Timeout";
        case ERROR_MEMORY_OVERFLOW:         return "Memory Overflow";
        
        /* 通信错误 */
        case ERROR_MODBUS_TIMEOUT:          return "Modbus Timeout";
        case ERROR_MODBUS_CRC_ERROR:        return "Modbus CRC Error";
        case ERROR_UART_TX_FAILED:          return "UART TX Failed";
        case ERROR_UART_RX_OVERFLOW:        return "UART RX Overflow";
        
        /* ADC/传感器错误 */
        case ERROR_ADC_INIT_FAILED:         return "ADC Init Failed";
        case ERROR_ADC_CONVERSION_TIMEOUT:  return "ADC Timeout";
        case ERROR_PRESSURE_SENSOR_FAULT:   return "Pressure Sensor Fault";
        case ERROR_PRESSURE_OUT_OF_RANGE:   return "Pressure Out of Range";
        
        /* 水位错误 */
        case ERROR_WATER_LEVEL_ABNORMAL:    return "Water Level Abnormal";
        case ERROR_WATER_LOW_BUT_MID_HIGH:  return "Low Dry But Mid/High Wet";
        case ERROR_WATER_DISCONTINUOUS:     return "Water Discontinuous";
        
        /* 硬件错误 */
        case ERROR_RELAY_FAULT:             return "Relay Fault";
        case ERROR_BEEP_INIT_FAILED:        return "Beep Init Failed";
        case ERROR_GPIO_INIT_FAILED:        return "GPIO Init Failed";
        
        /* 配置错误 */
        case ERROR_CONFIG_INVALID:          return "Config Invalid";
        case ERROR_CONFIG_CHECKSUM_FAILED:  return "Config Checksum Failed";
        case ERROR_FLASH_WRITE_FAILED:      return "Flash Write Failed";
        case ERROR_FLASH_ERASE_FAILED:      return "Flash Erase Failed";
        
        default:                            return "Unknown Error";
    }
}

