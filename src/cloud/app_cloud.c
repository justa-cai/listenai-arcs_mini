#define TAG "cloud"

#include <stdint.h>
#include <string.h>
#include "app_cloud.h"
#include "recognizer.h"
#include "lisa_aiui.h"
#include "app_client.h"
#include "lisa_log.h"
#include "aiui_cfg.h"
#include "evs_utils.h"
#include "tone.h"
#include "lisa_mem.h"
#include "proc_mgr.h"
#include "sound_player.h"
#include "assistant_controller.h"
#include "haoxueduo.h"
#include "mcp_integration.h"

#include "lisa_kv.h"
#include "kv.h"
#define NDEBUG
#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"

#define PREF_KEY_AIUI_APPID "aiui_appid"
#define PREF_KEY_AIUI_APPKEY "aiui_appkey"

static void _ws_conn_cb();
static void _ws_disconnect_cb();
static void _ws_ms_cb(const char *msg, int len);
static int _wifi_conn_runnable(void *arg);
static int _wifi_disconn_runnable(void *arg);

static lisa_aiui_cb_t s_aiui_cb = {
		.aiui_ws_connected_cb = _ws_conn_cb,
		.aiui_ws_disconnect_cb = _ws_disconnect_cb,
		.aiui_ws_onmessage_cb = _ws_ms_cb,
};

static app_cloud_t *s_cloud = NULL;

const app_cloud_t *app_cloud_get()
{
	return s_cloud;
}

app_cloud_t *app_cloud_create(app_client_t *client)
{
	app_cloud_t *handle = (app_cloud_t *)lisa_mem_calloc(1, sizeof(app_cloud_t));
	if (!handle) {
		LISA_LOGE(TAG, "app_cloud_create failed");
		return NULL;
	}

	int r;
	char *appid = NULL;
	char *appkey = NULL;
	bool is_kv_appid = true;
	bool is_kv_appkey = true;

	r = lisa_kv_get_string(KV_KEY_APPID, &appid);
	if (r || appid == NULL) {
		LISA_LOGW(TAG, "get appid from kv failed, use default appid");
		appid = AIUI_APPID;
		is_kv_appid = false;
	}

	r = lisa_kv_get_string(KV_KEY_APPKEY, &appkey);
	if (r || appkey == NULL) {
		LISA_LOGW(TAG, "get appkey from kv failed, use default appkey");
		appkey = AIUI_API_KEY;
		is_kv_appkey = false;
	}

	LISA_LOGI(TAG, "appid: %s", appid);
	LISA_LOGI(TAG, "appkey: %s", appkey);

	lisa_aiui_config_t config;
	config.api_key = appkey;
	config.appid = appid;
	handle->aiui = lisa_aiui_create(&config, &s_aiui_cb);
	handle->m_client = client;
	handle->m_rec = recognizer_create(
			client->short_player, client->tts_player, client->audio_mgr, handle->aiui);
	app_proc_init(client, handle);
	s_cloud = handle;

	if (is_kv_appid) {
		lisa_kv_free(appid);
	}
	if (is_kv_appkey) {
		lisa_kv_free(appkey);
	}

	// 使用默认配置
	mcp_integration_init(NULL);

	return s_cloud;
}

void app_cloud_process_wifi_connected(app_cloud_t *cloud)
{
	if (cloud == NULL) {
		return;
	}
	cloud->m_wifi_conn = true;
	LISA_LOGI(TAG, "wifi connected, try to connect to cloud");
	evs_handler_post_runnable(_wifi_conn_runnable, NULL);
}

void app_cloud_process_wifi_disconnected(app_cloud_t *cloud)
{
	if (cloud == NULL) {
		return;
	}
	cloud->m_wifi_conn = false;
	evs_handler_post_runnable(_wifi_disconn_runnable, NULL);
}

void app_cloud_txt(const char *txt)
{
	if (txt == NULL) {
		LISA_LOGE(TAG, "listen_client_tts text is null");
		return;
	}

	if (s_cloud->ws_state != LS_WS_CONNECT) {
		LISA_LOGE(
				TAG, "listen_client_tts web socket is not connected, state:%d", s_cloud->ws_state);
		return;
	}

	int err = lisa_aiui_send_txt(s_cloud->aiui, txt);
	if (err) {
		LISA_LOGE(TAG, "listen_client_tts, err:%d", err);
	}
}

