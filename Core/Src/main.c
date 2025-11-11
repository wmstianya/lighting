/*
 * @Author: Administrator wmstianya@gmail.com
 * @Date: 2025-08-20 15:21:11
 * @LastEditors: Administrator wmstianya@gmail.com
 * @LastEditTime: 2025-11-11 16:04:04
 * @FilePath: \MDK-ARMe:\data\lighting_ultra\lighting_ultra\Core\Src\main.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/* main.c — STM32F103C8T6 + HAL + USART1 DMA + TIM2 + IDLE 检帧 + 快照式读 */
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_tim.h"
#include "usart2_echo_test.h"
#include "usart2_echo_test_debug.h"
#include "usart2_simple_test.h"
#include "usart1_echo_test.h"
#include "app_config.h"
#include "relay.h"
#include "beep.h"
#include "led.h"
#include "watchdog.h"
#include "pressure_sensor.h"
#include "water_level.h"
#include "config_manager.h"
#include "error_handler.h"
#if RUN_MODE_ECHO_TEST == 10
#include "modbus_app.h"  /* 模块化Modbus应用 */
#else
#include "../../MDK-ARM/modbus_rtu_slave.h"
#endif

/* 运行模式选择集中到 app_config.h */

/* ---------------- 外设句柄 ---------------- */
UART_HandleTypeDef huart1;  /* USART1: PA9/PA10 */
DMA_HandleTypeDef  hdma_usart1_rx;
DMA_HandleTypeDef  hdma_usart1_tx;

UART_HandleTypeDef huart2;  /* USART2: PA2/PA3 */
DMA_HandleTypeDef  hdma_usart2_rx;
DMA_HandleTypeDef  hdma_usart2_tx;
// TIM_HandleTypeDef  htim2; // 暂时注释

/* ---------------- 全局 Modbus 实例 ---------------- */
#if RUN_MODE_ECHO_TEST != 10
ModbusRTU_Slave g_mb;   /* 绑定到 huart1 (USART1) - 旧版 */
ModbusRTU_Slave g_mb2;  /* 绑定到 huart2 (USART2) - 旧版 */
#endif



/* ---------------- 原型 ---------------- */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);  /* USART1: PA9/PA10 */
static void MX_USART2_UART_Init(void);  /* USART2: PA2/PA3 */
// static void MX_TIM2_Init(void); // 暂时注释

/* RS485 direction control is now defined in modbus_rtu_slave.h */

