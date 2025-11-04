/**
 ****************************************************************************************
 *
 * @file bcss.c
 *
 * @brief Body Composition Service  implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 * $ Rev $
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup BCSS
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"
#if (BLE_BCS_SERVER)
#include "bcss.h"
#include "gap.h"
#include "gatt.h"
#include "ke_mem.h"
#include "co_utils.h"
#include "co_endian.h"

#include <string.h>


/*
 * DEFINES
 ****************************************************************************************
 */
/// Feature value size 32 bits
#define BCSS_FEAT_VAL_SIZE                  (4)

/// BCSS Attributes database handle list
enum bcss_att_db_handles
{
    /// Body Composition Service
    BCSS_IDX_SVC,
    /// Body Composition Feature Characteristic
    BCSS_IDX_FEAT_CHAR,
    BCSS_IDX_FEAT_VAL,
    /// Body Composition Measurement Characteristic
    BCSS_IDX_MEAS_CHAR,
    BCSS_IDX_MEAS_IND,
    /// CCC Descriptor
    BCSS_IDX_MEAS_CCC,

    /// Number of attributes
    BCSS_IDX_NB,
};


/*
 * TYPES DEFINITION
 ****************************************************************************************
 */

/// Body composition server environment variable
typedef struct bcss_env
{
    /// profile environment
    prf_hdr_t  prf_env;
    /// Operation Event TX wait queue
    co_list_t  wait_queue;
    /// Feature configuration
    uint32_t   feature;
    /// Service Attribute Start Handle
    uint16_t   start_hdl;
    /// GATT user local identifier
    uint8_t    user_lid;
    /// Operation On-going
    bool       op_ongoing;
    /// Prevent recursion in execute_operation function
    bool       in_exe_op;
    /// Used to know if all measurement paylaod has been sent
    bool       all_meas_sent;
    /// Indication configuration for each connections
    uint8_t    ind_cfg[BLE_CONNECTION_MAX];

} bcss_env_t;

/// ongoing operation information
typedef struct bcss_buf_meta
{
     /// Flag information of the buffer
     uint16_t flags;
     /// Mandatory information
     uint16_t body_fat_percent;
     /// Connection index of peer device
     uint8_t  conidx;
} bcss_buf_meta_t;

/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// default read perm
#define RD_P                                (PROP(RD))
/// default write perm
#define WR_P                                (PROP(WR))
/// ind perm
#define IND_P                               (PROP(I))

