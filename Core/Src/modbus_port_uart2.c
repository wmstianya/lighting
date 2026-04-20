/**
 * @file modbus_port_uart2.c
 * @brief Modbus RTU UART2端口实现
 * @details STM32F103 UART2(PA2/PA3)硬件接口实现
 * @author Lighting Ultra Team
 * @date 2025-11-08
 */

#include "modbus_port.h"
#include "main.h"

/* 外部UART句柄 */
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;

/* UART2端口实例 */
ModbusPort_t modbusPortUart2;

/* ==================== 端口初始化 ==================== */
/**
 * @brief UART2端口初始化
 * @param mb Modbus RTU实例
 */
void ModbusPort_UART2_Init(ModbusRTU_t *mb)
{
    if (!mb) return;
    
    /* 配置端口参数 */
    modbusPortUart2.huart = &huart2;
    modbusPortUart2.hdma_rx = &hdma_usart2_rx;
    modbusPortUart2.hdma_tx = &hdma_usart2_tx;
    
    /* RS485控制引脚：PA4 */
    modbusPortUart2.rs485Port = GPIOA;
    modbusPortUart2.rs485Pin = GPIO_PIN_4;
    
    /* 关闭LED指示，释放PB1给DO1使用 */
    modbusPortUart2.ledPort = NULL;
    modbusPortUart2.ledPin = 0;
    
    /* 关联Modbus实例 */
    modbusPortUart2.modbusInstance = mb;
    
    /* 设置硬件接口 */
    ModbusHardware_t hw = {
        .sendData = ModbusPort_UART2_SendData,
        .setRS485Dir = ModbusPort_UART2_SetRS485Dir,
        .ledIndicate = ModbusPort_UART2_LedIndicate,
        .getSysTick = ModbusPort_GetSysTick,
        .portContext = &modbusPortUart2
    };
    
    ModbusRTU_SetHardware(mb, &hw);
    
    /* 设置RS485为接收模式 */
    HAL_GPIO_WritePin(modbusPortUart2.rs485Port, modbusPortUart2.rs485Pin, GPIO_PIN_RESET);
    
    /* 清除IDLE标志并启用IDLE中断 */
    __HAL_UART_CLEAR_IDLEFLAG(&huart2);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
    
    /* 启动DMA接收 */
    ModbusPort_UART2_StartReceive();
}

/**
 * @brief UART2端口反初始化
 */
void ModbusPort_UART2_DeInit(void)
{
    /* 停止DMA */
    HAL_UART_DMAStop(&huart2);
    
    /* 禁用IDLE中断 */
    __HAL_UART_DISABLE_IT(&huart2, UART_IT_IDLE);
    
    /* RS485设为接收模式 */
    HAL_GPIO_WritePin(modbusPortUart2.rs485Port, modbusPortUart2.rs485Pin, GPIO_PIN_RESET);
    
    /* 清除关联 */
    modbusPortUart2.modbusInstance = NULL;
}

/* ==================== 硬件操作函数 ==================== */
/**
 * @brief UART2发送数据
 */
void ModbusPort_UART2_SendData(void *port, uint8_t *data, uint16_t len)
{
    ModbusPort_t *p = (ModbusPort_t *)port;
    if (!p || !p->huart) return;
    
    HAL_UART_Transmit_DMA(p->huart, data, len);
}

/**
 * @brief UART2 RS485方向控制
 */
void ModbusPort_UART2_SetRS485Dir(void *port, uint8_t txMode)
{
    ModbusPort_t *p = (ModbusPort_t *)port;
    if (!p) return;
    
    HAL_GPIO_WritePin(p->rs485Port, p->rs485Pin, txMode ? GPIO_PIN_SET : GPIO_PIN_RESET);
    
    /* 短延时确保RS485切换稳定 */
    if (txMode) {
        for(volatile uint32_t i = 0; i < 100; i++);
    }
}

/**
 * @brief UART2 LED指示
 */
void ModbusPort_UART2_LedIndicate(void *port, uint8_t state)
{
    ModbusPort_t *p = (ModbusPort_t *)port;
    if (!p || !p->ledPort) return;
    
    HAL_GPIO_WritePin(p->ledPort, p->ledPin, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/**
 * @brief UART2启动接收
 */
void ModbusPort_UART2_StartReceive(void)
{
    if (modbusPortUart2.modbusInstance) {
        HAL_UART_Receive_DMA(&huart2, 
                             modbusPortUart2.modbusInstance->rxBuffer, 
                             MODBUS_BUFFER_SIZE);
    }
}

/* ==================== 中断处理函数 ==================== */
/**
 * @brief UART2 IDLE中断回调
 * @note 在stm32f1xx_it.c的USART2_IRQHandler中调用
 */
void ModbusPort_UART2_IdleCallback(void)
{
    if (!modbusPortUart2.modbusInstance) return;
    
    /* 停止DMA */
    HAL_UART_DMAStop(&huart2);
    
    /* 清除可能的ORE */
    volatile uint32_t sr = huart2.Instance->SR; (void)sr;
    volatile uint32_t dr = huart2.Instance->DR; (void)dr;
    
    /* 计算接收字节数 */
    uint16_t rxLen = MODBUS_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
    
    /* 通知Modbus核心 */
    ModbusRTU_RxCallback(modbusPortUart2.modbusInstance, rxLen);
    
    /* 如果没有数据或处理完成，重启接收 */
    if (rxLen == 0 || modbusPortUart2.modbusInstance->state != MODBUS_STATE_SENDING) {
        ModbusPort_UART2_StartReceive();
    }
}

/**
 * @brief UART2 DMA发送完成回调
 * @note 在HAL_UART_TxCpltCallback中调用
 */
void ModbusPort_UART2_TxCpltCallback(void)
{
    if (!modbusPortUart2.modbusInstance) return;
    
    /* 等待发送移位寄存器空 */
    uint32_t startTick = HAL_GetTick();
    while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) == RESET) {
        if ((HAL_GetTick() - startTick) > 2U) break;
    }
    
    /* 通知Modbus核心 */
    ModbusRTU_TxCallback(modbusPortUart2.modbusInstance);
    
    /* 重启接收 */
    ModbusPort_UART2_StartReceive();
}
