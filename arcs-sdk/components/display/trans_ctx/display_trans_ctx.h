
/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 #pragma once

 #include <stddef.h>
 #include <stdint.h>
 #include <stdbool.h>
 #include "FreeRTOS.h"
 #include "semphr.h"
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 

 enum display_trans_orient {
     DISPLAY_TRANS_ORIENT_NORMAL,
     DISPLAY_TRANS_ORIENT_ROTATED_90,
     DISPLAY_TRANS_ORIENT_ROTATED_180,
     DISPLAY_TRANS_ORIENT_ROTATED_270,
 };

 enum trans_type {
     TRANS_TYPE_CMD,
     TRANS_TYPE_IMG_CMD,
     TRANS_TYPE_DATA,
 };

 struct display_trans_ops {
    int  (*trans_init)(void *trans_config);
    int  (*trans_image_wait)(uint32_t timeout);
    int  (*trans_image)(void *buf, uint32_t size);
    int  (*trans_cmd)(uint8_t *data, uint32_t data_len);
    void (*trans_cs_trig)(uint8_t level);
    void (*trans_dc_trig)(uint8_t level);
 };
 
 typedef void(*transaction_cb_t)(enum trans_type type, int *cmd);

struct display_trans_ops *display_trans_ops_get(void);

int display_trans_ctx_init(uint32_t bpp, uint32_t cmd_bits, uint8_t dc_level, void *trans_config);
int display_trans_cmd_data(int cmd, const void *data, size_t data_len);
int display_trans_image(int cmd, void *buf, uint32_t w, uint32_t h, enum display_trans_orient orient);
 
 
 #ifdef __cplusplus
 }
 #endif