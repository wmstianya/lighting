/**
 * @file led.h
 * @brief LED指示灯驱动模块头文件
 * @details 提供4路LED指示灯控制，对应DO1-DO4状态指示
 * @author Lighting Ultra Team
 * @date 2025-11-11
 */

#ifndef LED_H
#define LED_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

/* ==================== LED通道定义 ==================== */
/**
 * @brief LED通道枚举
 */
typedef enum {
    LED_CHANNEL_1 = 0,   /**< LED1 - PB1  - DO1指示 */
    LED_CHANNEL_2,       /**< LED2 - PB15 - DO2指示 */
    LED_CHANNEL_3,       /**< LED3 - PB5  - DO3指示 */
    LED_CHANNEL_4,       /**< LED4 - PB6  - DO4指示 */
    LED_CHANNEL_COUNT    /**< LED总数 */
} LedChannel_e;

/**
 * @brief LED状态枚举
 */
typedef enum {
    LED_STATE_OFF = 0,   /**< LED关闭（灭） */
    LED_STATE_ON = 1     /**< LED开启（亮） */
} LedState_e;

/* ==================== LED配置 ==================== */
/**
 * @brief LED配置结构体
 */
typedef struct {
    GPIO_TypeDef* port;  /**< GPIO端口 */
    uint16_t pin;        /**< GPIO引脚 */
    LedState_e state;    /**< 当前状态 */
} LedConfig_t;

/* ==================== API函数 ==================== */
/**
 * @brief 初始化LED模块
 * @details 配置所有LED引脚为输出模式，低电平点亮（默认关闭）
 * @return HAL_StatusTypeDef HAL状态码
 */
HAL_StatusTypeDef ledInit(void);

/**
 * @brief 设置指定LED的状态
 * @param channel LED通道 (@ref LedChannel_e)
 * @param state 目标状态 (@ref LedState_e)
 * @return HAL_StatusTypeDef HAL状态码
 * @retval HAL_OK 设置成功
 * @retval HAL_ERROR 参数错误
 */
HAL_StatusTypeDef ledSetState(LedChannel_e channel, LedState_e state);

/**
 * @brief 获取指定LED的当前状态
 * @param channel LED通道 (@ref LedChannel_e)
 * @return LedState_e LED当前状态
 * @retval LED_STATE_OFF LED关闭
 * @retval LED_STATE_ON LED开启
 */
LedState_e ledGetState(LedChannel_e channel);

/**
 * @brief 翻转指定LED的状态
 * @param channel LED通道 (@ref LedChannel_e)
 * @return HAL_StatusTypeDef HAL状态码
 */
HAL_StatusTypeDef ledToggle(LedChannel_e channel);

/**
 * @brief 设置所有LED的状态
 * @param stateMask 状态掩码，bit0对应LED1，bit1对应LED2，以此类推
 * @return HAL_StatusTypeDef HAL状态码
 * @note 例如：stateMask=0x05 表示LED1和LED3开启，LED2和LED4关闭
 */
HAL_StatusTypeDef ledSetAllStates(uint8_t stateMask);

/**
 * @brief 关闭所有LED
 * @return HAL_StatusTypeDef HAL状态码
 */
HAL_StatusTypeDef ledTurnOffAll(void);

#endif /* LED_H */