void app_cloud_tts(const char *text)
{
	if (text == NULL) {
		LISA_LOGE(TAG, "listen_client_tts text is null");
		return;
	}

	if (s_cloud->ws_state != LS_WS_CONNECT) {
		LISA_LOGE(
				TAG, "listen_client_tts web socket is not connected, state:%d", s_cloud->ws_state);
		return;
	}

	int err = lisa_aiui_tts(s_cloud->aiui, text);
	if (err) {
		LISA_LOGE(TAG, "listen_client_tts, err:%d", err);
	}
}

bool app_cloud_is_connected()
{
	if (s_cloud == NULL) {
		return false;
	}
	return (s_cloud->ws_state == LS_WS_CONNECT);
}

void app_cloud_audio(app_cloud_t *cloud, const char *audio, uint32_t len)
{
	if (cloud == NULL || cloud->ws_state != LS_WS_CONNECT) {
		return;
	}
	recognizer_write_audio(cloud->m_rec, audio, len);
}

void app_cloud_wakeup(app_cloud_t *cloud)
{
	if (cloud == NULL) {
		return;
	}
	recognizer_recognize(cloud->m_rec);
	assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_RECORD_START, NULL, 0);
}

void app_cloud_ntp_ok(void *arg)
{
	if(s_cloud){
		s_cloud->m_ntp_conn = true;
	}
}

void app_cloud_connect()
{
	s_cloud->ws_state = LS_WS_CONNECT;
	s_cloud->m_fast_reconnect = false;
}

void app_cloud_disconnect()
{
	s_cloud->ws_state = LS_WS_DISCONNECT;
	lisa_aiui_disconnect(s_cloud->aiui);
}

void app_cloud_token_error()
{
	s_cloud->m_fast_reconnect = true;
}

static int _wifi_conn_runnable(void *arg)
{
	LISA_LOGI(TAG, "cloud connect, curr state: %d", s_cloud->ws_state);
	listen_soundplayer_play(s_cloud->m_client->sound_player, TONE_ID_59, 0);
	if (s_cloud->ws_state == LS_WS_DISCONNECT) {
		lisa_aiui_connect(s_cloud->aiui, true);
		s_cloud->ws_state = LS_WS_CONNECTING;
	}

	return 0;
}

static int _wifi_disconn_runnable(void *arg)
{
	listen_soundplayer_play(s_cloud->m_client->sound_player, TONE_ID_60, 0);
	if (s_cloud->ws_state != LS_WS_DISCONNECT) {
		lisa_aiui_disconnect(s_cloud->aiui);
		s_cloud->ws_state = LS_WS_DISCONNECT;
	}
	return 0;
}

void app_token_fresh(bool re_fresh)
{
    s_cloud->m_fast_reconnect = re_fresh;
}

static int _ws_reconnect(void *arg)
{
	LISA_LOGI(TAG, "_ws_reconnect, ws_state: %d, ntp: %d", s_cloud->ws_state, s_cloud->m_ntp_conn);

	if (s_cloud->ws_state != LS_WS_DISCONNECT) {
		return 0;
	}

	// 重连前需要主动调用断开链接
	s_cloud->ws_state = LS_WS_DISCONNECT;
	lisa_aiui_disconnect(s_cloud->aiui);
	if (LISA_OK != lisa_aiui_connect(s_cloud->aiui, true)) {
		evs_handler_post_runnable_delay(_ws_reconnect, NULL, 1000);
	} else {
		s_cloud->ws_state = LS_WS_CONNECTING;
	}
	return 0;
}

static void _ws_conn_cb()
{
	s_cloud->ws_state = LS_WS_CONNECT;
}

static void _ws_disconnect_cb()
{
	s_cloud->ws_state = LS_WS_DISCONNECT;
	LISA_LOGI(TAG, "websocket disconnect, reconnect after 1000ms");
	assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PRE_IDLE, NULL, 0);
	if (s_cloud->m_wifi_conn) {
		evs_handler_post_runnable_delay(_ws_reconnect, NULL, 1000);
	}
}

static void _ws_ms_cb(const char *msg, int len)
{
	LISA_LOGI(TAG, "ws message, len: %d, msg: %s", len, msg);
	app_proc_msg(msg, len);
}

struct jpeg_write_context {
	uint8_t *buf;
	uint32_t total_size;
	uint32_t wrote;
};

