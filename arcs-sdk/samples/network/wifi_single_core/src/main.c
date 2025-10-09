#include "stdio.h"
#include "listen_wifi.h"
#include "stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lisa_log.h"
#include "arcs_flash_if.h"


int main(int argc, char **argv)
{
    arcs_flash_init();
    ls_wifi_init();

    while (1) {

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
