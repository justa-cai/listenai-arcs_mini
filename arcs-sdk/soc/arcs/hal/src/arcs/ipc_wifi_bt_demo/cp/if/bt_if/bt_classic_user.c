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
#include "bt_classic_hal.h"

#include "bt_stack_cfg.h"
#include "bt_stack_hal.h"

#if BT_STACK_PRESENT
#include "bt_a2dp.h"
#include "bt_avrcp.h"
#include "bt_music_hal.h"
#include "bt_hfp.h"
#include "bt_call_hal.h"
#include "btos_al.h"

/*
 * DEFINES
 ****************************************************************************************
 */
 
#define BT_STACK_HFP_MSBC_SUPPORT        (1)

#define BT_STACK_HFP_CODEC_TYPE    (BT_STACK_HFP_MSBC_SUPPORT + HFP_MEDIA_CODEC_CVSD)


#define BT_SEARCH_PERR_BY_TYPE   (1)  //1:name,2:addr 3:name&addr
#define PEER_DEV_NAME0           "sanag S8S AI"
//#define PEER_DEV_NAME0           "S66+"
//#define PEER_DEV_NAME0           "X3M"
//#define PEER_DEV_NAME0           "Redmi AirDots 3 Pro"
#define PEER_DEV_NAME1             "Redmi Buds 4 Lite"
//#define PEER_DEV_NAME1           "picun H1"
//#define PEER_DEV_NAME1             "LaoZhu"
//#define PEER_DEV_NAME1           "AirPods Pro - Find My"
//#define PEER_DEV_NAME1           "sanag S8S AI"
//#define PEER_DEV_NAME1           "soundcore C40i"
//#define PEER_DEV_NAME1           "One's Xiaomi Buds 5"
//#define PEER_DEV_NAME1           "HUAWEI FreeBuds 4E"
//#define PEER_DEV_NAME0           "AirPods"
//#define PEER_DEV_NAME1           "BT_SINK"
const gap_addr_t peer_dev_addr = { .addr = {0x70, 0xa5, 0x20, 0x14, 0x63, 0xc8}};

#if (BT_STACK_CLASSIC_ROLE == BT_STACK_CLASSIC_SOURCE)
#define BT_STACK_A2DP_ROLE               (BT_A2DP_SOURCE)
#define BT_STACK_HFP_ROLE                (BT_HFP_ROLE_HF_AG)
#define BT_STACK_HFP_FEATS               ((BT_HFP_AGSF_WBS * BT_STACK_HFP_MSBC_SUPPORT)|BT_HFP_AGSF_NREC)//(BT_HFP_AGSF_WBS|BT_HFP_AGSF_IN_BAND_RING|BT_HFP_AGSF_NREC)

#define BT_STACK_HFP_PEER_ROLE           (BT_HFP_ROLE_HF)

#define BT_MUSIC_SOURCE_SEND_DUMMY    (1)
#define BT_HFP_SOURCE_SEND_DUMMY      (0)

#if(BT_HFP_SOURCE_SEND_DUMMY && BT_MUSIC_SOURCE_SEND_DUMMY)
    #error Only one dummy can be set 1!
#endif


#else
#define BT_STACK_A2DP_ROLE               (BT_A2DP_SINK)
#define BT_STACK_HFP_ROLE                (BT_HFP_ROLE_HF)
#define BT_STACK_HFP_FEATS               ((BT_HFP_AGSF_WBS * BT_STACK_HFP_MSBC_SUPPORT)|BT_HFP_HFSF_VOL_CTL|BT_HFP_HFSF_NREC)

#define BT_STACK_HFP_PEER_ROLE           (BT_HFP_ROLE_HF_AG)

#define BT_MUSIC_SOURCE_SEND_DUMMY    (0)
#define BT_HFP_SOURCE_SEND_DUMMY      (0)
#endif


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

#if BT_MUSIC_PRESENT

