/*
 * bt_stack_if.c
 *
 *  bt stack interface functions
 */

/*
 * INCLUDES
 ****************************************************************************************
 */
#include <string.h>
#include <assert.h>
#include <stdlib.h>    // standard lib functions
#include <stddef.h>    // standard definitions
#include <stdint.h>    // standard integer definition
#include <stdbool.h>   // boolean definition

#include "log_print.h"
#include "nvs.h"

//#include "plf.h"

#include "ble_task.h"
#include "ble_drv.h"
#include "ble_plf_config.h"
#include "ble_gap.h"
#include "ble_prf.h"

#include "bt_classic_if.h"

#include "bt_stack_cfg.h"
#include "bt_stack_hal.h"
#include "lisa_bluetooth.h"

#if BT_STACK_PRESENT
#include "bt_a2dp.h"
#include "bt_avrcp.h"
#include "bt_music_hal.h"
#include "bt_hfp.h"
#include "bt_call_hal.h"

extern void bt_paired_record_update(const gap_bdaddr_t *addr, uint8_t transport);

__attribute__((weak)) void bt_audio_adapter_a2dp_media_rsp(uint8_t conidx, uint8_t *data, uint16_t status)
{
    (void)conidx;
    (void)data;
    (void)status;
}

typedef void (*bt_stack_bt_inquiry_stop_cb_t)(int16_t status);
typedef void (*bt_stack_bt_connect_fail_cb_t)(uint8_t actv_id, int16_t status);
typedef void (*bt_stack_bt_connect_actv_cb_t)(uint8_t actv, int16_t status);
typedef void (*bt_stack_bt_link_auth_fail_cb_t)(uint8_t conidx, uint8_t reason);

static gap_bdaddr_t s_classic_peer_addr;
static bool s_classic_peer_valid;
static bt_stack_bt_inquiry_stop_cb_t s_bt_inquiry_stop_cb;
static bt_stack_bt_connect_fail_cb_t s_bt_connect_fail_cb;
static bt_stack_bt_connect_actv_cb_t s_bt_connect_actv_cb;
static bt_stack_bt_link_auth_fail_cb_t s_bt_link_auth_fail_cb;
static bool s_profile_connect_pending;
static uint8_t s_profile_connect_conidx;

/*
 * DEFINES
 ****************************************************************************************
 */

#define BT_STACK_HFP_CODEC_TYPE    (BT_STACK_HFP_MSBC_SUPPORT + HFP_MEDIA_CODEC_CVSD)

/* 角色配置（从 Kconfig 获取） */
#ifdef CONFIG_BT_CLASSIC_ROLE_SOURCE
    #define BT_STACK_A2DP_ROLE          BT_A2DP_SOURCE
    #define BT_STACK_HFP_ROLE           BT_HFP_ROLE_HF_AG
    #define BT_STACK_HFP_FEATS          ((BT_HFP_AGSF_WBS * BT_STACK_HFP_MSBC_SUPPORT)|BT_HFP_AGSF_NREC)
    #define BT_STACK_HFP_PEER_ROLE      BT_HFP_ROLE_HF
    #define BT_MUSIC_SOURCE_SEND_DUMMY  (0)
    #define BT_HFP_SOURCE_SEND_DUMMY    (0)
#else
    #define BT_STACK_A2DP_ROLE          BT_A2DP_SINK
    #define BT_STACK_HFP_ROLE           BT_HFP_ROLE_HF
    #define BT_STACK_HFP_FEATS          ((BT_HFP_AGSF_WBS * BT_STACK_HFP_MSBC_SUPPORT)|BT_HFP_HFSF_VOL_CTL|BT_HFP_HFSF_NREC)
    #define BT_STACK_HFP_PEER_ROLE      BT_HFP_ROLE_HF_AG
    #define BT_MUSIC_SOURCE_SEND_DUMMY  (0)
    #define BT_HFP_SOURCE_SEND_DUMMY    (0)
#endif

#if (BT_HFP_SOURCE_SEND_DUMMY && BT_MUSIC_SOURCE_SEND_DUMMY)
    #error "Only one dummy can be set to 1!"
#endif

/* HFP SCO 数据回环测试 */
#define BT_CALL_SCO_SEND_DUMMY       (0)


/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */
static void bt_stack_classic_key_req(uint8_t conidx, uint8_t key_type, uint32_t key);

#if BT_MUSIC_PRESENT
static void a2dp_enable_cmp(uint16_t status);
static void a2dp_revoke_cmp(uint16_t status);
static void a2dp_connect_cmp(uint8_t conidx, uint16_t status);
static void a2dp_disconnect_cmp(uint8_t conidx, uint16_t status);
static void a2dp_start_ind(uint8_t conidx, uint8_t codec, uint8_t ch, uint16_t sample_rate);
static void a2dp_stop_ind(uint8_t conidx, uint8_t status);
static void a2dp_media_ind(uint8_t conidx, uint8_t frame_num, uint16_t seq, uint16_t len, uint8_t *data);
static void a2dp_media_rsp(uint8_t conidx, uint8_t *data, uint16_t status);

static void avrcp_connect_cmp(uint8_t conidx, uint16_t status);
static void avrcp_disconnect_cmp(uint8_t conidx, uint16_t status);
static void avrcp_avrcp_press_cmp(uint8_t conidx, uint16_t status);
static void avrcp_notify_cmp(uint8_t conidx, uint16_t status);
static void avrcp_media_cmp(uint8_t conidx, uint16_t status);
static void avrcp_avrcp_press_ind(uint8_t conidx, uint8_t key_type, uint8_t key_id, uint8_t key_value);
static void avrcp_notify_ind(uint8_t conidx, uint8_t c_r, uint8_t event_id, uint8_t event_value);
static void avrcp_media_ind(uint8_t conidx, uint8_t status);
#endif

