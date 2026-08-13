/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief lisa_pm 双核 audio 设备睡眠/唤醒验证（CP 核）
 *
 * 验证 audio 设备的 destroy（睡前释放资源）/ reinit（唤醒后恢复到 init 状态）：
 *   1) 开机连 WiFi，配置 GPIOB9 为唤醒源，默认 ACTIVE 策略，启动 shell。
 *   2) `audio_test`  ：建测试线程做录音功能验证，PASS 后驻留等待 low_power。
 *   3) `low_power`   ：删测试线程 + lisa_device_destroy(audio0)，进 AUTO_LIGHT_SLEEP。
 *   4) 按 GPIOB9 唤醒：after_wake 回调切回 ACTIVE + lisa_device_reinit(audio0)。
 *   5) 可再次 `audio_test` 验证 reinit 后设备功能正常。
 *
 * 关键约束：lisa_device_destroy/reinit 会创建/删除 FreeRTOS 对象与堆内存，只能在
 * 正常任务上下文调用——destroy 在 shell 任务执行，reinit 在 lisa_pm 的 after_wake
 * 任务执行，二者都不在 HAL 关中断的 PM 临界区内。
 *
 * AP 固件由上级 device_wakeup/remote 共用（与 gpiob_device_wakeup 同一份）。
 */

#define LOG_TAG "lisa_pm_dc_audio"
#include <lisa_log.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "IOMuxManager.h"
#include "shell.h"
#include "lisa_shell.h"

#include "lisa_pm.h"
#include "lisa_device.h"
#include "lisa_gpio.h"
#include "lisa_audio.h"
#include "lisa_wifi.h"
#include "ls_event.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "net_al.h"
#include "net_def.h"
#include "net_ip.h"
#include "wifi_api.h"

#define TARGET_WIFI_SSID            "Xiaomi_listenai_2.4G"
#define TARGET_WIFI_PWD             "Xiaomi_listenai_2.4G"

#define WIFI_PS_LISTEN_INTERVAL     10
#define WIFI_WAIT_TIMEOUT_S         30
#define CP_CONTEXT_LOG_MS           5000
#define CP_CONTEXT_MAGIC            0x43504354U
#define CP_CONTEXT_SEED             0x5A5A0000U
#define WAKEUP_GPIO_DEV_NAME        "gpiob"
#define WAKEUP_GPIO_PIN             9

/* ===== audio 测试参数 ===== */
#define AUDIO_DEVICE_NAME           "audio0"
#define AUDIO_TEST_RATE             LISA_AUDIO_RATE_16K
#define AUDIO_TEST_CHANNELS         LISA_AUDIO_CH_LEFT
#define AUDIO_TEST_BITS             LISA_AUDIO_BIT_16
#define AUDIO_TEST_RECORD_MS        500
#define AUDIO_TEST_RATE_HZ          16000
#define AUDIO_TEST_EXPECT_SAMPLES   (AUDIO_TEST_RATE_HZ * AUDIO_TEST_RECORD_MS / 1000)
/* 容忍启动/停止边界的样本损耗，达到期望值一半即判 PASS。 */
#define AUDIO_TEST_MIN_SAMPLES      (AUDIO_TEST_EXPECT_SAMPLES / 2)
#define AUDIO_TEST_RECORD_ANALOG_GAIN   16
#define AUDIO_TEST_RECORD_DIGITAL_GAIN  8
#define AUDIO_TEST_TASK_STACK       2048
#define AUDIO_TEST_TASK_PRIO        3

#ifndef EXPECTED_CP_FLASH_BASE
#define EXPECTED_CP_FLASH_BASE      0x30100000U
#endif

#ifndef EXPECTED_CP_PSRAM_BASE
#define EXPECTED_CP_PSRAM_BASE      0x28400000U
#endif

#ifndef EXPECTED_CP_PSRAM_SIZE
#define EXPECTED_CP_PSRAM_SIZE      0x00400000U
#endif

