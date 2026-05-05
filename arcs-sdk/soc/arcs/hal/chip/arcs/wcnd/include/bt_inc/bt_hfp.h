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

enum bt_hfp_call_req_type
{
    /// call request
    BT_HFP_CALL_REQ,
    /// call indicate
    BT_HFP_CALL_IND,
    /// voice recognition
    BT_HFP_VR_REQ,
    /// DTMF
    BT_HFP_DTMF_REQ,
};

enum bt_hfp_status_req_type
{
    /// get status
    BT_HFP_STATUS_GET,
    /// set status
    BT_HFP_STATUS_SET,
    /// indicate app status changed
    BT_HFP_STATUS_IND,
};

enum bt_call_type
{
    /// Call Request
    BT_HF_CALL_RELEASE,
    BT_HF_CALL_INCOMMING,
    BT_HF_CALL_OUTGOING,
    BT_HF_CALL_ALERT,
    BT_HF_CALL_ACTIVE,
    BT_HF_CALL_NUM,
    BT_HF_CALL_MEM,
    BT_HF_CALL_REDIAL,
    /// Call Answer
    BT_HF_CALL_ACCEPT,
    BT_HF_CALL_REJECT,
    /// Multiparty Call
    BT_HF_CALL_WAIT,
    BT_HF_CALL_HOLD,
    BT_HF_CALL_CHLD,
    /// Voice_Recognition
    BT_HF_VR_START,
    BT_HF_VR_PROMPT,
    BT_HF_VR_READY,
    BT_HF_VR_PROC,
    BT_HF_VR_STOP,
    // voice tag
    BT_HF_VR_VTAG,
    // DTMF Code
    BT_HF_DTMF_STAR,
    BT_HF_DTMF_SHARP,
    
    BT_HF_CALL_BTRH,

};

/// Bt HFP Status type
/*@TRACE*/
enum bt_hfp_status_type
{
    /// AG indicator
    BT_HF_SERVICE_IND,
    BT_HF_CALL_IND,
    BT_HF_CALLSETUP_IND,
    BT_HF_CALLHELD_IND,
    BT_HF_BATTCHG_IND,
    BT_HF_SIGNAL_IND,
    BT_HF_ROAM_IND,
    /// HF indicator
    // Enhanced Safety
    BT_HF_EN_SAFETY,
    // Battery level
    BT_HF_BAT_LVL,
    /// in-band ring
    BT_HF_RING_INBAND,
    /// codec type
    BT_HF_CODEC_TYPE,
    /// Gain
    BT_HF_MIC_GAIN,
    BT_HF_SPK_GAIN,
    /// NREC
    BT_HF_NREC_CFG,
    /// Extended Audio Gateway Error Result Code
    BT_HF_CME_ERROR,
    /// status mask
    BT_HF_STAT_MASK,
    /// max count
    BT_HFP_MAX_STATS,
    /// features @see enum hfp_feats
    BT_HFP_FEATS,
};

enum bt_hfp_phb_vr_res
{
    // voice recognition of hf audio
    BT_HF_VR_RES_HF,
    // question textual result of voice recognition
    BT_HF_VR_RES_QUEST,
    // error description VR
    BT_HF_VR_RES_ERR,
    // new answer textual result of VR
    BT_HF_VR_RES_ANS,
    // replace answer textual result of VR
    BT_HF_VR_RES_ANS_REPLACE,
    // append answer textual result of VR
    BT_HF_VR_RES_ANS_APPEND,
};

typedef enum
{
    HFP_MEDIA_CODEC_AUTO = 0,
    HFP_MEDIA_CODEC_CVSD,
    HFP_MEDIA_CODEC_MSBC,
    HFP_MEDIA_CODEC_SCO,
}hfp_media_codec_type_t;


enum bt_hfp_btrh_param
{
    RELEASE_ALL_HOLD              = 0,
    HOLD_ACTIVE_AND_ACCEPT        = 1,
    REJECT_HOLD                   = 2,
};

