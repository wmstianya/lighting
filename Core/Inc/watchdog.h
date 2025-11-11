/**
 * @file watchdog.h
 * @brief 外部看门狗驱动模块头文件
 * @details TPS3823-33DBVR外部看门狗芯片驱动
 * @author Lighting Ultra Team
 * @date 2025-11-11
 * 
 * 硬件连接：
 * - PC14 (STM32) -> WDI (TPS3823-33DBVR)
 * - NRST (STM32) <- RESET (TPS3823-33DBVR)
 * 
 * 工作原理：
 * - PC14每500ms翻转一次（喂狗）
 * - 如果1.6s内未喂狗，TPS3823拉低RESET引脚
 * - RESET拉低NRST，强制STM32复位
 */

#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

/* ==================== 硬件配置 ==================== */
#define WDT_GPIO_PORT       GPIOC
#define WDT_GPIO_PIN        GPIO_PIN_14

/* ==================== 时序参数 ==================== */
/**
 * @brief 喂狗间隔时间（毫秒）
 * @note TPS3823-33DBVR超时时间为1.6s，设置500ms提供3倍安全裕量
 */
#define WDT_FEED_INTERVAL_MS    500

/**
 * @brief TPS3823-33DBVR看门狗超时时间（毫秒）
 * @note 典型值1.6s，仅供参考
 */
#define WDT_TIMEOUT_MS          1600

/* ==================== 数据结构 ==================== */
/**
 * @brief 看门狗统计信息
 */
typedef struct {
    uint32_t feedCount;      /**< 喂狗次数 */
    uint32_t lastFeedTick;   /**< 上次喂狗时间戳 */
    bool isInitialized;      /**< 初始化标志 */
} WatchdogStats_t;

/* ==================== API函数 ==================== */
/**
 * @brief 初始化看门狗模块
 * @details 配置PC14为输出模式，初始状态为低电平
 * @return HAL_StatusTypeDef HAL状态码
 * @retval HAL_OK 初始化成功
 */
HAL_StatusTypeDef watchdogInit(void);

/**
 * @brief 看门狗喂狗函数（非阻塞）
 * @details 在主循环中调用，每500ms自动翻转PC14电平
 * @note 必须在主循环中周期性调用，否则系统会被复位
 */
void watchdogFeed(void);

/**
 * @brief 手动喂狗（立即翻转）
 * @details 立即翻转PC14，不受500ms间隔限制
 * @note 一般情况下使用watchdogFeed()即可
 */
void watchdogFeedImmediate(void);

/**
 * @brief 获取看门狗统计信息
 * @return WatchdogStats_t 看门狗统计数据
 */
WatchdogStats_t watchdogGetStats(void);

/**
 * @brief 获取距离上次喂狗的时间（毫秒）
 * @return uint32_t 时间间隔（ms）
 */
uint32_t watchdogGetTimeSinceLastFeed(void);

/**
 * @brief 检查看门狗是否正常工作
 * @return bool 工作状态
 * @retval true 看门狗正常
 * @retval false 看门狗未初始化或喂狗超时
 */
bool watchdogIsHealthy(void);

#endif /* WATCHDOG_H */

