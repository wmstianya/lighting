/**
 * @file relay.c
 * @brief 继电器输出控制驱动程序实现
 * @details
 * 该模块提供5路继电器的硬件抽象层实现，支持独立控制每路继电器。
 * 使用GPIO输出方式控制继电器，提供完整的状态管理功能。
 * 
 * @author Lighting Ultra Team  
 * @date 2025-01-20
 * @version 1.0.0
 * 
 * 修订历史：
 * - v1.0.0: 初始版本，实现基本继电器控制功能
 */

#include "relay.h"

//=============================================================================
// 1. 私有定义和静态变量 (Private Definitions & Static Variables)
//=============================================================================

/**
 * @brief 继电器配置数组
 * @details 存储所有继电器的硬件配置信息，按通道顺序排列
 * @note 该数组为静态变量，仅在本模块内使用
 */
static RelayConfig_t relayConfigs[RELAY_CHANNEL_COUNT] = {
    {GPIOB, GPIO_PIN_4,  RELAY_STATE_OFF},  // 继电器1 - PB4
    {GPIOB, GPIO_PIN_3,  RELAY_STATE_OFF},  // 继电器2 - PB3  
    {GPIOA, GPIO_PIN_15, RELAY_STATE_OFF},  // 继电器3 - PA15
    {GPIOA, GPIO_PIN_12, RELAY_STATE_OFF},  // 继电器4 - PA12
    {GPIOA, GPIO_PIN_11, RELAY_STATE_OFF}   // 继电器5 - PA11
};

//=============================================================================
// 2. 私有函数声明 (Private Function Prototypes) 
//=============================================================================

/**
 * @brief 验证继电器通道参数的有效性
 * @param channel 继电器通道
 * @return bool 参数有效性
 * @retval true 通道参数有效
 * @retval false 通道参数无效
 */
static bool isValidChannel(RelayChannel_e channel);

/**
 * @brief 设置GPIO引脚输出状态
 * @param config 继电器配置指针
 * @param state 目标状态
 */
static void setGpioState(RelayConfig_t* config, RelayState_e state);

//=============================================================================
// 3. 公共API函数实现 (Public API Implementations)
//=============================================================================

HAL_StatusTypeDef relayInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // 配置所有继电器引脚为推挽输出
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
    // 初始化每个继电器引脚
    for (int i = 0; i < RELAY_CHANNEL_COUNT; i++)
    {
        GPIO_InitStruct.Pin = relayConfigs[i].pin;
        HAL_GPIO_Init(relayConfigs[i].port, &GPIO_InitStruct);
        
        // 设置为默认关闭状态
        setGpioState(&relayConfigs[i], RELAY_STATE_OFF);
        relayConfigs[i].currentState = RELAY_STATE_OFF;
    }
    
    return HAL_OK;
}

HAL_StatusTypeDef relaySetState(RelayChannel_e channel, RelayState_e state)
{
    if (!isValidChannel(channel))
    {
        return HAL_ERROR;
    }
    
    RelayConfig_t* config = &relayConfigs[channel];
    setGpioState(config, state);
    config->currentState = state;
    
    return HAL_OK;
}

RelayState_e relayGetState(RelayChannel_e channel)
{
    if (!isValidChannel(channel))
    {
        return RELAY_STATE_OFF;
    }
    
    return relayConfigs[channel].currentState;
}

HAL_StatusTypeDef relayToggle(RelayChannel_e channel)
{
    if (!isValidChannel(channel))
    {
        return HAL_ERROR;
    }
    
    RelayState_e currentState = relayConfigs[channel].currentState;
    RelayState_e newState = (currentState == RELAY_STATE_OFF) ? 
                           RELAY_STATE_ON : RELAY_STATE_OFF;
    
    return relaySetState(channel, newState);
}

HAL_StatusTypeDef relaySetAllStates(uint8_t stateMask)
{
    HAL_StatusTypeDef status = HAL_OK;
    
    for (int i = 0; i < RELAY_CHANNEL_COUNT; i++)
    {
        RelayState_e state = (stateMask & (1 << i)) ? 
                            RELAY_STATE_ON : RELAY_STATE_OFF;
        
        if (relaySetState((RelayChannel_e)i, state) != HAL_OK)
        {
            status = HAL_ERROR;
        }
    }
    
    return status;
}

uint8_t relayGetAllStates(void)
{
    uint8_t stateMask = 0;
    
    for (int i = 0; i < RELAY_CHANNEL_COUNT; i++)
    {
        if (relayConfigs[i].currentState == RELAY_STATE_ON)
        {
            stateMask |= (1 << i);
        }
    }
    
    return stateMask;
}

HAL_StatusTypeDef relayTurnOffAll(void)
{
    return relaySetAllStates(0x00);
}

//=============================================================================
// 4. 私有函数实现 (Private Function Implementations)
//=============================================================================

static bool isValidChannel(RelayChannel_e channel)
{
    return (channel >= RELAY_CHANNEL_FIRST && channel < RELAY_CHANNEL_COUNT);
}

static void setGpioState(RelayConfig_t* config, RelayState_e state)
{
    GPIO_PinState pinState = (state == RELAY_STATE_ON) ? 
                            GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(config->port, config->pin, pinState);
}
