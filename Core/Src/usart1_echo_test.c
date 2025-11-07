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

    /* 清 IDLE 并启动 DMA 接收 */
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    HAL_UART_Receive_DMA(&huart1, echo1RxBuffer, ECHO1_BUFFER_SIZE);
}

void usart1EchoHandleIdle(void)
{
    diag1IdleCount++;
    HAL_GPIO_WritePin(ECHO1_LED_PORT, ECHO1_LED_PIN, GPIO_PIN_RESET);

    HAL_UART_DMAStop(&huart1);
    /* 清 ORE：读 SR 再读 DR */
    volatile uint32_t sr = huart1.Instance->SR; (void)sr;
    volatile uint32_t dr = huart1.Instance->DR; (void)dr;

    echo1RxCount = ECHO1_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
    if (echo1RxCount > 0) {
        echo1DataReady = 1;
    }

    HAL_GPIO_WritePin(ECHO1_LED_PORT, ECHO1_LED_PIN, GPIO_PIN_SET);
}

void usart1EchoProcess(void)
{
    diag1ProcessCount++;
    if (!echo1DataReady) return;

    uint16_t rxLen = echo1RxCount;
    memcpy(echo1TxBuffer, echo1RxBuffer, rxLen);

    echo1DataReady = 0;
    echo1RxCount = 0;
    memset(echo1RxBuffer, 0, sizeof(echo1RxBuffer));

    /* 先重启接收，降低空窗 */
    HAL_UART_Receive_DMA(&huart1, echo1RxBuffer, ECHO1_BUFFER_SIZE);

    /* 发送回环 */
    HAL_GPIO_WritePin(ECHO1_RS485_PORT, ECHO1_RS485_PIN, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_UART_Transmit_DMA(&huart1, echo1TxBuffer, rxLen);
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

        HAL_GPIO_WritePin(ECHO1_RS485_PORT, ECHO1_RS485_PIN, GPIO_PIN_RESET);
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
    for (uint8_t i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(ECHO1_LED_PORT, ECHO1_LED_PIN, GPIO_PIN_RESET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(ECHO1_LED_PORT, ECHO1_LED_PIN, GPIO_PIN_SET);
        HAL_Delay(200);
    }

    while (1) {
        usart1EchoProcess();
        HAL_Delay(1);
    }
}


