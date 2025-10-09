/**
 ****************************************************************************************
 *
 * @file ble_lea.h
 *
 * @brief Header file - BLE LEA External API
 *
 * Copyright (C) ListenAI 2022-2042
 ****************************************************************************************
 */

#ifndef BLE_LEA_H_
#define BLE_LEA_H_

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
//#include "app_task.h"             // Application Manager Task API
//#include "btip_task.h"      // Task definitions
#include "log_print.h"
/*
 * MACRO DEFINITIONS
 ****************************************************************************************
 */
#ifndef __ARRAY_EMPTY
#define __ARRAY_EMPTY
#endif
/*
 * DEFINES
 ****************************************************************************************
 */
 
 /*
 * ENUMERATIONS
 ****************************************************************************************
 */
/// List all HL error codes
enum lea_err
{
    /// No error
    LEA_ERR_NO_ERROR                                                               = 0x00,

    // ----------------------------------------------------------------------------------
    // -------------------------        BAP          ------------------------------------
    // ----------------------------------------------------------------------------------
    LEA_ERR_ERROR_SAMPLE,
    LEA_ERR_ERROR_CODEC,
    LEA_ERR_ERROR_FRAME_DUR,
    // ----------------------------------------------------------------------------------
    // -------------------------       TMAP          ------------------------------------
    // ----------------------------------------------------------------------------------
    /// 0x21: TMAP is busy
    LEA_ERR_TMAP_BUSY                                                              = 0x21,
};

/// lea state machine
enum app_lea_state
{
    /// LEA idle.
    APP_LEA_STATE_IDLE           = 0,
    /// LEA enabling.
    APP_LEA_STATE_ENABLING       = 1,
    /// LEA enabled.
    APP_LEA_STATE_ENABLED        = 2,
    /// LEA discovering.
    APP_LEA_STATE_DISCOVERING    = 3,
    /// LEA discovered.
    APP_LEA_STATE_DISCOVERED     = 4,

    APP_LEA_STATE_READY          = 40,

    APP_LEA_STATE_BMR_STARTING   = 41,
    APP_LEA_STATE_BMR_STARTED    = 42,
    APP_LEA_STATE_BMR_STOPPING   = 43,
    APP_LEA_STATE_BMR_STOPPED    = 44,
    
    APP_LEA_STATE_BMS_STARTING   = 45,
    APP_LEA_STATE_BMS_STARTED    = 46,
    APP_LEA_STATE_BMS_STOPPING   = 47,
    APP_LEA_STATE_BMS_STOPPED    = 48,
    
    APP_LEA_STATE_UMR_STARTING   = 49,
    APP_LEA_STATE_UMR_STARTED    = 50,
    APP_LEA_STATE_UMR_STOPPING   = 51,
    APP_LEA_STATE_UMR_STOPPED    = 52,
    
    APP_LEA_STATE_UMS_STARTING   = 53,
    APP_LEA_STATE_UMS_STARTED    = 54,
    APP_LEA_STATE_UMS_STOPPING   = 55,
    APP_LEA_STATE_UMS_STOPPED    = 56,

    APP_LEA_STATE_CG_STARTING    = 57,
    APP_LEA_STATE_CG_STARTED     = 58,
    APP_LEA_STATE_CG_STOPPING    = 59,
    APP_LEA_STATE_CG_STOPPED     = 60,
    
    APP_LEA_STATE_CT_STARTING    = 61,
    APP_LEA_STATE_CT_STARTED     = 62,
    APP_LEA_STATE_CT_STOPPING    = 63,
    APP_LEA_STATE_CT_STOPPED     = 64,

};
    
enum aud_codec_type
{
    AUD_CODEC_LC3              = 0x06,
    AUD_CODEC_VENDOR           = 0xff
};

enum generic_codec_config_sample
{
    GEN_CFG_SAMPLE_8000HZ             = 1,
    GEN_CFG_SAMPLE_11025HZ            = 2,
    GEN_CFG_SAMPLE_16000HZ            = 3,
    GEN_CFG_SAMPLE_22050HZ            = 4,
    GEN_CFG_SAMPLE_24000HZ            = 5,
    GEN_CFG_SAMPLE_32000HZ            = 6,
    GEN_CFG_SAMPLE_44100HZ            = 7,
    GEN_CFG_SAMPLE_48000HZ            = 8,
    GEN_CFG_SAMPLE_88200HZ            = 9,
    GEN_CFG_SAMPLE_96000HZ            = 10,
    GEN_CFG_SAMPLE_176400HZ           = 11,
    GEN_CFG_SAMPLE_192000HZ           = 12,
    GEN_CFG_SAMPLE_384000HZ           = 13,
    GEN_CFG_SAMPLE_RFU                = 14,
};

