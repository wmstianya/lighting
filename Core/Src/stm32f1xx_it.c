/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f1xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f1xx_it.h"
#include "stm32f1xx_hal_tim.h"
#include "../../MDK-ARM/modbus_rtu_slave.h"
#include "usart2_echo_test.h"
#include "usart1_echo_test.h"
#include "usart2_echo_test_debug.h"
#include "usart2_simple_test.h"
// #include "app_config.h"  // 文件不存在，注释掉

// 声明全局 Modbus 实例在main.c中定义
extern ModbusRTU_Slave g_mb;   /* 串口2 (USART1) */
extern ModbusRTU_Slave g_mb2;  /* 串口1 (USART2) */

/* 从 main.c 获取运行模式定义 */
#define RUN_MODE_ECHO_TEST 4  /* 串口1回环测试模式 */

/* 根据 RUN_MODE_ECHO_TEST 自动映射中断处理模式 */
#if   RUN_MODE_ECHO_TEST == 1
#define USART2_TEST_MODE 1
#define USART1_TEST_MODE 0
#elif RUN_MODE_ECHO_TEST == 2
#define USART2_TEST_MODE 2
#define USART1_TEST_MODE 0
#elif RUN_MODE_ECHO_TEST == 3
#define USART2_TEST_MODE 3
#define USART1_TEST_MODE 0
#elif RUN_MODE_ECHO_TEST == 4
#define USART2_TEST_MODE 0
#define USART1_TEST_MODE 1
#else
#define USART2_TEST_MODE 0
#define USART1_TEST_MODE 0
#endif

// 声明TIM3句柄（在usart2_echo_test.c中定义） - 暂时不使用
// #if USART2_TEST_MODE == 1 || USART2_TEST_MODE == 2
// extern TIM_HandleTypeDef htim3;
// extern TIM_HandleTypeDef htim3_debug;
// #endif

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern UART_HandleTypeDef huart1;

extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern UART_HandleTypeDef huart2;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M3 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32F1xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f1xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles DMA1 channel4 global interrupt.
  */
void DMA1_Channel4_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel4_IRQn 0 */

  /* USER CODE END DMA1_Channel4_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart1_tx);
  /* USER CODE BEGIN DMA1_Channel4_IRQn 1 */

  /* USER CODE END DMA1_Channel4_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel5 global interrupt (USART1 RX).
  */
void DMA1_Channel5_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel5_IRQn 0 */
  /* USER CODE END DMA1_Channel5_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart1_rx);
  /* USER CODE BEGIN DMA1_Channel5_IRQn 1 */
  /* USER CODE END DMA1_Channel5_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel6 global interrupt (USART2 RX).
  */
void DMA1_Channel6_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel6_IRQn 0 */
  /* USER CODE END DMA1_Channel6_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart2_rx);
  /* USER CODE BEGIN DMA1_Channel6_IRQn 1 */
  /* USER CODE END DMA1_Channel6_IRQn 1 */
}

/**
  * @brief This function handles DMA1 channel7 global interrupt (USART2 TX).
  */
void DMA1_Channel7_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel7_IRQn 0 */
  /* USER CODE END DMA1_Channel7_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart2_tx);
  /* USER CODE BEGIN DMA1_Channel7_IRQn 1 */
  /* USER CODE END DMA1_Channel7_IRQn 1 */
}

/**
  * @brief This function handles USART1 global interrupt (串口2).
  */
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */
  #if USART1_TEST_MODE == 1
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) != RESET)
    {
      __HAL_UART_CLEAR_IDLEFLAG(&huart1);
      usart1EchoHandleIdle();
    }
  #else
    /* Modbus模式：先处理IDLE，再调用HAL（避免HAL清除标志） */
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE) != RESET)
    {
      __HAL_UART_CLEAR_IDLEFLAG(&huart1);
      ModbusRTU_UartRxCallback(&g_mb);
      /* 重要：IDLE已处理，不再调用HAL_UART_IRQHandler */
      return;
    }
  #endif
  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart1);
  /* USER CODE BEGIN USART1_IRQn 1 */
  /* USER CODE END USART1_IRQn 1 */
}

/**
  * @brief This function handles USART2 global interrupt (串口1).
  */
