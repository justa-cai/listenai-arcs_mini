/*
 * demo_compat_shim.c
 *
 * Provides symbols required by the CMake build system (bt_hal, atcmd)
 * that are not present in the original demo bt_if sources.
 * Only compiled when CONFIG_LISA_BT_USE_DEMO_SRC is enabled.
 */
#include <stdint.h>
#include <stddef.h>

#include "ble_gap.h"

/* BLE advertising data — bt_app_hal.c calls these via weak linkage */
static const uint8_t _default_adv[] = {0x02, 0x01, 0x06};

__attribute__((weak))
const uint8_t* lisa_bt_get_adv_data(uint8_t *len)
{
    *len = sizeof(_default_adv);
    return _default_adv;
}

__attribute__((weak))
const uint8_t* lisa_bt_get_scan_rsp_data(uint8_t *len)
{
    *len = 0;
    return NULL;
}

/* atcmd calls this */
__attribute__((weak))
void atcmd_ble_enc_clear_send(void)
{
}
