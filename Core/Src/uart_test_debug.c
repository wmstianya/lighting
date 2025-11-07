/**
 * @file uart_test_debug.c
 * @brief 简化的串口测试调试程序
 * @details
 * 用于排查串口通信问题的最简化测试程序
 * 
 * 使用方法：
 * 1. 用此文件替换main.c编译下载
 * 2. 连接PA2/PA3到串口工具，设置115200波特率
 * 3. 观察是否收到启动信息和按键回显
 */

#include "main.h"
#include "relay.h"
#include <string.h>
#include <stdio.h>

// 全局变量
UART_HandleTypeDef huart1;  // PA9/PA10 - 9600
UART_HandleTypeDef huart2;  // PA2/PA3 - 115200
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;

// 测试变量
static uint8_t rxData;
static uint8_t rxBuffer[256];
static uint16_t rxIndex = 0;

// 函数声明
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);

int main(void)
{
    // 系统初始化
    HAL_Init();
    SystemClock_Config();
    
    // 初始化GPIO和外设
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    
    // 初始化继电器
    relayInit();
    
    // 启动指示：继电器1闪烁3次
    for (int i = 0; i < 3; i++) {
        relaySetState(RELAY_CHANNEL_FIRST, RELAY_STATE_ON);
        HAL_Delay(200);
        relaySetState(RELAY_CHANNEL_FIRST, RELAY_STATE_OFF);
        HAL_Delay(200);
    }
    
    // 发送启动信息到串口2（调试串口）
    char startMsg[] = "\r\n========================================\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)startMsg, strlen(startMsg), 100);
    
    char msg1[] = "串口测试调试程序 V1.0\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)msg1, strlen(msg1), 100);
    
    char msg2[] = "串口2(PA2/PA3) - 115200 波特率\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)msg2, strlen(msg2), 100);
    
    char msg3[] = "请输入任意字符，系统会回显并控制继电器\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)msg3, strlen(msg3), 100);
    
    char msg4[] = "输入 '1' - 继电器1开关\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)msg4, strlen(msg4), 100);
    
    char msg5[] = "输入 '2' - 继电器2开关\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)msg5, strlen(msg5), 100);
    
    char msg6[] = "输入 'a' - 所有继电器开\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)msg6, strlen(msg6), 100);
    
    char msg7[] = "输入 'o' - 所有继电器关\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)msg7, strlen(msg7), 100);
    
    HAL_UART_Transmit(&huart2, (uint8_t*)startMsg, strlen(startMsg), 100);
    
    // 保持继电器2常亮表示系统运行
    relaySetState(RELAY_CHANNEL_SECOND, RELAY_STATE_ON);
    
    // 启动串口2接收中断（单字节）
    HAL_UART_Receive_IT(&huart2, &rxData, 1);
    
    // 主循环
    while (1) {
        // 简单的延时和状态指示
        static uint32_t lastBlink = 0;
        static bool ledState = false;
        
        if (HAL_GetTick() - lastBlink > 1000) {
            lastBlink = HAL_GetTick();
            ledState = !ledState;
            
            // 继电器5作为心跳指示
            relaySetState(RELAY_CHANNEL_FIFTH, 
                         ledState ? RELAY_STATE_ON : RELAY_STATE_OFF);
        }
        
        HAL_Delay(10);
    }
}

/**
 * @brief UART接收完成回调（简化版）
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) {
        char response[64];
        
        // 回显接收到的字符
        sprintf(response, "\r\n收到: '%c' (0x%02X)\r\n", rxData, rxData);
        HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
        
        // 根据接收的字符执行动作
        switch (rxData) {
            case '1':
                relayToggle(RELAY_CHANNEL_FIRST);
                sprintf(response, "继电器1切换\r\n");
                break;
                
            case '2':
                relayToggle(RELAY_CHANNEL_SECOND);
                sprintf(response, "继电器2切换\r\n");
                break;
                
            case '3':
                relayToggle(RELAY_CHANNEL_THIRD);
                sprintf(response, "继电器3切换\r\n");
                break;
                
            case '4':
                relayToggle(RELAY_CHANNEL_FOURTH);
                sprintf(response, "继电器4切换\r\n");
                break;
                
            case 'a':
            case 'A':
                relaySetAllStates(0x1F);
                sprintf(response, "所有继电器开启\r\n");
                break;
                
            case 'o':
            case 'O':
                relayTurnOffAll();
                sprintf(response, "所有继电器关闭\r\n");
                // 重新点亮运行指示
                relaySetState(RELAY_CHANNEL_SECOND, RELAY_STATE_ON);
                break;
                
            case '\r':
            case '\n':
                sprintf(response, "");  // 忽略回车换行
                break;
                
            default:
                sprintf(response, "未知命令，请输入 1/2/3/4/a/o\r\n");
                break;
        }
        
        if (strlen(response) > 0) {
            HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
        }
        
        // 继续接收下一个字节
        HAL_UART_Receive_IT(&huart2, &rxData, 1);
    }
}

// 系统时钟配置（72MHz）
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
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

// GPIO初始化
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // RS485控制引脚
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_8, GPIO_PIN_RESET);
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

// USART1初始化 (PA9/PA10 - 9600)
static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // GPIO配置
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
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
    
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

// USART2初始化 (PA2/PA3 - 115200)
static void MX_USART2_UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // GPIO配置
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;  // 注意是115200！
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
    
    HAL_NVIC_SetPriority(USART2_IRQn, 0, 1);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

void Error_Handler(void)
{
    __disable_irq();
    // 错误指示：所有继电器快速闪烁
    while (1) {
        relaySetAllStates(0x1F);
        HAL_Delay(100);
        relayTurnOffAll();
        HAL_Delay(100);
    }
}

