/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AP -> CP  notify subcmds */

typedef enum {
    CV_IPC_NOTIFY_SUBCMD_OCR_RESULT = 1,
    CV_IPC_NOTIFY_SUBCMD_STITCH_FRAME = 2,
    CV_IPC_NOTIFY_SUBCMD_STATUS = 3,
    CV_IPC_NOTIFY_SUBCMD_FRAME_DONE = 4,
    CV_IPC_NOTIFY_SUBCMD_IMG_SAVE = 5,
} cv_ipc_notify_subcmd_e;

typedef struct {
    cv_ipc_notify_subcmd_e cmd;
} __attribute__((packed)) cv_ipc_notify_subcmd_hdr_t;

typedef struct {
    cv_ipc_notify_subcmd_hdr_t hdr;
    uint32_t len;
    uint8_t data[];
} __attribute__((packed, aligned(32))) cv_ipc_notify_subcmd_ocr_result_t;

typedef struct {
    cv_ipc_notify_subcmd_hdr_t hdr;
    uint32_t width;
    uint32_t height;
    uint32_t len;
    uint8_t data[];
} __attribute__((packed, aligned(32))) cv_ipc_notify_subcmd_stitch_frame_t;

typedef struct {
    cv_ipc_notify_subcmd_hdr_t hdr;
    uint32_t status;
} __attribute__((packed, aligned(32))) cv_ipc_notify_subcmd_status_t;

typedef struct {
    cv_ipc_notify_subcmd_hdr_t hdr;
    uint32_t fb_addr;
} __attribute__((packed, aligned(32))) cv_ipc_notify_subcmd_frame_done_t;

typedef struct {
    cv_ipc_notify_subcmd_hdr_t hdr;
    uint32_t img_addr;
    uint16_t width;
    uint16_t height;
    uint8_t img_type;
} __attribute__((packed, aligned(32))) cv_ipc_notify_subcmd_img_save_t;

/* CP -> AP  control subcmds */

typedef enum {
    CV_IPC_CONTROL_SUBCMD_SET_SCAN_MODE = 1,
    CV_IPC_CONTROL_SUBCMD_SET_LR_MODE = 2,
    CV_IPC_CONTROL_SUBCMD_SET_SCREEN_HEIGHT = 3,
    CV_IPC_CONTROL_SUBCMD_SET_BOOT_TYPE = 4,
} cv_ipc_control_subcmd_e;

typedef struct {
    uint8_t scan_mode;
} __attribute__((packed)) cv_ipc_control_subcmd_scan_mode_set_t;

typedef struct {
    uint8_t lr_mode;
} __attribute__((packed)) cv_ipc_control_subcmd_lr_mode_set_t;

typedef struct {
    uint32_t screen_height;
} __attribute__((packed)) cv_ipc_control_subcmd_screen_height_set_t;

typedef struct {
    uint8_t boot_type;
} __attribute__((packed)) cv_ipc_control_subcmd_boot_type_set_t;

/* ipc end */

#ifdef __cplusplus
}
#endif
