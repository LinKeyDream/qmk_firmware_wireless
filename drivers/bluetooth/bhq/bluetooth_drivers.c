/*
 * Copyright 2022
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

#include "bluetooth.h"
#include "bluetooth_bhq.h"

void bluetooth_init(void) {
    bluetooth_bhq_init();
}

void bluetooth_task(void) {
    bluetooth_bhq_task();
}

bool bluetooth_is_connected(void) {
    return true;
}

bool bluetooth_can_send_nkro(void) {
    return true;
}

uint8_t bluetooth_keyboard_leds(void) {
    return bluetooth_bhq_get_keyboard_leds();
}

void bluetooth_send_keyboard(report_keyboard_t *report) {
    bluetooth_bhq_send_keyboard(report);
}

void bluetooth_send_nkro(report_nkro_t *report) {
    bluetooth_bhq_send_nkro(report);
}

void bluetooth_send_mouse(report_mouse_t *report) {
    bluetooth_bhq_send_mouse(report);
}

void bluetooth_send_consumer(uint16_t usage) {
    bluetooth_bhq_send_consumer(usage);
}


void bluetooth_send_system(uint16_t usage)
{
    bluetooth_bhq_send_system(usage);
}


void bluetooth_send_raw_hid(uint8_t *data, uint8_t length) {
    bluetooth_bhq_send_hid_raw(data, length);
}
