
#include "FreeRTOS.h"
#include "task.h"
#include "arcs_ap.h"
#include "memap.h"
#include "lisa_wifi.h"
#include "lisa_bluetooth.h"
#include "ble_adv_data.h"
#include "bt_app_if.h"
#define TAG "wifi_ap"
#include "lisa_log.h"

static const uint8_t user_adv_data[] = {
    BLE_AD_FLAGS(GAP_AD_TYPE_FLAGS_GENERAL | GAP_AD_TYPE_FLAGS_BREDR_NOT_SUPPORTED),
    BLE_AD_COMPLETE_NAME(8, 'A', 'R', 'C', 'S', '_', 'B', 'L', 'E'),
};

const uint8_t* lisa_bt_get_adv_data(uint8_t *len)
{
    *len = sizeof(user_adv_data);
    return user_adv_data;
}

int main(int argc, char **argv)
{
#if CONFIG_ARCS_AP_CORE
    LOGI("boot cp from address: 0x%x", MEM_CP_FLASH_BASE);
    IP_CMN_SYS->REG_N300_CP_RST_ADDR.all = MEM_CP_FLASH_BASE;
    IP_SYSCTRL->REG_SW_RESET_CP0.all = 0xCAFE000A;
#endif

    lisa_wifi_init();

    lisa_bluetooth_init(NULL);

    /* WiFi connect handler is registered on CP side;
     * AP BLE netcfg will forward credentials via MRPC automatically. */

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    return 0;
}
