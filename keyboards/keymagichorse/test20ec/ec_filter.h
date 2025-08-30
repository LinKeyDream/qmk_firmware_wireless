#pragma once
#include "util.h"
#include <string.h>

#define FILTER_DATAS_SIZE   32

typedef struct
{
    uint16_t filter_datas[FILTER_DATAS_SIZE];
    uint8_t head;
    uint8_t tail;   // 滤波后数据环形缓冲区

    float a;        // 平滑系数
    float b;        // 趋势系数
    float filtered_value; // 上一次滤波后的值
    float trend;          // 趋势值
    // α:越小，越平滑，但响应越慢。越大，对正常噪声敏感，容易“跳”。 0.05~0.1
    // b:用来对突变时放宽响应速度。相当于给突变一个“更快通道”。0.15~0.25
    // α、β 参数越大，滤波会更快，但噪声也会增大； α、β 参数越小，滤波后的值更平滑
} ec_filter_RingBuffer;


void ec_filter_initqueue(ec_filter_RingBuffer *efb)
{
    memset(efb->filter_datas, 0, sizeof(efb->filter_datas));
    efb->head           = 0;
    efb->tail           = 0;
    efb->a              = 0.05f;
    efb->b              = 0.15f;
    efb->filtered_value = 0;
    efb->trend          = 0;
}

// 获取当前环形缓冲区有效数据个数
uint8_t ec_filter_get_count(ec_filter_RingBuffer *efb) {
    uint8_t count = 0;
    if (efb->head >= efb->tail) {
        count = efb->head - efb->tail;
    } else {
        count = FILTER_DATAS_SIZE - efb->tail + efb->head;
    }
    return count;
}


// αβ 滤波处理函数
void ec_filter_alpha_beta(ec_filter_RingBuffer *efb, uint16_t new_adc_value)
{
    // 1. 预测
    float predicted_value = efb->filtered_value + efb->trend;

    // 2. 校正
    float residual = (float)new_adc_value - predicted_value;
    efb->filtered_value = predicted_value + efb->a * residual;

    // 3. 更新趋势
    efb->trend = efb->trend + efb->b * (residual / 1.0f); // 假设采样间隔为1

    // 4. 将新滤波数据存入环形缓冲区
    efb->filter_datas[efb->head] = (uint16_t)efb->filtered_value;
    efb->head = (efb->head + 1) % FILTER_DATAS_SIZE;
    if (efb->head == efb->tail) {
        efb->tail = (efb->tail + 1) % FILTER_DATAS_SIZE;
    }
}

// 计算缓冲区中的去极值平均值
uint16_t ec_filter_get_avg_value(ec_filter_RingBuffer *efb)
{
    // 获取有效数据个数
    uint8_t count = ec_filter_get_count(efb);
    // 如果缓冲区为空或数据太少，直接返回
    if (count == 0) {
        return 0;
    }

    uint32_t sum = 0;
    uint16_t min_val = 4096;
    uint16_t max_val = 0;

    // 使用 for 循环遍历所有有效数据
    for (uint8_t i = 0; i < count; i++) {
        uint8_t current_index = (efb->tail + i) % FILTER_DATAS_SIZE;
        uint16_t data = efb->filter_datas[current_index];
        
        sum += data;
        if (data < min_val) {
            min_val = data;
        }
        if (data > max_val) {
            max_val = data;
        }
    }

    // 根据数据量进行平均值计算
    if (count > 2) {
        // 去掉最大值和最小值后求平均值
        sum = sum - min_val - max_val;
        return (uint16_t)(sum / (count - 2));
    } else {
        // 如果数据量少于3个，直接求平均
        return (uint16_t)(sum / count);
    }
}