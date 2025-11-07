/**
 * @file rs485_test.c
 * @brief RS485通信测试程序
 * @details
 * 专门用于测试RS485收发功能
 * 先测试接收，收到数据后自动回复
 * 
 * 硬件连接：
 * - USART1(PA9/PA10) + PA8(485使能) - RS485通道1
 * - USART2(PA2/PA3) + PA4(485使能) - RS485通道2
 * 
 * RS485使能控制：
 * - 高电平：发送模式
 * - 低电平：接收模式
 */

#include "main.h"
#include "relay.h"
#include <string.h>
#include <stdio.h>

// 全局变量
UART_HandleTypeDef huart1;  // PA9/PA10 + PA8(485EN)
UART_HandleTypeDef huart2;  // PA2/PA3 + PA4(485EN)
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;

// 接收缓冲区
static uint8_t uart1_rx_buffer[256];
static uint8_t uart2_rx_buffer[256];
static volatile uint16_t uart1_rx_len = 0;
static volatile uint16_t uart2_rx_len = 0;
static volatile uint8_t uart1_rx_flag = 0;
static volatile uint8_t uart2_rx_flag = 0;

// 统计信息
static uint32_t uart1_rx_count = 0;
static uint32_t uart2_rx_count = 0;
static uint32_t uart1_tx_count = 0;
static uint32_t uart2_tx_count = 0;

// 函数声明
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void RS485_SetMode(uint8_t uart_num, uint8_t tx_mode);
static void ProcessUart1Data(void);
static void ProcessUart2Data(void);

int main(void)
{
    // 系统初始化
    HAL_Init();
    SystemClock_Config();
    
    // 初始化外设
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    
    // 初始化继电器（用作指示）
    relayInit();
    
    // 启动指示：继电器依次点亮
    for (int i = 0; i < 5; i++) {
        relaySetState((RelayChannel_e)i, RELAY_STATE_ON);
        HAL_Delay(100);
    }
    HAL_Delay(500);
    relayTurnOffAll();
    
    // 设置RS485为接收模式（重要！）
    RS485_SetMode(1, 0);  // UART1接收
    RS485_SetMode(2, 0);  // UART2接收
    HAL_Delay(10);
    
    // 发送测试信息（测试发送功能）
    char test_msg[] = "\r\n=== RS485 Test Started ===\r\n";
    
    // 测试UART1发送
    RS485_SetMode(1, 1);  // 切换到发送模式
    HAL_Delay(5);
    HAL_UART_Transmit(&huart1, (uint8_t*)test_msg, strlen(test_msg), 100);
    HAL_Delay(10);
    RS485_SetMode(1, 0);  // 切回接收模式
    
    // 测试UART2发送  
    RS485_SetMode(2, 1);  // 切换到发送模式
    HAL_Delay(5);
    HAL_UART_Transmit(&huart2, (uint8_t*)test_msg, strlen(test_msg), 100);
    HAL_Delay(10);
    RS485_SetMode(2, 0);  // 切回接收模式
    
    // 启用IDLE中断
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
    
    // 启动DMA接收
    HAL_UART_Receive_DMA(&huart1, uart1_rx_buffer, sizeof(uart1_rx_buffer));
    HAL_UART_Receive_DMA(&huart2, uart2_rx_buffer, sizeof(uart2_rx_buffer));
    
    // 运行指示：继电器1常亮
    relaySetState(RELAY_CHANNEL_FIRST, RELAY_STATE_ON);
    
    // 主循环
    uint32_t last_report = HAL_GetTick();
    
    while (1) {
        // 处理UART1接收的数据
        if (uart1_rx_flag) {
            ProcessUart1Data();
            uart1_rx_flag = 0;
        }
        
        // 处理UART2接收的数据
        if (uart2_rx_flag) {
            ProcessUart2Data();
            uart2_rx_flag = 0;
        }
        
        // 每5秒发送一次状态报告
        if (HAL_GetTick() - last_report > 5000) {
            last_report = HAL_GetTick();
            
            char report[128];
            sprintf(report, "\r\n[Status] UART1: RX=%lu TX=%lu | UART2: RX=%lu TX=%lu\r\n",
                    uart1_rx_count, uart1_tx_count, uart2_rx_count, uart2_tx_count);
            
            // 通过UART1发送状态
            RS485_SetMode(1, 1);
            HAL_Delay(5);
            HAL_UART_Transmit(&huart1, (uint8_t*)report, strlen(report), 100);
            HAL_Delay(10);
            RS485_SetMode(1, 0);
            
            // 心跳指示
            relayToggle(RELAY_CHANNEL_FIFTH);
        }
        
        HAL_Delay(1);
    }
}

