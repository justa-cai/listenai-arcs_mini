/**
 ****************************************************************************************
 *
 * @file lans.c
 *
 * @brief Location and Navigation Profile Sensor implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup LANS
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "btip_config.h"

#if (BLE_LN_SENSOR)

#include "lan_common.h"

#include "lans.h"
#include "gap.h"
#include "gatt.h"
#include "ble_prf.h"

#include "co_utils.h"
#include "co_endian.h"

#include <string.h>
#include "ke_mem.h"

/*
 * DEFINES
 ****************************************************************************************
 */

/********************************************
 ******* LANS Configuration Flag Masks ******
 ********************************************/

/// Disabled fields of Mask Location and Speed Characteristic Content
/// Save Bit 0 - 6 to mask Location and Speed Characteristic Flags
#define LANS_LSPEED_DISABLED_MASK       (0x7F)


/// LAN Control Point Value Response buffer head length
#define LANS_LAN_LN_CNTL_PT_RSP_BUFFER_HEAD_LEN (6)


/// Location and Navigation Service - Attribute List
enum lans_cps_att_list
{
    /// Location and Navigation Service
    LNS_IDX_SVC,
    /// LN Feature
    LNS_IDX_LN_FEAT_CHAR,
    LNS_IDX_LN_FEAT_VAL,
    /// Location and Speed
    LNS_IDX_LOC_SPEED_CHAR,
    LNS_IDX_LOC_SPEED_VAL,
    LNS_IDX_LOC_SPEED_NTF_CFG,
    /// Position Quality
    LNS_IDX_POS_Q_CHAR,
    LNS_IDX_POS_Q_VAL,
    /// LN Control Point
    LNS_IDX_LN_CTNL_PT_CHAR,
    LNS_IDX_LN_CTNL_PT_VAL,
    LNS_IDX_LN_CTNL_PT_IND_CFG,
    /// Navigation
    LNS_IDX_NAVIGATION_CHAR,
    LNS_IDX_NAVIGATION_VAL,
    LNS_IDX_NAVIGATION_NTF_CFG,

    /// Number of attributes
    LNS_IDX_NB,
};

/// Profile Configuration
enum lans_prf_cfg_flag_bf
{
    /// Control Point supported flag
    LANS_CTNL_PT_CHAR_SUPP_FLAG_POS         = 0,
    LANS_CTNL_PT_CHAR_SUPP_FLAG_BIT         = CO_BIT(LANS_CTNL_PT_CHAR_SUPP_FLAG_POS),

    /// Broadcast supported flag
    LANS_NAVIGATION_SUPP_FLAG_POS           = 1,
    LANS_NAVIGATION_SUPP_FLAG_BIT           = CO_BIT(LANS_NAVIGATION_SUPP_FLAG_POS),
};

/// Profile Configuration NTF/IND Flags bit field
enum lans_prf_cfg_ntfind_flag_bf
{
    /// Location and Speed - Client Char. Cfg
    LANS_PRF_CFG_FLAG_LOC_SPEED_NTF_POS         = 0,
    LANS_PRF_CFG_FLAG_LOC_SPEED_NTF_BIT         = CO_BIT(LANS_PRF_CFG_FLAG_LOC_SPEED_NTF_POS),

    /// Control Point - Client Characteristic configuration
    LANS_PRF_CFG_FLAG_LN_CTNL_PT_IND_POS        = 1,
    LANS_PRF_CFG_FLAG_LN_CTNL_PT_IND_BIT        = CO_BIT(LANS_PRF_CFG_FLAG_LN_CTNL_PT_IND_POS),


    /// Navigation - Client Characteristic configuration
    LANS_PRF_CFG_FLAG_NAVIGATION_NTF_POS        = 2,
    LANS_PRF_CFG_FLAG_NAVIGATION_NTF_BIT        = CO_BIT(LANS_PRF_CFG_FLAG_NAVIGATION_NTF_POS),

    /// Bonded Data configured
    LANS_PRF_CFG_PERFORMED_OK_POS               = 3,
    LANS_PRF_CFG_PERFORMED_OK_BIT               = CO_BIT(LANS_PRF_CFG_PERFORMED_OK_POS),
};

/// LANS Configuration Flag Masks
enum lans_config_flag_mask
{
    /// Mandatory Attributes (LN Feature + Location and Speed)
    LANS_MANDATORY_LSB            = 0,
    LANS_MANDATORY_MASK           = (0x003F),

    /// Position quality Attribute
    LANS_POS_Q_LSB                = 6,
    LANS_POS_Q_MASK               = (0x00C0),

    /// Control Point Attributes
    LANS_LN_CTNL_PT_LSB           = 8,
    LANS_LN_CTNL_PT_MASK          = (0x0700),

    /// Navigation Attributes
    LANS_NAVI_LSB                 = 11,
    LANS_NAVI_MASK                = (0x3800),
};

/// Navigation control enable bit field
enum lans_navi_ctrl_en_bf
{
    /// Enable bit (RFU bit 7)
    LANS_NAV_CTRL_PT_EN_POS       = 7,
    LANS_NAV_CTRL_PT_EN_BIT       = CO_BIT(LANS_NAV_CTRL_PT_EN_POS),
};

/*
 * MACROS
 ****************************************************************************************
 */

/// Check if a specific characteristic has been added in the database (according to mask)
#define LANS_IS_FEATURE_SUPPORTED(prfl_cfg, mask)  ((prfl_cfg & mask) == mask)

/// MACRO TO CALCULATE HANDLE    shdl + idx - POSQ - CNTL PT
/// POSQ is 2 if the Position quality is supported otherwise 0
/// CNTL PT is 3 if the CP characteristic is supported otherwise 0
#define LANS_HANDLE(idx) \
    (p_lans_env->start_hdl + (idx) - \
        ((!(LANS_IS_FEATURE_SUPPORTED(p_lans_env->prfl_cfg, LANS_POS_Q_MASK)) && \
                ((idx) > LNS_IDX_POS_Q_CHAR))? (2) : (0)) - \
        ((!(LANS_IS_FEATURE_SUPPORTED(p_lans_env->prfl_cfg, LANS_LN_CTNL_PT_MASK)) && \
                ((idx) > LNS_IDX_LN_CTNL_PT_CHAR))? (3) : (0)))

/*
 * TYPES DEFINITION
 ****************************************************************************************
 */


/// ongoing operation information
typedef struct lans_buf_meta
{
    /// meaningful for some operation
    uint32_t  conidx_bf;
    /// Operation
    uint8_t   operation;
    /// Connection index targeted
    uint8_t   conidx;
    /// last transmitted information flag
    uint16_t  last_flag;
    /// True if measurement must be sent onto a new connection.
    bool      new;
} lans_buf_meta_t;


/// Location and Navigation environment variable per connection
typedef struct lans_cnx_env
{
    /// Location and speed content mask (bit 0 - 6)
    /// 0 : leave as default, 1 : turn off
    uint16_t mask_lspeed_content;
    /// Profile Notify/Indication Flags
    uint8_t  prfl_ntf_ind_cfg;
    /// LN Navigation Control parameter (value 0 - 5)
    uint8_t  nav_ctrl;
} lans_cnx_env_t;

