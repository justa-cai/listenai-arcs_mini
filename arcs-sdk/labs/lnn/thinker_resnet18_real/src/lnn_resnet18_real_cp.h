/*
 * Copyright (c) 2026 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lnn_resnet18_real_ipc.h"

#define CAMERA_DEVICE             "camera"
#define DVP_DEVICE                "dvp0"
#define TOUCH_DEVICE              "touch_cst328"
#define I2C_DEVICE                "i2c0"

#define CAMERA_DMA_CHANNEL        (2U)

#define SCREEN_WIDTH              (320U)
#define SCREEN_HEIGHT             (240U)
#define CAMERA_WINDOW_WIDTH       (480U)
#define CAMERA_WINDOW_HEIGHT      (480U)
#define PREVIEW_WIDTH             (240U)
#define PREVIEW_HEIGHT            (240U)
#define SIDE_PANEL_WIDTH          (SCREEN_WIDTH - PREVIEW_WIDTH)
#define RESULT_POLL_INTERVAL_MS   (200U)
#define TOUCH_POLL_INTERVAL_MS    (50U)
#define REQUEST_NOTIFY_MODE       "mbox"

#ifdef CONFIG_BOARD_ARCS_EVB

#define LCD_CS_PIN                5
#define LCD_SPI_CLK_PIN           3
#define LCD_SPI_DATA_PIN          1

#define CAM_PWDN_PIN              7
#define CAM_HSYNC_PIN             10
#define CAM_VSYNC_PIN             11
#define CAM_PCLK_PIN              12
#define CAM_MCLK_PIN              26
#define CAM_D0_PIN                13
#define CAM_D1_PIN                14
#define CAM_D2_PIN                15
#define CAM_D3_PIN                16
#define CAM_D4_PIN                17
#define CAM_D5_PIN                18
#define CAM_D6_PIN                19
#define CAM_D7_PIN                20

#define TOUCH_I2C_SDA_PORT        CSK_IOMUX_PAD_A
#define TOUCH_I2C_SDA_PIN         22
#define TOUCH_I2C_SDA_FUNC        8
#define TOUCH_I2C_SCL_PORT        CSK_IOMUX_PAD_A
#define TOUCH_I2C_SCL_PIN         23
#define TOUCH_I2C_SCL_FUNC        8
#define TOUCH_RST_PORT            CSK_IOMUX_PAD_A
#define TOUCH_RST_PIN             25
#define TOUCH_RST_FUNC            0
#define TOUCH_INT_PORT            CSK_IOMUX_PAD_A
#define TOUCH_INT_PIN             24
#define TOUCH_INT_FUNC            0

#endif

typedef struct {
    const uint16_t *pixels;
    uint32_t stride;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} rgb565_window_t;

typedef struct {
    uint32_t magic;
    uint32_t seq;
    uint32_t state;
    int32_t result_score;
    char result_text[LNN_RESNET18_REAL_LABEL_MAX];
    char message[LNN_RESNET18_REAL_MESSAGE_MAX];
} lnn_ap_status_t;

int lnn_mbox_init(void);
int lnn_ipc_mutex_init(void);
void lnn_ipc_lock(void);
void lnn_ipc_unlock(void);
void lnn_ipc_cache_flush(volatile lnn_resnet18_real_ipc_t *ipc);
void lnn_ipc_cache_invalidate(volatile lnn_resnet18_real_ipc_t *ipc);
bool lnn_ipc_ready_for_request(volatile lnn_resnet18_real_ipc_t *ipc);
int lnn_ipc_notify_ap(uint32_t seq);
void lnn_ipc_mark_status_dirty(void);
bool lnn_ipc_take_status_dirty(void);

bool lnn_ap_ready_for_request(void);
int lnn_ap_submit_window_for_inference(const rgb565_window_t *window);
void lnn_ap_read_status(lnn_ap_status_t *status);
void lnn_ap_mark_status_dirty(void);
bool lnn_ap_take_status_dirty(void);

int lnn_display_touch_init(void);
int lnn_camera_init(void);
void lnn_task_ui(void *arg);
void lnn_task_camera(void *arg);
