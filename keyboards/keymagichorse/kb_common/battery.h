/* Copyright 2025 keymagichorse
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include "quantum.h"

// 电池电压最高最低 mv
#ifndef BATTERY_MAX_MV                       
#    define BATTERY_MAX_MV     4100
#endif
#ifndef BATTERY_MIN_MV                      
#    define BATTERY_MIN_MV     3500
#endif

// ------------------------ 电池分压电阻的配置 ------------------------
/* Battery voltage resistive voltage divider setting of MCU */
#ifndef BAT_R_UPPER                        
// Upper side resitor value (uint: KΩ)
#   define BAT_R_UPPER 100  
#endif
#ifndef BAT_R_LOWER    
 // Lower side resitor value (uint: KΩ)                   
#   define BAT_R_LOWER 100         
#endif
// ------------------------ 电池分压电阻的配置 ------------------------

// ------------------------ 电池电压读取的引脚 ------------------------
#ifndef BATTERY_ADC_PIN                       
#    define BATTERY_ADC_PIN     B1
#endif
// https://docs.qmk.fm/drivers/adc#stm32
#ifndef BATTERY_ADC_DRIVER                      
#    define BATTERY_ADC_DRIVER     ADCD1
#endif
// ------------------------ 电池电压读取的引脚 ------------------------

// ------------------------ VREFINT 校准配置 ------------------------
// 使用 STM32 内部参考电压通道(VREFINT)动态测量 VDDA，
// 消除 LDO dropout 导致 VDDA 偏离 3.3V 时的 ADC 误差。
// STM32F4xx: VREFINT 通道为 17，标称 1.21V
// STM32F1xx: VREFINT 通道为 17，标称 1.20V
#if !defined(BATTERY_USE_VREFINT) && (defined(STM32F4XX) || defined(STM32F1XX))
#    define BATTERY_USE_VREFINT
#endif

// VREFINT 标称电压 (mV)，STM32F4xx=1210, STM32F1xx=1200
#ifndef BATTERY_VREFINT_MV
#    ifdef STM32F4XX
#        define BATTERY_VREFINT_MV    1210
#    else
#        define BATTERY_VREFINT_MV    1200
#    endif
#endif

// VREFINT 通道号
#ifndef BATTERY_VREFINT_CHANNEL
#    define BATTERY_VREFINT_CHANNEL  ADC_CHANNEL_VREFINT
#endif

// ADC 满量程值 (10bit=1023, 12bit=4095)
#ifndef BATTERY_ADC_FULLSCALE
#    define BATTERY_ADC_FULLSCALE    1023
#endif
// ------------------------ VREFINT 校准配置 ------------------------

// ------------------------ 低电量保护配置 ------------------------
// 当电池电压低于此阈值 (mV) 时，禁用 RTC 周期唤醒，只保留 USB 插入唤醒
// 3.3V LDO 在此电压以下已无法稳压，ADC 读数不可靠，EC 矩阵可能误触发
#ifndef BATTERY_LOW_MV
#    define BATTERY_LOW_MV     3400
#endif

// RTC 唤醒循环中每 N 次唤醒检查一次电池电压
#ifndef BATTERY_RTC_CHECK_INTERVAL
#    define BATTERY_RTC_CHECK_INTERVAL  10
#endif
// ------------------------ 低电量保护配置 ------------------------



void battery_init(void);
void battery_task(void);
void battery_reset_timer(void);

// 获取电池百分比 (0~100)，未采样返回 0xFF
uint8_t battery_get_percent(void);
// 兼容旧代码的别名
#define battery_driver_sample_percent() battery_get_percent()

void battery_percent_changed_user(uint8_t level);
void battery_percent_changed_kb(uint8_t level);

// 控制函数
void battery_enable_read(void);
void battery_disable_read(void);
void battery_enable_ble_update(void);
void battery_disable_ble_update(void);

// 获取电池电压 (mV)，由 battery_task 更新
uint16_t battery_get_mv(void);

// 低电量查询：返回 true 表示电池电压过低，应禁用 RTC 唤醒
bool battery_is_low_voltage(void);

// RTC 唤醒循环中使用的轻量级电池检查
// 返回 true 表示电压正常，false 表示电压过低
bool battery_rtc_check_voltage(void);