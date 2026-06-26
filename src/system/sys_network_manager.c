#include <stdbool.h>
#include <string.h>

#include "lisa_kv.h"
#include "lisa_semaphore.h"
#include "lisa_thread.h"
#include "voice_msg.h"
#include "voice_cloud.h"

#include "app_datas.h"
#include "kv_user.h"
#include "sys_network_manager.h"
#include "sys_wifi.h"

#if CONFIG_LISA_MODEM
#include "lisa_modem_module.h"
#include "pinmux.h"
#endif

#define TAG "sys.net.mgr"
#include "lisa_log.h"

#define MODEM_UART_DEVICE "uart2"

int network_probe_start(void);

typedef struct {
    bool inited;
    bool wifi_available;
    bool switching;
    bool modem_started;
    bool network_connected;
    sys_network_mode_t current_mode;
    sys_network_mode_t pending_mode;
    bool pending_persist;
    bool has_pending_request;
    sys_network_bearer_t active_bearer;
    lisa_semaphore_t *sem;
} sys_network_manager_ctx_t;

static sys_network_manager_ctx_t s_mgr;

static bool sys_network_mode_is_valid(sys_network_mode_t mode)
{
    return mode == SYS_NETWORK_MODE_WIFI ||
           mode == SYS_NETWORK_MODE_MODEM;
}

static void sys_network_sync_app_data(void)
{
    struct app_datas *app_datas = get_app_datas();

    if (app_datas == NULL) {
        return;
    }

    app_datas->network_mode = (uint8_t)s_mgr.current_mode;
    app_datas->active_bearer = (uint8_t)s_mgr.active_bearer;
    app_datas->network_connected = s_mgr.network_connected ? 1U : 0U;
    app_datas->network_switching = s_mgr.switching ? 1U : 0U;
    app_datas->modem_connected = s_mgr.modem_started ? 1U : 0U;
}

/* 唯一的 bearer 状态变更入口：更新 s_mgr + 回写 app_datas + (可选)发消息 + (上线时)触发 probe */
static void sys_network_set_bearer(sys_network_bearer_t bearer, uint32_t msg)
{
    s_mgr.active_bearer = bearer;
    s_mgr.network_connected = false;
    sys_network_sync_app_data();

    if (msg != 0) {
        voice_msg_pub(msg, NULL, 0);
    }

    if (bearer != SYS_NETWORK_BEARER_NONE) {
        network_probe_start();
    }
}

static int sys_network_start_modem(void)
{
#if CONFIG_LISA_MODEM
    if (s_mgr.modem_started) {
        return 0;
    }

    LISA_LOGI(TAG, "Starting modem");
    if (!lisa_modem_module_init(MODEM_UART_DEVICE)) {
        LISA_LOGW(TAG, "modem is not ready");
        return -1;
    }

    s_mgr.modem_started = true;
    return 0;
#else
    LISA_LOGW(TAG, "modem support is disabled");
    return -1;
#endif
}

static void sys_network_stop_modem(void)
{
    if (!s_mgr.modem_started) {
        return;
    }

#if CONFIG_LISA_MODEM
    if (!lisa_modem_module_deinit()) {
        LISA_LOGW(TAG, "modem deinit reported failure");
    }
#endif

    s_mgr.modem_started = false;
    sys_network_sync_app_data();
}

static bool sys_network_start_bearer(sys_network_bearer_t bearer)
{
    if (bearer == SYS_NETWORK_BEARER_WIFI) {
        if (!s_mgr.wifi_available) {
            LISA_LOGW(TAG, "WiFi bearer is unavailable");
            return false;
        }

        LISA_LOGI(TAG, "Starting WiFi bearer");
        if (sys_wifi_start(true) != 0) {
            LISA_LOGW(TAG, "Failed to start WiFi bearer");
            return false;
        }

        /* WiFi 连接异步，已经有连接就直接发布，否则等 WIFI_IP_GOT */
        if (sys_wifi_is_connected()) {
            sys_network_set_bearer(SYS_NETWORK_BEARER_WIFI, VOICE_MSG_SYSTEM_NETWORK_CONNECTED);
        }
        return true;
    }

    if (bearer == SYS_NETWORK_BEARER_MODEM) {
        if (sys_network_start_modem() != 0) {
            return false;
        }
        sys_network_set_bearer(SYS_NETWORK_BEARER_MODEM, VOICE_MSG_SYSTEM_NETWORK_CONNECTED);
        return true;
    }

    return false;
}

