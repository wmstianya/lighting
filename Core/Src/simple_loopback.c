/**
 * @file simple_loopback.c
 * @brief 简单串口回环测试实现
 * @details
 * 提供最简单的串口回环测试，用于验证UART+DMA+RS485基础硬件功能
 * 
 * @author Lighting Ultra Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#include "main.h"
#include <string.h>
#include <stdbool.h>

// 外部变量声明
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;

// 回环测试缓冲区
static uint8_t loopback_rx_buffer[256];
static uint8_t loopback_tx_buffer[256];
static volatile uint16_t loopback_rx_len = 0;
static volatile bool loopback_data_ready = false;

// 回环测试统计
static uint32_t loopback_packet_count = 0;
static uint32_t loopback_error_count = 0;

//=============================================================================
// 简单回环测试函数 (Simple Loopback Test Functions)
//=============================================================================

/**
 * @brief 初始化简单回环测试
 */
void simpleLoopbackInit(void)
{
    // 清空缓冲区
    memset(loopback_rx_buffer, 0, sizeof(loopback_rx_buffer));
    memset(loopback_tx_buffer, 0, sizeof(loopback_tx_buffer));
    
    // 重置状态
    loopback_rx_len = 0;
    loopback_data_ready = false;
    loopback_packet_count = 0;
    loopback_error_count = 0;
    
    // 设置RS485为接收模式
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);  // 485EN1
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);  // 485EN2
    
    // 启用UART IDLE中断
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
    
    // 启动DMA接收
    HAL_UART_Receive_DMA(&huart1, loopback_rx_buffer, sizeof(loopback_rx_buffer));
}

/**
 * @brief 简单回环测试主循环
 */
void simpleLoopbackPoll(void)
{
    // 如果有数据需要回环
    if (loopback_data_ready && loopback_rx_len > 0)
    {
        // 复制数据到发送缓冲区
        memcpy(loopback_tx_buffer, loopback_rx_buffer, loopback_rx_len);
        
        // 设置RS485为发送模式
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);  // 485EN1发送
        
        // 小延时确保RS485切换
        for(volatile int i = 0; i < 50; i++);
        
        // 发送数据
        if (HAL_UART_Transmit_DMA(&huart1, loopback_tx_buffer, loopback_rx_len) == HAL_OK)
        {
            loopback_packet_count++;
            
            // 等待发送完成（简单延时方式）
            HAL_Delay(10 + loopback_rx_len / 10); // 根据数据长度调整延时
            
            // 切换回接收模式
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);  // 485EN1接收
            
            // 小延时确保RS485切换
            for(volatile int i = 0; i < 50; i++);
            
            // 重新启动DMA接收
            HAL_UART_Receive_DMA(&huart1, loopback_rx_buffer, sizeof(loopback_rx_buffer));
        }
        else
        {
            loopback_error_count++;
            
            // 发送失败，切换回接收模式
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
            HAL_UART_Receive_DMA(&huart1, loopback_rx_buffer, sizeof(loopback_rx_buffer));
        }
        
        // 清除数据准备标志
        loopback_data_ready = false;
        loopback_rx_len = 0;
        
        // 闪烁继电器4表示回环完成
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);   // 继电器4开启
        HAL_Delay(50);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET); // 继电器4关闭
    }
}

/**
 * @brief 处理UART1 IDLE中断（在中断函数中调用）
 */
void simpleLoopbackHandleIdle1(void)
{
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) != RESET)
    {
        // 清除IDLE标志
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);
        
        // 停止DMA
        HAL_UART_DMAStop(&huart1);
        
        // 计算接收长度
        loopback_rx_len = sizeof(loopback_rx_buffer) - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
        
        // 如果有有效数据，设置数据准备标志
        if (loopback_rx_len > 0)
        {
            loopback_data_ready = true;
            
            // 闪烁继电器3表示收到数据
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);   // 继电器3开启
            for(volatile int i = 0; i < 1000; i++);               // 短暂延时
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET); // 继电器3关闭
        }
        else
        {
            // 没有数据，重新启动接收
            HAL_UART_Receive_DMA(&huart1, loopback_rx_buffer, sizeof(loopback_rx_buffer));
        }
    }
}

/**
 * @brief 获取回环测试统计信息
 */
void simpleLoopbackGetStats(uint32_t* packets, uint32_t* errors)
{
    if (packets != NULL) *packets = loopback_packet_count;
    if (errors != NULL) *errors = loopback_error_count;
}

/**
 * @brief 重置统计信息
 */
void simpleLoopbackResetStats(void)
{
    loopback_packet_count = 0;
    loopback_error_count = 0;
}
