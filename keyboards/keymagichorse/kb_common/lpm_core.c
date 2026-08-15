/* Copyright 2024 keymagichorse
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

/**
 * @file lpm_core.c
 * @brief 统一的低功耗模式（LPM）核心实现
 *
 * 本文件整合了原先 6 个分散的 LPM 文件：
 *   - lpm_stm32f4_rtc_ec_v1.c   (STM32F4 + RTC + EC 静电容矩阵)
 *   - lpm_stm32f4_rtc_mx_v1.c   (STM32F4 + RTC + MX 机械矩阵)
 *   - lpm_stm32f4.c              (STM32F4 无 RTC)
 *   - lpm_stm32f1.c              (STM32F1 无 RTC)
 *   - lpm_stm32f1_rtc_mx_v1.c   (STM32F1 + RTC + MX)
 *   - lpm_at32f415.c             (AT32F415 无 RTC)
 *
 * 芯片专属的寄存器操作（STOP 模式配置、时钟初始化、USB 引脚、RTC 唤醒）
 * 被提取到 lpm_chip_stm32f4.h / lpm_chip_stm32f1.h / lpm_chip_at32f4.h 中。
 *
 * 功能选择宏（由 kb_common.mk 根据 KB_LPM_DRIVER 名称自动设置）：
 *   - LPM_CHIP_STM32F4 / LPM_CHIP_STM32F1 / LPM_CHIP_AT32  芯片系列
 *   - LPM_RTC_WAKEUP    启用 RTC 周期性唤醒（支持休眠中轮询按键）
 *   - LPM_EC_MATRIX     启用静电容矩阵（使用 ADC 扫描，需要 lpm_hal_init）
 */

// Very few codes are borrowed from https://www.keychron.com
#include "quantum.h"
#include "lpm.h"
#include "lpm_chip.h"
#include "matrix.h"
#include "gpio.h"
#include "debounce.h"
#include "usb_util.h"
#include "usb_main.h"
#include "bhq.h"
#include "report_buffer.h"
#include "uart.h"
#include "bhq_common.h"
#include "matrix_sleep.h"
#include "bluetooth.h"

#ifdef LPM_EC_MATRIX
#    include "ec_switch_matrix.h"
#    include "analog.h"
#else
#    if HAL_USE_ADC
#        include "analog.h"
#    endif
#endif

# if defined(KB_CHECK_BATTERY_ENABLED)
#   include "battery.h"
#endif

/* ================================================================== */
/*  可配置宏                                                           */
/* ================================================================== */

// 静电容误触唤醒超时时间 (ms)
// RTC 唤醒后由 raw_matrix 检测到按键，但在该时间内未收到 QMK 消抖确认
// （process_record_bhq → lpm_timer_reset），则判定为静电误触，立即重新休眠
#ifndef LPM_FALSE_WAKEUP_TIMEOUT
#    define LPM_FALSE_WAKEUP_TIMEOUT 300
#endif

// RTC 唤醒间隔（毫秒）
// 休眠前 10 分钟内使用短间隔，之后使用长间隔以节省功耗
#ifndef LPM_RTC_WAKEUP_INTERVAL_SHORT
#    ifdef LPM_EC_MATRIX
#        define LPM_RTC_WAKEUP_INTERVAL_SHORT 20   // EC 需要更快响应
#    else
#        define LPM_RTC_WAKEUP_INTERVAL_SHORT 50   // MX 可以慢一些
#    endif
#endif
#ifndef LPM_RTC_WAKEUP_INTERVAL_LONG
#    define LPM_RTC_WAKEUP_INTERVAL_LONG 150
#endif
// 短间隔切换为长间隔的时间阈值
#ifndef LPM_RTC_WAKEUP_THRESHOLD
#    define LPM_RTC_WAKEUP_THRESHOLD (10 * 60 * 1000)  // 10 分钟
#endif

// VIA 活动保护时间（ms），在此期间禁止休眠
#ifndef LPM_VIA_ACTIVITY_TIMEOUT
#    define LPM_VIA_ACTIVITY_TIMEOUT (2000 * 60)  // 2 分钟
#endif

/* ================================================================== */
/*  状态变量                                                           */
/* ================================================================== */

