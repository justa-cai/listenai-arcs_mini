#ifndef __BOOT_ADB_DISK_POLICY_H__
#define __BOOT_ADB_DISK_POLICY_H__

#include <stdbool.h>
#include <stdint.h>

bool adb_sync_ext_disk_policy_can_access(const char *name, uint64_t addr, uint64_t size, bool write);
int adb_sync_ext_disk_policy_write_start(const char *name, uint64_t addr, uint64_t size);
void adb_sync_ext_disk_policy_write_done(const char *name, uint64_t addr, uint64_t size);

#endif
