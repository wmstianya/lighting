/**
 * @file beep.h
 * @brief 蜂鸣器驱动模块头文件
 * @details 无源蜂鸣器PWM驱动（TIM1_CH2N @ 2700Hz）
 * @author Lighting Ultra Team
 * @date 2025-11-11
 */

#ifndef BEEP_H
#define BEEP_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

/* ==================== 宏定义 ==================== */
#define BEEP_GPIO_PORT  GPIOB
#define BEEP_GPIO_PIN   GPIO_PIN_14
#define BEEP_FREQ_HZ    2700    /* 无源蜂鸣器谐振频率 */
#define BEEP_DUTY_CYCLE 50      /* 占空比 50% */

/* ==================== 数据结构 ==================== */
/**
 * @brief 蜂鸣器控制数据结构
 */
typedef struct {
    uint16_t beepTimeSet;     /**< 蜂鸣器时间设定（ms） */
    uint16_t beepTimeCount;   /**< 蜂鸣器时间计数（ms） */
    bool beepOnFlag;          /**< 蜂鸣器开启标志 */
} BeepData_t;

/* ==================== 全局变量 ==================== */
extern TIM_HandleTypeDef htimBeep;

/* ==================== API函数 ==================== */
/**
 * @brief 初始化蜂鸣器模块（配置TIM1_CH2N PWM）
 * @note 无源蜂鸣器需要2700Hz PWM方波驱动
 */
void beepInit(void);

/**
 * @brief 设置蜂鸣器鸣叫时间（非阻塞）
 * @param timeMs 鸣叫时长（毫秒）
 * @note 调用后蜂鸣器立即开启，经过指定时间后自动关闭
 */
void beepSetTime(uint16_t timeMs);

/**
 * @brief 蜂鸣器处理函数（需在1ms定时器中断或主循环中调用）
 * @note 负责自动关闭蜂鸣器
 */
void beepProcess(void);

/**
 * @brief 立即关闭蜂鸣器
 */
void beepOff(void);

/**
 * @brief 获取蜂鸣器状态
 * @return true: 蜂鸣器开启, false: 蜂鸣器关闭
 */
bool beepGetStatus(void);

#endif /* BEEP_H */

