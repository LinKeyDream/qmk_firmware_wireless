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
 * @file lpm_chip_stm32f1.h
 * @brief STM32F1 芯片专属低功耗配置
 *
 * 提供 STM32F1 特有的 STOP 模式寄存器操作、时钟初始化、
 * USB 引脚配置和 RTC 唤醒（使用直接寄存器操作 + EXTI Line 17）。
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
 * @brief 进入 STM32F1 STOP 模式
 *
 * 流程：关闭 HSE → 使能 PWR 时钟 → 配置 PDDS/LPDS → __WFI() → 清除 SLEEPDEEP
 */
static inline void lpm_chip_enter_stop_mode(void) {
#if STM32_HSE_ENABLED
    /* Switch to HSI */
    RCC->CFGR = (RCC->CFGR & (~RCC_CFGR_SW)) | RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI)
        ;

    /* Set HSE off */
    RCC->CR &= ~RCC_CR_HSEON;
    while (RCC->CR & RCC_CR_HSERDY)
        ;

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
 * @brief 唤醒后配置 USB D+/D- 引脚（F1 使用 Alternate Push-Pull 模式）
 */
static inline void lpm_chip_usb_pins_init(void) {
    palSetLineMode(A11, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
    palSetLineMode(A12, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
}

/**
 * @brief 休眠前将 USB D+/D- 设为模拟输入
 */
static inline void lpm_chip_usb_pins_sleep(void) {
    palSetLineMode(A11, PAL_MODE_INPUT_ANALOG);
    palSetLineMode(A12, PAL_MODE_INPUT_ANALOG);
}

/* ------------------------------------------------------------------ */
/*  RTC 接口                                                           */
/* ------------------------------------------------------------------ */

#ifdef LPM_RTC_WAKEUP

/** RTC 闹钟变量（静态，避免每次调用重新分配栈） */
static RTCDateTime lpm_f1_rtc_timespec;
static RTCAlarm    lpm_f1_alarmspec;

/**
 * @brief RTC 初始化 - 设置预分频器
 *
 * F1 的 RTC 使用直接寄存器操作，需要在初始化时设置预分频器
 * 以确定唤醒间隔。
 */
static inline void lpm_chip_rtc_init(void) {
    /* 等待 RTC 寄存器同步 */
    while (!(RTC->CRL & RTC_CRL_RTOFF))
        ;
    /* 进入配置模式 */
    RTC->CRL |= RTC_CRL_CNF;
    /* 设置预分频器：LSE 32768Hz / 2048 = 16Hz → 62.5ms/tick */
    RTC->PRLH = 0;
    RTC->PRLL = 2047;
    /* 退出配置模式 */
    RTC->CRL &= ~RTC_CRL_CNF;
    /* 等待写操作完成 */
    while (!(RTC->CRL & RTC_CRL_RTOFF))
        ;
}

/**
 * @brief 配置 STM32F1 RTC 闹钟唤醒
 *
 * F1 使用 RTC 闹钟 + EXTI Line 17 实现周期性唤醒。
 * 闹钟设在当前时间 + interval_ms 对应的秒数后。
 *
 * @param interval_ms 唤醒间隔（毫秒），F1 使用固定约 1 秒间隔
 */
static inline void lpm_chip_rtc_wakeup_setup(uint32_t interval_ms) {
    (void)interval_ms; /* F1 使用固定闹钟间隔，忽略此参数 */

    uint32_t tv_sec;
    /* 获取当前 RTC 时间 */
    rtcGetTime(&RTCD1, &lpm_f1_rtc_timespec);
    /* 获取当前秒数 */
    rtcSTM32GetSecMsec(&RTCD1, &tv_sec, NULL);
    /* 设置闹钟：当前时间 + 1 秒 */
    lpm_f1_alarmspec.tv_sec = tv_sec + 1;
    rtcSetAlarm(&RTCD1, 0, &lpm_f1_alarmspec);

    /* 打通 EXTI Line 17（RTC Alarm → 内核） */
    EXTI->PR  = EXTI_PR_PR17;      /* 清除挂起标志 */
    EXTI->IMR |= EXTI_IMR_MR17;    /* 允许 EXTI 17 中断 */
    EXTI->RTSR |= EXTI_RTSR_TR17;  /* 必须上升沿触发 */
}

/**
 * @brief 清除 RTC 唤醒标志
 */
static inline void lpm_chip_rtc_wakeup_clear(void) {
    /* 清除 EXTI Line 17 挂起标志 */
    if (EXTI->PR & EXTI_PR_PR17) {
        EXTI->PR = EXTI_PR_PR17;
    }
}

#endif /* LPM_RTC_WAKEUP */
