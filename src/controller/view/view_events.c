#include <errno.h>
#include <stdio.h>

#include "listen_mic_gain.h"
#include "listen_volume.h"
#include "lisaui_user_data.h"
#include "assistant_view.h"
// #include "video_camera.h"
#include "ebus/ebus.h"
#include "workqueue.h"
#include "app_cloud.h"
#include "lisa_log.h"
#include "haoxueduo.h"
#include "comm_service.h"
#include "evs_utils.h"
#include "lisa_mem.h"
#include "led.h"

#define TAG "view_event"

#define VIEW_EVENT_WORKQ_THREAD_PRIORITY   2
#define VIEW_EVENT_WORKQ_THREAD_STACK_SIZE 4096
#define VIEW_EVENT_WORKQ_THREAD_NAME       "view_event_workq"
#define VIEW_EVENT_WORKQ_QUEUE_LENGTH      10

static workqueue_t *view_event_workq = NULL;

static void video_take_photo_work_handler(void *param)
{
    uint8_t *data = NULL;
    uint32_t h = 0;
    uint32_t w = 0;
    LISAUI_USERDATA_WITH_LOCK(_userdata)
    {
        if (_userdata->photo != NULL) {
            data = lisa_mem_alloc(_userdata->photo->size);
            if (data) {
                memcpy(data, _userdata->photo->data, _userdata->photo->size);
                h = _userdata->photo->height;
                w = _userdata->photo->width;
            }
        }
    }

    if (data != NULL && w > 0 && h > 0) {
        app_cloud_img_recognition((uint16_t *)&data[0], w, h);
        lisa_mem_free(data);
    }
}

static void video_exit_work_handler(void *param){

    app_cloud_img_recognition(NULL, 0, 0);
}

static void inter_role_update_work_handler(void *param){

    char *roles_name;

    LISAUI_USERDATA_WITH_LOCK(_userdata){
        if(_userdata->roles.role_idx >= 0){
            roles_name = _userdata->roles.roles[_userdata->roles.role_idx].name;
        }
    }                                                                       
    // haoxueduo_chat_start(roles_name);
    extern void app_chat_start(void);
    app_chat_start();
}


static void inter_role_exit_work_handler(void *param){

    // haoxueduo_chat_stop();
    extern void app_chat_stop(void);
    app_chat_stop();
}

