/* Copyright 2025 keymagichorse
 *
 * GPL v2 or later
 */

#include "battery.h"
#include "timer.h"
#include "bhq_common.h"
#include "bhq.h"
#include "analog.h"


static uint8_t  battery_percent = 100;
static uint16_t battery_mv      = 0;

static uint8_t battery_has_valid_sample = 0;
static uint8_t battery_is_read_enabled  = 1;
static uint8_t battery_ble_update_en    = 0;

static uint32_t battery_sample_timer = 0;
static uint32_t battery_report_timer = 0;

static uint8_t last_sample  = 0xFF;
static uint8_t stable_count = 0;

// 低电量标志：电池 ≤ BATTERY_LOW_MV 时置 true
// 一旦置位，即使电压回升也不清除（需充电/USB插入后重新初始化）
static bool battery_low_voltage = false;

// RTC 唤醒计数器，每 N 次唤醒检查一次电池
static uint8_t battery_rtc_wakeup_count = 0;

__attribute__((weak)) void battery_percent_changed_user(uint8_t level) {}
__attribute__((weak)) void battery_percent_changed_kb(uint8_t level) {}

static void battery_percent_changed_internal(uint8_t level) {
    battery_percent_changed_user(level);
    battery_percent_changed_kb(level);
}

static void battery_percent_update_wireless(void) {
    if (battery_ble_update_en && battery_has_valid_sample) {
        km_printf("update ble bat:%d\n", battery_percent);
        bhq_update_battery_percent(battery_percent, battery_mv);
    }
}

static uint8_t calculate_battery_percentage(uint16_t mv) {
    if (mv >= BATTERY_MAX_MV) return 100;
    if (mv <= BATTERY_MIN_MV) return 0;
    return (uint8_t)(((uint32_t)(mv - BATTERY_MIN_MV) * 100) / (BATTERY_MAX_MV - BATTERY_MIN_MV));
}

static uint8_t battery_percent_debounce(uint8_t new_percent) {
    km_printf("bat ldo:%d new:%d\n", last_sample, new_percent);
    if (new_percent == last_sample) {
        if (stable_count < 255) stable_count++;
    } else {
        last_sample  = new_percent;
        stable_count = 1;
    }

    if (!battery_has_valid_sample) {
        return (stable_count >= 2);
    }

    return (stable_count >= 3);
}
static void battery_percent_debounce_reset(void) {
    last_sample  = 0xFF;
    stable_count = 0;
}
/* ===================== 读取电池 ===================== */

// ADC 读取重试次数，用于唤醒/上电后 ADC 未稳定的场景
#ifndef BATTERY_ADC_RETRY_COUNT
#    define BATTERY_ADC_RETRY_COUNT 3
#endif

// ADC 最小有效值，低于此值认为 ADC 未就绪或读数异常
// 10bit 满量程 1023，100K/100K 分压下 3.0V 电池对应 ADC ≈ 465
#ifndef BATTERY_ADC_MIN_VALID
#    define BATTERY_ADC_MIN_VALID 50
#endif

// 使用 VREFINT 内部参考通道动态测量真实 VDDA
// 原理: VREFINT 是一个固定 ~1.21V 的内部参考电压，
// ADC 读它的值 = VREFINT_mV / VDDA_mV * FullScale
// 因此 VDDA_mV = VREFINT_mV * FullScale / vrefint_adc
#if defined(BATTERY_USE_VREFINT)
static uint16_t battery_read_vdda_mv(void) {
    // 使能内部参考电压通道 (TSVREFE)
    // STM32F4xx/F1xx 的 ADCv2 需要置位 ADC->CCR 的 TSVREFE 位才能读取 VREFINT
#    if defined(USE_ADCV2) && !defined(AT32F415)
    adcSTM32EnableTSVREFE();
#    endif

    // VREFINT 需要一定的 settling time，唤醒后首次读取可能为 0 或极低值
    // 通过重试来跳过无效的首次转换
    adc_mux      vrefint_mux = TO_MUX(BATTERY_VREFINT_CHANNEL, 0);
    int16_t      vrefint_adc = 0;
    for (uint8_t i = 0; i < BATTERY_ADC_RETRY_COUNT; i++) {
        vrefint_adc = adc_read(vrefint_mux);
        if (vrefint_adc > 0) {
            break;
        }
    }
    if (vrefint_adc <= 0) {
        // 读取失败，回退到默认 3.3V
        return 3300;
    }
    uint32_t vdda_mv = ((uint32_t)BATTERY_VREFINT_MV * BATTERY_ADC_FULLSCALE) / (uint16_t)vrefint_adc;
    // 钳位到合理范围 2.5V ~ 3.6V
    if (vdda_mv < 2500) vdda_mv = 2500;
    if (vdda_mv > 3600) vdda_mv = 3600;
    return (uint16_t)vdda_mv;
}
#endif

