

#include "FreeRTOS.h"
#include "task.h"
#include "arcs_ap.h"
#include "memap.h"
#include "lisa_wifi.h"

#define TAG "wifi_ap"
#include "lisa_log.h"

/* AP uses MEM_CP_FLASH_BASE to reset CP; it MUST equal CP prj.conf:CONFIG_MEM_FLASH_BASE.
 * Sample top-level CMake forwards CP's actual value as EXPECTED_CP_FLASH_BASE. */
_Static_assert(MEM_CP_FLASH_BASE == EXPECTED_CP_FLASH_BASE,
               "remote/memap.h: MEM_CP_FLASH_BASE mismatches CP prj.conf:CONFIG_MEM_FLASH_BASE");

int main(int argc, char **argv)
{
#if CONFIG_ARCS_AP_CORE
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = MEM_CP_FLASH_BASE;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
#endif

    lisa_wifi_init();

    LOGI("AP wifi ready");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    return 0;
}