/// Full BCSS Database Description - Used to add attributes into the database
__STATIC const gatt_att16_desc_t bcss_att_db[BCSS_IDX_NB] =
{
    /// ATT Index                 | ATT UUID                             | Permission  |  MAX ATT SIZE
    // Weight SCale Service Declaration
    [BCSS_IDX_SVC]             = {GATT_DECL_PRIMARY_SERVICE,              RD_P,         0                                   },
    // Weight SCale Feature
    [BCSS_IDX_FEAT_CHAR]       = {GATT_DECL_CHARACTERISTIC,               RD_P,         0                                   },
    // Descriptor Value Changed Characteristic Value
    [BCSS_IDX_FEAT_VAL]        = {GATT_CHAR_BODY_COMPOSITION_FEATURE,     RD_P,         OPT(NO_OFFSET) | BCSS_FEAT_VAL_SIZE },
    // Weight SCale Measurement
    [BCSS_IDX_MEAS_CHAR]       = {GATT_DECL_CHARACTERISTIC,               RD_P,         0                                   },
    // Weight SCale Measurement Characteristic Value
    [BCSS_IDX_MEAS_IND]        = {GATT_CHAR_BODY_COMPOSITION_MEASUREMENT, IND_P,        BCS_MEAS_IND_SIZE                  },
    // Weight SCale Measurement Characteristic - Client Characteristic Configuration Descriptor
    [BCSS_IDX_MEAS_CCC]        = {GATT_DESC_CLIENT_CHAR_CFG,              RD_P | WR_P,  OPT(NO_OFFSET) | sizeof(uint16_t)   },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @brief Get Mass Resolution from feature value
 *
 * @param[in] feature Body Composition Feature value
 *
 * @return Mass Resolution
 ****************************************************************************************
 */
__STATIC uint8_t bcss_get_mass_resol(uint32_t feature)
{
    uint8_t mass_resol;

    // Imperial - Weight and Mass in units of pound (lb) and Height in units of inch (in)
    switch(GETF(feature, BCS_FEAT_MASS_RESOL))
    {
        case BCS_MASS_RESOL_05kg_1lb:
        {
            mass_resol = BCS_MASS_RESOL_05kg_1lb;
        } break;

        case BCS_MASS_RESOL_02kg_05lb:
        {
            mass_resol = BCS_MASS_RESOL_02kg_05lb;
        } break;

        case BCS_MASS_RESOL_01kg_02lb:
        {
            mass_resol = BCS_MASS_RESOL_01kg_02lb;
        } break;

        case BCS_MASS_RESOL_005kg_01lb:
        {
            mass_resol = BCS_MASS_RESOL_005kg_01lb;
        } break;

        case BCS_MASS_RESOL_002kg_005lb:
        {
            mass_resol = BCS_MASS_RESOL_002kg_005lb;
        } break;

        case BCS_MASS_RESOL_001kg_002lb:
        {
            mass_resol = BCS_MASS_RESOL_001kg_002lb;
        } break;

        case BCS_MASS_RESOL_0005kg_001lb:
        {
            mass_resol = BCS_MASS_RESOL_0005kg_001lb;
        } break;

        case BCS_MASS_RESOL_NOT_SPECIFIED:
        default:
        {
            mass_resol = BCS_MASS_RESOL_NOT_SPECIFIED;
        } break;
    }

    return mass_resol;
}

/**
 ****************************************************************************************
 * @brief Get Height Resolution from feature value
 *
 * @param[in] feature Body Composition Feature value
 *
 * @return Height Resolution
 ****************************************************************************************
 */
__STATIC uint8_t bcss_get_hght_resol(uint32_t feature)
{
    uint8_t hght_resol;

    // Imperial - Weight and Mass in units of pound (lb) and Height in units of inch (in)
    switch(GETF(feature, BCS_FEAT_HGHT_RESOL))
    {
        case BCS_HGHT_RESOL_001mtr_1inch:
        {
            hght_resol = BCS_HGHT_RESOL_001mtr_1inch;
        } break;

        case BCS_HGHT_RESOL_0005mtr_05inch:
        {
            hght_resol = BCS_HGHT_RESOL_0005mtr_05inch;
        } break;

        case BCS_HGHT_RESOL_0001mtr_01inch:
        {
            hght_resol = BCS_HGHT_RESOL_0001mtr_01inch;
        } break;

        case BCS_HGHT_RESOL_NOT_SPECIFIED:
        default:
        {
            hght_resol = BCS_HGHT_RESOL_NOT_SPECIFIED;
        } break;
    }

    return hght_resol;
}


/**
 ****************************************************************************************
 * @brief  This function fully manage indication of body composition measurement
 *         to peer(s) device(s) according to on-going operation requested by application:
 ****************************************************************************************
 */
__STATIC void bcss_exe_operation(void)
{
    bcss_env_t* p_bcss_env = PRF_ENV_GET(BCSS, bcss);

    if((p_bcss_env != NULL) && (!p_bcss_env->in_exe_op))
    {
        p_bcss_env->in_exe_op = true;

        while(!co_list_is_empty(&(p_bcss_env->wait_queue)) && !(p_bcss_env->op_ongoing))
        {
            uint16_t status = GAP_ERR_NO_ERROR;
            co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&(p_bcss_env->wait_queue));
            bcss_buf_meta_t* p_buf_meta = (bcss_buf_meta_t*) co_buf_metadata(p_buf);
            bool release = true;
            gatt_att_t att_info;
            att_info.hdl    = p_bcss_env->start_hdl + BCSS_IDX_MEAS_IND;
            att_info.length = co_buf_data_len(p_buf);
            p_bcss_env->all_meas_sent = true;

            if ((p_bcss_env->ind_cfg[p_buf_meta->conidx] & PRF_CLI_START_IND) == 0)
            {
                status = PRF_ERR_IND_DISABLED;
            }
            else if(att_info.length <= (L2CAP_LE_MTU_MIN - 3))
            {
                status = gatt_srv_event_send(p_buf_meta->conidx, p_bcss_env->user_lid, p_buf_meta->conidx,
                                             GATT_INDICATE, att_info.hdl, p_buf);
            }
            else
            {
                // MTU is unknown, ask for a reliable indication send
                status = gatt_srv_event_reliable_send(p_buf_meta->conidx, p_bcss_env->user_lid, p_buf_meta->conidx,
                                                      GATT_INDICATE, 1, &att_info);
                if(status == GAP_ERR_NO_ERROR)
                {
                    // keep buffer in queue
                    co_list_push_back(&(p_bcss_env->wait_queue), &(p_buf->hdr));
                    p_bcss_env->all_meas_sent = false;
                    release = false;
                }
            }

            if(status == GAP_ERR_NO_ERROR)
            {
                p_bcss_env->op_ongoing = true;
            }
            else
            {
                const bcss_cb_t* p_cb = (const bcss_cb_t*) p_bcss_env->prf_env.p_cb;
                // Inform application that event has been sent
                p_cb->cb_meas_send_cmp(p_buf_meta->conidx, status);
            }

            if(release)
            {
                // release buffer
                co_buf_release(p_buf);
            }
        }

        p_bcss_env->in_exe_op = false;
    }
}

/*
 * GATT USER SERVICE HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief This function is called when peer want to read local attribute database value.
 *
 *        @see gatt_srv_att_read_get_cfm shall be called to provide attribute value
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] token         Procedure token that must be returned in confirmation function
 * @param[in] hdl           Attribute handle
 * @param[in] offset        Data offset
 * @param[in] max_length    Maximum data length to return
 ****************************************************************************************
 */
