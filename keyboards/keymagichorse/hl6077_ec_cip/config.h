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
#pragma once


// *********************************************** Cipulot 静电容配置 ***********************************************
// 有多少个mux
#define AMUX_COUNT 2
// mux最大支持多少列吧，1切8  1切16  
#define AMUX_MAX_COLS_COUNT 8

// mux的使能脚 
#define AMUX_EN_PINS \
    { B5, B6}

// mux的set脚
#define AMUX_SEL_PINS \
    { B4, B3, A15 }

// 第一个AMUX有8列，第二个AMUX有7列
#define AMUX_COL_CHANNELS_SIZES \
    { 7 , 7}

// 这里算是COl对应到mux的pin的映射关系。
// 这里的第1个数据是3，那就代表COL0连到了第1个mux的第A3 Pin
// 这里的第2个数据是0，那就代表COL1连到了第1个mux的第A0 Pin
// 这里的第7个数据是5，那就代表COL7连到了第1个mux的第A5 Pin
#define AMUX_0_COL_CHANNELS \
    { 3, 0, 1, 2, 6, 7, 5}
// 这里的第1个数据是1，那就代表COL8连到了第2个mux的第A1 Pin
// 这里的第2个数据是0，那就代表COL9连到了第2个mux的第A0 Pin
// 这里的第7个数据是7，那就代表COL14连到了第2个mux的第A7 Pin
#define AMUX_1_COL_CHANNELS \
    { 1, 0, 2, 3, 6, 5, 7 }
// 这里就是整个COl映射到mux pin的对应关系
#define AMUX_COL_CHANNELS AMUX_0_COL_CHANNELS, AMUX_1_COL_CHANNELS
// 放电脚
#define DISCHARGE_PIN A9
// 检测脚
#define ANALOG_PORT B1
// 放大器使能脚
#define OPAMP_EN_PIN A8
// 高电平使能
#define OPAMP_EN_ACTIVE 1 

#define DEFAULT_ACTUATION_MODE 0
#define DEFAULT_MODE_0_ACTUATION_LEVEL 550
#define DEFAULT_MODE_0_RELEASE_LEVEL 500
#define DEFAULT_MODE_1_INITIAL_DEADZONE_OFFSET DEFAULT_MODE_0_ACTUATION_LEVEL
#define DEFAULT_MODE_1_ACTUATION_OFFSET 70
#define DEFAULT_MODE_1_RELEASE_OFFSET 70
#define DEFAULT_EXTREMUM 1023
#define EXPECTED_NOISE_FLOOR 0
#define NOISE_FLOOR_THRESHOLD 50
#define BOTTOMING_CALIBRATION_THRESHOLD 50
#define DEFAULT_NOISE_FLOOR_SAMPLING_COUNT 30
#define DEFAULT_BOTTOMING_READING 1023
#define DEFAULT_CALIBRATION_STARTER true
// 放电时间
#define DISCHARGE_TIME 1
// typedef struct PACKED {
//     uint8_t  actuation_mode;                                 1                     
//     uint16_t mode_0_actuation_threshold;                     2
//     uint16_t mode_0_release_threshold;                       2
//     uint16_t mode_1_initial_deadzone_offset;                 2           
//     uint8_t  mode_1_actuation_offset;                        1
//     uint8_t  mode_1_release_offset;                          1
//     uint16_t bottoming_reading[MATRIX_ROWS][MATRIX_COLS];    (MATRIX_ROWS * MATRIX_COLS) * 2)
// } eeprom_ec_config_t;
// 1 + 2 + 2 + 2 + 1 + 1 + ((col*row) * 2）
#define EECONFIG_KB_DATA_SIZE (1 + 2 + 2 + 2 + 1 + 1 + ((MATRIX_ROWS * MATRIX_COLS) * 2))
// #define DEBUG_MATRIX_SCAN_RATE
// *********************************************** Cipulot 静电容配置 ***********************************************





#ifdef BLUETOOTH_BHQ
// Its active level is "BHQ_IRQ_AND_INT_LEVEL of bhq.h " 
// #   define BHQ_IQR_PIN          A1             
// #   define BHQ_INT_PIN          A0             
#   define USB_POWER_SENSE_PIN  A10             // USB插入检测引脚  
#   define USB_POWER_CONNECTED_LEVEL   1    

#   define UART_DRIVER          SD2
#   define UART_TX_PIN          A2
#   define UART_TX_PAL_MODE     7
#   define UART_RX_PIN          A3
#   define UART_RX_PAL_MODE	    7

// STM32使用到的高速晶振引脚号，做低功耗需要用户配置，每款芯片有可能不一样的
#define LPM_STM32_HSE_PIN_IN     H1  
#define LPM_STM32_HSE_PIN_OUT    H0

#define REPORT_BUFFER_QUEUE_SIZE    68
#define BATTERY_ADC_PIN              A7


#endif

// ws2812
#define WS2812_POWER_PIN    B14
#define WS2812_BYTE_ORDER   WS2812_BYTE_ORDER_GRB
#define RGBLIGHT_LIMIT_VAL 180
#define RGBLIGHT_LAYER_BLINK
#define RGBLIGHT_LAYERS_RETAIN_VAL



