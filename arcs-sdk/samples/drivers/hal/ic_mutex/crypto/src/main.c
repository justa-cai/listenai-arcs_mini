#include "stdio.h"
#include <stdbool.h>

// module: ic
#include "ic_message.h"

#include "FreeRTOS.h"

extern void ls_crypto_init(void);
extern void ls_crypto_sha256_test(void);

void ic_mutex_task(void *param)
{
    bool sample_reported = false;

    printf("ic_mutex_task enter...\n");

    ls_crypto_init();

    while (1) {
        ls_crypto_sha256_test();
        if (!sample_reported) {
            printf("IC mutex crypto sample passed\n");
            sample_reported = true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int main(int argc, char **argv)
{
    ic_message_init();
    printf("ic_message_init done!\n");

    xTaskCreate(ic_mutex_task, "ic_mutex_task",4096, NULL, 6, NULL);

    return 0;
}
