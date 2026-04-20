/**
 * @file kalman.h
 * @brief 卡尔曼滤波器头文件
 * @details 一维卡尔曼滤波器，用于传感器数据平滑处理
 * @author Lighting Ultra Team
 * @date 2025-11-11
 */

#ifndef KALMAN_H
#define KALMAN_H

#include <stdint.h>
#include <stdbool.h>

/* ==================== 数据结构 ==================== */
/**
 * @brief 卡尔曼滤波器状态结构体
 */
typedef struct {
    float x;          /**< 系统状态估计值 */
    float p;          /**< 估计误差协方差 */
    float q;          /**< 过程噪声协方差 */
    float r;          /**< 测量噪声协方差 */
    float k;          /**< 卡尔曼增益 */
    bool isInit;      /**< 初始化标志 */
} KalmanFilter_t;

/* ==================== API函数 ==================== */
/**
 * @brief 初始化卡尔曼滤波器
 * @param kf 卡尔曼滤波器指针
 * @param processNoise 过程噪声协方差Q（建议0.001~0.01）
 * @param measureNoise 测量噪声协方差R（建议0.1~10）
 * @param estimateError 初始估计误差协方差P（建议1.0）
 * @param initialValue 初始估计值
 * @note Q值越小，滤波越平滑但响应越慢；R值越小，越信任测量值
 */
void kalmanInit(KalmanFilter_t* kf, float processNoise, float measureNoise, 
                float estimateError, float initialValue);

/**
 * @brief 卡尔曼滤波更新
 * @param kf 卡尔曼滤波器指针
 * @param measurement 测量值（原始ADC转换后的值）
 * @return float 滤波后的估计值
 */
float kalmanUpdate(KalmanFilter_t* kf, float measurement);

/**
 * @brief 重置卡尔曼滤波器
 * @param kf 卡尔曼滤波器指针
 */
void kalmanReset(KalmanFilter_t* kf);

/**
 * @brief 获取当前估计值
 * @param kf 卡尔曼滤波器指针
 * @return float 当前估计值
 */
float kalmanGetValue(KalmanFilter_t* kf);

#endif /* KALMAN_H */

