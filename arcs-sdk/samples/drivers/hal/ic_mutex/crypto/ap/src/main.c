#include "stdio.h"
#include <string.h>
#include <stdbool.h>
// #include <assert.h>

#include "arcs_ap.h"
#include "ic_message.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

extern void ls_crypto_init(void);
extern void ls_crypto_sha256_test(void);

void ic_mutex_task(void *param)
{
    int ret;

    printf("ic_mutex_task enter...\n");

    ls_crypto_init();

    while (1) {

        ls_crypto_sha256_test();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int main(int argc, char **argv)
{
    printf("AP Hard ID: %d\n", CONFIG_HARTID);

#if CONFIG_ARCS_AP_CORE
    #define MEM_CP_FLASH_BASE  (0x30d00000)
    #define MEM_CP_FLASH_SIZE  (0x300000)
    printf("boot cp from address: 0x%x\n", MEM_CP_FLASH_BASE);
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = MEM_CP_FLASH_BASE;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
#endif

    ic_message_init();
    printf("ic_message_init done!\n");

    xTaskCreate(ic_mutex_task, "ic_mutex_task", 4096, NULL, 6, NULL);

    return 0;
}
