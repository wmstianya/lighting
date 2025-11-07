/**
 * @file usart2_simple_test.c
 * @brief USART2最简化测试 - 用于快速验证
 * @details 不使用TIM3，直接在中断中切换RS485
 */

#include "stm32f1xx_hal.h"
#include <string.h>

/* 外部变量引用 */
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_rx;

/* 简单缓冲区 */
#define SIMPLE_BUFFER_SIZE 256
static uint8_t simpleRxBuf[SIMPLE_BUFFER_SIZE];
static uint8_t simpleTxBuf[SIMPLE_BUFFER_SIZE];
static volatile uint16_t simpleRxCount = 0;
static volatile uint8_t simpleDataReady = 0;

/* 诊断计数器 */
static volatile uint32_t idleCount = 0;
static volatile uint32_t processCount = 0;
static volatile uint32_t txCount = 0;

/* RS485控制 */
#define SIMPLE_RS485_PORT GPIOA
#define SIMPLE_RS485_PIN  GPIO_PIN_4

/* LED诊断（PB1） */
#define SIMPLE_LED_PORT GPIOB
#define SIMPLE_LED_PIN  GPIO_PIN_1

/**
 * @brief 初始化简单测试
 */
void usart2SimpleTestInit(void)
{
    memset(simpleRxBuf, 0, SIMPLE_BUFFER_SIZE);
    memset(simpleTxBuf, 0, SIMPLE_BUFFER_SIZE);
    simpleRxCount = 0;
    simpleDataReady = 0;
    idleCount = 0;
    processCount = 0;
    txCount = 0;
    
    /* RS485接收模式 */
    HAL_GPIO_WritePin(SIMPLE_RS485_PORT, SIMPLE_RS485_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    
    /* LED闪3次表示启动 */
    for (uint8_t i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(SIMPLE_LED_PORT, SIMPLE_LED_PIN, GPIO_PIN_RESET);
        HAL_Delay(100);
        HAL_GPIO_WritePin(SIMPLE_LED_PORT, SIMPLE_LED_PIN, GPIO_PIN_SET);
        HAL_Delay(100);
    }
    
    /* 启用IDLE中断 */
    __HAL_UART_CLEAR_IDLEFLAG(&huart2);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
    
    /* 启动DMA接收 */
    HAL_StatusTypeDef status = HAL_UART_Receive_DMA(&huart2, simpleRxBuf, SIMPLE_BUFFER_SIZE);
    
    /* LED闪烁显示DMA启动状态 */
    if (status == HAL_OK) {
        HAL_GPIO_WritePin(SIMPLE_LED_PORT, SIMPLE_LED_PIN, GPIO_PIN_RESET);
        HAL_Delay(500);
        HAL_GPIO_WritePin(SIMPLE_LED_PORT, SIMPLE_LED_PIN, GPIO_PIN_SET);
    }
}

/**
 * @brief IDLE中断处理
 */
void usart2SimpleHandleIdle(void)
{
    /* LED快闪表示IDLE中断触发 */
    HAL_GPIO_WritePin(SIMPLE_LED_PORT, SIMPLE_LED_PIN, GPIO_PIN_RESET);
    
    idleCount++;
    
    /* 停止DMA */
    HAL_UART_DMAStop(&huart2);
    
    /* 计算接收字节数 */
    simpleRxCount = SIMPLE_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
    
    if (simpleRxCount > 0) {
        simpleDataReady = 1;
    }
    
    HAL_GPIO_WritePin(SIMPLE_LED_PORT, SIMPLE_LED_PIN, GPIO_PIN_SET);
}

/**
 * @brief 发送完成回调 - 直接在中断中延迟切换
 */
void usart2SimpleTxCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) {
        /* 短暂延迟确保数据发出 */
        for(volatile uint32_t i = 0; i < 200; i++);  // 约200us @ 72MHz
        
        /* 切换RS485回接收 */
        HAL_GPIO_WritePin(SIMPLE_RS485_PORT, SIMPLE_RS485_PIN, GPIO_PIN_RESET);
    }
}

/**
 * @brief 主循环处理
 */
void usart2SimpleProcess(void)
{
    if (!simpleDataReady) {
        return;
    }
    
    /* 保存长度 */
    uint16_t len = simpleRxCount;
    
    /* 复制数据 */
    memcpy(simpleTxBuf, simpleRxBuf, len);
    
    /* 清除标志 */
    simpleDataReady = 0;
    simpleRxCount = 0;
    memset(simpleRxBuf, 0, SIMPLE_BUFFER_SIZE);
    
    /* 重启接收 */
    HAL_UART_Receive_DMA(&huart2, simpleRxBuf, SIMPLE_BUFFER_SIZE);
    
    /* 切换RS485到发送 */
    HAL_GPIO_WritePin(SIMPLE_RS485_PORT, SIMPLE_RS485_PIN, GPIO_PIN_SET);
    HAL_Delay(1);
    
    /* DMA发送 */
    HAL_UART_Transmit_DMA(&huart2, simpleTxBuf, len);
}

/**
 * @brief 运行简单测试
 */
void usart2SimpleTestRun(void)
{
    usart2SimpleTestInit();
    
    while (1) {
        usart2SimpleProcess();
        HAL_Delay(10);
    }
}

