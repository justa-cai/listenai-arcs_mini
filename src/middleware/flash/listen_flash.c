#include "lisa_log.h"
#include "listen_flash.h"
#include "arcs_ap_base.h"

FLASH_DEV s_app_flash_dev = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xFF, //divider is 1
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 2000000,
    .addr_bytes = 3,
    .addr_auto = 0,
};

void listen_flash_init(void)
{
    int err = flash_init(&s_app_flash_dev, 0, 0);
    LISA_ASSERT((err == 0), "Flash Driver Initialize err %d ", err);
}

FLASH_DEV *listen_flash_get_dev(void)
{
    return &s_app_flash_dev;
}