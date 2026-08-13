/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#include "shell.h"

#define TAG "app-bt"
#include "lisa_log.h"

#include "lisa_shell.h"
#include "shell_passthrough.h"
#include "lisa_bluetooth.h"
#include "lisa_bt_classic_api.h"
#include "fs/user_fs.h"

static int parse_bt_addr(const char *str, gap_bdaddr_t *addr)
{
    char *end = NULL;
    unsigned long value;

    if (!str || !addr) {
        return -1;
    }

    memset(addr, 0, sizeof(*addr));
    addr->addr_type = 0;

    for (int i = 0; i < 6; i++) {
        value = strtoul(str, &end, 16);
        if ((end == str) || (value > 0xFF)) {
            return -1;
        }

        addr->addr[i] = (uint8_t)value;
        if (i < 5) {
            if (*end != ':') {
                return -1;
            }
            str = end + 1;
        } else if (*end != '\0') {
            return -1;
        }
    }

    return 0;
}


static const char *bt_paired_transport_name(uint8_t transport)
{
    switch (transport) {
    case BT_PAIRED_TRANSPORT_BLE:
        return "BLE";
    case BT_PAIRED_TRANSPORT_CLASSIC:
        return "Classic";
    default:
        return "Unknown";
    }
}

static void bt_print_addr(const gap_bdaddr_t *addr)
{
    LOGI("%02X:%02X:%02X:%02X:%02X:%02X type=%u",
         addr->addr[0], addr->addr[1], addr->addr[2],
         addr->addr[3], addr->addr[4], addr->addr[5],
         addr->addr_type);
}

static int cmd_bt_paired_list(int argc, char *argv[])
{
    bt_paired_info_t list[BT_PAIRED_MAX_COUNT];
    uint8_t count = 0;
    int ret;

    (void)argc;
    (void)argv;

    ret = bt_paired_list_get(list, BT_PAIRED_MAX_COUNT, &count);
    if (ret != 0 && ret != -ENOSPC) {
        LOGE("bt_paired_list_get failed: %d", ret);
        return ret;
    }

    LOGI("Paired device count: %u", count);
    for (uint8_t i = 0; i < count && i < BT_PAIRED_MAX_COUNT; i++) {
        LOGI("paired[%u]: transport=%s name=%.*s", i,
             bt_paired_transport_name(list[i].transport),
             list[i].name_len, (const char *)list[i].name);
        bt_print_addr(&list[i].addr);
    }

    if (ret == -ENOSPC) {
        LOGW("Paired list truncated to %u entries", BT_PAIRED_MAX_COUNT);
    }

    return ret;
}

static int cmd_bt_paired_name(int argc, char *argv[])
{
    gap_bdaddr_t addr;
    char name[BT_PAIRED_NAME_MAX_LEN + 1];
    int ret;

    if (argc < 2) {
        LOGE("Usage: bt_paired_name <XX:XX:XX:XX:XX:XX>");
        return -1;
    }

    if (parse_bt_addr(argv[1], &addr) != 0) {
        LOGE("Invalid bluetooth address: %s", argv[1]);
        return -1;
    }

    ret = bt_paired_name_get(&addr, name, sizeof(name));
    if (ret != 0) {
        LOGE("bt_paired_name_get failed: %d", ret);
        return ret;
    }

    LOGI("Paired device name: %s", name);
    return 0;
}

static int cmd_bt_paired_remove(int argc, char *argv[])
{
    gap_bdaddr_t addr;
    int ret;

    if (argc < 2 || strcmp(argv[1], "all") == 0) {
        ret = bt_paired_remove(NULL);
        LOGI("Remove all paired devices ret=%d", ret);
        return ret;
    }

    if (parse_bt_addr(argv[1], &addr) != 0) {
        LOGE("Invalid bluetooth address: %s", argv[1]);
        LOGE("Usage: bt_paired_remove [all|XX:XX:XX:XX:XX:XX]");
        return -1;
    }

    ret = bt_paired_remove(&addr);
    LOGI("Remove paired device %s ret=%d", argv[1], ret);
    return ret;
}

