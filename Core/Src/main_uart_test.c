/**
 * @file main_uart_test.c
 * @brief 串口综合测试主程序
 * @details
 * 专门用于串口和Modbus通信测试的主程序
 * 提供多种测试模式，可通过串口命令控制
 * 
 * 使用方法：
 * 1. 将此文件替换main.c或在Keil中选择此文件作为主程序
 * 2. 编译下载到STM32F103C8
 * 3. 使用串口工具连接，发送测试命令
 * 
 * 测试命令：
 * - 't1' : 执行基础环回测试
 * - 't2' : 执行模式测试（0x55/0xAA）
 * - 't3' : 执行压力测试
 * - 't4' : 执行Modbus功能测试
 * - 't5' : 执行完整测试套件
 * - 'r'  : 显示测试报告
 * - 'c'  : 清除测试统计
 * 
 * @author Lighting Ultra Team
 * @date 2025-01-20
 * @version 2.0.0
 */

#include "main.h"
#include "uart_test.h"
#include "relay.h"
#include "../../MDK-ARM/modbus_rtu_slave.h"
#include <string.h>
#include <stdio.h>

//=============================================================================
// 全局变量 (Global Variables)
//=============================================================================

// HAL句柄
UART_HandleTypeDef huart1;  // 测试串口 (PA9/PA10)
UART_HandleTypeDef huart2;  // 调试串口 (PA2/PA3)
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;

// Modbus实例
ModbusRTU_Slave g_mb;   // 串口1 Modbus实例
ModbusRTU_Slave g_mb2;  // 串口2 Modbus实例

// 测试控制
static uint8_t cmdBuffer[32];
static uint8_t cmdLength = 0;
static bool cmdReady = false;
static uint8_t currentTestMode = 0;

//=============================================================================
// 函数声明 (Function Prototypes)
//=============================================================================

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void processCommand(void);
static void displayMenu(void);
static void runBasicLoopbackTest(void);
static void runPatternTest(void);
static void runStressTest(void);
static void runModbusTest(void);
static void runFullTestSuite(void);

//=============================================================================
// 主程序 (Main Program)
//=============================================================================

int main(void)
{
    // 系统初始化
    HAL_Init();
    SystemClock_Config();
    
    // 外设初始化
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();  // 测试串口
    MX_USART2_UART_Init();  // 调试串口
    
    // 继电器初始化
    relayInit();
    
    // 启动指示：所有继电器快闪3次
    for (int i = 0; i < 3; i++) {
        relaySetAllStates(0x1F);
        HAL_Delay(200);
        relayTurnOffAll();
        HAL_Delay(200);
    }
    
    // 初始化UART测试模块
    UartTestConfig_t testConfig = {
        .huart = &huart1,
        .baudRate = 9600,
        .timeout = 1000,
        .mode = TEST_MODE_LOOPBACK,
        .useRS485 = true,
        .rs485Port = GPIOA,
        .rs485Pin = GPIO_PIN_8  // PA8控制USART1的RS485
    };
    
    if (uartTestInit(&testConfig) != HAL_OK) {
        Error_Handler();
    }
    
    // 初始化Modbus从站
    ModbusRTU_Init(&g_mb, &huart1, 0x01);  // 从站地址01
    ModbusRTU_Init(&g_mb2, &huart2, 0x02); // 从站地址02
    
    // 设置测试数据
    g_mb.holdingRegs[0] = 0x1234;
    g_mb.holdingRegs[1] = 0x5678;
    g_mb.holdingRegs[2] = 0xABCD;
    
    // 显示欢迎信息和菜单
    char msg[] = "\r\n==== STM32 UART测试系统 V2.0 ====\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
    displayMenu();
    
    // 启动调试串口接收
    HAL_UART_Receive_IT(&huart2, cmdBuffer, 1);
    
    // 主循环
    while (1) {
        // 处理Modbus请求
        ModbusRTU_Process(&g_mb);
        ModbusRTU_Process(&g_mb2);
        
        // 处理测试命令
        if (cmdReady) {
            processCommand();
            cmdReady = false;
            cmdLength = 0;
        }
        
        // 根据当前测试模式执行相应操作
        switch (currentTestMode) {
            case 1: // 连续环回测试
                {
                    static uint32_t lastTestTime = 0;
                    if (HAL_GetTick() - lastTestTime > 1000) {
                        runBasicLoopbackTest();
                        lastTestTime = HAL_GetTick();
                    }
                }
                break;
                
            case 2: // 自动模式测试
                {
                    static uint32_t lastTestTime = 0;
                    if (HAL_GetTick() - lastTestTime > 2000) {
                        runPatternTest();
                        lastTestTime = HAL_GetTick();
                    }
                }
                break;
                
            default:
                // 空闲模式，仅处理Modbus
                break;
        }
        
        // 状态指示：继电器1慢闪表示系统运行
        static uint32_t ledTime = 0;
        static bool ledState = false;
        if (HAL_GetTick() - ledTime > 500) {
            ledState = !ledState;
            relaySetState(RELAY_CHANNEL_FIRST, 
                         ledState ? RELAY_STATE_ON : RELAY_STATE_OFF);
            ledTime = HAL_GetTick();
        }
        
        HAL_Delay(1);
    }
}

