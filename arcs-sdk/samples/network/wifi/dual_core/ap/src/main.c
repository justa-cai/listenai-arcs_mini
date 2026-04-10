

#include "FreeRTOS.h"
#include "task.h"
#include "arcs_ap.h"
#include "memap.h"
#include "lisa_wifi.h"

#define TAG "wifi_ap"
#include "lisa_log.h"

int main(int argc, char **argv)
{
#if CONFIG_ARCS_AP_CORE
    LOGI("boot cp from address: 0x%x", MEM_CP_FLASH_BASE);
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = MEM_CP_FLASH_BASE;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
#endif

    lisa_wifi_init();


    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

    } 
    return 0;
}