__STATIC void bcss_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    bcss_env_t* p_bcss_env = PRF_ENV_GET(BCSS, bcss);
    co_buf_t*   p_buf      = NULL;
    uint16_t    status     = PRF_APP_ERROR;

    if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, BCSS_FEAT_VAL_SIZE + GATT_BUFFER_TAIL_LEN) != GAP_ERR_NO_ERROR)
    {
        status = ATT_ERR_INSUFF_RESOURCE;
    }
    else
    {
        if(p_bcss_env != NULL)
        {
            uint16_t att_idx = hdl - p_bcss_env->start_hdl;
            status = GAP_ERR_NO_ERROR;
            switch(att_idx)
            {
                case BCSS_IDX_FEAT_VAL:
                {
                    co_write32p(co_buf_data(p_buf), co_htobl(p_bcss_env->feature));
                    co_buf_tail_reserve(p_buf, BCSS_FEAT_VAL_SIZE);
                } break;
                case BCSS_IDX_MEAS_CCC:
                {
                    co_write16p(co_buf_data(p_buf), co_htobs(p_bcss_env->ind_cfg[conidx]));
                    co_buf_tail_reserve(p_buf, 2);
                } break;
                default: { status = PRF_APP_ERROR; } break;
            }
        }
    }

    // Send result to peer device
    gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, co_buf_data_len(p_buf), p_buf);
    if(p_buf != NULL)
    {
        co_buf_release(p_buf);
    }
}


/**
 ****************************************************************************************
 * @brief This function is called during a write procedure to modify attribute handle.
 *
 *        @see gatt_srv_att_val_set_cfm shall be called to accept or reject attribute
 *        update.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] token         Procedure token that must be returned in confirmation function
 * @param[in] hdl           Attribute handle
 * @param[in] offset        Value offset
 * @param[in] p_data        Pointer to buffer that contains data to write starting from offset
 ****************************************************************************************
 */
__STATIC void bcss_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                  co_buf_t* p_data)
{
    bcss_env_t *p_bcss_env = PRF_ENV_GET(BCSS, bcss);
    uint16_t status = PRF_APP_ERROR;

    if(p_bcss_env != NULL)
    {
        if (co_buf_data_len(p_data) != sizeof(uint16_t))
        {
            status = PRF_ERR_UNEXPECTED_LEN;
        }
        else
        {
            // Extract value
            uint16_t ind_cfg = co_btohs(co_read16p(co_buf_data(p_data)));

             // Only update configuration if value for stop or notification enable
            if (   (hdl == (p_bcss_env->start_hdl + BCSS_IDX_MEAS_CCC))
                && ((ind_cfg == PRF_CLI_STOP_NTFIND) || (ind_cfg == PRF_CLI_START_IND)))
            {
                const bcss_cb_t* p_cb  = (const bcss_cb_t*) p_bcss_env->prf_env.p_cb;

                p_bcss_env->ind_cfg[conidx] = (uint8_t) ind_cfg;

                // Inform application about bond data update
                p_cb->cb_bond_data_upd(conidx, p_bcss_env->ind_cfg[conidx]);
                status = GAP_ERR_NO_ERROR;
            }
            else
            {
                status = PRF_APP_ERROR;
            }
        }
    }

    gatt_srv_att_val_set_cfm(conidx, user_lid, token, status);
}

/**
 ****************************************************************************************
 * @brief This function is called when GATT server user has initiated event send procedure,
 *
 *        @see gatt_srv_att_event_get_cfm shall be called to provide attribute value
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] token         Procedure token that must be returned in confirmation function
 * @param[in] dummy         Dummy parameter provided by upper layer for command execution.
 * @param[in] hdl           Attribute handle
 * @param[in] max_length    Maximum value length to return
 ****************************************************************************************
 */
