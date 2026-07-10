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

# if defined(KB_CHECK_BATTERY_ENABLED)
#   include "battery.h"
#endif
#if HAL_USE_ADC
#    include "analog.h"
#endif

static uint32_t     lpm_timer_buffer = 0;
static bool         lpm_time_up               = false;

// use for config wakeUp Pin
// static const pin_t wakeUpRow_pins[MATRIX_ROWS] = MATRIX_ROW_PINS;
// static const pin_t wakeUpCol_pins[MATRIX_COLS]   = MATRIX_COL_PINS;

void ws2812power_enabled(void);
void ws2812power_Disabled(void);

void lpm_timer_reset(void) {
    lpm_time_up      = false;
    lpm_timer_buffer = 0;
}

__attribute__((weak)) void lpm_device_power_open(void) ;
__attribute__((weak)) void lpm_device_power_close(void) ;

void lpm_init(void)
{
    lpm_timer_reset();

#ifdef BHQ_INT_PIN
    gpio_write_pin_high(BHQ_INT_PIN);
#endif

    // usb
    gpio_set_pin_input(USB_POWER_SENSE_PIN);
    palEnableLineEvent(USB_POWER_SENSE_PIN, PAL_EVENT_MODE_RISING_EDGE);

    lpm_device_power_open();
}
__attribute__((weak)) void lpm_device_power_open(void) 
{
   
}
__attribute__((weak)) void lpm_device_power_close(void) 
{
   
}

// 将未使用的引脚设置为输入模拟
__attribute__((weak)) void lpm_set_unused_pins_to_input_analog(void)
{

}

void My_PWR_EnterSTOPMode(void)
{
    
    // 配置ldo为低功耗
    // PWC->LDOV |= PWC_LDOOV_VREXLPEN;
    PWC->CTRL |= PWC_CTRL_VRSEL;
    // at32 进入深度随眠模式
    PWC->CTRL &= ~PWC_CTRL_LPSEL;
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    __WFI();

    PWC->CTRL &= ~PWC_CTRL_VRSEL;
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
}

void enter_low_power_mode_prepare(void)
{
    if (usb_power_connected()) 
    {
       return;
    }
    lpm_set_unused_pins_to_input_analog();    // 设置没有使用的引脚为模拟输入

# if defined(KB_CHECK_BATTERY_ENABLED)
    battery_disable_read();
#endif
    matrix_sleepConfig();

#ifdef BHQ_IQR_PIN
    gpio_set_pin_input_low(BHQ_IQR_PIN);
    palEnableLineEvent(BHQ_IQR_PIN, PAL_EVENT_MODE_RISING_EDGE);
#endif
#ifdef BHQ_INT_PIN
    gpio_write_pin_low(BHQ_INT_PIN);
#endif

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
#if HAL_USE_ADC
    adc_stop_all();
#endif
    lpm_device_power_close();    // 外围设备 电源 关闭
    My_PWR_EnterSTOPMode();

    chSysLock();
        at32_clock_init();
        halInit();
        stInit();
        timer_init();
    chSysUnlock();

#if HAL_USE_ADC
    // halInit()→adcInit() 重置了ADC驱动状态，需同步 adcInitialized[] 标志
    adc_stop_all();
#endif

    /*  USB D+/D- */
    // palSetLineMode(A11, PAL_STM32_OTYPE_PUSHPULL | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_FLOATING | PAL_MODE_ALTERNATE(10U));
    // palSetLineMode(A12, PAL_STM32_OTYPE_PUSHPULL | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_FLOATING | PAL_MODE_ALTERNATE(10U));
 
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
    bhq_common_init();
# if defined(KB_CHECK_BATTERY_ENABLED)
    battery_enable_read();
#endif
    lpm_device_power_open();    // 外围设备 电源 关闭
  
#ifdef BHQ_INT_PIN
    gpio_write_pin_high(BHQ_INT_PIN);
#endif

}



void lpm_task(void)
{
    if (usb_power_connected() && USBD1.state == USB_STOP) {
        /*  USB D+/D- */
        // palSetLineMode(A11, PAL_STM32_OTYPE_PUSHPULL | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_FLOATING | PAL_MODE_ALTERNATE(10U));
        // palSetLineMode(A12, PAL_STM32_OTYPE_PUSHPULL | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_FLOATING | PAL_MODE_ALTERNATE(10U));
        usb_event_queue_init();
        init_usb_driver(&USBD1);
    }
    
    if (usb_power_connected()) 
    {
       return;
    }

    if(report_buffer_is_empty() == false)
    {
        lpm_time_up = false;
        lpm_timer_buffer = 0;
        return;
    }

    if(lpm_time_up == false && lpm_timer_buffer == 0)
    {
        lpm_time_up = true;
        lpm_timer_buffer = sync_timer_read32();
    }

    if (lpm_time_up == true && sync_timer_elapsed32(lpm_timer_buffer) > RUN_MODE_PROCESS_TIME) {
        lpm_time_up = false;
        lpm_timer_buffer = 0;
        enter_low_power_mode_prepare();
    }
}