enum bt_hfp_chld_param
{
    HFP_CHLD_RELEASE_ALL_HELD              = 0,
    HFP_CHLD_RELEASE_ACTIVE_AND_ACCEPT     = 1,
    HFP_CHLD_RELEASE_CALL1                 = 11,
    HFP_CHLD_RELEASE_CALL2                 = 12,
    HFP_CHLD_HOLD_ACTIVE_AND_ACCEPT        = 2,
    HFP_CHLD_PRIVATE_CALL1                 = 21,
    HFP_CHLD_PRIVATE_CALL2                 = 22,
    HFP_CHLD_ADD_TO_CONV                   = 3,
    HFP_CHLD_CONNECT_CALLS                 = 4,
};

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
     * @brief hfp connect cmp.
     ****************************************************************************************
     */
    void (*cb_hfp_con_cmp)(uint8_t conidx, uint8_t type, uint16_t status);

    /**
     ****************************************************************************************
     * @brief hfp connect cmp.
     ****************************************************************************************
     */
    void (*cb_hfp_discon_cmp)(uint8_t conidx, uint16_t status);

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
    void (*cb_hfp_aud_stop_ind)(uint8_t conidx, uint16_t status);

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

    /**
     ****************************************************************************************
     * @brief hfp status indicate.
     ****************************************************************************************
     */
    void (*cb_hfp_status_ind)(uint8_t conidx, uint8_t req_type, uint8_t status_type, uint16_t val);

    /**
     ****************************************************************************************
     * @brief call  indicate.
     ****************************************************************************************
     */
    void (*cb_hfp_call_ind)(uint8_t conidx, uint8_t req_type, uint8_t call_type, uint8_t call_idx);

}bt_hfp_cb_t;

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
 /**
 * hfp call incomming
 *
 * @param conidx,call_idx
 *
 * @return None.
 */
void app_hfp_call_incomming(uint8_t conidx, uint8_t call_idx);

/**
 * hfp call start
 *
 * @param conidx,call_idx
 *
 * @return None.
 */
void app_hfp_call_start(uint8_t conidx, uint8_t call_idx);

/**
 * hfp call end
 *
 * @param conidx, call_idx.
 *
 * @return None.
 */
void app_hfp_call_end(uint8_t conidx, uint8_t call_idx);

/**
 * app hfp voice recognition
 *
 * @param conidx, vr(@see enum bt_call_type)
 *
 * @return None.
 */
void app_hfp_set_bvra(uint8_t conidx, uint8_t vr);

/**
 * app hfp send dtmf
 *
 * @param conidx, vr(@see enum bt_call_type)
 *
 * @return None.
 */
void app_hfp_send_dtmf(uint8_t conidx, uint8_t code);

/**
 * app hfp voice recognition text
 *
 * @param conidx, type
 *
 * @return None.
 */
void app_hfp_set_vr_text(uint8_t conidx, uint8_t type, uint8_t len,uint8_t *data);

/**
 * app hfp set num
 *
 * @param conidx, call_idx
 *
 * @return None.
 */
void app_hfp_set_num(uint8_t conidx, uint8_t call_idx, uint8_t len,uint8_t *data);

/**
 * app hfp set btrh
 *
 * @param conidx, btrh_param(@see enum bt_hfp_btrh_param)
 *
 * @return None.
 */
void app_hfp_set_btrh(uint8_t conidx, uint8_t btrh_param);

/**
 * app hfp set chld
 *
 * @param conidx, chld_param(@see enum bt_hfp_chld_param)
 *
 * @return None.
 */
void app_hfp_set_chld(uint8_t conidx, uint8_t chld_param);

/**
 * app hfp set nrec
 *
 * @param conidx
 *
 * @return None.
 */
void app_hfp_set_nrec(uint8_t conidx, uint8_t val);


/**
 * app hfp set codec type
 *
 * @param conidx,type, 1:cvsd,2:msbc
 *
 * @return None.
 */
void app_hfp_set_codec_type(uint8_t conidx, uint8_t type);


/**
 * hfp call add audio
 *
 * @param conidx, aud_type(@see enum bt_hfp_chld_param)
 *
 * @return None.
 */
void app_hfp_call_add_audio(uint8_t conidx, uint8_t aud_type);

/**
 * hfp call remove audio
 *
 * @param conidx,reason
 *
 * @return None.
 */
void app_hfp_call_remove_audio(uint8_t conidx, uint8_t reason);
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

/**
 * hfp enable
 *
 * @param data cannot free untile cb_hfp_send_media_cmp callbak.
 *
 * @return status.
 */
int app_hfp_send_aud_to_peer(uint8_t conidx, uint16_t len, uint8_t *data);

/**
 * hfp connect
 *
 * @param conidx.
 *
 * @param role.
 *
 * @return status.
 */

void app_hfp_connect(uint8_t conidx, uint8_t role);


#endif



