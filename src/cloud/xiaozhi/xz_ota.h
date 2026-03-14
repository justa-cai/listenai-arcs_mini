/**
 * @file xz_ota.h
 * @brief 小智云端 OTA 激活模块
 */

#ifndef __XZ_OTA_H__
#define __XZ_OTA_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 设备信息 */
typedef struct {
    char mac_address[18];        /* MAC 地址格式: "AA:BB:CC:DD:EE:FF" */
    char client_id[64];          /* 客户端 UUID */
    char board_type[32];         /* 板型号，如 "arcs_mini" */
    char app_version[32];        /* 应用版本号，如 "1.7.0" */
    char chip_model[32];         /* 芯片型号，如 "AB230NxA" */
    int flash_size;              /* Flash 大小 (bytes) */
    int ram_size;                /* RAM 大小 (bytes) */
} xz_device_info_t;

/** WebSocket 激活信息 */
typedef struct {
    char url[256];               /* WebSocket 服务器 URL */
    char token[256];             /* WebSocket 认证 Token */
} xz_websocket_info_t;

/** OTA 激活响应 */
typedef struct {
    xz_websocket_info_t websocket;
    int64_t server_time;         /* 服务器时间戳 */
    int32_t timezone_offset;     /* 时区偏移 (秒) */
    bool success;                /* 激活是否成功 */
    char error_message[256];     /* 错误信息 */
} xz_ota_response_t;

/**
 * @brief 获取设备 MAC 地址
 * @param mac_out 输出缓冲区
 * @param mac_len 缓冲区大小
 * @return 0 成功, -1 失败
 */
int xz_device_get_mac(char *mac_out, size_t mac_len);

/**
 * @brief 生成客户端 UUID
 * @param uuid_out 输出缓冲区
 * @param uuid_len 缓冲区大小
 * @return 0 成功, -1 失败
 */
int xz_device_generate_uuid(char *uuid_out, size_t uuid_len);

/**
 * @brief 初始化设备信息
 * @param info 设备信息结构
 * @return 0 成功, -1 失败
 */
int xz_device_init_info(xz_device_info_t *info);

/**
 * @brief 发送 OTA 激活请求
 * @param server_url 激活服务器 URL (如 "https://api.tenclass.net/ota/activate")
 * @param device 设备信息
 * @param response 激活响应
 * @return 0 成功, -1 失败
 */
int xz_ota_activate(const char *server_url,
                    const xz_device_info_t *device,
                    xz_ota_response_t *response);

/**
 * @brief 保存激活信息到 KV 存储
 * @param response 激活响应
 * @return 0 成功, -1 失败
 */
int xz_ota_save_activation(const xz_ota_response_t *response);

/**
 * @brief 从 KV 存储加载激活信息
 * @param response 激活响应
 * @return 0 成功, -1 失败 (无激活信息)
 */
int xz_ota_load_activation(xz_ota_response_t *response);

/**
 * @brief 检查是否已激活
 * @return true 已激活, false 未激活
 */
bool xz_ota_is_activated(void);

/**
 * @brief 清除激活信息
 */
void xz_ota_clear_activation(void);

#ifdef __cplusplus
}
#endif

#endif /* __XZ_OTA_H__ */
