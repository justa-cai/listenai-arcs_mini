/**
 ****************************************************************************************
 *
 * @file wifi_ota.h
 *
 * @brief CLI cmd handle.
 *
 * Copyright (C) ListenAI  2024-2025
 *
 ****************************************************************************************
 */
#ifndef _WIFI_OTA_H
#define _WIFI_OTA_H

#include "rtos_al.h"
#include "log_print.h"

#define WIFI_OTA_DBG 0
#define DEFAULT_PROGRESS_FRAG 40

#if WIFI_OTA_DBG
#define WIFI_OTA_PROGRESS_ON 0
#else
#define WIFI_OTA_PROGRESS_ON 1
#endif

#define WOTA_TAG "WOTA:"
#define WOTA_LOG_FLUSH() log_flush()
#define WOTA_LOG(fmt, ...) CLOG(fmt, ##__VA_ARGS__)
#define WOTA_PRINT(fmt, ...) logDbg(fmt, ##__VA_ARGS__)
#if WIFI_OTA_DBG
#define WOTA_LOGD(fmt, ...) CLOG(WOTA_TAG fmt, ##__VA_ARGS__)
#else
#define WOTA_LOGD(fmt, ...)
#endif

#define DEFAULT_PORT 9527
//#define DEFAULT_IP "192.168.99.217"
#define DEFAULT_IP "192.168.149.120"
#define DEFAULT_BLOCK_SIZE 4096
#define FILENAME "recv_file.bin"
#define DEF_FW_NAME "wifi_ota.bin"
#define FRAGMENT_SIZE 1024
#define MAGIC_STRING "WOTA"
#define MAGIC_STRING_LEN 4
#define WOTA_MSG_OFFSET 4
#define WIFI_OTA_HEAD_SIZE (MAGIC_STRING_LEN + 4)
#define MAX_FRAG_BODY_LEN (FRAGMENT_SIZE + WIFI_OTA_HEAD_SIZE)


enum wota_op {
    OP_CLOSE_SERVER = 0xA001,
    OP_READ_FILE = 0xA002,
    OP_FILE_INFO,
    OP_BLOCK_RECVD,
    OP_WOTA_ABORT,
};
int wifi_ota_start(const uint8_t *file_name, const uint8_t *ip_str, uint16_t port, void *extra);

#endif
