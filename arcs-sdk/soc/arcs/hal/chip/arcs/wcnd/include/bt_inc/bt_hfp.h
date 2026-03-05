/**
 ****************************************************************************************
 *
 * @file bt_hfp.h
 *
 * @brief Header file - BLE GAP External API
 *
 * Copyright (C) ListenAI 2022-2042
 ****************************************************************************************
 */

#ifndef BT_HFP_H_
#define BT_HFP_H_

/**
 ****************************************************************************************
 * @addtogroup HFP External API
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
 /// List hfp error codes
enum hfp_err
{
    /// No error
    BT_HFP_ERR_NO_ERROR                                                            = 0x00,

};
    
/// BT hfp role
enum bt_hfp_role
{
    /// Handsfree unit
    BT_HFP_ROLE_HF     = 0,
    /// hf audio gateway
    BT_HFP_ROLE_HF_AG  = 1,
    /// headset
    BT_HFP_ROLE_HS     = 2,
    /// headset AG
    BT_HFP_ROLE_HS_AG  = 3,
};

/// BT hfp profile features
enum bt_hfp_feats
{
    /// HF Supported Features:
    // EC and/or NR function
    BT_HFP_HFSF_NREC                       = (1<<0),
    // Three-way calling
    BT_HFP_HFSF_3WAY                       = (1<<1),
    // CLI presentation capability
    BT_HFP_HFSF_CLIP                       = (1<<2),
    // Voice recognition activation
    BT_HFP_HFSF_VR                         = (1<<3),
    // Remote volume control
    BT_HFP_HFSF_VOL_CTL                    = (1<<4),
    // Wide band speech
    BT_HFP_HFSF_WBS                        = (1<<5),
    // Enhanced Voice Recognition Status
    BT_HFP_HFSF_EN_VR                      = (1<<6),
    // Voice Recognition Text
    BT_HFP_HFSF_VR_TXT                     = (1<<7),

    /// AG Supported Features:
    // Three-way calling
    BT_HFP_AGSF_3WAY                       = (1<<0),
    // EC and/or NR function
    BT_HFP_AGSF_NREC                       = (1<<1),
    // Voice recognition function
    BT_HFP_AGSF_VR                         = (1<<2),
    // In-band ring tone capability
    BT_HFP_AGSF_IN_BAND_RING               = (1<<3),
    // Attach a number to a voice tag
    BT_HFP_AGSF_VOICE_TAG                  = (1<<4),
    // Wide band speech
    BT_HFP_AGSF_WBS                        = (1<<5),
    // Enhanced Voice Recognition Status
    BT_HFP_AGSF_EN_VR                      = (1<<6),
    // Voice Recognition Text
    BT_HFP_AGSF_VR_TXT                     = (1<<7),

    /// AG Network
    BT_HFP_AG_NETWORK                      = (1<<16),

    /// native function feature
    // indicate user defined AT cmd
    BT_HFP_SF_USER_CMD                     = (1<<30),
    // indicate all raw data
    BT_HFP_SF_RAW_DATA                     = (1<<31),
};

typedef enum
{
    HFP_MEDIA_CODEC_CVSD = 0,
    HFP_MEDIA_CODEC_MSBC,
}hfp_media_codec_type_t;

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

 typedef struct bt_hfp_cfg
{
    uint8_t  hfp_role;
    uint32_t hfp_feats;
} bt_hfp_cfg_t;

 typedef struct bt_hfp_status
{
    uint8_t  sevice;
    uint8_t  signal;
    uint8_t  roam;
    uint8_t  call;
    uint8_t  call_setup;
    uint8_t  call_held;
    uint8_t  bat_chg;
    uint8_t  battery;
    uint8_t  en_safety;
    uint8_t  inband_ring;
    uint8_t  error;
} bt_hfp_status_t;

typedef struct bt_hfp_cb
{
    /**
     ****************************************************************************************
     * @brief Reception of hfp enable complete.
     ****************************************************************************************
     */
    void (*cb_hfp_enable_cmp)(uint16_t status);
    /**
     ****************************************************************************************
     * @brief Reception of hfp disable complete.
     ****************************************************************************************
     */
    void (*cb_hfp_disable_cmp)(uint16_t status);

    /**
     ****************************************************************************************
     * @brief hfp start indicate.
     ****************************************************************************************
     */
    void (*cb_hfp_aud_start_ind)(uint8_t conidx, uint8_t codec);

    /**
     ****************************************************************************************
     * @brief hfp stop indicate.
     ****************************************************************************************
     */
    void (*cb_hfp_aud_stop_ind)(uint8_t conidx, uint8_t status);

    /**
     ****************************************************************************************
     * @brief hfp media indicate.
     ****************************************************************************************
     */
    void (*cb_hfp_media_ind)(uint8_t conidx, uint8_t pkt_sta, uint16_t len, uint8_t *data);

    /**
     ****************************************************************************************
     * @brief hfp send media complete.
     ****************************************************************************************
     */
    void (*cb_hfp_send_media_cmp)(uint8_t conidx, uint8_t status, uint8_t *data);

}bt_hfp_cb_t;

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
 
/**
 * hfp enable
 *
 * @param role:sink,source. cb:callback function.
 *
 * @return None.
 */
void app_hfp_enable(uint8_t role, uint32_t feats, const bt_hfp_cb_t *cb);

/**
 * hfp enable
 *
 * @param role:sink,source. cb:callback function.
 *
 * @return None.
 */
void app_hfp_disable(void);

#endif



