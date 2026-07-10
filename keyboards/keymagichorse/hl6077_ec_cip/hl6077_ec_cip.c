// /* Copyright 2024 keymagichorse
//  *
//  * This program is free software: you can redistribute it and/or modify
//  * it under the terms of the GNU General Public License as published by
//  * the Free Software Foundation, either version 2 of the License, or
//  * (at your option) any later version.
//  *
//  * This program is distributed in the hope that it will be useful,
//  * but WITHOUT ANY WARRANTY; without even the implied warranty of
//  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  * GNU General Public License for more details.
//  *
//  * You should have received a copy of the GNU General Public License
//  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
//  */
// #include "quantum.h"
// #include "analog.h"
// // static uint32_t matrix_debug_timer = 0;

// void housekeeping_task_kb(void) {
//     // if (timer_elapsed32(matrix_debug_timer) > 500) // 1分钟
//     // {
//     //     matrix_debug_timer = timer_read32();
//     //     int16_t adc        = analogReadPin(A7);
//     //     adc_mux VREFINT_mux = {ADC_CHANNEL_VREFINT, 0};
//     //     int16_t VREFINT_v        = adc_read(VREFINT_mux);
//     //     uprintf("adc:%d  VREFINT_v:%d  \r\n", adc, VREFINT_v);
//     //     uint16_t mv_div = (adc * 3300UL) / 1024; 
//     //     uint16_t battery_mv      = (uint16_t)((uint32_t)mv_div * (100 + 100) / 100);
//     //     uprintf("adc:%d  mv_div:%d  battery_mv:%d  \r\n", adc, mv_div, battery_mv);

//     //     battery_mv = battery_mv + battery_mv - battery_mv;
//     //     VREFINT_v = VREFINT_v + VREFINT_v - VREFINT_v;
//     // }
// }
// // static uint32_t matrix_debug_timer = 0;

// // void housekeeping_task_kb(void)
// // {
// //     if (timer_elapsed32(matrix_debug_timer) < 500) {
// //         return;
// //     }

// //     matrix_debug_timer = timer_read32();

// //     /* ---------- 电池ADC ---------- */
// //     uint16_t adc = analogReadPin(A7);

// //     /* ---------- 内部参考 ---------- */
// //     adc_mux vrefint_mux = {
// //         ADC_CHANNEL_VREFINT,
// //         0
// //     };

// //     uint16_t vref = adc_read(vrefint_mux);

// //     /* ---------- VDDA ---------- */
// //     uint32_t vdda_mv =
// //         (1210UL * 4095UL) / vref;

// //     /* ---------- ADC转mV ---------- */
// //     uint32_t adc_mv =
// //         ((uint32_t)adc * vdda_mv) / 4095UL;

// //     /* ---------- 分压还原 ---------- */
// //     uint32_t battery_mv =
// //         adc_mv * 2;

// //     uprintf(
// //         "adc:%u vref:%u vdda:%lumV adc_mv:%lumV bat:%lumV\r\n",
// //         adc,
// //         vref,
// //         vdda_mv,
// //         adc_mv,
// //         battery_mv
// //     );
// // }

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

#if defined(BLUETOOTH_BHQ)
#   include "bhq.h"
#   include "bhq_common.h"
#endif

#if defined(KB_LPM_ENABLED)
#   include "lpm.h"
#endif

#if defined(KB_DEBUG)
#   include "km_printf.h"
#endif
#include "battery.h"


void board_init(void) 
{
#if defined(BLUETOOTH_BHQ)
    bhq_common_init();
#   if defined(KB_LPM_ENABLED)
    lpm_init();
#   endif
#endif
}

void housekeeping_task_kb(void) {
#if defined(BLUETOOTH_BHQ)
    bhq_wireless_task();
    #   if defined(KB_LPM_ENABLED)
        lpm_task();
    #   endif
#endif
}