const bt_a2dp_cfg_t a2dp_cfg = {
    .a2dp_role = BT_STACK_A2DP_ROLE,
    .aac_support = 0,
};

const bt_a2dp_meida_caps_cfg_t a2dp_caps = {
    .sbc_sample_freq = BT_SBC_SAMPLING_FREQ_16000 + BT_SBC_SAMPLING_FREQ_32000 + BT_SBC_SAMPLING_FREQ_44100 + BT_SBC_SAMPLING_FREQ_48000,
    .sbc_ch_mode = BT_SBC_CHANNEL_MODE_MONO + BT_SBC_CHANNEL_MODE_DUAL_CHAN + BT_SBC_CHANNEL_MODE_STEREO + BT_SBC_CHANNEL_MODE_JOINT_STEREO,
    .sbc_block_length = BT_SBC_BLOCK_LENGTH_4 + BT_SBC_BLOCK_LENGTH_8 + BT_SBC_BLOCK_LENGTH_12 + BT_SBC_BLOCK_LENGTH_16,
    .sbc_subband = BT_SBC_SUBBANDS_4 + BT_SBC_SUBBANDS_8,
    .sbc_allocation = BT_SBC_ALLOCATION_SNR + BT_SBC_ALLOCATION_LOUDNESS,
    .sbc_bitpool_min = BT_SBC_BITPOOL_MIN,
    .sbc_bitpool_max = BT_SBC_BITPOOL_MEDIUM_QUALITY,

    .aac_object = BT_AAC_OBJECT_MPEG2_AAC_LC + BT_AAC_OBJECT_MPEG4_AAC_LC,
    .aac_sample_freq1 = BT_AAC_SAMPLING_FREQ_16000 + BT_AAC_SAMPLING_FREQ_22050 + BT_AAC_SAMPLING_FREQ_24000 + BT_AAC_SAMPLING_FREQ_32000 + BT_AAC_SAMPLING_FREQ_44100,
    .aac_sample_freq2 = BT_AAC_SAMPLING_FREQ_48000,
    .aac_ch_num = BT_AAC_NUM_CHANNELS_1 + BT_AAC_NUM_CHANNELS_2,
    .aac_vbr = BT_AAC_VBR,
    .aac_max_bitrate = 320000,
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
#if BT_CALL_PRESENT
#define BT_CALL_SCO_SEND_DUMMY       (1)

const bt_hfp_cfg_t hfp_cfg = {
    .hfp_role = BT_STACK_HFP_ROLE,
    .hfp_feats = BT_STACK_HFP_FEATS,
};

const bt_hfp_cb_t bt_hfp_cb = {
    .cb_hfp_enable_cmp         = hfp_enable_cmp,
    .cb_hfp_disable_cmp        = hfp_disable_cmp,
    .cb_hfp_con_cmp            = hfp_connect_cmp,
    .cb_hfp_discon_cmp         = hfp_disconnect_cmp,
    ///register callback in bt stack if.
    .cb_hfp_aud_start_ind      = NULL,//hfp_aud_start_ind,
    .cb_hfp_aud_stop_ind       = NULL,//hfp_aud_stop_ind,
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

/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */
#if BT_MUSIC_SOURCE_SEND_DUMMY
uint8_t sbc_dummy_data[] = 
{
    0x9C, 0xF9, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xAD, 0x55, 0x55, 0x55, 0x56, 0xAA, 0xAA, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xD5, 0x55, 0x55, 0x55, 0x6A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xAD, 0x55, 0x55, 0x55, 0x56, 0xAA, 0xAA, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xD5, 0x55, 0x55, 0x55, 
    0x9C, 0xF9, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xAD, 0x55, 0x55, 0x55, 0x56, 0xAA, 0xAA, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xD5, 0x55, 0x55, 0x55, 0x6A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xAD, 0x55, 0x55, 0x55, 0x56, 0xAA, 0xAA, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xD5, 0x55, 0x55, 0x55, 
    0x9C, 0xF9, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xAD, 0x55, 0x55, 0x55, 0x56, 0xAA, 0xAA, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xD5, 0x55, 0x55, 0x55, 0x6A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xAD, 0x55, 0x55, 0x55, 0x56, 0xAA, 0xAA, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xD5, 0x55, 0x55, 0x55, 
    0x9C, 0xF9, 0x21, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xAD, 0x55, 0x55, 0x55, 0x56, 0xAA, 0xAA, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xD5, 0x55, 0x55, 0x55, 0x6A, 0xAA, 0xAA, 0xAA, 0xB5, 0x55, 0x55, 0x55, 0x5A, 0xAA, 0xAA, 0xAA, 0xAD, 0x55, 0x55, 0x55, 0x56, 0xAA, 0xAA, 0xAA, 0xAB, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA, 0xAA, 0xD5, 0x55, 0x55, 0x55
};

TimerHandle_t time_handle_id = NULL;
uint8_t peer_conidx;

void bt_a2dp_source_send_cb(TimerHandle_t time_id)
{
    //static uint32_t time1 = 0, time2 = 0, time3 = 0;
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    if(stack_env->bt_music_send_cnt <= 4)
    {
        app_a2dp_send_media_to_peer(peer_conidx, 4, sizeof(sbc_dummy_data), sbc_dummy_data);
        stack_env->bt_music_send_cnt++;
    }
    #if 1
    static uint32_t g_count = 0;
    g_count++;
    if((g_count % 256) == 0)
    {
        CLOGD("sbc dummy send:%d-%d", stack_env->bt_music_send_cnt, g_count);
        if(g_count == 50)
        {
            app_avrcp_play_status_set(peer_conidx, BT_AVRCP_PLAYBACK_STATUS_PLAYING);
        }
    }
    #endif
}

void bt_a2dp_source_send_dummy_start(uint8_t conidx, uint32_t milli_seconds)
{
    CLOGD("bt_a2dp_source_send_dummy_start");
    if(time_handle_id == NULL)
    {
       time_handle_id = btos_timer_creat(TIMER_TYPE_PERIODIC, milli_seconds, bt_a2dp_source_send_cb);
    }
    peer_conidx = conidx;
}

void bt_a2dp_source_send_dummy_stop(void)
{
    CLOGD("bt_a2dp_source_send_dummy_stop");
    btos_timer_cancel(time_handle_id);
    time_handle_id = NULL;
}

#endif

#if BT_HFP_SOURCE_SEND_DUMMY
TimerHandle_t hfp_time_handle_id = NULL;
uint8_t hfp_peer_conidx = 0xff;

void bt_hfp_source_dummy_call_cb(TimerHandle_t time_id)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    if(hfp_peer_conidx != 0xff)
    {
        ///set codec type
        app_hfp_set_codec_type(hfp_peer_conidx, BT_STACK_HFP_CODEC_TYPE);
        ///active call
        app_hfp_call_start(hfp_peer_conidx, 0);

        app_bt_set_asic_cvsd_en(BT_USE_ASIC_CVSD);
        #if 0
        ///open sco
        CLOGI("bt_call_codec_type:%d", stack_env->bt_call_codec_type);
        app_hfp_call_add_audio(hfp_peer_conidx, stack_env->bt_call_codec_type);
        #endif
    }

    btos_timer_cancel(time_id);
}

void bt_hfp_source_dummy_call_start(uint8_t conidx, uint32_t milli_seconds)
{
    btos_timer_creat(TIMER_TYPE_SINGLE, milli_seconds, bt_hfp_source_dummy_call_cb);
    hfp_peer_conidx = conidx;
}

void bt_hfp_source_send_data_cb(TimerHandle_t time_id)
{
    //static uint32_t time1 = 0, time2 = 0, time3 = 0;
    ///dummy data send 
    
    static uint8_t trans_buf[120];
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    if(stack_env->bt_call_send_cnt <= 4)
    {
        //memcpy(trans_buf, data, len);
        memset(trans_buf, 0x55*(1-BT_USE_ASIC_CVSD), 60);
        app_hfp_send_aud_to_peer(hfp_peer_conidx, 60, trans_buf);
        stack_env->bt_call_send_cnt++;
    }
    #if 1
    static uint32_t g_count = 0;
    g_count++;
    if((g_count % 256) == 0)
    {
        CLOGD("hfp dummy send:%d-%d", stack_env->bt_call_send_cnt, g_count);
        CLOGD("d:0x%02x%02x%02x%02x%02x%02x%02x%02x", trans_buf[0], trans_buf[1], trans_buf[2], trans_buf[3], \
            trans_buf[4], trans_buf[5], trans_buf[6], trans_buf[7]);
    }
    #endif
}

void bt_hfp_source_send_dummy_data_start(uint8_t conidx, uint32_t milli_seconds)
{
    CLOGD("bt_hfp_source_send_dummy_data_start");
    if(hfp_time_handle_id == NULL)
    {
       hfp_time_handle_id = btos_timer_creat(TIMER_TYPE_PERIODIC, milli_seconds, bt_hfp_source_send_data_cb);
    }
    hfp_peer_conidx = conidx;
}

void bt_hfp_source_send_dummy_data_stop(void)
{
    CLOGD("bt_hfp_source_send_dummy_data_stop");
    btos_timer_cancel(hfp_time_handle_id);
    hfp_time_handle_id = NULL;
}

#endif
/**
 ****************************************************************************************
 * @brief initialize platform
 *
 *
 ****************************************************************************************
 */
void bt_stack_connect_profile_cb(TimerHandle_t time_id)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    CLOGD("bt_stack_connect_profile_cb,conidx:0x%x",stack_env->bt_classic_conidx);
    app_hfp_connect(stack_env->bt_classic_conidx, BT_STACK_HFP_PEER_ROLE);
}

