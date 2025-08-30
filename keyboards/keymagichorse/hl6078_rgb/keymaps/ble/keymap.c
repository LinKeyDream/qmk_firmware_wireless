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
#include QMK_KEYBOARD_H
#include "config.h"

#include "ws2812.h"
#include "color.h"

#include "bhq_common.h"
#include "wireless.h"
#include "transport.h"
#include "report_buffer.h"
#include "battery.h"

#   if defined(KB_LPM_ENABLED)

#endif
// 临时变量，用于临时存放矩阵灯是否开启
uint8_t is_sleep = 0;
uint8_t rgb_matrix_is_enabled_temp_v = 0;

// 是否显示rgb电量指示灯
uint8_t rgb_bat_show_flag  = 0;
uint16_t rgb_bat_led_idx[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
#define RGB_BAT      QK_USER_1       
// 低电量提示
uint8_t rgb_bat_low_flag = 0;

// 唤醒后延时点亮 RGB 的标志位
static uint8_t lpm_wakeup_delay_open_rgb_flag = 0;
// 记录唤醒时间
static uint32_t lpm_wakeup_delay_open_rgb_timer = 0;


const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  [0] = LAYOUT(
    QK_GESC, KC_1,    KC_2,     KC_3,     KC_4,    KC_5,    KC_6,    KC_7,    KC_8,      KC_9,     KC_0,     KC_MINS,  KC_EQL,  KC_BSLS, KC_BSPC,
    KC_TAB,  KC_Q,    KC_W,     KC_E,     KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,      KC_O,     KC_P,     KC_LBRC,  KC_RBRC, KC_BSLS,
    KC_CAPS, KC_A,    KC_S,     KC_D,     KC_F,    KC_G,    KC_H,    KC_J,    KC_K,      KC_L,     KC_SCLN,  KC_QUOT,  KC_BSLS, KC_ENT,
    KC_LSFT, KC_Z,    KC_X,     KC_C,     KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM,   KC_DOT,   KC_SLSH,  KC_RSFT,  KC_UP,   KC_DEL,
    KC_LCTL, KC_LGUI, KC_LALT,  KC_SPC,  KC_SPC,                    KC_SPC,    MO(1),    KC_RCTL,    KC_LEFT,  KC_DOWN, KC_RIGHT),
  [1] = LAYOUT(
    KC_GRV , KC_F1,   KC_F2,   KC_F3,    KC_F4,   KC_F5,   KC_F6,   KC_F7,    KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,  KC_TRNS, KC_DEL,
    KC_TRNS, BL_SW_0, BL_SW_1, BL_SW_2,  RF_TOG, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, USB_TOG, NK_TOGG, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  RM_TOGG, RM_NEXT, RM_PREV, KC_TRNS, KC_TRNS, KC_BRIU, KC_TRNS,
    KC_TRNS, GU_TOGG, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, RGB_BAT, KC_VOLD, KC_BRID, KC_VOLU),
  [2] = LAYOUT(
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS),
  [3] = LAYOUT(
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                    KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS)
};


bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if(keycode == RGB_BAT)
    {
        if(record->event.pressed)
        {
            rgb_bat_show_flag  = 1;
        }
        else
        {
            rgb_bat_show_flag  = 0;
        }
    }
    return process_record_bhq(keycode, record);
}

//  每个通道的颜色 以及大写按键的颜色
// HSV_BLUE        // 蓝牙 蓝色
// HSV_PURPLE      // 大小写：紫色
// HSV_RED         // 低电量：红色
typedef struct {
    uint8_t index;       // LED 索引
    uint16_t blink_nums;      // 剩余闪烁次数（0 = 不闪）
    uint16_t on_time;    // 亮的时长 (ms)
    uint16_t off_time;   // 灭的时长 (ms)

    uint8_t red;
    uint8_t green;
    uint8_t blue;

    // 内部状态
    uint8_t active;      // 是否激活
    uint8_t is_on;       // 当前亮灭状态
    uint16_t counter;    // 当前阶段计数器
} user_blink_task_t;

