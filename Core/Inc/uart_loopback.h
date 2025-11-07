/**
 * @file uart_loopback.h
 * @brief UART回环测试模块头文件
 * @details
 * 提供串口回环测试功能，用于验证UART+DMA+RS485硬件功能。
 * 接收任意数据后立即返回，绕过Modbus协议处理。
 * 
 * @author Lighting Ultra Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#ifndef UART_LOOPBACK_H
#define UART_LOOPBACK_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"

//=============================================================================
// 回环测试配置 (Loopback Test Configuration)
//=============================================================================

#define LOOPBACK_BUFFER_SIZE    256     /**< 回环缓冲区大小 */
#define LOOPBACK_TIMEOUT_MS     100     /**< 回环响应超时时间 */

//=============================================================================
// 回环测试状态枚举 (Loopback Test State)
//=============================================================================

typedef enum
{
    LOOPBACK_STATE_IDLE,            /**< 空闲状态，等待数据 */
    LOOPBACK_STATE_RECEIVING,       /**< 正在接收数据 */
    LOOPBACK_STATE_DATA_READY,      /**< 数据接收完成，准备发送 */
    LOOPBACK_STATE_TRANSMITTING     /**< 正在发送回环数据 */
} LoopbackState_e;

//=============================================================================
// 回环测试实例结构体 (Loopback Test Instance)
//=============================================================================

typedef struct
{
    /* 硬件配置 */
    UART_HandleTypeDef*     huart;          /**< UART句柄 */
    DMA_HandleTypeDef*      hdma_rx;        /**< DMA接收句柄 */
    DMA_HandleTypeDef*      hdma_tx;        /**< DMA发送句柄 */
    GPIO_TypeDef*           de_re_port;     /**< RS485方向控制端口 */
    uint16_t                de_re_pin;      /**< RS485方向控制引脚 */
    
    /* 状态管理 */
    volatile LoopbackState_e eState;        /**< 当前状态 */
    uint16_t                u16RxLen;       /**< 接收数据长度 */
    uint32_t                u32LastRxTime;  /**< 最后接收时间 */
    
    /* 数据缓冲区 */
    uint8_t                 au8RxBuffer[LOOPBACK_BUFFER_SIZE];   /**< 接收缓冲区 */
    uint8_t                 au8TxBuffer[LOOPBACK_BUFFER_SIZE];   /**< 发送缓冲区 */
    
    /* 统计信息 */
    uint32_t                u32TotalPackets;    /**< 总包数 */
    uint32_t                u32ErrorCount;      /**< 错误计数 */
    
} LoopbackInstance_t;

//=============================================================================
// 公共API函数声明 (Public API Function Prototypes)
//=============================================================================

/**
 * @brief 初始化UART回环测试实例
 * @param pInstance 回环测试实例指针
 * @param huart UART句柄
 * @param hdma_rx DMA接收句柄
 * @param hdma_tx DMA发送句柄
 * @param de_re_port RS485方向控制GPIO端口
 * @param de_re_pin RS485方向控制GPIO引脚
 * @return HAL_StatusTypeDef 初始化结果
 */
HAL_StatusTypeDef loopbackInit(LoopbackInstance_t *pInstance,
                              UART_HandleTypeDef* huart,
                              DMA_HandleTypeDef* hdma_rx,
                              DMA_HandleTypeDef* hdma_tx,
                              GPIO_TypeDef* de_re_port,
                              uint16_t de_re_pin);

/**
 * @brief 启动回环测试
 * @param pInstance 回环测试实例指针
 * @return HAL_StatusTypeDef 启动结果
 */
HAL_StatusTypeDef loopbackStart(LoopbackInstance_t *pInstance);

/**
 * @brief 停止回环测试
 * @param pInstance 回环测试实例指针
 * @return HAL_StatusTypeDef 停止结果
 */
HAL_StatusTypeDef loopbackStop(LoopbackInstance_t *pInstance);

/**
 * @brief 回环测试主循环处理函数
 * @details 在主循环中调用，处理数据接收和发送
 * @param pInstance 回环测试实例指针
 */
void loopbackPoll(LoopbackInstance_t *pInstance);

/**
 * @brief 获取回环测试统计信息
 * @param pInstance 回环测试实例指针
 * @param totalPackets 输出总包数
 * @param errorCount 输出错误计数
 * @return HAL_StatusTypeDef 获取结果
 */
HAL_StatusTypeDef loopbackGetStats(LoopbackInstance_t *pInstance, 
                                  uint32_t* totalPackets, 
                                  uint32_t* errorCount);

/**
 * @brief 重置回环测试统计信息
 * @param pInstance 回环测试实例指针
 */
void loopbackResetStats(LoopbackInstance_t *pInstance);

#endif // UART_LOOPBACK_H

