/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * WiFi bring-up glue for the Rust wifi_scan sample.
 *
 * The WiFi core init (mac_manager + lisa_wifi_init) is asynchronous and is
 * platform boilerplate, so it lives here in C. Once the stack signals
 * readiness we hand off to rust_main(), which drives the wifi_manager
 * scan API via arcs::Wifi.
 */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define LOG_TAG "rust_wifi_scan"
#include <lisa_log.h>

#include "lisa_kv.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "lisa_wifi.h"

extern int rust_main(void);

static mac_manager_t *s_mac;
static SemaphoreHandle_t s_ready;

/* lisa_wifi requires a MAC provider; derive it from the chip-id efuse. */
static int8_t custom_get_wifi_mac(uint8_t mac[6])
{
    return mac_manager_get(s_mac, mac, 6);
}

/* Runs on the dedicated lisa_wifi task once the stack is up. */
static void on_wifi_init_done(void)
{
    LOGI("wifi core init done");
    xSemaphoreGive(s_ready);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    LOGI("=== Rust WiFi scan demo ===");

    s_ready = xSemaphoreCreateBinary();
    if (s_ready == NULL) {
        LOGE("sem create failed");
        return -1;
    }

    mac_manager_config_t mcfg = { .random_mac_if_mac_invalid = false };
    s_mac = mac_manager_init(mac_manager_ops_get()->mem_ops,
                             mac_manager_ops_get()->content_ops, &mcfg);
    if (s_mac == NULL) {
        LOGE("mac_manager_init failed");
        return -1;
    }

    lisa_kv_init();

    lisa_wifi_ops_t wops = {
        .custom_mac = custom_get_wifi_mac,
        .init_done  = on_wifi_init_done,
    };
    if (lisa_wifi_init(&wops) != 0) {
        LOGE("lisa_wifi_init failed");
        return -1;
    }

    /* Block until EVENT_WIFI_INIT_DONE fires (calibration can take ~1-2 s). */
    if (xSemaphoreTake(s_ready, pdMS_TO_TICKS(10000)) != pdTRUE) {
        LOGE("wifi init timeout");
        return -1;
    }

    int ret = rust_main();
    if (ret != 0) {
        LOGE("rust_main returned %d", ret);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return 0;
}
