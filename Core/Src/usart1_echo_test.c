/**
 * @file usart1_echo_test.c
 * @brief USART1 串口回环测试（DMA+IDLE 快照式）
 */

#include "stm32f1xx_hal.h"
#include <string.h>
#include "usart1_echo_test.h"

/* 外部句柄 */
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef  hdma_usart1_rx;
extern DMA_HandleTypeDef  hdma_usart1_tx;

/* 缓冲与状态 */
#define ECHO1_BUFFER_SIZE 256
static uint8_t echo1RxBuffer[ECHO1_BUFFER_SIZE];
static uint8_t echo1TxBuffer[ECHO1_BUFFER_SIZE];
static volatile uint16_t echo1RxCount = 0;
static volatile uint8_t  echo1DataReady = 0;
static volatile uint32_t diag1IdleCount = 0;
static volatile uint32_t diag1ProcessCount = 0;
static volatile uint32_t diag1TxCpltCount = 0;

/* LED 与 RS485 DE 控制（沿用项目定义：PB1 作为 LED，USART1 DE 在 PA8） */
#define ECHO1_LED_PORT GPIOB
#define ECHO1_LED_PIN  GPIO_PIN_1
#ifdef MB_USART1_RS485_DE_Pin
#define ECHO1_RS485_PORT MB_USART1_RS485_DE_GPIO_Port
#define ECHO1_RS485_PIN  MB_USART1_RS485_DE_Pin
#else
#define ECHO1_RS485_PORT GPIOA
#define ECHO1_RS485_PIN  GPIO_PIN_8
#endif

void usart1EchoTestInit(void)
{
    memset(echo1RxBuffer, 0, sizeof(echo1RxBuffer));
    memset(echo1TxBuffer, 0, sizeof(echo1TxBuffer));
    echo1RxCount = 0;
    echo1DataReady = 0;
    diag1IdleCount = 0;
    diag1ProcessCount = 0;
    diag1TxCpltCount = 0;

    /* 置 RS485 为接收 */
    HAL_GPIO_WritePin(ECHO1_RS485_PORT, ECHO1_RS485_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);

    /* 清 IDLE 并启动 DMA 接收（与USART2一致，显式启用IDLE） */
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    HAL_UART_Receive_DMA(&huart1, echo1RxBuffer, ECHO1_BUFFER_SIZE);
}

void usart1EchoHandleIdle(void)
{
    diag1IdleCount++;
    
    /* LED快闪表示IDLE触发 */
    HAL_GPIO_WritePin(ECHO1_LED_PORT, ECHO1_LED_PIN, GPIO_PIN_RESET);

    /* 停止DMA */
    HAL_UART_DMAStop(&huart1);
    
    /* 清 ORE：读 SR 再读 DR */
    volatile uint32_t sr = huart1.Instance->SR; (void)sr;
    volatile uint32_t dr = huart1.Instance->DR; (void)dr;

    /* 计算接收字节数 */
    echo1RxCount = ECHO1_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
    
    if (echo1RxCount > 0) {
        echo1DataReady = 1;
        /* 不在这里重启DMA，等待主循环处理完数据后重启 */
    } else {
        /* 如果没有接收到数据，立即重启DMA继续接收 */
        HAL_UART_Receive_DMA(&huart1, echo1RxBuffer, ECHO1_BUFFER_SIZE);
    }

    HAL_GPIO_WritePin(ECHO1_LED_PORT, ECHO1_LED_PIN, GPIO_PIN_SET);
}

void usart1EchoProcess(void)
{
    diag1ProcessCount++;

    /* LED慢闪表示主循环运行（对齐USART2实现） */
    static uint32_t lastBlink = 0;
    if (HAL_GetTick() - lastBlink > 1000) {
        lastBlink = HAL_GetTick();
        HAL_GPIO_TogglePin(ECHO1_LED_PORT, ECHO1_LED_PIN);
    }
    
    if (!echo1DataReady) {
        return;
    }

    /* 保存接收长度 */
    uint16_t rxLen = echo1RxCount;
    
    /* 立即复制数据到发送缓冲区（避免被覆盖） */
    memcpy(echo1TxBuffer, echo1RxBuffer, rxLen);



    /* 清除接收标志和缓冲区 */
    echo1DataReady = 0;
    echo1RxCount = 0;
    memset(echo1RxBuffer, 0, sizeof(echo1RxBuffer));

    /* 切换RS485到发送模式 */
    HAL_GPIO_WritePin(ECHO1_RS485_PORT, ECHO1_RS485_PIN, GPIO_PIN_SET);
    
    /* 小延时确保RS485切换稳定 */
    for(volatile uint32_t i = 0; i < 100; i++); // 约10us延时
    
    /* DMA异步发送 */
    HAL_UART_Transmit_DMA(&huart1, echo1TxBuffer, rxLen);
    
    /* 注意：DMA接收将在发送完成回调中重启 */
}

void usart1EchoTxCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1) {
        diag1TxCpltCount++;
        HAL_GPIO_WritePin(ECHO1_LED_PORT, ECHO1_LED_PIN, GPIO_PIN_RESET);

        /* 等待 TC，最长约 2ms */
        uint32_t t0 = HAL_GetTick();
        while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET) {
            if ((HAL_GetTick() - t0) > 2U) break;
        }

        /* 切换RS485回接收模式 */
        HAL_GPIO_WritePin(ECHO1_RS485_PORT, ECHO1_RS485_PIN, GPIO_PIN_RESET);
        
        /* 重要：立即重启DMA接收，准备接收下一帧数据 */
        HAL_UART_Receive_DMA(&huart1, echo1RxBuffer, ECHO1_BUFFER_SIZE);
        
        HAL_GPIO_WritePin(ECHO1_LED_PORT, ECHO1_LED_PIN, GPIO_PIN_SET);
    }
}

void usart1EchoGetDiagnostics(uint32_t *idle, uint32_t *process, uint32_t *txCplt)
{
    if (idle)    *idle = diag1IdleCount;
    if (process) *process = diag1ProcessCount;
    if (txCplt)  *txCplt = diag1TxCpltCount;
}

void usart1EchoTestRun(void)
{
    usart1EchoTestInit();
    
    /* LED闪3次表示启动 */
    for (uint8_t i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(ECHO1_LED_PORT, ECHO1_LED_PIN, GPIO_PIN_RESET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(ECHO1_LED_PORT, ECHO1_LED_PIN, GPIO_PIN_SET);
        HAL_Delay(200);
    }

    /* 确保开始时RS485处于接收模式（与USART2一致） */
    HAL_GPIO_WritePin(ECHO1_RS485_PORT, ECHO1_RS485_PIN, GPIO_PIN_RESET);

    while (1) {
        usart1EchoProcess();
        HAL_Delay(1);
    }
}