/**
 * @brief 设置RS485模式
 * @param uart_num 串口号(1或2)
 * @param tx_mode 1=发送模式, 0=接收模式
 */
static void RS485_SetMode(uint8_t uart_num, uint8_t tx_mode)
{
    if (uart_num == 1) {
        // UART1使用PA8控制
        if (tx_mode) {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);   // 发送
            // 继电器2亮表示UART1发送
            relaySetState(RELAY_CHANNEL_SECOND, RELAY_STATE_ON);
        } else {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET); // 接收
            relaySetState(RELAY_CHANNEL_SECOND, RELAY_STATE_OFF);
        }
    } else if (uart_num == 2) {
        // UART2使用PA4控制
        if (tx_mode) {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);   // 发送
            // 继电器3亮表示UART2发送
            relaySetState(RELAY_CHANNEL_THIRD, RELAY_STATE_ON);
        } else {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET); // 接收
            relaySetState(RELAY_CHANNEL_THIRD, RELAY_STATE_OFF);
        }
    }
}

/**
 * @brief 处理UART1接收的数据
 */
static void ProcessUart1Data(void)
{
    if (uart1_rx_len > 0) {
        uart1_rx_count++;
        
        // 继电器4闪烁表示UART1收到数据
        relaySetState(RELAY_CHANNEL_FOURTH, RELAY_STATE_ON);
        HAL_Delay(50);
        relaySetState(RELAY_CHANNEL_FOURTH, RELAY_STATE_OFF);
        
        // 准备回复消息
        char reply[512];
        sprintf(reply, "\r\n[UART1 Echo] Received %d bytes:\r\n", uart1_rx_len);
        
        // 发送回复
        RS485_SetMode(1, 1);  // 切换到发送模式
        HAL_Delay(5);
        
        // 发送提示信息
        HAL_UART_Transmit(&huart1, (uint8_t*)reply, strlen(reply), 100);
        
        // 发送原始数据
        HAL_UART_Transmit(&huart1, uart1_rx_buffer, uart1_rx_len, 100);
        
        // 发送换行
        char newline[] = "\r\n";
        HAL_UART_Transmit(&huart1, (uint8_t*)newline, 2, 100);
        
        HAL_Delay(10);
        RS485_SetMode(1, 0);  // 切回接收模式
        
        uart1_tx_count++;
        
        // 重新启动DMA接收
        HAL_UART_Receive_DMA(&huart1, uart1_rx_buffer, sizeof(uart1_rx_buffer));
    }
}

/**
 * @brief 处理UART2接收的数据
 */
static void ProcessUart2Data(void)
{
    if (uart2_rx_len > 0) {
        uart2_rx_count++;
        
        // 准备回复消息
        char reply[512];
        sprintf(reply, "\r\n[UART2 Echo] Received %d bytes:\r\n", uart2_rx_len);
        
        // 发送回复
        RS485_SetMode(2, 1);  // 切换到发送模式
        HAL_Delay(5);
        
        HAL_UART_Transmit(&huart2, (uint8_t*)reply, strlen(reply), 100);
        HAL_UART_Transmit(&huart2, uart2_rx_buffer, uart2_rx_len, 100);
        
        char newline[] = "\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t*)newline, 2, 100);
        
        HAL_Delay(10);
        RS485_SetMode(2, 0);  // 切回接收模式
        
        uart2_tx_count++;
        
        // 重新启动DMA接收
        HAL_UART_Receive_DMA(&huart2, uart2_rx_buffer, sizeof(uart2_rx_buffer));
    }
}