/// location and navigation server environment variable
typedef struct lans_env
{
    /// profile environment
    prf_hdr_t       prf_env;
    /// Position quality information
    lanp_posq_t     pos_q;
    /// Operation Event TX wait queue
    co_list_t       wait_queue;
    /// Services features
    uint32_t        features;
    /// Service Attribute Start Handle
    uint16_t        start_hdl;
    /// Profile Configuration Flags
    /// Every bit corresponds an attribute
    uint16_t        prfl_cfg;
    /// Control point update data content (for LANS_CTNL_PT_RSP_SEND_OP_CODE or LANS_NAVIGATION_CONTROL_OP_CODE)
    uint16_t        ctnl_upd_content;
    /// GATT user local identifier
    uint8_t         user_lid;
    /// Control point operation on-going (@see enum lanp_ln_ctnl_pt_code)
    uint8_t         ctrl_pt_op;
    /// Operation On-going
    bool            op_ongoing;
    /// Prevent recursion in execute_operation function
    bool            in_exe_op;
    /// Environment variable pointer for each connections
    lans_cnx_env_t  env[BLE_CONNECTION_MAX];

} lans_env_t;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full Database Description - Used to add attributes into the database
const gatt_att16_desc_t lans_att_db[LNS_IDX_NB] =
{
    // Location and Navigation Service Declaration
    [LNS_IDX_SVC]                = { GATT_DECL_PRIMARY_SERVICE,  PROP(RD),            0                                                },

    // LN Feature Characteristic Declaration
    [LNS_IDX_LN_FEAT_CHAR]       = { GATT_DECL_CHARACTERISTIC,   PROP(RD),            0                                                },
    // LN Feature Characteristic Value
    [LNS_IDX_LN_FEAT_VAL]        = { GATT_CHAR_LN_FEAT,          PROP(RD),            OPT(NO_OFFSET)                                   },

    // Location and Speed Characteristic Declaration
    [LNS_IDX_LOC_SPEED_CHAR]     = { GATT_DECL_CHARACTERISTIC,   PROP(RD),            0                                                },
    // Location and Speed Characteristic Value
    [LNS_IDX_LOC_SPEED_VAL]      = { GATT_CHAR_LOC_SPEED,        PROP(N),             OPT(NO_OFFSET)                                   },
    // Location and Speed Characteristic - Client Characteristic Configuration Descriptor
    [LNS_IDX_LOC_SPEED_NTF_CFG]  = { GATT_DESC_CLIENT_CHAR_CFG,  PROP(RD) | PROP(WR), OPT(NO_OFFSET)                                   },

    // Position Quality Characteristic Declaration
    [LNS_IDX_POS_Q_CHAR]         = { GATT_DECL_CHARACTERISTIC,   PROP(RD),            0                                                },
    // Position Quality Characteristic Value
    [LNS_IDX_POS_Q_VAL]          = { GATT_CHAR_POS_QUALITY,      PROP(RD),            OPT(NO_OFFSET)                                   },

    // LN Control Point Characteristic Declaration
    [LNS_IDX_LN_CTNL_PT_CHAR]    = { GATT_DECL_CHARACTERISTIC,   PROP(RD),            0                                                },
    // LN Control Point Characteristic Value - The response has the maximal length
    [LNS_IDX_LN_CTNL_PT_VAL]     = { GATT_CHAR_LN_CNTL_PT,       PROP(I) | PROP(WR),  OPT(NO_OFFSET) | LANP_LAN_LN_CNTL_PT_REQ_MAX_LEN },
    // LN Control Point Characteristic - Client Characteristic Configuration Descriptor
    [LNS_IDX_LN_CTNL_PT_IND_CFG] = { GATT_DESC_CLIENT_CHAR_CFG,  PROP(RD) | PROP(WR), OPT(NO_OFFSET)                                   },

    // Navigation Characteristic Declaration
    [LNS_IDX_NAVIGATION_CHAR]    = { GATT_DECL_CHARACTERISTIC,   PROP(RD),            0                                                },
    // Navigation Characteristic Value
    [LNS_IDX_NAVIGATION_VAL]     = { GATT_CHAR_NAVIGATION,       PROP(N),             OPT(NO_OFFSET)                                   },
    // Navigation Characteristic - Client Characteristic Configuration Descriptor
    [LNS_IDX_NAVIGATION_NTF_CFG] = { GATT_DESC_CLIENT_CHAR_CFG,  PROP(RD) | PROP(WR), OPT(NO_OFFSET)                                   },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/// Get database attribute index
__STATIC uint8_t lans_idx_get(lans_env_t* p_lans_env, uint16_t hdl)
{
    uint8_t att_idx = hdl - p_lans_env->start_hdl;

    if(   (att_idx >= LNS_IDX_POS_Q_CHAR)
       && !LANS_IS_FEATURE_SUPPORTED(p_lans_env->prfl_cfg, LANS_POS_Q_MASK))
    {
        att_idx += 2;
    }

    if(   (att_idx >= LNS_IDX_LN_CTNL_PT_CHAR)
       && !LANS_IS_FEATURE_SUPPORTED(p_lans_env->prfl_cfg, LANS_LN_CTNL_PT_MASK))
    {
        att_idx += 3;
    }

    return att_idx;
}

/**
 ****************************************************************************************
 * @brief  This function fully manages notification of measurement and vector
 ****************************************************************************************
 */
__STATIC void lans_exe_operation(lans_env_t* p_lans_env)
{
    if(!p_lans_env->in_exe_op)
    {
        p_lans_env->in_exe_op = true;

        while(!co_list_is_empty(&(p_lans_env->wait_queue)) && !(p_lans_env->op_ongoing))
        {
            uint16_t status = GAP_ERR_NO_ERROR;
            co_buf_t* p_buf = (co_buf_t*) co_list_pick(&(p_lans_env->wait_queue));
            lans_buf_meta_t* p_meta = (lans_buf_meta_t*) co_buf_metadata(p_buf);
            uint8_t  conidx;

            switch(p_meta->operation)
            {
                case LANS_NTF_LOC_SPEED_OP_CODE:
                {
                    if(p_meta->new)
                    {
                        p_meta->conidx = GAP_INVALID_CONIDX;
                        p_meta->last_flag = 0;

                        while(p_meta->conidx_bf != 0)
                        {
                            // retrieve first valid bit
                            uint8_t conidx = co_ctz(p_meta->conidx_bf);
                            p_meta->conidx_bf &= ~CO_BIT(conidx);

                            // check if notification enabled
                            if (GETB(p_lans_env->env[conidx].prfl_ntf_ind_cfg, LANS_PRF_CFG_FLAG_LOC_SPEED_NTF))
                            {
                                p_meta->conidx = conidx;
                                break;
                            }
                        }
                    }

                    // Use Reliable write
                    if(p_meta->conidx != GAP_INVALID_CONIDX)
                    {
                        gatt_att_t att_info;
                        att_info.hdl    = LANS_HANDLE(LNS_IDX_LOC_SPEED_VAL);
                        att_info.length = LANP_LAN_LOC_SPEED_MAX_LEN;

                        status = gatt_srv_event_reliable_send(p_meta->conidx, p_lans_env->user_lid, LANS_NTF_LOC_SPEED_OP_CODE,
                                                              GATT_NOTIFY, 1, &att_info);

                        if(status == GAP_ERR_NO_ERROR)
                        {
                            p_lans_env->op_ongoing = true;
                        }
                    }
                    else
                    {
                        const lans_cb_t* p_cb = (const lans_cb_t*) p_lans_env->prf_env.p_cb;
                        // Inform application that event has been sent
                        p_cb->cb_loc_speed_send_cmp(GAP_ERR_NO_ERROR);

                        // remove buffer from queue
                        co_list_pop_front(&(p_lans_env->wait_queue));
                        co_buf_release(p_buf);
                    }
                } break;

                case LANS_NTF_NAVIGATION_OP_CODE:
                {
                    uint32_t conidx_bf = 0;

                    // remove buffer from queue
                    co_list_pop_front(&(p_lans_env->wait_queue));

                    // check connection that support notification reception
                    for(conidx = 0 ; conidx < BLE_CONNECTION_MAX ; conidx++)
                    {
                        if(   GETB(p_lans_env->env[conidx].prfl_ntf_ind_cfg, LANS_PRF_CFG_FLAG_NAVIGATION_NTF)
                           && GETB(p_lans_env->env[conidx].nav_ctrl, LANS_NAV_CTRL_PT_EN))
                        {
                            conidx_bf |= CO_BIT(conidx);
                        }
                    }

                    // send notification only on selected connections
                    conidx_bf &= p_meta->conidx_bf;

                    if(conidx_bf != 0)
                    {
                        // send multi-point notification
                        status = gatt_srv_event_mtp_send(conidx_bf, p_lans_env->user_lid, LANS_NTF_NAVIGATION_OP_CODE,
                                                         GATT_NOTIFY, LANS_HANDLE(LNS_IDX_NAVIGATION_VAL), p_buf, true);
                        if(status == GAP_ERR_NO_ERROR)
                        {
                            p_lans_env->op_ongoing = true;
                        }
                    }

                    co_buf_release(p_buf);

                    if(!p_lans_env->op_ongoing)
                    {
                        const lans_cb_t* p_cb = (const lans_cb_t*) p_lans_env->prf_env.p_cb;
                        // Inform application that event has been sent
                        p_cb->cb_navigation_send_cmp(status);
                    }
                } break;

                default:
                {
                    // remove buffer from queue
                    co_list_pop_front(&(p_lans_env->wait_queue));

                    conidx = p_meta->conidx;

                    status = gatt_srv_event_send(conidx, p_lans_env->user_lid, p_meta->operation, GATT_INDICATE,
                                                 LANS_HANDLE(LNS_IDX_LN_CTNL_PT_VAL), p_buf);

                    if(status == GAP_ERR_NO_ERROR)
                    {
                        p_lans_env->op_ongoing = true;
                    }
                    else
                    {
                        // Inform application that control point response has been sent
                        if (p_lans_env->ctrl_pt_op != LANP_LN_CTNL_PT_RESPONSE_CODE)
                        {
                            const lans_cb_t* p_cb = (const lans_cb_t*) p_lans_env->prf_env.p_cb;
                            p_cb->cb_ctnl_pt_rsp_send_cmp(conidx, status);
                        }

                        // consider control point operation done
                        p_lans_env->ctrl_pt_op = LANP_LN_CTNL_PT_RESERVED;
                    }

                    co_buf_release(p_buf);
                } break;
            }
        }

        p_lans_env->in_exe_op = false;
    }
}

/**
 ****************************************************************************************
 * @brief Packs location and speed notifications
 *
 * @param[in]     p_lans_env    Environment data
 * @param[in]     p_buf         Pointer to output buffer
 * @param[in]     p_loc_speed   pointer to data information
 * @param[in]     max_size      Maximum PDU size that can be transmitted
 * @param[in]     mask_content  Measurement content that should be masked
 * @param[in|out] p_last_flags  Pointer to last flags set notified (if split)
 ****************************************************************************************
 */
__STATIC bool lans_pack_loc_speed(lans_env_t *p_lans_env, co_buf_t* p_buf, const lanp_loc_speed_t *p_loc_speed,
                                  uint16_t max_size, uint16_t mask_content, uint16_t* p_last_flags)
{
    uint16_t flags = p_loc_speed->flags;
    bool complete = false;
    uint16_t pkt_flag;

    // Mask unwanted fields if supported
    if (GETB(p_lans_env->features, LANP_FEAT_LSPEED_CHAR_CT_MASKING_SUP))
    {
        flags &= ~mask_content;
    }

    // just forward RFU and status fields
    pkt_flag = flags & (  0xE000 | LANP_LSPEED_POSITION_STATUS_LSB_BIT | LANP_LSPEED_POSITION_STATUS_MSB_BIT
                        | LANP_LSPEED_SPEED_AND_DISTANCE_FORMAT_BIT    | LANP_LSPEED_ELEVATION_SOURCE_LSB_BIT
                        | LANP_LSPEED_ELEVATION_SOURCE_MSB_BIT         | LANP_LSPEED_HEADING_SOURCE_BIT);


    // remove flags already transmitted
    flags &= ~(*p_last_flags);


    co_buf_tail_reserve(p_buf, 2);
    max_size -= 2;

    do
    {
        // Check provided flags
        if (   GETB(flags, LANP_LSPEED_INST_SPEED_PRESENT)
            && GETB(p_lans_env->features, LANP_FEAT_INSTANTANEOUS_SPEED_SUP))
        {
            if(max_size < 2) break; // Stop packing
            SETB(pkt_flag, LANP_LSPEED_INST_SPEED_PRESENT, 1);

            //Pack instantaneous speed
            co_write16p(co_buf_tail(p_buf), co_htobs(p_loc_speed->inst_speed));
            co_buf_tail_reserve(p_buf, 2);
            max_size -= 2;
        }

        if (   GETB(flags, LANP_LSPEED_TOTAL_DISTANCE_PRESENT)
            && GETB(p_lans_env->features, LANP_FEAT_TOTAL_DISTANCE_SUP))
        {
            if(max_size < 3) break; // Stop packing
            SETB(pkt_flag, LANP_LSPEED_TOTAL_DISTANCE_PRESENT, 1);

            //Pack total distance (24bits)
            co_write24p(co_buf_tail(p_buf), co_htob24(p_loc_speed->total_dist));
            co_buf_tail_reserve(p_buf, 3);
            max_size -= 3;
        }

        if (   GETB(flags, LANP_LSPEED_LOCATION_PRESENT)
            && GETB(p_lans_env->features, LANP_FEAT_LOCATION_SUP))
        {
            if(max_size < 8) break; // Stop packing
            SETB(pkt_flag, LANP_LSPEED_LOCATION_PRESENT, 1);

            //Pack Location
            co_write32p(co_buf_tail(p_buf), co_htobl(p_loc_speed->latitude));
            co_buf_tail_reserve(p_buf, 4);
            co_write32p(co_buf_tail(p_buf), co_htobl(p_loc_speed->longitude));
            co_buf_tail_reserve(p_buf, 4);
            max_size -= 8;
        }

        if (   GETB(flags, LANP_LSPEED_ELEVATION_PRESENT)
            && GETB(p_lans_env->features, LANP_FEAT_ELEVATION_SUP))
        {
            if(max_size < 3) break; // Stop packing
            SETB(pkt_flag, LANP_LSPEED_ELEVATION_PRESENT, 1);

            //Pack elevation (24 bits)
            co_write24p(co_buf_tail(p_buf), co_htob24(p_loc_speed->elevation));
            co_buf_tail_reserve(p_buf, 3);
            max_size -= 3;
        }

        if (   GETB(flags, LANP_LSPEED_HEADING_PRESENT)
            && GETB(p_lans_env->features, LANP_FEAT_HEADING_SUP))
        {
            if(max_size < 2) break; // Stop packing
            SETB(pkt_flag, LANP_LSPEED_HEADING_PRESENT, 1);

            //Pack Extreme Force Magnitudes (Maximum Force Magnitude & Minimum Force Magnitude)
            co_write16p(co_buf_tail(p_buf), co_htobs(p_loc_speed->heading));
            co_buf_tail_reserve(p_buf, 2);
            max_size -= 2;
        }

        if (    GETB(flags, LANP_LSPEED_ROLLING_TIME_PRESENT)
            && GETB(p_lans_env->features, LANP_FEAT_ROLLING_TIME_SUP))
        {
            if(max_size < 1) break; // Stop packing
            SETB(pkt_flag, LANP_LSPEED_ROLLING_TIME_PRESENT, 1);

            //Pack rolling time
            co_buf_tail(p_buf)[0] = p_loc_speed->rolling_time;
            co_buf_tail_reserve(p_buf, 1);
            max_size -= 1;
        }

        if (   GETB(flags, LANP_LSPEED_UTC_TIME_PRESENT)
            && GETB(p_lans_env->features, LANP_FEAT_UTC_TIME_SUP))
        {
            if(max_size < 7) break; // Stop packing
            SETB(pkt_flag, LANP_LSPEED_UTC_TIME_PRESENT, 1);

            // Pack UTC time
            prf_pack_date_time(p_buf, &(p_loc_speed->date_time));
        }

        complete = true;
    } while(0);

    // Copy flags
    co_write16p(co_buf_data(p_buf), co_htobs(pkt_flag));

    *p_last_flags = pkt_flag;

    return (complete);
}



/**
 ****************************************************************************************
 * @brief Packs navigation notifications
 *
 * @param[in]     p_lans_env    Environment data
 * @param[in]     p_buf         Pointer to output buffer
 * @param[in]     p_nav         pointer to data information
 * @param[in]     flags         pointer to data information present
 * @param[in]     max_size      Maximum PDU size that can be transmitted
 * @param[in|out] p_last_flags  Pointer to last flags set notified (if split)
 ****************************************************************************************
 */
__STATIC void lans_pack_navigation(lans_env_t *p_lans_env, co_buf_t* p_buf, const lanp_navigation_t *p_nav)
{
    uint16_t flags    = p_nav->flags;

    // reserve data for flag field
    co_buf_tail_reserve(p_buf, 2);

    // Bearing value
    co_write16p(co_buf_tail(p_buf), co_htobs(p_nav->bearing));
    co_buf_tail_reserve(p_buf, 2);
    // heading value
    co_write16p(co_buf_tail(p_buf), co_htobs(p_nav->heading));
    co_buf_tail_reserve(p_buf, 2);

    // Check provided flags
    if (   GETB(flags, LANP_NAVI_REMAINING_DIS_PRESENT)
        && GETB(p_lans_env->features, LANP_FEAT_REMAINING_DISTANCE_SUP))
    {
        //Pack distance (24bits)
        co_write24p(co_buf_tail(p_buf), co_htob24(p_nav->remaining_distance));
        co_buf_tail_reserve(p_buf, 3);
    }
    else
    {
        SETB(flags, LANP_NAVI_REMAINING_DIS_PRESENT, 0);
    }

    if (   GETB(flags, LANP_NAVI_REMAINING_VER_DIS_PRESENT)
        && GETB(p_lans_env->features, LANP_FEAT_REMAINING_VERTICAL_DISTANCE_SUP))
    {
        //Pack vertical distance (24bits)
        co_write24p(co_buf_tail(p_buf), co_htob24(p_nav->remaining_ver_distance));
        co_buf_tail_reserve(p_buf, 3);
    }
    else
    {
        SETB(flags, LANP_NAVI_REMAINING_VER_DIS_PRESENT, 0);
    }

    if (   GETB(flags, LANP_NAVI_ESTIMATED_TIME_OF_ARRIVAL_PRESENT)
        && GETB(p_lans_env->features, LANP_FEAT_ESTIMATED_TIME_OF_ARRIVAL_SUP))
    {
        // Pack UTC time
        prf_pack_date_time(p_buf, &(p_nav->estimated_arrival_time));
    }
    else
    {
        SETB(flags, LANP_NAVI_ESTIMATED_TIME_OF_ARRIVAL_PRESENT, 0);
    }

    // Copy flags
    co_write16p(co_buf_data(p_buf), co_htobs(flags));
}


/**
 ****************************************************************************************
 * @brief Packs position quality
 *
 * @param[in]     p_lans_env    Environment data
 * @param[in]     p_buf         Pointer to output buffer
 ****************************************************************************************
 */
void lans_pack_pos_q(lans_env_t *p_lans_env, co_buf_t* p_buf)
{
    uint16_t flags = p_lans_env->pos_q.flags;
    // Copy flags
    co_write16p(co_buf_tail(p_buf), co_htobs(flags));
    co_buf_tail_reserve(p_buf, 2);

    // Check provided flags
    if (GETB(flags, LANP_POSQ_NUMBER_OF_BEACONS_IN_SOLUTION_PRESENT))
    {
        //Pack beacons in solution
        co_buf_tail(p_buf)[0] = p_lans_env->pos_q.n_beacons_solution;
        co_buf_tail_reserve(p_buf, 1);
    }

    if (GETB(flags, LANP_POSQ_NUMBER_OF_BEACONS_IN_VIEW_PRESENT))
    {
        //Pack beacons in view
        co_buf_tail(p_buf)[0] = p_lans_env->pos_q.n_beacons_view;
        co_buf_tail_reserve(p_buf, 1);
    }

    if (GETB(flags, LANP_POSQ_TIME_TO_FIRST_FIX_PRESENT))
    {
        //Pack time to fix
        co_write16p(co_buf_tail(p_buf), co_htobs(p_lans_env->pos_q.time_first_fix));
        co_buf_tail_reserve(p_buf, 2);
    }

    if (GETB(flags, LANP_POSQ_EHPE_PRESENT))
    {
        //Pack ehpe
        co_write32p(co_buf_tail(p_buf), co_htobl(p_lans_env->pos_q.ehpe));
        co_buf_tail_reserve(p_buf, 4);
    }

    if (GETB(flags, LANP_POSQ_EVPE_PRESENT))
    {
        //Pack ehpe
        co_write32p(co_buf_tail(p_buf), co_htobl(p_lans_env->pos_q.evpe));
        co_buf_tail_reserve(p_buf, 4);
    }

    if (GETB(flags, LANP_POSQ_HDOP_PRESENT))
    {
        //Pack ehpe
        co_buf_tail(p_buf)[0] = p_lans_env->pos_q.hdop;
        co_buf_tail_reserve(p_buf, 1);
    }

    if (GETB(flags, LANP_POSQ_VDOP_PRESENT))
    {
        //Pack ehpe
        co_buf_tail(p_buf)[0] = p_lans_env->pos_q.vdop;
        co_buf_tail_reserve(p_buf, 1);
    }
}


/**
 ****************************************************************************************
 * @brief Unpack control point data and process it
 *
 * @param[in] p_lans_env Environment
 * @param[in] conidx     connection index
 * @param[in] p_buf      pointer to input data
 ****************************************************************************************
 */
__STATIC uint16_t lans_unpack_ctnl_point_req(lans_env_t *p_lans_env, uint8_t conidx, co_buf_t* p_buf)
{
    uint8_t op_code;
    uint8_t ctnl_pt_rsp_status = LANP_LN_CTNL_PT_RESP_NOT_SUPP;
    uint16_t status = GAP_ERR_NO_ERROR;
    union lanp_ln_ctnl_pt_req_val value;
    memset(&value, 0, sizeof(union lanp_ln_ctnl_pt_req_val));
    op_code =  co_buf_data(p_buf)[0];

    if(co_buf_head_release(p_buf, 1) == CO_BUF_ERR_NO_ERROR)
    {
        // Operation Code
        switch (op_code)
        {
            case (LANP_LN_CTNL_PT_SET_CUMUL_VALUE):
            {
                // Check if total distance feature is supported
                if (GETB(p_lans_env->features, LANP_FEAT_TOTAL_DISTANCE_SUP))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = LANP_LN_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 3)
                    {
                        // The request can be handled
                        ctnl_pt_rsp_status = LANP_LN_CTNL_PT_RESP_SUCCESS;
                        // Cumulative value
                        value.cumul_val = co_btoh24(co_read24p(co_buf_data(p_buf)));
                    }
                }
            } break;

            case (LANP_LN_CTNL_PT_MASK_LSPEED_CHAR_CT):
            {
                // Check if the LN Masking feature is supported
                if (GETB(p_lans_env->features, LANP_FEAT_LSPEED_CHAR_CT_MASKING_SUP))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = LANP_LN_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 2)
                    {
                        // The request can be handled
                        ctnl_pt_rsp_status = LANP_LN_CTNL_PT_RESP_SUCCESS;
                        // Mask content
                        value.mask_content = co_btohs(co_read16p(co_buf_data(p_buf)));
                        p_lans_env->ctnl_upd_content = value.mask_content;
                    }
                }
            } break;

            case (LANP_LN_CTNL_PT_NAVIGATION_CONTROL):
            {
                // Check if navigation feature is supported
                if ((LANS_IS_FEATURE_SUPPORTED(p_lans_env->prfl_cfg, LANS_NAVI_MASK)) &&
                     (GETB(p_lans_env->env[conidx].prfl_ntf_ind_cfg, LANS_PRF_CFG_FLAG_NAVIGATION_NTF)))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = LANP_LN_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 1)
                    {
                        // Control value
                        value.control_value = co_buf_data(p_buf)[0];
                        if (value.control_value <= 0x05)
                        {
                            // The request can be handled
                            ctnl_pt_rsp_status = LANP_LN_CTNL_PT_RESP_SUCCESS;
                            // Store value in the environment
                            p_lans_env->ctnl_upd_content = value.control_value;
                        }
                    }
                }
            } break;

            case (LANP_LN_CTNL_PT_REQ_NUMBER_OF_ROUTES):
            {
                // Check if navigation feature is supported
                if (LANS_IS_FEATURE_SUPPORTED(p_lans_env->prfl_cfg, LANS_NAVI_MASK))
                {
                    // The request can be handled
                    ctnl_pt_rsp_status = LANP_LN_CTNL_PT_RESP_SUCCESS;
                }
            } break;

            case (LANP_LN_CTNL_PT_REQ_NAME_OF_ROUTE):
            {
                // Check if navigation feature is supported
                if (LANS_IS_FEATURE_SUPPORTED(p_lans_env->prfl_cfg, LANS_NAVI_MASK))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = LANP_LN_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 2)
                    {
                        // The request can be handled
                        ctnl_pt_rsp_status = LANP_LN_CTNL_PT_RESP_SUCCESS;
                        // Route number
                        value.route_number = co_btohs(co_read16p(co_buf_data(p_buf)));
                    }
                }
            } break;

            case (LANP_LN_CTNL_PT_SELECT_ROUTE):
            {
                // Check if navigation feature is supported
                if (LANS_IS_FEATURE_SUPPORTED(p_lans_env->prfl_cfg, LANS_NAVI_MASK))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = LANP_LN_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 2)
                    {
                        // The request can be handled
                        ctnl_pt_rsp_status = LANP_LN_CTNL_PT_RESP_SUCCESS;
                        // route number
                        value.route_number = co_btohs(co_read16p(co_buf_data(p_buf)));
                    }
                }
            } break;

            case (LANP_LN_CTNL_PT_SET_FIX_RATE):
            {
                // Check if the LN Masking feature is supported
                if (GETB(p_lans_env->features, LANP_FEAT_FIX_RATE_SETTING_SUP))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = LANP_LN_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 1)
                    {
                        // The request can be handled
                        ctnl_pt_rsp_status = LANP_LN_CTNL_PT_RESP_SUCCESS;
                        // fix rate
                        value.fix_rate = co_buf_data(p_buf)[0];
                    }
                }
            } break;

            case (LANP_LN_CTNL_PT_SET_ELEVATION):
            {
                // Check if the Chain Weight Adjustment feature is supported
                if (GETB(p_lans_env->features, LANP_FEAT_ELEVATION_SETTING_SUP))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = LANP_LN_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 3)
                    {
                        // The request can be handled
                        ctnl_pt_rsp_status = LANP_LN_CTNL_PT_RESP_SUCCESS;
                        // elevation
                        value.elevation = co_btoh24(co_read24p(co_buf_data(p_buf)));
                    }
                }
            } break;

            default:
            {
                // Operation Code is invalid, status is already LAN_CTNL_PT_RESP_NOT_SUPP
            } break;
        }

        // If no error raised, inform the application about the request
        if (ctnl_pt_rsp_status == LANP_LN_CTNL_PT_RESP_SUCCESS)
        {
            const lans_cb_t* p_cb  = (const lans_cb_t*) p_lans_env->prf_env.p_cb;

            // inform application about control point request
            p_lans_env->ctrl_pt_op = op_code;
            p_cb->cb_ctnl_pt_req(conidx, op_code, &value);
        }
        else
        {
            co_buf_t* p_out_buf = NULL;

            if(co_buf_alloc(&p_out_buf, GATT_BUFFER_HEADER_LEN, 0, LANP_LAN_LN_CNTL_PT_RSP_MIN_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
            {
                lans_buf_meta_t* p_meta = (lans_buf_meta_t*)co_buf_metadata(p_out_buf);

                p_lans_env->ctrl_pt_op   = LANP_LN_CTNL_PT_RESPONSE_CODE;
                co_buf_tail(p_out_buf)[0] = LANP_LN_CTNL_PT_RESPONSE_CODE;
                co_buf_tail_reserve(p_out_buf, 1);
                co_buf_tail(p_out_buf)[0] = op_code;
                co_buf_tail_reserve(p_out_buf, 1);
                co_buf_tail(p_out_buf)[0] = ctnl_pt_rsp_status;
                co_buf_tail_reserve(p_out_buf, 1);

                p_meta->conidx    = conidx;
                p_meta->operation = LANS_CTNL_PT_RSP_SEND_OP_CODE;

                // put event on wait queue
                co_list_push_back(&(p_lans_env->wait_queue), &(p_out_buf->hdr));
                // execute operation
                lans_exe_operation(p_lans_env);
            }
            else
            {
                status = ATT_ERR_INSUFF_RESOURCE;
            }
        }
    }

    return (status);
}


