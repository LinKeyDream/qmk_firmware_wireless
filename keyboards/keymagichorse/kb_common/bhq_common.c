/* Copyright 2025 keymagichorse
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

#include "config.h"
#include "bluetooth.h"
#include "bhq_common.h"
#include "bhq.h"
#include "wireless.h"
#include "transport.h"
#include "km_printf.h"
#include "battery.h"
#include "outputselect.h"
#include "usb_main.h"

# if defined(KB_CHECK_BATTERY_ENABLED)
#   include "battery.h"
uint8_t bhq_bat_low_sta = 0; // 0:正常, 1:10%低电量, 2:5%严重低电量
#endif

# if defined(KB_LPM_ENABLED)
#   include "lpm.h"
#endif

bool usb_power_connected(void) {
#ifdef USB_POWER_SENSE_PIN
    return readPin(USB_POWER_SENSE_PIN) == USB_POWER_CONNECTED_LEVEL;
#else
    return true;
#endif
}

// 蓝牙模块配置发送标志:置 1 后,bhq_wireless_task 延迟 500ms 发送一次
static uint8_t  is_config_pending = 1;
static uint32_t config_send_time  = 0;


void bhq_common_init(void)
{
#   if defined(KB_DEBUG)
    km_printf_init();
    km_printf("hello bhq debug \r\n");
#   endif

# if defined(KB_CHECK_BATTERY_ENABLED)
    battery_init();
    bhq_bat_low_sta = 0;
#endif

    gpio_set_pin_input(USB_POWER_SENSE_PIN);
}
// --------------------  都是用于处理按键触发的变量 --------------------
uint16_t this_down_wireless_keycode = 0;
uint32_t down_wirlees_keycode_time = 0;
static uint8_t key_ble_host_index = 0;         // 蓝牙索引
// --------------------  都是用于处理按键触发的变量 --------------------


// 按键切换主机逻辑
bool process_record_bhq(uint16_t keycode, keyrecord_t *record) {
    km_printf("process_record_bhq\n");
# if defined(KB_CHECK_BATTER_ENABLED)
    battery_reset_timer();
#endif
#   if defined(KB_LPM_ENABLED)
    lpm_timer_reset();  // 这里用于低功耗，按下任何按键刷新低功耗计时器
#endif
#if defined(KB_CHECK_BATTERY_ENABLED)
    if(keycode == BAT_INFO && record->event.pressed)
    {
        // 输出电池信息: Bat: 85% 3950mV
        char buf[24];
        int  len = snprintf(buf, sizeof(buf), "Bat: %d%% %dmV", battery_get_percent(), battery_get_mv());
        if (len > 0) {
            send_string(buf);
        }
        return false;
    }
#endif
    // 如果是无线模式 且 没有连接的情况下 按下任意按键触发广播
    if ( (IS_WIRELESS_TRANSPORT(transport_get()) == true)  && wireless_get() != WT_STATE_CONNECTED ) 
    {
        if (
            keycode != BLE_TOG &&
            keycode != RF_TOG &&
            keycode != USB_TOG &&
            keycode != BLE_SW1 &&
            keycode != BLE_SW2 &&
            keycode != BLE_SW3 &&
            keycode != BLE_RESET &&
            keycode != BLE_OFF
        ) {
            if(wireless_get() == WT_STATE_DISCONNECTED || wireless_get() == WT_STATE_RESET || wireless_get() == WT_STATE_INITIALIZED )
            {
                if((IS_BLE_TRANSPORT(transport_get()) == true))
                {
                    // 检查传输模式是否为蓝牙模式
                    // KB_TRANSPORT_BLUETOOTH_1 在枚举 里面是2、在蓝牙通道内是0
                    // 那么 2 - 2 = 0 那就是host = 0;
                    // 重新打开非配对蓝牙广播。如已开启蓝牙广播或已连接，那么不会断开当前的蓝牙连接。
                    bhq_AnewOpenBleAdvertising(transport_get() - KB_TRANSPORT_BLUETOOTH_1, 30);
                }
            }
        }
    }
    // km_printf("keycode:%d %d\n",keycode,record->event.pressed);
    // 蓝牙模式点按
    if(keycode == BLE_SW1 || keycode == BLE_SW2 || keycode == BLE_SW3 || keycode == RF_TOG)
    {
        if(record->event.pressed)
        {   // 赋值 并记录当前时间
            this_down_wireless_keycode = keycode;
            down_wirlees_keycode_time = timer_read32();
        }
        else
        {
            if(timer_elapsed32(down_wirlees_keycode_time) >= 30 && timer_elapsed32(down_wirlees_keycode_time) <= 800)
            {
                switch (keycode)
                {
                    case RF_TOG:
                        key_ble_host_index = 0;
                        key_ble_host_index = 0;
                        bhq_switch_rf_easy_kb(key_ble_host_index, 30);
                        transport_set(KB_TRANSPORT_RF);  
                        break;  
                    case BLE_SW1:
                        key_ble_host_index = 0;
                        // 打开非配对模式蓝牙广播 10 = 10S
                        bhq_OpenBleAdvertising(key_ble_host_index, 30);
                        transport_set(key_ble_host_index + KB_TRANSPORT_BLUETOOTH_1);  
                        break;  
                    case BLE_SW2:
                        key_ble_host_index = 1;
                        // 打开非配对模式蓝牙广播 10 = 10S
                        bhq_OpenBleAdvertising(key_ble_host_index, 30);
                        transport_set(key_ble_host_index + KB_TRANSPORT_BLUETOOTH_1);  
                        break;  
                    case BLE_SW3:
                        key_ble_host_index = 2;
                        // 打开非配对模式蓝牙广播 10 = 10S
                        bhq_OpenBleAdvertising(key_ble_host_index, 30);
                        transport_set(key_ble_host_index + KB_TRANSPORT_BLUETOOTH_1);  
                        break;  
                }
                // km_printf("key short down:bleid->%d\n",key_ble_host_index);
            }
            this_down_wireless_keycode = 0;
            down_wirlees_keycode_time = 0;
        }
        return true;
    }

    switch (keycode)
    {
        case BLE_TOG:
        {
            if(!record->event.pressed)
            {
                key_ble_host_index = 0;
                // 打开非配对模式蓝牙广播 10 = 10S
                bhq_OpenBleAdvertising(key_ble_host_index, 30);
                transport_set(KB_TRANSPORT_BLUETOOTH_1);
            }
            return true;
        }

        case USB_TOG:
        {
            if(!record->event.pressed)
            {
                // 切换到usb模式 并 关闭蓝牙广播
                bhq_CloseBleAdvertising();
                transport_set(KB_TRANSPORT_USB);  
                clear_mods();
                clear_oneshot_mods();
                clear_weak_mods();
                send_keyboard_report();
            }
            return true;
        }
        case BLE_OFF:
        {
            if(!record->event.pressed)
            {
                // 关闭蓝牙广播
                bhq_CloseBleAdvertising();
            }
            return true;
        }
    }
    return true;
}

//  切换主机任务
void bhq_switch_host_task(void){
    static bool has_switched = false;
    // 蓝牙模式长按的
    if (this_down_wireless_keycode == BLE_SW1 ||
        this_down_wireless_keycode == BLE_SW2 ||
        this_down_wireless_keycode == BLE_SW3 ||
        this_down_wireless_keycode == RF_TOG 
    )
    {
        if (!has_switched && timer_elapsed32(down_wirlees_keycode_time) >= 1000)
        {
            has_switched = true;  // 标志位，用于只执行一次
            km_printf("key long down:bleid->%d\n",key_ble_host_index);
            switch (this_down_wireless_keycode)
            {
                case RF_TOG:
                    key_ble_host_index = 0;
                    key_ble_host_index = 0;
                    bhq_switch_rf_easy_kb_pair(key_ble_host_index, 30);
                    transport_set(KB_TRANSPORT_RF);  
                    break;  
                case BLE_SW1:
                    key_ble_host_index = 0;
                    // 打开配对广播
                    bhq_SetPairingMode(key_ble_host_index, 30);
                    transport_set(key_ble_host_index + KB_TRANSPORT_BLUETOOTH_1);  
                    break;
                case BLE_SW2:
                    key_ble_host_index = 1;
                    // 打开配对广播
                    bhq_SetPairingMode(key_ble_host_index, 30);
                    transport_set(key_ble_host_index + KB_TRANSPORT_BLUETOOTH_1);  
                    break;
                case BLE_SW3:
                    key_ble_host_index = 2;
                    // 打开配对广播
                    bhq_SetPairingMode(key_ble_host_index, 30);
                    transport_set(key_ble_host_index + KB_TRANSPORT_BLUETOOTH_1);  
                    break;
            }
        }
    }
    else
    {
        has_switched = false;  // 按键释放或变更后重置状态
    }
}

# if defined(KB_CHECK_BATTERY_ENABLED)
void bhq_common_battery_low_task(void)
{
    static uint8_t last_sta = 255;
    uint8_t sta;

    if (usb_power_connected()) {
        sta = 0;
    } 
    else {
        uint8_t battery_percent = battery_driver_sample_percent();

        if (battery_percent <= 5) {
            sta = 2;
        } 
        else if (battery_percent <= 10) {
            sta = 1;
        } 
        else {
            sta = 0;
        }
    }

    if (sta == last_sta) return;

    last_sta = sta;
    bhq_bat_low_sta = sta;

    if (sta == 0) {
        bluetooth_enabled();
        battery_enable_ble_update();
    } 
    else if (sta == 1) {
        bluetooth_enabled();
    } 
    else { // sta == 2
        bluetooth_disabled();
    }
}

#endif

void bhq_wireless_task(void)
{
    bhq_switch_host_task();
# if defined(KB_CHECK_BATTERY_ENABLED)
    battery_task();
    bhq_common_battery_low_task();
#endif

    if (is_config_pending == 1) {   // 稍微延迟500ms 等稳定一下下再发
        if (timer_elapsed32(config_send_time) >= 500) {
            is_config_pending = 0;

            // 局部变量,指定初始化(designated initializer),从 QMK 配置宏取值
            bhqDevConfigInfo_t cfg = {
                .vendor_id_source               = 1,    // 1: USB-IF
                .verndor_id                     = VENDOR_ID,
                .product_id                     = PRODUCT_ID,

                .le_connection_interval_min     = 6,    // 最小连接间隔(1.25ms)
                .le_connection_interval_max     = 35,   // 最大连接间隔(1.25ms)
                .le_connection_interval_timeout = 500,  // 连接间隔超时
                .tx_poweer                      = 0x3D, // 最强

                // 电池相关字段已废弃,全部填 0
                .mk_is_read_battery_voltage     = 0,
                .mk_adc_pga                     = 0,
                .mk_rvd_r1                      = 0,
                .mk_rvd_r2                      = 0,

                .sleep_1_s                      = 10,   // 一级休眠(秒)
                .sleep_2_s                      = 300,  // 二级休眠/关机(秒)

                .bleNameStrLength               = strlen(BLE_NAME), // 蓝牙名称
                .bleNameStr                     = BLE_NAME,

                .bleSwiftPairNameStrLength      = strlen(BLE_SWIFT_PAIR_NAME), // 蓝牙swift pair名称
                .bleSwiftPairNameStr            = BLE_SWIFT_PAIR_NAME,

                .usbNameStrLength               = strlen(USB_DOG_NAME),     // USB接收器名称
                .usbNameStr                     = USB_DOG_NAME,

                .usbVendorNameStrLength         = strlen(USB_DOG_VENDOR_NAME),  // USB接收器厂商名称
                .usbVendorNameStr               = USB_DOG_VENDOR_NAME,
            };


            bhq_ConfigParam(cfg);
        }
    }
}