// 系统时钟配置
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

// GPIO初始化
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // RS485控制引脚初始化（默认为低电平=接收模式）
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_8, GPIO_PIN_RESET);
    
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;  // 下拉确保默认接收
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

// DMA初始化
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    
    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 1, 1);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 1, 2);
    HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 1, 3);
    HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
}

// USART1初始化 (9600 baud)
static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // PA9 TX
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // PA10 RX
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 9600;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
    
    // DMA配置
    hdma_usart1_rx.Instance = DMA1_Channel5;
    hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode = DMA_NORMAL;
    hdma_usart1_rx.Init.Priority = DMA_PRIORITY_LOW;
    HAL_DMA_Init(&hdma_usart1_rx);
    __HAL_LINKDMA(&huart1, hdmarx, hdma_usart1_rx);
    
    hdma_usart1_tx.Instance = DMA1_Channel4;
    hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_tx.Init.Mode = DMA_NORMAL;
    hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
    HAL_DMA_Init(&hdma_usart1_tx);
    __HAL_LINKDMA(&huart1, hdmatx, hdma_usart1_tx);
    
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

// USART2初始化 (9600 baud)
static void MX_USART2_UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // PA2 TX
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // PA3 RX
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 9600;  // 改为9600以适配RS485
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
    
    // DMA配置
    hdma_usart2_rx.Instance = DMA1_Channel6;
    hdma_usart2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart2_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart2_rx.Init.Mode = DMA_NORMAL;
    hdma_usart2_rx.Init.Priority = DMA_PRIORITY_LOW;
    HAL_DMA_Init(&hdma_usart2_rx);
    __HAL_LINKDMA(&huart2, hdmarx, hdma_usart2_rx);
    
    hdma_usart2_tx.Instance = DMA1_Channel7;
    hdma_usart2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart2_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart2_tx.Init.Mode = DMA_NORMAL;
    hdma_usart2_tx.Init.Priority = DMA_PRIORITY_LOW;
    HAL_DMA_Init(&hdma_usart2_tx);
    __HAL_LINKDMA(&huart2, hdmatx, hdma_usart2_tx);
    
    HAL_NVIC_SetPriority(USART2_IRQn, 0, 1);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

// UART1中断处理（处理IDLE中断）
void USART1_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE)) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);
        
        // 停止DMA
        HAL_UART_DMAStop(&huart1);
        
        // 计算接收长度
        uart1_rx_len = sizeof(uart1_rx_buffer) - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
        
        // 设置标志
        if (uart1_rx_len > 0) {
            uart1_rx_flag = 1;
        }
    }
    
    HAL_UART_IRQHandler(&huart1);
}

// UART2中断处理（处理IDLE中断）
void USART2_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE)) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart2);
        
        // 停止DMA
        HAL_UART_DMAStop(&huart2);
        
        // 计算接收长度
        uart2_rx_len = sizeof(uart2_rx_buffer) - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
        
        // 设置标志
        if (uart2_rx_len > 0) {
            uart2_rx_flag = 1;
        }
    }
    
    HAL_UART_IRQHandler(&huart2);
}

// DMA中断处理
void DMA1_Channel4_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart1_tx); }
void DMA1_Channel5_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart1_rx); }
void DMA1_Channel6_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart2_rx); }
void DMA1_Channel7_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart2_tx); }

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        relaySetAllStates(0x1F);
        HAL_Delay(100);
        relayTurnOffAll();
        HAL_Delay(100);
    }
}

