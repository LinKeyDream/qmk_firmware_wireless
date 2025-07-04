/* Copyright 2023 Cipulot
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#define MATRIX_ROWS 7
#define MATRIX_COLS 16


#define UNUSED_POSITIONS_LIST \
{ \
  {3, 13}, {3, 14}, {3, 15}, \
  {5,  3}, {5,  4}, {5,  5}, {5,  7}, {5,  8}, {5,  9}, \
  {6,  0}, {6,  1}, {6,  2}, {6,  3}, {6,  4}, {6,  5}, {6,  6}, {6,  7}, {6,  8}, {6,  9}, {6, 10}, {6, 11} \
}

#define MATRIX_ROW_PINS \
    { B14, B13, B5, B1, A7, B0, A1 }

#define AMUX_COUNT 2
#define AMUX_MAX_COLS_COUNT 8

#define AMUX_EN_PINS \
    { A8, B15 }

#define AMUX_SEL_PINS \
    { A4, A5, A6 }

#define AMUX_COL_CHANNELS_SIZES \
    { 8, 8 }

#define AMUX_0_COL_CHANNELS \
    { 1, 0, 3, 2, 4, 6, 7, 5 }

#define AMUX_1_COL_CHANNELS \
    { 3, 0, 1, 2, 4, 6, 7, 5 }

#define AMUX_COL_CHANNELS AMUX_0_COL_CHANNELS, AMUX_1_COL_CHANNELS

#define DISCHARGE_PIN A3
#define ANALOG_PORT A2

#define DEFAULT_ACTUATION_MODE 0
#define DEFAULT_MODE_0_ACTUATION_LEVEL 550
#define DEFAULT_MODE_0_RELEASE_LEVEL 500
#define DEFAULT_MODE_1_INITIAL_DEADZONE_OFFSET DEFAULT_MODE_0_ACTUATION_LEVEL
#define DEFAULT_MODE_1_ACTUATION_OFFSET 70
#define DEFAULT_MODE_1_RELEASE_OFFSET 70
#define DEFAULT_EXTREMUM 1023
#define EXPECTED_NOISE_FLOOR 0
#define NOISE_FLOOR_THRESHOLD 50
#define BOTTOMING_CALIBRATION_THRESHOLD 100
#define DEFAULT_NOISE_FLOOR_SAMPLING_COUNT 30
#define DEFAULT_BOTTOMING_READING 1023
#define DEFAULT_CALIBRATION_STARTER true

#define DISCHARGE_TIME 10

// #define DEBUG_MATRIX_SCAN_RATE
#define DYNAMIC_KEYMAP_LAYER_COUNT 2
#define EECONFIG_KB_DATA_SIZE 233