#if BT_CALL_PRESENT
static void hfp_enable_cmp(uint16_t status);
static void hfp_disable_cmp(uint16_t status);
static void hfp_connect_cmp(uint8_t conidx, uint8_t type, uint16_t status);
static void hfp_disconnect_cmp(uint8_t conidx, uint16_t status);
void hfp_aud_start_ind(uint8_t conidx, uint16_t codec, uint16_t status);
void hfp_aud_stop_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason);
static void hfp_receive_media_from_peer(uint8_t conidx, uint8_t pkt_sta, uint16_t len, uint8_t *data);
static void hfp_send_media_cmp(uint8_t conidx, uint8_t status, uint8_t *data);
static void hfp_status_ind(uint8_t conidx, uint8_t req_type, uint8_t status_type, uint16_t val);
static void hfp_call_ind(uint8_t conidx, uint8_t req_type, uint8_t call_type, uint8_t call_idx);
#endif

/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
extern const ble_gap_cb_t bt_stack_gap_cb;

/* ========================================================================
 * A2DP 配置和回调
 * ======================================================================== */
#if BT_MUSIC_PRESENT

bt_a2dp_cfg_t a2dp_cfg = {
    .a2dp_role = BT_STACK_A2DP_ROLE,
    .aac_support = 0,
};

const bt_a2dp_cb_t bt_a2dp_cb = {
    .cb_a2dp_enable_cmp         = a2dp_enable_cmp,
    .cb_a2dp_revoke_cmp         = a2dp_revoke_cmp,
    .cb_a2dp_connect_cmp        = a2dp_connect_cmp,
    .cb_a2dp_disconnect_cmp     = a2dp_disconnect_cmp,
    .cb_a2dp_start_ind          = a2dp_start_ind,
    .cb_a2dp_stop_ind           = a2dp_stop_ind,
    .cb_a2dp_media_ind          = a2dp_media_ind,
    .cb_a2dp_media_rsp          = a2dp_media_rsp,
};

const bt_avrcp_cb_t bt_avrcp_cb = {
    .cb_avrcp_connect_cmp       = avrcp_connect_cmp,
    .cb_avrcp_disconnect_cmp    = avrcp_disconnect_cmp,
    .cb_avrcp_press_cmp         = avrcp_avrcp_press_cmp,
    .cb_avrcp_notify_cmp        = avrcp_notify_cmp,
    .cb_avrcp_media_cmp         = avrcp_media_cmp,
    .cb_avrcp_press_ind         = avrcp_avrcp_press_ind,
    .cb_avrcp_notify_ind        = avrcp_notify_ind,
    .cb_avrcp_media_ind         = avrcp_media_ind,
};

#endif

/* ========================================================================
 * HFP 配置和回调
 * ======================================================================== */
#if BT_CALL_PRESENT

bt_hfp_cfg_t hfp_cfg = {
    .hfp_role = BT_STACK_HFP_ROLE,
    .hfp_feats = BT_STACK_HFP_FEATS,
};

const bt_hfp_cb_t bt_hfp_cb = {
    .cb_hfp_enable_cmp         = hfp_enable_cmp,
    .cb_hfp_disable_cmp        = hfp_disable_cmp,
    .cb_hfp_con_cmp            = hfp_connect_cmp,
    .cb_hfp_discon_cmp         = hfp_disconnect_cmp,
    .cb_hfp_aud_start_ind      = NULL,  // 由 bt_stack_if 注册
    .cb_hfp_aud_stop_ind       = NULL,  // 由 bt_stack_if 注册
    .cb_hfp_media_ind          = hfp_receive_media_from_peer,
    .cb_hfp_send_media_cmp     = hfp_send_media_cmp,
    .cb_hfp_status_ind         = hfp_status_ind,
    .cb_hfp_call_ind           = hfp_call_ind,
};

#endif
/*
 * LOCAL FUNCTIONS
 ****************************************************************************************
 */

/* ========================================================================
 * A2DP SOURCE 模式 - 虚拟数据发送
 * ======================================================================== */
// #if BT_MUSIC_SOURCE_SEND_DUMMY
#if 1

static uint8_t sbc_dummy_data[] = {
    0x9C, 0xF9, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xAD, 0x55, 0x55, 0x55, 0x56, 0xAA, 0xAA, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xD5, 0x55, 0x55, 0x55, 0x6A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xAD, 0x55, 0x55, 0x55, 0x56, 0xAA, 0xAA, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xD5, 0x55, 0x55, 0x55, 
    0x9C, 0xF9, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xAD, 0x55, 0x55, 0x55, 0x56, 0xAA, 0xAA, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xD5, 0x55, 0x55, 0x55, 0x6A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xAD, 0x55, 0x55, 0x55, 0x56, 0xAA, 0xAA, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xD5, 0x55, 0x55, 0x55, 
    0x9C, 0xF9, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xAD, 0x55, 0x55, 0x55, 0x56, 0xAA, 0xAA, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xD5, 0x55, 0x55, 0x55, 0x6A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xAD, 0x55, 0x55, 0x55, 0x56, 0xAA, 0xAA, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xD5, 0x55, 0x55, 0x55, 
    0x9C, 0xF9, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xAD, 0x55, 0x55, 0x55, 0x56, 0xAA, 0xAA, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xD5, 0x55, 0x55, 0x55, 0x6A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xAD, 0x55, 0x55, 0x55, 0x56, 0xAA, 0xAA, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xD5, 0x55, 0x55, 0x55
};

