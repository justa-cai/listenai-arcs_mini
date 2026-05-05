#ifndef __BOOT_CONTROL_STORE_H__
#define __BOOT_CONTROL_STORE_H__

#include "boot_ota_request.h"

#define BOOT_CONTROL_STORE_MAGIC   0x42435453U
#define BOOT_CONTROL_STORE_VERSION 2U

int boot_control_store_save(const boot_ota_request_record_t *record);
int boot_control_store_load(boot_ota_request_record_t *record);
int boot_control_store_clear(void);
int boot_control_store_get_mode(boot_mode_t *mode);
int boot_control_store_set_mode(boot_mode_t mode);
int boot_control_store_get_failure(uboot_ota_failure_info_t *info);
int boot_control_store_set_failure(const uboot_ota_failure_info_t *info);
int boot_control_store_clear_failure(void);

#endif /* __BOOT_CONTROL_STORE_H__ */
