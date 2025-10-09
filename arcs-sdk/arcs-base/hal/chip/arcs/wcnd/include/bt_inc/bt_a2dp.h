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
} bt_a2dp_cfg_t;

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
     * @brief a2dp media indicate.
     ****************************************************************************************
     */
    void (*cb_a2dp_media_ind)(uint8_t conidx, uint8_t frame_num, uint16_t seq, uint16_t len, uint8_t *data);

}bt_a2dp_cb_t;

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
 
 /**
 * a2dp setup
 *
 * @param role            a2dp role 0:source, 1:sink 
 *
 * @return None.
 */
void bt_a2dp_setup(uint8_t role);

/**
 * a2dp revoke
 *
 * @param None
 *
 * @return None.
 */
void bt_a2dp_revoke(void);

/**
 * a2dp enable
 *
 * @param role:sink,source. cb:callback function.
 *
 * @return None.
 */

void app_a2dp_enable(uint8_t role, const bt_a2dp_cb_t *cb);

#endif