static TimerHandle_t s_a2dp_timer = NULL;
static uint8_t s_peer_conidx = 0;

// #include "audio_sbc.h"
extern const uint8_t audio_sbc[];
extern const uint32_t audio_sbc_len;

// SBC frame header sync byte
#define SBC_SYNC_BYTE   0x9C

// SBC frame info (all frames have same size in one file)
static uint16_t sbc_frame_size = 0;
static uint32_t total_frame_count = 0;

// Detect SBC frame size by finding distance between first two sync bytes
static void detect_sbc_frame_size(void)
{
    uint32_t i;
    
    if (sbc_frame_size > 0) {
        return;  // Already detected
    }
    
    // Find first frame
    for (i = 0; i < audio_sbc_len; i++) {
        if (audio_sbc[i] == SBC_SYNC_BYTE) {
            // Find next frame
            for (uint32_t j = i + 1; j < audio_sbc_len; j++) {
                if (audio_sbc[j] == SBC_SYNC_BYTE) {
                    sbc_frame_size = j - i;
                    total_frame_count = audio_sbc_len / sbc_frame_size;
                    CLOG("SBC frame size: %d bytes, total frames: %d", 
                         sbc_frame_size, total_frame_count);
                    return;
                }
            }
            break;
        }
    }
}

void bt_a2dp_source_send_cb(TimerHandle_t time_id)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    static uint32_t current_frame_idx = 0;  // Current frame index
    static uint32_t frames_per_packet = 0;  // Frames per packet (calculated once)
    uint32_t start_offset;
    uint32_t total_size;
    
    // Detect frame size on first call
    if (sbc_frame_size == 0) {
        detect_sbc_frame_size();
        // Calculate max frames that fit in 1021 bytes
        // Also limit max frames to avoid protocol limitations
        if (sbc_frame_size > 0) {
            frames_per_packet = 1021 / sbc_frame_size;
            if (frames_per_packet > 8) {
                frames_per_packet = 8;  // Limit to max 8 frames per packet
            }
            if (frames_per_packet == 0) {
                frames_per_packet = 1;  // At least 1 frame
            }
            CLOG("Frames per packet: %d (packet size: %d bytes)", 
                 frames_per_packet, frames_per_packet * sbc_frame_size);
        }
    }
    
    if(stack_env->bt_music_send_cnt <= 4 && sbc_frame_size > 0)
    {
        // Check if we have enough frames remaining
        if (current_frame_idx + frames_per_packet > total_frame_count) {
            current_frame_idx = 0;  // Loop back to start
        }
        
        // Calculate offset and size (O(1) operation)
        start_offset = current_frame_idx * sbc_frame_size;
        total_size = frames_per_packet * sbc_frame_size;
        
        // Send frames from audio_sbc
        app_a2dp_send_media_to_peer(s_peer_conidx, frames_per_packet, total_size, 
                                     (uint8_t *)&audio_sbc[start_offset]);
        
        // Move to next frame
        current_frame_idx += frames_per_packet;
        
        stack_env->bt_music_send_cnt++;
    }
}

 void bt_a2dp_source_send_dummy_start(uint8_t conidx, uint32_t milli_seconds)
{
    if (s_a2dp_timer == NULL) {
        s_a2dp_timer = btos_timer_creat(TIMER_TYPE_PERIODIC, milli_seconds, bt_a2dp_source_send_cb);
    }
    s_peer_conidx = conidx;
}

static void bt_a2dp_source_send_dummy_stop(void)
{
    if (s_a2dp_timer != NULL) {
        btos_timer_stop(s_a2dp_timer);
        s_a2dp_timer = NULL;
    }
}

#endif  // BT_MUSIC_SOURCE_SEND_DUMMY

/* ========================================================================
 * HFP SOURCE 模式 - 虚拟呼叫
 * ======================================================================== */
#if BT_HFP_SOURCE_SEND_DUMMY

static uint8_t s_hfp_peer_conidx = 0xFF;

static void bt_hfp_source_dummy_call_cb(TimerHandle_t time_id)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    if(hfp_peer_conidx != 0xff)
    {
        ///set codec type
        app_hfp_set_codec_type(s_hfp_peer_conidx, stack_env->bt_call_codec_type);
        ///active call
        app_hfp_call_start(s_hfp_peer_conidx, 0);

        app_bt_set_asic_cvsd_en(BT_USE_ASIC_CVSD);
        #if 0
        ///open sco
        CLOGI("bt_call_codec_type:%d", stack_env->bt_call_codec_type);
        app_hfp_call_add_audio(hfp_peer_conidx, stack_env->bt_call_codec_type);
        #endif
    }
    
    btos_timer_stop(time_id);
}

static void bt_hfp_source_dummy_call_start(uint8_t conidx, uint32_t milli_seconds)
{
    btos_timer_creat(TIMER_TYPE_SINGLE, milli_seconds, bt_hfp_source_dummy_call_cb);
    s_hfp_peer_conidx = conidx;
}

#endif  // BT_HFP_SOURCE_SEND_DUMMY
/* ========================================================================
 * GLOBAL FUNCTIONS - 蓝牙经典模式事件处理
 * ======================================================================== */

