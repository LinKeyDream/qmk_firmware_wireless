#pragma once

// WS2812配置
#define WS2812_PWM_DRIVER      PWMD2
#define WS2812_PWM_CHANNEL     3
#define WS2812_PWM_PAL_MODE    1
#define WS2812_DMA_STREAM      STM32_DMA1_STREAM1
#define WS2812_DMA_CHANNEL     3
#define RGB_POWER_ENABLED_PIN  A15

// 细节请看 app/src/utils/key-to-byte/v12.ts
#define USB_TOG     (0x7e00+0)    // 打开USB
#define BL_SW_0     (0x7e00+1)    // 开启蓝牙通道0（需要打开蓝牙的条件下才行） 短按打开广播 长按开启配对广播
#define BL_SW_1     (0x7e00+2)    // 开启蓝牙通道1（需要打开蓝牙的条件下才行） 短按打开广播 长按开启配对广播
#define BL_SW_2     (0x7e00+3)    // 开启蓝牙通道2（需要打开蓝牙的条件下才行） 短按打开广播 长按开启配对广播
#define BLE_OFF     (0x7e00+4)    // 关闭蓝牙连接
#define BLE_RESET   (0x7e00+5)    // 蓝牙复位
#define BLE_TOG     (0x7e00+6)    // 切换蓝牙输出 并 开启蓝牙广播（非配对类型）
#define RF_TOG      (0x7e00+7)    // 切换 2.4ghz输出

// 串口通信
#define BHQ_IQR_PIN              B8
#define BHQ_INT_PIN              B9

#define UART_DRIVER             SD1
#define UART_TX_PIN             B6
#define UART_TX_PAL_MODE        7
#define UART_RX_PIN             B7
#define UART_RX_PAL_MODE        7

// STM32使用到的高速晶振引脚号，做低功耗需要用户配置，每款芯片有可能不一样的
#define LPM_STM32_HSE_PIN_IN     H0
#define LPM_STM32_HSE_PIN_OUT    H1

// 电池电压读取的引脚
#define BATTER_ADC_PIN     B1
#define BATTER_ADC_DRIVER  ADCD1

#define REPORT_BUFFER_QUEUE_SIZE 68

// USB检测引脚，默认1，高电平有效
#define USB_POWER_SENSE_PIN        B0
#define USB_POWER_CONNECTED_LEVEL  1
