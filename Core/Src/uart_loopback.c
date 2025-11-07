/**
 * @file uart_loopback.c
 * @brief UART回环测试模块实现
 * @details
 * 实现简单的串口回环功能：接收任意数据后立即返回相同数据。
 * 用于验证UART+DMA+RS485硬件功能，绕过复杂的协议处理。
 * 
 * @author Lighting Ultra Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#include "uart_loopback.h"
#include <string.h>

//=============================================================================
// 私有函数声明 (Private Function Prototypes)
//=============================================================================

/**
 * @brief 设置RS485为发送模式
 * @param pInstance 回环测试实例指针
 */
static void setRS485TxMode(LoopbackInstance_t *pInstance);

/**
 * @brief 设置RS485为接收模式
 * @param pInstance 回环测试实例指针
 */
static void setRS485RxMode(LoopbackInstance_t *pInstance);

/**
 * @brief 启动DMA接收
 * @param pInstance 回环测试实例指针
 * @return HAL_StatusTypeDef 启动结果
 */
static HAL_StatusTypeDef startDMAReception(LoopbackInstance_t *pInstance);

//=============================================================================
// 公共API函数实现 (Public API Implementations)
//=============================================================================

HAL_StatusTypeDef loopbackInit(LoopbackInstance_t *pInstance,
                              UART_HandleTypeDef* huart,
                              DMA_HandleTypeDef* hdma_rx,
                              DMA_HandleTypeDef* hdma_tx,
                              GPIO_TypeDef* de_re_port,
                              uint16_t de_re_pin)
{
    if (pInstance == NULL || huart == NULL)
    {
        return HAL_ERROR;
    }
    
    // 保存硬件配置
    pInstance->huart = huart;
    pInstance->hdma_rx = hdma_rx;
    pInstance->hdma_tx = hdma_tx;
    pInstance->de_re_port = de_re_port;
    pInstance->de_re_pin = de_re_pin;
    
    // 初始化状态
    pInstance->eState = LOOPBACK_STATE_IDLE;
    pInstance->u16RxLen = 0;
    pInstance->u32LastRxTime = 0;
    
    // 清空缓冲区
    memset(pInstance->au8RxBuffer, 0, LOOPBACK_BUFFER_SIZE);
    memset(pInstance->au8TxBuffer, 0, LOOPBACK_BUFFER_SIZE);
    
    // 重置统计信息
    pInstance->u32TotalPackets = 0;
    pInstance->u32ErrorCount = 0;
    
    // 设置RS485为接收模式
    setRS485RxMode(pInstance);
    
    return HAL_OK;
}

HAL_StatusTypeDef loopbackStart(LoopbackInstance_t *pInstance)
{
    if (pInstance == NULL)
    {
        return HAL_ERROR;
    }
    
    // 启用UART IDLE中断
    __HAL_UART_ENABLE_IT(pInstance->huart, UART_IT_IDLE);
    
    // 启动DMA接收
    return startDMAReception(pInstance);
}

HAL_StatusTypeDef loopbackStop(LoopbackInstance_t *pInstance)
{
    if (pInstance == NULL)
    {
        return HAL_ERROR;
    }
    
    // 停止DMA
    HAL_UART_DMAStop(pInstance->huart);
    
    // 禁用UART IDLE中断
    __HAL_UART_DISABLE_IT(pInstance->huart, UART_IT_IDLE);
    
    // 设置为接收模式
    setRS485RxMode(pInstance);
    
    pInstance->eState = LOOPBACK_STATE_IDLE;
    
    return HAL_OK;
}

void loopbackPoll(LoopbackInstance_t *pInstance)
{
    if (pInstance == NULL)
    {
        return;
    }
    
    switch (pInstance->eState)
    {
        case LOOPBACK_STATE_IDLE:
            // 空闲状态，等待IDLE中断
            break;
            
        case LOOPBACK_STATE_DATA_READY:
            // 有数据需要回环发送
            if (pInstance->u16RxLen > 0)
            {
                // 复制接收数据到发送缓冲区
                memcpy(pInstance->au8TxBuffer, pInstance->au8RxBuffer, pInstance->u16RxLen);
                
                // 切换到发送模式
                setRS485TxMode(pInstance);
                
                // 添加小延时确保RS485切换稳定
                for(volatile int i = 0; i < 20; i++);
                
                // 启动DMA发送
                if (HAL_UART_Transmit_DMA(pInstance->huart, pInstance->au8TxBuffer, pInstance->u16RxLen) == HAL_OK)
                {
                    pInstance->eState = LOOPBACK_STATE_TRANSMITTING;
                    pInstance->u32TotalPackets++;
                }
                else
                {
                    pInstance->u32ErrorCount++;
                    pInstance->eState = LOOPBACK_STATE_IDLE;
                    setRS485RxMode(pInstance);
                    startDMAReception(pInstance);
                }
            }
            else
            {
                // 没有有效数据，回到空闲状态
                pInstance->eState = LOOPBACK_STATE_IDLE;
                startDMAReception(pInstance);
            }
            break;
            
        case LOOPBACK_STATE_RECEIVING:
        case LOOPBACK_STATE_TRANSMITTING:
            // 这些是瞬时状态，等待中断处理
            break;
            
        default:
            // 异常状态，复位
            pInstance->eState = LOOPBACK_STATE_IDLE;
            pInstance->u32ErrorCount++;
            setRS485RxMode(pInstance);
            startDMAReception(pInstance);
            break;
    }
}