static uint32_t lpm_timer_buffer  = 0;
static bool     lpm_time_up       = false;
static bool     lpm_usb_init_flag = false;

// VIA 活动保护
static bool     is_lpm_via_activity_flag = false;
static uint32_t lpm_via_activity_timer   = 0;

#ifdef LPM_RTC_WAKEUP
// RTC 唤醒累计计时器，用于控制短/长唤醒间隔的切换
static uint32_t rtc_wakeup_timer = 0;
#endif

#ifdef LPM_EC_MATRIX
// 静电容误触唤醒标记：RTC 唤醒后 raw_matrix 检测到按键置 true，
// 由 process_record_bhq → lpm_timer_reset 确认后清零，超时未确认则判定为误触
static bool     lpm_wakeup_pending   = false;
static uint32_t lpm_wakeup_timestamp = 0;
#endif

#if !defined(LPM_EC_MATRIX) && (defined(DIODE_DIRECTION) && DIODE_DIRECTION == COL2ROW)
static const pin_t wakeUpCol_pins[MATRIX_COLS] = MATRIX_COL_PINS;
#elif !defined(LPM_EC_MATRIX) && (defined(DIODE_DIRECTION) && DIODE_DIRECTION == ROW2COL)
static const pin_t wakeUpRow_pins[MATRIX_ROWS] = MATRIX_ROW_PINS;
#endif

/* ================================================================== */
/*  弱函数（可由键盘覆写）                                              */
/* ================================================================== */

__attribute__((weak)) void lpm_device_power_open(void) {}
__attribute__((weak)) void lpm_device_power_close(void) {}
__attribute__((weak)) void lpm_set_unused_pins_to_input_analog(void) {}

/* ================================================================== */
/*  RTC 唤醒间隔管理（仅 RTC 模式）                                     */
/* ================================================================== */

#ifdef LPM_RTC_WAKEUP

static void lpm_rtc_wakeup_set(void) {
    uint32_t interval;
    if (rtc_wakeup_timer < LPM_RTC_WAKEUP_THRESHOLD) {
        interval = LPM_RTC_WAKEUP_INTERVAL_SHORT;
    } else {
        interval = LPM_RTC_WAKEUP_INTERVAL_LONG;
    }
    rtc_wakeup_timer += interval;
    lpm_chip_rtc_wakeup_setup(interval);
}

static void lpm_rtc_wakeup_reset(void) {
    rtc_wakeup_timer = 0;
}

#endif /* LPM_RTC_WAKEUP */

/* ================================================================== */
/*  公共 API                                                           */
/* ================================================================== */

void lpm_timer_reset(void) {
    lpm_time_up      = false;
    lpm_timer_buffer = 0;
#ifdef LPM_EC_MATRIX
    lpm_wakeup_pending   = false;
    lpm_wakeup_timestamp = 0;
#endif
#ifdef LPM_RTC_WAKEUP
    lpm_rtc_wakeup_reset();
#endif
}

void lpm_via_activity_update(void) {
    lpm_via_activity_timer = sync_timer_read32();
    is_lpm_via_activity_flag = true;
}

void lpm_init(void) {
    lpm_chip_debug_disable();

    lpm_timer_reset();

#ifdef BHQ_INT_PIN
    gpio_write_pin_high(BHQ_INT_PIN);
#endif

    // USB 插入检测引脚
    gpio_set_pin_input(USB_POWER_SENSE_PIN);
    palEnableLineEvent(USB_POWER_SENSE_PIN, PAL_EVENT_MODE_RISING_EDGE);
    lpm_usb_init_flag = true;

    lpm_device_power_open();

#ifdef LPM_RTC_WAKEUP
    lpm_rtc_wakeup_reset();
    lpm_chip_rtc_init();
#endif
}

/* ================================================================== */
/*  休眠准备（公共部分）                                                */
/* ================================================================== */