_Static_assert(CONFIG_MEM_FLASH_BASE == EXPECTED_CP_FLASH_BASE,
               "CP flash base must match top-level CMake/remote memap");
_Static_assert(CONFIG_MEM_PSRAM_BASE == EXPECTED_CP_PSRAM_BASE,
               "CP PSRAM base must be 0x28400000");
_Static_assert(CONFIG_MEM_PSRAM_SIZE == EXPECTED_CP_PSRAM_SIZE,
               "CP PSRAM size must be 4M");

static mac_manager_t *g_mac_manager;
static volatile bool g_wifi_connected;
static volatile bool g_ip_ready;
static bool g_low_power_enabled;
static volatile uint32_t g_cp_magic;
static volatile uint32_t g_cp_counter;
static volatile uint32_t g_cp_checksum;
static volatile uint32_t g_cp_failed;

/* audio 测试线程句柄与回调累计样本数 */
static TaskHandle_t g_audio_test_task;
static volatile uint32_t g_audio_rec_samples;

/* GPIOB_9 运行态走普通 GPIO mux；进入 sleep 前 PM 底层会切到 AON wake mux。 */
void lisa_gpiob_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, WAKEUP_GPIO_PIN,
                              CSK_IOMUX_FUNC_DEFAULT);
}

static const char *wakeup_cause_str(lisa_pm_wakeup_cause_t cause)
{
    switch (cause) {
    case LISA_PM_WAKEUP_TIMER:   return "TIMER";
    case LISA_PM_WAKEUP_RTC:     return "RTC";
    case LISA_PM_WAKEUP_BT:      return "BT";
    case LISA_PM_WAKEUP_WIFI:    return "WIFI";
    case LISA_PM_WAKEUP_GPIO:    return "GPIO";
    default:                     return "UNKNOWN";
    }
}

static void gpiob_irq_callback(uint32_t pin, void *user_data)
{
    (void)user_data;

    /* GPIOB9 仅作系统唤醒源，这里只记录，不参与判定。 */
    LOGI("GPIOB_%lu wake IRQ", (unsigned long)pin);
}

static int setup_gpiob_wakeup_source(void)
{
    lisa_device_t *gpiob = lisa_device_get(WAKEUP_GPIO_DEV_NAME);
    int ret;
    int level;

    if (!lisa_device_ready(gpiob)) {
        LOGE("%s device not ready", WAKEUP_GPIO_DEV_NAME);
        return -1;
    }

    if (!lisa_device_wakeup_is_capable(gpiob)) {
        LOGE("%s device is not wakeup-capable", WAKEUP_GPIO_DEV_NAME);
        return -1;
    }

    ret = lisa_gpio_configure(gpiob, WAKEUP_GPIO_PIN,
                              LISA_GPIO_INPUT | LISA_GPIO_PULL_UP);
    if (ret != 0) {
        LOGE("configure GPIOB_%d input failed: %d", WAKEUP_GPIO_PIN, ret);
        return -1;
    }

    ret = lisa_gpio_configure_irq(gpiob, WAKEUP_GPIO_PIN,
                                  LISA_GPIO_IRQ_EDGE_FALLING,
                                  gpiob_irq_callback, NULL);
    if (ret != 0) {
        LOGE("configure GPIOB_%d IRQ failed: %d", WAKEUP_GPIO_PIN, ret);
        return -1;
    }

    ret = lisa_gpio_enable_irq(gpiob, WAKEUP_GPIO_PIN);
    if (ret != 0) {
        LOGE("enable GPIOB_%d IRQ failed: %d", WAKEUP_GPIO_PIN, ret);
        return -1;
    }

    ret = lisa_gpio_configure_wakeup(gpiob, WAKEUP_GPIO_PIN,
                                     LISA_GPIO_WAKEUP_LEVEL_LOW);
    if (ret != 0) {
        LOGE("configure GPIOB_%d wakeup failed: %d", WAKEUP_GPIO_PIN, ret);
        return -1;
    }

    ret = lisa_device_wakeup_enable(gpiob, true);
    if (ret != 0) {
        LOGE("enable GPIOB_%d wakeup failed: %d", WAKEUP_GPIO_PIN, ret);
        return -1;
    }

    level = lisa_gpio_read_pin(gpiob, WAKEUP_GPIO_PIN);
    if (level == LISA_GPIO_LOW) {
        LOGW("GPIOB_%d is already low; release the key before sleep to avoid immediate wakeup",
             WAKEUP_GPIO_PIN);
    } else if (level < 0) {
        LOGW("read GPIOB_%d level failed: %d", WAKEUP_GPIO_PIN, level);
    }

    LOGI("GPIOB_%d configured as LEVEL_LOW wakeup source", WAKEUP_GPIO_PIN);
    return 0;
}

