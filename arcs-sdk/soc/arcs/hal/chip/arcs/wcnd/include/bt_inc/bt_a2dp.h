/**
 ****************************************************************************************
 *
 * @file bt_a2dp.h
 *
 * @brief Header file - BLE GAP External API
 *
 * Copyright (C) ListenAI 2022-2042
 ****************************************************************************************
 */

#ifndef BT_A2DP_H_
#define BT_A2DP_H_

/**
 ****************************************************************************************
 * @addtogroup GAP External API
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */


/*
 * MACRO DEFINITIONS
 ****************************************************************************************
 */
/// SBC configuration bit fields
#define BT_SBC_SAMPLING_FREQ_16000        128
#define BT_SBC_SAMPLING_FREQ_32000         64
#define BT_SBC_SAMPLING_FREQ_44100         32
#define BT_SBC_SAMPLING_FREQ_48000         16
#define BT_SBC_CHANNEL_MODE_MONO            8
#define BT_SBC_CHANNEL_MODE_DUAL_CHAN       4
#define BT_SBC_CHANNEL_MODE_STEREO          2
#define BT_SBC_CHANNEL_MODE_JOINT_STEREO    1
#define BT_SBC_BLOCK_LENGTH_4             128
#define BT_SBC_BLOCK_LENGTH_8              64
#define BT_SBC_BLOCK_LENGTH_12             32
#define BT_SBC_BLOCK_LENGTH_16             16
#define BT_SBC_SUBBANDS_4                   8
#define BT_SBC_SUBBANDS_8                   4
#define BT_SBC_ALLOCATION_SNR               2
#define BT_SBC_ALLOCATION_LOUDNESS          1
#define BT_SBC_BITPOOL_MIN                  2
#define BT_SBC_BITPOOL_MAX                 53
#define BT_SBC_BITPOOL_MEDIUM_QUALITY      32
#define BT_SBC_BITPOOL_HIGH_QUALITY        50

/// AAC configuration bit fields
#define BT_AAC_OBJECT_MPEG2_AAC_LC        128
#define BT_AAC_OBJECT_MPEG4_AAC_LC         64
#define BT_AAC_OBJECT_MPEG4_AAC_LTP        32
#define BT_AAC_OBJECT_MPEG4_AAC_SCALABLE   16
#define BT_AAC_OBJECT_MPEG4_HE_AAC          8
#define BT_AAC_OBJECT_MPEG4_HE_AAC_V2       4
#define BT_AAC_OBJECT_MPEG4_AAC_ELD_V2      2

#define BT_AAC_DRC                        128

#define BT_AAC_SAMPLING_FREQ_8000         128
#define BT_AAC_SAMPLING_FREQ_11025         64
#define BT_AAC_SAMPLING_FREQ_12000         32
#define BT_AAC_SAMPLING_FREQ_16000         16
#define BT_AAC_SAMPLING_FREQ_22050          8
#define BT_AAC_SAMPLING_FREQ_24000          4
#define BT_AAC_SAMPLING_FREQ_32000          2
#define BT_AAC_SAMPLING_FREQ_44100          1
#define BT_AAC_SAMPLING_FREQ_48000        128
#define BT_AAC_SAMPLING_FREQ_64000         64
#define BT_AAC_SAMPLING_FREQ_88200         32
#define BT_AAC_SAMPLING_FREQ_96000         16

#define BT_AAC_NUM_CHANNELS_1               8
#define BT_AAC_NUM_CHANNELS_2               4
#define BT_AAC_NUM_CHANNELS_6               2
#define BT_AAC_NUM_CHANNELS_8               1

#define BT_AAC_VBR                        128


/*
 * DEFINES
 ****************************************************************************************
 */
 /// List a2dp error codes
enum a2dp_aud_err
{
    /// No error
    BT_AUD_ERR_NO_ERROR                                                            = 0x00,

    BT_AUD_ERR_SAMPLE,
    BT_AUD_ERR_CODEC,
    BT_AUD_ERR_FRAME_DUR,

};
    
/// BT A2dp role
enum bt_a2dp_role
{
    /// source
    BT_A2DP_SOURCE = 0x00,
    /// sink
    BT_A2DP_SINK   = 0x01,
};

typedef enum
{
    A2DP_MEDIA_CODEC_SBC = 0,
    A2DP_MEDIA_CODEC_MPEG1_2_AUDIO,
    A2DP_MEDIA_CODEC_MPEG2_4_AAC,
    A2DP_MEDIA_CODEC_ATRAC,
    A2DP_MEDIA_CODEC_NONA2DP = 0xff,
}a2dp_media_codec_type_t;


