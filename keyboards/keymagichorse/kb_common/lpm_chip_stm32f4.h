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
 * @file lpm_chip_stm32f4.h
 * @brief STM32F4 芯片专属低功耗配置
 *
 * 提供 STM32F4 特有的 STOP 模式寄存器操作、时钟初始化、
 * USB 引脚配置和 RTC 唤醒（使用 ChibiOS RTC API）。
 */

#pragma once
#include "quantum.h"
#include "gpio.h"

#ifdef LPM_RTC_WAKEUP
#    include "hal.h"
#endif

/* ------------------------------------------------------------------ */
/*  通用接口                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief 进入 STM32F4 STOP 模式
 *
 * 流程：关闭 HSE → 配置 PWR CR 寄存器 → __WFI() → 清除 SLEEPDEEP
 */
static inline void lpm_chip_enter_stop_mode(void) {
#if STM32_HSE_ENABLED
    /* Switch to HSI */
    RCC->CFGR = (RCC->CFGR & (~STM32_SW_MASK)) | STM32_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != (STM32_SW_HSI << 2))
        ;

    /* Set HSE off */
    RCC->CR &= ~RCC_CR_HSEON;
    while ((RCC->CR & RCC_CR_HSERDY))
        ;

    palSetLineMode(LPM_STM32_HSE_PIN_IN, PAL_MODE_INPUT_ANALOG);
    palSetLineMode(LPM_STM32_HSE_PIN_OUT, PAL_MODE_INPUT_ANALOG);
#endif

    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    PWR->CR |= PWR_CR_MRLVDS | PWR_CR_LPLVDS | PWR_CR_FPDS | PWR_CR_LPDS;
    __WFI();

    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
}

/**
 * @brief 唤醒后重新初始化 STM32 时钟
 */
static inline void lpm_chip_clock_init(void) {
    stm32_clock_init();
}

/**
 * @brief 禁用调试功能以降低功耗
 */
static inline void lpm_chip_debug_disable(void) {
    DBGMCU->CR &= ~DBGMCU_CR_DBG_SLEEP;
    DBGMCU->CR &= ~DBGMCU_CR_DBG_STOP;
    DBGMCU->CR &= ~DBGMCU_CR_DBG_STANDBY;
}

/**
 * @brief 唤醒后配置 USB D+/D- 引脚
 */
static inline void lpm_chip_usb_pins_init(void) {
    palSetLineMode(A11, PAL_STM32_OTYPE_PUSHPULL | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_FLOATING | PAL_MODE_ALTERNATE(10U));
    palSetLineMode(A12, PAL_STM32_OTYPE_PUSHPULL | PAL_STM32_OSPEED_HIGHEST | PAL_STM32_PUPDR_FLOATING | PAL_MODE_ALTERNATE(10U));
}

/**
 * @brief 休眠前将 USB D+/D- 设为模拟输入（F4 不需要）
 */
static inline void lpm_chip_usb_pins_sleep(void) {
    /* STM32F4 不需要在休眠前将 USB D+/D- 设为模拟输入 */
}

/* ------------------------------------------------------------------ */
/*  RTC 接口                                                           */
/* ------------------------------------------------------------------ */

#ifdef LPM_RTC_WAKEUP

/**
 * @brief RTC 初始化（F4 由 ChibiOS 处理，无需额外操作）
 */
static inline void lpm_chip_rtc_init(void) {
    /* ChibiOS RTC 驱动在 halInit() 中自动初始化，无需额外操作 */
}

/**
 * @brief 将毫秒转换为 RTC WUTR 寄存器值
 */
static inline uint32_t lpm_rtc_wakeup_calc(uint32_t ms) {
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

/**
 * @brief 配置 STM32F4 RTC 周期性唤醒
 * @param interval_ms 唤醒间隔（毫秒）
 */
static inline void lpm_chip_rtc_wakeup_setup(uint32_t interval_ms) {
    RTCWakeup wakeupspec;
    wakeupspec.wutr = lpm_rtc_wakeup_calc(interval_ms);
    rtcSTM32SetPeriodicWakeup(&RTCD1, &wakeupspec);
    rtcSetCallback(&RTCD1, NULL);
}

/**
 * @brief 清除 RTC 唤醒标志（F4 由 ChibiOS 处理，无需额外操作）
 */
static inline void lpm_chip_rtc_wakeup_clear(void) {
    /* ChibiOS RTC 驱动自动处理标志清除 */
}

#endif /* LPM_RTC_WAKEUP */
