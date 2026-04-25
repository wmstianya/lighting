/**
 * @file dip_switch.c
 * @brief 4 位拨码开关读取模块实现
 * @author Lighting Ultra Team
 * @date 2026-04-24
 */

#include "dip_switch.h"

void dipSwitchInit(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = DIP_ADDR1_PIN | DIP_ADDR2_PIN | DIP_ADDR3_PIN;
    HAL_GPIO_Init(DIP_ADDR1_PORT, &gpio);   /* PB7 / PB8 / PB9 */

    gpio.Pin = DIP_ADDR4_PIN;
    HAL_GPIO_Init(DIP_ADDR4_PORT, &gpio);   /* PC13 */
}

uint8_t dipSwitchReadRaw(void)
{
    uint8_t addr = 0;

    /* 低电平有效：GPIO 读 0 → 该位为 1 */
    if (HAL_GPIO_ReadPin(DIP_ADDR1_PORT, DIP_ADDR1_PIN) == GPIO_PIN_RESET) {
        addr |= (1u << 0);
    }
    if (HAL_GPIO_ReadPin(DIP_ADDR2_PORT, DIP_ADDR2_PIN) == GPIO_PIN_RESET) {
        addr |= (1u << 1);
    }
    if (HAL_GPIO_ReadPin(DIP_ADDR3_PORT, DIP_ADDR3_PIN) == GPIO_PIN_RESET) {
        addr |= (1u << 2);
    }
    if (HAL_GPIO_ReadPin(DIP_ADDR4_PORT, DIP_ADDR4_PIN) == GPIO_PIN_RESET) {
        addr |= (1u << 3);
    }

    return addr;
}

uint8_t dipSwitchGetSlaveAddr(void)
{
    uint8_t raw = dipSwitchReadRaw();
    return (raw == 0) ? DIP_ADDR_FALLBACK : raw;
}
