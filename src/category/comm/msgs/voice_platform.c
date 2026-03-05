#include "stdint.h"

#define TAG "voice.app.platform"

#include "voice_msg.h"
#include "lisa_log.h"
#include "sys_init.h"
#include "voice_cloud.h"
#ifdef CONFIG_OTA
#include "ota_manager.h"
#endif

#include "app_datas.h"

static void voice_platform_ready(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("network probe ready");

    app_datas_init();

    ls_wifi_mgr_init();
}

static void do_voice_cloud_connect(void)
{
    struct app_datas *app_datas = get_app_datas();
    assert(app_datas != NULL);
    app_datas->network_connected = 1;

    if (!app_datas->voice_cloud_connected) {
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
}

static void voice_system_network_probe_success(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("network probe success");
#ifdef CONFIG_OTA
    ota_manager_check_all();
#else
    do_voice_cloud_connect();
#endif
}

static void voice_system_network_probe_fail(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    LOGI("network probe fail");
    struct app_datas *app_datas = get_app_datas();
    assert(app_datas != NULL);

    app_datas->network_connected = 0;
}

#ifdef CONFIG_OTA
static void voice_ota_up_to_date(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    do_voice_cloud_connect();
}
#endif

int voice_platform_evt_init(void)
{
    LOGI("voice_platform_evt_init");

    voice_msg_sub(VOICE_MSG_PLATFORM_READY, voice_platform_ready, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS, voice_system_network_probe_success, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_FAIL, voice_system_network_probe_fail, NULL);
#ifdef CONFIG_OTA
    voice_msg_sub(VOICE_MSG_OTA_UP_TO_DATE, voice_ota_up_to_date, NULL);
#endif

    return 0;
}

SYS_INIT(voice_platform_evt_init, SYS_INIT_LEVEL_PRE_APPLICATION, 50);