/* ======================================================================
 * audio 设备功能验证（仅录音）
 * ====================================================================== */

/* 录音回调运行在中断上下文，仅累计样本数，保持简短。 */
static void audio_test_record_cb(const lisa_audio_event_t *event, void *user_data)
{
    (void)user_data;

    if (event->record_buffer) {
        g_audio_rec_samples += event->record_samples;
    }
}

/*
 * 录音功能验证：配置 record → 采集 AUDIO_TEST_RECORD_MS → 统计回调样本数。
 * 采集到的样本数达到期望一半即认为 ADC + DMA + dispatch + callback 通路正常。
 *
 * @return 0 PASS，<0 FAIL
 */
static int audio_functional_test(void)
{
    lisa_device_t *dev = lisa_device_get(AUDIO_DEVICE_NAME);
    int ret;

    if (dev == NULL) {
        LOGE("audio device %s not found", AUDIO_DEVICE_NAME);
        return -1;
    }

    /* destroy 之后设备处于 UNINITIALIZED，这里按需重建到 audio_init 状态。 */
    if (!lisa_device_ready(dev)) {
        ret = lisa_device_reinit(dev);
        if (ret != LISA_DEVICE_OK) {
            LOGE("audio reinit failed before test: %d", ret);
            return -1;
        }
        LOGI("audio reinit ok before test");
    }

    lisa_audio_record_config_t cfg = {
        .format = {
            .sample_rate = AUDIO_TEST_RATE,
            .channels    = AUDIO_TEST_CHANNELS,
            .sample_bits = AUDIO_TEST_BITS,
        },
        .gain = {
            .analog_gain  = AUDIO_TEST_RECORD_ANALOG_GAIN,
            .digital_gain = AUDIO_TEST_RECORD_DIGITAL_GAIN,
        },
        .enable_hpf         = true,
        .differential_input = true,
    };

    ret = lisa_audio_register_callback(dev, audio_test_record_cb, NULL);
    if (ret != LISA_DEVICE_OK) {
        LOGE("audio register callback failed: %d", ret);
        return -1;
    }

    ret = lisa_audio_record_config(dev, &cfg);
    if (ret != LISA_DEVICE_OK) {
        LOGE("audio record config failed: %d", ret);
        lisa_audio_unregister_callback(dev, audio_test_record_cb);
        return -1;
    }

    g_audio_rec_samples = 0;

    ret = lisa_audio_record_start(dev);
    if (ret != LISA_DEVICE_OK) {
        LOGE("audio record start failed: %d", ret);
        lisa_audio_unregister_callback(dev, audio_test_record_cb);
        return -1;
    }

    vTaskDelay(pdMS_TO_TICKS(AUDIO_TEST_RECORD_MS));

    lisa_audio_record_stop(dev);
    lisa_audio_unregister_callback(dev, audio_test_record_cb);

    uint32_t got = g_audio_rec_samples;
    if (got >= AUDIO_TEST_MIN_SAMPLES) {
        LOGI("AUDIO TEST PASS: captured %lu samples (expect ~%d, min %d)",
             (unsigned long)got, AUDIO_TEST_EXPECT_SAMPLES, AUDIO_TEST_MIN_SAMPLES);
        return 0;
    }

    LOGE("AUDIO TEST FAIL: captured %lu samples < min %d",
         (unsigned long)got, AUDIO_TEST_MIN_SAMPLES);
    return -1;
}