static uint8_t battery_read_percent(void) {
    /* USB 供电直接认为 100% */
    if (usb_power_connected()) {
        battery_percent = 100;
        battery_mv      = BATTERY_MAX_MV;

        battery_has_valid_sample = 1;
        battery_percent_debounce_reset();
        battery_percent_changed_internal(100);
        return 1;
    }

    // 唤醒或刚上电后 ADC 首次转换可能返回 0 或极低值
    // 通过重试来跳过未就绪的首次转换
    int16_t adc = 0;
    for (uint8_t i = 0; i < BATTERY_ADC_RETRY_COUNT; i++) {
        adc = analogReadPin(BATTERY_ADC_PIN);
        if (adc >= BATTERY_ADC_MIN_VALID) {
            break;
        }
    }
    if (adc < BATTERY_ADC_MIN_VALID || adc > 700) {
        // ADC 未就绪或读数异常，跳过本次采样等待下次
        return 0;
    }

#if defined(BATTERY_USE_VREFINT)
    // 动态测量 VDDA，消除 LDO dropout 导致的 VDDA 偏移
    uint16_t vdda_mv = battery_read_vdda_mv();
    uint16_t mv_div = ((uint32_t)adc * vdda_mv) / BATTERY_ADC_FULLSCALE;
#else
    uint16_t vdda_mv = 3300;
    uint16_t mv_div = (adc * 3300UL) / 1023; // 10bit
#endif

    battery_mv = (uint16_t)((uint32_t)mv_div * (BAT_R_UPPER + BAT_R_LOWER) / BAT_R_LOWER);

    // 低电量检测：一旦触发就锁定，防止 ADC 波动反复触发
    if (battery_mv <= BATTERY_LOW_MV) {
        battery_low_voltage = true;
    }

    // /* 电压 → 百分比 */
    uint8_t new_percent = calculate_battery_percentage(battery_mv);

    km_printf("1 adc:%d vdda:%d mv_div:%d bat mv:%d\n", adc, vdda_mv, mv_div, battery_mv);

    /* 5% 一档 */
    new_percent = ((new_percent + 2) / 5) * 5;
    if (new_percent > 100) new_percent = 100;

    /* 只允许下降 */
    if (battery_has_valid_sample && new_percent > battery_percent) {
        new_percent = battery_percent;
    }

    // /* 消抖判断 */
    if (battery_percent_debounce(new_percent)) {
        battery_percent_changed_internal(new_percent);
        battery_percent          = new_percent;
        battery_has_valid_sample = 1;
        km_printf("battery stable: %dmV -> %d%\n", battery_mv, battery_percent);
        return 1;
    }

    return 0;
}

void battery_task(void) {
    if (timer_elapsed32(battery_sample_timer) > 500) {
        battery_sample_timer = timer_read32();

        if (battery_is_read_enabled) {
            battery_read_percent();
        }
    }
    if (!battery_ble_update_en) {
        battery_report_timer = timer_read32();
    }
    if (battery_ble_update_en && timer_elapsed32(battery_report_timer) > 2500) {
        battery_report_timer = timer_read32();
        if (usb_power_connected()) {
            return ;
        }
        battery_percent_update_wireless();
    }
}

void battery_init(void) {
    battery_percent_debounce_reset();
    battery_low_voltage      = false;
    battery_rtc_wakeup_count = 0;

#if defined(BATTERY_USE_VREFINT) && defined(USE_ADCV2) && !defined(AT32F415)
    // 提前使能内部参考电压通道，确保第一次读取 VREFINT 时已就绪
    adcSTM32EnableTSVREFE();
#endif
}

void battery_reset_timer(void) {
    battery_report_timer = timer_read32();
}

uint8_t battery_get_percent(void) {
    return battery_has_valid_sample ? battery_percent : 0xFF;
}

void battery_enable_read(void) {
    battery_is_read_enabled = 1;
}

void battery_disable_read(void) {
    battery_is_read_enabled = 0;
}

void battery_enable_ble_update(void) {
    battery_ble_update_en = 1;
}

void battery_disable_ble_update(void) {
    battery_ble_update_en = 0;
}

uint16_t battery_get_mv(void) {
    return battery_mv;
}

bool battery_is_low_voltage(void) {
    return battery_low_voltage;
}

// RTC 唤醒循环中使用的轻量级电池电压检查
// 每 BATTERY_RTC_CHECK_INTERVAL 次唤醒执行一次 ADC 读取
// 返回 true 表示电压正常，false 表示电压过低应禁用 RTC 唤醒
bool battery_rtc_check_voltage(void) {
    // USB 连接时直接返回正常（USB 供电场景不走 RTC 唤醒）
    if (usb_power_connected()) {
        battery_low_voltage      = false;
        battery_rtc_wakeup_count = 0;
        return true;
    }

    // 已标记为低电量，直接返回 false
    if (battery_low_voltage) {
        return false;
    }

    // 每 N 次唤醒才检查一次，减少 RTC 唤醒循环中的功耗
    battery_rtc_wakeup_count++;
    if (battery_rtc_wakeup_count < BATTERY_RTC_CHECK_INTERVAL) {
        return true;
    }
    battery_rtc_wakeup_count = 0;

    // 读取电池 ADC（此时 ADC 已由 lpm_hal_init/ec_init 初始化）
    int16_t adc = 0;
    for (uint8_t i = 0; i < BATTERY_ADC_RETRY_COUNT; i++) {
        adc = analogReadPin(BATTERY_ADC_PIN);
        if (adc >= BATTERY_ADC_MIN_VALID) {
            break;
        }
    }
    if (adc < BATTERY_ADC_MIN_VALID) {
        // ADC 未就绪，不判定为低电量，继续 RTC 唤醒
        return true;
    }

#if defined(BATTERY_USE_VREFINT)
    uint16_t vdda_mv = battery_read_vdda_mv();
    uint16_t mv_div  = ((uint32_t)adc * vdda_mv) / BATTERY_ADC_FULLSCALE;
#else
    uint16_t mv_div = (adc * 3300UL) / 1023;
#endif
    uint16_t mv = (uint16_t)((uint32_t)mv_div * (BAT_R_UPPER + BAT_R_LOWER) / BAT_R_LOWER);

    if (mv <= BATTERY_LOW_MV) {
        battery_low_voltage = true;
        return false;
    }

    return true;
}
