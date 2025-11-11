/**
 * @file water_level.c
 * @brief 水位检测驱动模块实现
 * @details 三点式水位探针检测，低电平有效，带防抖处理
 * @author Lighting Ultra Team
 * @date 2025-11-11
 */

#include "water_level.h"

/* ==================== 私有变量 ==================== */
static WaterLevelData_t waterLevelData;
static uint32_t lastSampleTick = 0;
static WaterLevelChangeCallback_t changeCallback = NULL;

/* ==================== 私有函数 ==================== */
/**
 * @brief 读取探针GPIO状态
 * @param pin GPIO引脚
 * @return bool 探针状态（0=有水，1=无水）
 */
static bool readProbeState(uint16_t pin)
{
    /* 低电平有效：低电平=有水(0)，高电平=无水(1) */
    GPIO_PinState pinState = HAL_GPIO_ReadPin(WATER_LEVEL_GPIO_PORT, pin);
    return (pinState == GPIO_PIN_SET) ? true : false;
}

/**
 * @brief 更新探针防抖状态
 * @param probe 探针状态指针
 * @param currentState 当前采样状态
 */
static void updateProbeDebounce(ProbeState_t* probe, bool currentState)
{
    /* 如果状态与稳定状态不同 */
    if (currentState != probe->stableState) {
        /* 检查是否是新的变化 */
        if (currentState != probe->currentState) {
            /* 状态发生变化，重置防抖计数 */
            probe->currentState = currentState;
            probe->lastChangeTime = HAL_GetTick();
            probe->debounceCount = 0;
        } else {
            /* 状态持续相同，增加计数 */
            probe->debounceCount++;
            
            /* 达到防抖时间，确认状态变化 */
            uint32_t elapsed = HAL_GetTick() - probe->lastChangeTime;
            if (elapsed >= WATER_LEVEL_DEBOUNCE_TIME_MS) {
                probe->stableState = currentState;
            }
        }
    } else {
        /* 当前状态与稳定状态相同，重置计数 */
        probe->currentState = currentState;
        probe->debounceCount = 0;
    }
}

/**
 * @brief 根据探针状态判断水位
 * @param lowWater 低水位探针（0=有水）
 * @param midWater 中水位探针（0=有水）
 * @param highWater 高水位探针（0=有水）
 * @return WaterLevelState_e 水位状态
 */
static WaterLevelState_e determineWaterLevel(bool lowWater, bool midWater, bool highWater)
{
    /* 低电平=有水(0)，高电平=无水(1)
     * 有水的逻辑：探针接触水面，导电，GPIO被拉低
     */
    
    /* 高水位：所有探针都有水 */
    if (!lowWater && !midWater && !highWater) {
        return WATER_LEVEL_HIGH;
    }
    
    /* 中水位：低和中探针有水，高探针无水 */
    if (!lowWater && !midWater && highWater) {
        return WATER_LEVEL_MID;
    }
    
    /* 低水位：只有低探针有水 */
    if (!lowWater && midWater && highWater) {
        return WATER_LEVEL_LOW;
    }
    
    /* 无水：所有探针都无水 */
    if (lowWater && midWater && highWater) {
        return WATER_LEVEL_NONE;
    }
    
    /* 异常状态（如中探针有水但低探针无水，不符合物理规律） */
    return WATER_LEVEL_ERROR;
}

