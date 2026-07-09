#include "stdint.h"

#define TAG "voice.app.platform"

#include "voice_msg.h"
#include "lisa_log.h"
#include "sys_init.h"
#include "sys_network_manager.h"
#include "voice_cloud.h"
#ifdef CONFIG_OTA
#include "ota_manager.h"
#ifdef CONFIG_BOARD_ARCS_MINI
#include "lisa_kv.h"
#include "kv_user.h"
#include "ota_flash.h"
#include "app_tone.h"
#endif
#endif

#include "app_datas.h"

#ifdef CONFIG_OTA
static bool s_ota_checked_once = false;
extern int network_probe_start(void);

static bool voice_ota_is_active(void)
{
    ota_state_e state = ota_manager_get_state();

    return state == OTA_STATE_CHECKING ||
           state == OTA_STATE_PACKAGE_INFO ||
           state == OTA_STATE_UPDATING;
}
#endif

static void voice_platform_ready(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("network probe ready");

    

    // ls_wifi_mgr_init();
}

static void do_voice_cloud_connect(void)
{
    struct app_datas *app_datas = get_app_datas();
    voice_cloud_state_t cloud_state = voice_cloud_get_state();
    assert(app_datas != NULL);

    if (cloud_state == VOICE_CLOUD_STATE_CONNECTED || cloud_state == VOICE_CLOUD_STATE_CONNECTING) {
        return;
    }

    struct voice_cloud_connect_config connect_config = {
        .pid = app_datas->pid,
        .sid = app_datas->sid,
        .did = app_datas->did,
        .auto_reconn = true,
        .reconn_interval_ms = 3000,
        .device_mode = app_datas->device_mode,
        .host = app_datas->host[0] == 0 ? NULL : app_datas->host,
        .host_staging = app_datas->host_staging[0] == 0 ? NULL : app_datas->host_staging,
        .host_integration = app_datas->host_integration[0] == 0 ? NULL : app_datas->host_integration,
        .token_url = app_datas->token_url[0] == 0 ? NULL : app_datas->token_url,
        .token_url_staging = app_datas->token_url_staging[0] == 0 ? NULL : app_datas->token_url_staging,
        .token_url_integration = app_datas->token_url_integration[0] == 0 ? NULL : app_datas->token_url_integration,
        .music_active_url = app_datas->music_active_url[0] == 0 ? NULL : app_datas->music_active_url,
        .music_tranlink_url = app_datas->music_tranlink_url[0] == 0 ? NULL : app_datas->music_tranlink_url,
        .port = app_datas->port[0] == 0 ? NULL : app_datas->port,
        .scheme = app_datas->scheme[0] == 0 ? NULL : app_datas->scheme,
    };

    voice_cloud_connect(&connect_config);
}

static void voice_ble_auth_info_done(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    sys_network_status_t status;
    voice_cloud_state_t cloud_state = voice_cloud_get_state();

    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    if (sys_network_get_status(&status) != 0 ||
        status.active_bearer == SYS_NETWORK_BEARER_NONE ||
        !status.connected) {
        LOGI("skip cloud reconnect after BLE auth info: network unavailable");
        return;
    }

    if (cloud_state != VOICE_CLOUD_STATE_CONNECT_FAILED) {
        LOGI("skip cloud reconnect after BLE auth info: cloud_state=%d", cloud_state);
        return;
    }

    LOGI("BLE auth info sent, reconnect cloud to refresh bind state");
    if (voice_cloud_disconnect() != 0) {
        LOGW("voice cloud disconnect before bind-state refresh failed");
    }

    do_voice_cloud_connect();
}

