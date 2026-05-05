#include "boot_ota_handoff.h"

#include "arcs_ap.h"
#include "boot_config.h"

static volatile uint32_t *boot_ota_handoff_info_reg(void)
{
    return &IP_AON_CTRL->REG_AON_DIG_RSVD4.all;
}

static uint32_t boot_ota_handoff_request_mask(void)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    bits.info.req = 1;
    return bits.raw;
}

static uint32_t boot_ota_handoff_pending_mask(void)
{
    union {
        struct boot_info info;
        uint32_t raw;
    } bits = {0};

    bits.info.ota_pending = 1;
    return bits.raw;
}

int boot_ota_handoff_request_upgrade_reboot(void)
{
    volatile uint32_t *raw = boot_ota_handoff_info_reg();
    uint32_t value = *raw;

    value |= boot_ota_handoff_request_mask();
    value |= boot_ota_handoff_pending_mask();
    *raw = value;
    return 0;
}

bool boot_ota_handoff_consume_upgrade_request(void)
{
    volatile uint32_t *raw = boot_ota_handoff_info_reg();
    uint32_t value = *raw;

    if ((value & boot_ota_handoff_request_mask()) == 0) {
        return false;
    }

    value &= ~boot_ota_handoff_request_mask();
    *raw = value;
    return true;
}

bool boot_ota_handoff_has_pending_update(void)
{
    return (*boot_ota_handoff_info_reg() & boot_ota_handoff_pending_mask()) != 0;
}

void boot_ota_handoff_clear_pending_update(void)
{
    *boot_ota_handoff_info_reg() &= ~boot_ota_handoff_pending_mask();
}

bool boot_ota_handoff_should_enter_second_stage(void)
{
    if (boot_ota_handoff_consume_upgrade_request()) {
        return true;
    }

    return boot_ota_handoff_has_pending_update();
}
