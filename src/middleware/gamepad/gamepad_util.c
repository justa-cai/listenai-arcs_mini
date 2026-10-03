/*
 * Gamepad 工具: 本机 IP / 设备 ID / 固件版本。
 */
#include <stdio.h>
#include <string.h>

#include "gamepad_internal.h"
#include "lisa_log.h"

#include "lwip/sockets.h"
#include "net_al.h"
#include "project_version.h"

#define TAG "gamepad"

bool gamepad_util_get_local_ip(char *buf, uint32_t buf_len)
{
    /* 经 net_if 读取 STA IPv4 (设备联网后才有效) */
    net_if_t *net_if = net_if_get(WIFI_VIF_STA_IDX);
    uint32_t ip = 0;
    uint32_t mask = 0;
    uint32_t gw = 0;

    if (!net_if || net_if_get_ip(net_if, &ip, &mask, &gw) != 0 || ip == 0) {
        return false;
    }

    snprintf(buf, buf_len, "%u.%u.%u.%u",
             (unsigned)(ip & 0xFF),
             (unsigned)((ip >> 8) & 0xFF),
             (unsigned)((ip >> 16) & 0xFF),
             (unsigned)((ip >> 24) & 0xFF));
    return true;
}

__attribute__((weak)) const char *gamepad_util_get_device_id(void)
{
    /* app_ble_common.c 的 KV 读取实现 (KV_KEY_USER_DEVICE_ID) */
    extern int get_current_device_id(char *device_id, int max_len);
    static char s_device_id[40] = "unknown";

    if (strcmp(s_device_id, "unknown") == 0) {
        get_current_device_id(s_device_id, sizeof(s_device_id));
    }
    return s_device_id;
}

const char *gamepad_util_get_fw_version(void)
{
    return PROJECT_VERSION_STR; /* project_version.h (version.cmake 生成) */
}