void bt_stack_connect_profile_by_timer(uint32_t milli_seconds)
{
       TimerHandle_t update_id = btos_timer_creat(TIMER_TYPE_SINGLE, milli_seconds, bt_stack_connect_profile_cb);
}

void  bt_stack_classic_search_peer(uint8_t role)
{
    uint8_t len = sizeof(gap_bdaddr_t);
    gap_bdaddr_t peer = {0};

    #ifndef MFG_RF_TEST
    if(bt_stack_nvs_get(NVS_ID_BT_PEER_ADDRESS, &len, (uint8_t*)&peer) == NVDS_OK) 
    {
        if(role == BT_STACK_CLASSIC_SINK)
        {
            CLOGD("reconnect addr: 0x%02x%02x%02x%02x%02x%02x", peer.addr[5], peer.addr[4], peer.addr[3], \
                peer.addr[2], peer.addr[1], peer.addr[0]);
            bt_stack_bt_connect(peer, 2, 0, 0);
        }
        else
        {
            /// page scan enable
            bt_stack_bt_scan(BT_GAP_PSCAN_EN);
        }
    }
    else
    {
        if(role == BT_STACK_CLASSIC_SINK)
        {
            /// page&inqury scan enable
            bt_stack_bt_scan(BT_GAP_ISCAN_PSCAN_EN);
        }
        else
        {
            ///scan
            CLOGD("start inquiry peer");
            bt_stack_bt_inquiry(BT_GAPM_DISC_TYPE_GEN_DISC, 5);
        }
    }
    #endif
}
void bt_stack_classic_enable_cmp(uint16_t status)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    uint16_t flags = 0;
    uint16_t uuid[2];
    uint8_t len = GAP_BD_ADDR_LEN;

    //uuid[0] = HID_UUID;
    // start adv
    gap_bdaddr_t peer = {0};

    stack_env->bt_role = BT_STACK_CLASSIC_ROLE;

