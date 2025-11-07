/**
 * @file usart2_echo_test_debug.c
 * @brief USART2调试版本 - 带LED和串口状态打印
 * @details 用于逐步排查收发问题
 */

#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_tim.h"
#include <string.h>
#include <stdio.h>

/* 外部变量引用 */
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;

/* TIM3用于RS485延迟切换（非阻塞） */
TIM_HandleTypeDef htim3_debug;

/* 测试缓冲区 */
#define DEBUG_BUFFER_SIZE 256
static uint8_t debugRxBuffer[DEBUG_BUFFER_SIZE];
static uint8_t debugTxBuffer[DEBUG_BUFFER_SIZE];
static volatile uint16_t debugRxCount = 0;
static volatile uint8_t debugDataReady = 0;
static volatile uint8_t debugTxComplete = 0;
static volatile uint32_t idleIntCount = 0;
static volatile uint32_t processCount = 0;

/* LED控制（使用PB1） */
#define DEBUG_LED_PORT GPIOB
#define DEBUG_LED_PIN  GPIO_PIN_1

/* RS485控制引脚（PA4） */
#define DEBUG_RS485_PORT GPIOA
#define DEBUG_RS485_PIN  GPIO_PIN_4

/**
 * @brief LED闪烁指示
 */
static void ledBlink(uint8_t times)
{
    for (uint8_t i = 0; i < times; i++) {
        HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED_PIN, GPIO_PIN_RESET); // 点亮
        HAL_Delay(100);
        HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED_PIN, GPIO_PIN_SET);   // 熄灭
        HAL_Delay(100);
    }
}

/**
 * @brief 发送调试信息
 */
static void debugPrint(const char* msg)
{
    HAL_GPIO_WritePin(DEBUG_RS485_PORT, DEBUG_RS485_PIN, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 1000);
    HAL_Delay(1);
    HAL_GPIO_WritePin(DEBUG_RS485_PORT, DEBUG_RS485_PIN, GPIO_PIN_RESET);
}

/**
 * @brief 初始化TIM3用于RS485延迟切换（调试版本）
 */
static void usart2DebugTim3Init(void)
{
    /* 使能TIM3时钟 */
    __HAL_RCC_TIM3_CLK_ENABLE();
    
    /* 配置TIM3：单次触发模式，200us延迟 */
    htim3_debug.Instance = TIM3;
    htim3_debug.Init.Prescaler = 72 - 1;          // 72MHz / 72 = 1MHz (1us/tick)
    htim3_debug.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3_debug.Init.Period = 200 - 1;            // 200us = 0.2ms @ 115200bps
    htim3_debug.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3_debug.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    
    if (HAL_TIM_Base_Init(&htim3_debug) != HAL_OK) {
        while(1);  // 初始化失败
    }
    
    /* 配置为单次触发模式 */
    TIM3->CR1 |= TIM_CR1_OPM;  // One Pulse Mode
    
    /* 使能TIM3中断 */
    HAL_NVIC_SetPriority(TIM3_IRQn, 2, 0);  // 优先级2，比UART低
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
}

/**
 * @brief 初始化调试测试
 */
void usart2DebugTestInit(void)
{
    char msg[128];
    
    /* 清空缓冲区 */
    memset(debugRxBuffer, 0, DEBUG_BUFFER_SIZE);
    memset(debugTxBuffer, 0, DEBUG_BUFFER_SIZE);
    debugRxCount = 0;
    debugDataReady = 0;
    debugTxComplete = 0;
    idleIntCount = 0;
    processCount = 0;
    
    /* RS485设置为接收模式 */
    HAL_GPIO_WritePin(DEBUG_RS485_PORT, DEBUG_RS485_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    
    /* 初始化TIM3定时器（非阻塞延迟） */
    usart2DebugTim3Init();
    
    /* LED闪烁3次表示启动 */
    ledBlink(3);
    
    /* 打印启动信息 */
    debugPrint("\r\n=== USART2 Debug Test Started ===\r\n");
    
    snprintf(msg, sizeof(msg), "UART State: %d\r\n", HAL_UART_GetState(&huart2));
    debugPrint(msg);
    
    snprintf(msg, sizeof(msg), "DMA RX State: %d\r\n", HAL_DMA_GetState(&hdma_usart2_rx));
    debugPrint(msg);
    
    /* 清除IDLE标志 */
    __HAL_UART_CLEAR_IDLEFLAG(&huart2);
    
    /* 启用IDLE中断 */
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
    
    /* 启动DMA接收 */
    HAL_StatusTypeDef status = HAL_UART_Receive_DMA(&huart2, debugRxBuffer, DEBUG_BUFFER_SIZE);
    snprintf(msg, sizeof(msg), "DMA Start Status: %d\r\n", status);
    debugPrint(msg);
    
    snprintf(msg, sizeof(msg), "DMA Counter: %d\r\n", __HAL_DMA_GET_COUNTER(&hdma_usart2_rx));
    debugPrint(msg);
    
    debugPrint("Waiting for data...\r\n\r\n");
}

/**
 * @brief IDLE中断处理（调试版本）
 */
void usart2DebugHandleIdle(void)
{
    char msg[64];
    
    /* LED快闪表示中断触发 */
    HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED_PIN, GPIO_PIN_RESET);
    
    /* 停止DMA */
    HAL_UART_DMAStop(&huart2);
    
    /* 计算接收字节数 */
    uint16_t remaining = __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
    debugRxCount = DEBUG_BUFFER_SIZE - remaining;
    
    idleIntCount++;
    
    if (debugRxCount > 0) {
        debugDataReady = 1;
    }
    
    HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED_PIN, GPIO_PIN_SET);
}