static bool lpm_sleep_prepare(void) {
    if (usb_power_connected()) {
        return false;
    }

    lpm_set_unused_pins_to_input_analog();

#if defined(KB_CHECK_BATTERY_ENABLED)
    battery_disable_read();
#endif

// 矩阵休眠配置
#ifdef LPM_EC_MATRIX
    // EC 静电容矩阵：不需要 GPIO 唤醒配置，由 lpm_hal_init() 管理
#elif defined(LPM_RTC_WAKEUP)
    // RTC 模式：配置行/列输出低电平，供 RTC 唤醒后轮询读取
    matrix_rtc_Config();
#else
    // 非 RTC 模式：配置 GPIO 唤醒事件
    matrix_sleepConfig();
#endif

#ifdef LPM_RTC_WAKEUP
    lpm_rtc_wakeup_set();
#endif

    // 唤醒引脚配置
#ifdef BHQ_IQR_PIN
    gpio_set_pin_input_low(BHQ_IQR_PIN);
    palEnableLineEvent(BHQ_IQR_PIN, PAL_EVENT_MODE_RISING_EDGE);
#endif
#ifdef BHQ_INT_PIN
    gpio_write_pin_low(BHQ_INT_PIN);
#endif

    // USB 插入检测
    gpio_set_pin_input(USB_POWER_SENSE_PIN);
    palEnableLineEvent(USB_POWER_SENSE_PIN, PAL_EVENT_MODE_RISING_EDGE);

    // 停止 UART
    sdStop(&UART_DRIVER);
    palSetLineMode(UART_TX_PIN, PAL_MODE_INPUT_ANALOG);
    palSetLineMode(UART_RX_PIN, PAL_MODE_INPUT_ANALOG);

    // 停止 USB
    usbStop(&USBD1);
    usbDisconnectBus(&USBD1);
    lpm_chip_usb_pins_sleep();

    bhq_Disable();

#if HAL_USE_ADC
    // 关闭 ADC 外设，降低 STOP 模式功耗，并同步 adcInitialized[] 标志
    adc_stop_all();
#endif

    lpm_device_power_close();
    return true;
}

/* ================================================================== */
/*  唤醒恢复（公共部分）                                                */
/* ================================================================== */

static void lpm_wake_restore(void) {
    // 重新初始化时钟和 HAL
    chSysLock();
    lpm_chip_clock_init();
    halInit();
    stInit();
    timer_init();
    chSysUnlock();

#if HAL_USE_ADC
    // halInit()→adcInit() 已重置 ADC 驱动状态为 ADC_STOP，
    // 需同步 adcInitialized[] 标志，使后续 adc_read() 能正确调用 adcStart()
    adc_stop_all();
#endif

    // 配置 USB D+/D- 引脚
    lpm_chip_usb_pins_init();

    if (usb_power_connected()) {
        usb_event_queue_init();
        init_usb_driver(&USBD1);
        lpm_usb_init_flag = true;
    } else {
        lpm_usb_init_flag = false;
    }

    // 重新初始化矩阵
    matrix_init();

    lpm_timer_reset();
    report_buffer_init();
    bhq_init();  // uart_init

#if defined(MOUSEKEY_ENABLE)
    mousekey_clear();
#endif

    bhq_common_init();

#if defined(KB_CHECK_BATTERY_ENABLED)
    battery_enable_read();
#endif

    lpm_device_power_open();

#ifdef BHQ_INT_PIN
    gpio_write_pin_high(BHQ_INT_PIN);
#endif

    // 如果蓝牙已连接，发送空按键包以清除可能残留的按键状态
    if (wireless_get() == WT_STATE_CONNECTED) {
        report_keyboard_t report = {0};
        bluetooth_send_keyboard(&report);
    }

#ifdef LPM_EC_MATRIX
    if (lpm_wakeup_pending) {
        lpm_wakeup_timestamp = sync_timer_read32();
    }
#endif
}

/* ================================================================== */
/*  低功耗矩阵扫描                                                     */
/* ================================================================== */

