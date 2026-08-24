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
 * @file lpm_chip_at32f4.h
 * @brief AT32F415 芯片专属低功耗配置
 *
 * 提供 AT32F415 特有的 STOP 模式寄存器操作、时钟初始化、
 * USB 引脚配置。AT32F415 不支持 RTC 唤醒低功耗模式。
 */

#pragma once
#include "quantum.h"
#include "gpio.h"

/* ------------------------------------------------------------------ */
/*  通用接口                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief 进入 AT32F415 STOP 模式
 *
 * 流程：配置 LDO 低功耗 → 设置 LPSEL → __WFI() → 恢复 LDO
 */
static inline void lpm_chip_enter_stop_mode(void) {
    /* 配置 LDO 为低功耗 */
    PWC->CTRL |= PWC_CTRL_VRSEL;
    /* AT32 进入深度睡眠模式 */
    PWC->CTRL &= ~PWC_CTRL_LPSEL;
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    __WFI();

    /* 恢复 LDO */
    PWC->CTRL &= ~PWC_CTRL_VRSEL;
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
}

/**
 * @brief 唤醒后重新初始化 AT32 时钟
 */
static inline void lpm_chip_clock_init(void) {
    at32_clock_init();
}

/**
 * @brief 禁用调试功能以降低功耗（AT32 无此操作）
 */
static inline void lpm_chip_debug_disable(void) {
    /* AT32F415 不支持通过 DBGMCU 禁用调试 */
}

/**
 * @brief 唤醒后配置 USB D+/D- 引脚
 *
 * AT32 的 USB 引脚配置由 USB 驱动自行管理，此处无需操作。
 */
static inline void lpm_chip_usb_pins_init(void) {
    /* AT32 USB 引脚配置由 USB 驱动管理 */
}

/**
 * @brief 休眠前将 USB D+/D- 设为模拟输入（AT32 不需要）
 */
static inline void lpm_chip_usb_pins_sleep(void) {
    /* AT32 不需要在休眠前将 USB D+/D- 设为模拟输入 */
}

/* ------------------------------------------------------------------ */
/*  RTC 接口 - AT32F415 不支持 RTC 唤醒低功耗                           */
/* ------------------------------------------------------------------ */
/*  当定义了 LPM_RTC_WAKEUP 时提供空实现，避免编译错误。               */
/*  正常情况下不应在 AT32 上启用 RTC 唤醒。                            */

#ifdef LPM_RTC_WAKEUP
static inline void lpm_chip_rtc_init(void) {
    /* AT32F415 不支持 RTC 唤醒 */
}

static inline void lpm_chip_rtc_wakeup_setup(uint32_t interval_ms) {
    (void)interval_ms;
    /* AT32F415 不支持 RTC 唤醒 */
}

static inline void lpm_chip_rtc_wakeup_clear(void) {
    /* AT32F415 不支持 RTC 唤醒 */
}

/**
 * @brief 禁用 RTC 周期性唤醒（低电量时调用，只保留 USB 唤醒）
 */
static inline void lpm_chip_rtc_wakeup_disable(void) {
    /* AT32F415 不支持 RTC 唤醒 */
}
#endif