#if WHITE_LIST_ADD
    bt_stack_ble_add_paired_to_wlist();
#endif
#if RESOVLE_LIST_ADD
    ble_gap_add_paired_rpa_to_rlist();
#endif
#if BT_MUSIC_PRESENT
    bt_stack_a2dp_caps_set(a2dp_cfg.a2dp_role, &a2dp_caps);
    bt_stack_a2dp_enable(&a2dp_cfg);
#endif
#if BT_CALL_PRESENT
    bt_stack_hfp_enable(&hfp_cfg);
#endif
    CLOGD("bt classic enable cmp, status:%d", status);
}

///actv         0:stop, 1:start
///resquester   0:auto, 1:user
void bt_stack_bt_actv_ind(uint8_t actv, uint8_t type, uint8_t actv_id, uint8_t resquester, int16_t status)
{
    switch(type)
    {
        case GAPM_ACTV_TYPE_DISCOVERY :
        case GAPM_ACTV_TYPE_CONNECT:
        {
            CLOGD("bt actv stop:act-type-id:%d-%d-%d,req:%d,sta:0x%x", actv, type, actv_id, resquester, status);
        }
        break;
        default : break;
    }
}

void bt_stack_classic_conn_ind(uint8_t conidx, uint16_t conhdl, gap_bdaddr_t *peer_addr)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    stack_env->bt_classic_connected = 1;

    stack_env->bt_classic_conidx = conidx;

