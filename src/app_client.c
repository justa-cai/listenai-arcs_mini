#define TAG "app_client"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_tone.h"
#include "lisa_err.h"
#include "lisa_mem.h"
#include "lisa_log.h"
#include "app_client.h"
#include "app_player.h"
#include "pa_manager.h"
#include "lisa_typedef.h"
#include "evs_utils.h"
#include "app_algo.h"
#include "listen_audiomgr.h"
#include "audio_player.h"
#include "short_player.h"
#include "tts_player.h"
#include "sound_player.h"
#include "play_mode.h"
#include "listen_volume.h"
#include "tone.h"
#include "music_manager.h"
#ifdef LISTEN_CLOUD
#include "app_cloud.h"
#endif
#include "listen_wifi.h"
#include "listen_system.h"
#include "assistant_controller.h"
#include "test.h"
#include "alarm_aiui.h"
#include "alarm_ring.h"
#include "lisa_aiui.h"
#include "config_parser.h"
#include "recognizer.h"
#include "led.h"
#include "ota_manager.h"

static app_client_t *s_app_client = NULL;

#define LS_ALGO_KEYWORD_MAX_LEN (64)

static int offline_tone_play(const uint8_t *keyword)
{
	int tone_id = local_tone_get(keyword);
	if (tone_id < 0) {
		LISA_LOGE(TAG, "dont support this keyword: %s \n", keyword);
		return -1;
	}
	///提高唤醒响应速率
#ifdef LISTEN_CLOUD
	if(tone_id == 0) {
		// 播放网络未连接提示音
		listen_soundplayer_play(s_app_client->sound_player, TONE_ID_64, 0);
	}
#else
	listen_soundplayer_play(s_app_client->sound_player, tone_id, 0);
#endif
	
	return 0;
}

const char *app_client_voice_keywords_list[] = {
	"hai ti ni",
	"hei ti ni",
	"hai di ni",
	"hei di ni",
	"xiao xian xiao xian",
	"xiao mei xiao mei",
};

bool app_client_is_voice_keywords(const char *keyword)
{
    const Config *config = config_get();

	if (!config) {
		LOGE("config is not exist");
		return true;
	}

	if (!config->wake_up.voice.enable) {
		LOGE("wake up by voice is disabled");
		return false;
	}

	if (!config->wake_up.voice.keywords_filter) {
		LOGE("wake up without keywords filter");
		return true;
	}

    if (config->wake_up.voice.keywords.items) {
        for (size_t i = 0; i < config->wake_up.voice.keywords.count; i++) {
            if (config->wake_up.voice.keywords.items[i] &&
                strcmp(keyword, config->wake_up.voice.keywords.items[i]) == 0) {
                return true;
            }
        }
    }

	LOGE("wake up keywords %s not found", keyword);

    return false;
}

static void do_wakeup()
{
    alarm_ring_stop();
    if (!app_cloud_is_wifi_connected()) {
        /* server is not connected, play tone */
        LISA_LOGW(TAG, "server is not connected");
        listen_soundplayer_play(s_app_client->sound_player, TONE_ID_64, 0);
        // assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_PRE_IDLE, NULL, 0);
        return;
    }

    /* do wakeup */
    assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_WAKEUP, NULL, 0);
    // pa_manager_refresh(PA_MGR_ON, LS_PA_BASE_TIME, "wakeup");
// #ifdef LISTEN_CLOUD
//     app_cloud_wakeup(s_app_client->cloud);
// #endif

	// extern int haoxueduo_chat_start(const char *name);
	// haoxueduo_chat_start("Teeni");
	// assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_RECORD_START, NULL, 0);
}

static int voice_wakeup_runnable(void *arg)
{
    uint8_t keyword[LS_ALGO_KEYWORD_MAX_LEN] = {0};
    int ret = app_algo_keyword_and_kid_extract((const uint8_t *)arg, keyword, sizeof(keyword) - 1);

    lisa_mem_free(arg);

    if (ret != 0) {
        LISA_LOGE(TAG, "extra keyword fail");
        return -1;
    }

    if (app_client_is_voice_keywords((const char *)keyword) == false) {
        LISA_LOGE(TAG, "dont support this keyword: %s \n", keyword);
        return -1;
    }

    LISA_LOGI(TAG, "wakeup keyword: %s", keyword);

	do_wakeup();

    return 0;
}