#define MAX_BLINK_TASKS 2   // 最多同时 2 个闪烁任务
static user_blink_task_t blink_tasks[MAX_BLINK_TASKS];
void add_blink_task(int index, uint8_t red, uint8_t green, uint8_t blue, uint16_t blink_nums, uint16_t on_ms, uint16_t off_ms) {
    if(blink_nums == 0)
    {
        blink_nums = blink_nums;
    }
    for (int i = 0; i < MAX_BLINK_TASKS; i++) {
        if (!blink_tasks[i].active) {
            blink_tasks[i].index        = index;
            blink_tasks[i].blink_nums   = blink_nums;
            blink_tasks[i].on_time      = on_ms;
            blink_tasks[i].off_time     = off_ms;
            blink_tasks[i].red          = red;
            blink_tasks[i].green        = green;
            blink_tasks[i].blue         = blue;
            blink_tasks[i].active       = 1;
            blink_tasks[i].is_on        = 1;
            blink_tasks[i].counter      = 0;
            break;
        }
    }
}
void del_all_blink_task(void)
{
    for (int i = 0; i < MAX_BLINK_TASKS; i++) {
        blink_tasks[i].index        = 0;
        blink_tasks[i].blink_nums   = 0;
        blink_tasks[i].on_time      = 0;
        blink_tasks[i].off_time     = 0;
        blink_tasks[i].red          = 0;
        blink_tasks[i].green        = 0;
        blink_tasks[i].blue         = 0;
        blink_tasks[i].active       = 0;
        blink_tasks[i].is_on        = 0;
        blink_tasks[i].counter      = 0;
    }
}

void rgb_matrix_all_black(void)
{
    for (size_t i = 0; i < RGB_MATRIX_LED_COUNT; i++)
    {
        rgb_matrix_set_color(i, RGB_BLACK);
    }
}
// 矩阵灯任务
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    if (lpm_wakeup_delay_open_rgb_flag == 1) {
        lpm_wakeup_delay_open_rgb_flag = 2;
        lpm_wakeup_delay_open_rgb_timer = timer_read32();
        rgb_matrix_all_black();
        return false;
    }
    if (lpm_wakeup_delay_open_rgb_flag == 2) {
        rgb_matrix_all_black();
        if (timer_elapsed32(lpm_wakeup_delay_open_rgb_timer) > 500) { // 延时 500ms
            lpm_wakeup_delay_open_rgb_flag = 0;
        }
        return false;
    }

// **************************** 低电量闪烁逻辑 ****************************
    if (rgb_bat_low_flag == 1) {
        static uint16_t last_toggle = 0;   // 上次切换时间
        static bool led_on = false;        // 当前红灯状态

        // 每500ms切换一次状态
        if (timer_elapsed(last_toggle) > 500) {
            led_on = !led_on;              // 状态取反
            last_toggle = timer_read();    // 更新时间戳
        }
        rgb_matrix_all_black();
        // 根据 led_on 状态设置颜色
        if (led_on) {
            rgb_matrix_set_color(0, RGB_RED); 
        } else {
            rgb_matrix_set_color(0, RGB_BLACK);      
        }
        return false;  
    }
// **************************** 低电量闪烁逻辑 ****************************

    // 如果当前是USB连接，或者是蓝牙/2.4G连接且已配对连接状态
    if( (transport_get() > KB_TRANSPORT_USB && wireless_get() == WT_STATE_CONNECTED) || ( usb_power_connected() == true && transport_get() == KB_TRANSPORT_USB))
    {
        // 两个大写灯
        if (host_keyboard_led_state().caps_lock) {
            // 两个大写灯
            rgb_matrix_set_color(30, RGB_PURPLE); 
            rgb_matrix_set_color(31, RGB_PURPLE);

            // QWE
            // rgb_matrix_set_color(17, 255, 255, 255);
            // rgb_matrix_set_color(18, 255, 255, 255);
            // rgb_matrix_set_color(19, 255, 255, 255);
        }
    }  
    // usb模式时，没有枚举成功，就强行灭灯
    if(transport_get() == KB_TRANSPORT_USB)
    {
        if(USBD1.state != USB_ACTIVE)
        {
            rgb_matrix_all_black();
        }
    }
    // 无线模式时，没有连接成功，就强行灭灯
    if(transport_get() > KB_TRANSPORT_USB)
    {
        if(wireless_get() != WT_STATE_CONNECTED)
        {
            rgb_matrix_all_black();
        }
    }



