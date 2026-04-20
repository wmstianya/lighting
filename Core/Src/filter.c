/**
 * @file filter.c
 * @brief 数字滤波器模块实现
 * @details 实现中值滤波和IIR低通滤波算法
 * @author Lighting Ultra Team
 * @date 2025-11-13
 * 
 * 修订历史：
 * - 2025-11-13: 初始版本
 * 
 * 算法说明：
 * 1. 中值滤波：
 *    - 对窗口内数据排序，取中值
 *    - 有效去除脉冲噪声和电磁干扰
 *    - 适合工业现场环境
 * 
 * 2. IIR低通滤波：
 *    - 公式：y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
 *    - alpha越小越平滑，越大响应越快
 *    - 计算简单，内存占用小
 */

#include "filter.h"
#include <string.h>

/* ==================== 私有函数 ==================== */
/**
 * @brief 对数组进行冒泡排序
 * @param arr 待排序数组
 * @param size 数组大小
 */
static void bubbleSort(float* arr, uint8_t size)
{
    for (uint8_t i = 0; i < size - 1; i++) {
        for (uint8_t j = 0; j < size - 1 - i; j++) {
            if (arr[j] > arr[j + 1]) {
                float temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

/**
 * @brief 计算数组中值
 * @param arr 已排序数组
 * @param size 数组大小
 * @return float 中值
 */
static float getMedian(float* arr, uint8_t size)
{
    if (size == 0) {
        return 0.0f;
    }
    
    return arr[size / 2];
}

/* ==================== 中值滤波器实现 ==================== */
void medianFilterInit(MedianFilter_t* filter)
{
    memset(filter->buffer, 0, sizeof(filter->buffer));
    filter->index = 0;
    filter->isFull = false;
}

float medianFilterUpdate(MedianFilter_t* filter, float newValue)
{
    /* 添加新值到缓冲区 */
    filter->buffer[filter->index] = newValue;
    filter->index++;
    
    /* 检查缓冲区是否已满 */
    if (filter->index >= MEDIAN_FILTER_SIZE) {
        filter->index = 0;
        filter->isFull = true;
    }
    
    /* 确定有效数据长度 */
    uint8_t validSize = filter->isFull ? MEDIAN_FILTER_SIZE : filter->index;
    
    /* 复制到临时数组进行排序 */
    float sorted[MEDIAN_FILTER_SIZE];
    memcpy(sorted, filter->buffer, validSize * sizeof(float));
    
    /* 排序并返回中值 */
    bubbleSort(sorted, validSize);
    return getMedian(sorted, validSize);
}

void medianFilterReset(MedianFilter_t* filter)
{
    medianFilterInit(filter);
}

/* ==================== IIR低通滤波器实现 ==================== */
void iirFilterInit(IirFilter_t* filter, float alpha, float initialValue)
{
    filter->y = initialValue;
    filter->alpha = alpha;
    filter->isInit = true;
}

float iirFilterUpdate(IirFilter_t* filter, float x)
{
    if (!filter->isInit) {
        filter->y = x;
        filter->isInit = true;
        return x;
    }
    
    /* IIR滤波公式：y[n] = alpha * x[n] + (1 - alpha) * y[n-1] */
    filter->y = filter->alpha * x + (1.0f - filter->alpha) * filter->y;
    
    return filter->y;
}

void iirFilterReset(IirFilter_t* filter)
{
    filter->y = 0.0f;
    filter->isInit = false;
}

float iirFilterGetValue(IirFilter_t* filter)
{
    return filter->y;
}