/**
 ****************************************************************************************
 * @brief Packs control point response
 * @param[in]     p_lans_env   Environment data
 * @param[in]     conidx       Connection Index
 * @param[in]     p_buf        Pointer to output buffer
 * @param[in]     op_code      Operation Code
 * @param[in]     resp_val     Operation result
 * @param[in]     p_value      Pointer to operation response parameters
 ****************************************************************************************
 */
void lans_pack_ctnl_point_rsp(lans_env_t *p_lans_env, uint8_t conidx, co_buf_t* p_buf, uint8_t op_code, uint8_t resp_val,
                               const union lanp_ctnl_pt_rsp_val* p_value)
{
    // ---- Data must be filled in reversed order since data contains already name length


    if (resp_val == LANP_LN_CTNL_PT_RESP_SUCCESS)
    {
        switch (p_lans_env->ctrl_pt_op)
        {
            case (LANP_LN_CTNL_PT_MASK_LSPEED_CHAR_CT):
            {
                // Ignore bits other than 0 - 6
                p_lans_env->env[conidx].mask_lspeed_content = (p_lans_env->ctnl_upd_content & LANS_LSPEED_DISABLED_MASK);
            } break;

            case (LANP_LN_CTNL_PT_NAVIGATION_CONTROL):
            {
                switch(p_lans_env->ctnl_upd_content)
                {
                    // Disable notifications
                    case LANP_LN_CTNL_STOP_NAVI:
                    case LANP_LN_CTNL_PAUSE_NAVI:
                    {
                        SETB(p_lans_env->env[conidx].nav_ctrl, LANS_NAV_CTRL_PT_EN, 0);
                    } break;

                    // Enable notifications
                    case LANP_LN_CTNL_START_NAVI:
                    case LANP_LN_CTNL_RESUME_NAVI:
                    case LANP_LN_CTNL_START_NST_WPT:
                    {
                        SETB(p_lans_env->env[conidx].nav_ctrl, LANS_NAV_CTRL_PT_EN, 1);
                    } break;

                    // Do nothing
                    case LANP_LN_CTNL_SKIP_WPT:
                    default:
                    {

                    } break;
                }
            } break;

            case (LANP_LN_CTNL_PT_REQ_NUMBER_OF_ROUTES):
            {
                co_buf_head_reserve(p_buf, 2);
                co_write16p(co_buf_data(p_buf), co_htobs(p_value->number_of_routes));
            } break;

            default: { /* Nothing to do */ } break;
        }
    }

    // Set the Response Value
    co_buf_head_reserve(p_buf, 1);
    co_buf_data(p_buf)[0] = (resp_val > LANP_LN_CTNL_PT_RESP_FAILED) ? LANP_LN_CTNL_PT_RESP_FAILED : resp_val;

    // Set the request operation code
    co_buf_head_reserve(p_buf, 1);
    co_buf_data(p_buf)[0] = p_lans_env->ctrl_pt_op;

    // Set the Response Code
    co_buf_head_reserve(p_buf, 1);
    co_buf_data(p_buf)[0] = LANP_LN_CTNL_PT_RESPONSE_CODE;
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
__STATIC void lans_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    lans_env_t *p_lans_env = PRF_ENV_GET(LANS, lans);
    // retrieve value attribute
    co_buf_t* p_buf       = NULL;
    uint16_t  status      = GAP_ERR_NO_ERROR;
    uint16_t  att_val_len = 0;

    if(p_lans_env == NULL)
    {
        status = PRF_APP_ERROR;
    }
    else if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0,  LANP_LAN_POSQ_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
    {
        uint8_t att_idx = lans_idx_get(p_lans_env, hdl);

        switch (att_idx)
        {
            case LNS_IDX_LN_FEAT_VAL:
            {
                co_write32p(co_buf_tail(p_buf), co_htobl(p_lans_env->features));
                co_buf_tail_reserve(p_buf, 4);
            } break;


            case LNS_IDX_POS_Q_VAL:
            {
                lans_pack_pos_q(p_lans_env, p_buf);
            } break;

            case LNS_IDX_LOC_SPEED_NTF_CFG:
            {
                uint16_t ntf_cfg = GETB(p_lans_env->env[conidx].prfl_ntf_ind_cfg, LANS_PRF_CFG_FLAG_LOC_SPEED_NTF)
                                 ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ntf_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case LNS_IDX_NAVIGATION_NTF_CFG:
            {
                uint16_t ntf_cfg = GETB(p_lans_env->env[conidx].prfl_ntf_ind_cfg, LANS_PRF_CFG_FLAG_NAVIGATION_NTF)
                                 ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ntf_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case LNS_IDX_LN_CTNL_PT_IND_CFG:
            {
                uint16_t ind_cfg = GETB(p_lans_env->env[conidx].prfl_ntf_ind_cfg, LANS_PRF_CFG_FLAG_LN_CTNL_PT_IND)
                                 ? PRF_CLI_START_IND : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ind_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            default:
            {
                status = ATT_ERR_REQUEST_NOT_SUPPORTED;
            } break;
        }

        att_val_len = co_buf_data_len(p_buf);
    }
    else
    {
        status = ATT_ERR_INSUFF_RESOURCE;
    }


    gatt_srv_att_read_get_cfm(conidx, user_lid, token, status, att_val_len, p_buf);
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
__STATIC void lans_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                        co_buf_t* p_data)
{
    lans_env_t *p_lans_env = PRF_ENV_GET(LANS, lans);
    uint16_t  status      = PRF_APP_ERROR;

    if(p_lans_env != NULL)
    {
        uint8_t cfg_upd_flag  = 0;
        uint8_t cfg_upd_char  = 0;
        uint16_t cfg_en_val = 0;

        switch (lans_idx_get(p_lans_env, hdl))
        {
            case LNS_IDX_LOC_SPEED_NTF_CFG:
            {
                cfg_upd_char = LANS_PRF_CFG_FLAG_LOC_SPEED_NTF_BIT;
                cfg_upd_flag = LANS_PRF_CFG_FLAG_LOC_SPEED_NTF_BIT;
                cfg_en_val   = PRF_CLI_START_NTF;
            } break;

            case LNS_IDX_NAVIGATION_NTF_CFG:
            {
                cfg_upd_char = LANS_PRF_CFG_FLAG_NAVIGATION_NTF_BIT;
                cfg_upd_flag = LANS_PRF_CFG_FLAG_NAVIGATION_NTF_BIT;
                cfg_en_val   = PRF_CLI_START_NTF;
            } break;

            case LNS_IDX_LN_CTNL_PT_IND_CFG:
            {
                cfg_upd_char = LANS_PRF_CFG_FLAG_LN_CTNL_PT_IND_BIT;
                cfg_upd_flag = LANS_PRF_CFG_FLAG_LN_CTNL_PT_IND_BIT;
                cfg_en_val   = PRF_CLI_START_IND;
            } break;

            case LNS_IDX_LN_CTNL_PT_VAL:
            {
                // Check if sending of indications has been enabled
                if (!GETB(p_lans_env->env[conidx].prfl_ntf_ind_cfg, LANS_PRF_CFG_FLAG_LN_CTNL_PT_IND))
                {
                    // CPP improperly configured
                    status = PRF_CCCD_IMPR_CONFIGURED;
                }
                else if (p_lans_env->ctrl_pt_op != LANP_LN_CTNL_PT_RESERVED)
                {
                    // A procedure is already in progress
                    status = LAN_ERROR_PROC_IN_PROGRESS;
                }
                else
                {
                    // Unpack Control Point parameters
                    status = lans_unpack_ctnl_point_req(p_lans_env, conidx, p_data);
                }
            } break;

            default: { status = ATT_ERR_REQUEST_NOT_SUPPORTED; } break;
        }

        if(cfg_upd_flag != 0)
        {
            uint16_t cfg = co_btohs(co_read16p(co_buf_data(p_data)));

            // parameter check
            if(   (co_buf_data_len(p_data) == sizeof(uint16_t))
               && ((cfg == PRF_CLI_STOP_NTFIND) || (cfg == cfg_en_val)))
            {
                const lans_cb_t* p_cb  = (const lans_cb_t*) p_lans_env->prf_env.p_cb;

                if(cfg == PRF_CLI_STOP_NTFIND)
                {
                    p_lans_env->env[conidx].prfl_ntf_ind_cfg &= ~cfg_upd_flag;
                }
                else
                {
                    p_lans_env->env[conidx].prfl_ntf_ind_cfg |= cfg_upd_flag;
                }

                // inform application about update
                p_cb->cb_bond_data_upd(conidx, cfg_upd_char, cfg);
                status = GAP_ERR_NO_ERROR;
            }
            else
            {
                status = PRF_CCCD_IMPR_CONFIGURED;
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
__STATIC void lans_cb_att_event_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t dummy, uint16_t hdl,
                                      uint16_t max_length)
{
    lans_env_t *p_lans_env = PRF_ENV_GET(LANS, lans);
    co_buf_t*   p_buf       = NULL;
    uint16_t    status      = GAP_ERR_NO_ERROR;
    uint16_t    att_length  = 0;

    // allocate buffer for event transmission
    if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, LANP_LAN_LOC_SPEED_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
    {
        if(p_lans_env == NULL)
        {
            status = PRF_APP_ERROR;
        }
        else
        {
            co_buf_t* p_loc_speed = (co_buf_t*) co_list_pick(&(p_lans_env->wait_queue));
            lans_buf_meta_t* p_meta = (lans_buf_meta_t*) co_buf_metadata(p_loc_speed);

            ASSERT_ERR(p_loc_speed != NULL);

            // pack measurement
            p_meta->new = lans_pack_loc_speed(p_lans_env, p_buf, (lanp_loc_speed_t*)co_buf_head(p_loc_speed), max_length,
                                              p_lans_env->env[conidx].mask_lspeed_content, &(p_meta->last_flag));

            att_length = co_buf_data_len(p_buf);
        }
    }
    else
    {
        status = GAP_ERR_INSUFF_RESOURCES;
    }

    // confirm data
    gatt_srv_att_event_get_cfm(conidx, user_lid, token, status, att_length, p_buf);

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
__STATIC void lans_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Consider job done
    lans_env_t *p_lans_env = PRF_ENV_GET(LANS, lans);
    if(p_lans_env != NULL)
    {
        const lans_cb_t* p_cb  = (const lans_cb_t*) p_lans_env->prf_env.p_cb;
        p_lans_env->op_ongoing = false;

        switch(dummy)
        {
            case LANS_NTF_NAVIGATION_OP_CODE:
            {
                p_cb->cb_navigation_send_cmp(status);
            } break;
            case LANS_NTF_LOC_SPEED_OP_CODE: { /* Nothing to do */ } break;
            default:
            {
                // Inform application that control point response has been sent
                if (p_lans_env->ctrl_pt_op != LANP_LN_CTNL_PT_RESPONSE_CODE)
                {
                    p_cb->cb_ctnl_pt_rsp_send_cmp(conidx, status);
                }

                p_lans_env->ctrl_pt_op = LANP_LN_CTNL_PT_RESERVED;
            } break;
        }

        // continue operation execution
        lans_exe_operation(p_lans_env);
    }
}

/// Service callback hander
__STATIC const gatt_srv_cb_t lans_cb =
{
        .cb_event_sent    = lans_cb_event_sent,
        .cb_att_read_get  = lans_cb_att_read_get,
        .cb_att_event_get = lans_cb_att_event_get,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = lans_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */

uint16_t lans_enable(uint8_t conidx, uint16_t prfl_ntf_ind_cfg)
{
    lans_env_t* p_lans_env = PRF_ENV_GET(LANS, lans);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_lans_env != NULL)
    {
        // check state of the task
        if (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL)
        {
            // Bonded data was not used before
            if (!GETB(p_lans_env->env[conidx].prfl_ntf_ind_cfg, LANS_PRF_CFG_PERFORMED_OK))
            {
                status = GAP_ERR_NO_ERROR;
                p_lans_env->env[conidx].prfl_ntf_ind_cfg = prfl_ntf_ind_cfg;
            }
            // Enable Bonded Data
            SETB(conidx, LANS_PRF_CFG_PERFORMED_OK, 1);
        }
    }

    return (status);

}

uint16_t lans_loc_speed_send(uint32_t conidx_bf, const lanp_loc_speed_t* p_loc_speed)
{
    lans_env_t* p_lans_env = PRF_ENV_GET(LANS, lans);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_loc_speed == NULL)
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_lans_env != NULL)
    {
        co_buf_t* p_buf;

        if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, sizeof(lanp_loc_speed_t), GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            lans_buf_meta_t* p_buf_meta = (lans_buf_meta_t*) co_buf_metadata(p_buf);
            p_buf_meta->operation = LANS_NTF_LOC_SPEED_OP_CODE;
            p_buf_meta->conidx    = GAP_INVALID_CONIDX;
            p_buf_meta->conidx_bf = conidx_bf & ((1 << BLE_CONNECTION_MAX) - 1);
            p_buf_meta->new       = true;

            // copy structure info - use buffer head to ensure that buffer is 32-bit aligned
            memcpy(co_buf_head(p_buf), p_loc_speed, sizeof(lanp_loc_speed_t));

            // put event on wait queue
            co_list_push_back(&(p_lans_env->wait_queue), &(p_buf->hdr));
            // execute operation
            lans_exe_operation(p_lans_env);
            status = GAP_ERR_NO_ERROR;
        }
        else
        {
            status = GAP_ERR_INSUFF_RESOURCES;
        }
    }

    return (status);
}

uint16_t lans_navigation_send(uint32_t conidx_bf, const lanp_navigation_t* p_nav)
{
    lans_env_t* p_lans_env = PRF_ENV_GET(LANS, lans);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_nav == NULL)
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_lans_env != NULL)
    {
        co_buf_t* p_buf;

        if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, LANP_LAN_NAVIGATION_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            lans_buf_meta_t* p_buf_meta = (lans_buf_meta_t*) co_buf_metadata(p_buf);
            p_buf_meta->operation = LANS_NTF_NAVIGATION_OP_CODE;
            p_buf_meta->conidx    = GAP_INVALID_CONIDX;
            p_buf_meta->conidx_bf = conidx_bf & ((1 << BLE_CONNECTION_MAX) - 1);

            // pack measurement
            lans_pack_navigation(p_lans_env, p_buf, p_nav);
            // put event on wait queue
            co_list_push_back(&(p_lans_env->wait_queue), &(p_buf->hdr));
            // execute operation
            lans_exe_operation(p_lans_env);
            status = GAP_ERR_NO_ERROR;
        }
        else
        {
            status = GAP_ERR_INSUFF_RESOURCES;
        }
    }

    return (status);
}


