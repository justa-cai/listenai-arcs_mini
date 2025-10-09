#include "arcs_ap_base.h"
#include "cli_main.h"
#include "spiflash.h"
#include "stdio.h"
#include "listen_wifi.h"
#include "listen_ble.h"
#include "stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_log.h"
#include "arcs_flash_if.h"
#include "nvs.h"

#if CFG_NVS
#define NVDS_FLASH_ADDRESS_OFFSET   (0xFF8000) //The last 32KB of 16B flash
#define NVDS_FLASH_SIZE      (0x8000) //32KB

struct nvs_fs arcs_nvs_fs;
FLASH_DEV arcs_flash_dev  = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xFF, //divider is 1
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 0x180000
};

int arcs_nvs_init(void)
{
    struct flash_pages_info info;

    flash_if_init(&arcs_flash_dev, 0, 0);

    //init nvs module
    arcs_nvs_fs.offset = NVDS_FLASH_ADDRESS_OFFSET;
    arcs_nvs_fs.flash_device = &arcs_flash_dev;
    flash_get_page_info_by_offs(&arcs_flash_dev, arcs_nvs_fs.offset, &info);
    arcs_nvs_fs.sector_size = info.size;
    arcs_nvs_fs.sector_count = NVDS_FLASH_SIZE/info.size;

    nvds_init(&arcs_nvs_fs);

    return 0;
}
#endif

int main(int argc, char **argv)
{
    // atcmd_init();

#if CONFIG_ARCS_HAL_MODULE_SHELL
    shell_init(cli_shell_process);
#endif

    arcs_nvs_init();

    ls_rf_probe();
    ls_rf_cali_proc();

    ls_wifi_init();

    ls_ble_init();

    while (1) {

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
