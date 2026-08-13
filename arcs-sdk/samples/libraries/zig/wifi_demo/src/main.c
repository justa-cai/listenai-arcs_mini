#define LOG_TAG "zig_wifi"

#include <lisa_log.h>
#include "FreeRTOS.h"
#include "task.h"

extern int zig_wifi_demo_main(void);
extern int zig_wifi_scan_task(void);

int main(int argc, char **argv)
{
    LOGI("=== Zig WiFi Scan Demo ===");

    int ret = zig_wifi_demo_main();
    if (ret != 0) {
        LOGE("WiFi init failed (ret=%d)", ret);
        return ret;
    }

    ret = zig_wifi_scan_task();
    if (ret == 0) {
        LOGI("=== WiFi Scan PASSED ===");
    } else {
        LOGE("WiFi scan failed (ret=%d)", ret);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

    return 0;
}
