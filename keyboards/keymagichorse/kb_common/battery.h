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




void battery_init(void);
void battery_task(void);
void battery_reset_timer(void);
uint8_t battery_driver_sample_percent(void);

void battery_percent_changed_user(uint8_t level);
void battery_percent_changed_kb(uint8_t level);

// 控制函数
void battery_enable_read(void);
void battery_disable_read(void);
void battery_enable_ble_update(void);
void battery_disable_ble_update(void);