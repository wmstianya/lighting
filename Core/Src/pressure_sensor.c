/**
 * @file pressure_sensor.c
 * @brief 压力传感器驱动模块实现
 * @details 4-20mA压力变送器ADC采集与卡尔曼滤波处理
 * @author Lighting Ultra Team
 * @date 2025-11-11
 */

#include "pressure_sensor.h"
#include <math.h>

/* ==================== 私有变量 ==================== */
ADC_HandleTypeDef hadcPressure;
static PressureConfig_t pressureConfig;
static PressureData_t pressureData;
static KalmanFilter_t kalmanFilter;
static uint32_t lastSampleTick = 0;

/* ==================== 私有函数 ==================== */
/**
 * @brief 初始化ADC1用于压力采集
 */
static HAL_StatusTypeDef initAdc(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    
    /* 使能ADC1和GPIOB时钟 */
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    /* 配置PB0为模拟输入 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = PRESSURE_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(PRESSURE_GPIO_PORT, &GPIO_InitStruct);
    
    /* 配置ADC1 */
    hadcPressure.Instance = PRESSURE_ADC_INSTANCE;
    hadcPressure.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadcPressure.Init.ContinuousConvMode = DISABLE;
    hadcPressure.Init.DiscontinuousConvMode = DISABLE;
    hadcPressure.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadcPressure.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadcPressure.Init.NbrOfConversion = 1;
    
    if (HAL_ADC_Init(&hadcPressure) != HAL_OK) {
        return HAL_ERROR;
    }
    
    /* 配置ADC通道 */
    sConfig.Channel = PRESSURE_ADC_CHANNEL;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;  /* 慢速采样，提高精度 */
    
    if (HAL_ADC_ConfigChannel(&hadcPressure, &sConfig) != HAL_OK) {
        return HAL_ERROR;
    }
    
    /* ADC校准 */
    HAL_ADCEx_Calibration_Start(&hadcPressure);
    
    return HAL_OK;
}

/**
 * @brief ADC值转换为电压
 * @param adcValue ADC原始值
 * @return float 电压值（V）
 */
static float adcToVoltage(uint16_t adcValue)
{
    return ((float)adcValue / ADC_RESOLUTION) * ADC_VREF;
}

/**
 * @brief 电压转换为电流
 * @param voltage 电压值（V）
 * @return float 电流值（mA）
 */
static float voltageToCurrent(float voltage)
{
    /* I = V / R，其中R = 250Ω */
    return (voltage / SAMPLE_RESISTOR_OHM) * 1000.0f;  /* 转换为mA */
}

/**
 * @brief 电流转换为压力
 * @param current 电流值（mA）
 * @return float 压力值
 */
static float currentToPressure(float current)
{
    /* 线性插值：pressure = pressureMin + (current - 4mA) * (pressureMax - pressureMin) / (20mA - 4mA) */
    if (current < CURRENT_MIN_MA) {
        current = CURRENT_MIN_MA;
    }
    if (current > CURRENT_MAX_MA) {
        current = CURRENT_MAX_MA;
    }
    
    float ratio = (current - pressureConfig.currentMin) / 
                  (pressureConfig.currentMax - pressureConfig.currentMin);
    
    return pressureConfig.pressureMin + ratio * 
           (pressureConfig.pressureMax - pressureConfig.pressureMin);
}

/**
 * @brief 检查电流值是否有效
 * @param current 电流值（mA）
 * @return bool 有效性
 */
static bool isCurrentValid(float current)
{
    /* 允许一定容差：3.5mA ~ 20.5mA */
    return (current >= 3.5f && current <= 20.5f);
}