/* ---------------- 用户写寄存器回调（示例：保持寄存器0 控制 LED） ---------------- */
void ModbusRTU_PostWriteCallback(uint16_t addr, uint16_t value)
{
    #if RUN_MODE_ECHO_TEST != 10
    if (addr == 0) {
        /* PB1 低电平点亮 - 旧版Modbus模式才使用 */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, (value > 0) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }
    #else
    /* 模块化模式下不使用此回调，由modbus_app.c中的回调处理 */
    (void)addr;
    (void)value;
    #endif
}
/* ---------------- 主程序 ---------------- */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();  /* 初始化串口2 (USART1) */
    MX_USART2_UART_Init();  /* 初始化串口1 (USART2) */
    // MX_TIM2_Init(); // 暂时注释，避免链接错误

    /* 使能 USART IDLE 中断 */
    /* 为了调试方便，两个串口的IDLE中断都启用 */
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
    
    /* 初始化配置管理模块（从Flash加载或使用默认配置） */
    configManagerInit();
    const SystemConfig_t* config = configGet();
    
    /* 初始化错误处理框架 */
    errorHandlerInit();
    
    /* 初始化蜂鸣器驱动 */
    beepInit();
    
    /* 初始化LED指示灯驱动 */
    ledInit();
    
    /* 初始化外部看门狗驱动 */
    watchdogInit();
    
    /* 初始化压力传感器（使用配置参数） */
    pressureSensorInit(config->pressureMin, config->pressureMax);
    
    /* 初始化水位检测模块 */
    waterLevelInit();

    #if RUN_MODE_ECHO_TEST == 0
        /* 仅在Modbus模式下初始化 */
        /* Modbus 初始化 */
        ModbusRTU_Init(&g_mb,  &huart1, 0x01);  /* USART1: 从站地址 0x01 */
        ModbusRTU_Init(&g_mb2, &huart2, 0x02);  /* USART2: 从站地址 0x02 */

        /* 启动 1ms 定时中断（兜底用） */
        // HAL_TIM_Base_Start_IT(&htim2); // 暂时注释，避免链接错误

        /* 测试数据 */
        g_mb.holdingRegs[0] = 100;
        g_mb.holdingRegs[1] = 200;
        g_mb.holdingRegs[2] = 300;
        g_mb.inputRegs[0]   = 1000;
        g_mb.inputRegs[1]   = 2000;
        
        g_mb2.holdingRegs[0] = 500;
        g_mb2.holdingRegs[1] = 600;
        g_mb2.holdingRegs[2] = 700;
        g_mb2.inputRegs[0]   = 5000;
        g_mb2.inputRegs[1]   = 6000;
    #endif

    #if RUN_MODE_ECHO_TEST == 10
        /* 模块化Modbus双串口模式 */
        ModbusApp_Init();
        
        /* 系统自检完成：蜂鸣器提示 */
        beepSetTime(200);
        
        while (1) {
            ModbusApp_Process();  /* 处理两个串口的Modbus */
            
            /* 蜂鸣器定时处理（非阻塞式自动关闭） */
            beepProcess();
            
            /* 压力传感器采集处理（100ms自动采样） */
            pressureSensorProcess();
            
            /* 水位检测处理（50ms自动采样，带防抖） */
            waterLevelProcess();
            
            /* 定期更新传感器数据（示例） */
            static uint32_t lastSensorUpdate = 0;
            if (HAL_GetTick() - lastSensorUpdate > 1000) {
                lastSensorUpdate = HAL_GetTick();
                ModbusApp_UpdateSensorData();
            }
            
            /* 喂狗：外部看门狗TPS3823-33DBVR */
            watchdogFeed();
            
            HAL_Delay(1);
        }
    #elif RUN_MODE_ECHO_TEST == 3
        /* 简单测试模式 - 最基础版本 */
        usart2SimpleTestRun();
    #elif RUN_MODE_ECHO_TEST == 2
        /* 调试模式 - 详细状态输出 */
        usart2DebugTestRun();
    #elif RUN_MODE_ECHO_TEST == 1
        /* USART2(PA2/PA3)回环测试模式 */
        usart2EchoTestRun();
    #elif RUN_MODE_ECHO_TEST == 4
        /* USART1(PA9/PA10)回环测试模式 */
        usart1EchoTestRun();
    #else
        /* 旧版Modbus双串口模式 */
        while (1) {
            ModbusRTU_Process(&g_mb);   /* 处理USART1 */
            ModbusRTU_Process(&g_mb2);  /* 处理USART2 */
            HAL_Delay(1);
        }
    #endif
}

/* ---------------- 时钟配置（72MHz，HSE+PLL） ---------------- */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG(); /* 释放 PB3/PB4 等引脚 */

    RCC_OscInitStruct.OscillatorType   = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSEState         = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue   = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState         = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState     = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource    = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL       = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        while(1);
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2; /* 36MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1; /* 72MHz */
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        while(1);
    }
}

/* ---------------- GPIO ---------------- */
static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* LED1-LED4 (PB1/PB15/PB5/PB6)：由led.c模块初始化 */
    
    /* PB14 蜂鸣器：由beep.c模块初始化（TIM1_CH2N PWM） */
    
    /* PC14 外部看门狗：由watchdog.c模块初始化（TPS3823-33DBVR） */

    /* RS485 使能引脚配置 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    /* RS485 DE 引脚: 模块化模式下使用固定引脚，老模式使用宏 */
    #if RUN_MODE_ECHO_TEST == 10
      /* USART2: PA4 */
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
      GPIO_InitStruct.Pin   = GPIO_PIN_4;
      GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
      GPIO_InitStruct.Pull  = GPIO_NOPULL;
      GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
      HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
      
      /* USART1: PA8 */
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
      GPIO_InitStruct.Pin   = GPIO_PIN_8;
      GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
      GPIO_InitStruct.Pull  = GPIO_NOPULL;
      GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
      HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    #else
      /* USART2 RS485使能: 由旧版宏定义提供 */
      HAL_GPIO_WritePin(MB_USART2_RS485_DE_GPIO_Port, MB_USART2_RS485_DE_Pin, GPIO_PIN_RESET);
      GPIO_InitStruct.Pin   = MB_USART2_RS485_DE_Pin;
      GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
      GPIO_InitStruct.Pull  = GPIO_NOPULL;
      GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
      HAL_GPIO_Init(MB_USART2_RS485_DE_GPIO_Port, &GPIO_InitStruct);
      
      /* USART1 RS485使能: 由旧版宏定义提供 */
      HAL_GPIO_WritePin(MB_USART1_RS485_DE_GPIO_Port, MB_USART1_RS485_DE_Pin, GPIO_PIN_RESET);
      GPIO_InitStruct.Pin   = MB_USART1_RS485_DE_Pin;
      GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
      GPIO_InitStruct.Pull  = GPIO_NOPULL;
      GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
      HAL_GPIO_Init(MB_USART1_RS485_DE_GPIO_Port, &GPIO_InitStruct);
    #endif


}

