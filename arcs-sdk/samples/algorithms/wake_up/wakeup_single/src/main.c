#include "stdio.h"
#include <string.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "ic_message.h"

#include "acomp.h"
#include "app_wakeup.h"

#define TAG "main"
#include "lisa_log.h"

int main(int argc, char **argv)
{
    int ret = 0;
    LISA_LOGI(TAG, "CP=======! Hard ID: %d", CONFIG_HARTID);

    ic_message_init();
    LISA_LOGI(TAG, "ic_message_init done!");

    vTaskDelay(pdMS_TO_TICKS(2000));

    acomp_init();
    app_wakeup_init();

    return 0;
}