//=============================================================================
// 测试功能实现 (Test Function Implementation)
//=============================================================================

/**
 * @brief 显示菜单
 */
static void displayMenu(void)
{
    char menu[] = 
        "\r\n测试菜单:\r\n"
        "  1 - 基础环回测试\r\n"
        "  2 - 模式测试(0x55/0xAA)\r\n"
        "  3 - 压力测试(10秒)\r\n"
        "  4 - Modbus功能测试\r\n"
        "  5 - 完整测试套件\r\n"
        "  r - 显示测试报告\r\n"
        "  c - 清除统计数据\r\n"
        "  m - 显示菜单\r\n"
        "  s - 停止连续测试\r\n"
        "请输入命令: ";
    
    HAL_UART_Transmit(&huart2, (uint8_t*)menu, strlen(menu), 100);
}

/**
 * @brief 处理命令
 */
static void processCommand(void)
{
    char response[128];
    
    switch (cmdBuffer[0]) {
        case '1':
            sprintf(response, "\r\n开始基础环回测试...\r\n");
            HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
            runBasicLoopbackTest();
            currentTestMode = 0;
            break;
            
        case '2':
            sprintf(response, "\r\n开始模式测试...\r\n");
            HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
            runPatternTest();
            currentTestMode = 0;
            break;
            
        case '3':
            sprintf(response, "\r\n开始压力测试(10秒)...\r\n");
            HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
            runStressTest();
            currentTestMode = 0;
            break;
            
        case '4':
            sprintf(response, "\r\n开始Modbus功能测试...\r\n");
            HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
            runModbusTest();
            currentTestMode = 0;
            break;
            
        case '5':
            sprintf(response, "\r\n开始完整测试套件...\r\n");
            HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
            runFullTestSuite();
            currentTestMode = 0;
            break;
            
        case 'r':
        case 'R':
            uartTestPrintReport(&huart2);
            break;
            
        case 'c':
        case 'C':
            uartTestResetStats();
            sprintf(response, "\r\n统计数据已清除\r\n");
            HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
            break;
            
        case 'm':
        case 'M':
            displayMenu();
            break;
            
        case 's':
        case 'S':
            currentTestMode = 0;
            sprintf(response, "\r\n测试已停止\r\n");
            HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
            break;
            
        default:
            sprintf(response, "\r\n无效命令: %c\r\n", cmdBuffer[0]);
            HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
            break;
    }
}

/**
 * @brief 基础环回测试
 */
static void runBasicLoopbackTest(void)
{
    char testStr[] = "Hello UART Test 12345!";
    TestResult_e result = uartTestLoopback((uint8_t*)testStr, strlen(testStr), 1000);
    
    char response[128];
    if (result == TEST_RESULT_PASS) {
        sprintf(response, "环回测试: 通过 ✓\r\n");
        relaySetState(RELAY_CHANNEL_THIRD, RELAY_STATE_ON);
        HAL_Delay(100);
        relaySetState(RELAY_CHANNEL_THIRD, RELAY_STATE_OFF);
    } else {
        sprintf(response, "环回测试: 失败 ✗ (结果=%d)\r\n", result);
        relaySetState(RELAY_CHANNEL_FOURTH, RELAY_STATE_ON);
        HAL_Delay(100);
        relaySetState(RELAY_CHANNEL_FOURTH, RELAY_STATE_OFF);
    }
    HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
}

/**
 * @brief 模式测试
 */
