/**
 * @file dip_switch.h
 * @brief 4 位拨码开关读取模块（Modbus 从站地址）
 * @details
 *   硬件：SW1 拨码开关 4 位
 *     ADDR1 = PB7  (LQFP48 pin 43, SW1.8) → bit0 最低位
 *     ADDR2 = PB8  (LQFP48 pin 45, SW1.7) → bit1
 *     ADDR3 = PB9  (LQFP48 pin 46, SW1.6) → bit2
 *     ADDR4 = PC13 (LQFP48 pin 2,  SW1.5) → bit3 最高位
 *
 *   逻辑：拨码 ON = 接 GND = GPIO 读 0 = 该位为 1（低电平有效，内部上拉）
 *   地址范围：0~15，地址 0 视为无效回退到 1
 *
 * @author Lighting Ultra Team
 * @date 2026-04-24
 */

#ifndef __DIP_SWITCH_H
#define __DIP_SWITCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32f1xx_hal.h"

/* ==================== 引脚定义 ==================== */
#define DIP_ADDR1_PORT      GPIOB
#define DIP_ADDR1_PIN       GPIO_PIN_7

#define DIP_ADDR2_PORT      GPIOB
#define DIP_ADDR2_PIN       GPIO_PIN_8

#define DIP_ADDR3_PORT      GPIOB
#define DIP_ADDR3_PIN       GPIO_PIN_9

#define DIP_ADDR4_PORT      GPIOC
#define DIP_ADDR4_PIN       GPIO_PIN_13

#define DIP_ADDR_FALLBACK   1u    /**< 拨码全 OFF(=0) 时回退到此地址 */

/* ==================== API ==================== */

/**
 * @brief 初始化拨码开关 GPIO（输入 + 内部上拉）
 */
void dipSwitchInit(void);

/**
 * @brief 读取 4 位拨码原始值
 * @return 0~15，低电平有效取反后的值
 */
uint8_t dipSwitchReadRaw(void);

/**
 * @brief 获取 Modbus 从站地址
 * @return 1~15，地址 0 自动回退到 DIP_ADDR_FALLBACK
 */
uint8_t dipSwitchGetSlaveAddr(void);

#ifdef __cplusplus
}
#endif

#endif /* __DIP_SWITCH_H */
