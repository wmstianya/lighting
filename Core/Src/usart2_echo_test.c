/**
 * @file usart2_echo_test.c
 * @brief USART2串口回环测试（接收→打印→发送）
 * @details 用于验证串口2 (PA2/PA3) DMA收发功能
 * @author Lighting Ultra Team
 * @date 2025-11-05
 */

#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_tim.h"
#include <string.h>
#include <stdio.h>

/* 外部变量引用 */
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;

/* TIM3用于RS485延迟切换（非阻塞） - 暂时不使用 */
// TIM_HandleTypeDef htim3;

/* 测试缓冲区 */
#define ECHO_BUFFER_SIZE 256
static uint8_t echoRxBuffer[ECHO_BUFFER_SIZE];
static uint8_t echoTxBuffer[ECHO_BUFFER_SIZE];
static volatile uint16_t echoRxCount = 0;
static volatile uint8_t echoDataReady = 0;
static volatile uint8_t echoTxComplete = 0;

/* 诊断计数器 */
static volatile uint32_t diagIdleCount = 0;
static volatile uint32_t diagProcessCount = 0;
static volatile uint32_t diagTxCpltCount = 0;
static volatile uint32_t diagTim3Count = 0;
static volatile uint32_t diagDmaTxCount = 0;

/* LED诊断（PB1） */
#define DIAG_LED_PORT GPIOB
#define DIAG_LED_PIN  GPIO_PIN_1

/* RS485控制引脚（PA4） */
#define ECHO_RS485_PORT GPIOA
#define ECHO_RS485_PIN  GPIO_PIN_4

/**
 * @brief 初始化串口2回环测试
 */
void usart2EchoTestInit(void)
{
    /* 清空缓冲区 */
    memset(echoRxBuffer, 0, ECHO_BUFFER_SIZE);
    memset(echoTxBuffer, 0, ECHO_BUFFER_SIZE);
    echoRxCount = 0;
    echoDataReady = 0;
    echoTxComplete = 0;
    
    /* 诊断计数器清零 */
    diagIdleCount = 0;
    diagProcessCount = 0;
    diagTxCpltCount = 0;
    diagTim3Count = 0;
    diagDmaTxCount = 0;
    
    /* RS485设置为接收模式 */
    HAL_GPIO_WritePin(ECHO_RS485_PORT, ECHO_RS485_PIN, GPIO_PIN_RESET);
    
    /* 短暂延时确保RS485稳定 */
    HAL_Delay(10);
    
    /* 清除IDLE标志并启用IDLE中断 */
    __HAL_UART_CLEAR_IDLEFLAG(&huart2);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
    
    /* 启动DMA接收 */
    HAL_UART_Receive_DMA(&huart2, echoRxBuffer, ECHO_BUFFER_SIZE);
}

/**
 * @brief IDLE中断回调（在stm32f1xx_it.c中调用）
 */
void usart2EchoHandleIdle(void)
{
    diagIdleCount++;
    
    /* LED快闪表示IDLE触发 */
    HAL_GPIO_WritePin(DIAG_LED_PORT, DIAG_LED_PIN, GPIO_PIN_RESET);
    
    /* 停止DMA */
    HAL_UART_DMAStop(&huart2);
    
    /* 清除可能的ORE：读SR再读DR（F1系列需如此操作） */
    volatile uint32_t sr = huart2.Instance->SR; (void)sr;
    volatile uint32_t dr = huart2.Instance->DR; (void)dr;
    
    /* 计算接收字节数 */
    echoRxCount = ECHO_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
    
    if (echoRxCount > 0) {
        echoDataReady = 1;
    }
    
    HAL_GPIO_WritePin(DIAG_LED_PORT, DIAG_LED_PIN, GPIO_PIN_SET);
}

/**
 * @brief 处理回环测试（纯数据回环，无调试信息）
 * @note DMA异步发送，TIM3硬件延迟切换RS485
 */
void usart2EchoProcess(void)
{
    diagProcessCount++;
    
    /* LED慢闪表示主循环运行 */
    static uint32_t lastBlink = 0;
    if (HAL_GetTick() - lastBlink > 5000) {
        lastBlink = HAL_GetTick();
        HAL_GPIO_WritePin(DIAG_LED_PORT, DIAG_LED_PIN, GPIO_PIN_RESET);
        HAL_Delay(50);
        HAL_GPIO_WritePin(DIAG_LED_PORT, DIAG_LED_PIN, GPIO_PIN_SET);
    }
    
    if (!echoDataReady) {
        return;
    }
    
    /* 保存接收长度 */
    uint16_t rxLen = echoRxCount;
    
    /* 立即复制数据到发送缓冲区（避免被覆盖） */
    memcpy(echoTxBuffer, echoRxBuffer, rxLen);
    
    /* 清除接收标志，准备下一帧（提前清除，提高响应速度） */
    echoDataReady = 0;
    echoRxCount = 0;
    memset(echoRxBuffer, 0, ECHO_BUFFER_SIZE);
    
    /* 重启DMA接收（立即启动，避免丢失后续数据） */
    HAL_UART_Receive_DMA(&huart2, echoRxBuffer, ECHO_BUFFER_SIZE);
    
    /* 切换RS485到发送模式 */
    HAL_GPIO_WritePin(ECHO_RS485_PORT, ECHO_RS485_PIN, GPIO_PIN_SET);
    HAL_Delay(1); // RS485切换延时
    
    /* DMA异步发送（不阻塞CPU） */
    echoTxComplete = 0;
    diagDmaTxCount++;
    HAL_UART_Transmit_DMA(&huart2, echoTxBuffer, rxLen);
    
    /* LED闪烁3次表示发送启动 */
    for (uint8_t i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(DIAG_LED_PORT, DIAG_LED_PIN, GPIO_PIN_RESET);
        for(volatile int j = 0; j < 5000; j++);
        HAL_GPIO_WritePin(DIAG_LED_PORT, DIAG_LED_PIN, GPIO_PIN_SET);
        for(volatile int j = 0; j < 5000; j++);
    }
    
    /* 发送完成后会自动调用 HAL_UART_TxCpltCallback */
    /* TxCallback会启动TIM3定时器，200us后自动切换RS485 */
    /* CPU完全释放，可以处理其他任务 */
}

