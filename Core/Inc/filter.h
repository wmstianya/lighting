/**
 * @file filter.h
 * @brief 数字滤波器模块头文件
 * @details 提供中值滤波和IIR低通滤波算法
 * @author Lighting Ultra Team
 * @date 2025-11-13
 * 
 * 修订历史：
 * - 2025-11-13: 初始版本，支持中值滤波和IIR滤波
 */

#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>
#include <stdbool.h>

/* ==================== 配置参数 ==================== */
#define MEDIAN_FILTER_SIZE      5     /**< 中值滤波窗口大小（必须为奇数） */

/* ==================== 数据结构 ==================== */
/**
 * @brief 中值滤波器结构体
 * @details 用于去除脉冲噪声和电磁干扰
 */
typedef struct {
    float buffer[MEDIAN_FILTER_SIZE];  /**< 滑动窗口缓冲区 */
    uint8_t index;                     /**< 当前写入索引 */
    bool isFull;                       /**< 缓冲区是否已满 */
} MedianFilter_t;

/**
 * @brief 一阶IIR低通滤波器结构体
 * @details 指数移动平均滤波器，计算简单，效果接近卡尔曼滤波
 */
typedef struct {
    float y;          /**< 当前输出值 */
    float alpha;      /**< 平滑系数（0-1），越小越平滑 */
    bool isInit;      /**< 初始化标志 */
} IirFilter_t;

/* ==================== API函数 ==================== */

/* ---------- 中值滤波器 ---------- */
/**
 * @brief 初始化中值滤波器
 * @param filter 中值滤波器指针
 */
void medianFilterInit(MedianFilter_t* filter);

/**
 * @brief 中值滤波更新
 * @param filter 中值滤波器指针
 * @param newValue 新的测量值
 * @return float 滤波后的中值
 * @note 初始阶段（缓冲区未满）返回当前缓冲区的中值
 */
float medianFilterUpdate(MedianFilter_t* filter, float newValue);

/**
 * @brief 重置中值滤波器
 * @param filter 中值滤波器指针
 */
void medianFilterReset(MedianFilter_t* filter);

/* ---------- IIR低通滤波器 ---------- */
/**
 * @brief 初始化IIR滤波器
 * @param filter IIR滤波器指针
 * @param alpha 平滑系数（0-1）
 *              - 建议值：0.1~0.3（0.1最平滑，0.3快速响应）
 *              - alpha = 0.2: 噪声较大场景
 *              - alpha = 0.3: 平衡型（推荐）
 *              - alpha = 0.5: 快速响应
 * @param initialValue 初始输出值
 */
void iirFilterInit(IirFilter_t* filter, float alpha, float initialValue);

/**
 * @brief IIR滤波更新
 * @param filter IIR滤波器指针
 * @param x 新的测量值
 * @return float 滤波后的值
 * @note 滤波公式：y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
 */
float iirFilterUpdate(IirFilter_t* filter, float x);

/**
 * @brief 重置IIR滤波器
 * @param filter IIR滤波器指针
 */
void iirFilterReset(IirFilter_t* filter);

/**
 * @brief 获取当前IIR滤波输出值
 * @param filter IIR滤波器指针
 * @return float 当前输出值
 */
float iirFilterGetValue(IirFilter_t* filter);

#endif /* FILTER_H */

