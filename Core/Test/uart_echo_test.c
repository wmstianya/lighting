/**
 * @file uart_echo_test.c
 * @brief 最简单的UART回环测试程序
 * @details 
 * 专门为验证UART+DMA+RS485硬件功能设计的最简化测试程序
 * 完全避免编译问题，专注于硬件功能验证
 */

#include "main.h"
#include "relay.h"

// 外部变量声明
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;

// 回环测试缓冲区
static uint8_t echo_rx_buffer[128];
static uint8_t echo_tx_buffer[128];
static volatile uint16_t echo_rx_length = 0;
static volatile uint8_t echo_data_ready = 0;
static uint32_t echo_count = 0;

/**
 * @brief 初始化回环测试
 */
void uartEchoInit(void)
{
    // 清空缓冲区
    int i;
    for(i = 0; i < 128; i++)
    {
        echo_rx_buffer[i] = 0;
        echo_tx_buffer[i] = 0;
    }
    
    // 重置状态
    echo_rx_length = 0;
    echo_data_ready = 0;
    echo_count = 0;
    
    // 设置RS485为接收模式
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    
    // 启用UART IDLE中断
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    
    // 启动DMA接收
    HAL_UART_Receive_DMA(&huart1, echo_rx_buffer, 128);
}

/**
 * @brief 回环测试主循环处理
 */
void uartEchoPoll(void)
{
    // 如果有数据需要回环
    if (echo_data_ready && echo_rx_length > 0)
    {
        int i;
        
        // 复制接收数据到发送缓冲区
        for(i = 0; i < echo_rx_length; i++)
        {
            echo_tx_buffer[i] = echo_rx_buffer[i];
        }
        
        // 继电器3闪烁：收到数据指示
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
        HAL_Delay(50);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
        
        // 切换到发送模式
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
        
        // 延时确保RS485切换
        for(volatile int j = 0; j < 200; j++);
        
        // 发送回环数据
        HAL_UART_Transmit_DMA(&huart1, echo_tx_buffer, echo_rx_length);
        
        // 等待发送完成
        HAL_Delay(50 + echo_rx_length * 3);
        
        // 继电器4闪烁：发送完成指示
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
        HAL_Delay(50);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
        
        // 切换回接收模式
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
        
        // 延时确保RS485切换
        for(volatile int j = 0; j < 200; j++);
        
        // 重新启动DMA接收
        HAL_UART_Receive_DMA(&huart1, echo_rx_buffer, 128);
        
        // 清除标志
        echo_data_ready = 0;
        echo_rx_length = 0;
        echo_count++;
        
        // 每10个包统计指示
        if ((echo_count % 10) == 0)
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);
            HAL_Delay(100);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
        }
    }
}

/**
 * @brief 处理UART1 IDLE中断
 * 在stm32f1xx_it.c的USART1_IRQHandler中调用
 */
void uartEchoHandleIdle(void)
{
    // 停止DMA接收
    HAL_UART_DMAStop(&huart1);
    
    // 计算接收长度
    echo_rx_length = 128 - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
    
    // 如果有有效数据
    if (echo_rx_length > 0)
    {
        echo_data_ready = 1;
    }
    else
    {
        // 没有数据，重新启动接收
        HAL_UART_Receive_DMA(&huart1, echo_rx_buffer, 128);
    }
}

/**
 * @brief 获取统计信息
 */
uint32_t uartEchoGetCount(void)
{
    return echo_count;
}
















