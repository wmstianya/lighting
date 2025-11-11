/**
 * @file beep.c
 * @brief 蜂鸣器驱动模块实现（无源蜂鸣器PWM驱动）
 * @details 使用TIM1_CH2N输出2700Hz PWM驱动无源蜂鸣器
 * @author Lighting Ultra Team
 * @date 2025-11-11
 */

#include "beep.h"

/* ==================== 私有变量 ==================== */
static BeepData_t beepData = {0};
static uint32_t lastTickMs = 0;
TIM_HandleTypeDef htimBeep;  /* TIM1句柄，用于PWM输出 */

/* ==================== 私有函数 ==================== */
/**
 * @brief 初始化TIM1用于PWM输出
 * @note TIM1_CH2N映射到PB14
 */
static void beepInitTimer(void)
{
    /* 使能TIM1和GPIOB时钟 */
    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    
    /* 配置PB14为TIM1_CH2N复用推挽输出 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = BEEP_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BEEP_GPIO_PORT, &GPIO_InitStruct);
    
    /* 配置TIM1 PWM参数
     * 时钟源：APB2 = 72MHz
     * 预分频：PSC = 0（不分频）
     * 自动重装载值：ARR = 72000000 / 2700 - 1 = 26666
     * 比较值：CCR2 = ARR * 50% = 13333
     * 输出频率：72MHz / (1 * 26667) = 2700Hz
     */
    uint32_t arr = (72000000 / BEEP_FREQ_HZ) - 1;
    uint32_t ccr = arr * BEEP_DUTY_CYCLE / 100;
    
    htimBeep.Instance = TIM1;
    htimBeep.Init.Prescaler = 0;
    htimBeep.Init.CounterMode = TIM_COUNTERMODE_UP;
    htimBeep.Init.Period = arr;
    htimBeep.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htimBeep.Init.RepetitionCounter = 0;
    htimBeep.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    
    if (HAL_TIM_PWM_Init(&htimBeep) != HAL_OK) {
        Error_Handler();
    }
    
    /* 配置PWM通道2（互补输出CH2N） */
    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = ccr;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;  /* 互补通道极性 */
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    
    if (HAL_TIM_PWM_ConfigChannel(&htimBeep, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) {
        Error_Handler();
    }
    
    /* 配置刹车和死区（高级定时器必需） */
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
    sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
    sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime = 0;
    sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    
    if (HAL_TIMEx_ConfigBreakDeadTime(&htimBeep, &sBreakDeadTimeConfig) != HAL_OK) {
        Error_Handler();
    }
}

/* ==================== API函数实现 ==================== */
void beepInit(void)
{
    beepData.beepTimeSet = 0;
    beepData.beepTimeCount = 0;
    beepData.beepOnFlag = false;
    lastTickMs = HAL_GetTick();
    
    /* 初始化TIM1 PWM */
    beepInitTimer();
    
    /* 确保蜂鸣器初始为关闭状态 */
    HAL_TIMEx_PWMN_Stop(&htimBeep, TIM_CHANNEL_2);
}

void beepSetTime(uint16_t timeMs)
{
    if (timeMs == 0) {
        beepOff();
        return;
    }
    
    beepData.beepTimeSet = timeMs;
    beepData.beepTimeCount = 0;
    beepData.beepOnFlag = true;
    
    /* 重置时间戳，避免时间增量异常 */
    lastTickMs = HAL_GetTick();
    
    /* 启动PWM互补输出（CH2N） */
    HAL_TIMEx_PWMN_Start(&htimBeep, TIM_CHANNEL_2);
}

void beepProcess(void)
{
    if (!beepData.beepOnFlag) {
        return;
    }
    
    /* 计算时间增量（处理HAL_GetTick溢出） */
    uint32_t currentTick = HAL_GetTick();
    uint32_t deltaMs = currentTick - lastTickMs;
    lastTickMs = currentTick;
    
    /* 累加时间计数 */
    beepData.beepTimeCount += deltaMs;
    
    /* 时间到，关闭蜂鸣器 */
    if (beepData.beepTimeCount >= beepData.beepTimeSet) {
        beepOff();
    }
}

void beepOff(void)
{
    beepData.beepOnFlag = false;
    beepData.beepTimeCount = 0;
    
    /* 停止PWM互补输出 */
    HAL_TIMEx_PWMN_Stop(&htimBeep, TIM_CHANNEL_2);
}

bool beepGetStatus(void)
{
    return beepData.beepOnFlag;
}