static void audio_test_thread(void *arg)
{
    (void)arg;

    LOGI("audio test thread start");

    if (audio_functional_test() == 0) {
        LOGI("audio0 ready; you can run `low_power` to sleep, or `audio_test` again");
    } else {
        LOGE("audio0 functional test failed");
    }

    /* 跑一次后驻留，等待 low_power 命令删除本线程再 destroy 设备。 */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ======================================================================
 * 低功耗进入 / 唤醒恢复
 * ====================================================================== */

static int enter_low_power(void)
{
    if (g_low_power_enabled) {
        return 0;
    }

    if (net_enable_keep_alive() != 0) {
        LOGE("net_enable_keep_alive failed");
        return -1;
    }

    if (lisa_pm_set_system_policy(LISA_PM_SYSTEM_POLICY_AUTO_LIGHT_SLEEP) != 0) {
        LOGE("set AUTO_LIGHT_SLEEP failed");
        net_disable_keep_alive();
        return -1;
    }

    g_low_power_enabled = true;
    LOGI("AUTO_LIGHT_SLEEP enabled (WiFi LISTEN interval=%d, wake GPIOB_%d)",
         WIFI_PS_LISTEN_INTERVAL, WAKEUP_GPIO_PIN);

    return 0;
}

static void exit_low_power(void)
{
    if (!g_low_power_enabled) {
        return;
    }

    lisa_pm_set_system_policy(LISA_PM_SYSTEM_POLICY_ACTIVE);
    net_disable_keep_alive();
    g_low_power_enabled = false;
    LOGI("AUTO_LIGHT_SLEEP disabled, back to ACTIVE");
}

/* PM after_wake 回调：运行在 lisa_pm 内部任务（正常上下文），可安全 reinit。 */
static void app_after_wake(void *user_data, lisa_pm_wakeup_cause_t cause)
{
    (void)user_data;

    LOGI("after_wake cause=%s", wakeup_cause_str(cause));

    if (cause != LISA_PM_WAKEUP_GPIO) {
        return;
    }

    /* 唤醒后切回 ACTIVE，并把 audio 设备恢复到 init 后状态。 */
    exit_low_power();

    lisa_device_t *dev = lisa_device_get(AUDIO_DEVICE_NAME);
    if (dev != NULL) {
        int ret = lisa_device_reinit(dev);
        if (ret == LISA_DEVICE_OK) {
            LOGI("audio0 reinit ok after wake; run `audio_test` to verify");
        } else {
            LOGE("audio0 reinit failed after wake: %d", ret);
        }
    }
}

/* ======================================================================
 * shell 命令
 * ====================================================================== */

/* 建测试线程做录音功能验证 */
static int cmd_audio_test(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (g_audio_test_task != NULL) {
        LOGW("audio test thread already running");
        return 0;
    }

    if (xTaskCreate(audio_test_thread, "audio_test",
                    AUDIO_TEST_TASK_STACK / sizeof(StackType_t), NULL,
                    AUDIO_TEST_TASK_PRIO, &g_audio_test_task) != pdPASS) {
        LOGE("create audio test thread failed");
        g_audio_test_task = NULL;
        return -1;
    }

    LOGI("audio test thread created");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 audio_test, cmd_audio_test, run audio record functional test);

/* 进入低功耗：删测试线程 + destroy(audio0)，再进 AUTO_LIGHT_SLEEP */
static int cmd_low_power(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    /* 先删除使用 audio 的测试线程，再释放设备，避免 use-after-free。 */
    if (g_audio_test_task != NULL) {
        vTaskDelete(g_audio_test_task);
        g_audio_test_task = NULL;
        LOGI("audio test thread deleted");
    }

    lisa_device_t *dev = lisa_device_get(AUDIO_DEVICE_NAME);
    if (dev != NULL) {
        lisa_device_destroy(dev);
        LOGI("audio0 destroyed (resources released, back to power-on state)");
    }

    if (enter_low_power() != 0) {
        LOGE("enter low power failed");
        return -1;
    }

    LOGI("entering low power; press the wake key (GPIOB_%d) to wake", WAKEUP_GPIO_PIN);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 low_power, cmd_low_power, destroy audio0 and enter AUTO_LIGHT_SLEEP);

/* ======================================================================
 * CP 上下文存活校验（沿用 gpio_wakeup 示例）
 * ====================================================================== */

static uint32_t cp_checksum(uint32_t counter)
{
    return CP_CONTEXT_SEED ^ (counter * 2246822519UL) ^ (counter << 5);
}

static void cp_context_init(void)
{
    g_cp_magic = CP_CONTEXT_MAGIC;
    g_cp_counter = 0;
    g_cp_checksum = cp_checksum(0);
    g_cp_failed = 0;
}

static void cp_context_step(void)
{
    uint32_t counter = g_cp_counter;
    uint32_t expected = cp_checksum(counter);

    if ((g_cp_magic != CP_CONTEXT_MAGIC) || (g_cp_checksum != expected)) {
        g_cp_failed = 1;
        LOGE("CP CTX CHECK FAILED: magic=0x%08lx counter=%lu checksum=0x%08lx expected=0x%08lx",
             (unsigned long)g_cp_magic,
             (unsigned long)counter,
             (unsigned long)g_cp_checksum,
             (unsigned long)expected);
        return;
    }

    counter++;
    g_cp_counter = counter;
    g_cp_checksum = cp_checksum(counter);
    LOGI("CP ctx alive: counter=%lu wifi=%d ip=%d lp=%d wake=%s checksum=0x%08lx",
         (unsigned long)counter,
         (int)g_wifi_connected,
         (int)g_ip_ready,
         (int)g_low_power_enabled,
         wakeup_cause_str(lisa_pm_get_wakeup_cause()),
         (unsigned long)g_cp_checksum);
}

/* ======================================================================
 * WiFi 接入（沿用 gpio_wakeup 示例，去掉自动进入低功耗）
 * ====================================================================== */

static int8_t custom_get_wifi_mac(uint8_t mac[6])
{
    int8_t ret;

    if ((mac == NULL) || (g_mac_manager == NULL)) {
        return -1;
    }

    ret = mac_manager_get(g_mac_manager, mac, 6);
    LOGI("custom MAC ret=%d %02X:%02X:%02X:%02X:%02X:%02X",
         ret, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return ret;
}

static void dhcp_status_callback(int vif, bool success, uint32_t ip,
                                 uint32_t mask, uint32_t gw, void *arg)
{
    (void)mask;
    (void)gw;
    (void)arg;

    if (success) {
        g_ip_ready = true;
        LOGI("DHCP success on VIF-%d: %d.%d.%d.%d",
             vif,
             ip & 0xff,
             (ip >> 8) & 0xff,
             (ip >> 16) & 0xff,
             (ip >> 24) & 0xff);
    } else {
        g_ip_ready = false;
        exit_low_power();
        LOGI("DHCP failed on VIF-%d", vif);
    }
}

static int app_wifi_event_handler(void *arg, event_module_t mod, int id, void *data)
{
    net_if_t *nif;

    (void)arg;
    (void)mod;
    (void)data;

    switch (id) {
    case EVENT_WIFI_CONNECTED:
        g_wifi_connected = true;
        LOGI("WiFi connected");
        nif = net_if_get(WIFI_VIF_STA_IDX);
        net_if_up(nif);
        if (!nif->static_ip) {
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        } else {
            g_ip_ready = true;
        }
        break;

    case EVENT_WIFI_DISCONNECT:
    case EVENT_WIFI_STA_CONNECT_FAIL:
        LOGI("WiFi disconnected or connect failed: event=%d", id);
        exit_low_power();
        g_wifi_connected = false;
        g_ip_ready = false;
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        net_if_down(net_if_get(WIFI_VIF_STA_IDX));
        break;

    default:
        break;
    }

    return 0;
}

static void connect_target_ap(void)
{
    wifi_connect_cfg_t conn = {0};

    strncpy((char *)conn.ssid, TARGET_WIFI_SSID, sizeof(conn.ssid) - 1);
    strncpy((char *)conn.key, TARGET_WIFI_PWD, sizeof(conn.key) - 1);
    conn.dhcp_mode = DHCP_CLIENT;
    conn.sec = WIFI_SEC_AUTO;

    if (wifi_sta_connect(&conn) != 0) {
        LOGE("wifi_sta_connect failed");
        return;
    }

    LOGI("WiFi connecting to %s", TARGET_WIFI_SSID);
}

static void cb_lisa_wifi_init_done(void)
{
    lisa_pm_wifi_ps_config_t cfg = {
        .listen_interval = WIFI_PS_LISTEN_INTERVAL,
    };

    LOGI("lisa_wifi init done");
    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, app_wifi_event_handler, NULL);
    wifi_sta_mode_enable();
    wifi_sta_auto_reconnect_enable();

    /* LISTEN 模式 listen_interval 必须在 connect 前下发，否则不生效。 */
    if (lisa_pm_wifi_set_ps_mode(LISA_PM_WIFI_PS_LISTEN, &cfg) != 0) {
        LOGE("set WiFi LISTEN power save failed");
    }

    connect_target_ap();
}

static int wait_for_wifi_ready(void)
{
    int timeout = WIFI_WAIT_TIMEOUT_S;

    LOGI("Waiting for WiFi + IP...");
    while (timeout > 0) {
        if (g_wifi_connected && g_ip_ready) {
            LOGI("WiFi connected and IP obtained");
            return 0;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
        timeout--;
        if ((timeout % 5) == 0) {
            LOGI("Waiting... wifi=%d ip=%d timeout=%d",
                 (int)g_wifi_connected, (int)g_ip_ready, timeout);
        }
    }

    LOGE("WiFi/IP wait timeout");
    return -1;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    LOGI("=== lisa_pm dual_core audio_device_wakeup CP ===");

    int ret = lisa_shell_init();
    if (ret != 0) {
        LOGE("Failed to initialize shell (error: %d)\n", ret);
        return ret;
    }

    if (lisa_pm_init() != 0) {
        LOGE("lisa_pm_init failed");
        return -1;
    }

    /* 1) 默认保持 ACTIVE，不自动进入轻睡。 */
    lisa_pm_set_system_policy(LISA_PM_SYSTEM_POLICY_ACTIVE);

    cp_context_init();

    if (setup_gpiob_wakeup_source() != 0) {
        return -1;
    }

    /* 注册 after_wake 回调，唤醒后切回 ACTIVE 并 reinit audio。 */
    lisa_pm_sleep_callback_t pm_cb = {
        .before_sleep = NULL,
        .after_wake   = app_after_wake,
    };
    if (lisa_pm_sleep_callback_register(&pm_cb) != 0) {
        LOGE("register PM sleep callback failed");
        return -1;
    }

    mac_manager_config_t mcfg = {
        .random_mac_if_mac_invalid = false,
    };
    g_mac_manager = mac_manager_init(mac_manager_ops_get()->mem_ops,
                                     mac_manager_ops_get()->content_ops,
                                     &mcfg);
    if (g_mac_manager == NULL) {
        LOGE("mac_manager_init failed");
        return -1;
    }

    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    lisa_wifi_ops_t ops = {
        .init_done = cb_lisa_wifi_init_done,
        .custom_mac = custom_get_wifi_mac,
    };
    if (lisa_wifi_init(&ops) != 0) {
        LOGE("lisa_wifi_init failed");
        return -1;
    }

    if (wait_for_wifi_ready() == 0) {
        LOGI("Ready. policy=ACTIVE. Run `audio_test`, then `low_power` to sleep.");
    } else {
        LOGW("WiFi not ready; you can still run `audio_test` over shell");
    }

    while (1) {
        cp_context_step();
        vTaskDelay(pdMS_TO_TICKS(CP_CONTEXT_LOG_MS));
    }

    return 0;
}