__STATIC void bcss_cb_att_event_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t dummy, uint16_t hdl,
                                    uint16_t max_length)
{
    co_buf_t* p_buf = NULL;
    uint16_t status = GAP_ERR_NO_ERROR;
    bcss_env_t *p_bcss_env = PRF_ENV_GET(BCSS, bcss);
    if(p_bcss_env != NULL)
    {
        bcss_buf_meta_t* p_buf_meta;;
        uint16_t data_len;

        p_buf = (co_buf_t*) co_list_pick(&(p_bcss_env->wait_queue));
        p_buf_meta = (bcss_buf_meta_t*) co_buf_metadata(p_buf);
        data_len = co_buf_data_len(p_buf);

        if(max_length >= data_len)
        {
            co_list_pop_front(&(p_bcss_env->wait_queue));
            p_bcss_env->all_meas_sent = true;
        }
        else
        {
            uint16_t  pkt_len = data_len;
            co_buf_t* p_ind_buf = NULL;
            // duplicate buffer to transmit
            status = co_buf_duplicate(p_buf, &p_ind_buf, GATT_BUFFER_HEADER_LEN, GATT_BUFFER_TAIL_LEN);
            if(status == GAP_ERR_NO_ERROR)
            {
                uint16_t flags = p_buf_meta->flags;
                uint16_t next_segment_flags = 0;

                // Reduce number of field present
                do
                {
                    // Height
                    if (GETB(flags, BCS_MEAS_FLAGS_HEIGHT_PRESENT))
                    {
                        pkt_len -= sizeof(uint16_t) + 1;
                        SETB(next_segment_flags, BCS_MEAS_FLAGS_HEIGHT_PRESENT, 1);
                        if(pkt_len <= max_length) break;
                    }

                    // Weight
                    if (GETB(flags, BCS_MEAS_FLAGS_WEIGHT_PRESENT))
                    {
                        pkt_len -= sizeof(uint16_t) + 1;
                        SETB(next_segment_flags, BCS_MEAS_FLAGS_WEIGHT_PRESENT, 1);
                        if(pkt_len <= max_length) break;
                    }

                    // Impedance
                    if (GETB(flags, BCS_MEAS_FLAGS_IMPEDANCE_PRESENT))
                    {
                        pkt_len -= sizeof(uint16_t);
                        SETB(next_segment_flags, BCS_MEAS_FLAGS_IMPEDANCE_PRESENT, 1);
                        if(pkt_len <= max_length) break;
                    }

                    // Body Water Mass
                    if (GETB(flags, BCS_MEAS_FLAGS_BODY_WATER_MASS_PRESENT))
                    {
                        pkt_len -= sizeof(uint16_t);
                        SETB(next_segment_flags, BCS_MEAS_FLAGS_BODY_WATER_MASS_PRESENT, 1);
                        if(pkt_len <= max_length) break;
                    }

                    // Soft Lean Mass
                    if (GETB(flags, BCS_MEAS_FLAGS_SOFT_LEAN_MASS_PRESENT))
                    {
                        pkt_len -= sizeof(uint16_t);
                        SETB(next_segment_flags, BCS_MEAS_FLAGS_SOFT_LEAN_MASS_PRESENT, 1);
                        if(pkt_len <= max_length) break;
                    }

                    // Fat Free Mass
                    if (GETB(flags, BCS_MEAS_FLAGS_FAT_FREE_MASS_PRESENT))
                    {
                        pkt_len -= sizeof(uint16_t);
                        SETB(next_segment_flags, BCS_MEAS_FLAGS_FAT_FREE_MASS_PRESENT, 1);
                        if(pkt_len <= max_length) break;
                    }
                } while(0);

                // reduce size of buffers
                co_buf_tail_release(p_ind_buf, data_len - pkt_len);
                co_buf_head_release(p_buf, pkt_len);


                SETB(flags, BCS_MEAS_FLAGS_MULTIPACKET_MEAS, 1);

                // Set flag in payload
                co_write16p(co_buf_data(p_ind_buf), co_htobs(flags & ~next_segment_flags));
                SETB(next_segment_flags, BCS_MEAS_FLAGS_MULTIPACKET_MEAS, 1);

                // Mandatory Body Fat Percentage
                co_buf_head_reserve(p_buf, 2);
                co_write16p(co_buf_data(p_buf),     co_htobs(p_buf_meta->body_fat_percent));
                co_buf_head_reserve(p_buf, 2);
                co_write16p(co_buf_data(p_buf),     co_htobs(next_segment_flags));

                // change buffer pointer
                p_buf = p_ind_buf;
            }
        }
    }
    else
    {
        status = PRF_APP_ERROR;
    }

    // send data
    gatt_srv_att_event_get_cfm(conidx, user_lid, token, status, co_buf_data_len(p_buf), p_buf);
    if(p_buf != NULL)
    {
        co_buf_release(p_buf);
    }
}

/**
 ****************************************************************************************
 * @brief This function is called when GATT server user has initiated event send to peer
 *        device or if an error occurs.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] dummy         Dummy parameter provided by upper layer for command execution
 * @param[in] status        Status of the procedure (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void bcss_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Consider job done
    bcss_env_t *p_bcss_env = PRF_ENV_GET(BCSS, bcss);
    if((p_bcss_env != NULL) && (p_bcss_env->op_ongoing))
    {
        const bcss_cb_t* p_cb  = (const bcss_cb_t*) p_bcss_env->prf_env.p_cb;
        p_bcss_env->op_ongoing = false;

        // segment transmission failed or all mesurement sent
        if((status != GAP_ERR_NO_ERROR) || (p_bcss_env->all_meas_sent))
        {
            if(!p_bcss_env->all_meas_sent)
            {
                // release buffer
                co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&(p_bcss_env->wait_queue));
                co_buf_release(p_buf);
            }

            // Inform application that event has been sent
            p_cb->cb_meas_send_cmp(conidx, status);
        }

        // continue operation execution
        bcss_exe_operation();
    }
}

/// Service callback hander
__STATIC const gatt_srv_cb_t bcss_cb =
{
        .cb_event_sent    = bcss_cb_event_sent,
        .cb_att_read_get  = bcss_cb_att_read_get,
        .cb_att_event_get = bcss_cb_att_event_get,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = bcss_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */
uint16_t bcss_enable(uint8_t conidx, uint8_t ind_cfg)
{
    bcss_env_t *p_bcss_env = PRF_ENV_GET(BCSS, bcss);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_bcss_env != NULL)
    {
        // check state of the task
        if (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL)
        {
            p_bcss_env->ind_cfg[conidx] = ind_cfg;
            status = GAP_ERR_NO_ERROR;
        }
    }

    return (status);
}

uint16_t bcss_meas_send(uint8_t conidx, const bcs_meas_t* p_meas)
{
    bcss_env_t *p_bcss_env = PRF_ENV_GET(BCSS, bcss);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_bcss_env != NULL)
    {
        if ((p_bcss_env->ind_cfg[conidx] & PRF_CLI_START_IND) == 0)
        {
            status = PRF_ERR_IND_DISABLED;
        }
        // Parameter sanity check
        else if (p_meas != NULL)
        {
            co_buf_t* p_buf = NULL;
            status = co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0,
                                                   BCS_MEAS_IND_SIZE + GATT_BUFFER_TAIL_LEN);

            if(status == CO_BUF_ERR_NO_ERROR)
            {

                bcss_buf_meta_t* p_buf_meta  = (bcss_buf_meta_t*) co_buf_metadata(p_buf);
                p_buf_meta->conidx           = conidx;
                p_buf_meta->body_fat_percent = p_meas->body_fat_percent;

                // Mask off any illegal bits in the flags field
                uint16_t flags              = p_meas->flags & BCS_MEAS_FLAGS_VALID;

                if (p_meas->body_fat_percent == BCS_MEAS_UNSUCCESSFUL)
                {
                    // Disable all other optional fields other than Timestamp and User ID
                    SETB(flags, BCS_MEAS_FLAGS_BASAL_METAB_PRESENT,     0);
                    SETB(flags, BCS_MEAS_FLAGS_MUSCLE_PERCENT_PRESENT,  0);
                    SETB(flags, BCS_MEAS_FLAGS_MUSCLE_MASS_PRESENT,     0);
                    SETB(flags, BCS_MEAS_FLAGS_FAT_FREE_MASS_PRESENT,   0);
                    SETB(flags, BCS_MEAS_FLAGS_SOFT_LEAN_MASS_PRESENT,  0);
                    SETB(flags, BCS_MEAS_FLAGS_BODY_WATER_MASS_PRESENT, 0);
                    SETB(flags, BCS_MEAS_FLAGS_IMPEDANCE_PRESENT,       0);
                    SETB(flags, BCS_MEAS_FLAGS_WEIGHT_PRESENT,          0);
                    SETB(flags, BCS_MEAS_FLAGS_HEIGHT_PRESENT,          0);
                }

                //**************************************************************
                // Encode the Fields of the  Measurement
                // if the Application provide flags and fields which do not correspond
                // to the features declared by the server, we adjust the flags field to
                // ensure we only Indicate with fields compatible with our features.
                // Thus the flags fields is encoded last as it will be modified by checks on
                // features.
                //
                // If the Fields provided require 2 ATT_HANDLE_VALUE_INDs. The following will
                // always be contained in the first message (if selected in flags field).
                //          1/ Flags
                //          2/ Body Fat (mandatory)
                //          3/ Measurement Units (retrieve from flags)
                //          4/ Time Stamp
                //          5/ User Id
                //          6/ Basal Metabolism
                //          7/ Muscle Percentage
                //          8/ Muscle Mass
                //********************************************************************
                // reserve 2 bytes for Flags
                co_buf_tail_reserve(p_buf, sizeof(uint16_t));


                // Mandatory Body Fat Percentage
                co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->body_fat_percent));
                co_buf_tail_reserve(p_buf, sizeof(uint16_t));

                // Measurement Units
                // 0 for SI and 1 for Imperial
                co_buf_tail(p_buf)[0] = GETB(flags, BCS_MEAS_FLAGS_UNITS_IMPERIAL);
                co_buf_tail_reserve(p_buf, 1);

                // We always include the Time-Stamp and User Id in the first message - if segmenting.
                // Time stamp shall be included in flags field if the Server supports Time Stamp feature
                if (GETB(p_bcss_env->feature, BCS_FEAT_TIME_STAMP_SUPP))
                {
                    SETB(flags, BCS_MEAS_FLAGS_TIMESTAMP_PRESENT, 1);
                    prf_pack_date_time(p_buf, &(p_meas->time_stamp));
                }
                else
                {
                    // Shall not be included if the Time Stamp feature is not supported
                    SETB(flags, BCS_MEAS_FLAGS_TIMESTAMP_PRESENT, 0);
                }

                // User ID shall be included in flags field if the Server supports Multiple Users feature
                if (GETB(p_bcss_env->feature, BCS_FEAT_MULTIPLE_USERS_SUPP))
                {
                    SETB(flags, BCS_MEAS_FLAGS_USER_ID_PRESENT, 1);

                    co_buf_tail(p_buf)[0] = p_meas->user_id;
                    co_buf_tail_reserve(p_buf, 1);
                }
                else
                {
                    // Shall not be included if the Multiple Users feature is not supported
                    SETB(flags, BCS_MEAS_FLAGS_USER_ID_PRESENT, 0);
                }

                // Basal Metabolism if present and enabled in the features.
                if (GETB(flags, BCS_MEAS_FLAGS_BASAL_METAB_PRESENT))
                {
                    if (GETB(p_bcss_env->feature, BCS_FEAT_BASAL_METAB_SUPP))
                    {
                        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->basal_metab));
                        co_buf_tail_reserve(p_buf, sizeof(uint16_t));
                    }
                    else
                    {
                        SETB(flags, BCS_MEAS_FLAGS_BASAL_METAB_PRESENT, 0);
                    }
                }

                // Muscle Percentage if present and enabled in the features.
                if (GETB(flags, BCS_MEAS_FLAGS_MUSCLE_PERCENT_PRESENT))
                {
                    if (GETB(p_bcss_env->feature, BCS_FEAT_MUSCLE_PERCENTAGE_SUPP))
                    {
                        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->muscle_percent));
                        co_buf_tail_reserve(p_buf, sizeof(uint16_t));
                    }
                    else
                    {
                        SETB(flags, BCS_MEAS_FLAGS_MUSCLE_PERCENT_PRESENT, 0);
                    }
                }

                // Muscle Mass if present and enabled in the features.
                if (GETB(flags, BCS_MEAS_FLAGS_MUSCLE_MASS_PRESENT))
                {
                    if (GETB(p_bcss_env->feature, BCS_FEAT_MUSCLE_MASS_SUPP))
                    {
                        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->muscle_mass));
                        co_buf_tail_reserve(p_buf, sizeof(uint16_t));
                    }
                    else
                    {
                        SETB(flags, BCS_MEAS_FLAGS_MUSCLE_MASS_PRESENT, 0);
                    }
                }

                // Fat Free Mass if present and enabled in the features.
                if (GETB(flags, BCS_MEAS_FLAGS_FAT_FREE_MASS_PRESENT))
                {
                    if (GETB(p_bcss_env->feature, BCS_FEAT_FAT_FREE_MASS_SUPP))
                    {
                        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->fat_free_mass));
                        co_buf_tail_reserve(p_buf, sizeof(uint16_t));
                    }
                    else
                    {
                        SETB(flags, BCS_MEAS_FLAGS_FAT_FREE_MASS_PRESENT, 0);
                    }
                }

                // Soft Lean Mass if present and enabled in the features.
                if (GETB(flags, BCS_MEAS_FLAGS_SOFT_LEAN_MASS_PRESENT))
                {
                    if (GETB(p_bcss_env->feature, BCS_FEAT_SOFT_LEAN_MASS_SUPP))
                    {
                        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->soft_lean_mass));
                        co_buf_tail_reserve(p_buf, sizeof(uint16_t));
                    }
                    else
                    {
                        SETB(flags, BCS_MEAS_FLAGS_SOFT_LEAN_MASS_PRESENT, 0);
                    }
                }

                // Body Water Mass if present and enabled in the features.
                if (GETB(flags, BCS_MEAS_FLAGS_BODY_WATER_MASS_PRESENT))
                {
                    if (GETB(p_bcss_env->feature, BCS_FEAT_BODY_WATER_MASS_SUPP))
                    {
                        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->body_water_mass));
                        co_buf_tail_reserve(p_buf, sizeof(uint16_t));
                    }
                    else
                    {
                        SETB(flags, BCS_MEAS_FLAGS_BODY_WATER_MASS_PRESENT, 0);
                    }
                }

                // Impedance if present and enabled in the features.
                if (GETB(flags, BCS_MEAS_FLAGS_IMPEDANCE_PRESENT))
                {
                    if (GETB(p_bcss_env->feature, BCS_FEAT_IMPEDANCE_SUPP))
                    {
                        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->impedance));
                        co_buf_tail_reserve(p_buf, sizeof(uint16_t));
                    }
                    else
                    {
                        SETB(flags, BCS_MEAS_FLAGS_IMPEDANCE_PRESENT, 0);
                    }
                }

                // Weight if present and enabled in the features.
                if (GETB(flags, BCS_MEAS_FLAGS_WEIGHT_PRESENT))
                {
                    if (GETB(p_bcss_env->feature, BCS_FEAT_WEIGHT_SUPP))
                    {
                        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->weight));
                        co_buf_tail_reserve(p_buf, sizeof(uint16_t));
                        // Get Mass Resolution
                        co_buf_tail(p_buf)[0] = bcss_get_mass_resol(p_bcss_env->feature);
                        co_buf_tail_reserve(p_buf, 1);
                    }
                    else
                    {
                        SETB(flags, BCS_MEAS_FLAGS_WEIGHT_PRESENT, 0);
                    }
                }

                // Height if present and enabled in the features.
                if (GETB(flags, BCS_MEAS_FLAGS_HEIGHT_PRESENT))
                {
                    if (GETB(p_bcss_env->feature, BCS_FEAT_HEIGHT_SUPP))
                    {
                        co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->height));
                        co_buf_tail_reserve(p_buf, sizeof(uint16_t));
                        // Get Height Resolution
                        co_buf_tail(p_buf)[0] = bcss_get_hght_resol(p_bcss_env->feature);
                        co_buf_tail_reserve(p_buf, 1);
                    }
                    else
                    {
                        SETB(flags, BCS_MEAS_FLAGS_HEIGHT_PRESENT, 0);
                    }
                }

                // Set flag in payload
                co_write16p(co_buf_data(p_buf), co_htobs(flags));
                p_buf_meta->flags           = flags;

                // put event on wait queue
                co_list_push_back(&(p_bcss_env->wait_queue), &(p_buf->hdr));
                // execute operation
                bcss_exe_operation();
            }
            else
            {
                status = GAP_ERR_INSUFF_RESOURCES;
            }
        }
        else
        {
            status = PRF_ERR_INVALID_PARAM;
        }
    }

    return (status);
}