static void rgb565_to_rgb888_le(uint16_t *src, uint8_t *dst, size_t pixel_count)
{
	for (size_t i = 0; i < pixel_count; i++) {
		uint16_t pixel = src[i];
		uint8_t r = (pixel >> 11) & 0x1F;
		uint8_t g = (pixel >> 5) & 0x3F;
		uint8_t b = pixel & 0x1F;

		dst[i * 3 + 0] = (r * 255) / 31;
		dst[i * 3 + 1] = (g * 255) / 63;
		dst[i * 3 + 2] = (b * 255) / 31;
	}
}

static void jpeg_write(void *context, void *data, int size)
{
	struct jpeg_write_context *ctx = (struct jpeg_write_context *)context;
	if (ctx == NULL) {
		return;
	}

	uint32_t can_write = ctx->total_size - ctx->wrote;
	can_write = can_write >= size ? size : can_write;

	memcpy(ctx->buf + ctx->wrote, data, can_write);

	ctx->wrote += can_write;
}

static int rgb565_datas_to_jpg(uint16_t *rgb565_datas, uint32_t width, uint32_t height, uint8_t **jpg_buf,
                        uint32_t *jpg_buf_size)
{
    uint8_t *rgb888_buf;
    uint32_t rgb888_buf_size;
    uint8_t *jpeg_buf;
    uint32_t jpeg_buf_size;
    int err;

    *jpg_buf = NULL;
    *jpg_buf_size = 0;

    rgb888_buf_size = width * height * 3;
    jpeg_buf_size = rgb888_buf_size / 5;

    rgb888_buf = lisa_mem_alloc(rgb888_buf_size);
    jpeg_buf = lisa_mem_alloc(jpeg_buf_size);

    rgb565_to_rgb888_le(rgb565_datas, rgb888_buf, width * height);

    struct jpeg_write_context jpeg_write_ctx = {
        .buf = jpeg_buf,
        .total_size = jpeg_buf_size,
        .wrote = 0,
    };

    err = tje_encode_with_func(jpeg_write, &jpeg_write_ctx, 1, width, height, 3, rgb888_buf);
    if (err == 0) {
        lisa_mem_free(rgb888_buf);
        lisa_mem_free(jpeg_buf);
        return -1;
    }

    *jpg_buf = jpeg_buf;
    *jpg_buf_size = jpeg_buf_size;

    lisa_mem_free(rgb888_buf);

    return 0;
}

int audio_recognition_restart_runnable(void *arg)
{
    // extern const uint32_t haoxueduo_speaker_id_get();
    // uint32_t id = haoxueduo_speaker_id_get();

    // extern const char *haoxueduo_speaker_name_get();
    // const char *name = haoxueduo_speaker_name_get();

    // extern int lisa_aiui_start_frame_send_audio_haoxueduo(lisa_aiui_t * handle, const char *speaker_name,
    //                                                       uint32_t speaker_id);
    // lisa_aiui_start_frame_send_audio_haoxueduo(s_cloud->aiui, (char *)name, id);
	recognizer_record_resume();
	assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_RECORD_START, NULL, 0);
}

#include "lsfs.h"

static void flip_rgb565_horizontal(uint16_t *data, uint32_t width, uint32_t height)
{
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width / 2; x++) {
            uint32_t left_idx = y * width + x;
            uint32_t right_idx = y * width + (width - 1 - x);
            uint16_t temp = data[left_idx];
            data[left_idx] = data[right_idx];
            data[right_idx] = temp;
        }
    }
}

struct app_app_cloud_img_recognition_runnable_arg {
	uint8_t *jpg_buf;
	uint32_t jpg_buf_size;
};

int app_app_cloud_img_recognition_runnable(void *p_arg)
{
	// struct app_app_cloud_img_recognition_runnable_arg *arg = (struct app_app_cloud_img_recognition_runnable_arg *)p_arg;
	// uint8_t *jpg_buf = arg->jpg_buf;
	// uint32_t jpg_buf_size = arg->jpg_buf_size;

	// extern const char *haoxueduo_speaker_name_get();
	// const char *name = haoxueduo_speaker_name_get();
	// if (name == NULL) {
	// 	lisa_mem_free(jpg_buf);
	// 	lisa_mem_free(arg);
	// 	return -1;
	// }

	// int err = lisa_aiui_send_jpg_img(s_cloud->aiui, (char *)name, jpg_buf, jpg_buf_size);
	// lisa_mem_free(jpg_buf);
	// lisa_mem_free(arg);

	// evs_handler_post_runnable_delay(audio_recognition_restart_runnable, NULL, 2000);
}