static int btn_wakeup_runnable(void *arg)
{
    do_wakeup();

    return 0;
}

static int btn_idle_runnable(void *arg)
{
	recognizer_record_suspend();
    assist_controller_trigger_event(CONTROLLER_EVENT_STATE_AUDIO_IDLE, NULL, 0);
	assistant_view_hide_camera_image();

    return 0;
}

void app_btn_idle(void)
{
	LISA_LOGD(TAG, "audio idle---");
    evs_handler_post_runnable(btn_idle_runnable, NULL);
}

void app_btn_wakeup(void)
{
    // int mode = lisa_aiui_get_interactive_mode();

    // if (mode != INTER_BUTTON) {
    //     LISA_LOGW(TAG, "not in button wakeup mode, current mode: %d", mode);
    //     return;
    // }
	LISA_LOGD(TAG, "audio Wakeup---");
    evs_handler_post_runnable(btn_wakeup_runnable, NULL);
}

void app_btn_wakeup_release(void)
{
    int mode = lisa_aiui_get_interactive_mode();

    // if (mode != INTER_BUTTON) {
    //     LISA_LOGW(TAG, "not in button wakeup mode, current mode: %d", mode);
    //     return;
    // }

    extern void recognizer_manual_stop(void);
    recognizer_manual_stop();
}

void app_client_wakeup(const uint8_t *data, uint32_t len)
{

    int mode = lisa_aiui_get_interactive_mode();

    if (mode != INTER_ONESHOT && mode != INTER_CONTINUE) {
        LISA_LOGW(TAG, "not in voice wakeup mode, current mode: %d", mode);
        return;
    }

    // 此处需要放到evs_event线程中运行, 否则会阻塞核间通信
    uint8_t *info = lisa_mem_alloc(sizeof(uint8_t) * (len + 1));
    LISA_LOGD(TAG, "Wakeup: %s", data);
    if (info) {
        memcpy(info, data, len);
        info[len] = '\0';
        evs_handler_post_runnable(voice_wakeup_runnable, (void *)info);
    }
}

static int _handle_esr_timeout_runnable(void *arg)
{
	LISA_LOGD(TAG, "esr timeout");
	return 0;
}

void app_client_algo_timeout()
{
	// 此处需要放到evs_event线程中运行, 否则可能会阻塞核间通信
	evs_handler_post_runnable(_handle_esr_timeout_runnable, NULL);
}

/**
 * @brief 	停止 Client
 */
void app_client_stop()
{
	if (s_app_client) {
		if (s_app_client->sound_player) {
			s_app_client->sound_player->stop(s_app_client->sound_player, false);
		}
		if (s_app_client->tts_player) {
			s_app_client->tts_player->stop(s_app_client->tts_player);
		}
		if (s_app_client->audio_player) {
			s_app_client->audio_player->stop(s_app_client->audio_player, false);
		}
		if (s_app_client->short_player) {
			listen_shortplayer_stop(s_app_client->short_player);
		}
	}
}

/** SNTP Synced */
static void _ls_sntp_synced_callback(void)
{
	LISA_LOGI(TAG, "sntp synced");
#ifdef LISTEN_CLOUD
	app_cloud_ntp_ok(NULL);
#endif
	app_led_on();

	ota_manager_check_all();

	// extern lisa_err_t lisa_aiui_active(void);
	// lisa_aiui_active();
	alarm_ring_init(s_app_client->sound_player);
	alarm_aiui_init(alarm_ring_on_alarm);
#ifdef LISTEN_CLOUD
	app_cloud_process_wifi_connected(s_app_client->cloud);
#endif
}

static void _ls_wifi_status_cb(ls_wifi_status_t status)
{
	LISA_LOGI(TAG, "_ls_wifi_status_cb, wifi status: %d", status);
	if (status == LS_WIFI_STA_CONNECTED) {
		// start sntp
		ls_sys_sntp_start(_ls_sntp_synced_callback);

	} else if (status == LS_WIFI_STA_DISCONNECTED) {
	#ifdef LISTEN_CLOUD
		app_cloud_process_wifi_disconnected(s_app_client->cloud);
	#endif
	}
}

