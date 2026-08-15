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
 * @file lpm_chip.h
 * @brief 芯片抽象层选择器
 *
 * 根据 KB_LPM_DRIVER 宏自动选择对应芯片的头文件。
 * 每个芯片头文件必须提供以下 static inline 函数：
 *
 * 通用接口（所有芯片）：
 *   - lpm_chip_enter_stop_mode()      进入 STOP 模式
 *   - lpm_chip_clock_init()           唤醒后重新初始化时钟
 *   - lpm_chip_debug_disable()        禁用调试功能以降低功耗
 *   - lpm_chip_usb_pins_init()        唤醒后配置 USB D+/D- 引脚
 *   - lpm_chip_usb_pins_sleep()       休眠前将 USB D+/D- 设为模拟输入
 *
 * RTC 接口（仅 LPM_RTC_WAKEUP 定义时）：
 *   - lpm_chip_rtc_init()             RTC 初始化（在 lpm_init 中调用）
 *   - lpm_chip_rtc_wakeup_setup(ms)   配置 RTC 唤醒（在休眠前调用）
 *   - lpm_chip_rtc_wakeup_clear()     清除 RTC 唤醒标志（在唤醒后调用）
 */

#pragma once
#include "quantum.h"

#if defined(LPM_CHIP_STM32F4)
#    include "lpm_chip_stm32f4.h"
#elif defined(LPM_CHIP_STM32F1)
#    include "lpm_chip_stm32f1.h"
#elif defined(LPM_CHIP_AT32)
#    include "lpm_chip_at32f4.h"
#else
#    error "No LPM chip family defined. Check KB_LPM_DRIVER in rules.mk"
#endif
