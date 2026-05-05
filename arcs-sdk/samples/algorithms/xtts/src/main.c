#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "ic_message.h"
#include "lisa_device.h"
#include "lisa_sdmmc.h"

#include "acomp.h"
#include "app_xtts.h"

#if defined(CONFIG_DISK_MEM_SERVER)
#include "disk_mem.h"
#endif

#define TAG "main"
#include "lisa_log.h"

int main(int argc, char **argv)
{
    LISA_LOGI(TAG, "CP=======! Hard ID: %d", CONFIG_HARTID);

    ic_message_init();
    LISA_LOGI(TAG, "ic_message_init done!");

#if defined(CONFIG_LISA_SDMMC_DEVICE)
    int ret;
    lisa_device_t *sdmmc;

    sdmmc = lisa_device_get("sdmmc0");
    if (!sdmmc) {
        LISA_LOGE(TAG, "get sdmmc0 failed");
    } else {
        ret = lisa_sdmmc_probe(sdmmc);
        if (ret != LISA_DEVICE_OK) {
            LISA_LOGE(TAG, "sdmmc probe failed: %d", ret);
        } else {
            LISA_LOGI(TAG, "sdmmc probe success");
        }
    }
#endif

#if defined(CONFIG_DISK_MEM_SERVER)
    disk_mem_urpc_init();
    LISA_LOGI(TAG, "disk_mem_urpc_init done!");
#endif

    vTaskDelay(pdMS_TO_TICKS(2000));

    acomp_init();

    return app_xtts_init();
}
