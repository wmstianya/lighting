/**
 * @file led.c
 * @brief LED指示灯驱动模块实现
 * @details 提供4路LED指示灯控制，低电平点亮
 * @author Lighting Ultra Team
 * @date 2025-11-11
 */

#include "led.h"

/* ==================== 私有变量 ==================== */
/**
 * @brief LED配置数组
 * @note LED低电平点亮，对应DO1-DO4状态指示
 */
static LedConfig_t ledConfigs[LED_CHANNEL_COUNT] = {
    {GPIOB, GPIO_PIN_1,  LED_STATE_OFF},  // LED1 - PB1  - DO1指示
    {GPIOB, GPIO_PIN_15, LED_STATE_OFF},  // LED2 - PB15 - DO2指示
    {GPIOB, GPIO_PIN_5,  LED_STATE_OFF},  // LED3 - PB5  - DO3指示
    {GPIOB, GPIO_PIN_6,  LED_STATE_OFF}   // LED4 - PB6  - DO4指示
};

/* ==================== 私有函数 ==================== */
/**
 * @brief 验证LED通道参数的有效性
 * @param channel LED通道
 * @return bool 参数有效性
 */
static bool isValidChannel(LedChannel_e channel)
{
    return (channel >= LED_CHANNEL_1 && channel < LED_CHANNEL_COUNT);
}

/**
 * @brief 设置GPIO引脚状态
 * @param config LED配置指针
 * @param state 目标状态
 * @note LED低电平点亮：ON->RESET, OFF->SET
 */
static void setGpioState(LedConfig_t* config, LedState_e state)
{
    GPIO_PinState pinState = (state == LED_STATE_ON) ? 
                            GPIO_PIN_RESET : GPIO_PIN_SET;
    HAL_GPIO_WritePin(config->port, config->pin, pinState);
}

/* ==================== API函数实现 ==================== */
HAL_StatusTypeDef ledInit(void)
{
    /* 确保GPIO时钟已使能 */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
    /* 初始化所有LED引脚 */
    for (int i = 0; i < LED_CHANNEL_COUNT; i++) {
        GPIO_InitStruct.Pin = ledConfigs[i].pin;
        HAL_GPIO_Init(ledConfigs[i].port, &GPIO_InitStruct);
        
        /* 默认关闭LED（高电平） */
        setGpioState(&ledConfigs[i], LED_STATE_OFF);
        ledConfigs[i].state = LED_STATE_OFF;
    }
    
    return HAL_OK;
}

HAL_StatusTypeDef ledSetState(LedChannel_e channel, LedState_e state)
{
    if (!isValidChannel(channel)) {
        return HAL_ERROR;
    }
    
    LedConfig_t* config = &ledConfigs[channel];
    
    /* 只有状态真正改变时才操作GPIO，避免重复操作 */
    if (config->state != state) {
        setGpioState(config, state);
        config->state = state;
    }
    
    return HAL_OK;
}

LedState_e ledGetState(LedChannel_e channel)
{
    if (!isValidChannel(channel)) {
        return LED_STATE_OFF;
    }
    
    return ledConfigs[channel].state;
}

HAL_StatusTypeDef ledToggle(LedChannel_e channel)
{
    if (!isValidChannel(channel)) {
        return HAL_ERROR;
    }
    
    LedState_e currentState = ledConfigs[channel].state;
    LedState_e newState = (currentState == LED_STATE_OFF) ? 
                         LED_STATE_ON : LED_STATE_OFF;
    
    return ledSetState(channel, newState);
}

HAL_StatusTypeDef ledSetAllStates(uint8_t stateMask)
{
    HAL_StatusTypeDef status = HAL_OK;
    
    for (int i = 0; i < LED_CHANNEL_COUNT; i++) {
        LedState_e state = (stateMask & (1 << i)) ? 
                          LED_STATE_ON : LED_STATE_OFF;
        
        if (ledSetState((LedChannel_e)i, state) != HAL_OK) {
            status = HAL_ERROR;
        }
    }
    
    return status;
}

HAL_StatusTypeDef ledTurnOffAll(void)
{
    return ledSetAllStates(0x00);
}