/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

 typedef struct bt_a2dp_cfg
{
    uint8_t a2dp_role;
    uint8_t aac_support;
} bt_a2dp_cfg_t;

 typedef struct bt_a2dp_meida_caps_cfg
{
    uint8_t a2dp_role;
    ///sbc cfg
    uint8_t sbc_sample_freq;
    uint8_t sbc_ch_mode;
    uint8_t sbc_block_length;
    uint8_t sbc_subband;
    uint8_t sbc_allocation;
    uint8_t sbc_bitpool_min;
    uint8_t sbc_bitpool_max;
    ///aac cfg
    
    uint8_t aac_object;
    /// 8kHz to 44.1kHz
    uint8_t aac_sample_freq1;
    /// 48kHz to 96kHz
    uint8_t aac_sample_freq2;
    uint8_t aac_ch_num;
    uint8_t aac_vbr;
    uint32_t aac_max_bitrate;
} bt_a2dp_meida_caps_cfg_t;

typedef struct bt_a2dp_cb
{
    /**
     ****************************************************************************************
     * @brief Reception of a2dp enable complete.
     ****************************************************************************************
     */
    void (*cb_a2dp_enable_cmp)(uint16_t status);

    /**
     ****************************************************************************************
     * @brief Reception of a2dp revoke complete.
     ****************************************************************************************
     */
    void (*cb_a2dp_revoke_cmp)(uint16_t status);
    
    /**
     ****************************************************************************************
     * @brief Reception of a2dp connect complete.
     ****************************************************************************************
     */
    void (*cb_a2dp_connect_cmp)(uint8_t conidx, uint16_t status);
    
    /**
     ****************************************************************************************
     * @brief Reception of a2dp disconnect complete.
     ****************************************************************************************
     */
    void (*cb_a2dp_disconnect_cmp)(uint8_t conidx, uint16_t status);
    
    /**
     ****************************************************************************************
     * @brief Reception of a2dp start complete.
     ****************************************************************************************
     */
    void (*cb_a2dp_start_cmp)(uint8_t conidx, uint16_t status);
    
    /**
     ****************************************************************************************
     * @brief Reception of a2dp suspend complete.
     ****************************************************************************************
     */
    void (*cb_a2dp_suspend_cmp)(uint8_t conidx, uint16_t status);

    /**
     ****************************************************************************************
     * @brief Reception of a2dp close complete.
     ****************************************************************************************
     */
    void (*cb_a2dp_close_cmp)(uint8_t conidx, uint16_t status);

    /**
     ****************************************************************************************
     * @brief a2dp start indicate.
     ****************************************************************************************
     */
    void (*cb_a2dp_start_ind)(uint8_t conidx, uint8_t codec, uint8_t ch, uint16_t sample_rate);

    /**
     ****************************************************************************************
     * @brief a2dp stop indicate.
     ****************************************************************************************
     */
    void (*cb_a2dp_stop_ind)(uint8_t conidx, uint8_t status);

    /**
     ****************************************************************************************
     * @brief a2dp media indicate,receive media data from peer.
     ****************************************************************************************
     */
    void (*cb_a2dp_media_ind)(uint8_t conidx, uint8_t frame_num, uint16_t seq, uint16_t len, uint8_t *data);


     /**
     ****************************************************************************************
     * @brief a2dp media respons,respons from peer when send meida data to peer.
     ****************************************************************************************
     */
    void (*cb_a2dp_media_rsp)(uint8_t conidx, uint8_t *data, uint16_t status);

}bt_a2dp_cb_t;
/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
/**
 * a2dp enable
 *
 * @param role:sink,source. cb:callback function.
 *
 * @return None.
 */

void app_a2dp_enable(uint8_t role, uint8_t aac_support, const bt_a2dp_cb_t *cb);

/**
 * a2dp disable
 *
 * @param None
 *
 * @return None.
 */
void app_a2dp_disable(void);

/**
 * a2dp get peer media mtu
 *
 * @param conidx
 *
 * @return mtu.
 */
uint16_t app_a2dp_get_media_peer_mtu(uint8_t conidx);

/**
 * a2dp send media to peer
 *
 * @param conidx
  *
 * @param frame_num
  *
 * @param len
  *
 * @param data
 *
 * @return .
 */
void app_a2dp_send_media_to_peer(uint8_t conidx, uint8_t frame_num, uint16_t len, uint8_t *data);

/**
 * a2dp connect
 *
 * @param conidx, role
 *
 * @return None.
 */
void app_a2dp_connect(uint8_t conidx, uint8_t role);

/**
 * a2dp start
 *
 * @param conidx
 *
 * @return None.
 */
void app_a2dp_start(uint8_t conidx);

/**
 * a2dp set media caps
 *
 * @param role:a2dp role
 * @param caps_cfg:a2dp media config
 *
 * @return None.
 */
void app_a2dp_set_media_caps(uint8_t role, bt_a2dp_meida_caps_cfg_t *caps_cfg);

#endif