void bt_stack_connect_profile_cb(TimerHandle_t time_id)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    uint8_t expected_conidx = s_profile_connect_conidx;
    bool pending = s_profile_connect_pending;

    s_profile_connect_pending = false;
    CLOGD("bt_stack_connect_profile_cb,conidx:0x%x,exp:0x%x,pending:%d",
          stack_env->bt_classic_conidx, expected_conidx, pending);
    if(pending && stack_env->bt_classic_connected &&
       stack_env->bt_open == BT_STATE_OPENED &&
       stack_env->bt_classic_conidx == expected_conidx)
    {
        app_hfp_connect(expected_conidx, BT_STACK_HFP_PEER_ROLE);
    }
}

void bt_stack_connect_profile_by_timer(uint8_t conidx, uint32_t milli_seconds)
{
       s_profile_connect_conidx = conidx;
       s_profile_connect_pending = true;
       TimerHandle_t update_id = btos_timer_creat(TIMER_TYPE_SINGLE, milli_seconds, bt_stack_connect_profile_cb);
}

/**
 * @brief 蓝牙经典模式使能完成
 */
void bt_stack_classic_enable_cmp(uint16_t status)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();


    stack_env->bt_role = BT_STACK_CLASSIC_ROLE;

#if WHITE_LIST_ADD
    bt_stack_ble_add_paired_to_wlist();
#endif
#if RESOVLE_LIST_ADD
    ble_gap_add_paired_rpa_to_rlist();
#endif
#if BT_MUSIC_PRESENT
    // bt_stack_a2dp_caps_set(a2dp_cfg.a2dp_role, &a2dp_caps);
    bt_stack_a2dp_enable(&a2dp_cfg);
#endif
#if BT_CALL_PRESENT
    bt_stack_hfp_enable(&hfp_cfg);
#endif
    if (status == 0) {
        stack_env->bt_open = BT_STATE_OPENED;
    }
    lisa_bluetooth_notify_classic_enabled(status);
    CLOGD("bt classic enable cmp, status:%d", status);
}

///actv         0:stop, 1:start
///resquester   0:auto, 1:user
void bt_stack_bt_register_inquiry_stop_cb(bt_stack_bt_inquiry_stop_cb_t cb)
{
    s_bt_inquiry_stop_cb = cb;
}

void bt_stack_bt_register_connect_fail_cb(bt_stack_bt_connect_fail_cb_t cb)
{
    s_bt_connect_fail_cb = cb;
}

void bt_stack_bt_register_connect_actv_cb(bt_stack_bt_connect_actv_cb_t cb)
{
    s_bt_connect_actv_cb = cb;
}

void bt_stack_bt_register_link_auth_fail_cb(bt_stack_bt_link_auth_fail_cb_t cb)
{
    s_bt_link_auth_fail_cb = cb;
}

void bt_stack_bt_actv_ind(uint8_t actv, uint8_t type, uint8_t actv_id, uint8_t resquester, int16_t status)
{
    switch(type)
    {
        case GAPM_ACTV_TYPE_DISCOVERY :
        {
            CLOGD("bt actv stop:act-type-id:%d-%d-%d,req:%d,sta:0x%x", actv, type, actv_id, resquester, status);
            if (actv == 0 && s_bt_inquiry_stop_cb) {
                s_bt_inquiry_stop_cb(status);
            }
        }
        break;
        case GAPM_ACTV_TYPE_CONNECT:
        {
            CLOGD("bt actv:act-type-id:%d-%d-%d,req:%d,sta:0x%x", actv, type, actv_id, resquester, status);
            if (s_bt_connect_actv_cb) {
                s_bt_connect_actv_cb(actv, status);
            }
            if (status == 0x4c && s_bt_connect_fail_cb) {
                s_bt_connect_fail_cb(actv_id, status);
            }
        }
        break;
        default : break;
    }
}

/**
 * @brief 蓝牙经典连接指示
 */
void bt_stack_classic_conn_ind(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    
    stack_env->bt_classic_connected = 1;
    stack_env->bt_classic_conidx = conidx;
    
#if BT_MUSIC_PRESENT
    stack_env->bt_music_send_cnt = 0;
    stack_env->bt_music_full = 0;
#endif

#if BT_CALL_PRESENT
    stack_env->bt_call_send_cnt = 0;
    stack_env->bt_call_codec_type = BT_STACK_HFP_CODEC_TYPE;
#endif
    
    CLOGD("BT classic connected");
    if (peer_addr) {
        s_classic_peer_addr = *peer_addr;
        s_classic_peer_valid = true;
    }

    CLOGD("BT classic connected,role:%d",bt_gap_get_role(conidx));

    bt_gap_set_sup_timeout(conidx, BT_STACK_LINK_TIMEOUT);

    ///some earphone not give tx credit.
    app_bt_rfcomm_dis_tx_credit_check(1);
    if(bt_gap_get_role(conidx) == BT_ROLE_MASTER)
    {
        ///need connect hfp & a2dp.
        stack_env->bt_init_connect = 1;
        
        bt_gap_auth_req(conidx, GAP_SEC_UNAUTH);
        if(stack_env->bt_role == BT_STACK_CLASSIC_SINK)
        {
            ///switch role to slave.
            bt_gap_set_role(conidx, BT_ROLE_SLAVE);
            //app_hfp_connect(conidx, BT_STACK_HFP_PEER_ROLE);
        }

    }
    else
    {
        ///not connect hfp & a2dp.
        stack_env->bt_init_connect = 0;
        if(stack_env->bt_role == BT_STACK_CLASSIC_SOURCE)
        {
            //bt_gap_auth_req(conidx, GAP_SEC_UNAUTH);
            ///switch role to master.
            //bt_gap_set_role(conidx, BT_ROLE_MASTER);
        }
        bt_classic_scan_enable(BT_GAP_SCAN_DIS);
    }

    lisa_bt_classic_notify_connected(conidx, conhdl, peer_addr);
}

