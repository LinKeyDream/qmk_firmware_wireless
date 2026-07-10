/* Copyright 2025 keykmhorse
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
#include <stdarg.h>
#include "km_printf.h"
#include "print.h"
#include "sendchar.h"

#if defined(KB_DEBUG_RTT)
    #include "SEGGER_RTT.h"
#elif defined(KB_DEBUG_UART_BHQ)
    #include "uart.h"
#endif

int8_t km_putchar(uint8_t c);

void km_printf_init(void)
{
#if defined(KB_DEBUG_RTT)
    SEGGER_RTT_Init();
#elif defined(KB_DEBUG_UART_BHQ)
    // BHQ 驱动已含 UART 初始化
    // 不需要在这里调用 print_set_sendchar(km_putchar)
    // 因为下方定义了 strong sendchar()，QMK 的 keyboard_setup()
    // 会自动调用 print_set_sendchar(sendchar) 使用我们的实现
#endif
}

int8_t km_putchar(uint8_t c)
{
#if defined(KB_DEBUG_RTT)
    SEGGER_RTT_printf(0, "%c", c);
#elif defined(KB_DEBUG_UART_BHQ)
    uart_write(c);
#endif
    return 0;
}

/*
 * 覆写 QMK 的 weak sendchar()，使 uprintf/print 自动路由到调试输出。
 *
 * 原理：QMK 的 keyboard_setup() 会执行 print_set_sendchar(sendchar)，
 * 此时 sendchar 是我们的 strong 定义，而非 weak 默认空实现。
 * 因此无论在 board_init() 还是 keyboard_post_init_user() 中调用
 * km_printf_init()，都不会被覆盖。
 *
 * 用 !CONSOLE_ENABLE 保护：如果使能了 USB Console，usb_main.c 中已有
 * strong sendchar()，避免重复定义链接冲突。
 */
#if defined(KB_DEBUG) && !defined(CONSOLE_ENABLE)
int8_t sendchar(uint8_t c) {
    return km_putchar(c);
}
#endif

int km_printf(const char* format, ...) {
#if defined(KB_DEBUG)
    va_list args;
    va_start(args, format);
    
    int count = 0;
    
    while (*format) {
        if (*format == '%' && format[1] == 'd') {
            format++;

            int value = va_arg(args, int);

            char buf[12];
            char *p = buf + sizeof(buf) - 1;
            *p = '\0';

            unsigned int u;

            if (value < 0) {
                km_putchar('-');
                u = (unsigned int)(-value);
            } else {
                u = (unsigned int)value;
            }

            do {
                *--p = '0' + (u % 10);
                u /= 10;
            } while (u);

            while (*p) {
                km_putchar(*p++);
                count++;
            }
        }
        else if (*format == '%' && format[1] == '0' && format[2] == '2' && format[3] == 'x') {
            format += 3;
            unsigned int value = va_arg(args, unsigned int);
            
            km_putchar("0123456789abcdef"[(value >> 4) & 0x0F]);
            km_putchar("0123456789abcdef"[value & 0x0F]);
            count += 2;
        }
        else if (*format == '%' && format[1] == '0' && format[2] == '4' && format[3] == 'x') {
            format += 3;
            unsigned int value = va_arg(args, unsigned int);
            
            km_putchar("0123456789abcdef"[(value >> 12) & 0x0F]);
            km_putchar("0123456789abcdef"[(value >> 8) & 0x0F]);
            km_putchar("0123456789abcdef"[(value >> 4) & 0x0F]);
            km_putchar("0123456789abcdef"[value & 0x0F]);
            count += 4;
        }
        else if (*format == '%' && format[1] == '0' && format[2] == '8' && format[3] == 'x') {
            format += 3;
            unsigned int value = va_arg(args, unsigned int);
            
            km_putchar("0123456789abcdef"[(value >> 28) & 0x0F]);
            km_putchar("0123456789abcdef"[(value >> 24) & 0x0F]);
            km_putchar("0123456789abcdef"[(value >> 20) & 0x0F]);
            km_putchar("0123456789abcdef"[(value >> 16) & 0x0F]);
            km_putchar("0123456789abcdef"[(value >> 12) & 0x0F]);
            km_putchar("0123456789abcdef"[(value >> 8) & 0x0F]);
            km_putchar("0123456789abcdef"[(value >> 4) & 0x0F]);
            km_putchar("0123456789abcdef"[value & 0x0F]);
            count += 8;
        }
       else if (*format == '%' && format[1] == '0' && format[2] == '2' && format[3] == 'd') {
            format += 3;
            unsigned int value = va_arg(args, unsigned int);
            
            km_putchar('0' + ((value / 10) % 10));
            km_putchar('0' + (value % 10));
            count += 2;
        }
        else if (*format == '%' && format[1] == '0' && format[2] == '4' && format[3] == 'd') {
            format += 3;
            unsigned int value = va_arg(args, unsigned int);
            
            km_putchar('0' + ((value / 1000) % 10));
            km_putchar('0' + ((value / 100) % 10));
            km_putchar('0' + ((value / 10) % 10));
            km_putchar('0' + (value % 10));
            count += 4;
        }
        else if (*format == '%' && format[1] == '.') {  // %.1f
            format += 1; // 跳过 %

            // ===== 解析精度 %.nf =====
            int precision = 0;
            format++; // skip '.'

            while (*format >= '0' && *format <= '9') {
                precision = precision * 10 + (*format - '0');
                format++;
            }

            if (*format == 'f') {
                double value = va_arg(args, double);

                // 限制最大精度 9位（避免溢出/性能问题）
                if (precision > 9) precision = 9;

                // 处理负数
                if (value < 0) {
                    km_putchar('-');
                    value = -value;
                    count++;
                }

                // ===== 整数部分 =====
                int int_part = (int)value;
                double frac = value - (double)int_part;

                char buf[16];
                char *p = buf + sizeof(buf) - 1;
                *p = '\0';

                unsigned int u = (unsigned int)int_part;
                do {
                    *--p = '0' + (u % 10);
                    u /= 10;
                } while (u);

                while (*p) {
                    km_putchar(*p++);
                    count++;
                }

                // ===== 小数点 =====
                km_putchar('.');
                count++;

                // ===== 小数部分（最多9位）=====
                unsigned int scale = 1;
                for (int i = 0; i < precision; i++) {
                    scale *= 10;
                }

                // 四舍五入
                unsigned int frac_part = (unsigned int)(frac * scale + 0.5);

                // 输出补零 + 数字
                for (int i = precision - 1; i >= 0; i--) {
                    unsigned int pow10 = 1;
                    for (int j = 0; j < i; j++) {
                        pow10 *= 10;
                    }

                    unsigned int digit = (frac_part / pow10) % 10;
                    km_putchar('0' + digit);
                    count++;

                    if (i == 0) break; // 防止 unsigned 下溢
                }
            }
        }
        else {
            km_putchar(*format);
            count++;
        }
        format++;
    }
    
    va_end(args);
    return count;
#else
    return 0;
#endif
}
