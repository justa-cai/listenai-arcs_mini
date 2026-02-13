#pragma once

#include <stddef.h>
#include <stdint.h>

#define MD5_PRI "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
#define MD5_ARG(md5)                                                                                                   \
    md5[0], md5[1], md5[2], md5[3], md5[4], md5[5], md5[6], md5[7], md5[8], md5[9], md5[10], md5[11], md5[12],         \
        md5[13], md5[14], md5[15]

#define OTA_RES_MD5_LEN 16
#define OTA_RES_URL_LEN 256

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
} ota_dev_conf_t;

int ota_api_get_dev_conf(ota_dev_conf_t *conf);

typedef int (*ota_download_cb_t)(const ota_res_info_t *res_info, uint32_t offset, const uint8_t *data, uint32_t size);

int ota_api_download(const ota_res_info_t *res_info, ota_download_cb_t cb);