int view_event_handler(ebus_chn_t *chn, uint32_t code, void *message, uint32_t msg_size, void *user_data){
    assistant_view_t *view = (assistant_view_t *)user_data;

    LISA_LOGI(TAG, "View trigger event:%d", code);
    switch(code ){
        case LISAUI_EBUS_CH_EVENT_U2M_INTER_ROLE_UPDATE:
            workqueue_submit(view_event_workq, inter_role_update_work_handler, NULL, 0);
            break;
        case LISAUI_EBUS_CH_EVENT_U2M_INTER_ROLE_EXIT:
            LISAUI_USERDATA_WITH_LOCK(_userdata){
                if(_userdata->inter.iat_text != NULL){
                    _userdata->inter.iat_text[0] = '\0';
                }
            }
            workqueue_submit(view_event_workq, inter_role_exit_work_handler, NULL, 0);
            break;
        case LISAUI_EBUS_CH_EVENT_U2M_WIFI_CONNECT:{
            view_wifi_sta_cfg_t cfg;
            LISAUI_USERDATA_WITH_LOCK(_userdata){
                snprintf(cfg.ssid, sizeof(cfg.ssid), "%s", _userdata->setting.wifi.connect_info.cfg.ssid);
                snprintf(cfg.pwd, sizeof(cfg.pwd), "%s", _userdata->setting.wifi.connect_info.cfg.pwd);
            }
            view->cbs.wifi_hotspot_connect(&cfg);
            break;
        }

        case LISAUI_EBUS_CH_EVENT_U2M_WIFI_DISCONNECT:{
            view_wifi_sta_cfg_t cfg;
            LISAUI_USERDATA_WITH_LOCK(_userdata){
                snprintf(cfg.ssid, sizeof(cfg.ssid), "%s", _userdata->setting.wifi.connect_info.cfg.ssid);
                snprintf(cfg.pwd, sizeof(cfg.pwd), "%s", _userdata->setting.wifi.connect_info.cfg.pwd);
            }
            view->cbs.wifi_hotspot_disconnect(&cfg);
            break;
        }
        
        case LISAUI_EBUS_CH_EVENT_U2M_WIFI_SCAN:
            view->cbs.wifi_hotspot_scan();
            break;
        case LISAUI_EBUS_CH_EVENT_U2M_SETTING_VOL_UPDATE:
            LISAUI_USERDATA_WITH_LOCK(_userdata){
                listen_set_volume(_userdata->setting.volume_percent);
            }
            break;
        case LISAUI_EBUS_CH_EVENT_U2M_SETTING_LIGHT_UPDATE:
            break;
        case LISAUI_EBUS_CH_EVENT_U2M_SETTING_INTER_MODE_UPDATE:
            break;
        case LISAUI_EBUS_CH_EVENT_U2M_SETTING_CTR_VIDEO_START:
            lis_ivw_idle();
            // video_camera_start();
            break;
        case LISAUI_EBUS_CH_EVENT_U2M_SETTING_CTR_VIDEO_STOP:
            lis_ivw_run();
            // video_camera_stop();
            break;
        case LISAUI_EBUS_CH_EVENT_U2M_SETTING_TAKE_PHOTO:
            workqueue_submit(view_event_workq, video_take_photo_work_handler, NULL, 0);
            break;
        case LISAUI_EBUS_CH_EVENT_U2M_SETTING_VIDEO_EXIT:
            workqueue_submit(view_event_workq, video_exit_work_handler, NULL, 0);
            break;
        case LISAUI_EBUS_CH_EVENT_U2M_SETTING_MIC_GAIN_UPDATE:
            LISAUI_USERDATA_WITH_LOCK(_userdata){
                listen_mic_gain_set(_userdata->setting.mic_gain);
            }
            break;
        case LISAUI_EBUS_CH_EVENT_U2M_SETTING_MIC_MUTE_UPDATE: {
            bool is_mute = false;
            LISAUI_USERDATA_WITH_LOCK(_userdata)
            {
                is_mute = _userdata->setting.mic_is_mute;
            }
            LISA_LOGI(TAG,"view_event_handler, is_mute:%d", is_mute);
            if (is_mute) {
                extern void recognizer_stop_audio();
                recognizer_stop_audio();
            } else {
                extern void recognizer_start_audio();
                recognizer_start_audio();
                extern int audio_recognition_restart_runnable(void *arg);
                evs_handler_post_runnable(audio_recognition_restart_runnable, NULL);
            }
        } break;
        case LISAUI_EBUS_CH_EVENT_U2M_WIFI_ENABLE:
            if(view->cbs.wifi_sta_enable){
                view->cbs.wifi_sta_enable();
            }
            break;
        case LISAUI_EBUS_CH_EVENT_U2M_WIFI_DISABLE:
            if(view->cbs.wifi_sta_disable){
                view->cbs.wifi_sta_disable();
            }
            break;
        case LISAUI_EBUS_CH_EVENT_U2M_PAGE_INFO_TOGGLE:
            app_led_blink(500, 500);
            break;
        case LISAUI_EBUS_CH_EVENT_U2M_SETTING_HOME_UPDATE:
            if(view->cbs.wifi_cnt_clear){
                view->cbs.wifi_cnt_clear();
            }
            if(wifi_mgr_sta_get_status() == WIFI_MGR_STA_CONNECTED){
                app_led_stop();
                app_led_on();
            }
            break;
        default:
            break;
    }
    return 0;
}

int view_event_init(assistant_view_t *view){
    
    view_event_workq = workqueue_create(VIEW_EVENT_WORKQ_THREAD_NAME, 
                                        VIEW_EVENT_WORKQ_THREAD_PRIORITY, 
                                        VIEW_EVENT_WORKQ_QUEUE_LENGTH,
                                        VIEW_EVENT_WORKQ_THREAD_STACK_SIZE);

    if (view_event_workq == NULL) {
        LISA_LOGE(TAG, "Create view event workqueue failed.");
        return -ENOMEM;
    }

    ebus_message_subscribe(view->ebus_info.base_event_chn, EBUS_EVENT_ALL, EBUS_SUBSCRIBER_TYPE_SYNC,  view_event_handler, view);
    return 0;
}