static void sys_network_stop_bearer(sys_network_bearer_t bearer)
{
    if (s_mgr.active_bearer == bearer) {
        sys_network_set_bearer(SYS_NETWORK_BEARER_NONE, 0);
    }

    if (bearer == SYS_NETWORK_BEARER_WIFI) {
        sys_wifi_stop();
    } else if (bearer == SYS_NETWORK_BEARER_MODEM) {
        sys_network_stop_modem();
    }
}

static void sys_network_apply_mode(sys_network_mode_t mode, bool persist)
{
    bool success;

    s_mgr.current_mode = mode;
    sys_network_sync_app_data();

    if (persist && lisa_kv_set_int(KV_KEY_NETWORK_MODE, (int)mode) != 0) {
        LISA_LOGW(TAG, "Failed to persist network mode: %d", mode);
    }

    s_mgr.switching = true;
    sys_network_sync_app_data();
    voice_msg_pub(VOICE_MSG_SYSTEM_NETWORK_SWITCHING, NULL, 0);
    voice_cloud_disconnect();

    if (mode == SYS_NETWORK_MODE_WIFI) {
        /* 严格 WiFi：不起 modem，也不回落 */
        sys_network_stop_bearer(SYS_NETWORK_BEARER_MODEM);
        success = sys_network_start_bearer(SYS_NETWORK_BEARER_WIFI);
    } else {
        /* 严格 MODEM：不起 WiFi，也不回落 */
        sys_network_stop_bearer(SYS_NETWORK_BEARER_WIFI);
#if CONFIG_LISA_MODEM
        lisa_uart2_pinmux();
#endif
        success = sys_network_start_bearer(SYS_NETWORK_BEARER_MODEM);
        if (!success) {
            LISA_LOGW(TAG, "modem unavailable");
        }
    }

    s_mgr.switching = false;
    sys_network_sync_app_data();
    voice_msg_pub(success ? VOICE_MSG_SYSTEM_NETWORK_SWITCH_DONE
                          : VOICE_MSG_SYSTEM_NETWORK_SWITCH_FAIL,
                  NULL, 0);
}

static void sys_network_worker(void *arg)
{
    (void)arg;

    while (1) {
        sys_network_mode_t mode;
        bool persist;

        lisa_semaphore_take(s_mgr.sem, LISA_OS_WAIT_FOREVER);

        if (!s_mgr.has_pending_request) {
            continue;
        }

        mode = s_mgr.pending_mode;
        persist = s_mgr.pending_persist;
        s_mgr.has_pending_request = false;

        sys_network_apply_mode(mode, persist);
    }
}

static void sys_network_wifi_ip_got(void *unused, uint32_t evt, void *data, uint32_t len, void *user_data)
{
    (void)unused; (void)evt; (void)data; (void)len; (void)user_data;

    /* modem 已接管就忽略 WiFi 拿到 IP 的事件 */
    if (s_mgr.modem_started) {
        return;
    }

    sys_network_set_bearer(SYS_NETWORK_BEARER_WIFI, VOICE_MSG_SYSTEM_NETWORK_CONNECTED);
}

static void sys_network_wifi_disconnected(void *unused, uint32_t evt, void *data, uint32_t len, void *user_data)
{
    (void)unused; (void)evt; (void)data; (void)len; (void)user_data;

    if (s_mgr.active_bearer != SYS_NETWORK_BEARER_WIFI) {
        return;
    }

    sys_network_set_bearer(SYS_NETWORK_BEARER_NONE,
                           s_mgr.switching ? 0 : VOICE_MSG_SYSTEM_NETWORK_DISCONNECTED);
}

