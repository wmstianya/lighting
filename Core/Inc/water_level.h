/**
 * @file water_level.h
 * @brief 水位检测驱动模块头文件
 * @details 三点式水位探针检测（低、中、高水位），带防抖处理
 * @author Lighting Ultra Team
 * @date 2025-11-11
 * 
 * 硬件连接：
 * - DI1 (PA0) -> 低水位探针（低电平有效）
 * - DI2 (PA1) -> 中水位探针（低电平有效）
 * - DI3 (PA5) -> 高水位探针（低电平有效）
 * 
 * 水位逻辑：
 * - 无水：   DI1=1, DI2=1, DI3=1
 * - 低水位： DI1=0, DI2=1, DI3=1
 * - 中水位： DI1=0, DI2=0, DI3=1
 * - 高水位： DI1=0, DI2=0, DI3=0
 */

#ifndef WATER_LEVEL_H
#define WATER_LEVEL_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

/* ==================== 硬件配置 ==================== */
#define WATER_LEVEL_GPIO_PORT       GPIOA

#define WATER_LEVEL_LOW_PIN         GPIO_PIN_0   /* DI1 - 低水位 */
#define WATER_LEVEL_MID_PIN         GPIO_PIN_1   /* DI2 - 中水位 */
#define WATER_LEVEL_HIGH_PIN        GPIO_PIN_5   /* DI3 - 高水位 */

/* ==================== 防抖配置 ==================== */
#define WATER_LEVEL_DEBOUNCE_TIME_MS    200   /* 防抖时间200ms */
#define WATER_LEVEL_SAMPLE_INTERVAL_MS  50    /* 采样间隔50ms */

/* ==================== 水位状态枚举 ==================== */
/**
 * @brief 水位状态枚举
 */
typedef enum {
    WATER_LEVEL_NONE = 0,   /**< 无水 */
    WATER_LEVEL_LOW,        /**< 低水位 */
    WATER_LEVEL_MID,        /**< 中水位 */
    WATER_LEVEL_HIGH,       /**< 高水位 */
    WATER_LEVEL_ERROR       /**< 异常状态（传感器故障） */
} WaterLevelState_e;

/* ==================== 探针状态 ==================== */
/**
 * @brief 单个探针状态
 */
typedef struct {
    bool currentState;      /**< 当前状态（0=有水，1=无水） */
    bool stableState;       /**< 稳定状态（防抖后） */
    uint32_t lastChangeTime; /**< 上次状态变化时间 */
    uint16_t debounceCount;  /**< 防抖计数器 */
} ProbeState_t;

/**
 * @brief 水位检测数据结构
 */
typedef struct {
    ProbeState_t lowProbe;   /**< 低水位探针 */
    ProbeState_t midProbe;   /**< 中水位探针 */
    ProbeState_t highProbe;  /**< 高水位探针 */
    WaterLevelState_e currentLevel;  /**< 当前水位 */
    WaterLevelState_e lastLevel;     /**< 上次水位 */
    uint32_t levelChangeTime;        /**< 水位变化时间 */
    uint32_t sampleCount;            /**< 采样计数 */
} WaterLevelData_t;

/* ==================== 回调函数类型 ==================== */
/**
 * @brief 水位变化回调函数类型
 * @param oldLevel 旧水位
 * @param newLevel 新水位
 */
typedef void (*WaterLevelChangeCallback_t)(WaterLevelState_e oldLevel, WaterLevelState_e newLevel);

/* ==================== API函数 ==================== */
/**
 * @brief 初始化水位检测模块
 * @return HAL_StatusTypeDef HAL状态码
 */
HAL_StatusTypeDef waterLevelInit(void);

/**
 * @brief 水位检测处理函数（在主循环中调用）
 * @details 自动50ms采样一次，非阻塞
 */
void waterLevelProcess(void);

/**
 * @brief 获取当前水位状态
 * @return WaterLevelState_e 当前水位
 */
WaterLevelState_e waterLevelGetLevel(void);

/**
 * @brief 获取水位状态字符串
 * @param level 水位状态
 * @return const char* 水位状态字符串
 */
const char* waterLevelGetLevelString(WaterLevelState_e level);

/**
 * @brief 获取探针原始状态
 * @param low 低水位探针状态（输出参数）
 * @param mid 中水位探针状态（输出参数）
 * @param high 高水位探针状态（输出参数）
 * @note 0=有水，1=无水
 */
void waterLevelGetProbeStates(bool* low, bool* mid, bool* high);

/**
 * @brief 获取完整水位数据
 * @return WaterLevelData_t 水位数据结构体
 */
WaterLevelData_t waterLevelGetData(void);

/**
 * @brief 注册水位变化回调函数
 * @param callback 回调函数指针
 */
void waterLevelSetCallback(WaterLevelChangeCallback_t callback);

/**
 * @brief 检查水位是否稳定
 * @return bool 稳定性
 * @retval true 水位稳定（最近200ms无变化）
 * @retval false 水位不稳定
 */
bool waterLevelIsStable(void);

#endif /* WATER_LEVEL_H */

