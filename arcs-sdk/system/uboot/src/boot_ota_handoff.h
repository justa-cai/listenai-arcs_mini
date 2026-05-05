#ifndef __BOOT_OTA_HANDOFF_H__
#define __BOOT_OTA_HANDOFF_H__

#include <stdbool.h>

int boot_ota_handoff_request_upgrade_reboot(void);
bool boot_ota_handoff_consume_upgrade_request(void);
bool boot_ota_handoff_has_pending_update(void);
void boot_ota_handoff_clear_pending_update(void);
bool boot_ota_handoff_should_enter_second_stage(void);

#endif /* __BOOT_OTA_HANDOFF_H__ */
