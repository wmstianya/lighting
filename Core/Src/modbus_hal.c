/**
 * @file modbus_hal.c
 * @brief Modbus RTU从机协议栈硬件抽象层的STM32F1平台的具体实现
 * @details
 * 此文件使用STM32 HAL库函数实现modbus_hal.h中定义的标准接口。
 */

#include "modbus_hal.h"

HAL_StatusTypeDef Modbus_HAL_Init(ModbusInstance_t *pInstance)
{
    if (pInstance == NULL)
    {
        return HAL_ERROR;
    }
    // 启用初始DMA接收。这是协议栈初始化后的第一步。
    return Modbus_HAL_StartReception(pInstance);
}

HAL_StatusTypeDef Modbus_HAL_StartReception(ModbusInstance_t *pInstance)
{
    if (pInstance == NULL || pInstance->huart == NULL)
    {
        return HAL_ERROR;
    }
    
    // 1. 启用UART IDLE中断 - 这是Modbus RTU协议的关键！
    __HAL_UART_ENABLE_IT(pInstance->huart, UART_IT_IDLE);
    
    // 2. 使用STM32 HAL库函数启动DMA模式下的UART接收
    return HAL_UART_Receive_DMA(pInstance->huart, pInstance->au8RxBuffer, MODBUS_BUFFER_SIZE);
}

HAL_StatusTypeDef Modbus_HAL_StopReception(ModbusInstance_t *pInstance)
{
    if (pInstance == NULL || pInstance->huart == NULL)
    {
        return HAL_ERROR;
    }
    // 使用STM32 HAL库函数停止DMA接收
    return HAL_UART_DMAStop(pInstance->huart);
}

HAL_StatusTypeDef Modbus_HAL_Transmit(ModbusInstance_t *pInstance, uint16_t u16Length)
{
    if (pInstance == NULL || pInstance->huart == NULL || u16Length == 0)
    {
        return HAL_ERROR;
    }

    // 1. 将RS485收发器设置为发送模式
    Modbus_HAL_SetDirTx(pInstance);
    
    // 2. 添加小延时确保RS485方向切换稳定
    for(volatile int i = 0; i < 10; i++); // 约1微秒延时

    // 3. 使用STM32 HAL库函数启动DMA模式下的UART发送
    return HAL_UART_Transmit_DMA(pInstance->huart, pInstance->au8TxBuffer, u16Length);
}

void Modbus_HAL_SetDirTx(ModbusInstance_t *pInstance)
{
    if (pInstance != NULL && pInstance->de_re_port != NULL)
    {
        HAL_GPIO_WritePin(pInstance->de_re_port, pInstance->de_re_pin, GPIO_PIN_SET);
    }
}

void Modbus_HAL_SetDirRx(ModbusInstance_t *pInstance)
{
    if (pInstance != NULL && pInstance->de_re_port != NULL)
    {
        HAL_GPIO_WritePin(pInstance->de_re_port, pInstance->de_re_pin, GPIO_PIN_RESET);
    }
}