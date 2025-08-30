/* Copyright 2021 @ Keychron (https://www.keychron.com)
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

#include "matrix.h"
#include <string.h>
#include "atomic_util.h"
#include "config.h"
#include "wait.h"
#include "analog.h"
#include "km_printf.h"
#include "timer.h"
#include "ec_filter.h"
#include "util.h"

#define ECOUT_ADC_PIN   A1
#define DISCHARGE_PIN   A3

#define MUX1_EN     B4
#define MUX2_EN     B5
#define MUX_SET0    C13
#define MUX_SET1    C14
#define MUX_SET2    C15
#define MUX_SET3    A0

uint32_t matrix_debug_timer = 0;
// A1   ECOUT_ADC  
// A3   DISCHARGE_PIN
// B4  MUX1_EN
// B5  MUX2_EN
// C13  MUX_SET0
// C14  MUX_SET1
// C15  MUX_SET2
// A0   MUX_SET3
#define MUX_SEL_PINS {C13, C14, C15, A0}


ec_filter_RingBuffer ec_adc_buffs[MATRIX_ROWS][MATRIX_COLS];


#ifdef MATRIX_ROW_PINS
static const pin_t row_pins[MATRIX_ROWS] = MATRIX_ROW_PINS;
#endif 

uint16_t key_adcs[MATRIX_ROWS][MATRIX_COLS] = {0};

static const pin_t mux_sel_pins[4] = MUX_SEL_PINS;
static const pin_t mux_en_pins[2] = {MUX1_EN, MUX2_EN};


// 放电
static inline void discharge_capacitor(void) {
    setPinOutput(DISCHARGE_PIN);
    writePinLow(DISCHARGE_PIN);
}
// 充电
static inline void charge_capacitor(uint8_t row) {
    setPinInput(DISCHARGE_PIN); // Z state
    // charge select row
    writePinHigh(row_pins[row]);
}
// 行拉低
static inline void clear_row_pin(uint8_t row) {
    writePinLow(row_pins[row]);
}

// [ MUX 编号 (bits 7–4) ] [ 通道编号 (bits 3–0) ]
#define MUX_EC_KEYMAP(mux_index, channel) (((mux_index) << 4) | ((channel) & 0x0F))

// 提取 MUX 编号（高 4 位）
#define MUX_EC_GET_MUX(addr)       (uint8_t)(((addr) >> 4) & 0x0F)

// 提取通道编号（低 4 位）
#define MUX_EC_GET_CH(addr)        (uint8_t)((addr) & 0x0F)

// 每个矩阵都连了一个mux
static const uint8_t matrix_mux_channels[MATRIX_ROWS][MATRIX_COLS] = {
    {MUX_EC_KEYMAP(0,1), MUX_EC_KEYMAP(0,2), MUX_EC_KEYMAP(0,4), MUX_EC_KEYMAP(0,6),  MUX_EC_KEYMAP(0,7)},
    {MUX_EC_KEYMAP(0,0), MUX_EC_KEYMAP(0,3), MUX_EC_KEYMAP(0,5), MUX_EC_KEYMAP(1,15), MUX_EC_KEYMAP(1,14)},
    {MUX_EC_KEYMAP(1,1), MUX_EC_KEYMAP(1,3), MUX_EC_KEYMAP(1,9), MUX_EC_KEYMAP(1,11), MUX_EC_KEYMAP(1,13)},
    {MUX_EC_KEYMAP(1,0), MUX_EC_KEYMAP(1,2), MUX_EC_KEYMAP(1,8), MUX_EC_KEYMAP(1,10), MUX_EC_KEYMAP(1,12)}
};


// 选择mux的通道号
static inline void select_mux_ch(uint8_t row, uint8_t col)
{
    uint8_t ch = MUX_EC_GET_CH(matrix_mux_channels[row][col]);
    writePin(mux_sel_pins[0], (ch >> 0) & 1);  // S0
    writePin(mux_sel_pins[1], (ch >> 1) & 1);  // S1
    writePin(mux_sel_pins[2], (ch >> 2) & 1);  // S2
    writePin(mux_sel_pins[3], (ch >> 3) & 1);  // S3
}
// 使能引脚
static inline void select_mux_en(uint8_t row, uint8_t col)
{
    uint8_t mux_en_pin_i = MUX_EC_GET_MUX(matrix_mux_channels[row][col]);

    // 关闭所有
    writePinHigh(mux_en_pins[1]);   
    writePinHigh(mux_en_pins[0]);  
    // 打开一个
    writePinLow(mux_en_pins[mux_en_pin_i]);   // 使能一个
}
static inline void select_mux_diAll(void)
{
    // 关闭所有
    writePinHigh(mux_en_pins[1]);   
    writePinHigh(mux_en_pins[0]);  
}





// 读取电容值
static uint16_t ecsm_readkey_raw(uint8_t row, uint8_t col) {

    uint16_t sw_value = 0;
    charge_capacitor(row); // 拉高这一行, 给这一行的电容充电
    // 理论上这里要等下等待充电完成
    wait_us(5);

// 获取滤波后ad值
    ec_filter_alpha_beta(&ec_adc_buffs[row][col],analogReadPin(ECOUT_ADC_PIN));
    sw_value = ec_filter_get_avg_value(&ec_adc_buffs[row][col]);
    // sw_value = analogReadPin(ECOUT_ADC_PIN);
// 获取滤波后ad值

    discharge_capacitor();
    clear_row_pin(row);

    wait_us(5); // 5*1nf*1k = 5us
    return sw_value;
}



static void matrix_init_pins(void) {

}

static bool ecsm_matrix_scan(matrix_row_t current_matrix[]) {
    bool updated = false;
    discharge_capacitor();
    wait_us(20);
     for (int row = 0; row < MATRIX_ROWS; row++) {
        for (int col = 0; col < MATRIX_COLS; col++) {
            // 先选择通道，再打开en
            select_mux_ch(row, col);
            select_mux_en(row, col);
            wait_us(5);
            key_adcs[row][col] = ecsm_readkey_raw(row, col);
        }   
        select_mux_diAll();
    }
    return updated;

}

void matrix_init_custom(void) {
    // initialize key pins
    matrix_init_pins();

    for (int i = 0; i < MATRIX_ROWS; i++) {
        setPinOutput(row_pins[i]);
        writePinLow(row_pins[i]);
    }

    for (uint8_t rowi = 0; rowi < MATRIX_ROWS; rowi++)
    {
        for (uint8_t coli = 0; coli < MATRIX_COLS; coli++)
        {
            key_adcs[rowi][coli] = 0;
        }
    }
    

    setPinOutput(DISCHARGE_PIN);
    writePinLow(DISCHARGE_PIN);

    setPinOutput(mux_en_pins[0]);
    writePinLow(mux_en_pins[0]);

    setPinOutput(mux_en_pins[1]);
    writePinLow(mux_en_pins[1]);



    setPinOutput(mux_sel_pins[0]);
    writePinLow(mux_sel_pins[0]);

    setPinOutput(mux_sel_pins[1]);
    writePinLow(mux_sel_pins[1]);

    setPinOutput(mux_sel_pins[2]);
    writePinLow(mux_sel_pins[2]);

    setPinOutput(mux_sel_pins[3]);
    writePinLow(mux_sel_pins[3]);


    // writePinHigh(mux_sel_pins[0]);
    // writePinLow(mux_sel_pins[1]);
    // writePinLow(mux_sel_pins[2]);
    // writePinLow(MUX1_EN);

    analogReadPin(ECOUT_ADC_PIN);   
    matrix_debug_timer = timer_read32(); 

    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        for (uint8_t c = 0; c < MATRIX_COLS; c++)
        {
            ec_filter_initqueue(&ec_adc_buffs[r][c]);
        }
    }

}



bool matrix_scan_custom(matrix_row_t current_matrix[]) {
    if (timer_elapsed32(matrix_debug_timer) > 100)     // 1分钟
    {
        matrix_debug_timer = timer_read32();
        // uint16_t sw_value = 0;
        // wait_us(16);
        // // raw:0 col:0
        // // MXU选择
        // writePinLow(MUX_SET0);
        // writePinLow(MUX_SET1);
        // writePinLow(MUX_SET2);

        // setPinInput(DISCHARGE_PIN); // 设置为高组态
        // writePinHigh(row_pins[0]);  // 给一行输出高电平，按下后会给电容充电

        // sw_value = analogReadPin(ECOUT_ADC_PIN); // 获取ADC的读数

        // setPinOutput(DISCHARGE_PIN);
        // writePinLow(DISCHARGE_PIN); // 给电容放电
        // writePinLow(row_pins[0]);   // 给一行输出低电平

        // wait_us(10); // 5*1nf*1k = 5us

        // km_printf("raw:0 col:0 adc:%d\n",sw_value);
        km_printf("rowxx ");
        for (uint8_t coli = 0; coli < MATRIX_COLS; coli++)
        {
            km_printf("col%02d\t",coli);
        }
        km_printf("\r\n");

        for (uint8_t rowi = 0; rowi < MATRIX_ROWS; rowi++)
        {
            km_printf("row%02d:",rowi);
            for (uint8_t coli = 0; coli < MATRIX_COLS; coli++)
            {
                km_printf("\t%04d",key_adcs[rowi][coli]);
            }
            km_printf("\r\n");
        }
        km_printf("\r\n");
    }


    uint8_t isUpdate = ecsm_matrix_scan(current_matrix);
    
    if(isUpdate)
    {
        km_printf("matrix update\n");
    }


    return isUpdate;
}
