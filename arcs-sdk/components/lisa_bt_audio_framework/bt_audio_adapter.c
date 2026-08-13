/**
 * @file bt_audio_adapter.c
 * @brief 蓝牙音频适配层 - 支持 Sink 和 Source 模式
 * 
 * 职责：桥接蓝牙协议栈（A2DP/HFP）与 bt_audio_framework
 * 
 * Copyright (C) ListenAI 2025
 */

#include "bt_config.h"
#include "bt_audio_adapter.h"
#include "bt_os_task.h"
#include "aud_common.h"
#include "bt_api.h"
#include "bt_music_hal.h"
#include "bt_call_hal.h"
#include "bt_stack_hal.h"
#include "lisa_log.h"
#include <stdbool.h>
#include <string.h>

#define TAG "BT_AUDIO_ADAPTER"

#define BT_STACK_HFP_CODEC_TYPE    (BT_STACK_HFP_MSBC_SUPPORT ? HFP_MEDIA_CODEC_MSBC : HFP_MEDIA_CODEC_CVSD)
#define BT_AUDIO_INVALID_CONIDX    0xFF

static uint8_t g_a2dp_conidx = BT_AUDIO_INVALID_CONIDX;  /* A2DP 连接索引 */
static uint8_t g_hfp_conidx = BT_AUDIO_INVALID_CONIDX;   /* HFP 连接索引 */
static bt_audio_adapter_event_ops_t *g_event_ops = NULL;
static bt_audio_profile_e g_current_profile = BT_PROFILE_NONE;

extern bool bt_stack_classic_connected(void);

static uint8_t bt_audio_adapter_handle_data(void *data);
static void bt_audio_adapter_send_data_confirm(const void *pkt_data, size_t len);
static void bt_audio_adapter_clear_connection(uint8_t conidx);
extern void bt_stack_bt_send_a2dp_media_to_peer(uint8_t conidx, uint8_t frame_num,
                                                uint16_t len, uint8_t *data);

/**
 * @brief 根据 codec 类型推断 profile 类型
 */
static inline bt_audio_profile_e codec_to_profile(uint8_t aud_type)
{
    switch (aud_type) {
        case AUD_TYPE_SBC:
            return BT_PROFILE_A2DP;
        case AUD_TYPE_MSBC:
        case AUD_TYPE_CVSD:
            return BT_PROFILE_HFP;
        case AUD_TYPE_LC3:
            return BT_PROFILE_LEA;
        default:
            return BT_PROFILE_NONE;
    }
}

static void bt_audio_adapter_clear_connection(uint8_t conidx)
{
    bool clear_all = (conidx == BT_AUDIO_INVALID_CONIDX);

    if (clear_all || g_a2dp_conidx == conidx) {
        g_a2dp_conidx = BT_AUDIO_INVALID_CONIDX;
        if (g_current_profile == BT_PROFILE_A2DP) {
            g_current_profile = BT_PROFILE_NONE;
        }
    }

    if (clear_all || g_hfp_conidx == conidx) {
        g_hfp_conidx = BT_AUDIO_INVALID_CONIDX;
        if (g_current_profile == BT_PROFILE_HFP) {
            g_current_profile = BT_PROFILE_NONE;
        }
    }
}

/* ========================================================================
 * os_task_cb_t回调实现
 * ======================================================================== */

void bt_audio_adapter_os_init(uint8_t type)
{
    LISA_LOGI(TAG, "BT audio adapter OS task init, type=%d", type);
}

