/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_NES_BTN_A1  (1U << 15)
#define GAME_NES_BTN_B1  (1U << 14)
#define GAME_NES_BTN_SE1 (1U << 13)
#define GAME_NES_BTN_ST1 (1U << 12)
#define GAME_NES_BTN_U1  (1U << 11)
#define GAME_NES_BTN_D1  (1U << 10)
#define GAME_NES_BTN_L1  (1U << 9)
#define GAME_NES_BTN_R1  (1U << 8)

uint16_t game_nes_get_joypad_state(void);
bool game_nes_consume_rom_list_request(void);
void game_nes_clear_rom_list_request(void);

#ifdef __cplusplus
}
#endif
