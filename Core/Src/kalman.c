/**
 * @file kalman.c
 * @brief 卡尔曼滤波器实现
 * @details 一维卡尔曼滤波算法实现，用于平滑ADC采样噪声
 * @author Lighting Ultra Team
 * @date 2025-11-11
 * 
 * 卡尔曼滤波原理：
 * 1. 预测：x_pred = x
 *          p_pred = p + q
 * 2. 更新：k = p_pred / (p_pred + r)
 *          x = x_pred + k * (measurement - x_pred)
 *          p = (1 - k) * p_pred
 */

#include "kalman.h"

/* ==================== API函数实现 ==================== */
void kalmanInit(KalmanFilter_t* kf, float processNoise, float measureNoise,
                float estimateError, float initialValue)
{
    kf->x = initialValue;
    kf->p = estimateError;
    kf->q = processNoise;
    kf->r = measureNoise;
    kf->k = 0.0f;
    kf->isInit = true;
}

float kalmanUpdate(KalmanFilter_t* kf, float measurement)
{
    if (!kf->isInit) {
        /* 未初始化，直接返回测量值 */
        return measurement;
    }
    
    /* 预测步骤 */
    float x_pred = kf->x;              /* 状态预测 */
    float p_pred = kf->p + kf->q;      /* 误差协方差预测 */
    
    /* 更新步骤 */
    kf->k = p_pred / (p_pred + kf->r); /* 计算卡尔曼增益 */
    kf->x = x_pred + kf->k * (measurement - x_pred);  /* 状态更新 */
    kf->p = (1.0f - kf->k) * p_pred;   /* 误差协方差更新 */
    
    return kf->x;
}

void kalmanReset(KalmanFilter_t* kf)
{
    kf->x = 0.0f;
    kf->p = 1.0f;
    kf->k = 0.0f;
    kf->isInit = false;
}

float kalmanGetValue(KalmanFilter_t* kf)
{
    return kf->x;
}