#if BT_MUSIC_PRESENT
    stack_env->bt_music_send_cnt = 0;
#endif

#if BT_CALL_PRESENT
    stack_env->bt_call_send_cnt = 0;
    stack_env->bt_call_codec_type = BT_STACK_HFP_CODEC_TYPE;
#endif

    CLOGD("BT classic connected,role:%d",bt_gap_get_role(conidx));

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
        if(stack_env->bt_role == BT_STACK_CLASSIC_SOURCE)
        {
            //bt_gap_auth_req(conidx, GAP_SEC_UNAUTH);
            ///switch role to master.
            //bt_gap_set_role(conidx, BT_ROLE_MASTER);
        }
        bt_classic_scan_enable(BT_GAP_SCAN_DIS);
    }

    //bt_a2dp_setup(BT_A2DP_SINK);
    /// save last device address
    bt_stack_nvs_set(NVS_ID_BT_PEER_ADDRESS, sizeof(gap_bdaddr_t), (uint8_t*)peer_addr);
}

void bt_stack_classic_disc_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    stack_env->bt_classic_connected = 0;
    uint16_t flags = 0;
    uint8_t disc = 0;
    uint16_t uuid[2];
    uint8_t len = sizeof(gap_bdaddr_t);
    // start adv
    gap_bdaddr_t peer = {0};

    bt_stack_nvs_get(NVS_ID_BT_PEER_ADDRESS, &len, (uint8_t*)&peer);

    CLOGD("DISCONNECT BDADDR: 0x%02x%02x%02x%02x%02x%02x, reason:0x%x", peer.addr[5], peer.addr[4], peer.addr[3], \
            peer.addr[2], peer.addr[1], peer.addr[0], reason);
    if(reason == BT_ERROR_AUTH_FAILURE)
    {
        bt_gap_delete_bond(&peer);
    }
    else
    {
        bt_gap_save_lk_mem_to_nvs(conidx);
    }

    bt_stack_classic_search_peer(stack_env->bt_role);
}

static void bt_stack_classic_key_req(uint8_t conidx, uint8_t key_type, uint32_t key)
{
    //ble_gap_key_cfm(conidx, 1, 123456);
}