uint8_t bt_audio_adapter_os_msg_handle(btos_event_t *msg)
{
    uint8_t msg_free = 1;  // 默认需要释放消息
    
    if (!msg || !msg->msg_body) {
        LISA_LOGE(TAG, "Invalid message");
        return msg_free;
    }
    
    switch (msg->msg_body->msg_id) {
        case AUD_OS_START_EVT:
        {
            if (!msg->msg_body->param) {
                LISA_LOGE(TAG, "AUD_OS_START_EVT: invalid param");
                break;
            }
            aud_codec_info_t codec_info;
            memcpy(&codec_info, msg->msg_body->param, sizeof(aud_codec_info_t));
            LISA_LOGI(TAG, "AUD_OS_START_EVT: type=%d, ch=%d, sample=%d",
                  codec_info.aud_type, codec_info.aud_ch, codec_info.aud_sample);

            g_current_profile = codec_to_profile(codec_info.aud_type);
            if (g_event_ops && g_event_ops->bt_event_start) {
                g_event_ops->bt_event_start(codec_info);
            }
        }
        break;
        
        case AUD_OS_STOP_EVT:
        {
            bt_audio_profile_e stopped_profile = g_current_profile;

            if (!msg->msg_body->param) {
                LISA_LOGE(TAG, "AUD_OS_STOP_EVT: invalid param");
                break;
            }
            uint8_t conidx = *msg->msg_body->param;
            uint8_t status = *(msg->msg_body->param + 1);
            LISA_LOGI(TAG, "AUD_OS_STOP_EVT: conidx=%d, status=0x%x", conidx, status);
            g_current_profile = BT_PROFILE_NONE;
            if (conidx == BT_AUDIO_INVALID_CONIDX) {
                bt_audio_adapter_clear_connection(BT_AUDIO_INVALID_CONIDX);
            } else if (!bt_stack_classic_connected()) {
                bt_audio_adapter_clear_connection(conidx);
            } else if (stopped_profile == BT_PROFILE_A2DP && g_a2dp_conidx == conidx) {
                LISA_LOGI(TAG, "A2DP stream stopped, keep active connection conidx=%d", conidx);
            } else if (stopped_profile == BT_PROFILE_HFP && g_hfp_conidx == conidx) {
                LISA_LOGI(TAG, "HFP audio stopped, keep active connection conidx=%d", conidx);
            }
            if (g_event_ops && g_event_ops->bt_event_stop) {
                g_event_ops->bt_event_stop(conidx, status);
            }
        }
        break;
        
        case AUD_OS_RCV_DATA_EVT:
        {
            bt_audio_adapter_handle_data(msg->msg_body->param);
            bt_audio_adapter_send_data_confirm(msg->msg_body->param, msg->msg_body->param_len);
        }
        break;
        
        case AUD_OS_PAUSE_EVT:
        {
            LISA_LOGI(TAG, "AUD_OS_PAUSE_EVT");
            if (g_event_ops && g_event_ops->bt_event_pause) {
                g_event_ops->bt_event_pause();
            }
        }
        break;
        
        case AUD_OS_RESUME_EVT:
        {
            if (!msg->msg_body->param) {
                LISA_LOGE(TAG, "AUD_OS_RESUME_EVT: invalid param");
                break;
            }
            aud_codec_info_t codec_info;
            memcpy(&codec_info, msg->msg_body->param, sizeof(aud_codec_info_t));
            LISA_LOGI(TAG, "AUD_OS_RESUME_EVT");
            if (g_event_ops && g_event_ops->bt_event_resume) {
                g_event_ops->bt_event_resume(codec_info);
            }
        }
        break;
        
        case AUD_OS_A2DP_CONNECTION_UPDATE_EVT:
        {
            if (!msg->msg_body->param) {
                LISA_LOGE(TAG, "AUD_OS_A2DP_CONNECTION_UPDATE_EVT: invalid param");
                break;
            }
            uint8_t conidx = *msg->msg_body->param;
            
            LISA_LOGI(TAG, "AUD_OS_A2DP_CONNECTION_UPDATE_EVT: conidx=%d", conidx);
            if (conidx == BT_AUDIO_INVALID_CONIDX) {
                g_a2dp_conidx = BT_AUDIO_INVALID_CONIDX;
                if (g_current_profile == BT_PROFILE_A2DP) {
                    g_current_profile = BT_PROFILE_NONE;
                }
            } else {
                g_a2dp_conidx = conidx;
            }
        }
        break;
        
        case AUD_OS_HFP_CONNECTION_UPDATE_EVT:
        {
            if (!msg->msg_body->param) {
                LISA_LOGE(TAG, "AUD_OS_HFP_CONNECTION_UPDATE_EVT: invalid param");
                break;
            }
            uint8_t conidx = *msg->msg_body->param;
            LISA_LOGI(TAG, "AUD_OS_HFP_CONNECTION_UPDATE_EVT: conidx=%d", conidx);
            if (conidx == BT_AUDIO_INVALID_CONIDX) {
                g_hfp_conidx = BT_AUDIO_INVALID_CONIDX;
                if (g_current_profile == BT_PROFILE_HFP) {
                    g_current_profile = BT_PROFILE_NONE;
                }
            } else {
                g_hfp_conidx = conidx;
            }
        }
        break;
        
        case AUD_OS_A2DP_MEDIA_RSP_EVT:
        {
            /* A2DP 媒体数据发送完成回调，释放信号量允许下次发送 */
            if (g_event_ops && g_event_ops->bt_audio_send_complete) {
                g_event_ops->bt_audio_send_complete();
            }
        }
        break;
        
        default:
            LISA_LOGW(TAG, "Unknown message id: 0x%x", msg->msg_body->msg_id);
            break;
    }
    
    return msg_free;
}