void USART2_IRQHandler(void)
{
  /* USER CODE BEGIN USART2_IRQn 0 */
  #if USART2_TEST_MODE == 3
    /* 简单测试模式 */
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE) != RESET)
    {
      __HAL_UART_CLEAR_IDLEFLAG(&huart2);
      usart2SimpleHandleIdle();
    }
  #elif USART2_TEST_MODE == 2
    /* 调试模式 */
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE) != RESET)
    {
      __HAL_UART_CLEAR_IDLEFLAG(&huart2);
      usart2DebugHandleIdle();
    }
  #elif USART2_TEST_MODE == 1
    /* Echo测试模式 */
    /* 强制进入这个分支进行调试 */
    #pragma message("USART2_TEST_MODE is 1 - Echo test mode")
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE) != RESET)
    {
      __HAL_UART_CLEAR_IDLEFLAG(&huart2);
      usart2EchoHandleIdle();
    }
  #else
    /* Modbus模式：先处理IDLE，再调用HAL */
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE) != RESET)
    {
      __HAL_UART_CLEAR_IDLEFLAG(&huart2);
      ModbusRTU_UartRxCallback(&g_mb2);
      /* 重要：IDLE已处理，不再调用HAL_UART_IRQHandler */
      return;
    }
  #endif
  /* USER CODE END USART2_IRQn 0 */
  HAL_UART_IRQHandler(&huart2);
  /* USER CODE BEGIN USART2_IRQn 1 */
  /* USER CODE END USART2_IRQn 1 */
}

/**
  * @brief  Tx Transfer completed callback.
  * @param  huart UART handle.
  * @retval None
  * 
  * @details 
  * Critical callback for Modbus RTU communication:
  * 1. Switch RS485 transceiver back to receive mode
  * 2. Restart DMA reception for next Modbus frame
  * 3. Clear transmission flags and reset state
  * 
  * Without this implementation, the system will deadlock after first response.
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  /* USER CODE BEGIN HAL_UART_TxCpltCallback 0 */
  #if USART1_TEST_MODE == 1
    if (huart == &huart1) {
      usart1EchoTxCallback(huart);
      return;
    }
  #endif
  /* Modbus: 串口2 (USART1) Tx 完成处理 - 暂时注释，函数未定义
  if (huart == &huart1) {
    // 等待 TC，避免过早切 DE
    uint32_t t0 = HAL_GetTick();
    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET) {
      if ((HAL_GetTick() - t0) > 2U) break;
    }
    // ModbusRTU_TxCpltISR(&g_mb);  // 函数未定义
    return;
  }
  */
  #if USART2_TEST_MODE == 3
    /* 简单测试模式 - USART2发送完成 */
    if (huart == &huart2) {
      usart2SimpleTxCallback(huart);
      return;
    }
  #elif USART2_TEST_MODE == 1
    /* Echo测试模式 - USART2发送完成 */
    if (huart == &huart2) {
      usart2EchoTxCallback(huart);
      return;
    }
  #elif USART2_TEST_MODE == 2
    /* 调试模式 - USART2发送完成 */
    if (huart == &huart2) {
      usart2DebugTxCallback(huart);
      return;
    }
  #endif
  
  /* 处理串口2 (USART1) - Modbus */
  if (huart == &huart1) {
    /* Step 1: Switch RS485 to receive mode */
    HAL_GPIO_WritePin(MB_USART1_RS485_DE_GPIO_Port, MB_USART1_RS485_DE_Pin, GPIO_PIN_RESET);
    
    /* Step 2: Clear transmission state flags */
    g_mb.txCount = 0;
    g_mb.rxComplete = 0;
    g_mb.rxCount = 0;
    g_mb.frameReceiving = 0;
    
    /* Step 3: Restart DMA reception */
    HAL_UART_Receive_DMA(&huart1, g_mb.rxBuffer, MB_RTU_FRAME_MAX_SIZE);
  }
  
  #if USART2_TEST_MODE == 0
    /* 处理串口1 (USART2) - 仅Modbus模式 */
    if (huart == &huart2) {
      uint32_t t1 = HAL_GetTick();
      while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) == RESET) {
        if ((HAL_GetTick() - t1) > 2U) break;
      }
      // ModbusRTU_TxCpltISR(&g_mb2);  // 函数未定义
      return;
    }
  #endif
  /* USER CODE END HAL_UART_TxCpltCallback 0 */
  
  /* USER CODE BEGIN HAL_UART_TxCpltCallback 1 */
  /* USER CODE END HAL_UART_TxCpltCallback 1 */
}

/**
  * @brief  UART Error callback
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  /* 清错误标志：读 SR/DR */
  volatile uint32_t sr = huart->Instance->SR; (void)sr;
  volatile uint32_t dr = huart->Instance->DR; (void)dr;

  if (huart == &huart1) {
    /* Modbus 串口2 恢复接收 */
    HAL_UART_Receive_DMA(&huart1, g_mb.rxBuffer, MB_RTU_FRAME_MAX_SIZE);
    return;
  }
  if (huart == &huart2 && USART2_TEST_MODE == 0) {
    /* Modbus 串口1 恢复接收 */
    HAL_UART_Receive_DMA(&huart2, g_mb2.rxBuffer, MB_RTU_FRAME_MAX_SIZE);
    return;
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