int app_cloud_img_recognition(uint16_t *rgb565_datas, uint32_t width, uint32_t height)
{
    // if (s_cloud == NULL) {
    //     return -1;
    // }

    // if (s_cloud->ws_state != LS_WS_CONNECT) {
    //     LISA_LOGE(TAG, "listen_client_tts web socket is not connected, state:%d", s_cloud->ws_state);
    //     return -1;
    // }

    // if (rgb565_datas != NULL && width > 0 && height > 0) {

    //     flip_rgb565_horizontal(rgb565_datas, width, height);

    //     uint8_t *jpg_buf;
    //     uint32_t jpg_buf_size;
    //     if (rgb565_datas_to_jpg(rgb565_datas, width, height, &jpg_buf, &jpg_buf_size) != 0) {
    //         return -1;
    //     }

	// 	recognizer_record_suspend();
	// 	lisa_aiui_cancel_send(s_cloud->aiui);

	// 	if (s_cloud && s_cloud->aiui && s_cloud->aiui->aiui_ws && s_cloud->aiui->aiui_ws->ws_client) {
	// 		lisa_ws_stop(s_cloud->aiui->aiui_ws->ws_client);
	// 	}

    //     struct app_app_cloud_img_recognition_runnable_arg *arg =
    //         (struct app_app_cloud_img_recognition_runnable_arg *)lisa_mem_alloc(
    //             sizeof(struct app_app_cloud_img_recognition_runnable_arg));
    //     arg->jpg_buf = jpg_buf;
    //     arg->jpg_buf_size = jpg_buf_size;
    //     evs_handler_post_runnable_delay(app_app_cloud_img_recognition_runnable, arg, 1000);
    // } else {
	// 	lisa_aiui_cancel_send(s_cloud->aiui);
    //     evs_handler_post_runnable_delay(audio_recognition_restart_runnable, NULL, 0);
    // }

	return -1;
}
#include "pa_manager.h"
void app_chat_start(void)
{
    if (!app_cloud_is_connected()) {
        extern void recongizer_play_audio_id(uint8_t id);
        recongizer_play_audio_id(TONE_ID_85);
        LISA_LOGI(TAG, "cloud is not connected");
        return;
    }

	app_client_t *client = app_client_get_instance();
    /* 打开全双工链路 */
    lisa_aiui_set_interactive_mode(INTER_CONTINUE);
    assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_RECORD_START, NULL, 0);
    pa_manager_refresh(PA_MGR_ON, LS_PA_BASE_TIME, "wakeup");
    app_cloud_wakeup(client->cloud);
}

void app_chat_stop(void)
{

}

#include "shell.h"
#include "lsfs.h"
#include <stdio.h>

static int img_recognition(int argc, char **argv)
{
    char *path = argv[1];
    if (path == NULL) {
        return -1;
    }
	LISA_LOGI(TAG, "img_recognition path: %s", path);
    struct lsfs_file_t fp;
	lsfs_file_t_init(&fp);

    int err = lsfs_open(&fp, path, LSFS_O_READ);
    if (err != 0) {
		LISA_LOGE(TAG, "lsfs_open failed, err:%d", err);
        return -1;
    }

    lsfs_seek(&fp, 0, SEEK_END);
    uint32_t size = lsfs_tell(&fp);
    lsfs_seek(&fp, 0, SEEK_SET);

	LISA_LOGI(TAG, "img_recognition size: %d", size);
    uint8_t *rgb565_datas;

    rgb565_datas = lisa_mem_alloc(size);
    if (rgb565_datas == NULL) {
		lsfs_close(&fp);
        return -1;
    }

    int r = lsfs_read(&fp, rgb565_datas, size);
    if (r != size) {
		lsfs_close(&fp);
		lisa_mem_free(rgb565_datas);
		LISA_LOGE(TAG, "lsfs_read failed, r:%d", r);
        return -1;
    }

    lsfs_close(&fp);

    err = lisa_aiui_send_jpg_img(s_cloud->aiui, "minmax-cartoon-boy-06", rgb565_datas, size);
	if (err != 0) {
		LISA_LOGE(TAG, "lisa_aiui_img_recognition failed, err:%d", err);
	}
    lisa_mem_free(rgb565_datas);

    return err;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 img_rec, img_recognition, img_recognition test cmd);


