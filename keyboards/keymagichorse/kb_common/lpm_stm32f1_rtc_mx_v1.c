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
#include "bluetooth.h"
#include "matrix_sleep.h"

#if HAL_USE_ADC
#    include "analog.h"
#endif

static uint32_t     lpm_timer_buffer = 0;
static bool         lpm_time_up               = false;
bool is_lpm_via_activity_flag = false;
uint32_t lpm_via_activity_timer = 0;

typedef enum{
    RTC_LIGHT_SLEEP_MODE = 0,
    RTC_DEEP_SLEEP_MODE
}rtc_sleep_mode_enum;
rtc_sleep_mode_enum ret_sleep_mode = RTC_LIGHT_SLEEP_MODE;

#if (DIODE_DIRECTION == COL2ROW)
    static const pin_t wakeUpCol_pins[MATRIX_COLS]   = MATRIX_COL_PINS;
#elif (DIODE_DIRECTION == ROW2COL)
    static const pin_t wakeUpRow_pins[MATRIX_ROWS] = MATRIX_ROW_PINS;
#endif

void lpm_timer_reset(void) {
    lpm_time_up      = false;
    lpm_timer_buffer = 0;
}

__attribute__((weak)) void lpm_device_power_open(void) {}
__attribute__((weak)) void lpm_device_power_close(void) {}
// 将未使用的引脚设置为输入模拟
__attribute__((weak)) void lpm_set_unused_pins_to_input_analog(void){}

RTCDateTime timespec;
RTCAlarm alarmspec;
void rtc_wakeup_set(rtc_sleep_mode_enum mode_enmu)
{
    ret_sleep_mode = mode_enmu;
    // 1. 等待 RTC 寄存器同步（确保上一次操作完成）
    while (!(RTC->CRL & RTC_CRL_RTOFF)); 

    // 2. 进入配置模式
    RTC->CRL |= RTC_CRL_CNF;
    switch (mode_enmu)
    {
        case RTC_LIGHT_SLEEP_MODE:
            // 125ms
            RTC->PRLH = 0;
            RTC->PRLL = 2047; 
            break;

        case RTC_DEEP_SLEEP_MODE:
            // 410ms
            RTC->PRLH = 0;
            RTC->PRLL = 8181; 
            break;
    }

    // 4. 退出配置模式
    RTC->CRL &= ~RTC_CRL_CNF;

    // 5. 等待写操作完成
    while (!(RTC->CRL & RTC_CRL_RTOFF));
}

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

    lpm_device_power_open();
    rtc_wakeup_set(RTC_LIGHT_SLEEP_MODE);
}


void My_PWR_EnterSTOPMode(void)
{
#if STM32_HSE_ENABLED
    /* Switch to HSI */
    RCC->CFGR = (RCC->CFGR & (~RCC_CFGR_SW)) | RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI);

    /* Set HSE off  */
    RCC->CR &= ~RCC_CR_HSEON;
    while (RCC->CR & RCC_CR_HSERDY);

    palSetLineMode(LPM_STM32_HSE_PIN_IN, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(LPM_STM32_HSE_PIN_OUT, PAL_MODE_INPUT_ANALOG); 

#endif

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR &= ~PWR_CR_PDDS; 
    PWR->CR |= PWR_CR_LPDS;
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    
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

    matrix_sleepConfig();

// rtc唤醒
    uint32_t tv_sec;
    /* compile ability test */
    rtcGetTime(&RTCD1, &timespec);
    /* set alarm in near future */
    rtcSTM32GetSecMsec(&RTCD1, &tv_sec, NULL);
    alarmspec.tv_sec = tv_sec + 1;
    rtcSetAlarm(&RTCD1, 0, &alarmspec);

    // 2. 打通 EXTI Line 17 (RTC Alarm -> 内核)
    // ChibiOS 虽然有 EXTI 驱动，但直接写寄存器最稳健且不增加代码体积
    EXTI->PR = EXTI_PR_PR17;         // 清除挂起标志
    EXTI->IMR |= EXTI_IMR_MR17;      // 允许 EXTI 17 中断
    EXTI->RTSR |= EXTI_RTSR_TR17;    // 必须上升沿触发
// rtc唤醒

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
    /*  USB D+/D- */
    palSetLineMode(A11, PAL_MODE_INPUT_ANALOG);  
    palSetLineMode(A12, PAL_MODE_INPUT_ANALOG); 

    bhq_Disable();
#if HAL_USE_ADC
    adc_stop_all();
#endif
    lpm_device_power_close();    // 外围设备 电源 关闭
    My_PWR_EnterSTOPMode();

}