/* ==================== API函数实现 ==================== */
HAL_StatusTypeDef pressureSensorInit(float pressureMin, float pressureMax)
{
    /* 保存配置 */
    pressureConfig.pressureMin = pressureMin;
    pressureConfig.pressureMax = pressureMax;
    pressureConfig.currentMin = CURRENT_MIN_MA;
    pressureConfig.currentMax = CURRENT_MAX_MA;
    
    /* 初始化数据 */
    pressureData.adcRaw = 0;
    pressureData.voltage = 0.0f;
    pressureData.current = 0.0f;
    pressureData.pressureRaw = 0.0f;
    pressureData.pressureFiltered = 0.0f;
    pressureData.sampleCount = 0;
    pressureData.isValid = false;
    
    /* 初始化ADC */
    if (initAdc() != HAL_OK) {
        return HAL_ERROR;
    }
    
    /* 初始化卡尔曼滤波器
     * 参数说明：
     * - processNoise = 0.005：过程噪声，压力变化较慢
     * - measureNoise = 0.5：测量噪声，ADC噪声较大
     * - estimateError = 1.0：初始估计误差
     * - initialValue = pressureMin：初始压力值
     */
    kalmanInit(&kalmanFilter, 0.005f, 0.5f, 1.0f, pressureMin);
    
    /* 初始化时间戳 */
    lastSampleTick = HAL_GetTick();
    
    return HAL_OK;
}

void pressureSensorProcess(void)
{
    uint32_t currentTick = HAL_GetTick();
    
    /* 检查是否到达采样间隔 */
    if (currentTick - lastSampleTick >= PRESSURE_SAMPLE_INTERVAL_MS) {
        lastSampleTick = currentTick;
        pressureSensorSample();
    }
}

HAL_StatusTypeDef pressureSensorSample(void)
{
    uint32_t adcSum = 0;
    
    /* 多次采样求平均，提高精度 */
    for (int i = 0; i < PRESSURE_AVERAGE_COUNT; i++) {
        /* 启动ADC转换 */
        HAL_ADC_Start(&hadcPressure);
        
        /* 等待转换完成（最大10ms超时） */
        if (HAL_ADC_PollForConversion(&hadcPressure, 10) != HAL_OK) {
            return HAL_ERROR;
        }
        
        /* 读取ADC值 */
        adcSum += HAL_ADC_GetValue(&hadcPressure);
        
        /* 停止转换 */
        HAL_ADC_Stop(&hadcPressure);
    }
    
    /* 计算平均值 */
    pressureData.adcRaw = adcSum / PRESSURE_AVERAGE_COUNT;
    
    /* ADC -> 电压 */
    pressureData.voltage = adcToVoltage(pressureData.adcRaw);
    
    /* 电压 -> 电流 */
    pressureData.current = voltageToCurrent(pressureData.voltage);
    
    /* 检查电流有效性 */
    pressureData.isValid = isCurrentValid(pressureData.current);
    
    if (pressureData.isValid) {
        /* 电流 -> 压力（原始值） */
        pressureData.pressureRaw = currentToPressure(pressureData.current);
        
        /* 卡尔曼滤波 */
        pressureData.pressureFiltered = kalmanUpdate(&kalmanFilter, pressureData.pressureRaw);
    } else {
        /* 数据无效，保持上一次的滤波值 */
        pressureData.pressureRaw = 0.0f;
    }
    
    /* 更新采样计数 */
    pressureData.sampleCount++;
    
    return HAL_OK;
}

float pressureSensorGetPressure(void)
{
    return pressureData.pressureFiltered;
}

float pressureSensorGetPressureRaw(void)
{
    return pressureData.pressureRaw;
}

float pressureSensorGetCurrent(void)
{
    return pressureData.current;
}

PressureData_t pressureSensorGetData(void)
{
    return pressureData;
}

bool pressureSensorIsValid(void)
{
    return pressureData.isValid;
}

void pressureSensorResetFilter(void)
{
    kalmanReset(&kalmanFilter);
    kalmanInit(&kalmanFilter, 0.005f, 0.5f, 1.0f, pressureConfig.pressureMin);
}