bool lowpower_matrix_task(void) {
#ifdef LPM_EC_MATRIX
    // EC 静电容矩阵：使用 ADC 扫描
    bool           any_key_pressed = false;
    matrix_row_t   raw_matrix[MATRIX_ROWS];
    ec_matrix_scan(raw_matrix);
    for (size_t i = 0; i < MATRIX_ROWS; i++) {
        if (raw_matrix[i] > 0) {
            any_key_pressed = true;
        }
    }
#    if defined(KB_DEBUG)
    ec_print_matrix();
#    endif
    return any_key_pressed;
#else
    // MX 机械矩阵：直接读取 GPIO 引脚
    bool any_key_pressed = false;

#    if (DIODE_DIRECTION == COL2ROW)
    // Set row(low valid), read cols
    for (uint8_t i = 0; i < matrix_cols(); i++) {
        if (wakeUpCol_pins[i] == NO_PIN) {
            continue;
        }
        if (gpio_read_pin(wakeUpCol_pins[i]) == 0) {
            any_key_pressed = true;
            return any_key_pressed;
        }
    }
#    elif (DIODE_DIRECTION == ROW2COL)
    // Set col(low valid), read rows
    for (uint8_t i = 0; i < matrix_rows(); i++) {
        if (wakeUpRow_pins[i] == NO_PIN) {
            continue;
        }
        if (gpio_read_pin(wakeUpRow_pins[i]) == 0) {
            any_key_pressed = true;
            return any_key_pressed;
        }
    }
#    endif
    return any_key_pressed;
#endif
}

/* ================================================================== */
/*  EC 专属：HAL 重新初始化（RTC 唤醒循环中使用）                       */
/* ================================================================== */

#ifdef LPM_RTC_WAKEUP
#    ifdef LPM_EC_MATRIX

/**
 * @brief EC 矩阵 RTC 唤醒循环中的 HAL 重新初始化
 *
 * EC 矩阵扫描依赖 ADC，从 STOP 模式唤醒后需要完整地重新初始化
 * 时钟、PAL、ADC 等外设，然后执行一次矩阵扫描检测按键。
 */
static void lpm_hal_init(void) {
    chSysLock();
    lpm_chip_clock_init();
    /* Initializes the OS Abstraction Layer.*/
    osalInit();
    /* Platform low level initializations.*/
    hal_lld_init();
#        if (HAL_USE_PAL == TRUE) || defined(__DOXYGEN__)
#        if defined(PAL_NEW_INIT)
    palInit();
#        else
    palInit(&pal_default_config);
#        endif
#        endif
#        if (HAL_USE_ADC == TRUE) || defined(__DOXYGEN__)
    adcInit();
#        endif
    stInit();
    timer_init();
    chSysUnlock();

    // adcInit() 已重置 ADC 驱动状态为 ADC_STOP，需同步 adcInitialized[] 标志
    adc_stop_all();

#        if defined(KB_DEBUG)
    bhq_init();
#        endif
    ec_init();
    lowpower_matrix_task();
}

#    endif /* LPM_EC_MATRIX */
#endif     /* LPM_RTC_WAKEUP */

/* ================================================================== */
/*  RTC 唤醒循环                                                       */
/* ================================================================== */

#ifdef LPM_RTC_WAKEUP

/**
 * @brief 带 RTC 周期性唤醒的休眠循环
 *
 * 流程：
 *   1. 进入 STOP 模式
 *   2. RTC 唤醒后重新初始化 HAL（EC 模式）或直接检测（MX 模式）
 *   3. 检测到按键或 USB 插入 → 退出循环
 *   4. 未检测到按键 → 重新进入 STOP 模式
 *   5. 退出循环后执行完整的唤醒恢复
 */
static void lpm_sleep_with_rtc_wakeup(void) {
    if (!lpm_sleep_prepare()) {
        return;  // USB 已连接，不进入休眠
    }
    lpm_chip_enter_stop_mode();

    while (1) {
#ifdef LPM_EC_MATRIX
        lpm_hal_init();
#endif
        lpm_chip_rtc_wakeup_clear();

        if (lowpower_matrix_task()) {
#ifdef LPM_EC_MATRIX
            lpm_wakeup_pending   = true;
            lpm_wakeup_timestamp = sync_timer_read32();
#endif
            break;
        }
        if (usb_power_connected()) {
            break;
        }
        // 重新进入休眠
        if (!lpm_sleep_prepare()) {
            break;  // USB 已连接，不重新休眠
        }
        lpm_chip_enter_stop_mode();
    }

    lpm_wake_restore();
}