uint16_t lans_pos_q_upd(const lanp_posq_t* p_pos_q)
{
    lans_env_t* p_lans_env = PRF_ENV_GET(LANS, lans);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_pos_q == NULL)
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_lans_env != NULL)
    {
        if(!LANS_IS_FEATURE_SUPPORTED(p_lans_env->prfl_cfg, LANS_POS_Q_MASK))
        {
            status = PRF_ERR_FEATURE_NOT_SUPPORTED;
        }
        else
        {
            // Update position quality
            uint16_t flags   = p_pos_q->flags;

            memcpy(&(p_lans_env->pos_q), p_pos_q, sizeof(lanp_posq_t));


            if (!GETB(p_lans_env->features, LANP_FEAT_NUMBER_OF_BEACONS_IN_SOLUTION_SUP))
            {
                // Disable beacons in solution presence
                SETB(flags, LANP_POSQ_NUMBER_OF_BEACONS_IN_SOLUTION_PRESENT, false);
            }

            if (!GETB(p_lans_env->features, LANP_FEAT_NUMBER_OF_BEACONS_IN_VIEW_SUP))
            {
                // Disable beacons in view presence
                SETB(flags, LANP_POSQ_NUMBER_OF_BEACONS_IN_VIEW_PRESENT, false);
            }

            if (!GETB(p_lans_env->features, LANP_FEAT_TIME_TO_FIRST_FIX_SUP))
            {
                // Disable time to fix presence
                SETB(flags, LANP_POSQ_TIME_TO_FIRST_FIX_PRESENT, false);
            }

            if (!GETB(p_lans_env->features, LANP_FEAT_ESTIMATED_HOR_POSITION_ERROR_SUP))
            {
                // Disable EHPE presence
                SETB(flags, LANP_POSQ_EHPE_PRESENT, false);
            }

            if (!GETB(p_lans_env->features, LANP_FEAT_ESTIMATED_VER_POSITION_ERROR_SUP))
            {
                // Disable EVPE presence
                SETB(flags, LANP_POSQ_EVPE_PRESENT, false);
            }

            if (!GETB(p_lans_env->features, LANP_FEAT_HOR_DILUTION_OF_PRECISION_SUP))
            {
                // Disable HDOP presence
                SETB(flags, LANP_POSQ_HDOP_PRESENT, false);
            }

            if (!GETB(p_lans_env->features, LANP_FEAT_VER_DILUTION_OF_PRECISION_SUP))
            {
                //Pack VDOP
                SETB(flags, LANP_POSQ_VDOP_PRESENT, false);
            }

            // Update Flags value
            p_lans_env->pos_q.flags = flags;

            status = GAP_ERR_NO_ERROR;
        }
    }

    return (status);
}