// ************** 闪烁rgb灯逻辑 **************
    for (int i = 0; i < MAX_BLINK_TASKS; i++) {
        if (!blink_tasks[i].active) continue;
        // 时间推进
        blink_tasks[i].counter++;
        if (blink_tasks[i].is_on) {
            if (blink_tasks[i].counter >= blink_tasks[i].on_time) {
                blink_tasks[i].is_on = 0;
                blink_tasks[i].counter = 0;
                if (blink_tasks[i].blink_nums > 0 && --blink_tasks[i].blink_nums == 0) {
                    blink_tasks[i].active = 0;
                }
            }
        } else {
            if (blink_tasks[i].counter >= blink_tasks[i].off_time) {
                blink_tasks[i].is_on = 1;
                blink_tasks[i].counter = 0;
            }
        }
        if (blink_tasks[i].active && blink_tasks[i].is_on) {
            rgb_matrix_set_color(blink_tasks[i].index, blink_tasks[i].red, blink_tasks[i].green, blink_tasks[i].blue); 
        } else {
            rgb_matrix_set_color(blink_tasks[i].index, 0, 0, 0);  
        }
    }
// ************** 闪烁rgb灯逻辑 **************


// ************** 电量百分比 亮灯逻辑 **************
    if(rgb_bat_show_flag  == 1)   
    {
        rgb_matrix_all_black();
        uint8_t bat_led_count = battery_get() / 10;
        if (battery_get() > 0 && bat_led_count == 0) {
            bat_led_count = 1;  
        }
        if (bat_led_count > 10) {
            bat_led_count = 10; 
        }
        uint8_t r = 0, g = 0, b = 0;
        if (battery_get() < 30) {
            r = 0xFF; g = 0x00; b = 0x00;   // 红色（低电量）
        } else if (battery_get() < 70) {
            r = 0xFF; g = 0xFF; b = 0x00;   // 黄色（中电量）
        } else {
            r = 0x00; g = 0xFF; b = 0x00;   // 绿色（高电量）
        }
        for (size_t j = 0; j < bat_led_count; j++) {
            uint8_t led_index = rgb_bat_led_idx[j];
            if (led_index < RGB_MATRIX_LED_COUNT) {
                rgb_matrix_set_color(led_index, r, g, b);
            }
        }
        return false;   
    }
// ************** 电量百分比 亮灯逻辑 **************
    return false;
}

// 无线蓝牙回调函数
void wireless_ble_hanlde_kb(uint8_t host_index,uint8_t advertSta,uint8_t connectSta,uint8_t pairingSta)
{
    km_printf("wireless_ble_hanlde_kb->host_index: %d\r\n",host_index);
    del_all_blink_task();
    // 蓝牙没有连接 && 蓝牙广播开启  && 蓝牙配对模式
    if(connectSta != 1 && advertSta == 1 && pairingSta == 1)
    {
        // 这里第一个参数使用host_index正好对应_rgb_layers的索引
        add_blink_task(17 + host_index, RGB_BLUE, 0, 100, 100);
        km_printf("if 1\n");
    }
    // 蓝牙没有连接 && 蓝牙广播开启  && 蓝牙非配对模式
    else if(connectSta != 1 && advertSta == 1 && pairingSta == 0)
    {
        add_blink_task(17 + host_index, RGB_BLUE, 0, 200, 300);
        km_printf("if 2\n");
    }
    else if(connectSta != 1 && advertSta == 0 && pairingSta == 0)
    {
        del_all_blink_task();
        km_printf("if 3\n");
    }
    // 蓝牙已连接
    if(connectSta == 1)
    {
        report_buffer_clear();
        layer_clear();
        add_blink_task(17 + host_index, RGB_BLUE, 5, 50, 50);
        km_printf("if 4\n");
    }
}
void bhq_set_lowbat_led(bool on)
{
    rgb_bat_low_flag = on;
}