/**
 * @brief 蓝牙经典断开指示
 */
void bt_stack_classic_disc_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    lisa_bt_classic_notify_disconnected(conidx, conhdl, reason);
    uint16_t flags = 0;
    uint8_t disc = 0;
    uint16_t uuid[2];
    uint8_t len = GAP_BD_ADDR_LEN;
    // start adv
    gap_bdaddr_t peer = {0};

    stack_env->bt_classic_connected = 0;
    stack_env->bt_classic_a2dp_connected = 0;
    stack_env->bt_classic_hfp_connected = 0;

    bt_stack_nvs_get(NVS_ID_BT_PEER_ADDRESS, &len, (uint8_t*)&peer);

    CLOGD("DISCONNECT BDADDR: 0x%02x%02x%02x%02x%02x%02x, reason:0x%x", peer.addr[5], peer.addr[4], peer.addr[3], \
            peer.addr[2], peer.addr[1], peer.addr[0], reason);

    if(reason == BT_ERROR_AUTH_FAILURE)
    {
        bt_paired_remove(s_classic_peer_valid ? &s_classic_peer_addr : &peer);
    }
    else
    {
        CLOGD("skip lk save while bt state:%d,conidx:%d,current:%d",
              stack_env->bt_open, conidx, stack_env->bt_classic_conidx);
    }

    if(stack_env->bt_open != BT_STATE_OPENED)
    {
        bt_stack_if_close_discon(0);
    }

#if (BT_STACK_CLASSIC_ROLE == BT_STACK_CLASSIC_SINK)
    bt_classic_scan_enable(3);
#endif

}

/**
 * @brief 蓝牙密钥请求
 */
static void bt_stack_classic_key_req(uint8_t conidx, uint8_t key_type, uint32_t key)
{
    // ble_gap_key_cfm(conidx, 1, 123456);
}

/**
 * @brief 蓝牙配对完成指示
 */
void bt_stack_classic_bond_ind(uint8_t conidx, uint16_t status)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    
    CLOG("bt_stack_classic_bond_ind:idx:%d,sta:0x%x", conidx, status);
    uint8_t info = status >> 8;
    uint8_t value = status & 0xff;

    if (info == GAP_BT_LINK_AUTH_REQ && value == 0) {
        bt_gap_save_lk_mem_to_nvs(conidx);
        if (s_classic_peer_valid) {
            bt_paired_record_update(&s_classic_peer_addr, BT_PAIRED_TRANSPORT_CLASSIC);
        }
    }
    
    switch(info)
    {
        case GAP_BT_LINK_AUTH_REQ :
        {
            if(value == 0)
            {
                //app_a2dp_connect(conidx, a2dp_cfg.a2dp_role);
                if(stack_env->bt_init_connect == 1)
                {
                    #if 0
                    if(stack_env->bt_classic_bond == 1)
                    {
                        /// avoid some earphone l2cap conflict.
                        bt_stack_connect_profile_by_timer(conidx, 700);
                    }
                    else
                    #endif
                    {
                        app_hfp_connect(conidx, BT_STACK_HFP_PEER_ROLE);
                    }
                }
            } else {
                ble_gap_disconnect(conidx, BT_ERROR_AUTH_FAILURE);

                if(value == GAP_PIN_MISSING)
                {                
                    uint8_t len = sizeof(gap_bdaddr_t);
                    gap_bdaddr_t peer = {0};
                    if(bt_stack_nvs_get(NVS_ID_BT_PEER_ADDRESS, &len, (uint8_t*)&peer) == NVDS_OK)
                    {
                        bt_gap_delete_bond(&peer);  //clear only
                        bt_stack_nvs_del(NVS_ID_BT_PEER_ADDRESS);
                    }
                }
                CLOGD("bt link auth failed, reason:0x%x", value);
                if (s_bt_link_auth_fail_cb) {
                    s_bt_link_auth_fail_cb(conidx, value);
                }
            }
        }break;
        case GAP_PAIRING_FAILED :
        {
            ///disconnect
            ble_gap_disconnect(conidx, BT_ERROR_AUTH_FAILURE);

            if(value == GAP_PIN_MISSING)
            {
                uint8_t len = sizeof(gap_bdaddr_t);
                gap_bdaddr_t peer = {0};
                if(bt_stack_nvs_get(NVS_ID_BT_PEER_ADDRESS, &len, (uint8_t*)&peer) == NVDS_OK)
                {
                    bt_gap_delete_bond(&peer);  //clear only
                    bt_stack_nvs_del(NVS_ID_BT_PEER_ADDRESS);
                }
            }
            
            CLOGD("bt pairing failed, reason:0x%x", value);
        }break;
        case GAP_LK_EXCH :
        {
            if(value == 0)
            {
                stack_env->bt_classic_bond = 1;
                CLOGI("lk save suc");
            }
        }break;
        default : break;
    }
}

/**
 * @brief 蓝牙信息指示
 */
void bt_stack_classic_info_ind(uint8_t conidx, uint8_t type, ble_info_data_t *data)
{

}

void bt_stack_sniff_change_ind(uint8_t conidx, uint8_t status, uint8_t mode, uint16_t interval)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    CLOGD("bt_stack_sniff_change_ind,idx:%d,sta:0x%x,mode:%d,int:%d", conidx, status, mode, interval);
    if(stack_env->bt_classic_connected && stack_env->bt_open == BT_STATE_OPENED)
    {
        if(status == 0)
        {
            stack_env->bt_classic_sniff_mode = mode;
        }
    }
}

