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
// Very few codes are borrowed from https://www.keychron.com
#include "quantum.h"
#include "lpm.h"
#include "matrix.h"
#include "gpio.h"
#include "debounce.h"
#include "usb_util.h"
#include <usb_main.h>
#include "bhq.h"
#include "report_buffer.h"
#include "uart.h"
#include "bhq_common.h"
#include "matrix_sleep.h"
#include "bluetooth.h"
#include "ec_switch_matrix.h"
# if defined(KB_CHECK_BATTERY_ENABLED)
#   include "battery.h"
#endif

// 静电容误触唤醒超时时间 (ms)
// RTC 唤醒后由 raw_matrix 检测到按键，但在该时间内未收到 QMK 消抖确认
// （process_record_bhq → lpm_timer_reset），则判定为静电误触，立即重新休眠
#ifndef LPM_FALSE_WAKEUP_TIMEOUT
#    define LPM_FALSE_WAKEUP_TIMEOUT 300
#endif

static uint32_t     lpm_timer_buffer    = 0;
static bool         lpm_time_up         = false;
static bool         lpm_usb_init_flag   = false;

static bool is_lpm_via_activity_flag = false;
static uint32_t lpm_via_activity_timer = 0;

static uint32_t rtc_wakeup_timer = 0;

// 静电容误触唤醒标记：RTC 唤醒后 raw_matrix 检测到按键置 true，
// 由 process_record_bhq → lpm_timer_reset 确认后清零，超时未确认则判定为误触
static bool         lpm_wakeup_pending     = false;
// 误触唤醒 pending 置位的时间戳，用于超时判断
static uint32_t     lpm_wakeup_timestamp   = 0;

__attribute__((weak)) void lpm_device_power_open(void) {}
__attribute__((weak)) void lpm_device_power_close(void) {}
// 将未使用的引脚设置为输入模拟
__attribute__((weak)) void lpm_set_unused_pins_to_input_analog(void){}

static inline uint32_t rtc_wakeup_calc(uint32_t ms)
{
    uint32_t wutr;

    if (ms <= 1000) {
        uint32_t ticks = (ms * 2048 + 500) / 1000;
        if (ticks == 0) {
            ticks = 1;
        }
        wutr = (0U << 16) | (ticks - 1);
    } else {
        uint32_t sec = ms / 1000;
        if (sec == 0) {
            sec = 1;
        }
        wutr = (4U << 16) | (sec - 1);
    }

    return wutr;
}


void rtc_wakeup_set(void)
{
    RTCWakeup wakeupspec;
    // 10分钟内 50ms唤醒一次
    if (rtc_wakeup_timer < 10 * 60 * 1000) 
    {
        wakeupspec.wutr = rtc_wakeup_calc(16);
        rtc_wakeup_timer += 16;
    }
    // 30分钟后
    else 
    {
        wakeupspec.wutr = rtc_wakeup_calc(150);
        rtc_wakeup_timer += 150;
    }
    rtcSTM32SetPeriodicWakeup(&RTCD1, &wakeupspec);
    rtcSetCallback(&RTCD1, NULL);
}


void lpm_timer_reset(void) {
    lpm_time_up        = false;
    lpm_timer_buffer   = 0;
    lpm_wakeup_pending = false;
    rtc_wakeup_timer   = 0;
}


void lpm_init(void)
{
    // 禁用调试功能以降低功耗
    DBGMCU->CR &= ~DBGMCU_CR_DBG_SLEEP;   // 禁用在Sleep模式下的调试
    DBGMCU->CR &= ~DBGMCU_CR_DBG_STOP;    // 禁用在Stop模式下的调试
    DBGMCU->CR &= ~DBGMCU_CR_DBG_STANDBY; // 禁用在Standby模式下的调试

    lpm_timer_reset();
    // usb
    gpio_set_pin_input(USB_POWER_SENSE_PIN);
    palEnableLineEvent(USB_POWER_SENSE_PIN, PAL_EVENT_MODE_RISING_EDGE);
    lpm_usb_init_flag   = true;

    lpm_device_power_open();
}