/**
 * @brief 处理回环测试（带调试信息版本）
 */
void usart2EchoProcessDebug(void)
{
    if (!echoDataReady) {
        return;
    }
    
    /* 保存接收长度 */
    uint16_t rxLen = echoRxCount;
    
    /* 立即复制数据到发送缓冲区（避免被覆盖） */
    memcpy(echoTxBuffer, echoRxBuffer, rxLen);
    
    /* 清除标志（先清除，避免重复触发） */
    echoDataReady = 0;
    echoRxCount = 0;
    
    /* 重启DMA接收（立即启动，避免丢失数据） */
    HAL_UART_Receive_DMA(&huart2, echoRxBuffer, ECHO_BUFFER_SIZE);
    
    /* 切换RS485到发送模式 */
    HAL_GPIO_WritePin(ECHO_RS485_PORT, ECHO_RS485_PIN, GPIO_PIN_SET);
    HAL_Delay(2);
    
    /* 打印接收数据（HEX格式） */
    char debugMsg[128];
    snprintf(debugMsg, sizeof(debugMsg), "\r\n[RX %d] ", rxLen);
    HAL_UART_Transmit(&huart2, (uint8_t*)debugMsg, strlen(debugMsg), 100);
    
    for (uint16_t i = 0; i < rxLen && i < 32; i++) {
        snprintf(debugMsg, sizeof(debugMsg), "%02X ", echoTxBuffer[i]);
        HAL_UART_Transmit(&huart2, (uint8_t*)debugMsg, strlen(debugMsg), 100);
    }
    HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, 100);
    
    /* 发送回环数据 */
    HAL_UART_Transmit(&huart2, (uint8_t*)"[ECHO] ", 7, 100);
    HAL_UART_Transmit(&huart2, echoTxBuffer, rxLen, 1000);
    
    /* 等待最后字节发出 */
    HAL_Delay(5);
    
    /* 切换RS485回接收模式 */
    HAL_GPIO_WritePin(ECHO_RS485_PORT, ECHO_RS485_PIN, GPIO_PIN_RESET);
    
    HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n[OK]\r\n", 7, 100);
}

/**
 * @brief DMA发送完成回调（在中断中自动调用）
 * @note 此函数在stm32f1xx_it.c的HAL_UART_TxCpltCallback中被调用
 * @note 使用阻塞式延迟（简单可靠）
 */
void usart2EchoTxCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) {
        diagTxCpltCount++;
        
        /* LED长亮表示TxCallback触发 */
        HAL_GPIO_WritePin(DIAG_LED_PORT, DIAG_LED_PIN, GPIO_PIN_RESET);
        
        /* 等待发送移位寄存器完全空 (TC=1)，设置小超时防卡死 */
        uint32_t startTick = HAL_GetTick();
        while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) == RESET) {
            if ((HAL_GetTick() - startTick) > 2U) { /* ~2ms 超时 */
                break;
            }
        }
        
        /* 切换RS485回接收模式（DE=LOW） */
        HAL_GPIO_WritePin(ECHO_RS485_PORT, ECHO_RS485_PIN, GPIO_PIN_RESET);
        
        /* LED熄灭表示切换完成 */
        HAL_GPIO_WritePin(DIAG_LED_PORT, DIAG_LED_PIN, GPIO_PIN_SET);
    }
}

/**
 * @brief 获取诊断计数器
 */
void usart2EchoGetDiagnostics(uint32_t *idle, uint32_t *process, uint32_t *txCplt, uint32_t *tim3, uint32_t *dmaTx)
{
    if (idle) *idle = diagIdleCount;
    if (process) *process = diagProcessCount;
    if (txCplt) *txCplt = diagTxCpltCount;
    if (tim3) *tim3 = diagTim3Count;
    if (dmaTx) *dmaTx = diagDmaTxCount;
}

/**
 * @brief 运行完整回环测试
 */
void usart2EchoTestRun(void)
{
    usart2EchoTestInit();
    
    /* LED闪3次表示启动 */
    for (uint8_t i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(DIAG_LED_PORT, DIAG_LED_PIN, GPIO_PIN_RESET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(DIAG_LED_PORT, DIAG_LED_PIN, GPIO_PIN_SET);
        HAL_Delay(200);
    }
    
    while (1) {
        usart2EchoProcess();        // 使用纯净版本（推荐）
        // usart2EchoProcessDebug();  // 使用调试版本（可选）
        HAL_Delay(1);
    }
}

