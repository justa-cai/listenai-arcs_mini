#ifndef __BOOT_RECOVERY_ADB_GATE_H__
#define __BOOT_RECOVERY_ADB_GATE_H__

#include <stdbool.h>
#include <stdint.h>

bool boot_recovery_adb_should_start(uint32_t boot_info_raw);

#endif /* __BOOT_RECOVERY_ADB_GATE_H__ */
