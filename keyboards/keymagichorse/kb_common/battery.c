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


static bool battery_low_voltage = false;
static uint8_t battery_low_count = 0;   // 低电量连续确认计数
#define BATTERY_LOW_CONFIRM  3           // 连续 N 次低于阈值才锁定低电量

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
    // 允许在 ±BATTERY_DEBOUNCE_TOLERANCE 范围内波动视为稳定
    int16_t diff = (int16_t)new_percent - (int16_t)last_sample;
    if (diff < 0) diff = -diff;
    if (diff <= BATTERY_DEBOUNCE_TOLERANCE) {
        if (stable_count < 255) stable_count++;
        // 用最新值更新 last_sample，使基准跟随实际电压
        last_sample = new_percent;
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

#ifndef BATTERY_ADC_RETRY_COUNT
#    define BATTERY_ADC_RETRY_COUNT 3
#endif

#ifndef BATTERY_ADC_MIN_VALID
#    define BATTERY_ADC_MIN_VALID 50
#endif

/* ===================== 公共：读取电池电压 ===================== */

// 读取电池电压 (mV)，通过 VREFINT 动态校准 VDDA
// 返回电池电压 (mV)，读取失败返回 0
static uint16_t battery_read_mv(void) {
    // 读取外部 ADC (电池分压)
    int16_t adc = 0;
    for (uint8_t i = 0; i < BATTERY_ADC_RETRY_COUNT; i++) {
        adc = analogReadPin(BATTERY_ADC_PIN);
        if (adc >= BATTERY_ADC_MIN_VALID) {
            break;
        }
    }
    if (adc < BATTERY_ADC_MIN_VALID) {
        return 0;
    }

    // 读取 VREFINT
    adcSTM32EnableTSVREFE();
    adc_mux vrefint_mux = TO_MUX(BATTERY_VREFINT_CHANNEL, 0);
    int16_t vrefint_adc = adc_read(vrefint_mux);

    if (vrefint_adc <= 0) {
        return 0;
    }

    // VDDA = VREFINT_mV * FullScale / vrefint_adc
    uint32_t vdda_mv = ((uint32_t)BATTERY_VREFINT_MV * BATTERY_ADC_FULLSCALE) / (uint16_t)vrefint_adc;
    // 分压后的电压
    uint32_t mv_div = (uint32_t)adc * vdda_mv / BATTERY_ADC_FULLSCALE;
    // 电池电压 = 分压后电压 * 分压系数
    uint16_t mv = (uint16_t)(mv_div * (BAT_R_UPPER + BAT_R_LOWER) / BAT_R_LOWER);

    km_printf("adc:%d vref:%d vdda:%d bat:%d\n", adc, vrefint_adc, vdda_mv, mv);

    return mv;
}

// 低电量检测（带迟滞恢复）：
// - 电压 <= BATTERY_MIN_MV：连续 N 次确认后锁定低电量
// - 电压 >= BATTERY_LOW_RECOVER_MV：连续 N 次确认后解除低电量
// 返回 true 表示当前处于低电量锁定状态
static bool battery_check_low(uint16_t mv) {
    if (battery_low_voltage) {
        // 已锁定低电量，检查是否恢复
        if (mv >= BATTERY_LOW_RECOVER_MV) {
            if (battery_low_count < 255) battery_low_count++;
            if (battery_low_count >= BATTERY_LOW_CONFIRM) {
                battery_low_voltage = false;
                battery_low_count  = 0;
            }
        } else {
            battery_low_count = 0;
        }
    } else {
        // 未锁定，检查是否触发低电量
        if (mv <= BATTERY_MIN_MV) {
            if (battery_low_count < 255) battery_low_count++;
            if (battery_low_count >= BATTERY_LOW_CONFIRM) {
                battery_low_voltage = true;
            }
        } else {
            battery_low_count = 0;
        }
    }
    return battery_low_voltage;
}

/* ===================== 正常运行：battery_task 调用 ===================== */

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

    uint16_t mv = battery_read_mv();
    if (mv == 0) {
        return 0;
    }

    battery_mv = mv;

    // 低电量检测
    battery_check_low(mv);

    // 电压 → 百分比
    uint8_t new_percent = calculate_battery_percentage(mv);

    km_printf("bat:%d pct:%d\n", mv, new_percent);

    // 先对原始百分比做去抖动确认
    if (battery_percent_debounce(new_percent)) {
        // 去抖动通过后，再应用回滞规则决定是否更新 battery_percent
        uint8_t apply_percent = new_percent;

        if (battery_has_valid_sample && new_percent > battery_percent) {
            if (new_percent - battery_percent < BATTERY_PERCENT_HYSTERESIS) {
                apply_percent = battery_percent;  // 上升幅度不够，保持不变
            }
        }

        if (apply_percent != battery_percent) {
            battery_percent_changed_internal(apply_percent);
        }
        battery_percent          = apply_percent;
        battery_has_valid_sample = 1;
        km_printf("battery stable: %dmV -> %d%%\n", battery_mv, battery_percent);
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
            return;
        }
        battery_percent_update_wireless();
    }
}

void battery_init(void) {
    battery_percent_debounce_reset();
    battery_low_voltage      = false;
    battery_low_count       = 0;
    adcSTM32EnableTSVREFE();
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

/* ===================== RTC 唤醒：轻量级电池检查 ===================== */

bool battery_rtc_check_voltage(void) {
    if (usb_power_connected()) {
        battery_low_voltage      = false;
        battery_low_count       = 0;
        return true;
    }
    if (battery_low_voltage) {
        return false;
    }

    // 读取电池电压（复用公共函数）
    uint16_t mv = battery_read_mv();
    if (mv == 0) {
        return true;  // ADC 未就绪，不判定为低电量
    }

    battery_mv = mv;

    // 低电量检测
    if (battery_check_low(mv)) {
        return false;  // 已锁定低电量，禁用 RTC 唤醒
    }

    return true;
}
