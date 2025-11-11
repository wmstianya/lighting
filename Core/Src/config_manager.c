/**
 * @file config_manager.c
 * @brief 系统配置管理模块实现
 * @details Flash配置存储、读取、校验
 * @author Lighting Ultra Team
 * @date 2025-11-11
 */

#include "config_manager.h"
#include <string.h>

/* ==================== 私有变量 ==================== */
static SystemConfig_t systemConfig;
static bool configLoaded = false;

/* ==================== 私有函数 ==================== */
/**
 * @brief 擦除配置Flash页
 */
static HAL_StatusTypeDef eraseConfigPage(void)
{
    HAL_FLASH_Unlock();
    
    FLASH_EraseInitTypeDef eraseInit;
    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInit.PageAddress = CONFIG_FLASH_BASE_ADDR;
    eraseInit.NbPages = 1;
    
    uint32_t pageError = 0;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&eraseInit, &pageError);
    
    HAL_FLASH_Lock();
    return status;
}

/**
 * @brief 写入配置到Flash
 */
static HAL_StatusTypeDef writeConfigToFlash(const SystemConfig_t* config)
{
    HAL_StatusTypeDef status;
    uint32_t* pData = (uint32_t*)config;
    uint32_t address = CONFIG_FLASH_BASE_ADDR;
    uint32_t dataSize = sizeof(SystemConfig_t);
    
    HAL_FLASH_Unlock();
    
    /* 按32位写入 */
    for (uint32_t i = 0; i < dataSize / 4; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, pData[i]);
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return HAL_ERROR;
        }
        address += 4;
    }
    
    HAL_FLASH_Lock();
    return HAL_OK;
}

/**
 * @brief 从Flash读取配置
 */
static HAL_StatusTypeDef readConfigFromFlash(SystemConfig_t* config)
{
    uint32_t* pFlash = (uint32_t*)CONFIG_FLASH_BASE_ADDR;
    uint32_t* pConfig = (uint32_t*)config;
    uint32_t dataSize = sizeof(SystemConfig_t);
    
    /* 从Flash读取 */
    for (uint32_t i = 0; i < dataSize / 4; i++) {
        pConfig[i] = pFlash[i];
    }
    
    return HAL_OK;
}

/**
 * @brief 设置默认配置
 */
static void setDefaultConfig(SystemConfig_t* config)
{
    /* 头部 */
    config->magicNumber = CONFIG_MAGIC_NUMBER;
    config->version = CONFIG_VERSION;
    config->dataSize = sizeof(SystemConfig_t);
    
    /* Modbus配置 */
    config->modbus1SlaveAddr = DEFAULT_MODBUS1_ADDR;
    config->modbus2SlaveAddr = DEFAULT_MODBUS2_ADDR;
    config->modbus1Baudrate = DEFAULT_MODBUS_BAUDRATE;
    config->modbus2Baudrate = DEFAULT_MODBUS_BAUDRATE;
    
    /* 传感器配置 */
    config->pressureMin = DEFAULT_PRESSURE_MIN;
    config->pressureMax = DEFAULT_PRESSURE_MAX;
    config->pressureSampleInterval = DEFAULT_PRESSURE_INTERVAL;
    
    /* 水位配置 */
    config->waterLevelDebounceTime = DEFAULT_WATER_DEBOUNCE;
    config->waterLevelSampleInterval = DEFAULT_WATER_INTERVAL;
    
    /* 蜂鸣器配置 */
    config->beepFrequency = DEFAULT_BEEP_FREQ;
    config->beepDefaultDuration = DEFAULT_BEEP_DURATION;
    
    /* 看门狗配置 */
    config->watchdogFeedInterval = DEFAULT_WDT_INTERVAL;
    
    /* 系统配置 */
    config->enableWatchdog = true;
    config->enableBeep = true;
    config->systemMode = 0;
    
    /* 计算校验和 */
    config->checksum = configCalculateChecksum(config);
}