void My_PWR_EnterSTOPMode(void)
{
#if STM32_HSE_ENABLED
    /* Switch to HSI */
    RCC->CFGR = (RCC->CFGR & (~STM32_SW_MASK)) | STM32_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != (STM32_SW_HSI << 2));

    /* Set HSE off  */
    RCC->CR &= ~RCC_CR_HSEON;
    while ((RCC->CR & RCC_CR_HSERDY));

    palSetLineMode(LPM_STM32_HSE_PIN_IN, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(LPM_STM32_HSE_PIN_OUT, PAL_MODE_INPUT_ANALOG); 
#endif
    /* Wake source: Reset pin, all I/Os, BOR, PVD, PVM, RTC, LCD, IWDG,
    COMPx, USARTx, LPUART1, I2Cx, LPTIMx, USB, SWPMI */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    PWR->CR |=
        PWR_CR_MRLVDS |
        PWR_CR_LPLVDS |
        PWR_CR_FPDS |
        PWR_CR_LPDS |
        0;
    __WFI();

    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
}

void enter_low_power_mode_prepare(void)
{
    if (usb_power_connected()) 
    {
       return;
    }
    
    lpm_set_unused_pins_to_input_analog();    // 设置没有使用的引脚为模拟输入
    rtc_wakeup_set();



    gpio_set_pin_input_low(BHQ_IQR_PIN);
    palEnableLineEvent(BHQ_IQR_PIN, PAL_EVENT_MODE_RISING_EDGE);
    gpio_write_pin_low(BHQ_INT_PIN);

// usb 插入检测
    gpio_set_pin_input(USB_POWER_SENSE_PIN);
    palEnableLineEvent(USB_POWER_SENSE_PIN, PAL_EVENT_MODE_RISING_EDGE);

    /* Usb unit is actived and running, stop and disconnect first */
    sdStop(&UART_DRIVER);
    palSetLineMode(UART_TX_PIN, PAL_MODE_INPUT_ANALOG);
    palSetLineMode(UART_RX_PIN, PAL_MODE_INPUT_ANALOG);

    usbStop(&USBD1);
    usbDisconnectBus(&USBD1);

    bhq_Disable();
    lpm_device_power_close();    // 外围设备 电源 关闭
    My_PWR_EnterSTOPMode();

}

void lpm_via_activity_update(void)
{
    lpm_via_activity_timer = sync_timer_read32();
    is_lpm_via_activity_flag = true;
}


void exit_low_power_mode_prepare(void)
{
    chSysLock();
        stm32_clock_init();
        halInit();
        stInit();
        timer_init();
    chSysUnlock();

    /*  USB D+/D- */
    palSetLineMode(A11, PAL_STM32_OTYPE_PUSHPULL | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_FLOATING | PAL_MODE_ALTERNATE(10U));
    palSetLineMode(A12, PAL_STM32_OTYPE_PUSHPULL | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_FLOATING | PAL_MODE_ALTERNATE(10U));
    if (usb_power_connected()) 
    {
        usb_event_queue_init();
        init_usb_driver(&USBD1);
        lpm_usb_init_flag   = true;
    }
    else
    {
        lpm_usb_init_flag   = false;
    }

    // /* Call debounce_free() to avoiding memory leak of debounce_counters as debounce_init()
    // invoked in matrix_init() alloc new memory to debounce_counters */
    // debounce_free();
    matrix_init();

    lpm_timer_reset();
    report_buffer_init();
    bhq_init();     // uart_init
#if defined (MOUSEKEY_ENABLE)
    mousekey_clear();
#endif
    // clear_keyboard();
    // layer_clear();
    lpm_device_power_open();    // 外围设备 电源 关闭
  
    gpio_write_pin_high(BHQ_INT_PIN);
    report_keyboard_t report = {0};
    bluetooth_send_keyboard(&report);   // 往里面填充一个空的按键包
}