static void runPatternTest(void)
{
    char response[128];
    
    // 测试0x55模式
    sprintf(response, "测试模式0x55...\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
    TestResult_e result1 = uartTestPattern(0x55, 64, 10);
    
    // 测试0xAA模式
    sprintf(response, "测试模式0xAA...\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
    TestResult_e result2 = uartTestPattern(0xAA, 64, 10);
    
    // 测试0xFF模式
    sprintf(response, "测试模式0xFF...\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
    TestResult_e result3 = uartTestPattern(0xFF, 32, 5);
    
    if (result1 == TEST_RESULT_PASS && 
        result2 == TEST_RESULT_PASS && 
        result3 == TEST_RESULT_PASS) {
        sprintf(response, "模式测试: 全部通过 ✓\r\n");
    } else {
        sprintf(response, "模式测试: 部分失败 (0x55=%d, 0xAA=%d, 0xFF=%d)\r\n",
                result1, result2, result3);
    }
    HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
}

/**
 * @brief 压力测试
 */
static void runStressTest(void)
{
    char response[128];
    TestResult_e result = uartTestStress(10, 256, 10); // 10秒压力测试
    
    if (result == TEST_RESULT_PASS) {
        sprintf(response, "压力测试: 通过 ✓\r\n");
    } else {
        sprintf(response, "压力测试: 失败 ✗ (结果=%d)\r\n", result);
    }
    HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
    
    // 显示压力测试统计
    uartTestPrintReport(&huart2);
}

/**
 * @brief Modbus功能测试
 */
static void runModbusTest(void)
{
    char response[128];
    
    // 初始化Modbus测试
    modbusTestSlaveInit(0x01);
    
    // 测试响应时间
    sprintf(response, "测试Modbus响应时间...\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
    
    uint32_t avgTime = modbusTestResponseTime(0x01, 20);
    
    if (avgTime > 0) {
        sprintf(response, "Modbus平均响应时间: %lu us\r\n", avgTime);
    } else {
        sprintf(response, "Modbus测试失败\r\n");
    }
    HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
    
    // 测试继电器控制
    sprintf(response, "测试继电器控制...\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
    
    TestResult_e result = modbusTestRelayControl(0x01, 0x15); // 开启继电器1,3,5
    HAL_Delay(500);
    modbusTestRelayControl(0x01, 0x0A); // 开启继电器2,4
    HAL_Delay(500);
    modbusTestRelayControl(0x01, 0x00); // 全部关闭
    
    if (result == TEST_RESULT_PASS) {
        sprintf(response, "继电器控制测试: 通过 ✓\r\n");
    } else {
        sprintf(response, "继电器控制测试: 失败 ✗\r\n");
    }
    HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
}

/**
 * @brief 完整测试套件
 */
static void runFullTestSuite(void)
{
    char response[128];
    
    sprintf(response, "执行完整测试套件...\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
    
    // 运行所有测试
    TestResult_e result = uartTestRunSuite(0x0F); // 所有测试
    
    if (result == TEST_RESULT_PASS) {
        sprintf(response, "\r\n★ 完整测试套件: 全部通过 ★\r\n");
        // 成功指示：所有继电器依次点亮
        for (int i = 0; i < RELAY_CHANNEL_COUNT; i++) {
            relaySetState((RelayChannel_e)i, RELAY_STATE_ON);
            HAL_Delay(200);
        }
        HAL_Delay(500);
        relayTurnOffAll();
    } else {
        sprintf(response, "\r\n✗ 完整测试套件: 有失败项 ✗\r\n");
        // 失败指示：所有继电器闪烁
        for (int i = 0; i < 3; i++) {
            relaySetAllStates(0x1F);
            HAL_Delay(200);
            relayTurnOffAll();
            HAL_Delay(200);
        }
    }
    HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
    
    // 打印详细报告
    uartTestPrintReport(&huart2);
}

//=============================================================================
// HAL回调函数 (HAL Callback Functions)
//=============================================================================
// 注意：HAL_UART_TxCpltCallback已在stm32f1xx_it.c中定义
// 这里只处理接收回调

/**
 * @brief UART接收回调
 * 注：如果stm32f1xx_it.c中已有此函数，请注释掉此处定义
 */
#ifndef UART_CALLBACKS_IN_IT
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) {
        // 调试串口接收命令
        if (cmdBuffer[cmdLength] == '\r' || cmdBuffer[cmdLength] == '\n') {
            cmdReady = true;
        } else {
            cmdLength++;
            if (cmdLength >= sizeof(cmdBuffer) - 1) {
                cmdLength = 0;
            }
        }
        // 继续接收
        HAL_UART_Receive_IT(&huart2, &cmdBuffer[cmdLength], 1);
    }
}
#endif

//=============================================================================
// 系统配置函数 (System Configuration Functions)
//=============================================================================

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    // 配置时钟源
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    // 配置系统时钟
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // 使能GPIO时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // 配置RS485控制引脚
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // 初始为接收模式
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_8, GPIO_PIN_RESET);
    
    // 配置UART引脚
    // USART1: PA9(TX), PA10(RX)
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // USART2: PA2(TX), PA3(RX)
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

static void MX_DMA_Init(void)
{
    // DMA时钟使能
    __HAL_RCC_DMA1_CLK_ENABLE();

    // DMA中断配置
    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 1, 1);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 1, 2);
    HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
    HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 1, 3);
    HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
}

static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 9600;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
    
    // 配置DMA
    hdma_usart1_rx.Instance = DMA1_Channel5;
    hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode = DMA_NORMAL;
    hdma_usart1_rx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(&huart1, hdmarx, hdma_usart1_rx);
    
    hdma_usart1_tx.Instance = DMA1_Channel4;
    hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_tx.Init.Mode = DMA_NORMAL;
    hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(&huart1, hdmatx, hdma_usart1_tx);
    
    // USART1中断配置
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void MX_USART2_UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;  // 调试串口使用高速
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
    
    // USART2中断配置
    HAL_NVIC_SetPriority(USART2_IRQn, 0, 1);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

void Error_Handler(void)
{
    __disable_irq();
    
    // 错误指示：所有继电器快速闪烁
    while (1) {
        relaySetAllStates(0x1F);
        HAL_Delay(100);
        relayTurnOffAll();
        HAL_Delay(100);
    }
}