/* ---------------- DMA ---------------- */
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* USART1 DMA */
    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 1, 0);  /* TX */
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 1, 1);  /* RX */
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
    
    /* USART2 DMA */
    HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 1, 2);  /* TX */
    HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 1, 3);  /* RX */
    HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
}

/* ---------------- USART1 (PA9 TX, PA10 RX) ---------------- */
static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* GPIO */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* TX PA9 */
    GPIO_InitStruct.Pin   = GPIO_PIN_9;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    /* RX PA10 */
    GPIO_InitStruct.Pin  = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART1 配置 */
    huart1.Instance        = USART1;
    huart1.Init.BaudRate   = 115200;  /* 调整为115200 bps */
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits   = UART_STOPBITS_1;
    huart1.Init.Parity     = UART_PARITY_NONE;
    huart1.Init.Mode       = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) { while(1); }

    /* DMA RX: DMA1_Channel5 */
    hdma_usart1_rx.Instance                 = DMA1_Channel5;
    hdma_usart1_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode                = DMA_NORMAL;
    hdma_usart1_rx.Init.Priority            = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK) { while(1); }
    __HAL_LINKDMA(&huart1, hdmarx, hdma_usart1_rx);

    /* DMA TX: DMA1_Channel4 */
    hdma_usart1_tx.Instance                 = DMA1_Channel4;
    hdma_usart1_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart1_tx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart1_tx.Init.Mode                = DMA_NORMAL;
    hdma_usart1_tx.Init.Priority            = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK) { while(1); }
    __HAL_LINKDMA(&huart1, hdmatx, hdma_usart1_tx);

    /* USART1 中断 */
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* ---------------- USART2 (PA2 TX, PA3 RX) ---------------- */
static void MX_USART2_UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* GPIO */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* TX PA2 */
    GPIO_InitStruct.Pin   = GPIO_PIN_2;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    /* RX PA3 */
    GPIO_InitStruct.Pin  = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART2 配置 */
    huart2.Instance        = USART2;
    huart2.Init.BaudRate   = 115200;  /* 调整为115200 bps */
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits   = UART_STOPBITS_1;
    huart2.Init.Parity     = UART_PARITY_NONE;
    huart2.Init.Mode       = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) { while(1); }

    /* DMA RX: DMA1_Channel6 */
    hdma_usart2_rx.Instance                 = DMA1_Channel6;
    hdma_usart2_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart2_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart2_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart2_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart2_rx.Init.Mode                = DMA_NORMAL;
    hdma_usart2_rx.Init.Priority            = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart2_rx) != HAL_OK) { while(1); }
    __HAL_LINKDMA(&huart2, hdmarx, hdma_usart2_rx);

    /* DMA TX: DMA1_Channel7 */
    hdma_usart2_tx.Instance                 = DMA1_Channel7;
    hdma_usart2_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_usart2_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart2_tx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart2_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart2_tx.Init.Mode                = DMA_NORMAL;
    hdma_usart2_tx.Init.Priority            = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart2_tx) != HAL_OK) { while(1); }
    __HAL_LINKDMA(&huart2, hdmatx, hdma_usart2_tx);

    /* USART2 中断 */
    HAL_NVIC_SetPriority(USART2_IRQn, 0, 1);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/* ---------------- TIM2: 1ms 中断 (暂时注释) ---------------- */
/*
static void MX_TIM2_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance = TIM2;
    htim2.Init.Prescaler         = 72 - 1;   // 72MHz /72 = 1MHz
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 1000 - 1; // 1ms
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) { while(1); }

    HAL_NVIC_SetPriority(TIM2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}
*/

/* ---------------- Error Handler ---------------- */
void Error_Handler(void)
{
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
}