void wireless_rf24g_hanlde_kb(uint8_t connectSta,uint8_t pairingSta)
{
    if(connectSta == 1)
    {
        report_buffer_clear();
        layer_clear();
        km_printf("if 3\n");
    }
}

// After initializing the peripheral
void keyboard_post_init_kb(void)
{
    gpio_set_pin_output(WS2812_POWER_PIN);        // ws2812 power
    gpio_write_pin_high(WS2812_POWER_PIN);
    // rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_REACTIVE_WIDE);
    // rgb_matrix_mode_noeeprom(RGB_MATRIX_MULTISPLASH);    // 这两个测试xy用，挺好
    // rgb_matrix_mode_noeeprom(RGB_MATRIX_CYCLE_SPIRAL);
    rgb_matrix_is_enabled_temp_v = rgb_matrix_is_enabled();

}
__attribute__((weak)) bool via_command_kb(uint8_t *data, uint8_t length) {
    return via_command_bhq(data, length);
}

#   if defined(KB_LPM_ENABLED)
// 低功耗外围设备电源控制
void lpm_device_power_open(void) 
{
    lpm_wakeup_delay_open_rgb_flag = 1;
    gpio_set_pin_output(WS2812_POWER_PIN);
    gpio_write_pin_high(WS2812_POWER_PIN);
    if(is_sleep == 1)
    {
        is_sleep = 0;
        ws2812_init();
        if(rgb_matrix_is_enabled_temp_v)
        {
            rgb_matrix_enable();    // 重新打开rgb矩阵灯
        }
        rgb_matrix_set_suspend_state(false);
    }
}

//关闭外围设备电源
void lpm_device_power_close(void) 
{

    is_sleep = 1;
    // 低功耗前 获取矩阵灯的状态
    rgb_matrix_is_enabled_temp_v = rgb_matrix_is_enabled();
    // 软关灯
    if(rgb_matrix_is_enabled_temp_v == 0)
    {
        // 软关灯，且不写入eeprom
        rgb_matrix_disable_noeeprom();  
    }
    rgb_matrix_set_suspend_state(true);
    // 关闭电源
    // ws2812电源关闭
    gpio_set_pin_output(WS2812_POWER_PIN);        // ws2812 power
    gpio_write_pin_low(WS2812_POWER_PIN);

    gpio_set_pin_output(WS2812_DI_PIN);        // ws2812 DI Pin
    gpio_write_pin_low(WS2812_DI_PIN);
}










// 将未使用的引脚设置为输入模拟 
// PS：在6095中，如果不加以下代码休眠时是102ua。如果加了就是30ua~32ua浮动
void lpm_set_unused_pins_to_input_analog(void)
{
    // 禁用调试功能以降低功耗
    DBGMCU->CR &= ~DBGMCU_CR_DBG_SLEEP;   // 禁用在Sleep模式下的调试
    DBGMCU->CR &= ~DBGMCU_CR_DBG_STOP;    // 禁用在Stop模式下的调试
    DBGMCU->CR &= ~DBGMCU_CR_DBG_STANDBY; // 禁用在Standby模式下的调试
    // 在系统初始化代码中禁用SWD接口
    palSetLineMode(A13, PAL_MODE_INPUT_ANALOG);
    palSetLineMode(A14, PAL_MODE_INPUT_ANALOG);

    // palSetLineMode(A0, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(A1, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(A2, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(A3, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A4, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A5, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A6, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(A7, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A8, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A9, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(A10, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A11, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A13, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A14, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(A15, PAL_MODE_INPUT_ANALOG); 

    // palSetLineMode(B0, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(B1, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(B2, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(B3, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(B4, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(B5, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(B6, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(B7, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(B8, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(B9, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(B10, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(B11, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(B13, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(B14, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(B15, PAL_MODE_INPUT_ANALOG); 
}

#endif
