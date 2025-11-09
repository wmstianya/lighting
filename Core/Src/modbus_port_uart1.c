/**
 * @file modbus_port_uart1.c
 * @brief Modbus RTU UART1端口实现
 * @details STM32F103 UART1(PA9/PA10)硬件接口实现
 * @author Lighting Ultra Team
 * @date 2025-11-08
 */

#include "modbus_port.h"
#include "main.h"

/* 外部UART句柄 */
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;

/* UART1端口实例 */
ModbusPort_t modbusPortUart1;

/* ==================== 端口初始化 ==================== */
/**
 * @brief UART1端口初始化
 * @param mb Modbus RTU实例
 */
void ModbusPort_UART1_Init(ModbusRTU_t *mb)
{
    if (!mb) return;
    
    /* 配置端口参数 */
    modbusPortUart1.huart = &huart1;
    modbusPortUart1.hdma_rx = &hdma_usart1_rx;
    modbusPortUart1.hdma_tx = &hdma_usart1_tx;
    
    /* RS485控制引脚：PA8 */
    modbusPortUart1.rs485Port = GPIOA;
    modbusPortUart1.rs485Pin = GPIO_PIN_8;
    
    /* LED指示引脚：PB0（可选，根据需要修改） */
    modbusPortUart1.ledPort = GPIOB;
    modbusPortUart1.ledPin = GPIO_PIN_0;
    
    /* 关联Modbus实例 */
    modbusPortUart1.modbusInstance = mb;
    
    /* 设置硬件接口 */
    ModbusHardware_t hw = {
        .sendData = ModbusPort_UART1_SendData,
        .setRS485Dir = ModbusPort_UART1_SetRS485Dir,
        .ledIndicate = ModbusPort_UART1_LedIndicate,
        .getSysTick = ModbusPort_GetSysTick,
        .portContext = &modbusPortUart1
    };
    
    ModbusRTU_SetHardware(mb, &hw);
    
    /* 设置RS485为接收模式 */
    HAL_GPIO_WritePin(modbusPortUart1.rs485Port, modbusPortUart1.rs485Pin, GPIO_PIN_RESET);
    
    /* 清除IDLE标志并启用IDLE中断 */
    __HAL_UART_CLEAR_IDLEFLAG(&huart1);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    
    /* 启动DMA接收 */
    ModbusPort_UART1_StartReceive();
}

/**
 * @brief UART1端口反初始化
 */
void ModbusPort_UART1_DeInit(void)
{
    /* 停止DMA */
    HAL_UART_DMAStop(&huart1);
    
    /* 禁用IDLE中断 */
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_IDLE);
    
    /* RS485设为接收模式 */
    HAL_GPIO_WritePin(modbusPortUart1.rs485Port, modbusPortUart1.rs485Pin, GPIO_PIN_RESET);
    
    /* 清除关联 */
    modbusPortUart1.modbusInstance = NULL;
}

/* ==================== 硬件操作函数 ==================== */
/**
 * @brief UART1发送数据
 */
void ModbusPort_UART1_SendData(void *port, uint8_t *data, uint16_t len)
{
    ModbusPort_t *p = (ModbusPort_t *)port;
    if (!p || !p->huart) return;
    
    HAL_UART_Transmit_DMA(p->huart, data, len);
}

/**
 * @brief UART1 RS485方向控制
 */
void ModbusPort_UART1_SetRS485Dir(void *port, uint8_t txMode)
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
 * @brief UART1 LED指示
 */
void ModbusPort_UART1_LedIndicate(void *port, uint8_t state)
{
    ModbusPort_t *p = (ModbusPort_t *)port;
    if (!p || !p->ledPort) return;
    
    HAL_GPIO_WritePin(p->ledPort, p->ledPin, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/**
 * @brief UART1启动接收
 */
void ModbusPort_UART1_StartReceive(void)
{
    if (modbusPortUart1.modbusInstance) {
        HAL_UART_Receive_DMA(&huart1, 
                             modbusPortUart1.modbusInstance->rxBuffer, 
                             MODBUS_BUFFER_SIZE);
    }
}

/* ==================== 中断处理函数 ==================== */
/**
 * @brief UART1 IDLE中断回调
 * @note 在stm32f1xx_it.c的USART1_IRQHandler中调用
 */
void ModbusPort_UART1_IdleCallback(void)
{
    if (!modbusPortUart1.modbusInstance) return;
    
    /* 停止DMA */
    HAL_UART_DMAStop(&huart1);
    
    /* 清除可能的ORE */
    volatile uint32_t sr = huart1.Instance->SR; (void)sr;
    volatile uint32_t dr = huart1.Instance->DR; (void)dr;
    
    /* 计算接收字节数 */
    uint16_t rxLen = MODBUS_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
    
    /* 通知Modbus核心 */
    ModbusRTU_RxCallback(modbusPortUart1.modbusInstance, rxLen);
    
    /* 如果没有数据或处理完成，重启接收 */
    if (rxLen == 0 || modbusPortUart1.modbusInstance->state != MODBUS_STATE_SENDING) {
        ModbusPort_UART1_StartReceive();
    }
}

/**
 * @brief UART1 DMA发送完成回调
 * @note 在HAL_UART_TxCpltCallback中调用
 */
void ModbusPort_UART1_TxCpltCallback(void)
{
    if (!modbusPortUart1.modbusInstance) return;
    
    /* 等待发送移位寄存器空 */
    uint32_t startTick = HAL_GetTick();
    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET) {
        if ((HAL_GetTick() - startTick) > 2U) break;
    }
    
    /* 通知Modbus核心 */
    ModbusRTU_TxCallback(modbusPortUart1.modbusInstance);
    
    /* 重启接收 */
    ModbusPort_UART1_StartReceive();
}

/* ==================== 系统函数 ==================== */
/**
 * @brief 获取系统时间
 */
uint32_t ModbusPort_GetSysTick(void)
{
    return HAL_GetTick();
}