void bt_stack_classic_bond_ind(uint8_t conidx, uint16_t status)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    CLOGD("bt_stack_classic_bond_ind:idx:%d,sta:0x%x,con:%d",conidx, status, stack_env->bt_init_connect);
    uint8_t info = status >> 8;
    uint8_t value = status & 0xff;

    switch(info)
    {
        case GAP_BT_LINK_AUTH_REQ :
        {
            if(value == 0)
            {
                //app_a2dp_connect(conidx, a2dp_cfg.a2dp_role);
                if(stack_env->bt_init_connect == 1)
                {
                    if(stack_env->bt_classic_bond == 1)
                    {
                        /// avoid some earphone l2cap conflict.
                        bt_stack_connect_profile_by_timer(700);
                    }
                    else
                    {
                        app_hfp_connect(conidx, BT_STACK_HFP_PEER_ROLE);
                    }

                }               
            }
            else
            {
                ///disconnect
                ble_gap_disconnect(conidx, BT_ERROR_AUTH_FAILURE);
                            
                bt_gap_delete_bond(NULL);  //clear all
                bt_stack_nvs_del(NVS_ID_PEER_ADDRESS);
                CLOGI("auth error,user dis: BT_ERROR_AUTH_FAILURE");
            }
        }break;
        case GAP_LK_EXCH :
        {
            if(value == 0)
            {
                stack_env->bt_classic_bond = 1;
                CLOGI("lk save suc");
            }
        }break;
        case GAP_PAIRING_FAILED :
        {
            ///disconnect
            ble_gap_disconnect(conidx, BT_ERROR_AUTH_FAILURE);
                    
            bt_gap_delete_bond(NULL);  //clear all
            bt_stack_nvs_del(NVS_ID_PEER_ADDRESS);
            CLOGI("pair error,user dis: BT_ERROR_AUTH_FAILURE");
        }break;
        default : break;
    }
}

void bt_stack_classic_discover_ind(gap_bdaddr_t *peer_addr, uint16_t clk_off, int8_t rssi, uint8_t mode, uint32_t cod, struct gap_dev_name *name)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    if(stack_env->bt_role == BT_STACK_CLASSIC_SOURCE)
    {
        CLOG("bt inq,mode:%d,addr:%02x:%02x:%02x:%02x:%02x:%02x,clk_off:0x%x,rssi:%d,",mode, peer_addr->addr[0], peer_addr->addr[1],peer_addr->addr[2],peer_addr->addr[3],peer_addr->addr[4],peer_addr->addr[5],clk_off, rssi);
        if(name->value_length > 0)
        {
            uint8_t peer_name[2][32]={PEER_DEV_NAME0, PEER_DEV_NAME1};
            uint8_t search_type = BT_SEARCH_PERR_BY_TYPE;
            uint8_t peer_found = 0;

            uint8_t name_buf[name->value_length];
            memcpy(name_buf, name->value, name->value_length);
            //name_buf[sizeof(peer_name)-1] = '\0';

            //CLOG("peer:%s-%s,search:%s-%d", &peer_name[0], &peer_name[1],name_buf,name->value_length);
            CLOG("name:%s", name_buf);

            switch(search_type)
            {
                case 1:
                    if((memcmp(&peer_name[0], name->value, sizeof(PEER_DEV_NAME0)-1) == 0) || (memcmp(&peer_name[1], name_buf, sizeof(PEER_DEV_NAME1)-1) == 0))
                    {
                        peer_found = 1;
                    }break;
                        
                case 2:
                    if(memcmp(&peer_dev_addr, peer_addr, GAP_BD_ADDR_LEN) == 0)
                    {
                        peer_found = 1;
                    }break;
                case 3:
                    if(((memcmp(&peer_name[0], name_buf, sizeof(PEER_DEV_NAME0)-1) == 0) || (memcmp(&peer_name[1], name_buf, sizeof(PEER_DEV_NAME1)-1) == 0))
                        && (memcmp(&peer_dev_addr, peer_addr, GAP_BD_ADDR_LEN) == 0))
                    {
                        peer_found = 1;
                    }break;
                default : break;
            }
            
            CLOG("peer_found:%d",peer_found);
            if(peer_found)
            {
                bt_gap_discover_stop();
                bt_stack_bt_connect(*peer_addr, 2, clk_off, 0);
            }
        }
        CLOG("\n");
    }
}

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
    CLOGI("a2dp connect cmp!,peer  media mtu:%d", app_a2dp_get_media_peer_mtu(conidx));
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    
    if (stack_env->bt_role == BT_STACK_CLASSIC_SOURCE)
    {
#if(BT_MUSIC_SOURCE_SEND_DUMMY)
        app_a2dp_start(conidx);
#endif
    }
}