static int cmd_bt_scan(int argc, char **argv)
{
    char *end = NULL;
    unsigned long value;
    int ret;

    if (argc != 2) {
        LOGE("Usage: bt_scan <0|1|2|3>\n");
        return -1;
    }

    value = strtoul(argv[1], &end, 0);
    if ((argv[1][0] == '\0') || (end == NULL) || (*end != '\0') || (value > 3)) {
        LOGE("Invalid argument: %s\n", argv[1]);
        LOGE("Usage: bt_scan <0|1|2|3>\n");
        return -1;
    }

    ret = lisa_bt_scan((uint8_t)value);
    if (ret != 0) {
        LOGE("bt_scan failed: %d\n", ret);
        return ret;
    }

    LOGI("bt_scan %lu\n", value);
    return 0;
}

static int cmd_bt_open(int argc, char *argv[])
{
    int ret;

    (void)argc;
    (void)argv;

    ret = lisa_bluetooth_open();
    LOGI("bt_open ret=%d opened=%d", ret, lisa_bluetooth_is_opened());
    return ret;
}

static int cmd_bt_close(int argc, char *argv[])
{
    int ret;

    (void)argc;
    (void)argv;

    ret = lisa_bluetooth_close();
    LOGI("bt_close ret=%d opened=%d", ret, lisa_bluetooth_is_opened());
    return ret;
}

static int cmd_bt_status(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    LOGI("Bluetooth opened: %s", lisa_bluetooth_is_opened() ? "yes" : "no");
    return 0;
}

static int cmd_bt_disconnect_by_addr(int argc, char *argv[])
{
    gap_bdaddr_t addr;
    int ret;

    if (argc < 2) {
        LOGE("Usage: bt_disconnect_addr <XX:XX:XX:XX:XX:XX>");
        return -1;
    }

    if (parse_bt_addr(argv[1], &addr) != 0) {
        LOGE("Invalid bluetooth address: %s", argv[1]);
        LOGE("Usage: bt_disconnect_addr <XX:XX:XX:XX:XX:XX>");
        return -1;
    }

    ret = lisa_bluetooth_disconnect_by_addr(&addr);
    if (ret != 0) {
        LOGE("Failed to disconnect device addr '%s': %d", argv[1], ret);
    }
    return ret;
}

int main(int argc, char **argv)
{
    int ret = 0;

    extern uint8_t _sshram[], _eshram[];
    memset(_sshram, 0, (_eshram - _sshram));

    atcmd_init();

    ret = lisa_shell_init();
    if (ret != 0) {
        LOGI("Failed to initialize shell (error: %d)\n", ret);
    }

    /* Initialize filesystem for BT pairing info storage */
    user_fs_init();
    lisa_kv_init();

    lisa_bluetooth_init(NULL);

    ret = bt_audio_framework_init();
    if (ret != 0) {
        LOGE("Failed to initialize BT audio framework: %d", ret);
        return -1;
    }

    ret = bt_sink_init();
    if (ret != 0) {
        LOGE("Failed to initialize BT SINK: %d", ret);
        return -1;
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}

static int at_passthrough_handler(char *data, unsigned short len)
{
    atcmd_handler(data, len);

    return 0;
}

// Register AT passthrough mode command
// Usage:
//   Interactive mode: AT (then input AT+?, no space needed)
//   Single-line mode: AT AT+? (execute at once)

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_paired_list, cmd_bt_paired_list, list paired bt devices);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_paired_name, cmd_bt_paired_name, get paired bt device name by address);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_paired_remove, cmd_bt_paired_remove, remove paired bt device by address or all);

SHELL_EXPORT_PASSTROUGH(SHELL_CMD_PERMISSION(0), AT, AT>>, at_passthrough_handler, AT command passthrough mode);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_open, cmd_bt_open, open bluetooth stack);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_close, cmd_bt_close, close bluetooth stack);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_status, cmd_bt_status, show bluetooth status);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_scan, cmd_bt_scan, enable or disable bt classic scan);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 bt_disconnect_addr, cmd_bt_disconnect_by_addr, bt disconnect device by address);
