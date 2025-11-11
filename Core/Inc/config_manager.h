/**
 * @file config_manager.h
 * @brief 系统配置管理模块头文件
 * @details Flash配置存储、读取、校验和恢复出厂设置
 * @author Lighting Ultra Team
 * @date 2025-11-11
 * 
 * 配置存储：
 * - 使用Flash最后一页（STM32F103C8T6: 0x0800F800）
 * - 包含配置校验和，防止数据损坏
 * - 支持恢复出厂设置
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

/* ==================== 功能开关 ==================== */
/**
 * @brief Flash功能总开关
 * @note 设置为1启用Flash读写，设置为0禁用Flash（使用默认配置）
 */
#define ENABLE_FLASH_STORAGE    0   /* 0=禁用Flash, 1=启用Flash */

/* ==================== Flash配置 ==================== */
#define CONFIG_FLASH_PAGE_SIZE      2048        /* STM32F103 Flash页大小2KB */
#define CONFIG_FLASH_BASE_ADDR      0x0800F800  /* C8T6最后一页地址（64KB） */
#define CONFIG_MAGIC_NUMBER         0x4C544C55  /* "LTLU" ASCII码 */
#define CONFIG_VERSION              0x0100      /* 版本1.0 */

/* ==================== 系统配置结构体 ==================== */
/**
 * @brief 系统配置结构体
 */
typedef struct {
    /* 配置头部 */
    uint32_t magicNumber;       /**< 魔术字：0x4C544C55 */
    uint16_t version;           /**< 配置版本号 */
    uint16_t dataSize;          /**< 数据大小（字节） */
    
    /* Modbus配置 */
    uint8_t modbus1SlaveAddr;   /**< UART1 Modbus从站地址 (1-247) */
    uint8_t modbus2SlaveAddr;   /**< UART2 Modbus从站地址 (1-247) */
    uint32_t modbus1Baudrate;   /**< UART1波特率 */
    uint32_t modbus2Baudrate;   /**< UART2波特率 */
    
    /* 传感器配置 */
    float pressureMin;          /**< 压力下限 (MPa) */
    float pressureMax;          /**< 压力上限 (MPa) */
    uint16_t pressureSampleInterval; /**< 压力采样间隔 (ms) */
    
    /* 水位配置 */
    uint16_t waterLevelDebounceTime;  /**< 水位防抖时间 (ms) */
    uint16_t waterLevelSampleInterval; /**< 水位采样间隔 (ms) */
    
    /* 蜂鸣器配置 */
    uint16_t beepFrequency;     /**< 蜂鸣器频率 (Hz) */
    uint16_t beepDefaultDuration; /**< 默认鸣叫时长 (ms) */
    
    /* 看门狗配置 */
    uint16_t watchdogFeedInterval; /**< 喂狗间隔 (ms) */
    
    /* 系统配置 */
    bool enableWatchdog;        /**< 使能看门狗 */
    bool enableBeep;            /**< 使能蜂鸣器 */
    uint8_t systemMode;         /**< 系统模式 (0=正常, 1=调试) */
    
    /* 校验和（必须放在最后） */
    uint32_t checksum;          /**< CRC32校验和 */
} SystemConfig_t;

/* ==================== 默认配置 ==================== */
#define DEFAULT_MODBUS1_ADDR        0x01
#define DEFAULT_MODBUS2_ADDR        0x02
#define DEFAULT_MODBUS_BAUDRATE     115200
#define DEFAULT_PRESSURE_MIN        0.0f
#define DEFAULT_PRESSURE_MAX        1.6f
#define DEFAULT_PRESSURE_INTERVAL   100
#define DEFAULT_WATER_DEBOUNCE      200
#define DEFAULT_WATER_INTERVAL      50
#define DEFAULT_BEEP_FREQ           2700
#define DEFAULT_BEEP_DURATION       200
#define DEFAULT_WDT_INTERVAL        500

/* ==================== API函数 ==================== */
/**
 * @brief 初始化配置管理模块
 * @details 从Flash读取配置，若无效则使用默认配置
 * @return HAL_StatusTypeDef HAL状态码
 */
HAL_StatusTypeDef configManagerInit(void);

/**
 * @brief 获取系统配置
 * @return SystemConfig_t* 配置指针（只读）
 */
const SystemConfig_t* configGet(void);

/**
 * @brief 保存配置到Flash
 * @return HAL_StatusTypeDef HAL状态码
 * @retval HAL_OK 保存成功
 * @retval HAL_ERROR Flash操作失败
 */
HAL_StatusTypeDef configSave(void);

/**
 * @brief 恢复出厂设置
 * @return HAL_StatusTypeDef HAL状态码
 */
HAL_StatusTypeDef configResetToDefault(void);

/**
 * @brief 更新Modbus配置
 * @param uart1Addr UART1从站地址
 * @param uart2Addr UART2从站地址
 * @return HAL_StatusTypeDef HAL状态码
 */
HAL_StatusTypeDef configSetModbus(uint8_t uart1Addr, uint8_t uart2Addr);

/**
 * @brief 更新压力传感器配置
 * @param min 压力下限
 * @param max 压力上限
 * @return HAL_StatusTypeDef HAL状态码
 */
HAL_StatusTypeDef configSetPressure(float min, float max);

/**
 * @brief 验证配置有效性
 * @return bool 配置有效性
 */
bool configIsValid(void);

/**
 * @brief 计算配置校验和
 * @param config 配置指针
 * @return uint32_t CRC32校验和
 */
uint32_t configCalculateChecksum(const SystemConfig_t* config);

#endif /* CONFIG_MANAGER_H */

