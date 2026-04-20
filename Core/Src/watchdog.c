/**
 * @file watchdog.c
 * @brief 外部看门狗驱动模块实现
 * @details TPS3823-33DBVR外部看门狗芯片驱动实现
 * @author Lighting Ultra Team
 * @date 2025-11-11
 * 
 * 修订历史：
 * - v1.0.0: 初始版本，实现基本喂狗功能
 */

#include "watchdog.h"

/* ==================== 私有变量 ==================== */
static WatchdogStats_t wdtStats = {
    .feedCount = 0,
    .lastFeedTick = 0,
    .isInitialized = false
};

/* ==================== API函数实现 ==================== */
HAL_StatusTypeDef watchdogInit(void)
{
    /* 确保GPIO时钟已使能 */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    /* 配置PC14为推挽输出 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = WDT_GPIO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(WDT_GPIO_PORT, &GPIO_InitStruct);
    
    /* 初始状态设为低电平 */
    HAL_GPIO_WritePin(WDT_GPIO_PORT, WDT_GPIO_PIN, GPIO_PIN_RESET);
    
    /* 初始化统计信息 */
    wdtStats.feedCount = 0;
    wdtStats.lastFeedTick = HAL_GetTick();
    wdtStats.isInitialized = true;
    
    return HAL_OK;
}

void watchdogFeed(void)
{
    uint32_t currentTick = HAL_GetTick();
    
    /* 检查是否到达喂狗间隔 */
    if (currentTick - wdtStats.lastFeedTick >= WDT_FEED_INTERVAL_MS) {
        /* 翻转PC14电平（喂狗） */
        HAL_GPIO_TogglePin(WDT_GPIO_PORT, WDT_GPIO_PIN);
        
        /* 更新统计信息 */
        wdtStats.lastFeedTick = currentTick;
        wdtStats.feedCount++;
    }
}

void watchdogFeedImmediate(void)
{
    /* 立即翻转PC14电平 */
    HAL_GPIO_TogglePin(WDT_GPIO_PORT, WDT_GPIO_PIN);
    
    /* 更新统计信息 */
    wdtStats.lastFeedTick = HAL_GetTick();
    wdtStats.feedCount++;
}

WatchdogStats_t watchdogGetStats(void)
{
    return wdtStats;
}

uint32_t watchdogGetTimeSinceLastFeed(void)
{
    return HAL_GetTick() - wdtStats.lastFeedTick;
}

bool watchdogIsHealthy(void)
{
    /* 检查是否已初始化 */
    if (!wdtStats.isInitialized) {
        return false;
    }
    
    /* 检查喂狗间隔是否正常（不应超过超时时间的80%） */
    uint32_t timeSinceFeed = watchdogGetTimeSinceLastFeed();
    if (timeSinceFeed > (WDT_TIMEOUT_MS * 80 / 100)) {
        return false;  /* 喂狗间隔过长，可能有问题 */
    }
    
    return true;
}

