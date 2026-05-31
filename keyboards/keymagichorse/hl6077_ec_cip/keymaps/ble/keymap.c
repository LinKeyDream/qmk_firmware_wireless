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
#include "via.h"
#if defined(RGBLIGHT_WS2812)
#    include "ws2812.h"
#endif

#include "bhq_common.h"
#include "wireless.h"
#include "transport.h"
#include "report_buffer.h"

# if defined(KB_CHECK_BATTERY_ENABLED)
#   include "battery.h"
#endif

led_t kb_led_state = {0};
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  [0] = LAYOUT(
        QK_GESC, KC_1,    KC_2,     KC_3,     KC_4,    KC_5,    KC_6,    KC_7,    KC_8,      KC_9,     KC_0,     KC_MINS,  KC_EQL,  KC_BSPC,
        KC_TAB,  KC_Q,    KC_W,     KC_E,     KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,      KC_O,     KC_P,     KC_LBRC,  KC_RBRC, KC_ENT,
        KC_CAPS, KC_A,    KC_S,     KC_D,     KC_F,    KC_G,    KC_H,    KC_J,    KC_K,      KC_L,     KC_SCLN,  KC_QUOT,  KC_ENT,  KC_BSLS,
        KC_LSFT, KC_Z,    KC_X,     KC_C,     KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM,   KC_DOT,   KC_SLSH,  KC_RSFT,  KC_UP, 
        KC_LCTL, KC_LGUI, KC_LALT,  KC_SPC,   LT(2, KC_SPC),  KC_SPC,             MO(1),     KC_DEL,   KC_LEFT,  KC_RIGHT, KC_DOWN),
  [1] = LAYOUT(
        KC_GRV , KC_F1,   KC_F2,   KC_F3,    KC_F4,   KC_F5,   KC_F6,   KC_F7,    KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,  KC_DEL,
        KC_TRNS, BLE_SW1, BLE_SW2, BLE_SW3,  RF_TOG,  KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, USB_TOG, NK_TOGG, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, 
        KC_LCTL, KC_LGUI, KC_LALT,           KC_TRNS,           KC_SPC,  KC_SPC,   MO(1),   KC_VOLU, KC_VOLD, KC_BRIU, KC_BRID),
  [2] = LAYOUT(
		KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
		KC_TRNS, KC_TRNS, KC_UP,  KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
		KC_TRNS, KC_LEFT, KC_DOWN, KC_RIGHT, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
		KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
		KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS,          KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS),
  [3] = LAYOUT(
		KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
		KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
		KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
		KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
		KC_TRNS, KC_TRNS, KC_TRNS,          KC_TRNS,          KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS)
};


bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    return process_record_bhq(keycode, record);
}


#if defined(RGBLIGHT_ENABLE) 
//  每个通道的颜色 以及大写按键的颜色
const rgblight_segment_t PROGMEM bt_conn1[]   = RGBLIGHT_LAYER_SEGMENTS( {0, 1, HSV_BLUE} );      // 通道1：蓝色
const rgblight_segment_t PROGMEM bt_conn2[]   = RGBLIGHT_LAYER_SEGMENTS( {0, 1, HSV_TURQUOISE} ); // 通道2：蓝绿色（青绿）
const rgblight_segment_t PROGMEM bt_conn3[]   = RGBLIGHT_LAYER_SEGMENTS( {0, 1, HSV_ORANGE} );    // 通道3：橙色
const rgblight_segment_t PROGMEM caps_lock_[] = RGBLIGHT_LAYER_SEGMENTS( {0, 1, HSV_PURPLE} );    // 大小写：紫色
const rgblight_segment_t PROGMEM bat_low_led[] = RGBLIGHT_LAYER_SEGMENTS( {0, 1, HSV_RED} );      // 低电量：红色

const rgblight_segment_t* const PROGMEM _rgb_layers[] = RGBLIGHT_LAYERS_LIST( 
    bt_conn1, bt_conn2, bt_conn3, caps_lock_, bat_low_led
);

void rgb_adv_unblink_all_layer(void) {
    for (uint8_t i = 0; i < 4; i++) {
        rgblight_unblink_layer(i);
    }
}