#if (BLE_HL_MSG_API)
/*
 * PROFILE MSG HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref BCSS_ENABLE_REQ message.
 *
 * @param[in] msgid Id of the message received
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance
 * @param[in] src_id ID of the sending task instance.
 *
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int bcss_enable_req_handler(ke_msg_id_t const msgid,
                                    struct bcss_enable_req *p_param,
                                    ke_task_id_t const dest_id,
                                    ke_task_id_t const src_id)
{
    struct bcss_enable_rsp *p_rsp;
    // Request status
    uint16_t status = bcss_enable(p_param->conidx, p_param->ind_cfg);

    // send response to application
    p_rsp = KE_MSG_ALLOC(BCSS_ENABLE_RSP, src_id, dest_id, bcss_enable_rsp);
    if(p_rsp)
    {
        p_rsp->conidx = p_param->conidx;
        p_rsp->status = status;

        ke_msg_send(p_rsp);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref BCSS_MEAS_INDICATE_CMD message.
 * Send MEASUREMENT INDICATION to the connected peer case of CCC enabled
 *
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 *
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int bcss_meas_indicate_cmd_handler(ke_msg_id_t const msgid,
                                          struct bcss_meas_indicate_cmd *p_param,
                                          ke_task_id_t const dest_id,
                                          ke_task_id_t const src_id)
{
    uint16_t status = bcss_meas_send(p_param->conidx, &(p_param->meas));

    if(status != GAP_ERR_NO_ERROR)
    {
        // an error occurs, trigger it.
        struct bcss_cmp_evt *p_cmp_evt = KE_MSG_ALLOC(BCSS_CMP_EVT, src_id, dest_id, bcss_cmp_evt);
        if(p_cmp_evt != NULL)
        {
            p_cmp_evt->conidx    = p_param->conidx;
            p_cmp_evt->operation = BCSS_MEAS_INDICATE_CMD_OP_CODE;
            p_cmp_evt->status    = status;
            ke_msg_send(p_cmp_evt);
        }
    }

    return KE_MSG_CONSUMED;
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(bcss)
{
    // Note: all messages must be sorted in ID ascending order

    {BCSS_ENABLE_REQ,          (ke_msg_func_t) bcss_enable_req_handler},
    {BCSS_MEAS_INDICATE_CMD,   (ke_msg_func_t) bcss_meas_indicate_cmd_handler},
};

/**
 ****************************************************************************************
 * @brief Completion of BCS sensor measurement transmission
 *
 * @param[in] conidx        Connection index
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
void bcss_cb_meas_send_cmp(uint8_t conidx, uint16_t status)
{
    // an error occurs, trigger it.
    struct bcss_cmp_evt *p_cmp_evt = KE_MSG_ALLOC(BCSS_CMP_EVT, PRF_DST_TASK(BCSS),
                                                  PRF_SRC_TASK(BCSS), bcss_cmp_evt);
    if(p_cmp_evt != NULL)
    {
        p_cmp_evt->conidx    = conidx;
        p_cmp_evt->operation = BCSS_MEAS_INDICATE_CMD_OP_CODE;
        p_cmp_evt->status    = status;
        ke_msg_send(p_cmp_evt);
    }
}

/**
 ****************************************************************************************
 * @brief Inform that bond data updated for the connection.
 *
 * @param[in] conidx        Connection index
 * @param[in] ind_cfg       Indication configuration
 ****************************************************************************************
 */