uint16_t lans_ctnl_pt_rsp_send(uint8_t conidx, uint8_t op_code, uint8_t resp_val,
                               const union lanp_ctnl_pt_rsp_val* p_value)
{
    lans_env_t* p_lans_env = PRF_ENV_GET(LANS, lans);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_value == NULL)
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_lans_env != NULL)
    {
        do
        {
            co_buf_t* p_buf = NULL;
            uint16_t name_len = 0;

            // Check the current operation
            if (p_lans_env->ctrl_pt_op == LANP_LN_CTNL_PT_RESERVED)
            {
                // The confirmation has been sent without request indication, ignore
                break;
            }

            // The CP Control Point Characteristic must be supported if we are here
            if (!LANS_IS_FEATURE_SUPPORTED(p_lans_env->prfl_cfg, LANS_LN_CTNL_PT_MASK))
            {
                status = PRF_ERR_REQ_DISALLOWED;
                break;
            }

            // Check if sending of indications has been enabled
            if (!GETB(p_lans_env->env[conidx].prfl_ntf_ind_cfg, LANS_PRF_CFG_FLAG_LN_CTNL_PT_IND))
            {
                // mark operation done
                p_lans_env->ctrl_pt_op = LANP_LN_CTNL_PT_RESERVED;
                // CPP improperly configured
                status = PRF_CCCD_IMPR_CONFIGURED;
                break;
            }

            // compute buffer for name length
            if((op_code == LANP_LN_CTNL_PT_REQ_NAME_OF_ROUTE) && (resp_val == LANP_LN_CTNL_PT_RESP_SUCCESS))
            {
                name_len = (p_value->route.length > LANP_LAN_LN_CNTL_DATA_MAX_LEN)
                         ? LANP_LAN_LN_CNTL_DATA_MAX_LEN : p_value->route.length;
            }

            if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN + LANS_LAN_LN_CNTL_PT_RSP_BUFFER_HEAD_LEN, name_len,
                            GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
            {
                lans_buf_meta_t* p_buf_meta = (lans_buf_meta_t*) co_buf_metadata(p_buf);
                p_buf_meta->operation = LANS_CTNL_PT_RSP_SEND_OP_CODE;
                p_buf_meta->conidx    = conidx;

                // copy name information
                if(name_len > 0)
                {
                    co_buf_copy_data_from_mem(p_buf, p_value->route.name, name_len);
                }

                // Pack structure
                lans_pack_ctnl_point_rsp(p_lans_env, conidx, p_buf, op_code, resp_val, p_value);
                // put event on wait queue
                co_list_push_back(&(p_lans_env->wait_queue), &(p_buf->hdr));
                // execute operation
                lans_exe_operation(p_lans_env);
                status = GAP_ERR_NO_ERROR;
            }
            else
            {
                status = GAP_ERR_INSUFF_RESOURCES;
            }

        } while(0);
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
 * @brief Send a LANS_CMP_EVT message to the application.
 * @param[in] conidx connection index
 * @param[in] operation Operation completed
 * @param[in] status status of the operation
 ****************************************************************************************
 */
__STATIC void lans_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct lans_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(LANS_CMP_EVT, PRF_DST_TASK(LANS), PRF_SRC_TASK(LANS), lans_cmp_evt);

    if(p_evt != NULL)
    {
        p_evt->conidx     = conidx;
        p_evt->operation  = operation;
        p_evt->status     = status;
        ke_msg_send(p_evt);
    }
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref LANS_ENABLE_REQ message.
 * @param[in] msgid Id of the message received
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int lans_enable_req_handler(ke_msg_id_t const msgid, struct lans_enable_req *p_param,
                                      ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    struct lans_enable_rsp *p_cmp_evt;
    uint16_t status = lans_enable(p_param->conidx, p_param->prfl_ntf_ind_cfg);

    // send completed information to APP task that contains error status
    p_cmp_evt = KE_MSG_ALLOC(LANS_ENABLE_RSP, src_id, dest_id, lans_enable_rsp);

    if(p_cmp_evt)
    {
        p_cmp_evt->conidx     = p_param->conidx;
        p_cmp_evt->status     = status;
        ke_msg_send(p_cmp_evt);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref LANS_UPD_POS_Q_REQ message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int lans_upd_pos_q_req_handler(ke_msg_id_t const msgid, struct lans_upd_pos_q_req *p_param,
                                        ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    struct lans_upd_pos_q_rsp *p_rsp;
    uint16_t status = lans_pos_q_upd(&(p_param->parameters));

    // send response information to APP task that contains error status
    p_rsp = KE_MSG_ALLOC(LANS_UPD_POS_Q_RSP, src_id, dest_id, lans_upd_pos_q_rsp);

    if(p_rsp)
    {
        p_rsp->status     = status;
        ke_msg_send(p_rsp);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref LANS_NTF_LN_MEAS_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int lans_ntf_loc_speed_cmd_handler(ke_msg_id_t const msgid, struct lans_ntf_loc_speed_cmd *p_param,
                                            ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = lans_loc_speed_send(p_param->conidx_bf, &(p_param->parameters));

    if(status != GAP_ERR_NO_ERROR)
    {
        lans_send_cmp_evt(GAP_INVALID_CONIDX, LANS_NTF_LOC_SPEED_OP_CODE, status);
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref LANS_NTF_NAVIGATION_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int lans_ntf_navigation_cmd_handler(ke_msg_id_t const msgid,  struct lans_ntf_navigation_cmd *p_param,
                                             ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = lans_navigation_send(p_param->conidx_bf, &(p_param->parameters));

    if(status != GAP_ERR_NO_ERROR)
    {
        lans_send_cmp_evt(GAP_INVALID_CONIDX, LANS_NTF_NAVIGATION_OP_CODE, status);
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref LANS_LN_CTNL_PT_RSP_SEND_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int lans_ln_ctnl_pt_rsp_send_cmd_handler(ke_msg_id_t const msgid, struct lans_ln_ctnl_pt_rsp_send_cmd *p_param,
                                                 ke_task_id_t const dest_id,  ke_task_id_t const src_id)
{
    uint16_t status = lans_ctnl_pt_rsp_send(p_param->conidx, p_param->req_op_code, p_param->resp_value, &(p_param->value));

    if(status != GAP_ERR_NO_ERROR)
    {
        lans_send_cmp_evt(p_param->conidx, LANS_CTNL_PT_RSP_SEND_OP_CODE, status);
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(lans)
{
    // Note: all messages must be sorted in ID ascending order

    {LANS_ENABLE_REQ,              (ke_msg_func_t) lans_enable_req_handler              },
    {LANS_UPD_POS_Q_REQ,           (ke_msg_func_t) lans_upd_pos_q_req_handler           },
    {LANS_NTF_LOC_SPEED_CMD,       (ke_msg_func_t) lans_ntf_loc_speed_cmd_handler       },
    {LANS_NTF_NAVIGATION_CMD,      (ke_msg_func_t) lans_ntf_navigation_cmd_handler      },
    {LANS_LN_CTNL_PT_RSP_SEND_CMD, (ke_msg_func_t) lans_ln_ctnl_pt_rsp_send_cmd_handler },
};

/**
 ****************************************************************************************
 * @brief Inform that bond data updated for the connection.
 *
 * @param[in] conidx        Connection index
 * @param[in] char_code     Characteristic Code (Location and speed, Control Point or navigation)
 * @param[in] cfg_val       Stop/notify/indicate value to configure into the peer characteristic
 ****************************************************************************************
 */
__STATIC void lans_cb_bond_data_upd(uint8_t conidx, uint8_t char_code, uint16_t cfg_val)
{
    struct lans_cfg_ntfind_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(LANS_CFG_NTFIND_IND, PRF_DST_TASK(LANS),
                         PRF_SRC_TASK(LANS), lans_cfg_ntfind_ind);

    if(p_evt != NULL)
    {
        p_evt->conidx     = conidx;
        p_evt->char_code  = char_code;
        p_evt->ntf_cfg    = cfg_val;
        ke_msg_send(p_evt);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of location and speed transmission
 *
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void lans_cb_loc_speed_send_cmp(uint16_t status)
{
    lans_send_cmp_evt(GAP_INVALID_CONIDX, LANS_NTF_LOC_SPEED_OP_CODE, status);
}


/**
 ****************************************************************************************
 * @brief Completion of navigation information transmission
 *
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void lans_cb_navigation_send_cmp(uint16_t status)
{
    lans_send_cmp_evt(GAP_INVALID_CONIDX, LANS_NTF_NAVIGATION_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Inform that peer device requests an action using control point
 *
 * @note control point request must be answered using @see lans_ctnl_pt_rsp_send function
 *
 * @param[in] conidx        Connection index
 * @param[in] op_code       Operation Code (@see lanp_ln_ctnl_pt_code)
 * @param[in] p_value       Pointer to control point request value
 ****************************************************************************************
 */
__STATIC void lans_cb_ctnl_pt_req(uint8_t conidx, uint8_t op_code, const union lanp_ln_ctnl_pt_req_val* p_value)
{
    struct lans_ln_ctnl_pt_req_recv_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(LANS_LN_CTNL_PT_REQ_RECV_IND, PRF_DST_TASK(LANS),
                         PRF_SRC_TASK(LANS), lans_ln_ctnl_pt_req_recv_ind);

    if(p_evt != NULL)
    {
        p_evt->conidx     = conidx;
        p_evt->op_code    = op_code;
        memcpy(&(p_evt->value), p_value, sizeof(union lanp_ln_ctnl_pt_req_val));
        ke_msg_send(p_evt);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of control point response send procedure
 *
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void lans_cb_ctnl_pt_rsp_send_cmp(uint8_t conidx, uint16_t status)
{
    lans_send_cmp_evt(conidx, LANS_CTNL_PT_RSP_SEND_OP_CODE, status);
}

/// Default Message handle
__STATIC const lans_cb_t lans_msg_cb =
{
        .cb_bond_data_upd        = lans_cb_bond_data_upd,
        .cb_loc_speed_send_cmp   = lans_cb_loc_speed_send_cmp,
        .cb_navigation_send_cmp  = lans_cb_navigation_send_cmp,
        .cb_ctnl_pt_req          = lans_cb_ctnl_pt_req,
        .cb_ctnl_pt_rsp_send_cmp = lans_cb_ctnl_pt_rsp_send_cmp,
};
#endif // (BLE_HL_MSG_API)

/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the LANS module.
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
__STATIC uint16_t lans_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct lans_db_cfg *p_params, const lans_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        lans_env_t* p_lans_env;
        // Service content flag
        uint32_t cfg_flag = LANS_MANDATORY_MASK;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(lans_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL)
           || (p_cb->cb_loc_speed_send_cmp == NULL) || (p_cb->cb_navigation_send_cmp == NULL)
           || (p_cb->cb_bond_data_upd == NULL) || (p_cb->cb_ctnl_pt_req == NULL) || (p_cb->cb_ctnl_pt_rsp_send_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register LANS user
        status = gatt_user_srv_register(GATT_NTF_HEADER_LEN + LANP_LAN_LN_CNTL_PT_RSP_MAX_LEN, user_prio, &lans_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        /*
         * Check if Position Quality shall be added.
         */
        if((GETB(p_params->ln_feature, LANP_FEAT_NUMBER_OF_BEACONS_IN_SOLUTION_SUP)) ||
           (GETB(p_params->ln_feature, LANP_FEAT_NUMBER_OF_BEACONS_IN_VIEW_SUP)) ||
           (GETB(p_params->ln_feature, LANP_FEAT_TIME_TO_FIRST_FIX_SUP)) ||
           (GETB(p_params->ln_feature, LANP_FEAT_ESTIMATED_HOR_POSITION_ERROR_SUP)) ||
           (GETB(p_params->ln_feature, LANP_FEAT_ESTIMATED_VER_POSITION_ERROR_SUP)) ||
           (GETB(p_params->ln_feature, LANP_FEAT_HOR_DILUTION_OF_PRECISION_SUP)) ||
           (GETB(p_params->ln_feature, LANP_FEAT_VER_DILUTION_OF_PRECISION_SUP)) ||
           (GETB(p_params->ln_feature, LANP_FEAT_POSITION_STATUS_SUP)))
        {
            //Add configuration to the database
            cfg_flag |= LANS_POS_Q_MASK;
        }

        /*
         * Check if the Navigation characteristic shall be added.
         */
        if ((GETB(p_params->prfl_config, LANS_NAVIGATION_SUPP_FLAG)) ||
            (GETB(p_params->ln_feature, LANP_FEAT_NUMBER_OF_BEACONS_IN_SOLUTION_SUP)) ||
            (GETB(p_params->ln_feature, LANP_FEAT_REMAINING_DISTANCE_SUP)) ||
            (GETB(p_params->ln_feature, LANP_FEAT_REMAINING_VERTICAL_DISTANCE_SUP)) ||
            (GETB(p_params->ln_feature, LANP_FEAT_ESTIMATED_TIME_OF_ARRIVAL_SUP)) ||
            (GETB(p_params->ln_feature, LANP_FEAT_POSITION_STATUS_SUP)))
        {
            cfg_flag |= LANS_NAVI_MASK;
        }
        /*
         * Check if the LN Control Point characteristic shall be added
         */
        if ((LANS_IS_FEATURE_SUPPORTED(cfg_flag, LANS_NAVI_MASK)) ||
            (GETB(p_params->prfl_config, LANS_CTNL_PT_CHAR_SUPP_FLAG)) ||
            (GETB(p_params->ln_feature, LANP_FEAT_TOTAL_DISTANCE_SUP)) ||
            (GETB(p_params->ln_feature, LANP_FEAT_LSPEED_CHAR_CT_MASKING_SUP)) ||
            (GETB(p_params->ln_feature, LANP_FEAT_FIX_RATE_SETTING_SUP)) ||
            ((GETB(p_params->ln_feature, LANP_FEAT_ELEVATION_SUP)) &&
            (GETB(p_params->ln_feature, LANP_FEAT_ELEVATION_SETTING_SUP))))
        {
            cfg_flag |= LANS_LN_CTNL_PT_MASK;
        }


        // Add GAP service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_LOCATION_AND_NAVIGATION, LNS_IDX_NB,
                                   (uint8_t *)&cfg_flag, &(lans_att_db[0]), LNS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_lans_env = (lans_env_t *) ke_malloc(sizeof(lans_env_t), KE_MEM_ATT_DB);

        if(p_lans_env != NULL)
        {
            // allocate LANS required environment variable
            p_env->p_env = (prf_hdr_t *) p_lans_env;
            p_lans_env->start_hdl       = *p_start_hdl;
            p_lans_env->features        = p_params->ln_feature;
            p_lans_env->user_lid        = user_lid;
            p_lans_env->prfl_cfg        = cfg_flag;
            p_lans_env->op_ongoing      = false;
            p_lans_env->in_exe_op       = false;
            p_lans_env->ctrl_pt_op      = LANP_LN_CTNL_PT_RESERVED;
            memset(p_lans_env->env,  0, sizeof(p_lans_env->env));
            memset(&(p_lans_env->pos_q), 0, sizeof(lanp_posq_t));
            co_list_init(&(p_lans_env->wait_queue));


            // initialize profile environment variable
            p_lans_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = lans_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(lans_msg_handler_tab);
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
__STATIC uint16_t lans_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    lans_env_t *p_lans_env = (lans_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_lans_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        if(reason != PRF_DESTROY_RESET)
        {
            // remove buffer in wait queue
            while(!co_list_is_empty(&p_lans_env->wait_queue))
            {
                co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&p_lans_env->wait_queue);
                co_buf_release(p_buf);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_lans_env);
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
__STATIC void lans_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    lans_env_t *p_lans_env = (lans_env_t *) p_env->p_env;
    memset(&(p_lans_env->env[conidx]), 0, sizeof(lans_cnx_env_t));
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
__STATIC void lans_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    lans_env_t *p_lans_env = (lans_env_t *) p_env->p_env;
    memset(&(p_lans_env->env[conidx]), 0, sizeof(lans_cnx_env_t));
}

/// LANS Task interface required by profile manager
const prf_task_cbs_t lans_itf =
{
    .cb_init          = (prf_init_cb) lans_init,
    .cb_destroy       = lans_destroy,
    .cb_con_create    = lans_con_create,
    .cb_con_cleanup   = lans_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *lans_prf_itf_get(void)
{
    return &lans_itf;
}
#endif //(BLE_LN_SENSOR)

/// @} LANS
