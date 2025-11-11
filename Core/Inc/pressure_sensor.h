/**
 * @file pressure_sensor.h
 * @brief 压力传感器驱动模块头文件
 * @details 4-20mA压力变送器采集与处理（带卡尔曼滤波）
 * @author Lighting Ultra Team
 * @date 2025-11-11
 * 
 * 硬件连接：
 * - PB0 -> ADC12_IN8 -> 4-20mA采样电阻(140Ω) -> 压力变送器
 * 
 * 工作原理：
 * - 4mA  -> 0.56V   -> 压力下限（如0 MPa）
 * - 20mA -> 2.8V   -> 压力上限（如1.6 MPa）
 * - ADC采样 -> 电压值 -> 电流值 -> 压力值
 * - 卡尔曼滤波平滑数据
 */

#ifndef PRESSURE_SENSOR_H
#define PRESSURE_SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f1xx_hal.h"
#include "kalman.h"

/* ==================== 硬件配置 ==================== */
#define PRESSURE_ADC_INSTANCE       ADC1
#define PRESSURE_ADC_CHANNEL        ADC_CHANNEL_8   /* PB0 -> ADC12_IN8 */
#define PRESSURE_GPIO_PORT          GPIOB
#define PRESSURE_GPIO_PIN           GPIO_PIN_0

/* ==================== 转换参数 ==================== */
#define ADC_RESOLUTION              4096.0f   /* 12位ADC */
#define ADC_VREF                    3.3f      /* 参考电压3.3V */
#define CURRENT_MIN_MA              4.0f      /* 最小电流4mA */
#define CURRENT_MAX_MA              20.0f     /* 最大电流20mA */
#define SAMPLE_RESISTOR_OHM         140.0f    /* 采样电阻140Ω */

/* 电压范围：4mA*140Ω=0.56V, 20mA*140Ω=2.8V（但受ADC Vref限制） */
#define VOLTAGE_MIN                 1.0f      /* 对应4mA */
#define VOLTAGE_MAX                 3.3f      /* ADC最大电压 */

/* 采样配置 */
#define PRESSURE_SAMPLE_INTERVAL_MS 100       /* 采样间隔100ms */
#define PRESSURE_AVERAGE_COUNT      10        /* 移动平均滤波采样数 */

/* ==================== 数据结构 ==================== */
/**
 * @brief 压力传感器配置结构体
 */
typedef struct {
    float pressureMin;    /**< 压力下限（如0 MPa） */
    float pressureMax;    /**< 压力上限（如1.6 MPa） */
    float currentMin;     /**< 电流下限（4mA） */
    float currentMax;     /**< 电流上限（20mA） */
} PressureConfig_t;

/**
 * @brief 压力传感器数据结构体
 */
typedef struct {
    uint16_t adcRaw;          /**< ADC原始值 */
    float voltage;            /**< 电压值（V） */
    float current;            /**< 电流值（mA） */
    float pressureRaw;        /**< 原始压力值（未滤波） */
    float pressureFiltered;   /**< 滤波后压力值 */
    uint32_t sampleCount;     /**< 采样计数 */
    bool isValid;             /**< 数据有效标志 */
} PressureData_t;

/* ==================== 全局变量 ==================== */
extern ADC_HandleTypeDef hadcPressure;

/* ==================== API函数 ==================== */
/**
 * @brief 初始化压力传感器模块
 * @param pressureMin 压力下限（如0.0 MPa）
 * @param pressureMax 压力上限（如1.6 MPa）
 * @return HAL_StatusTypeDef HAL状态码
 */
HAL_StatusTypeDef pressureSensorInit(float pressureMin, float pressureMax);

/**
 * @brief 压力传感器处理函数（在主循环中调用）
 * @details 自动100ms采样一次，非阻塞
 */
void pressureSensorProcess(void);

/**
 * @brief 立即采样一次压力值
 * @return HAL_StatusTypeDef HAL状态码
 */
HAL_StatusTypeDef pressureSensorSample(void);

/**
 * @brief 获取当前压力值（滤波后）
 * @return float 压力值（单位：MPa或用户设置单位）
 */
float pressureSensorGetPressure(void);

/**
 * @brief 获取原始压力值（未滤波）
 * @return float 原始压力值
 */
float pressureSensorGetPressureRaw(void);

/**
 * @brief 获取当前电流值
 * @return float 电流值（mA）
 */
float pressureSensorGetCurrent(void);

/**
 * @brief 获取完整的压力数据
 * @return PressureData_t 压力数据结构体
 */
PressureData_t pressureSensorGetData(void);

/**
 * @brief 检查压力数据是否有效
 * @return bool 数据有效性
 * @retval true 数据有效（电流在4-20mA范围内）
 * @retval false 数据无效（传感器故障或断线）
 */
bool pressureSensorIsValid(void);

/**
 * @brief 重置卡尔曼滤波器
 */
void pressureSensorResetFilter(void);

#endif /* PRESSURE_SENSOR_H */

