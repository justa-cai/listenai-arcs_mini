#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MD5_PRI "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
#define MD5_ARG(md5)                                                                                                   \
    md5[0], md5[1], md5[2], md5[3], md5[4], md5[5], md5[6], md5[7], md5[8], md5[9], md5[10], md5[11], md5[12],         \
        md5[13], md5[14], md5[15]

#define OTA_RES_MD5_LEN 16
#define OTA_RES_URL_LEN 512

typedef struct {
    uint32_t size;
    char md5[OTA_RES_MD5_LEN];
    char url[OTA_RES_URL_LEN];
} ota_res_info_t;

typedef struct {
    struct {
        ota_res_info_t resource;
        char text[20];
    } wakeup_word;
    ota_res_info_t greeting;
    ota_res_info_t prompt_tone;
    ota_res_info_t emoji;
} ota_dev_conf_t;

int ota_api_get_dev_conf(ota_dev_conf_t *conf);

typedef int (*ota_download_cb_t)(const ota_res_info_t *res_info, uint32_t offset, const uint8_t *data, uint32_t size);

int ota_api_download(const ota_res_info_t *res_info, ota_download_cb_t cb);

/* --- 系统 OTA（/v1/ota/packages） --- */

#define OTA_APP_VERSION_LEN    32
#define OTA_APP_PACKAGE_ID_LEN 64  /* 服务端侧包的数据库唯一 id；留足长度兼容 UUID/md5 等格式 */
#define OTA_APP_RELEASE_NOTES_LEN 384

typedef struct {
    bool available;
    char package_id[OTA_APP_PACKAGE_ID_LEN]; /* 服务端包 id，适合做"这个具体包失败过"的黑名单键 */
    char version[OTA_APP_VERSION_LEN];       /* 展示用字符串，形如 "2.3.0"。服务端不保证 semver，只用来显示 */
    uint32_t version_number;                 /* 单调整数，上层可用来判新旧 */
    bool has_md5;                            /* md5_checksum 字段非空且格式正确时为 true；false 则跳过下载校验 */
    char md5[OTA_RES_MD5_LEN];
    char url[OTA_RES_URL_LEN];
    uint32_t size;                           /* 可选字段，不存在时为 0 */
    char release_notes[OTA_APP_RELEASE_NOTES_LEN];
} ota_app_package_t;

/**
 * 检查是否有系统 OTA 升级包。
 *
 * @param pkg 出参，有可用更新时 pkg->available = true 并填充其它字段
 * @return 0 成功（包括"无可用更新"的情况，此时 pkg->available == false），
 *         < 0 失败（例如返回体 md5 字段格式非法）
 */
int ota_api_check_app(ota_app_package_t *pkg);

/**
 * 下载回调。
 *
 * - chunk_offset 是当前 chunk 在完整下载流中的起始偏移
 * - total 为服务器报告的总大小；若未知则为 0
 * - 返回非 0 可主动中断下载
 */
typedef int (*ota_app_download_cb_t)(void *user, uint32_t chunk_offset, const uint8_t *data, uint32_t size,
                                     uint32_t total);

/**
 * 流式下载系统 OTA 包。`pkg->has_md5=true` 时会边下载边计算 md5 并在结束时
 * 和 `pkg->md5` 比对；`has_md5=false` 则不做 md5 校验（服务端 md5_checksum
 * 留空的合法场景）。
 *
 * @return 下载到的总字节数；< 0 表示失败
 */
int ota_api_download_app(const ota_app_package_t *pkg, ota_app_download_cb_t cb, void *user);
