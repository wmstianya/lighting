/**
 * @file relay.h
 * @brief 继电器输出控制驱动程序头文件
 * @details
 * 该模块实现5路继电器输出控制，用于照明系统的开关控制。
 * 支持独立控制每路继电器的开关状态，提供标准化的API接口。
 * 
 * @author Lighting Ultra Team
 * @date 2025-01-20
 * @version 1.0.0
 * 
 * @note 继电器引脚分配：
 *       - 继电器1: PB4
 *       - 继电器2: PB3  
 *       - 继电器3: PA15
 *       - 继电器4: PA12
 *       - 继电器5: PA11
 */

#ifndef RELAY_H
#define RELAY_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

//=============================================================================
// 1. 继电器定义和枚举 (Relay Definitions & Enums)
//=============================================================================

/**
 * @brief 继电器通道枚举
 * @note 枚举值从0开始，方便数组索引操作
 */
typedef enum
{
    RELAY_CHANNEL_FIRST = 0,    /**< 继电器1 - PB4 */
    RELAY_CHANNEL_SECOND,       /**< 继电器2 - PB3 */
    RELAY_CHANNEL_THIRD,        /**< 继电器3 - PA15 */
    RELAY_CHANNEL_FOURTH,       /**< 继电器4 - PA12 */
    RELAY_CHANNEL_FIFTH,        /**< 继电器5 - PA11 */
    RELAY_CHANNEL_COUNT         /**< 继电器总数量 */
} RelayChannel_e;

/**
 * @brief 继电器状态枚举
 */
typedef enum
{
    RELAY_STATE_OFF = 0,        /**< 继电器关闭状态 */
    RELAY_STATE_ON = 1          /**< 继电器开启状态 */
} RelayState_e;

/**
 * @brief 继电器配置结构体
 * @details 包含单个继电器的硬件配置信息
 */
typedef struct
{
    GPIO_TypeDef* port;         /**< GPIO端口 */
    uint16_t pin;               /**< GPIO引脚 */
    RelayState_e currentState;  /**< 当前状态 */
} RelayConfig_t;

//=============================================================================
// 2. 公共API函数声明 (Public API Function Prototypes)
//=============================================================================

/**
 * @brief 初始化继电器驱动模块
 * @details 配置所有继电器引脚为输出模式，并设置为默认关闭状态
 * @param None
 * @return HAL_StatusTypeDef HAL状态码
 * @retval HAL_OK 初始化成功
 * @retval HAL_ERROR 初始化失败
 */
HAL_StatusTypeDef relayInit(void);

/**
 * @brief 设置指定继电器的开关状态
 * @param channel 继电器通道 (@ref RelayChannel_e)
 * @param state 目标状态 (@ref RelayState_e)
 * @return HAL_StatusTypeDef HAL状态码
 * @retval HAL_OK 设置成功
 * @retval HAL_ERROR 参数错误或设置失败
 */
HAL_StatusTypeDef relaySetState(RelayChannel_e channel, RelayState_e state);

/**
 * @brief 获取指定继电器的当前状态
 * @param channel 继电器通道 (@ref RelayChannel_e)
 * @return RelayState_e 继电器当前状态
 * @retval RELAY_STATE_OFF 继电器关闭
 * @retval RELAY_STATE_ON 继电器开启
 * @note 如果通道参数无效，返回RELAY_STATE_OFF
 */
RelayState_e relayGetState(RelayChannel_e channel);

/**
 * @brief 切换指定继电器的状态
 * @details 如果继电器当前为开启状态则关闭，反之则开启
 * @param channel 继电器通道 (@ref RelayChannel_e)
 * @return HAL_StatusTypeDef HAL状态码
 * @retval HAL_OK 切换成功
 * @retval HAL_ERROR 参数错误或切换失败
 */
HAL_StatusTypeDef relayToggle(RelayChannel_e channel);

/**
 * @brief 设置所有继电器的状态
 * @param stateMask 状态掩码，bit0对应继电器1，bit1对应继电器2，以此类推
 * @return HAL_StatusTypeDef HAL状态码
 * @retval HAL_OK 设置成功
 * @retval HAL_ERROR 设置失败
 * @note 例如：stateMask=0x05 表示继电器1和继电器3开启，其他关闭
 */
HAL_StatusTypeDef relaySetAllStates(uint8_t stateMask);

/**
 * @brief 获取所有继电器的状态掩码
 * @return uint8_t 状态掩码，bit0对应继电器1，bit1对应继电器2，以此类推
 */
uint8_t relayGetAllStates(void);

/**
 * @brief 关闭所有继电器
 * @return HAL_StatusTypeDef HAL状态码
 * @retval HAL_OK 关闭成功
 */
HAL_StatusTypeDef relayTurnOffAll(void);

#endif // RELAY_H
