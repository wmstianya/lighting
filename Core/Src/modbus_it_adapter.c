/**
 * @file modbus_it_adapter.c
 * @brief Modbus中断处理适配器
 * @details 连接STM32中断处理和模块化Modbus实现
 * @author Lighting Ultra Team
 * @date 2025-11-08
 */

#include "modbus_port.h"
#include "modbus_app.h"
#include "app_config.h"

/* 外部UART句柄 */
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

/**
 * @brief USART1 IDLE中断处理
 * @note 在stm32f1xx_it.c的USART1_IRQHandler中调用
 */
void ModbusIT_UART1_IdleHandler(void)
{
    #if RUN_MODE_ECHO_TEST == 0 || RUN_MODE_ECHO_TEST == 10
        /* Modbus模式 */
        ModbusPort_UART1_IdleCallback();
    #endif
}

/**
 * @brief USART2 IDLE中断处理
 * @note 在stm32f1xx_it.c的USART2_IRQHandler中调用
 */
void ModbusIT_UART2_IdleHandler(void)
{
    #if RUN_MODE_ECHO_TEST == 0 || RUN_MODE_ECHO_TEST == 10
        /* Modbus模式 */
        ModbusPort_UART2_IdleCallback();
    #elif RUN_MODE_ECHO_TEST == 1
        /* 兼容原有的echo测试 */
        extern void usart2EchoHandleIdle(void);
        usart2EchoHandleIdle();
    #endif
}

/**
 * @brief UART发送完成回调
 * @note 在HAL_UART_TxCpltCallback中调用
 */
void ModbusIT_TxCpltCallback(UART_HandleTypeDef *huart)
{
    #if RUN_MODE_ECHO_TEST == 0 || RUN_MODE_ECHO_TEST == 10
        if (huart == &huart1) {
            ModbusPort_UART1_TxCpltCallback();
        }
        else if (huart == &huart2) {
            ModbusPort_UART2_TxCpltCallback();
        }
    #elif RUN_MODE_ECHO_TEST == 1
        if (huart == &huart2) {
            extern void usart2EchoTxCallback(UART_HandleTypeDef *huart);
            usart2EchoTxCallback(huart);
        }
    #endif
}
