/**
 * @file main_modbus_example.c
 * @brief 模块化Modbus使用示例
 * @details 展示如何在main.c中使用模块化的Modbus RTU
 * @author Lighting Ultra Team
 * @date 2025-11-08
 * 
 * @note 这是一个示例文件，展示如何集成模块化Modbus
 *       实际使用时，将相关代码复制到你的main.c中
 */

#include "main.h"
#include "modbus_app.h"
#include "app_config.h"
#include "modbus_port.h"      /* for ModbusPort_UARTx_* callbacks */
#include "usart2_echo_test.h" /* for usart2Echo* when RUN_MODE_ECHO_TEST==1 */

/* extern UART handles provided by main.c */
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

/* 如果需要定义新的运行模式 */
#define RUN_MODE_MODBUS_DUAL    10  /* 双串口Modbus模式(使用新的模块化设计) */

/* ==================== 初始化示例 ==================== */
void Example_ModbusInit(void)
{
    /* 初始化Modbus应用 */
    ModbusApp_Init();
    
    /* 如果需要自定义配置，可以获取实例进行修改 */
    ModbusRTU_t *mb1 = ModbusApp_GetUart1Instance();
    ModbusRTU_t *mb2 = ModbusApp_GetUart2Instance();
    
    /* 示例：修改从站地址 */
    // ModbusRTU_SetSlaveAddr(mb1, 0x10);
    // ModbusRTU_SetSlaveAddr(mb2, 0x20);
}

/* ==================== 主循环示例 ==================== */
void Example_MainLoop(void)
{
    /* 系统初始化代码... */
    // HAL_Init();
    // SystemClock_Config();
    // MX_GPIO_Init();
    // MX_DMA_Init();
    // MX_USART1_UART_Init();
    // MX_USART2_UART_Init();
    
    #if RUN_MODE_ECHO_TEST == RUN_MODE_MODBUS_DUAL
        /* 使用新的模块化Modbus */
        ModbusApp_Init();
        
        while (1) {
            /* 处理Modbus通信 */
            ModbusApp_Process();
            
            /* 更新传感器数据（可选） */
            static uint32_t lastUpdate = 0;
            if (HAL_GetTick() - lastUpdate > 1000) {
                lastUpdate = HAL_GetTick();
                ModbusApp_UpdateSensorData();
            }
            
            /* 其他应用逻辑... */
            
            HAL_Delay(1);
        }
    #elif RUN_MODE_ECHO_TEST == 1
        /* 原有的echo测试模式 */
        // usart2EchoTestRun();
    #else
        /* 原有的Modbus模式 */
        // 原有代码...
    #endif
}

/* ==================== 中断处理示例 ==================== */
/**
 * @note 在stm32f1xx_it.c中的USART1_IRQHandler添加：
 */
void Example_USART1_IRQHandler(void)
{
    #if RUN_MODE_ECHO_TEST == RUN_MODE_MODBUS_DUAL
        /* 新的模块化Modbus IDLE处理 */
        if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) != RESET) {
            __HAL_UART_CLEAR_IDLEFLAG(&huart1);
            ModbusPort_UART1_IdleCallback();
            return;
        }
    #endif
    
    HAL_UART_IRQHandler(&huart1);
}

/**
 * @note 在stm32f1xx_it.c中的USART2_IRQHandler添加：
 */
void Example_USART2_IRQHandler(void)
{
    #if RUN_MODE_ECHO_TEST == RUN_MODE_MODBUS_DUAL
        /* 新的模块化Modbus IDLE处理 */
        if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE) != RESET) {
            __HAL_UART_CLEAR_IDLEFLAG(&huart2);
            ModbusPort_UART2_IdleCallback();
            return;
        }
    #elif RUN_MODE_ECHO_TEST == 1
        /* 原有的echo测试 */
        if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE) != RESET) {
            __HAL_UART_CLEAR_IDLEFLAG(&huart2);
            usart2EchoHandleIdle();
            return;
        }
    #endif
    
    HAL_UART_IRQHandler(&huart2);
}

/**
 * @note 在HAL_UART_TxCpltCallback中添加：
 */
void Example_HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    #if RUN_MODE_ECHO_TEST == RUN_MODE_MODBUS_DUAL
        if (huart == &huart1) {
            ModbusPort_UART1_TxCpltCallback();
        }
        else if (huart == &huart2) {
            ModbusPort_UART2_TxCpltCallback();
        }
    #elif RUN_MODE_ECHO_TEST == 1
        if (huart == &huart2) {
            usart2EchoTxCallback(huart);
        }
    #endif
}

/* ==================== 数据访问示例 ==================== */
/**
 * @brief 示例：在其他模块中访问Modbus数据
 */
void Example_AccessModbusData(void)
{
    ModbusRTU_t *mb1 = ModbusApp_GetUart1Instance();
    ModbusRTU_t *mb2 = ModbusApp_GetUart2Instance();
    
    /* 读取保持寄存器 */
    uint16_t value1 = ModbusRTU_ReadHoldingReg(mb1, 0);
    uint16_t value2 = ModbusRTU_ReadHoldingReg(mb2, 0);
    
    /* 写入保持寄存器 */
    ModbusRTU_WriteHoldingReg(mb1, 10, 0x1234);
    ModbusRTU_WriteHoldingReg(mb2, 10, 0x5678);
    
    /* 控制线圈 */
    ModbusRTU_WriteCoil(mb1, 0, 1);  /* 打开线圈0 */
    ModbusRTU_WriteCoil(mb2, 0, 0);  /* 关闭线圈0 */
    
    /* 读取线圈状态 */
    uint8_t coil1 = ModbusRTU_ReadCoil(mb1, 0);
    uint8_t coil2 = ModbusRTU_ReadCoil(mb2, 0);
    
    /* 获取统计信息 */
    ModbusStats_t *stats1 = ModbusRTU_GetStats(mb1);
    ModbusStats_t *stats2 = ModbusRTU_GetStats(mb2);
    
    /* 使用统计信息 */
    if (stats1->crcErrorCount > 10) {
        /* 通信质量差，采取措施 */
    }
}