static void voice_system_network_probe_success(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    struct app_datas *app_datas = get_app_datas();
    sys_network_status_t status;
    assert(app_datas != NULL);

    if (sys_network_get_status(&status) != 0 || status.active_bearer == SYS_NETWORK_BEARER_NONE) {
        LOGI("ignore stale network probe success");
        return;
    }

    LOGI("network probe success");
#ifdef CONFIG_OTA
    if (!s_ota_checked_once) {
        s_ota_checked_once = true;
        ota_manager_check_all();
        return;
    }

    if (voice_ota_is_active()) {
        LOGI("OTA check in progress, skip cloud reconnect");
        return;
    }
#endif
    do_voice_cloud_connect();
}

static void voice_system_network_probe_fail(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    struct app_datas *app_datas = get_app_datas();
    sys_network_status_t status;
    assert(app_datas != NULL);

    if (sys_network_get_status(&status) != 0 || status.active_bearer == SYS_NETWORK_BEARER_NONE) {
        LOGI("ignore stale network probe fail");
        return;
    }

    LOGI("network probe fail");
}

#ifdef CONFIG_OTA
static void voice_ota_up_to_date(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    do_voice_cloud_connect();
}

static void voice_ota_failed(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (data == NULL || len < sizeof(ota_state_t)) {
        return;
    }

    ota_state_t *state = (ota_state_t *)data;
    if (state->state != OTA_STATE_APP_FAILED) {
        return;
    }

    LOGI("App OTA failed, restart network probe before cloud business");
    network_probe_start();
}

static void voice_power_battery_update(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)user_data;

    if (data == NULL || len < sizeof(voice_msg_battery_info_t)) {
        return;
    }

    voice_msg_battery_info_t *info = (voice_msg_battery_info_t *)data;
    if (info->status == VOICE_MSG_BATTERY_STATUS_CHARGING ||
        info->status == VOICE_MSG_BATTERY_STATUS_CHARGE_DONE) {
        ota_manager_check_after_power_connected();
    }
}

static int voice_ota_resources_updated(bool wake_word_updated, bool prompt_tone_updated, bool emoji_updated, void *user_data)
{
    (void)wake_word_updated;
    (void)emoji_updated;
    (void)user_data;

    if (!prompt_tone_updated) {
        return 0;
    }

    int disable_tone_update = 0;
    if (lisa_kv_get_int(KV_KEY_USER_DISABLE_TONE_UPDATE, &disable_tone_update) == 0 && disable_tone_update != 0) {
        return 0;
    }

    const void *tone_addr = NULL;
    uint32_t tone_size = 0;
    int ret = ota_flash_get(OTA_PART_PROMPT_TONE_BIN, &tone_addr, &tone_size);
    if (ret < 0) {
        LOGE("get prompt_tone partition failed: %d", ret);
        return ret;
    }

    ret = app_tone_init((uint32_t)tone_addr, tone_size);
    if (ret < 0) {
        LOGE("reload prompt_tone failed: %d", ret);
        return ret;
    }

    LOGI("prompt_tone runtime index refreshed before OTA success tone");
    return 0;
}
#endif

int voice_platform_evt_init(void)
{
    LOGI("voice_platform_evt_init");

    voice_msg_sub(VOICE_MSG_PLATFORM_READY, voice_platform_ready, NULL);
    voice_msg_sub(VOICE_MSG_BLE_AUTH_INFO_DONE, voice_ble_auth_info_done, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS, voice_system_network_probe_success, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_FAIL, voice_system_network_probe_fail, NULL);
#ifdef CONFIG_OTA
    voice_msg_sub(VOICE_MSG_OTA_UP_TO_DATE, voice_ota_up_to_date, NULL);
    voice_msg_sub(VOICE_MSG_OTA_FAILED, voice_ota_failed, NULL);
    voice_msg_sub(VOICE_MSG_POWER_BATTERY_UPDATE, voice_power_battery_update, NULL);
#ifdef CONFIG_BOARD_ARCS_MINI
    ota_manager_register_resources_updated_cb(voice_ota_resources_updated, NULL);
#endif
#endif

    return 0;
}

SYS_INIT(voice_platform_evt_init, SYS_INIT_LEVEL_PRE_APPLICATION, 50);
