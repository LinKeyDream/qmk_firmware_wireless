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
# if defined(KB_CHECK_BATTERY_ENABLED)
#   include "battery.h"
#endif
#if HAL_USE_ADC
#    include "analog.h"
#endif

static uint32_t     lpm_timer_buffer    = 0;
static bool         lpm_time_up         = false;
static bool         lpm_usb_init_flag   = false;

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
    // 禁用调试功能以降低功耗
    DBGMCU->CR &= ~DBGMCU_CR_DBG_SLEEP;   // 禁用在Sleep模式下的调试
    DBGMCU->CR &= ~DBGMCU_CR_DBG_STOP;    // 禁用在Stop模式下的调试
    DBGMCU->CR &= ~DBGMCU_CR_DBG_STANDBY; // 禁用在Standby模式下的调试

    lpm_timer_reset();

#ifdef BHQ_INT_PIN
    gpio_write_pin_high(BHQ_INT_PIN);
#endif

    // usb
    gpio_set_pin_input(USB_POWER_SENSE_PIN);
    palEnableLineEvent(USB_POWER_SENSE_PIN, PAL_EVENT_MODE_RISING_EDGE);
    lpm_usb_init_flag   = true;

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

# if defined(KB_CHECK_BATTERY_ENABLED)
    battery_disable_read();
#endif
    matrix_sleepConfig();



#ifdef BHQ_IQR_PIN
    gpio_set_pin_input_low(BHQ_IQR_PIN);
    palEnableLineEvent(BHQ_IQR_PIN, PAL_EVENT_MODE_RISING_EDGE);
#endif
#ifdef BHQ_INT_PIN
    gpio_set_pin_input(BHQ_INT_PIN);
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
        stm32_clock_init();
        halInit();
        stInit();
        timer_init();
    chSysUnlock();

#if HAL_USE_ADC
    // halInit()→adcInit() 重置了ADC驱动状态，需同步 adcInitialized[] 标志
    adc_stop_all();
#endif

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
    bhq_common_init();
# if defined(KB_CHECK_BATTERY_ENABLED)
#ifdef BHQ_INT_PIN
    gpio_write_pin_high(BHQ_INT_PIN);
#endif
#endif
    lpm_device_power_open();    // 外围设备 电源 关闭
  
    gpio_write_pin_high(BHQ_INT_PIN);
    report_keyboard_t report = {0};
    bluetooth_send_keyboard(&report);   // 往里面填充一个空的按键包
}

void lpm_via_activity_update(void)
{
    // TODO：这里可以无需实现
}

void lpm_task(void)
{
    if (usb_power_connected()) 
    {
        if(lpm_usb_init_flag == false)
        {
            usb_event_queue_init();
            init_usb_driver(&USBD1);
            lpm_usb_init_flag   = true;
        }
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