uint8_t bt_audio_adapter_os_user_schedule(void)
{
    // 可以在这里做周期性任务，比如统计信息更新
    return 0;
}

/* ========================================================================
 * os_task_cb_t回调结构体
 * ======================================================================== */

static const os_task_cb_t g_adapter_os_cb = {
    .cb_os_init          = bt_audio_adapter_os_init,
    .cb_os_msg_handle    = bt_audio_adapter_os_msg_handle,
    .cb_os_user_schedule = bt_audio_adapter_os_user_schedule,
};

/**
 * @brief 处理接收到的蓝牙音频数据
 * @param pkt_info 音频包信息
 * @return 1 失败 0 成功
 */
static uint8_t bt_audio_adapter_handle_data(void *data)
{
    if (!data) {
        LISA_LOGE(TAG, "Invalid data");
        return 1;
    }
    
    /* 根据当前 profile 提取包数据信息 */
    uint8_t *pkt_data = NULL;
    uint16_t pkt_len = 0;
    uint32_t pkt_seq = 0;
    
    if (g_current_profile == BT_PROFILE_A2DP) {
        bt_aud_pkt_info_t *pkt_info = (bt_aud_pkt_info_t *)data;
        pkt_data = pkt_info->data;
        pkt_len = pkt_info->len;
        pkt_seq = pkt_info->seq;
    } else if (g_current_profile == BT_PROFILE_HFP) {
        bt_call_pkt_info_t *pkt_info = (bt_call_pkt_info_t *)data;
        pkt_data = pkt_info->data;
        pkt_len = pkt_info->len;
    } else {
        LISA_LOGW(TAG, "No active profile to handle data");
        return 1;
    }
    
    if (!pkt_data || pkt_len == 0) {
        LISA_LOGE(TAG, "Invalid packet data or length");
        return 1;
    }
    
    if (g_event_ops && g_event_ops->bt_event_rcv_data) {
        g_event_ops->bt_event_rcv_data(pkt_data, pkt_len);
    } else {
        LISA_LOGW(TAG, "No event handler for received data");
    }

    return 0;
}

/**
 * @brief 发送数据确认消息给蓝牙任务
 * @param pkt_data 数据包信息指针
 * @param len 数据包大小
 * 
 * 当音频数据处理完成后，需要发送确认消息通知蓝牙任务可以释放相关内存。
 * 必须复制数据，因为原消息在发送确认消息前就会被释放。
 * 
 * @note 使用 btos_malloc 分配内存，与接收消息时的分配器保持一致
 */
static void bt_audio_adapter_send_data_confirm(const void *pkt_data, size_t len)
{
    if (!pkt_data || len == 0) {
        LISA_LOGW(TAG, "Invalid parameters for data confirm");
        return;
    }
    
    btos_event_t cfm_ev;
    cfm_ev.msg_body = btos_malloc(sizeof(btos_msg_t) + len);
    if (!cfm_ev.msg_body) {
        LISA_LOGE(TAG, "Failed to allocate memory for data confirm message");
        return;
    }
    
    cfm_ev.msg_body->msg_id = BT_OS_DATA_SEND_CNF_EVT;
    uint16_t task_id = cfm_ev.msg_body->msg_id >> 8;
    
    memcpy(cfm_ev.msg_body->param, pkt_data, len);
    
    if (btos_send_event(task_id, &cfm_ev, BTOS_TASK_MAX_DELAY) == pdFALSE) {
        LISA_LOGE(TAG, "Failed to send data confirm event");
        btos_free(cfm_ev.msg_body);
    }
}

os_task_cb_t *bt_audio_adapter_get_os_task_cb(void)
{
    return (os_task_cb_t *)&g_adapter_os_cb;
}

int bt_audio_adapter_send_frames(uint8_t conidx, const uint8_t *data, 
                                    size_t frame_count, size_t total_bytes)
{
    if (!data || total_bytes == 0 || conidx == BT_AUDIO_INVALID_CONIDX) {
        return -1;
    }
    
    if (g_current_profile == BT_PROFILE_A2DP) {
#if BT_MUSIC_PRESENT
        bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
        if (!stack_env) {
            return -1;
        }
        if (stack_env->bt_music_send_cnt >= BT_STACK_CLASSIC_BIG_ACL_SEND_MAX) {
            stack_env->bt_music_full = 1;
            return -2;
        }
        bt_stack_bt_send_a2dp_media_to_peer(conidx, (uint8_t)frame_count,
                                            (uint16_t)total_bytes, (uint8_t *)data);
#endif
    } else if (g_current_profile == BT_PROFILE_HFP) {
#if BT_CALL_PRESENT
        bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
        if(stack_env->bt_call_send_cnt <= 4) {
            app_hfp_send_aud_to_peer(conidx, (uint16_t)total_bytes, (uint8_t *)data);
            stack_env->bt_call_send_cnt++;
        }
#endif
    }

    return 0;
}

