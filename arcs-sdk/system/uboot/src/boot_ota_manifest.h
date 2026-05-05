#ifndef __BOOT_OTA_MANIFEST_H__
#define __BOOT_OTA_MANIFEST_H__

#include "boot_ota.h"

int boot_ota_manifest_parse_json(const char *json_text, ota_param_t *param);

#endif /* __BOOT_OTA_MANIFEST_H__ */
