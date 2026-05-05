#ifndef __BOOT_OTA_REQUEST_H__
#define __BOOT_OTA_REQUEST_H__

#include <stdbool.h>
#include <stdint.h>

#include "boot_partab.h"
#include "uboot_ota_api.h"

#define BOOT_OTA_REQUEST_MAGIC   0x5154414fU
#define BOOT_OTA_REQUEST_VERSION 1U

typedef struct {
    uint32_t magic;
    uint32_t version;
    uboot_ota_request_t request;
} boot_ota_request_record_t;

int boot_ota_request_is_valid(const uboot_ota_request_t *req);
int boot_ota_request_save(const uboot_ota_request_t *req);
int boot_ota_request_load(uboot_ota_request_t *req);
int boot_ota_request_clear(void);
boot_mode_t boot_ota_request_get_mode(void);
int boot_ota_request_set_mode(boot_mode_t mode);
int boot_ota_request_mark_update_mode(void);

/* Flash 里是否挂着一份 OTA 请求记录（持久化，直到 lifecycle 成功/放弃
 * 才会被清）。用来识别上一轮 OTA 没跑完、RSVD4 里 mode 又被硬复位清
 * 掉的情形——只要请求记录还在，就能把残局接着跑完。*/
bool boot_ota_request_has_pending(void);

#endif /* __BOOT_OTA_REQUEST_H__ */