/* ==================== API函数实现 ==================== */
uint32_t configCalculateChecksum(const SystemConfig_t* config)
{
    /* 简单CRC32校验（不包含checksum字段本身） */
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* pData = (const uint8_t*)config;
    uint32_t dataLen = sizeof(SystemConfig_t) - sizeof(uint32_t);
    
    for (uint32_t i = 0; i < dataLen; i++) {
        crc ^= pData[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return ~crc;
}

bool configIsValid(void)
{
    /* 检查魔术字 */
    if (systemConfig.magicNumber != CONFIG_MAGIC_NUMBER) {
        return false;
    }
    
    /* 检查版本 */
    if (systemConfig.version != CONFIG_VERSION) {
        return false;
    }
    
    /* 检查校验和 */
    uint32_t calculatedCrc = configCalculateChecksum(&systemConfig);
    if (systemConfig.checksum != calculatedCrc) {
        return false;
    }
    
    /* 检查参数合理性 */
    if (systemConfig.modbus1SlaveAddr == 0 || systemConfig.modbus1SlaveAddr > 247) {
        return false;
    }
    
    if (systemConfig.pressureMin >= systemConfig.pressureMax) {
        return false;
    }
    
    return true;
}

HAL_StatusTypeDef configManagerInit(void)
{
#if ENABLE_FLASH_STORAGE
    /* Flash存储已启用：从Flash读取配置 */
    SystemConfig_t tempConfig;
    
    if (readConfigFromFlash(&tempConfig) == HAL_OK) {
        memcpy(&systemConfig, &tempConfig, sizeof(SystemConfig_t));
        
        /* 验证配置有效性 */
        if (configIsValid()) {
            configLoaded = true;
            return HAL_OK;
        }
    }
    
    /* Flash配置无效或读取失败，使用默认配置 */
    setDefaultConfig(&systemConfig);
    configLoaded = false;
    
    /* 注意：不在初始化时自动保存，避免Flash擦除延迟 */
    /* 用户需要手动调用configSave()保存配置 */
#else
    /* Flash存储已禁用：直接使用默认配置 */
    setDefaultConfig(&systemConfig);
    configLoaded = false;
#endif
    
    return HAL_OK;
}

const SystemConfig_t* configGet(void)
{
    return &systemConfig;
}

HAL_StatusTypeDef configSave(void)
{
#if ENABLE_FLASH_STORAGE
    /* Flash存储已启用：保存配置到Flash */
    
    /* 重新计算校验和 */
    systemConfig.checksum = configCalculateChecksum(&systemConfig);
    
    /* 擦除Flash页 */
    if (eraseConfigPage() != HAL_OK) {
        return HAL_ERROR;
    }
    
    /* 写入Flash */
    if (writeConfigToFlash(&systemConfig) != HAL_OK) {
        return HAL_ERROR;
    }
    
    configLoaded = true;
    return HAL_OK;
#else
    /* Flash存储已禁用：返回成功但不实际保存 */
    return HAL_OK;
#endif
}

HAL_StatusTypeDef configResetToDefault(void)
{
    /* 设置默认配置 */
    setDefaultConfig(&systemConfig);
    
    /* 保存到Flash */
    return configSave();
}

HAL_StatusTypeDef configSetModbus(uint8_t uart1Addr, uint8_t uart2Addr)
{
    /* 参数检查 */
    if (uart1Addr == 0 || uart1Addr > 247 || 
        uart2Addr == 0 || uart2Addr > 247) {
        return HAL_ERROR;
    }
    
    systemConfig.modbus1SlaveAddr = uart1Addr;
    systemConfig.modbus2SlaveAddr = uart2Addr;
    
    return HAL_OK;
}

HAL_StatusTypeDef configSetPressure(float min, float max)
{
    /* 参数检查 */
    if (min >= max || min < 0.0f || max > 10.0f) {
        return HAL_ERROR;
    }
    
    systemConfig.pressureMin = min;
    systemConfig.pressureMax = max;
    
    return HAL_OK;
}

