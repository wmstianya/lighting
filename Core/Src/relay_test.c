/**
 * @file relay_test.c
 * @brief 继电器功能测试模块实现
 * @details
 * 提供完整的继电器系统测试功能，用于验证硬件连接和功能正确性。
 * 
 * @author Lighting Ultra Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#include "relay_test.h"
#include "relay.h"

//=============================================================================
// 私有变量 (Private Variables)
//=============================================================================

static struct {
    uint32_t totalTests;
    uint32_t passedTests;
    uint32_t failedTests;
    bool relayStatus[RELAY_CHANNEL_COUNT];
} testReport = {0};

//=============================================================================
// 公共函数实现 (Public Function Implementations)  
//=============================================================================

bool relaySystemSelfTest(void)
{
    bool allPassed = true;
    testReport.totalTests = 0;
    testReport.passedTests = 0;
    testReport.failedTests = 0;
    
    // 确保所有继电器初始状态为关闭
    relayTurnOffAll();
    HAL_Delay(100);
    
    // 逐个测试每个继电器
    for (int i = 0; i < RELAY_CHANNEL_COUNT; i++)
    {
        RelayChannel_e channel = (RelayChannel_e)i;
        testReport.totalTests++;
        
        // 测试开启功能
        relaySetState(channel, RELAY_STATE_ON);
        HAL_Delay(100); // 给继电器稳定时间
        
        RelayState_e actualState = relayGetState(channel);
        if (actualState == RELAY_STATE_ON)
        {
            // 测试关闭功能
            HAL_Delay(400); // 保持开启状态400ms
            relaySetState(channel, RELAY_STATE_OFF);
            HAL_Delay(100);
            
            actualState = relayGetState(channel);
            if (actualState == RELAY_STATE_OFF)
            {
                testReport.passedTests++;
                testReport.relayStatus[i] = true;
            }
            else
            {
                testReport.failedTests++;
                testReport.relayStatus[i] = false;
                allPassed = false;
            }
        }
        else
        {
            testReport.failedTests++;
            testReport.relayStatus[i] = false;
            allPassed = false;
            
            // 尝试关闭，避免继电器卡在开启状态
            relaySetState(channel, RELAY_STATE_OFF);
            HAL_Delay(100);
        }
    }
    
    return allPassed;
}

HAL_StatusTypeDef relayRunningLightTest(uint32_t delayMs, uint8_t cycles)
{
    // 确保所有继电器初始关闭
    relayTurnOffAll();
    HAL_Delay(100);
    
    for (uint8_t cycle = 0; cycle < cycles; cycle++)
    {
        // 正向流水灯
        for (int i = 0; i < RELAY_CHANNEL_COUNT; i++)
        {
            RelayChannel_e channel = (RelayChannel_e)i;
            
            relaySetState(channel, RELAY_STATE_ON);
            HAL_Delay(delayMs);
            relaySetState(channel, RELAY_STATE_OFF);
            
            // 检查操作是否成功
            if (relayGetState(channel) != RELAY_STATE_OFF)
            {
                return HAL_ERROR;
            }
        }
        
        // 反向流水灯
        for (int i = RELAY_CHANNEL_COUNT - 1; i >= 0; i--)
        {
            RelayChannel_e channel = (RelayChannel_e)i;
            
            relaySetState(channel, RELAY_STATE_ON);
            HAL_Delay(delayMs);
            relaySetState(channel, RELAY_STATE_OFF);
            
            // 检查操作是否成功
            if (relayGetState(channel) != RELAY_STATE_OFF)
            {
                return HAL_ERROR;
            }
        }
    }
    
    return HAL_OK;
}

HAL_StatusTypeDef relaySyncFlashTest(uint8_t flashCount, uint32_t onTimeMs, uint32_t offTimeMs)
{
    // 确保所有继电器初始关闭
    relayTurnOffAll();
    HAL_Delay(100);
    
    for (uint8_t i = 0; i < flashCount; i++)
    {
        // 全部开启
        relaySetAllStates(0x1F); // 二进制11111，开启所有5个继电器
        
        // 验证状态
        uint8_t actualStates = relayGetAllStates();
        if (actualStates != 0x1F)
        {
            relayTurnOffAll(); // 安全关闭
            return HAL_ERROR;
        }
        
        HAL_Delay(onTimeMs);
        
        // 全部关闭
        relayTurnOffAll();
        
        // 验证状态
        actualStates = relayGetAllStates();
        if (actualStates != 0x00)
        {
            return HAL_ERROR;
        }
        
        HAL_Delay(offTimeMs);
    }
    
    return HAL_OK;
}

uint32_t relayResponseTimeTest(RelayChannel_e channel, uint8_t testCount)
{
    if (testCount == 0 || channel >= RELAY_CHANNEL_COUNT)
    {
        return 0;
    }
    
    uint32_t totalTime = 0;
    
    // 确保继电器初始关闭
    relaySetState(channel, RELAY_STATE_OFF);
    HAL_Delay(100);
    
    for (uint8_t i = 0; i < testCount; i++)
    {
        uint32_t startTime = HAL_GetTick();
        
        // 开启继电器
        relaySetState(channel, RELAY_STATE_ON);
        
        // 等待状态确认
        while (relayGetState(channel) != RELAY_STATE_ON)
        {
            // 添加超时保护
            if ((HAL_GetTick() - startTime) > 1000) // 1秒超时
            {
                return 0; // 测试失败
            }
        }
        
        uint32_t endTime = HAL_GetTick();
        totalTime += (endTime - startTime) * 1000; // 转换为微秒
        
        // 关闭继电器，准备下次测试
        relaySetState(channel, RELAY_STATE_OFF);
        HAL_Delay(100);
    }
    
    return totalTime / testCount; // 返回平均时间
}

void relayPrintTestReport(void)
{
    // 注意：此函数需要printf重定向到调试串口才能工作
    // 如果没有printf，可以使用HAL_UART_Transmit发送调试信息
    
    #ifdef DEBUG
    printf("\r\n========== 继电器测试报告 ==========\r\n");
    printf("总测试数: %lu\r\n", testReport.totalTests);
    printf("通过数: %lu\r\n", testReport.passedTests);
    printf("失败数: %lu\r\n", testReport.failedTests);
    printf("通过率: %.1f%%\r\n", 
           (float)testReport.passedTests * 100.0f / testReport.totalTests);
    
    printf("\r\n详细结果:\r\n");
    for (int i = 0; i < RELAY_CHANNEL_COUNT; i++)
    {
        printf("继电器%d (P%c%d): %s\r\n", 
               i + 1,
               (i < 2) ? 'B' : 'A',  // PB4,PB3 or PA15,PA12,PA11
               (i == 0) ? 4 : (i == 1) ? 3 : (i == 2) ? 15 : (i == 3) ? 12 : 11,
               testReport.relayStatus[i] ? "通过" : "失败");
    }
    printf("=====================================\r\n");
    #endif
}
