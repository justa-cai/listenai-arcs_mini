#include "boot_env.h"
#include "boot_ota_request.h"

static boot_slot_t g_boot_active_slot = BOOT_SLOT_A;

int boot_env_init(uint32_t store_base)
{
    (void)store_base;
    return 0;
}

boot_mode_t boot_env_get_mode(void)
{
    return boot_ota_request_get_mode();
}

bool boot_env_is_update_mode(void)
{
    return boot_env_get_mode() != BOOT_MODE_NORMAL;
}

int boot_env_set_mode(boot_mode_t mode)
{
    return boot_ota_request_set_mode(mode);
}

int boot_env_enter_update_mode(void)
{
    return boot_env_set_mode(BOOT_MODE_UPDATE);
}

int boot_env_enter_normal_mode(void)
{
    return boot_env_set_mode(BOOT_MODE_NORMAL);
}

boot_slot_t boot_env_get_active_slot(void)
{
    return g_boot_active_slot;
}

int boot_env_set_active_slot(boot_slot_t slot)
{
    g_boot_active_slot = slot;
    return 0;
}