/* ========================================================================
 * A2DP 回调实现
 * ======================================================================== */
#if BT_MUSIC_PRESENT

static void a2dp_enable_cmp(uint16_t status)
{
    CLOGI("a2dp enable cmp!");
}

static void a2dp_revoke_cmp(uint16_t status)
{
    CLOGI("a2dp revoke cmp!");
}

static void a2dp_connect_cmp(uint8_t conidx, uint16_t status)
{
    CLOGI("a2dp connect cmp! conidx=%d, status=0x%x", conidx, status);
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    static uint8_t error_cnt = 0;
    if(stack_env->bt_classic_connected && stack_env->bt_open == BT_STATE_OPENED)
    {
        if(status == 0)
        {
            stack_env->bt_classic_a2dp_connected = 1;
        }
        else
        {
            error_cnt++;
            if(error_cnt <= 2)
            {
                app_a2dp_connect(conidx, a2dp_cfg.a2dp_role);
            }
            else
            {
                ble_gap_disconnect(conidx, BT_ERROR_REMOTE_USER_TERM_CON);
            }
        }
    }

    if (stack_env->bt_role == BT_STACK_CLASSIC_SOURCE && stack_env->bt_classic_a2dp_connected == 1)
    {
#if BT_MUSIC_SOURCE_SEND_DUMMY
        app_a2dp_start(conidx);
#else
        bt_stack_a2dp_connection_update(conidx, true);
#endif
    }

    lisa_bt_classic_notify_profile(conidx, LISA_BT_PROFILE_A2DP, true);
}

static void a2dp_disconnect_cmp(uint8_t conidx, uint16_t status)
{
    CLOGI("a2dp disconnect cmp! conidx=%d, status=0x%x", conidx, status);

#ifdef CONFIG_BT_CLASSIC_ROLE_SOURCE
    bt_stack_a2dp_connection_update(conidx, false);
#endif

    lisa_bt_classic_notify_profile(conidx, LISA_BT_PROFILE_A2DP, false);
}

static void a2dp_start_ind(uint8_t conidx, uint8_t codec, uint8_t ch, uint16_t sample_rate)
{
    CLOGI("a2dp start! codec:%d, ch:%d, sample_rate:%d", codec, ch, sample_rate);
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    if(stack_env->bt_role == BT_STACK_CLASSIC_SOURCE)
    {
        if(stack_env->bt_classic_sniff_mode == BT_SNIFF_MODE)
        {
            bt_gap_exit_sniff(conidx);
        }
    }

#if BT_MUSIC_SOURCE_SEND_DUMMY
    bt_a2dp_source_send_dummy_start(conidx, 20);
#else
    bt_stack_a2dp_send_start(conidx, codec, ch, sample_rate);
#endif

    if(stack_env->bt_role == BT_STACK_CLASSIC_SOURCE)
    {
        app_avrcp_play_status_set(conidx, BT_AVRCP_PLAYBACK_STATUS_PLAYING);
    }


}

static void a2dp_stop_ind(uint8_t conidx, uint8_t status)
{
    CLOGI("a2dp stop!, conidx:%d, sta:0x%x", conidx, status);
    
#if BT_MUSIC_SOURCE_SEND_DUMMY
    bt_a2dp_source_send_dummy_stop();
#else
    bt_stack_a2dp_send_stop(conidx, status);
#endif

#if (BT_STACK_CLASSIC_ROLE == BT_STACK_CLASSIC_SOURCE)
    app_avrcp_play_status_set(conidx, BT_AVRCP_PLAYBACK_STATUS_PAUSED);
#endif
}

static void a2dp_media_ind(uint8_t conidx, uint8_t frame_num, uint16_t seq, uint16_t len, uint8_t *data)
{
    //uint32_t debug_data = data[len - 3] | (data[len - 2] << 8) | (data[len - 1] << 16) | (data[len] << 24);
    //CLOGI("s:%d,0x%x", seq, debug_data);
    //if(seq | 0x1f == 0)
    //{
        //CLOGI("s:%d", seq);
    //}
    bt_stack_a2dp_send_data(conidx, frame_num, seq, len, data);

}

static void a2dp_media_rsp(uint8_t conidx, uint8_t *data, uint16_t status)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    uint8_t stage = status >> 15;
    uint16_t send_status = status & 0x7f;

    ///stage 0: push data to l2cap, 1:push data to controller.
    if(stage == 1)
    {
        bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
        if(stack_env->bt_music_send_cnt > 0)
        {
            stack_env->bt_music_send_cnt--;
        }
        /// to do send message to app task release buf
        bt_audio_adapter_a2dp_media_rsp(conidx, data, status);
        if(send_status == 0)
        {
            ///not wait timer, send data rigt now.
            if(stack_env->bt_music_full == 1)
            {
                stack_env->bt_music_full = 0;
            }
        }
    }

    if(send_status != 0)
    {
        CLOGI("a2dp media rsp,data:0x%x,sta:0x%x,cnt:%d", data, status, stack_env->bt_music_send_cnt);
    }
}

/* ========================================================================
 * AVRCP 回调实现
 * ======================================================================== */