void exit_low_power_mode_prepare(void)
{
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
    palSetLineMode(A11, PAL_MODE_STM32_ALTERNATE_PUSHPULL);  
    palSetLineMode(A12, PAL_MODE_STM32_ALTERNATE_PUSHPULL);  
    usb_event_queue_init();
    init_usb_driver(&USBD1);

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
#ifdef BHQ_INT_PIN
    gpio_write_pin_high(BHQ_INT_PIN);
#endif

    lpm_device_power_open();    // 外围设备 电源 关闭
  
    gpio_write_pin_high(BHQ_INT_PIN);
    report_keyboard_t report = {0};
    bluetooth_send_keyboard(&report);   // 往里面填充一个空的按键包
}

bool lowpower_matrix_task(void) 
{
    bool any_key_pressed = false; 

    uint8_t i = 0;
#if (DIODE_DIRECTION == COL2ROW)
    // Set row(low valid), read cols
    for (i = 0; i < matrix_cols(); i++)
    { // set col pull-up input
        if(wakeUpCol_pins[i] == NO_PIN)
        {
            continue;
        } 
        if(gpio_read_pin(wakeUpCol_pins[i]) == 0 )
        {
            any_key_pressed = true; 
            return any_key_pressed; 
        }
    }
#elif (DIODE_DIRECTION == ROW2COL)
    // 读取row 有一行是低电平那就唤醒
    // Set col(low valid), read rows
    for (i = 0; i < matrix_rows(); i++)
    { // set row pull-up input
        if(wakeUpRow_pins[i] == NO_PIN)
        {
            continue;
        } 
        if(gpio_read_pin(wakeUpRow_pins[i]) == 0 )
        {
            any_key_pressed = true; 
            return any_key_pressed; 
        }
    }
#endif
    return any_key_pressed; 
}

void lpm_via_activity_update(void)
{
    lpm_via_activity_timer = sync_timer_read32();
    is_lpm_via_activity_flag = true;
}

void lpm_task(void)
{
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
    
    if(wireless_get() == WT_STATE_ADV_UNPAIRED || wireless_get() == WT_STATE_ADV_PAIRING)
    {
        lpm_time_up = false;
        lpm_timer_buffer = 0;
        return;
    }
    
    if (is_lpm_via_activity_flag == true)
    {
        if(sync_timer_elapsed32(lpm_via_activity_timer) > (2000 * 60)) 
        {
            lpm_time_up = false;
            lpm_timer_buffer = 0;
            is_lpm_via_activity_flag = false;
            return;
        }
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
// rtc唤醒逻辑 start 
        uint8_t temp_cut = 0;
        if (EXTI->PR & EXTI_PR_PR17) {
            EXTI->PR = EXTI_PR_PR17; 
            while(1)
            {
                // usb插入时,直接唤醒
                if(usb_power_connected())
                {
                    exit_low_power_mode_prepare();
                    return;
                }
                if(lowpower_matrix_task())
                {
                    break;
                }
                else
                {
                    temp_cut++;
                    if(temp_cut >= 5)
                    {
                        temp_cut = 0;
                        enter_low_power_mode_prepare();
                    }
                }
                wait_us(50);
            }
        } 
        else
        {
            exit_low_power_mode_prepare();
        }
// rtc唤醒逻辑 end 
        exit_low_power_mode_prepare();
    }
}