int sys_network_manager_init(bool wifi_available)
{
    lisa_thread_attr_t attr = {
        .name = "sys.net.mgr",
        .stack_size = 3072,
        .priority = LISA_OS_PRIORITY_NORMAL,
    };

    if (s_mgr.inited) {
        s_mgr.wifi_available = wifi_available;
        return 0;
    }

    memset(&s_mgr, 0, sizeof(s_mgr));
    s_mgr.wifi_available = wifi_available;
    s_mgr.current_mode = SYS_NETWORK_MODE_WIFI;

    struct app_datas *app_datas = get_app_datas();
    if (app_datas != NULL && sys_network_mode_is_valid((sys_network_mode_t)app_datas->network_mode)) {
        s_mgr.current_mode = (sys_network_mode_t)app_datas->network_mode;
    }

    s_mgr.sem = lisa_semaphore_create(1);
    if (s_mgr.sem == NULL) {
        return -1;
    }

    if (lisa_thread_create(&attr, sys_network_worker, NULL) == NULL) {
        return -1;
    }

    voice_msg_sub(VOICE_MSG_WIFI_IP_GOT, sys_network_wifi_ip_got, NULL);
    voice_msg_sub(VOICE_MSG_WIFI_DISCONNECTED, sys_network_wifi_disconnected, NULL);

    s_mgr.inited = true;
    sys_network_sync_app_data();
    return sys_network_request_mode(s_mgr.current_mode, false);
}

int sys_network_request_mode(sys_network_mode_t mode, bool persist)
{
    if (!s_mgr.inited || !sys_network_mode_is_valid(mode)) {
        return -1;
    }

    s_mgr.pending_mode = mode;
    s_mgr.pending_persist = persist;
    s_mgr.has_pending_request = true;
    lisa_semaphore_give(s_mgr.sem);
    return 0;
}

int sys_network_connect_wifi(const char *ssid, const char *pwd, const char *bssid)
{
    if (!s_mgr.inited || !s_mgr.wifi_available) {
        return -1;
    }

    /* 当前跑在 modem 上：只存凭据不切 bearer，等用户显式切回 WiFi */
    if (s_mgr.active_bearer == SYS_NETWORK_BEARER_MODEM) {
        LISA_LOGI(TAG, "modem bearer is active, save WiFi credentials only");
        return sys_wifi_save_ap(ssid, pwd, bssid);
    }

    if (!sys_wifi_is_started() && sys_wifi_start(false) != 0) {
        LISA_LOGW(TAG, "Failed to start WiFi before connect");
        return -1;
    }

    return sys_wifi_connect(ssid, pwd, bssid);
}

int sys_network_toggle_mode(bool persist)
{
    if (!s_mgr.inited) {
        return -1;
    }

    sys_network_mode_t next = (s_mgr.current_mode == SYS_NETWORK_MODE_WIFI)
                                  ? SYS_NETWORK_MODE_MODEM
                                  : SYS_NETWORK_MODE_WIFI;
    return sys_network_request_mode(next, persist);
}

void sys_network_report_probe_result(bool connected)
{
    if (!s_mgr.inited) {
        return;
    }

    if (s_mgr.active_bearer == SYS_NETWORK_BEARER_NONE) {
        s_mgr.network_connected = false;
        return;
    }

    s_mgr.network_connected = connected;
    sys_network_sync_app_data();
}

int sys_network_get_status(sys_network_status_t *status)
{
    if (status == NULL) {
        return -1;
    }

    status->mode = s_mgr.current_mode;
    status->active_bearer = s_mgr.active_bearer;
    status->connected = s_mgr.network_connected;
    status->switching = s_mgr.switching;
    status->wifi_available = s_mgr.wifi_available;
    status->wifi_connected = s_mgr.wifi_available && sys_wifi_is_connected();
    status->modem_connected = s_mgr.modem_started;
    return 0;
}

bool sys_network_get_signal_quality(int *rssi, int *ber)
{
    switch (s_mgr.active_bearer) {
    case SYS_NETWORK_BEARER_WIFI:
        if (ber != NULL) {
            *ber = 99;
        }
        return sys_wifi_get_signal_quality(rssi);

    case SYS_NETWORK_BEARER_MODEM:
#if CONFIG_LISA_MODEM
        return lisa_modem_get_signal_quality(rssi, ber);
#else
        return false;
#endif

    case SYS_NETWORK_BEARER_NONE:
    default:
        return false;
    }
}