enum generic_cfg_codec_frame_dur
{
    GEN_CFG_FRAME_DUR_7_5MS           = 0,
    GEN_CFG_FRAME_DUR_10MS            = 1,
    GEN_CFG_FRAME_DUR_RFU             = 2,
};

/// modules of le audio
enum lea_modules
{
    // services
    LEA_MICS_ID,
    LEA_VOCS_ID,
    LEA_AICS_ID,
    LEA_VCS_ID,
    LEA_MCS_ID,
    LEA_GMCS_ID,
    LEA_TBS_ID,
    LEA_GTBS_ID,
    LEA_OTS_ID,
    LEA_PACS_ID,
    LEA_ASCS_ID,
    LEA_BAASS_ID,
    LEA_CSIS_ID,
    LEA_CAS_ID,
    LEA_TMAS_ID,
    LEA_HAS_ID,
    LEA_IAS_ID,
    LEA_SERVICE_COUNT,
    // profiles
    LEA_MICP_ID = LEA_SERVICE_COUNT,
    LEA_VCP_ID,
    LEA_MCP_ID,
    LEA_CCP_ID,
    LEA_OTP_ID,
    LEA_BAP_ID,
    LEA_CSIP_ID,
    LEA_CAP_ID,
    LEA_TMAP_ID,
    LEA_PBP_ID,
    LEA_HAP_ID,

    LEA_MODULES_COUNT,

};

/// TMAP sub module id
enum lea_tmap_sub_id
{
    /// tmap role set
    LEA_TMAP_SUB_SET_ROLE                   = (LEA_TMAP_ID<<8)|0,
    /// tmap role get
    LEA_TMAP_SUB_GET_ROLE                   = (LEA_TMAP_ID<<8)|1,

    /// tmap enable or disable set
    LEA_TMAP_SUB_ENABLE                     = (LEA_TMAP_ID<<8)|2,

    /// cg
    LEA_TMAP_CG_START                       = (LEA_TMAP_ID<<8)|3,
    LEA_TMAP_CG_STOP                        = (LEA_TMAP_ID<<8)|4,
    /// ct
    LEA_TMAP_CT_START                       = (LEA_TMAP_ID<<8)|5,
    LEA_TMAP_CT_STOP                        = (LEA_TMAP_ID<<8)|6,
    /// ums
    LEA_TMAP_UMS_START                      = (LEA_TMAP_ID<<8)|7,
    LEA_TMAP_UMS_STOP                       = (LEA_TMAP_ID<<8)|8,
    /// umr
    LEA_TMAP_UMR_START                      = (LEA_TMAP_ID<<8)|9,
    LEA_TMAP_UMR_STOP                       = (LEA_TMAP_ID<<8)|10,
    /// bms
    LEA_TMAP_BMS_START                      = (LEA_TMAP_ID<<8)|11,
    LEA_TMAP_BMS_STOP                       = (LEA_TMAP_ID<<8)|12,
    /// bmr
    LEA_TMAP_BMR_START                      = (LEA_TMAP_ID<<8)|13,
    LEA_TMAP_BMR_STOP                       = (LEA_TMAP_ID<<8)|14,
};

/// TMAP status module id
enum lea_tmap_event_id
{
    /// send per sync, big, cig info
    LEA_TMAP_CIG_ESTABLE                    = (LEA_TMAP_ID<<8)|0,
    LEA_TMAP_PER_SYNC_ESTABLE               = (LEA_TMAP_ID<<8)|1,
    LEA_TMAP_BIG_ESTABLE                    = (LEA_TMAP_ID<<8)|2,
    LEA_TMAP_CIG_DISCONNECT                 = (LEA_TMAP_ID<<8)|3,
    LEA_TMAP_PER_SYNC_TERMINATE             = (LEA_TMAP_ID<<8)|4,
    LEA_TMAP_BIG_SYNC_TERMINATE             = (LEA_TMAP_ID<<8)|5,


    LEA_TMAP_RCV_ISO_DATA                   = (LEA_TMAP_ID<<8)|6,
};

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */
/// LE Audio server message structure
/// le audio enable service request structure
/*@TRACE*/
struct lea_enable_req
{
    /// service mask each bit for one module in @enum lea_modules
    uint32_t svc_mask;
} ;

/// lea enable response
/*@TRACE*/
struct lea_enable_rsp
{
    uint16_t  status;
};

/// LE Audio client message structure
/// lea discover request
/*@TRACE*/
struct lea_discover_req
{
    /// connection index
    uint8_t  conidx;
    /// service mask for discover
    uint16_t svc_mask;
};

/// lea discover response
/*@TRACE*/
struct lea_discover_rsp
{
    /// connection index
    uint8_t  conidx;
    /// discover status
    uint16_t  status;
};

