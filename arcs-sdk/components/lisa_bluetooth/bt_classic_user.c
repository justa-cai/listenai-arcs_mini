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

#if BT_STACK_PRESENT
#include "bt_a2dp.h"
#include "bt_avrcp.h"
#include "bt_music_hal.h"
#include "bt_hfp.h"
#include "bt_call_hal.h"

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
static void hfp_send_media_cmp(uint8_t conidx, uint16_t status, uint8_t *data);
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
        ///open sco
        app_hfp_call_add_audio(s_hfp_peer_conidx, stack_env->bt_call_codec_type);
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

/**
 * @brief 蓝牙经典模式使能完成
 */
void bt_stack_classic_enable_cmp(uint16_t status)
{
    CLOGD("bt classic enable cmp, status:%d", status);
    
#if WHITE_LIST_ADD
    bt_stack_ble_add_paired_to_wlist();
#endif
#if RESOVLE_LIST_ADD
    ble_gap_add_paired_rpa_to_rlist();
#endif

#if BT_MUSIC_PRESENT
    bt_stack_a2dp_enable(&a2dp_cfg);
#endif
#if BT_CALL_PRESENT
    bt_stack_hfp_enable(&hfp_cfg);
#endif
}

/**
 * @brief 蓝牙经典连接指示
 */
void bt_stack_classic_conn_ind(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    
    stack_env->bt_classic_connected = 1;
    
#if BT_MUSIC_PRESENT
    stack_env->bt_music_send_cnt = 0;
#endif

#if BT_CALL_PRESENT
    stack_env->bt_call_send_cnt = 0;
    stack_env->bt_call_codec_type = BT_STACK_HFP_CODEC_TYPE;
#endif
    
    CLOGD("BT classic connected");
    
#ifdef CONFIG_BT_CLASSIC_ROLE_SOURCE
    bt_gap_auth_req(conidx, 2);
#else
    bt_classic_scan_enable(BT_GAP_SCAN_DIS);
#endif
    bt_stack_nvs_set(NVS_ID_BT_PEER_ADDRESS, GAP_BD_ADDR_LEN, peer_addr->addr);
}

/**
 * @brief 蓝牙经典断开指示
 */
void bt_stack_classic_disc_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    stack_env->bt_classic_connected = 0;
    uint16_t flags = 0;
    uint8_t disc = 0;
    uint16_t uuid[2];
    uint8_t len = GAP_BD_ADDR_LEN;
    // start adv
    gap_bdaddr_t peer = {0};

    bt_stack_nvs_get(NVS_ID_BT_PEER_ADDRESS, &len, peer.addr);

    CLOGD("DISCONNECT BDADDR: 0x%02x%02x%02x%02x%02x%02x, reason:0x%x", peer.addr[5], peer.addr[4], peer.addr[3], \
            peer.addr[2], peer.addr[1], peer.addr[0], reason);

    bt_gap_save_lk_mem_to_nvs(conidx);

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
    CLOG("bt_stack_classic_bond_ind:idx:%d,sta:0x%x", conidx, status);
    uint8_t info = status >> 8;
    uint8_t value = status & 0xff;
    
#ifdef CONFIG_BT_CLASSIC_ROLE_SOURCE
    switch(info)
    {
        case GAP_BT_LINK_AUTH_REQ : 
        {
            if(value == 0)
            {
                app_hfp_connect(conidx, BT_STACK_HFP_PEER_ROLE);
            }
        }break;
        case GAP_PAIRING_FAILED : 
        {

        }break;
        default : break;
    }
#endif
}

/**
 * @brief 蓝牙信息指示
 */
void bt_stack_classic_info_ind(uint8_t conidx, uint8_t type, ble_info_data_t *data)
{

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
    
#ifdef CONFIG_BT_CLASSIC_ROLE_SOURCE
#if BT_MUSIC_SOURCE_SEND_DUMMY
    app_a2dp_start(conidx);
#else
    bt_stack_a2dp_connection_update(conidx, true);
#endif
#endif
}

static void a2dp_disconnect_cmp(uint8_t conidx, uint16_t status)
{
    CLOGI("a2dp disconnect cmp! conidx=%d, status=0x%x", conidx, status);
    
#ifdef CONFIG_BT_CLASSIC_ROLE_SOURCE
    bt_stack_a2dp_connection_update(conidx, false);
#endif
}

static void a2dp_start_ind(uint8_t conidx, uint8_t codec, uint8_t ch, uint16_t sample_rate)
{
    CLOGI("a2dp start! codec:%d, ch:%d, sample_rate:%d", codec, ch, sample_rate);
#if BT_MUSIC_SOURCE_SEND_DUMMY
    bt_a2dp_source_send_dummy_start(conidx, 20);
#else
    bt_stack_a2dp_send_start(conidx, codec, ch, sample_rate);
#endif

#if (BT_STACK_CLASSIC_ROLE == BT_STACK_CLASSIC_SOURCE)
    app_avrcp_play_status_set(conidx, BT_AVRCP_PLAYBACK_STATUS_PLAYING);
#endif

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

    if (stage == 1)
    {
        if(stack_env->bt_music_send_cnt > 0)
        {
            stack_env->bt_music_send_cnt--;
        }
        bt_stack_a2dp_send_media_rsp(conidx);
    }
    else
    {

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
    app_avrcp_press_req(conidx, 0, BT_AVRCP_PRESS_ID_SET_ABSOLUTE_VOLUME, 0x40);
#ifdef CONFIG_BT_CLASSIC_ROLE_SOURCE
    // app_hfp_connect(conidx, BT_STACK_HFP_PEER_ROLE);
#endif
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
    CLOGD("hfp connect cmp,conidx:%d, type:%d, status:0x%x", conidx, type, status);
    
#ifdef CONFIG_BT_CLASSIC_ROLE_SOURCE
    app_a2dp_connect(conidx, a2dp_cfg.a2dp_role);
#if BT_HFP_SOURCE_SEND_DUMMY
    if(status == 0)
    {
        ///incomming call
        app_hfp_call_req(conidx, 0, BT_HF_CALL_INCOMMING);
        bt_hfp_source_dummy_call_start(conidx, 6000);
    }
#else
    bt_stack_hfp_connection_update(conidx, true);
#endif
#endif
}

static void hfp_disconnect_cmp(uint8_t conidx, uint16_t status)
{
    CLOGD("hfp disconnect cmp,conidx:%d, status:0x%x", conidx, status);
    
#ifdef CONFIG_BT_CLASSIC_ROLE_SOURCE
    bt_stack_hfp_connection_update(conidx, false);
#endif
}

void hfp_aud_start_ind(uint8_t conidx, uint16_t codec, uint16_t status)
{
    CLOGD("hfp start! conidx:0x%x,codec:%d,status:0x%x",conidx, codec, status);
    if(status == 0)
    {
        bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

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
static void hfp_send_media_cmp(uint8_t conidx, uint16_t status, uint8_t *data)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    if(stack_env->bt_call_send_cnt > 0)
    {
        stack_env->bt_call_send_cnt--;
    }
}

static void hfp_status_ind(uint8_t conidx, uint8_t req_type, uint8_t status_type, uint16_t val)
{
    CLOGI("hfp_status_ind,idx:0x%x,type:0x%x-0x%x,val:%d", conidx, req_type, status_type, val);
    if(req_type == BT_HFP_STATUS_IND)
    {
        switch(status_type)
        {
            case BT_HF_CODEC_TYPE :
                bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
                stack_env->bt_call_codec_type = val;
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