bool led_update_user(led_t led_state) {
    kb_led_state = led_state;
    return true;
}
static bool wireless_update_led_block  = false;
static uint32_t wireless_update_led_block_timer  = 0;
// 无线蓝牙回调函数
void wireless_ble_hanlde_kb(uint8_t host_index,uint8_t advertSta,uint8_t connectSta,uint8_t pairingSta)
{
    wireless_update_led_block  = true;
    wireless_update_led_block_timer = timer_read32();
    rgblight_disable();
    rgb_adv_unblink_all_layer();
    km_printf("wireless_ble_hanlde_kb->host_index: %d\r\n",host_index);
    // 蓝牙没有连接 && 蓝牙广播开启  && 蓝牙配对模式
    if(connectSta != 1 && advertSta == 1 && pairingSta == 1)
    {
        // 这里第一个参数使用host_index正好对应_rgb_layers的索引
        rgblight_blink_layer_repeat(host_index , 500, 50);
        km_printf("if 1\n");
    }
    // 蓝牙没有连接 && 蓝牙广播开启  && 蓝牙非配对模式
    else if(connectSta != 1 && advertSta == 1 && pairingSta == 0)
    {
        rgblight_blink_layer_repeat(host_index , 2000, 50);
        km_printf("if 2\n");
    }
    // 蓝牙已连接
    if(connectSta == 1)
    {
        report_buffer_clear();
        layer_clear();
        rgblight_blink_layer_repeat(host_index , 200, 2);
        km_printf("if 3\n");
    }
}
void wireless_rf24g_hanlde_kb(uint8_t connectSta,uint8_t pairingSta)
{
    wireless_update_led_block  = true;
    wireless_update_led_block_timer = timer_read32();
    rgblight_disable_noeeprom();
    rgb_adv_unblink_all_layer();
    if(connectSta == 1)
    {
        report_buffer_clear();
        layer_clear();
        rgblight_blink_layer_repeat(3 , 200, 2);
        km_printf("if 3\n");
    }
}
#endif

// 2812 电源开关
void ws2812_set_power(uint8_t on)
{
    gpio_set_pin_output(WS2812_POWER_PIN);        // ws2812 power
    if(on)  // 开
    {
#if WS2812_POWER_ON_LEVEL == 0
        gpio_write_pin_low(WS2812_POWER_PIN);
#else
        gpio_write_pin_high(WS2812_POWER_PIN);
#endif
    }
    else    // 关
    {
#if WS2812_POWER_ON_LEVEL == 0
        gpio_write_pin_high(WS2812_POWER_PIN);
#else
        gpio_write_pin_low(WS2812_POWER_PIN);
#endif
    }
}

// After initializing the peripheral
void keyboard_post_init_user(void)
{
    ws2812_init();
    ws2812_set_power(1);

    rgblight_disable();
    rgblight_layers = _rgb_layers;  // 层灯光赋值
    rgb_adv_unblink_all_layer();
    bhq_common_init();
}

void housekeeping_task_user(void) 
{
    static uint32_t kb_led_cut = 0;
# if defined(KB_CHECK_BATTERY_ENABLED)
    static uint32_t low_led_blink_timer = 0;
    static uint8_t low_led_sta = 0;
    if (battery_driver_sample_percent() <= 10)
    {
        if (timer_elapsed32(low_led_blink_timer) > 500)
        {
            low_led_blink_timer = timer_read32();
            low_led_sta ^= 1;
            rgblight_set_layer_state(4, low_led_sta);
        }
        return;
    }
    else
    {
        low_led_blink_timer = 0;
        low_led_sta = 0;
    }
#endif
    if (wireless_update_led_block  && timer_elapsed32(wireless_update_led_block_timer ) > 500) {
        wireless_update_led_block  = false;
    }
    if(wireless_update_led_block)
    {
        return;
    }
    // 如果当前是USB连接，或者是蓝牙/2.4G连接且已配对连接状态
    if( (transport_get() > KB_TRANSPORT_USB && wireless_get() == WT_STATE_CONNECTED) || ( usb_power_connected() == true && transport_get() == KB_TRANSPORT_USB))
    {
        if (timer_elapsed32(kb_led_cut) > 500) {
            kb_led_cut = timer_read32();
            rgb_adv_unblink_all_layer();
            rgblight_set_layer_state(3, kb_led_state.caps_lock);
        }
    }
}

#   if defined(KB_LPM_ENABLED)
// 低功耗外围设备电源控制
void lpm_device_power_open(void) 
{
#if defined(RGBLIGHT_WS2812) && defined(RGBLIGHT_ENABLE) 
    // ws2812电源开启
    ws2812_init();
    gpio_set_pin_output(WS2812_POWER_PIN);        // ws2812 power
    gpio_write_pin_low(WS2812_POWER_PIN);
#endif

}
//关闭外围设备电源
void lpm_device_power_close(void) 
{
#if defined(RGBLIGHT_WS2812) && defined(RGBLIGHT_ENABLE) 
    // ws2812电源关闭
    rgblight_setrgb_at(0, 0, 0, 0);
    gpio_set_pin_output(WS2812_POWER_PIN);        // ws2812 power
    gpio_write_pin_high(WS2812_POWER_PIN);

    gpio_set_pin_output(WS2812_DI_PIN);        // ws2812 DI Pin
    gpio_write_pin_low(WS2812_DI_PIN);
#endif
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

    palSetLineMode(A0, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A1, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A2, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A3, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A4, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A5, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A6, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(A7, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(A8, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(A9, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A10, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A11, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A13, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(A14, PAL_MODE_INPUT_ANALOG); 
    palSetLineMode(A15, PAL_MODE_INPUT_ANALOG); 

    palSetLineMode(B0, PAL_MODE_INPUT_ANALOG); 
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
    // palSetLineMode(B14, PAL_MODE_INPUT_ANALOG); 
    // palSetLineMode(B15, PAL_MODE_INPUT_ANALOG); 
}

#endif