static void avrcp_connect_cmp(uint8_t conidx, uint16_t status)
{
    CLOGI("avrcp connect cmp!,conidx:0x%x, status:0x%x", conidx, status);
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    if(stack_env->bt_classic_connected && stack_env->bt_open == BT_STATE_OPENED)
    {
        if(stack_env->bt_role == BT_STACK_CLASSIC_SOURCE)
        {
            uint16_t version;
#if(BT_HFP_SOURCE_SEND_DUMMY)
            bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

            //app_hfp_connect(conidx, BT_STACK_HFP_PEER_ROLE);
#if 0
            app_hfp_set_status(conidx, BT_HF_SERVICE_IND, 1);
            app_hfp_set_status(conidx, BT_HF_BATTCHG_IND, 5);
            app_hfp_set_status(conidx, BT_HF_SIGNAL_IND, 5);
            app_hfp_set_status(conidx, BT_HF_ROAM_IND, 0);
            app_hfp_set_status(conidx, BT_HF_RING_INBAND, 0);
            app_hfp_set_status(conidx, BT_HF_CME_ERROR, 0);
#endif
#endif
            app_avrcp_get_peer_version(conidx, &version);

            if(version >= 0x106)
            {
                app_avrcp_notify_req(conidx, BT_AVRCP_NOTIFI_VOLUME_CHANGED);
            }
        }
    }
}

static void avrcp_disconnect_cmp(uint8_t conidx, uint16_t status)
{
    CLOGI("avrcp disconnect cmp!");
}

static void avrcp_avrcp_press_cmp(uint8_t conidx, uint16_t status)
{
    CLOGI("avrcp press cmp!");
}

static void avrcp_notify_cmp(uint8_t conidx, uint16_t status)
{
    CLOGI("avrcp notify cmp!");
}

static void avrcp_media_cmp(uint8_t conidx, uint16_t status)
{
    CLOGI("avrcp media cmp!");
}

static void avrcp_avrcp_press_ind(uint8_t conidx, uint8_t key_type, uint8_t key_id, uint8_t key_value)
{
    CLOGI("avrcp press ind,idx:%d,key:0x%x-0x%x-0x%x", conidx, key_type, key_id, key_value);
    if (key_type == BT_AVRCP_KEY_RELEASED) {
        lisa_bt_classic_notify_avrcp_key(conidx, key_id);
    }
    switch(key_id)
    {
        case BT_AVRCP_KEY_ID_PAUSE :
            {
                if(key_type == BT_AVRCP_KEY_PRESSED)
                {

                }
                else
                {
                    app_avrcp_play_status_set(conidx, BT_AVRCP_PLAYBACK_STATUS_PAUSED);
                }
            }
            break;
        case BT_AVRCP_KEY_ID_PLAY :
            {
                if(key_type == BT_AVRCP_KEY_PRESSED)
                {
                
                }
                else
                {
                    app_avrcp_play_status_set(conidx, BT_AVRCP_PLAYBACK_STATUS_PLAYING);
                }
            }
            break;
        case BT_AVRCP_KEY_ID_STOP :
            {

            }
            break;
        case BT_AVRCP_KEY_ID_MUTE :
            {

            }
            break;
        case BT_AVRCP_KEY_ID_VOLUME_UP :
        case BT_AVRCP_KEY_ID_VOLUME_DOWN :
            {

            }
            break;
        default : break;
    }
}

static void avrcp_notify_ind(uint8_t conidx, uint8_t c_r, uint8_t event_id, uint8_t event_value)
{
    CLOGI("avrcp notify ind,c_r:%d,event_id:0x%x-0x%x", c_r, event_id, event_value);
    lisa_bt_classic_notify_avrcp_event(conidx, c_r, event_id, event_value);
    if(c_r == 1)//response
    {
        switch(event_id)
        {
            case BT_AVRCP_NOTIFI_VOLUME_CHANGED : 
                {
                    CLOGI("volume changed:0x%x", event_value);
                }
                break;
            default : break;
        }
    }
}

static void avrcp_media_ind(uint8_t conidx, uint8_t status)
{
    CLOGI("avrcp media ind!");
}
#endif  // BT_MUSIC_PRESENT

/* ========================================================================
 * HFP 回调实现
 * ======================================================================== */
#if BT_CALL_PRESENT
static void hfp_enable_cmp(uint16_t status)
{
    CLOGD("hfp enable cmp,status:0x%x", status);
}

static void hfp_disable_cmp(uint16_t status)
{
    CLOGD("hfp disable cmp,status:0x%x", status);
}

static void hfp_connect_cmp(uint8_t conidx, uint8_t type, uint16_t status)
{
    static uint8_t error_cnt = 0;
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    CLOGD("hfp connect cmp,conidx:%d,init:%d,type:%d,status:0x%x,err:%d", conidx, stack_env->bt_init_connect, type, status, error_cnt);

    if(stack_env->bt_open != BT_STATE_OPENED ||
       !stack_env->bt_classic_connected ||
       stack_env->bt_classic_conidx != conidx)
    {
        CLOGD("drop hfp connect cmp,state:%d,connected:%d,current:%d",
              stack_env->bt_open, stack_env->bt_classic_connected, stack_env->bt_classic_conidx);
        return;
    }

    if(stack_env->bt_init_connect == 1)
    {
        if(status == 0)
        {
            app_a2dp_connect(conidx, a2dp_cfg.a2dp_role);
            error_cnt = 0;
            stack_env->bt_classic_hfp_connected = 1;
        }
        else
        {
            if(error_cnt++ >= 2)
            {
                /// hfp connect failed,try to connect a2dp;
                app_a2dp_connect(conidx, a2dp_cfg.a2dp_role);
                error_cnt = 0;
            }
            else
            {
                /// hfp connect failed,try to connect hfp again;
                app_hfp_connect(conidx, BT_STACK_HFP_PEER_ROLE);
            }
        }
    }
    else
    {
        if(status != 0)
        {
            ///retry 2
            if(error_cnt++ <= 2)
            {
                /// hfp connect failed,try to connect hfp again;
                app_hfp_connect(conidx, BT_STACK_HFP_PEER_ROLE);
            }
            else
            {
                ble_gap_disconnect(conidx, BT_ERROR_REMOTE_USER_TERM_CON);
                error_cnt = 0;
            }
        }
        else
        {
            error_cnt = 0;
        }
    }

#if BT_HFP_SOURCE_SEND_DUMMY
    ///incomming call
    app_hfp_call_req(conidx, 0, BT_HF_CALL_INCOMMING);
    bt_hfp_source_dummy_call_start(conidx, 6000);
#else
    bt_stack_hfp_connection_update(conidx, true);
#endif

    lisa_bt_classic_notify_profile(conidx, LISA_BT_PROFILE_HFP, true);
}

