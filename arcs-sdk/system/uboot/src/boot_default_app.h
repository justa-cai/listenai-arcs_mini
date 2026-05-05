#ifndef __BOOT_DEFAULT_APP_H__
#define __BOOT_DEFAULT_APP_H__

#include <stdbool.h>
#include <stdint.h>

bool boot_default_app_image_is_valid(const uint8_t *image_base, uint32_t expected_base);
bool boot_default_app_is_valid(void);
void boot_default_app_prepare_recovery(uint32_t *boot_info_raw, bool app_valid);

#endif /* __BOOT_DEFAULT_APP_H__ */