/* ==================== API函数实现 ==================== */
HAL_StatusTypeDef waterLevelInit(void)
{
    /* 使能GPIOA时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    /* 配置GPIO为上拉输入（低电平有效，需要上拉） */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = WATER_LEVEL_LOW_PIN | WATER_LEVEL_MID_PIN | WATER_LEVEL_HIGH_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;  /* 上拉，探针导通时拉低 */
    HAL_GPIO_Init(WATER_LEVEL_GPIO_PORT, &GPIO_InitStruct);
    
    /* 初始化数据结构 */
    waterLevelData.lowProbe.currentState = true;
    waterLevelData.lowProbe.stableState = true;
    waterLevelData.lowProbe.lastChangeTime = HAL_GetTick();
    waterLevelData.lowProbe.debounceCount = 0;
    
    waterLevelData.midProbe.currentState = true;
    waterLevelData.midProbe.stableState = true;
    waterLevelData.midProbe.lastChangeTime = HAL_GetTick();
    waterLevelData.midProbe.debounceCount = 0;
    
    waterLevelData.highProbe.currentState = true;
    waterLevelData.highProbe.stableState = true;
    waterLevelData.highProbe.lastChangeTime = HAL_GetTick();
    waterLevelData.highProbe.debounceCount = 0;
    
    waterLevelData.currentLevel = WATER_LEVEL_NONE;
    waterLevelData.lastLevel = WATER_LEVEL_NONE;
    waterLevelData.levelChangeTime = HAL_GetTick();
    waterLevelData.sampleCount = 0;
    
    lastSampleTick = HAL_GetTick();
    
    return HAL_OK;
}

void waterLevelProcess(void)
{
    uint32_t currentTick = HAL_GetTick();
    
    /* 检查是否到达采样间隔 */
    if (currentTick - lastSampleTick < WATER_LEVEL_SAMPLE_INTERVAL_MS) {
        return;
    }
    lastSampleTick = currentTick;
    
    /* 读取三个探针的当前状态 */
    bool lowState = readProbeState(WATER_LEVEL_LOW_PIN);
    bool midState = readProbeState(WATER_LEVEL_MID_PIN);
    bool highState = readProbeState(WATER_LEVEL_HIGH_PIN);
    
    /* 更新各探针的防抖状态 */
    updateProbeDebounce(&waterLevelData.lowProbe, lowState);
    updateProbeDebounce(&waterLevelData.midProbe, midState);
    updateProbeDebounce(&waterLevelData.highProbe, highState);
    
    /* 根据稳定状态判断水位 */
    WaterLevelState_e newLevel = determineWaterLevel(
        waterLevelData.lowProbe.stableState,
        waterLevelData.midProbe.stableState,
        waterLevelData.highProbe.stableState
    );
    
    /* 检查水位是否发生变化 */
    if (newLevel != waterLevelData.currentLevel) {
        waterLevelData.lastLevel = waterLevelData.currentLevel;
        waterLevelData.currentLevel = newLevel;
        waterLevelData.levelChangeTime = currentTick;
        
        /* 触发回调 */
        if (changeCallback != NULL) {
            changeCallback(waterLevelData.lastLevel, newLevel);
        }
    }
    
    /* 更新采样计数 */
    waterLevelData.sampleCount++;
}

WaterLevelState_e waterLevelGetLevel(void)
{
    return waterLevelData.currentLevel;
}

const char* waterLevelGetLevelString(WaterLevelState_e level)
{
    switch (level) {
        case WATER_LEVEL_NONE:  return "No Water";
        case WATER_LEVEL_LOW:   return "Low";
        case WATER_LEVEL_MID:   return "Medium";
        case WATER_LEVEL_HIGH:  return "High";
        case WATER_LEVEL_ERROR: return "Error";
        default:                return "Unknown";
    }
}

void waterLevelGetProbeStates(bool* low, bool* mid, bool* high)
{
    if (low != NULL) {
        *low = waterLevelData.lowProbe.stableState;
    }
    if (mid != NULL) {
        *mid = waterLevelData.midProbe.stableState;
    }
    if (high != NULL) {
        *high = waterLevelData.highProbe.stableState;
    }
}

WaterLevelData_t waterLevelGetData(void)
{
    return waterLevelData;
}

void waterLevelSetCallback(WaterLevelChangeCallback_t callback)
{
    changeCallback = callback;
}

bool waterLevelIsStable(void)
{
    uint32_t currentTick = HAL_GetTick();
    uint32_t timeSinceChange = currentTick - waterLevelData.levelChangeTime;
    
    /* 如果最近200ms内没有水位变化，认为稳定 */
    return (timeSinceChange >= WATER_LEVEL_DEBOUNCE_TIME_MS);
}