static void hfp_disconnect_cmp(uint8_t conidx, uint16_t status)
{
    CLOGD("hfp disconnect cmp,conidx:%d, status:0x%x", conidx, status);

#ifdef CONFIG_BT_CLASSIC_ROLE_SOURCE
    bt_stack_hfp_connection_update(conidx, false);
#endif

    lisa_bt_classic_notify_profile(conidx, LISA_BT_PROFILE_HFP, false);
}

void hfp_aud_start_ind(uint8_t conidx, uint16_t codec, uint16_t status)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    CLOGD("hfp aud start! conidx:0x%x,codec:%d-%d,status:0x%x",conidx, codec,stack_env->bt_call_codec_type, status);
    if(status == 0)
    {
        if(stack_env->bt_classic_sniff_mode == BT_SNIFF_MODE)
        {
            bt_gap_exit_sniff(conidx);
        }
        bt_stack_hfp_send_start(conidx, stack_env->bt_call_codec_type);
    }
}

void hfp_aud_stop_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    CLOGD("hfp stop!, conidx:%d, reason:0x%x", conidx, reason);
    bt_stack_hfp_send_stop(conidx, (uint8_t)reason);
}

/**
 * @brief 从对端接收 SCO 音频数据
 */
static void hfp_receive_media_from_peer(uint8_t conidx, uint8_t pkt_sta, uint16_t len, uint8_t *data)
{
    // 让 bt_audio_adapter 处理 HFP 音频数据
    bt_stack_hfp_send_data(conidx, pkt_sta, len, data);
    
#if BT_CALL_SCO_SEND_DUMMY
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    if(stack_env->bt_call_send_cnt <= 4)
    {
        static uint8_t trans_buf[120];
        if(pkt_sta == 0)
        {
            memcpy(trans_buf, data, len);
        }
        else
        {
            // memset(trans_buf, 0x55*(1-0), len);
        }
        app_hfp_send_aud_to_peer(conidx, len, trans_buf);
        stack_env->bt_call_send_cnt++;
    }
#endif
}

/**
 * @brief HFP 音频发送完成回调
 */
static void hfp_send_media_cmp(uint8_t conidx, uint8_t status, uint8_t *data)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    if(stack_env->bt_call_send_cnt > 0)
    {
        stack_env->bt_call_send_cnt--;
    }
}

static void hfp_status_ind(uint8_t conidx, uint8_t req_type, uint8_t status_type, uint16_t val)
{
    CLOGI("hfp_status_ind,idx:0x%x,type:0x%x-0x%x,val:0x%x", conidx, req_type, status_type, val);
    if(req_type == BT_HFP_STATUS_IND)
    {
        switch(status_type)
        {
            case BT_HF_CODEC_TYPE :
                bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
                stack_env->bt_call_codec_type = val & 0xff;
                uint8_t cmd_type = (val >> 8) & 0xff;
                ///codec feature exchange
                if(cmd_type == BT_BAC_HF_CODEC_TYPE)
                {
                    ///sco codec feature exchange.
                    CLOGI("hfp codec feature:%d", stack_env->bt_call_codec_type);
                }
                else ///codec type select
                {
#if 1
                    app_hfp_call_add_audio(conidx, stack_env->bt_call_codec_type);
#endif
                }
                break;
            case BT_HF_NREC_CFG :
                break;
            default : break;
        }
    }
}

static void hfp_call_ind(uint8_t conidx, uint8_t req_type, uint8_t call_type, uint8_t call_idx)
{
    CLOGI("hfp_call_ind,idx:0x%x,req_type:0x%x,call:0x%x-%d", conidx, req_type, call_type, call_idx);
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    if(req_type == BT_HFP_CALL_IND)
    {
        switch(call_type)
        {
            ///peer hang up call
            case BT_HF_CALL_RELEASE :
                app_hfp_call_remove_audio(conidx, 0x13);
                break;
            case BT_HF_CALL_ACTIVE :
                break;
            ///peer answer call
            case BT_HF_CALL_ACCEPT :
                {
                    ///set codec type
                    app_hfp_set_codec_type(conidx, BT_STACK_HFP_CODEC_TYPE);
                    ///active call
                    app_hfp_call_start(conidx, 0);

                    app_bt_set_asic_cvsd_en(BT_USE_ASIC_CVSD);
                    ///open sco
                    app_hfp_call_add_audio(conidx, stack_env->bt_call_codec_type);
                }
                break;
            default : break;
        }

    }
    else if(req_type == BT_HFP_VR_REQ)
    {
        switch(call_type)
        {
            case BT_HF_VR_START :
                break;
            case BT_HF_VR_STOP :
                break;
            default : break;
        }
    }
}

#endif  // BT_CALL_PRESENT

#endif  // BT_STACK_PRESENT