HAL_StatusTypeDef loopbackGetStats(LoopbackInstance_t *pInstance, 
                                  uint32_t* totalPackets, 
                                  uint32_t* errorCount)
{
    if (pInstance == NULL || totalPackets == NULL || errorCount == NULL)
    {
        return HAL_ERROR;
    }
    
    *totalPackets = pInstance->u32TotalPackets;
    *errorCount = pInstance->u32ErrorCount;
    
    return HAL_OK;
}

void loopbackResetStats(LoopbackInstance_t *pInstance)
{
    if (pInstance != NULL)
    {
        pInstance->u32TotalPackets = 0;
        pInstance->u32ErrorCount = 0;
    }
}

//=============================================================================
// 中断处理函数 (Interrupt Handler Functions)
//=============================================================================

/**
 * @brief UART IDLE中断处理函数
 * @details 在UART中断处理函数中调用
 * @param pInstance 回环测试实例指针
 */
void loopbackHandleIdleInterrupt(LoopbackInstance_t *pInstance)
{
    if (pInstance == NULL || pInstance->huart == NULL)
    {
        return;
    }
    
    // 停止DMA接收
    HAL_UART_DMAStop(pInstance->huart);
    
    // 计算接收到的数据长度
    pInstance->u16RxLen = LOOPBACK_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(pInstance->hdma_rx);
    
    // 记录接收时间
    pInstance->u32LastRxTime = HAL_GetTick();
    
    // 如果有有效数据，切换到数据准备状态
    if (pInstance->u16RxLen > 0)
    {
        pInstance->eState = LOOPBACK_STATE_DATA_READY;
    }
    else
    {
        // 没有数据，重新启动接收
        pInstance->eState = LOOPBACK_STATE_IDLE;
        startDMAReception(pInstance);
    }
}

/**
 * @brief UART发送完成中断处理函数
 * @details 在UART发送完成回调中调用
 * @param pInstance 回环测试实例指针
 */
void loopbackHandleTxComplete(LoopbackInstance_t *pInstance)
{
    if (pInstance == NULL)
    {
        return;
    }
    
    // 发送完成，切换回接收模式
    setRS485RxMode(pInstance);
    
    // 添加小延时确保RS485切换稳定
    for(volatile int i = 0; i < 20; i++);
    
    // 重新启动DMA接收
    startDMAReception(pInstance);
    
    // 回到空闲状态
    pInstance->eState = LOOPBACK_STATE_IDLE;
}

//=============================================================================
// 私有函数实现 (Private Function Implementations)
//=============================================================================

static void setRS485TxMode(LoopbackInstance_t *pInstance)
{
    if (pInstance != NULL && pInstance->de_re_port != NULL)
    {
        HAL_GPIO_WritePin(pInstance->de_re_port, pInstance->de_re_pin, GPIO_PIN_SET);
    }
}

static void setRS485RxMode(LoopbackInstance_t *pInstance)
{
    if (pInstance != NULL && pInstance->de_re_port != NULL)
    {
        HAL_GPIO_WritePin(pInstance->de_re_port, pInstance->de_re_pin, GPIO_PIN_RESET);
    }
}

static HAL_StatusTypeDef startDMAReception(LoopbackInstance_t *pInstance)
{
    if (pInstance == NULL || pInstance->huart == NULL)
    {
        return HAL_ERROR;
    }
    
    pInstance->eState = LOOPBACK_STATE_RECEIVING;
    return HAL_UART_Receive_DMA(pInstance->huart, pInstance->au8RxBuffer, LOOPBACK_BUFFER_SIZE);
}