void bt_audio_adapter_a2dp_media_rsp(uint8_t conidx, uint8_t *data, uint16_t status)
{
    if (g_event_ops && g_event_ops->bt_audio_send_buffer_release) {
        g_event_ops->bt_audio_send_buffer_release(conidx, data, status);
    }

    if (g_event_ops && g_event_ops->bt_audio_send_complete) {
        g_event_ops->bt_audio_send_complete();
    }
}

bt_audio_profile_e bt_audio_adapter_get_profile(void)
{
    return g_current_profile;
}

int bt_audio_adapter_start_audio_stream(bt_audio_profile_e profile)
{
    // 检查对应 profile 的 conidx 是否已设置
    uint8_t conidx = (profile == BT_PROFILE_A2DP) ? g_a2dp_conidx : g_hfp_conidx;
    if (profile != BT_PROFILE_A2DP && profile != BT_PROFILE_HFP) {
        LISA_LOGE(TAG, "Unsupported profile: %d", profile);
        return -1;
    }
    if (conidx == BT_AUDIO_INVALID_CONIDX) {
        LISA_LOGW(TAG, "No active connection for profile %d yet.", profile);
        return -1;
    }
    if (!bt_stack_classic_connected()) {
        LISA_LOGW(TAG, "Classic link is disconnected, drop cached conidx=%d for profile %d", conidx, profile);
        bt_audio_adapter_clear_connection(conidx);
        return -1;
    }
    
    // 设置当前活跃 profile
    g_current_profile = profile;
    
    LISA_LOGI(TAG, "Starting audio stream for profile: %d (conidx=%d)", profile, conidx);
    
    /* 根据 profile 类型调用对应的启动函数 */
    if (profile == BT_PROFILE_A2DP) {
        /* A2DP Source: 启动音乐流 */
        extern void app_a2dp_start(uint8_t conidx);
        app_a2dp_start(conidx);
        LISA_LOGI(TAG, "A2DP stream start requested");
        
    } else if (profile == BT_PROFILE_HFP) {
        /* HFP AG: 建立 SCO 连接（ */
        
        app_hfp_set_codec_type(conidx, BT_STACK_HFP_CODEC_TYPE);
        /* 先设置呼叫为激活状态 */
        app_hfp_call_start(conidx, 0);

        /* codec select status callback will request SCO; avoid duplicate add_audio. */
        LISA_LOGI(TAG, "HFP call/codec setup requested, wait codec callback for SCO");
        
    } else {
        LISA_LOGE(TAG, "Unsupported profile: %d", profile);
        return -1;
    }
    
    return 0;
}

int bt_audio_adapter_stop_audio_stream(bt_audio_profile_e profile)
{
    uint8_t conidx = (profile == BT_PROFILE_A2DP) ? g_a2dp_conidx : g_hfp_conidx;
    if (profile != BT_PROFILE_A2DP && profile != BT_PROFILE_HFP) {
        LISA_LOGE(TAG, "Unsupported profile: %d", profile);
        return -1;
    }
    if (conidx == BT_AUDIO_INVALID_CONIDX) {
        LISA_LOGW(TAG, "No active connection for profile %d yet.", profile);
        return -1;
    }
    
    if (profile == BT_PROFILE_A2DP) {
        /* A2DP Source: 停止音乐流 */
        extern uint8_t app_a2dp_suspend(uint8_t conidx);
        app_a2dp_suspend(conidx);
        LISA_LOGI(TAG, "A2DP stream suspend requested");
        
    } else if (profile == BT_PROFILE_HFP) {
        /* HFP AG: 断开 SCO */
        app_hfp_call_end(conidx, 0);
        app_hfp_call_remove_audio(conidx, CO_ERROR_REMOTE_USER_TERM_CON);
        LISA_LOGI(TAG, "HFP SCO disconnect requested");
    }

    return 0;
}

int bt_audio_adapter_registeer(bt_audio_adapter_event_ops_t *ops)
{
    if (!ops) {
        LISA_LOGE(TAG, "event ops is NULL");
        return -1;
    }
    g_event_ops = ops;
    return 0;
}