/// lea get attribute request
/*@TRACE*/
struct lea_get_req
{
    /// attribute id
    uint16_t  attr;
    /// connection index
    uint8_t  conidx;
};

/// lea get attribute response
/*@TRACE*/
struct lea_get_rsp
{
    /// attribute id
    uint16_t  attr;
    /// connection index
    uint8_t  conidx;
    /// get result status
    uint16_t status;
    /// length of the data
    uint16_t len;
    /// attribute data
    uint8_t  data[__ARRAY_EMPTY];
};

/// lea set attribute request
/*@TRACE*/
struct lea_set_req
{
    /// attribute id
    uint16_t attr;
    /// connection index
    uint8_t  conidx;
    /// length of the data
    uint16_t len;
    /// attribute data
    uint8_t  data[__ARRAY_EMPTY];
};

/// lea set attribute response
/*@TRACE*/
struct lea_set_rsp
{
    /// attribute id
    uint16_t attr;
    /// connection index
    uint8_t  conidx;
    /// get result status
    uint16_t status;

};

struct lea_sub_rsp
{
    /// sub moudule id
    uint16_t sub_id;
    /// connection index
    uint8_t  conidx;
    /// get result status
    uint16_t status;
    /// length of the data
    uint16_t len;
    /// attribute data
    uint8_t  data[__ARRAY_EMPTY];
};

/// lea set attribute request
/*@TRACE*/
struct lea_sub_req
{
    /// sub moudule id
    uint16_t sub_id;
    /// connection index
    uint8_t  conidx;
    /// length of the data
    uint16_t len;
    /// attribute data
    uint8_t  data[__ARRAY_EMPTY];
};

/// lea attribute data indicate
/*@TRACE*/
struct lea_data_ind
{
    /// attribute id
    uint16_t attr;
    /// connection index
    uint8_t  conidx;
    /// result status
    uint16_t status;
    /// length of the data
    uint16_t len;
    /// attribute data
    uint8_t  data[__ARRAY_EMPTY];
};

struct lea_ind_cfm
{
    /// attribute id
    uint16_t attr;
    /// connection index
    uint8_t  conidx;
    /// result status
    uint16_t status;
    /// length of the data
    uint16_t len;
    /// attribute data
    uint8_t  data[__ARRAY_EMPTY];
};

struct lea_event_ind
{
    /// status moudule id
    uint16_t event_id;
    /// get result status
    uint16_t status;
    /// length of the data
    uint16_t len;
    /// attribute data
    uint8_t  data[__ARRAY_EMPTY];
};

typedef struct ble_lea_cfg
{
    uint8_t lea_role;
    uint32_t svc_mask;
} ble_lea_cfg_t;

typedef struct codec_spec_cfg
{
    /// sample frequency,see@generic_codec_config_sample.
    uint8_t           sample_rate;
    /// frmae duration,see@generic_cfg_codec_frame_dur.
    uint8_t           frame_dur;
    /// octets per codec frame.
    uint16_t          oct_per_frame;
    /// codec frame blocks per sdu.
    uint8_t           frame_per_sdu;
    /// audio channel allocation,see@generic_aud_loca.
    uint32_t          ch_alloc;
}codec_spec_cfg_t;

typedef struct bap_iso_codec_info
{
    /// codec type,see@aud_codec_type.
    uint8_t codec;
    codec_spec_cfg_t codec_cfg;
}bap_iso_codec_info_t;

typedef struct ble_lea_cb
{
    /**
     ****************************************************************************************
     * @brief Reception of lea activate complete
     ****************************************************************************************
     */
    void (*cb_lea_enable_cmp)(uint16_t status);

    /**
     ****************************************************************************************
     * @brief Reception of lea discover service complete
     ****************************************************************************************
     */
    void (*cb_lea_svc_discover_cmp)(uint8_t conidx, uint16_t status);

    /**
     ****************************************************************************************
     * @brief Handles get complete event from the LEA
     ****************************************************************************************
     */
    void (*cb_lea_get_ind)(uint16_t attr, uint8_t conidx, uint16_t status, uint16_t len, uint8_t *p_data);

    /**
     ****************************************************************************************
     * @brief Handles set complete event from the LEA
     ****************************************************************************************
     */
    void (*cb_lea_set_ind)(uint16_t attr, uint8_t conidx, uint16_t status);

    /**
     ****************************************************************************************
     * @brief Handles sub complete event from the LEA
     ****************************************************************************************
     */
    void (*cb_lea_sub_ind)(uint16_t sub_id, uint8_t conidx, uint16_t status, uint16_t len, uint8_t *p_data);
    /**
     ****************************************************************************************
     * @brief Handles status indicate from the LEA
     ****************************************************************************************
     */
    void (*cb_lea_event_ind)(uint16_t event_id, uint16_t status, uint16_t len, uint8_t *data);

}ble_lea_cb_t;
#endif