void bcss_cb_bond_data_upd(uint8_t conidx, uint8_t ind_cfg)
{
    // an error occurs, trigger it.
    struct bcss_meas_ccc_ind *p_ind = KE_MSG_ALLOC(BCSS_MEAS_CCC_IND, PRF_DST_TASK(BCSS),
                                                   PRF_SRC_TASK(BCSS), bcss_meas_ccc_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx    = conidx;
        p_ind->ind_cfg   = ind_cfg;
        ke_msg_send(p_ind);
    }
}

/// Default Message handle
__STATIC const bcss_cb_t bcss_msg_cb =
{
    .cb_meas_send_cmp = bcss_cb_meas_send_cmp,
    .cb_bond_data_upd = bcss_cb_bond_data_upd,
};
#endif // (BLE_HL_MSG_API)

/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the BCSS module.
 * This function performs all the initializations of the Profile module.
 *  - Creation of database (if it's a service)
 *  - Allocation of profile required memory
 *  - Initialization of task descriptor to register application
 *      - Task State array
 *      - Number of tasks
 *      - Default task handler
 *
 * @param[out]    p_env        Collector or Service allocated environment data.
 * @param[in|out] p_start_hdl  Service start handle (0 - dynamically allocated), only applies for services.
 * @param[in]     sec_lvl      Security level (@see enum gatt_svc_info_bf)
 * @param[in]     user_prio    GATT User priority
 * @param[in]     p_param      Configuration parameters of profile collector or service (32 bits aligned)
 * @param[in]     p_cb         Callback structure that handles event from profile
 *
 * @return status code to know if profile initialization succeed or not.
 ****************************************************************************************
 */
__STATIC uint16_t bcss_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct bcss_db_cfg *p_params, const bcss_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        gatt_att16_desc_t att_db[BCSS_IDX_NB];
        bcss_env_t* p_bcss_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(bcss_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_meas_send_cmp == NULL)
           || (p_cb->cb_bond_data_upd == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register BCSS user
        status = gatt_user_srv_register(BCS_MEAS_IND_SIZE, user_prio, &bcss_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        // copy attribute database to update service type
        memcpy(att_db, bcss_att_db, sizeof(bcss_att_db));

        // update service type
        if(p_params->secondary_service)
        {
            att_db[0].uuid16 = GATT_DECL_SECONDARY_SERVICE;
        }

        // Add GAP service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_BODY_COMPOSITION, BCSS_IDX_NB,
                                   NULL, &(att_db[0]), BCSS_IDX_NB, p_start_hdl);

        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_bcss_env = (bcss_env_t *) ke_malloc(sizeof(bcss_env_t), KE_MEM_ATT_DB);

        if(p_bcss_env != NULL)
        {
            // allocate BCSS required environment variable
            p_env->p_env = (prf_hdr_t *) p_bcss_env;
            p_bcss_env->start_hdl = *p_start_hdl;
            p_bcss_env->feature   = p_params->feature;
            p_bcss_env->user_lid  = user_lid;
            memset(p_bcss_env->ind_cfg, 0, sizeof(p_bcss_env->ind_cfg));
            p_bcss_env->op_ongoing = false;
            p_bcss_env->in_exe_op  = false;
            co_list_init(&(p_bcss_env->wait_queue));

            // initialize profile environment variable
            p_bcss_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = bcss_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(bcss_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)
        }
        else
        {
            status = GAP_ERR_INSUFF_RESOURCES;
        }

    } while(0);

    if((status != GAP_ERR_NO_ERROR) && (user_lid != GATT_INVALID_USER_LID))
    {
        gatt_user_unregister(user_lid);
    }

    return (status);
}