app_client_t *app_client_create()
{
	app_client_t *handle = lisa_mem_calloc(1, sizeof(app_client_t));
	s_app_client = handle;

	if (handle) {
		ota_manager_init();

		// Wifi init
		ls_wifi_init(_ls_wifi_status_cb);

		// 播放器模块初始化
		app_player_init();
		listen_volume_init();
		music_manager_init();
		
		handle->audio_mgr = listen_audiomgr_create();
		handle->play_mode = listen_play_mode_create();
		handle->short_player = listen_shortplayer_create();
		handle->audio_player = listen_audioplayer_create(handle->audio_mgr, handle->play_mode);
		handle->tts_player = listen_ttsplayer_create(handle->audio_mgr);
		handle->sound_player = listen_soundplayer_create(handle->audio_mgr);

#ifdef LISTEN_CLOUD
		handle->cloud = app_cloud_create(handle);
#endif

	}
	LISA_LOGD(TAG, "APP client create [E]");
	return handle;
}

app_client_t *app_client_get_instance()
{
	return s_app_client;
}

#define UAS_REC_FRM_SAMPS       	256     // samples/frame
#define UAS_REC_CHANNELS        	5       // channels, stero
#define LS_RECORD_REC_CHANNEL_NUM 	(UAS_REC_CHANNELS - 1)
static short s_record_rec_buf[UAS_REC_FRM_SAMPS] = {0};
#define LS_RECORD_ONE_CHNNEL_SIZE (512)

#include "shell.h"
#include "lsfs.h"
static struct lsfs_file_t s_audio_capture_file;
static volatile bool s_audio_capture = false;
static volatile bool s_audio_capture_write = false;

void app_client_record(const char *audio, int len)
{
	if (!s_app_client) return;

	short(*uac_rec)[UAS_REC_CHANNELS] = (short int (*)[5])audio;
	for (int i = 0; i < UAS_REC_FRM_SAMPS; i++) {
		s_record_rec_buf[i] = uac_rec[i][3];
	}
	if (s_audio_capture) {
		s_audio_capture_write = true;
		int r = lsfs_write(&s_audio_capture_file, audio, len);
		if (r <= 0) {
			LISA_LOGE(TAG, "write file failed, err: %d", r);
		}
		s_audio_capture_write = false;
	}
#ifdef LISTEN_CLOUD
	app_cloud_audio(s_app_client->cloud, (const char*)s_record_rec_buf, LS_RECORD_ONE_CHNNEL_SIZE);
#endif
}

static int shell_audio_capture(int argc, char **argv)
{
    int r;

    if (argc < 2) {
        return -1;
    }

    if (strncmp(argv[1], "on", 2) == 0) {
        if (s_audio_capture == false) {
            lsfs_file_t_init(&s_audio_capture_file);
            r = lsfs_open(&s_audio_capture_file, "/SD:/audio_capture.pcm", LSFS_O_WRITE | LSFS_O_CREATE | LSFS_O_TRUNC);
            if (r != 0) {
				shellPrint(shellGetCurrent(), "open file failed, err: %d\r\n", r);
            } else {
				s_audio_capture = true;
				shellPrint(shellGetCurrent(), "open file success, audio capture on\r\n");
			}
        }
    } else if (strncmp(argv[1], "off", 3) == 0) {
        if (s_audio_capture) {
            while (s_audio_capture_write) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            lsfs_close(&s_audio_capture_file);
            s_audio_capture = false;
			shellPrint(shellGetCurrent(), "close file success, audio capture off\r\n");
        }
    }

    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, audio_capture,
                 shell_audio_capture, "audio capture");

static int wakeup_test_runnable(void *arg)
{
	app_cloud_wakeup(s_app_client->cloud);
	return 0;
}
#include "lisa_thread.h"

static void _client_wakeup_func(void *arg)
{
	evs_handler_post_runnable(wakeup_test_runnable, NULL);
	int left = test_pcm_len;
	while (left >= LS_RECORD_ONE_CHNNEL_SIZE) {
		app_cloud_audio(s_app_client->cloud, test_pcm + test_pcm_len - left, LS_RECORD_ONE_CHNNEL_SIZE);
		left -= LS_RECORD_ONE_CHNNEL_SIZE;
		lisa_thread_mdelay(16);
	}
}

void client_wakeup_test()
{
	lisa_thread_attr_t attr;
	attr.stack_size = 4096;
	attr.priority = LISA_OS_PRIORITY_NORMAL;
	attr.name = (uint8_t *)"wakeup_test";
	lisa_thread_create(&attr, _client_wakeup_func, NULL);
}

void app_main(void)
{
	app_client_create();
}

void handle_stop_play(void)
{
	app_client_stop();
}
