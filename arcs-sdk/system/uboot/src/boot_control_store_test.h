#ifndef __BOOT_CONTROL_STORE_TEST_H__
#define __BOOT_CONTROL_STORE_TEST_H__

#include "boot_control_store.h"

void boot_control_store_test_reset(void);
boot_mode_t boot_control_store_test_get_mode(void);
int boot_control_store_test_get_save_sequence(void);
int boot_control_store_test_get_mode_sequence(void);
int boot_control_store_test_get_failure(uboot_ota_failure_info_t *info);

#endif /* __BOOT_CONTROL_STORE_TEST_H__ */
