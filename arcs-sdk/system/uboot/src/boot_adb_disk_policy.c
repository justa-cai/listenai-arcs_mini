#include "boot_adb_disk_policy.h"

#include "boot_flash.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

extern bool boot_handshake_is_need(void);
extern bool boot_handshake_is_ok(void);
extern uint8_t boot_upgrade_mode_get(void);

#define BOOT_ADB_PROTECT_AREA_SIZE (256U * 1024U)

static bool boot_adb_disk_policy_is_flash_device(const char *name)
{
    if (name == NULL) {
        return false;
    }

    return strncmp(name, "NAND", 4) == 0 || strncmp(name, "FLASH", 5) == 0;
}

__attribute__((weak)) void disk_device_write_start(const char *name, uint64_t addr,
                                                   uint64_t size)
{
    (void)size;

    if (!boot_adb_disk_policy_is_flash_device(name)) {
        return;
    }

    boot_flash_lock_clear();

    if (addr >= BOOT_ADB_PROTECT_AREA_SIZE) {
        boot_flash_lock_boot();
    }
}

__attribute__((weak)) void disk_device_write_done(const char *name, uint64_t addr,
                                                  uint64_t size)
{
    (void)addr;
    (void)size;

    if (!boot_adb_disk_policy_is_flash_device(name)) {
        return;
    }

    boot_flash_lock_resume();
}

__attribute__((weak)) bool disk_device_can_access(const char *name, uint64_t addr)
{
    if (name == NULL) {
        return false;
    }

    if (boot_handshake_is_need() && !boot_handshake_is_ok()) {
        return false;
    }

    if (!boot_adb_disk_policy_is_flash_device(name)) {
        return true;
    }

    if (addr < BOOT_ADB_PROTECT_AREA_SIZE && !boot_upgrade_mode_get()) {
        return false;
    }

    return true;
}

bool adb_sync_ext_disk_policy_can_access(const char *name, uint64_t addr, uint64_t size, bool write)
{
    (void)size;
    (void)write;
    return disk_device_can_access(name, addr);
}

int adb_sync_ext_disk_policy_write_start(const char *name, uint64_t addr, uint64_t size)
{
    disk_device_write_start(name, addr, size);
    return 0;
}

void adb_sync_ext_disk_policy_write_done(const char *name, uint64_t addr, uint64_t size)
{
    disk_device_write_done(name, addr, size);
}
