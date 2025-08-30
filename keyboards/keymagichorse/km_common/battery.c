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
#include "battery.h"
#include "analog.h"
#include "bhq.h"
#include "bhq_common.h"
#include "wireless.h"
#include "transport.h"
#include "timer.h"

uint32_t battery_timer = 0;
uint8_t battery_percent = 100;
// 其实这个标志位算是是否上报电池电量到蓝牙模块。
uint8_t battery_is_start = 0;


// 电池电压转百分比
uint8_t calculate_battery_percentage(uint16_t current_mv) {
    if (current_mv >= BATTER_MAX_MV) {
        return 100;
    } else if (current_mv <= BATTER_MIN_MV) {
        return 0;
    } else {
        uint16_t percentage = ((current_mv - BATTER_MIN_MV) * 100) / (BATTER_MAX_MV - BATTER_MIN_MV);
        // 如果百分比超过100，确保其被限制在100以内
        if (percentage > 100) {
            percentage = 100;
        }
        return (uint8_t)percentage;
    }
}

// 读取并更新数据
void battery_read_and_update_data(void)
{
    uint32_t sum   = 0;
    uint16_t max_v = 0;
    uint16_t min_v = 0xFFFF;
    const uint8_t  NUM_SAMPLES  =   10;
    const uint8_t  ADC_DEADBAND  =  5;
    static uint16_t last_adc = 0;

    analogReadPin(BATTER_ADC_PIN);
    wait_us(50);          
    /* 采样 NUM_SAMPLES 次 */
    for (uint8_t i = 0; i < NUM_SAMPLES; i++) {
        uint16_t v = analogReadPin(BATTER_ADC_PIN);

        sum   += v;
        if (v > max_v) max_v = v;
        if (v < min_v) min_v = v;

    }

    /* 去掉最大/最小值后求平均 */
    sum -= (uint32_t)max_v + (uint32_t)min_v;
    uint16_t new_adc = (uint16_t)(sum / (NUM_SAMPLES - 2));

    /* 死区滤波：只有跳出 ±5 LSB 才更新 */
    if (new_adc > last_adc + ADC_DEADBAND ||
        new_adc < last_adc - ADC_DEADBAND) {
        last_adc = new_adc;
    }
    uint16_t adc = last_adc;

    uint16_t voltage_mV_Fenya = (adc * 3300) / 1023;
    uint16_t voltage_mV_actual = voltage_mV_Fenya  * (1 + (BAT_R_UPPER / BAT_R_LOWER));

    // 计算当前电压对应的百分比
    uint8_t new_percent = calculate_battery_percentage(voltage_mV_actual);
    voltage_mV_actual = voltage_mV_actual;  // 
    km_printf("adc:%d   fymv:%d  sjmv:%d  bfb:%d  \r\n",
    adc,voltage_mV_Fenya,voltage_mV_actual,calculate_battery_percentage(voltage_mV_actual));
    km_printf("adcState:%d\r\n",ADCD1.state);
    analogAdcStop(BATTER_ADC_PIN);
    // 如果USB插入
    if (usb_power_connected()) 
    {
        battery_percent = 100;
        km_printf("usb_power_connected\r\n");
        return;  // 不上传电量
    }
    battery_percent = new_percent;

    if(battery_is_start == 0)
    {
        return;
    }
    bhq_update_battery_percent(battery_percent, voltage_mV_actual);  // 上报电量
}
void battery_percent_read_task(void)
{ 

    if (battery_timer == 0) {
        battery_timer = timer_read32();
        battery_read_and_update_data();
    }

    // 定时任务，2秒执行一次
    if (timer_elapsed32(battery_timer) > 2000) {
        battery_timer = timer_read32();
        battery_read_and_update_data();
    }
}
void battery_reset_timer(void)
{
    battery_timer = timer_read32();
}

uint8_t battery_get(void)
{
    return battery_percent;
}

void battery_stop(void)
{
    battery_is_start = 0;
}

void battery_start(void)
{
    battery_is_start = 1;
}

void battery_init(void)
{
    // battery_reset_timer();
}