bool lowpower_matrix_task(void) 
{
    bool any_key_pressed = false;
    matrix_row_t raw_matrix[MATRIX_ROWS];
    any_key_pressed = ec_matrix_scan(raw_matrix);
#if defined(KB_DEBUG)
    ec_print_matrix();
#endif
    return any_key_pressed; 
}
void lmp_hal_init(void)
{
    chSysLock();
        stm32_clock_init();
        /* Initializes the OS Abstraction Layer.*/
        osalInit();
        /* Platform low level initializations.*/
        hal_lld_init();
        #if (HAL_USE_PAL == TRUE) || defined(__DOXYGEN__)
        #if defined(PAL_NEW_INIT)
            palInit();
        #else
            palInit(&pal_default_config);
        #endif
        #endif
        #if (HAL_USE_ADC == TRUE) || defined(__DOXYGEN__)
            adcInit();
        #endif
        stInit();
        timer_init();
    chSysUnlock();
#if defined(KB_DEBUG)
    bhq_init();
#endif
    ec_init();
}
/**
 * @brief 进入 STOP 休眠，由 RTC 定时唤醒并扫描静电容原始矩阵。
 *
 * 唤醒后调用 exit_low_power_mode_prepare() 恢复外设，然后设置
 * lpm_wakeup_pending 等待 process_record_bhq → lpm_timer_reset 的确认。
 */
static void lpm_sleep_with_rtc_wakeup(void)
{
    enter_low_power_mode_prepare();

    // RTC 定时唤醒循环：在低功耗状态下周期扫描原始矩阵
    while (1) {
        lmp_hal_init();
        bool pressed = false;
        for (int i = 0; i < 2; i++) {
            if (lowpower_matrix_task()) {
                // 按键唤醒
                pressed = true;
                break;
            }
        }
        if (pressed || usb_power_connected()) {
            break;
        }
        enter_low_power_mode_prepare();
    }

    exit_low_power_mode_prepare();
    // exit_low_power_mode_prepare 内部调用 lpm_timer_reset 会清零 pending
    // 需要在此重新设置，等待 process_record_bhq 的确认
    lpm_wakeup_pending   = true;
    lpm_wakeup_timestamp = sync_timer_read32();
}

void lpm_task(void)
{
    // USB 已连接时：确保 USB 驱动已初始化，然后跳过所有低功耗逻辑
    if (usb_power_connected()) 
    {
        if(lpm_usb_init_flag == false)
        {
            /*  USB D+/D- */
            palSetLineMode(A11, PAL_STM32_OTYPE_PUSHPULL | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_FLOATING | PAL_MODE_ALTERNATE(10U));
            palSetLineMode(A12, PAL_STM32_OTYPE_PUSHPULL | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_FLOATING | PAL_MODE_ALTERNATE(10U));
            usb_event_queue_init();
            init_usb_driver(&USBD1);
            lpm_usb_init_flag   = true;
        }
       return;
    }

    // ---------- 误触超时检测 ----------
    // 当 RTC 唤醒后，raw_matrix（未经 QMK 消抖）检测到按键会设置 lpm_wakeup_pending。
    // 只有 process_record_bhq（QMK 消抖后的真正按键）调用 lpm_timer_reset 才會清零 pending。
    // 若超过 LPM_FALSE_WAKEUP_TIMEOUT ms 仍未收到 lpm_timer_reset，说明是静电误触，立即重新休眠。
    if (lpm_wakeup_pending && sync_timer_elapsed32(lpm_wakeup_timestamp) > LPM_FALSE_WAKEUP_TIMEOUT) {
        lpm_wakeup_pending = false;
        lpm_sleep_with_rtc_wakeup();
        return;
    }

    // 按键缓冲区未清空，说明有数据正在发送，重置休眠计时器
    if(report_buffer_is_empty() == false)
    {
        lpm_time_up = false;
        lpm_timer_buffer = 0;
        return;
    }

    // 蓝牙处于广播状态时不进入休眠
    if(wireless_get() == WT_STATE_ADV_UNPAIRED || wireless_get() == WT_STATE_ADV_PAIRING)
    {
        lpm_time_up = false;
        lpm_timer_buffer = 0;
        return;
    }
    
    // 开始计时：记录进入空闲模式的起始时间
    if(lpm_time_up == false && lpm_timer_buffer == 0)
    {
        lpm_time_up = true;
        lpm_timer_buffer = sync_timer_read32();
    }

    // 空闲超时，进入低功耗 STOP 模式
    if (lpm_time_up == true && sync_timer_elapsed32(lpm_timer_buffer) > RUN_MODE_PROCESS_TIME) {
        lpm_time_up = false;
        lpm_timer_buffer = 0;
        lpm_sleep_with_rtc_wakeup();
    }
}
 