/**
 ****************************************************************************************
 * @brief Destruction of the profile module - due to a reset or profile remove.
 *
 * This function clean-up allocated memory.
 *
 * @param[in|out]    p_env        Collector or Service allocated environment data.
 * @param[in]        reason       Destroy reason (@see enum prf_destroy_reason)
 *
 * @return status of the destruction, if fails, profile considered not removed.
 ****************************************************************************************
 */
__STATIC uint16_t bcss_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    bcss_env_t *p_bcss_env = (bcss_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_bcss_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        if(reason != PRF_DESTROY_RESET)
        {
            // remove buffer in wait queue
            while(!co_list_is_empty(&p_bcss_env->wait_queue))
            {
                co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&p_bcss_env->wait_queue);
                co_buf_release(p_buf);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_bcss_env);
    }

    return (status);
}

/**
 ****************************************************************************************
 * @brief @brief Handles Connection creation
 *
 * @param[in|out]    env          Collector or Service allocated environment data.
 * @param[in]        conidx       Connection index
 * @param[in]        p_con_param  Pointer to connection parameters information
 ****************************************************************************************
 */
__STATIC void bcss_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    bcss_env_t *p_bcss_env = (bcss_env_t *) p_env->p_env;
    p_bcss_env->ind_cfg[conidx] = 0;
}

/**
 ****************************************************************************************
 * @brief Handles Disconnection
 *
 * @param[in|out]    p_env      Collector or Service allocated environment data.
 * @param[in]        conidx     Connection index
 * @param[in]        reason     Detach reason
 ****************************************************************************************
 */
__STATIC void bcss_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    bcss_env_t *p_bcss_env = (bcss_env_t *) p_env->p_env;
    p_bcss_env->ind_cfg[conidx] = 0;
}



/// BCSS Task interface required by profile manager
const prf_task_cbs_t bcss_itf =
{
    .cb_init          = (prf_init_cb) bcss_init,
    .cb_destroy       = bcss_destroy,
    .cb_con_create    = bcss_con_create,
    .cb_con_cleanup   = bcss_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *bcss_prf_itf_get(void)
{
    return &bcss_itf;
}



#endif //(BLE_BCS_SERVER)

/// @} BCSS