static void a2dp_disconnect_cmp(uint8_t conidx, uint16_t status)
{
    CLOGI("a2dp disconnect cmp!");
}

static void a2dp_start_ind(uint8_t conidx, uint8_t codec, uint8_t ch, uint16_t sample_rate)
{
    CLOGI("a2dp start! codec:%d, ch:%d, sample_rate:%d", codec, ch, sample_rate);
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    
    if(stack_env->bt_role == BT_STACK_CLASSIC_SOURCE)
    {
        #if (BT_MUSIC_SOURCE_SEND_DUMMY)
        bt_a2dp_source_send_dummy_start(conidx, 20);
        #endif
    }
    else
    {
        bt_stack_a2dp_send_start(conidx, codec, ch, sample_rate);
    }
}

static void a2dp_stop_ind(uint8_t conidx, uint8_t status)
{
    CLOGI("a2dp stop!, conidx:%d, sta:0x%x", conidx, status);
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    
    if(stack_env->bt_role == BT_STACK_CLASSIC_SOURCE)
    {
#if (BT_MUSIC_SOURCE_SEND_DUMMY)
        app_avrcp_play_status_set(conidx, BT_AVRCP_PLAYBACK_STATUS_PAUSED);
        bt_a2dp_source_send_dummy_stop();
#endif
    }
    else
    {
        bt_stack_a2dp_send_stop(conidx, status);
    }
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
    
    if(stage == 1)
    {
        if(stack_env->bt_music_send_cnt > 0)
        {
            stack_env->bt_music_send_cnt--;
        }
    }
    else
    {
        // to free data buf if needed.
    }
    if(send_status != 0)
    {
        CLOGI("a2dp media rsp,data:0x%x,sta:0x%x,cnt:%d", data, status, stack_env->bt_music_send_cnt);
    }
}

static void avrcp_connect_cmp(uint8_t conidx, uint16_t status)
{
    CLOGI("avrcp connect cmp!,conidx:0x%x, status:0x%x", conidx, status);
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    
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
            app_avrcp_press_req(conidx, 0, BT_AVRCP_PRESS_ID_SET_ABSOLUTE_VOLUME, 0x40);
            app_avrcp_notify_req(conidx, BT_AVRCP_NOTIFI_VOLUME_CHANGED);
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

#endif

#if BT_CALL_PRESENT
static void hfp_enable_cmp(uint16_t status)
{
    CLOGD("hfp enable cmp,status:0x%x", status);
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    bt_stack_classic_search_peer(stack_env->bt_role);

}

static void hfp_disable_cmp(uint16_t status)
{
    CLOGD("hfp disable cmp,status:0x%x", status);
}

static void hfp_connect_cmp(uint8_t conidx, uint8_t type, uint16_t status)
{
    CLOGD("hfp connect cmp,conidx:%d, type:%d, status:0x%x", conidx, type, status);
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    if(stack_env->bt_init_connect == 1)
    {
        app_a2dp_connect(conidx, a2dp_cfg.a2dp_role);
    }
#if BT_HFP_SOURCE_SEND_DUMMY
    if(status == 0)
    {
        ///incomming call
        app_hfp_call_incomming(conidx, 0);
        bt_hfp_source_dummy_call_start(conidx, 6000);
    }
#endif
}

static void hfp_disconnect_cmp(uint8_t conidx, uint16_t status)
{
    CLOGD("hfp disconnect cmp,conidx:%d, status:0x%x", conidx, status);
}

void hfp_aud_start_ind(uint8_t conidx, uint16_t codec, uint16_t status)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    CLOGD("hfp aud start! conidx:0x%x,codec:%d-%d,status:0x%x",conidx, codec,stack_env->bt_call_codec_type, status);
    if(status == 0)
    {
        bt_stack_hfp_send_start(conidx, stack_env->bt_call_codec_type);
        
        #if BT_HFP_SOURCE_SEND_DUMMY
        //bt_hfp_source_send_dummy_data_start(conidx, 8);
        #endif
    }
}

