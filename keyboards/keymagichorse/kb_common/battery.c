/* Copyright 2025 keymagichorse
 *
 * GPL v2 or later
 */

#include "battery.h"
#include "timer.h"
#include "bhq_common.h"
#include "bhq.h"
#include "analog.h"


uint8_t  battery_percent = 100;
uint16_t battery_mv      = 0;

static uint8_t battery_has_valid_sample = 0;
static uint8_t battery_is_read_enabled  = 1;
static uint8_t battery_ble_update_en    = 0;

static uint32_t battery_sample_timer = 0;
static uint32_t battery_report_timer = 0;

static uint8_t last_sample  = 0xFF;
static uint8_t stable_count = 0;

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

    int16_t adc = analogReadPin(BATTERY_ADC_PIN);
    if(adc > 700)
    {
        return 0;
    }
    uint16_t mv_div = (adc * 3300UL) / 1023; // 10bit

    battery_mv = (uint16_t)((uint32_t)mv_div * (BAT_R_UPPER + BAT_R_LOWER) / BAT_R_LOWER);

    // /* 电压 → 百分比 */
    uint8_t new_percent = calculate_battery_percentage(battery_mv);

    km_printf("1 adc:%d mv_div:%d bat mv:%d\n", adc, mv_div, battery_mv);

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
}

void battery_reset_timer(void) {
    battery_report_timer = timer_read32();
}

uint8_t battery_driver_sample_percent(void) {
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