#endif /* LPM_RTC_WAKEUP */

/* ================================================================== */
/*  外部接口：进入/退出低功耗模式                                       */
/* ================================================================== */

#ifdef LPM_RTC_WAKEUP
// RTC 模式：休眠准备和唤醒恢复分离，由 lpm_sleep_with_rtc_wakeup 统一调度

void enter_low_power_mode_prepare(void) {
    // RTC 模式下，由 lpm_sleep_with_rtc_wakeup() 统一调用
    // 此函数保留兼容性，直接执行完整循环
    lpm_sleep_with_rtc_wakeup();
}

void exit_low_power_mode_prepare(void) {
    // RTC 模式下，唤醒恢复已在 lpm_sleep_with_rtc_wakeup() 中完成
    // 此函数保留兼容性
}

#else
// 非 RTC 模式：休眠和唤醒恢复在同一函数中完成

void enter_low_power_mode_prepare(void) {
    if (!lpm_sleep_prepare()) {
        return;  // USB 已连接，不进入休眠
    }
    lpm_chip_enter_stop_mode();
    lpm_wake_restore();
}

void exit_low_power_mode_prepare(void) {
    // 非 RTC 模式下，唤醒恢复已在 enter_low_power_mode_prepare() 中完成
}

#endif

/* ================================================================== */
/*  主任务                                                             */
/* ================================================================== */

void lpm_task(void) {
    // ---------- USB 连接检测 ----------
    if (usb_power_connected()) {
        if (!lpm_usb_init_flag) {
            lpm_chip_usb_pins_init();
            usb_event_queue_init();
            init_usb_driver(&USBD1);
            lpm_usb_init_flag = true;
            lpm_timer_reset();
        }
        return;
    }

#ifdef LPM_EC_MATRIX
    // ---------- 误触超时检测 ----------
    // 当 RTC 唤醒后，raw_matrix（未经 QMK 消抖）检测到按键会设置 lpm_wakeup_pending。
    // 只有 process_record_bhq（QMK 消抖后的真正按键）调用 lpm_timer_reset 才会清零 pending。
    // 若超过 LPM_FALSE_WAKEUP_TIMEOUT ms 仍未收到 lpm_timer_reset，
    // 说明是静电误触，立即重新休眠。
    if (lpm_wakeup_pending && sync_timer_elapsed32(lpm_wakeup_timestamp) > LPM_FALSE_WAKEUP_TIMEOUT) {
        lpm_wakeup_pending   = false;
        lpm_wakeup_timestamp = 0;
#    ifdef LPM_RTC_WAKEUP
        lpm_sleep_with_rtc_wakeup();
#    else
        enter_low_power_mode_prepare();
#    endif
    }
#endif

    // ---------- 报告缓冲区检测 ----------
    if (!report_buffer_is_empty()) {
        lpm_time_up      = false;
        lpm_timer_buffer = 0;
        return;
    }

    // ---------- 蓝牙状态检测 ----------
    if (wireless_get() == WT_STATE_ADV_UNPAIRED || wireless_get() == WT_STATE_ADV_PAIRING) {
        lpm_time_up      = false;
        lpm_timer_buffer = 0;
        return;
    }

    // ---------- VIA 活动保护 ----------
    if (is_lpm_via_activity_flag) {
        if (sync_timer_elapsed32(lpm_via_activity_timer) > LPM_VIA_ACTIVITY_TIMEOUT) {
            lpm_time_up              = false;
            lpm_timer_buffer         = 0;
            is_lpm_via_activity_flag = false;
            return;
        }
        return;
    }

    // ---------- 休眠计时 ----------
    if (!lpm_time_up && lpm_timer_buffer == 0) {
        lpm_time_up      = true;
        lpm_timer_buffer = sync_timer_read32();
    }

    if (lpm_time_up && sync_timer_elapsed32(lpm_timer_buffer) > RUN_MODE_PROCESS_TIME) {
        lpm_time_up      = false;
        lpm_timer_buffer = 0;
#ifdef LPM_RTC_WAKEUP
        lpm_sleep_with_rtc_wakeup();
#else
        enter_low_power_mode_prepare();
#endif
    }
}
