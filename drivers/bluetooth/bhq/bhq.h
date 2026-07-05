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
#include "stdint.h"
#include <stdbool.h>
#include "debug.h"

#define bhq_printf(format, ...) 

enum { 
    BHQ_ACK_RUN_STA_CMDID = 0x93,
    BHQ_ACK_LED_LOCK_CMDID = 0x26
};

#define PACKECT_HEADER_LEN  4
#define PACKET_MAX_LEN      256

// -------------------- bhq protocol Small terminal mode --------------------

#define BHQ_FRAME_HEADER_1  0x5D
#define BHQ_FRAME_HEADER_2  0x7E

#define BHQ_FRAME_END_1     0x5E

#define BHQ_ACK             0x51
#define BHQ_NOT_ACK         0x50
#define BHQ_CMD_TO_ACKCMD(value) ((value) |= (1 << 7))

#define BHQ_H_UINT16(a) (((a) >> 8) & 0xFF) 
#define BHQ_L_UINT16(a) ((a) & 0xFF)       
#define BHQ_BUILD_UINT16(loByte, hiByte) ((uint16_t)(((loByte) & 0x00FF)|(((hiByte) & 0x00FF)<<8)))

#define BHQ_SET_BIT_VALUE(var, xbit, value) ((value) ? ((var) |= (1 << (xbit))) : ((var) &= ~(1 << (xbit))))

#define BHQ_GET_BLE_ADVERT_STA(var) ((var) & 0x01)          // 0x13->0x93:bat[1]->bit0:     ble Advert state
#define BHQ_GET_BLE_CONNECT_STA(var) (((var) >> 1) & 0x03)  // 0x13->0x93:bat[1]->bit1~2:   ble connect state
#define BHQ_GET_BLE_PAIRING_STA(var) (((var) >> 3) & 0x01)  // 0x13->0x93:bat[1]->bit3:     ble Pairing state

#define BHQ_SUCCESS     0
// -------------------- bhq protocol Small terminal mode --------------------

// Module operating status and qmk have the level status of data transmission       
#define BHQ_RUN_OR_INT_LEVEL       1

// #ifndef BHQ_IQR_PIN
// #    warning "BHQ_IQR_PIN is not defined (not required for BHQ firmware >= 226)"
// #endif

// #ifndef BHQ_INT_PIN
// #    warning "BHQ_INT_PIN is not defined (not required for BHQ firmware >= 226)"
// #endif


void bhq_init(void);
void bhq_Disable(void);
bool bhq_available(void);
void BHQ_Protocol_Process_user(uint8_t *dat, uint16_t length) ;
void BHQ_SendCmd(uint8_t isack, uint8_t *dat, uint8_t datLength);

void bhq_SetPairingMode(uint8_t host_index, uint16_t timeout_1S);
void bhq_OpenBleAdvertising(uint8_t host_index, uint16_t timeout_1S);
void bhq_AnewOpenBleAdvertising(uint8_t host_index, uint16_t timeout_1S);
void bhq_CloseBleAdvertising(void);
void bhq_switch_rf_easy_kb(uint8_t host_index,uint16_t timeout_1S);
void bhq_switch_rf_easy_kb_pair(uint8_t host_index,uint16_t timeout_1S);
void bhq_update_battery_percent(uint8_t percent, uint16_t bat_mv);


bool via_command_bhq(uint8_t *data, uint8_t length);

void bhq_send_keyboard(uint8_t* report);
void bhq_send_nkro(uint8_t* report);
void bhq_send_consumer(uint16_t report);
void bhq_send_system(uint16_t report);
void bhq_send_mouse(uint8_t* report);
void bhq_send_hid_raw(uint8_t *data, uint8_t length);


void bhq_task(void);