void hfp_aud_stop_ind(uint8_t conidx, uint16_t conhdl, uint16_t reason)
{
    CLOGD("hfp aud stop!, conidx:%d, reason:0x%x", conidx, reason);
    #if BT_HFP_SOURCE_SEND_DUMMY
    bt_hfp_source_send_dummy_data_stop();
    #endif
    bt_stack_hfp_send_stop(conidx, (uint8_t)reason);
}
///app receive sco aud data from peer.
static void hfp_receive_media_from_peer(uint8_t conidx, uint8_t pkt_sta, uint16_t len, uint8_t *data)
{
#if 1
    static uint32_t r_g_count = 0, error_count = 0;
    if(pkt_sta != 0)
    {
        error_count++;
    }
    r_g_count++;
    if((r_g_count % 256) == 0)
    {
        CLOGD("hfp rcv, cnt:%d-%d,len:%d", error_count, r_g_count, len);
        error_count = 0;
    }
#endif
    bt_stack_hfp_send_data(conidx, pkt_sta, len, data);

#if BT_CALL_SCO_SEND_DUMMY
    ///dummy data send 
    //static uint32_t time1 = 0, time2 = 0, time3 = 0;
    ///dummy data send 
    
    static uint8_t trans_buf[120];
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();
    if(stack_env->bt_call_send_cnt <= 4)
    {
        
        if(pkt_sta == 0)
        {
            memcpy(trans_buf, data, len);
        }
        else
        {
            memset(trans_buf, 0x55*(1-BT_USE_ASIC_CVSD), len);
        }
        app_hfp_send_aud_to_peer(conidx, len, trans_buf);
        stack_env->bt_call_send_cnt++;
    }
    #if 1
    static uint32_t s_g_count = 0;
    s_g_count++;
    if((s_g_count % 256) == 0)
    {
        CLOGD("hfp dummy send:%d-%d", stack_env->bt_call_send_cnt, s_g_count);
        CLOGD("d:0x%02x%02x%02x%02x%02x%02x%02x%02x", trans_buf[0], trans_buf[1], trans_buf[2], trans_buf[3], \
            trans_buf[4], trans_buf[5], trans_buf[6], trans_buf[7]);
    }
    ///stop hfp after 150 s
    if(s_g_count >= 125 * 15)
    {
        s_g_count = 0;
        CLOGD("hfp dummy end:%d-%d", stack_env->bt_call_send_cnt, s_g_count);
        app_hfp_call_end(conidx, 0);
        app_hfp_call_remove_audio(conidx, 0x00);
    }
    #endif
#endif
}

static void hfp_send_media_cmp(uint8_t conidx, uint8_t status, uint8_t *data)
{
    bt_stack_if_env_tag_t *stack_env = bt_stack_if_get_env();

    ///to do 
    ///if need to free data;
    //CLOGD("cmp:0x%x-0x%x",status,data);
    if(stack_env->bt_call_send_cnt > 0)
    {
        stack_env->bt_call_send_cnt--;
    }
    if(status != 0)
    {
        CLOGI("hfp call rsp,data:0x%x,sta:0x%x,cnt:%d", data, status, stack_env->bt_call_send_cnt);
    }
    #if 0
    if(data != NULL)
    {
        ke_free(data);
    }
    #endif
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

#endif

#endif