/**
 * @brief 处理回环（调试版本）
 */
void usart2DebugProcess(void)
{
    char msg[256];
    processCount++;
    
    /* 每10秒打印一次心跳 */
    static uint32_t lastHeartbeat = 0;
    if (HAL_GetTick() - lastHeartbeat > 10000) {
        lastHeartbeat = HAL_GetTick();
        snprintf(msg, sizeof(msg), "[Heartbeat] Process: %lu, IDLE: %lu, Ready: %d\r\n",
                 processCount, idleIntCount, debugDataReady);
        debugPrint(msg);
        
        /* LED慢闪表示运行中 */
        ledBlink(1);
    }
    
    if (!debugDataReady) {
        return;
    }
    
    /* LED快闪3次表示数据到达 */
    ledBlink(3);
    
    /* 保存接收长度 */
    uint16_t rxLen = debugRxCount;
    
    /* 立即复制数据 */
    memcpy(debugTxBuffer, debugRxBuffer, rxLen);
    
    /* 清除标志 */
    debugDataReady = 0;
    debugRxCount = 0;
    
    /* 打印接收信息 */
    snprintf(msg, sizeof(msg), "\r\n[IDLE #%lu] Received %d bytes:\r\n", idleIntCount, rxLen);
    debugPrint(msg);
    
    /* 打印HEX */
    debugPrint("HEX: ");
    for (uint16_t i = 0; i < rxLen && i < 32; i++) {
        snprintf(msg, sizeof(msg), "%02X ", debugTxBuffer[i]);
        debugPrint(msg);
    }
    debugPrint("\r\n");
    
    /* 打印ASCII */
    debugPrint("ASCII: [");
    HAL_GPIO_WritePin(DEBUG_RS485_PORT, DEBUG_RS485_PIN, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_UART_Transmit(&huart2, debugTxBuffer, rxLen, 1000);
    HAL_Delay(1);
    HAL_GPIO_WritePin(DEBUG_RS485_PORT, DEBUG_RS485_PIN, GPIO_PIN_RESET);
    debugPrint("]\r\n");
    
    /* 重启DMA接收 */
    memset(debugRxBuffer, 0, DEBUG_BUFFER_SIZE);
    HAL_StatusTypeDef status = HAL_UART_Receive_DMA(&huart2, debugRxBuffer, DEBUG_BUFFER_SIZE);
    
    snprintf(msg, sizeof(msg), "DMA Restart Status: %d\r\n", status);
    debugPrint(msg);
    
    /* 切换RS485到发送模式 */
    HAL_GPIO_WritePin(DEBUG_RS485_PORT, DEBUG_RS485_PIN, GPIO_PIN_SET);
    HAL_Delay(1);
    
    /* DMA异步发送（不阻塞CPU） */
    debugTxComplete = 0;
    HAL_UART_Transmit_DMA(&huart2, debugTxBuffer, rxLen);
    
    /* 发送完成后会在回调中打印状态 */
}

/**
 * @brief DMA发送完成回调（调试版本 - TIM3非阻塞）
 */
void usart2DebugTxCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) {
        /* 启动TIM3定时器，200us后自动触发中断切换RS485 */
        /* 完全非阻塞，CPU立即释放 */
        __HAL_TIM_SET_COUNTER(&htim3_debug, 0);           // 重置计数器
        __HAL_TIM_ENABLE_IT(&htim3_debug, TIM_IT_UPDATE); // 使能更新中断
        __HAL_TIM_ENABLE(&htim3_debug);                   // 启动定时器
        
        /* 立即返回，不阻塞CPU */
    }
}

/**
 * @brief TIM3中断回调（调试版本）
 */
void usart2DebugTim3Callback(void)
{
    /* 切换RS485回接收模式 */
    HAL_GPIO_WritePin(DEBUG_RS485_PORT, DEBUG_RS485_PIN, GPIO_PIN_RESET);
    
    /* 设置发送完成标志 */
    debugTxComplete = 1;
    
    /* LED快闪表示发送完成 */
    HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED_PIN, GPIO_PIN_RESET);
    volatile uint32_t delay = 500;
    while(delay--);
    HAL_GPIO_WritePin(DEBUG_LED_PORT, DEBUG_LED_PIN, GPIO_PIN_SET);
    
    /* TIM3单次触发模式会自动停止 */
}

/**
 * @brief 运行调试测试
 */
void usart2DebugTestRun(void)
{
    usart2DebugTestInit();
    
    while (1) {
        usart2DebugProcess();
        HAL_Delay(100); // 100ms轮询
    }
}


