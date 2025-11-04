/**
 ****************************************************************************************
 *
 * @file lanc.c
 *
 * @brief Location and Navigation Profile Collector implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @addtogroup LANC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"

#if (BLE_LN_COLLECTOR)

#include "lanc.h"
#include "gap.h"
#include "gatt.h"
#include "ble_prf.h"

#include "co_utils.h"
#include "co_endian.h"
#include "co_time.h"

#include <string.h>
#include "ke_mem.h"


/*
 * DEFINES
 ****************************************************************************************
 */

/// Maximum number of Client task instances

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */


/// Environment variable for each Connections
typedef struct lanc_cnx_env
{
    /// Control point timer
    co_time_timer_t     timer;
    /// Peer database discovered handle mapping
    lanc_lns_content_t  lns;
    /// counter used to check service uniqueness
    uint8_t             nb_svc;
    /// Client is in discovering state
    bool                discover;
    /// Control point operation on-going (@see enum cpp_ctnl_pt_code)
    uint8_t             ctrl_pt_op;
} lanc_cnx_env_t;

/// Client environment variable
typedef struct lanc_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    lanc_cnx_env_t*      p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} lanc_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve Location and Navigation service characteristics information
const prf_char_def_t lanc_lns_char[LANP_LANS_CHAR_MAX] =
{
    // LN Feature
    [LANP_LANS_LN_FEAT_CHAR]    = { GATT_CHAR_LN_FEAT,     ATT_REQ(PRES, MAND), PROP(RD)           },
    // LN Measurement
    [LANP_LANS_LOC_SPEED_CHAR]  = { GATT_CHAR_LOC_SPEED,   ATT_REQ(PRES, MAND), PROP(N)            },
    // Position Quality
    [LANP_LANS_POS_Q_CHAR]      = { GATT_CHAR_POS_QUALITY, ATT_REQ(PRES, OPT),  PROP(RD)           },
    // SC Control Point
    [LANP_LANS_LN_CTNL_PT_CHAR] = { GATT_CHAR_LN_CNTL_PT,  ATT_REQ(PRES, OPT),  PROP(WR) | PROP(I) },
    // Navigation
    [LANP_LANS_NAVIG_CHAR]      = { GATT_CHAR_NAVIGATION,  ATT_REQ(PRES, OPT),  PROP(N)            },
};

/// State machine used to retrieve Location and Navigation service characteristic descriptor information
const prf_desc_def_t lanc_lns_char_desc[LANC_DESC_MAX] =
{
    // Location and Speed Char. - Client Characteristic Configuration
    [LANC_DESC_LOC_SPEED_CL_CFG]  = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, MAND), LANP_LANS_LOC_SPEED_CHAR  },
    // Control Point Char. - Client Characteristic Configuration
    [LANC_DESC_LN_CTNL_PT_CL_CFG] = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, OPT),  LANP_LANS_LN_CTNL_PT_CHAR },
    // Navigation Char. - Client Characteristic Configuration
    [LANC_DESC_NAVIGATION_CL_CFG] = { GATT_DESC_CLIENT_CHAR_CFG, ATT_REQ(PRES, OPT),  LANP_LANS_NAVIG_CHAR      },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_lanc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void lanc_enable_cmp(lanc_env_t* p_lanc_env, uint8_t conidx, uint16_t status)
{
    const lanc_cb_t* p_cb = (const lanc_cb_t*) p_lanc_env->prf_env.p_cb;

    if(p_lanc_env != NULL)
    {
        lanc_cnx_env_t* p_con_env = p_lanc_env->p_env[conidx];
        p_cb->cb_enable_cmp(conidx, status, &(p_con_env->lns));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_lanc_env->p_env[conidx] = NULL;
        }
        else
        {
             p_con_env->discover = false;

             // Register profile handle to catch gatt indications
             gatt_cli_event_register(conidx, p_lanc_env->user_lid, p_con_env->lns.svc.shdl,
                                     p_con_env->lns.svc.ehdl);
        }
    }
}


/**
 ****************************************************************************************
 * @brief Packs Control Point data
 * @param[in]  p_buf    Pointer to output buffer
 * @param[in]  op_code  Control point operation code
 * @param[in]  p_value  Control point request value
 * @return length
 ****************************************************************************************
 */
__STATIC uint16_t lanc_pack_ctnl_pt_req(co_buf_t* p_buf, uint8_t op_code, const union lanp_ln_ctnl_pt_req_val* p_value)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    // Set the operation code
    co_buf_tail(p_buf)[0] = op_code;
    co_buf_tail_reserve(p_buf, 1);

    // Fulfill the message according to the operation code
    switch (op_code)
    {
        case (LANP_LN_CTNL_PT_SET_CUMUL_VALUE):
        {
            // Set the cumulative value (24 bits)
            co_write24p(co_buf_tail(p_buf), co_htob24(p_value->cumul_val));
            co_buf_tail_reserve(p_buf, 3);
        } break;

        case (LANP_LN_CTNL_PT_MASK_LSPEED_CHAR_CT):
        {
            // Set mask content
            co_write16p(co_buf_tail(p_buf), co_htobs(p_value->mask_content));
            co_buf_tail_reserve(p_buf, 2);
        } break;

        case (LANP_LN_CTNL_PT_NAVIGATION_CONTROL):
        {
            // Set control value
            co_buf_tail(p_buf)[0] = p_value->control_value;
            co_buf_tail_reserve(p_buf, 1);
        } break;

        case (LANP_LN_CTNL_PT_REQ_NAME_OF_ROUTE):
        case (LANP_LN_CTNL_PT_SELECT_ROUTE):
        {
            // Set route number
            co_write16p(co_buf_tail(p_buf), co_htobs(p_value->route_number));
            co_buf_tail_reserve(p_buf, 2);
        } break;

        case (LANP_LN_CTNL_PT_SET_FIX_RATE):
        {
            // Set the fix rate
            co_buf_tail(p_buf)[0] = p_value->fix_rate;
            co_buf_tail_reserve(p_buf, 1);
        } break;

        case (LANP_LN_CTNL_PT_SET_ELEVATION):
        {
            // Set elevation (24 bits)
            co_write24p(co_buf_tail(p_buf), co_htob24(p_value->elevation));
            co_buf_tail_reserve(p_buf, 3);
        } break;

        case (LANP_LN_CTNL_PT_REQ_NUMBER_OF_ROUTES):
        {
            // Nothing more to do
        } break;

        default:
        {
            status = PRF_ERR_INVALID_PARAM;
        } break;
    }

    return (status);
}


/**
 ****************************************************************************************
 * @brief Unpacks measurement data and sends the indication
 * @param[in] p_lanc_env    Environment variable
 * @param[in] conidx        Connection index
 * @param[in] p_buf         Pointer of input buffer received
 ****************************************************************************************
 */
__STATIC void lanc_unpack_loc_speed(lanc_env_t* p_lanc_env, uint8_t conidx, co_buf_t* p_buf)
{
    const lanc_cb_t* p_cb = (const lanc_cb_t*) p_lanc_env->prf_env.p_cb;
    lanp_loc_speed_t loc_speed;
    memset(&loc_speed, 0, sizeof(lanp_loc_speed_t));

    // Flags
    loc_speed.flags = co_btohs(co_read16p(co_buf_data(p_buf)));
    co_buf_head_release(p_buf, 2);

    if (GETB(loc_speed.flags, LANP_LSPEED_INST_SPEED_PRESENT))
    {
        // Unpack instantaneous speed
        loc_speed.inst_speed = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    if (GETB(loc_speed.flags, LANP_LSPEED_TOTAL_DISTANCE_PRESENT))
    {
        // Unpack Total distance (24 bits)
        loc_speed.total_dist = co_btoh24(co_read24p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 3);
    }

    if (GETB(loc_speed.flags, LANP_LSPEED_LOCATION_PRESENT))
    {
        // Unpack Location
        loc_speed.latitude  = co_btohl(co_read32p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 4);
        loc_speed.longitude = co_btohl(co_read32p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 4);
    }

    if (GETB(loc_speed.flags, LANP_LSPEED_ELEVATION_PRESENT))
    {
        // Unpack Elevation (24 bits)
        loc_speed.elevation = co_btoh24(co_read24p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 3);
    }

    if (GETB(loc_speed.flags, LANP_LSPEED_HEADING_PRESENT))
    {
        // Unpack heading
        loc_speed.heading = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    if (GETB(loc_speed.flags, LANP_LSPEED_ROLLING_TIME_PRESENT))
    {
        // Unpack rolling time
        loc_speed.rolling_time = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
    }

    if (GETB(loc_speed.flags, LANP_LSPEED_UTC_TIME_PRESENT))
    {
        //Unpack UTC time
        prf_unpack_date_time(p_buf, &(loc_speed.date_time));
    }

    // Inform application about received vector
    p_cb->cb_loc_speed(conidx, &loc_speed);
}

/**
 ****************************************************************************************
 * @brief Unpacks Vector data and sends the indication
 * @param[in] p_lanc_env    Environment variable
 * @param[in] conidx        Connection index
 * @param[in] p_buf         Pointer of input buffer received
 ****************************************************************************************
 */
__STATIC void lanc_unpack_navigation(lanc_env_t* p_lanc_env, uint8_t conidx, co_buf_t* p_buf)
{
    const lanc_cb_t* p_cb = (const lanc_cb_t*) p_lanc_env->prf_env.p_cb;
    lanp_navigation_t navigation;
    memset(&navigation, 0, sizeof(lanp_navigation_t));

    // Flags
    navigation.flags = co_btohs(co_read16p(co_buf_data(p_buf)));
    co_buf_head_release(p_buf, 2);
    // Bearing
    navigation.bearing = co_btohs(co_read16p(co_buf_data(p_buf)));
    co_buf_head_release(p_buf, 2);
    // Heading
    navigation.heading = co_btohs(co_read16p(co_buf_data(p_buf)));
    co_buf_head_release(p_buf, 2);

    if (GETB(navigation.flags, LANP_NAVI_REMAINING_DIS_PRESENT))
    {
        //Unpack remaining distance (24 bits)
        navigation.remaining_distance = co_btoh24(co_read24p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 3);
    }

    if (GETB(navigation.flags, LANP_NAVI_REMAINING_VER_DIS_PRESENT))
    {
        // Unpack remaining vertical distance (24 bits)
        navigation.remaining_ver_distance = co_btoh24(co_read24p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 3);
    }

    if (GETB(navigation.flags, LANP_NAVI_ESTIMATED_TIME_OF_ARRIVAL_PRESENT))
    {
        //Unpack time
         prf_unpack_date_time(p_buf, &(navigation.estimated_arrival_time));
    }

    // Inform application about received vector
    p_cb->cb_navigation(conidx, &navigation);
}


/**
 ****************************************************************************************
 * @brief Unpacks position quality
 *
 * @param[in] p_lanc_env    Environment variable
 * @param[in] p_buf         Pointer of input buffer received
 * @param[in] p_pos_q       Pointer to output position quality structure
 ****************************************************************************************
 */
__STATIC void lanc_unpack_pos_q(lanc_env_t* p_lanc_env, co_buf_t* p_buf, lanp_posq_t* p_pos_q)
{
    /*----------------------------------------------------
     * Unpack Position Quality ---------------------------
     *----------------------------------------------------*/

    // Flags
    p_pos_q->flags = co_btohs(co_read16p(co_buf_data(p_buf)));
    co_buf_head_release(p_buf, 2);

    if (GETB(p_pos_q->flags, LANP_POSQ_NUMBER_OF_BEACONS_IN_SOLUTION_PRESENT))
    {
        //Unpack beacons in solution
        p_pos_q->n_beacons_solution = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
    }

    if (GETB(p_pos_q->flags, LANP_POSQ_NUMBER_OF_BEACONS_IN_VIEW_PRESENT))
    {
        //Unpack beacons in view
        p_pos_q->n_beacons_view = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
    }

    if (GETB(p_pos_q->flags, LANP_POSQ_TIME_TO_FIRST_FIX_PRESENT))
    {
        //Unpack time first fix
        p_pos_q->time_first_fix = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    if (GETB(p_pos_q->flags, LANP_POSQ_EHPE_PRESENT))
    {
        //Unpack ehpe
        p_pos_q->ehpe = co_btohl(co_read32p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 4);
    }

    if (GETB(p_pos_q->flags, LANP_POSQ_EVPE_PRESENT))
    {
        //Unpack evpe
        p_pos_q->evpe = co_btohl(co_read32p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 4);
    }

    if (GETB(p_pos_q->flags, LANP_POSQ_HDOP_PRESENT))
    {
        //Unpack hdop
        p_pos_q->hdop = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
    }

    if (GETB(p_pos_q->flags, LANP_POSQ_VDOP_PRESENT))
    {
        //Unpack vdop
        p_pos_q->vdop = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
    }
}


/**
 ****************************************************************************************
 * @brief Unpacks Control Point data and sends the indication
 * @param[in] p_lanc_env    Environment variable
 * @param[in] conidx        Connection index
 * @param[in] p_buf         Pointer of input buffer received
 ****************************************************************************************
 */
__STATIC void lanc_unpack_ctln_pt_rsp(lanc_env_t* p_lanc_env, uint8_t conidx, co_buf_t* p_buf)
{
    bool valid = (co_buf_data_len(p_buf) >= LANP_LAN_LN_CNTL_PT_RSP_MIN_LEN);

    uint8_t op_code;
    uint8_t req_op_code;
    uint8_t resp_value;

    // Response Op code
    op_code = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Requested operation code
    req_op_code = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Response value
    resp_value = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    if(valid && (op_code == LANP_LN_CTNL_PT_RESPONSE_CODE) && (req_op_code == p_lanc_env->p_env[conidx]->ctrl_pt_op))
    {
        const lanc_cb_t* p_cb = (const lanc_cb_t*) p_lanc_env->prf_env.p_cb;
        uint16_t number_of_routes = 0;
        co_buf_t* p_name = NULL;

        if (resp_value == LANP_LN_CTNL_PT_RESP_SUCCESS)
        {
            switch (req_op_code)
            {
                case (LANP_LN_CTNL_PT_REQ_NUMBER_OF_ROUTES):
                {
                    number_of_routes = co_btohs(co_read16p(co_buf_data(p_buf)));
                    co_buf_head_release(p_buf, 2);

                } break;

                case (LANP_LN_CTNL_PT_REQ_NAME_OF_ROUTE):
                {
                    p_name = p_buf;
                } break;

                case (LANP_LN_CTNL_PT_SET_CUMUL_VALUE):
                case (LANP_LN_CTNL_PT_MASK_LSPEED_CHAR_CT):
                case (LANP_LN_CTNL_PT_NAVIGATION_CONTROL):
                case (LANP_LN_CTNL_PT_SELECT_ROUTE):
                case (LANP_LN_CTNL_PT_SET_FIX_RATE):
                case (LANP_LN_CTNL_PT_SET_ELEVATION):
                {
                    // No parameters
                } break;

                default: { /* Nothign to do */ } break;
            }
        }

        p_lanc_env->p_env[conidx]->ctrl_pt_op = LANP_LN_CTNL_PT_RESP_RESERVED;
        // stop timer
        co_time_timer_stop(&(p_lanc_env->p_env[conidx]->timer));
        // provide control point response
        p_cb->cb_ctnl_pt_req_cmp(conidx, GAP_ERR_NO_ERROR, req_op_code, resp_value, number_of_routes, p_name);
    }
}


/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] val_id        Value Identifier (@see enum lanc_val_id)
 * @param[in] length        Length of data value)
 * @param[in] p_data        Pointer of data value
 ****************************************************************************************
 */
__STATIC void lanc_read_val_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, co_buf_t* p_buf)
{
    lanc_env_t* p_lanc_env = PRF_ENV_GET(LANC, lanc);

    if(p_lanc_env != NULL)
    {
        const lanc_cb_t* p_cb = (const lanc_cb_t*) p_lanc_env->prf_env.p_cb;
        switch (val_id)
        {
            // Read LN Feature
            case (LANC_RD_LN_FEAT):
            {
                uint32_t features = 0;
                if(status == GAP_ERR_NO_ERROR)
                {
                    features = co_btohl(co_read32p(co_buf_data(p_buf)));
                }
                p_cb->cb_read_features_cmp(conidx, status, features);
            } break;

            // Read Position Quality
            case (LANC_RD_POS_Q):
            {
                lanp_posq_t pos_q;
                memset(&pos_q, 0, sizeof(lanp_posq_t));

                if(status == GAP_ERR_NO_ERROR)
                {
                    lanc_unpack_pos_q(p_lanc_env, p_buf, &(pos_q));
                }

                p_cb->cb_read_pos_q_cmp(conidx, status, &(pos_q));

            } break;

            // Read Client Characteristic Configuration Descriptor value
            case (LANC_RD_WR_LOC_SPEED_CL_CFG):
            case (LANC_RD_WR_LN_CTNL_PT_CFG):
            case (LANC_RD_WR_NAVIGATION_CFG):
            {
                uint16_t cfg_val = 0;

                if(status == GAP_ERR_NO_ERROR)
                {
                    cfg_val = co_btohs(co_read16p(co_buf_data(p_buf)));
                }

                p_cb->cb_read_cfg_cmp(conidx, status, val_id, cfg_val);
            } break;

            default:
            {
                ASSERT_ERR(0);
            } break;
        }
    }
}

/**
 ****************************************************************************************
 * @brief Perform Value read procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] val_id        Value Identifier (@see enum lanc_info)
 ****************************************************************************************
 */
__STATIC uint16_t lanc_read_val(uint8_t conidx, uint16_t val_id)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    lanc_env_t* p_lanc_env = PRF_ENV_GET(LANC, lanc);

    if(p_lanc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_lanc_env->p_env[conidx] != NULL) && (!p_lanc_env->p_env[conidx]->discover))
        {
            lanc_cnx_env_t* p_con_env = p_lanc_env->p_env[conidx];
            uint16_t hdl;
            lanc_lns_content_t* p_lns = &(p_con_env->lns);

            switch(val_id)
            {
                case LANC_RD_LN_FEAT:             { hdl = p_lns->chars[LANP_LANS_LN_FEAT_CHAR].val_hdl;       } break;
                case LANC_RD_POS_Q:               { hdl = p_lns->chars[LANP_LANS_POS_Q_CHAR].val_hdl;         } break;
                case LANC_RD_WR_LOC_SPEED_CL_CFG: { hdl = p_lns->descs[LANC_DESC_LOC_SPEED_CL_CFG].desc_hdl;  } break;
                case LANC_RD_WR_LN_CTNL_PT_CFG:   { hdl = p_lns->descs[LANC_DESC_LN_CTNL_PT_CL_CFG].desc_hdl; } break;
                case LANC_RD_WR_NAVIGATION_CFG:   { hdl = p_lns->descs[LANC_DESC_NAVIGATION_CL_CFG].desc_hdl; } break;
                default:                          { hdl = GATT_INVALID_HDL;                                   } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else
            {
                // perform read request
                status = gatt_cli_read(conidx, p_lanc_env->user_lid, val_id, hdl, 0, 0);
            }
        }
    }

    return (status);
}

/**
 ****************************************************************************************
 * @brief Function to called once timer expires
 *
 * @param[in] conidx Connection index
 ****************************************************************************************
 */
__STATIC void lanc_timer_handler(uint32_t conidx)
{
    // Get the address of the environment
    lanc_env_t *p_lanc_env = PRF_ENV_GET(LANC, lanc);

    if (p_lanc_env != NULL)
    {
        lanc_cnx_env_t* p_con_env = p_lanc_env->p_env[conidx];
        ASSERT_ERR(p_con_env != NULL);
        if(p_con_env->ctrl_pt_op != LANP_LN_CTNL_PT_RESP_RESERVED)
        {
            const lanc_cb_t* p_cb = (const lanc_cb_t*) p_lanc_env->prf_env.p_cb;
            uint8_t op_code = p_con_env->ctrl_pt_op;
            p_con_env->ctrl_pt_op = LANP_LN_CTNL_PT_RESP_RESERVED;

            p_cb->cb_ctnl_pt_req_cmp((uint8_t)conidx, PRF_ERR_PROC_TIMEOUT, op_code, 0, 0, NULL);
        }
    }
}

/*
 * GATT USER CLIENT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief This function is called when a full service has been found during a discovery procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] dummy         Dummy parameter provided by upper layer for command execution
 * @param[in] hdl           First handle value of following list
 * @param[in] disc_info     Discovery information (@see enum gatt_svc_disc_info)
 * @param[in] nb_att        Number of attributes
 * @param[in] p_atts        Pointer to attribute information present in a service
 ****************************************************************************************
 */
__STATIC void lanc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    lanc_env_t* p_lanc_env = PRF_ENV_GET(LANC, lanc);

    if(p_lanc_env != NULL)
    {
        lanc_cnx_env_t* p_con_env = p_lanc_env->p_env[conidx];

        ASSERT_INFO(p_con_env != NULL, conidx, user_lid);

        if (p_con_env->nb_svc == 0)
        {
            //Even if we get multiple responses we only store 1 range
            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_START))
            {
                p_con_env->lns.svc.shdl = hdl;
            }

            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
            {
                p_con_env->lns.svc.ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 LANP_LANS_CHAR_MAX, &lanc_lns_char[0],      &(p_con_env->lns.chars[0]),
                                 LANC_DESC_MAX,    &lanc_lns_char_desc[0], &(p_con_env->lns.descs[0]));
        }

        if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
        {
            p_con_env->nb_svc++;
        }
    }
}

/**
 ****************************************************************************************
 * @brief This function is called when GATT client user discovery procedure is over.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] dummy         Dummy parameter provided by upper layer for command execution
 * @param[in] status        Status of the procedure (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void lanc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    lanc_env_t* p_lanc_env = PRF_ENV_GET(LANC, lanc);

    if(p_lanc_env != NULL)
    {
        lanc_cnx_env_t* p_con_env = p_lanc_env->p_env[conidx];

        if (p_con_env->nb_svc ==  1)
        {
            status = prf_check_svc_validity(LANP_LANS_CHAR_MAX, p_con_env->lns.chars, lanc_lns_char,
                                            LANC_DESC_MAX, p_con_env->lns.descs, lanc_lns_char_desc);
        }
        // too much services
        else if (p_con_env->nb_svc > 1)
        {
            status = PRF_ERR_MULTIPLE_SVC;
        }
        // no services found
        else
        {
            status = PRF_ERR_STOP_DISC_CHAR_MISSING;
        }

        lanc_enable_cmp(p_lanc_env, conidx, status);
    }
}

/**
 ****************************************************************************************
 * @brief This function is called during a read procedure when attribute value is retrieved
 *        form peer device.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] dummy         Dummy parameter provided by upper layer for command execution
 * @param[in] hdl           Attribute handle
 * @param[in] offset        Data offset
 * @param[in] p_data        Pointer to buffer that contains attribute value starting from offset
 ****************************************************************************************
 */
__STATIC void lanc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              co_buf_t* p_data)
{
    lanc_read_val_cmp(conidx, GAP_ERR_NO_ERROR, (uint8_t) dummy, p_data);
}


/**
 ****************************************************************************************
 * @brief This function is called when GATT client user read procedure is over.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] dummy         Dummy parameter provided by upper layer for command execution
 * @param[in] status        Status of the procedure (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void lanc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != GAP_ERR_NO_ERROR)
    {
        lanc_read_val_cmp(conidx, status, (uint8_t) dummy, NULL);
    }
}

/**
 ****************************************************************************************
 * @brief This function is called when GATT client user write procedure is over.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] dummy         Dummy parameter provided by upper layer for command execution
 * @param[in] status        Status of the procedure (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void lanc_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    lanc_env_t* p_lanc_env = PRF_ENV_GET(LANC, lanc);

    if(p_lanc_env != NULL)
    {
        const lanc_cb_t* p_cb = (const lanc_cb_t*) p_lanc_env->prf_env.p_cb;

        switch(dummy)
        {
            // Config control
            case LANC_RD_WR_LOC_SPEED_CL_CFG:
            case LANC_RD_WR_LN_CTNL_PT_CFG:
            case LANC_RD_WR_NAVIGATION_CFG:
            {
                p_cb->cb_write_cfg_cmp(conidx, status, dummy);
            } break;
            // Control point commands
            case LANC_IND_LN_CTNL_PT:
            {
                if(status != GAP_ERR_NO_ERROR)
                {
                    uint8_t opcode = p_lanc_env->p_env[conidx]->ctrl_pt_op;
                    p_lanc_env->p_env[conidx]->ctrl_pt_op = LANP_LN_CTNL_PT_RESP_RESERVED;
                    p_cb->cb_ctnl_pt_req_cmp(conidx, status, opcode, 0, 0, NULL);
                }
                else
                {
                    // Start Timeout Procedure - wait for Indication reception
                    co_time_timer_set(&(p_lanc_env->p_env[conidx]->timer), LANP_CP_TIMEOUT);
                }
            } break;

            default: { /* Nothing to do */} break;
        }
    }
}


/**
 ****************************************************************************************
 * @brief This function is called when a notification or an indication is received onto
 *        register handle range (@see gatt_cli_event_register).
 *
 *        @see gatt_cli_val_event_cfm must be called to confirm event reception.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] token         Procedure token that must be returned in confirmation function
 * @param[in] evt_type      Event type triggered (@see enum gatt_evt_type)
 * @param[in] complete      True if event value if complete value has been received
 *                          False if data received is equals to maximum attribute protocol value.
 *                          In such case GATT Client User should perform a read procedure.
 * @param[in] hdl           Attribute handle
 * @param[in] p_data        Pointer to buffer that contains attribute value
 ****************************************************************************************
 */
__STATIC void lanc_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                        uint16_t hdl, co_buf_t* p_data)
{
    lanc_env_t* p_lanc_env = PRF_ENV_GET(LANC, lanc);

    if(p_lanc_env != NULL)
    {
        lanc_cnx_env_t* p_con_env = p_lanc_env->p_env[conidx];
        lanc_lns_content_t* p_lns = &(p_con_env->lns);

        if (hdl == p_lns->chars[LANP_LANS_LOC_SPEED_CHAR].val_hdl)
        {
            //Unpack location and speed information
            lanc_unpack_loc_speed(p_lanc_env, conidx, p_data);
        }
        else if (hdl == p_lns->chars[LANP_LANS_NAVIG_CHAR].val_hdl)
        {
            //Unpack Navigation information
            lanc_unpack_navigation(p_lanc_env, conidx, p_data);
        }
        else if (hdl == p_lns->chars[LANP_LANS_LN_CTNL_PT_CHAR].val_hdl)
        {
            // Unpack control point
            lanc_unpack_ctln_pt_rsp(p_lanc_env, conidx, p_data);
        }
    }

    // confirm event handling
    gatt_cli_att_event_cfm(conidx, user_lid, token);
}

/**
 ****************************************************************************************
 * @brief Event triggered when a service change has been received or if an attribute
 *        transaction triggers an out of sync error.
 *
 * @param[in] conidx        Connection index
 * @param[in] user_lid      GATT user local identifier
 * @param[in] out_of_sync   True if an out of sync error has been received
 * @param[in] start_hdl     Service start handle
 * @param[in] end_hdl       Service end handle
 ****************************************************************************************
 */
__STATIC void lanc_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do Nothing
}

/// Client callback hander
__STATIC const gatt_cli_cb_t lanc_cb =
{
    .cb_discover_cmp    = lanc_discover_cmp_cb,
    .cb_read_cmp        = lanc_read_cmp_cb,
    .cb_write_cmp       = lanc_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = lanc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = lanc_att_val_cb,
    .cb_att_val_evt     = lanc_att_val_evt_cb,
    .cb_svc_changed     = lanc_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t lanc_enable(uint8_t conidx, uint8_t con_type, const lanc_lns_content_t* p_lns)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    lanc_env_t* p_lanc_env = PRF_ENV_GET(LANC, lanc);

    if(p_lanc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_lanc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_lanc_env->p_env[conidx] = (struct lanc_cnx_env *) ke_malloc(sizeof(struct lanc_cnx_env), KE_MEM_ATT_DB);

            if(p_lanc_env->p_env[conidx] != NULL)
            {
                memset(p_lanc_env->p_env[conidx], 0, sizeof(struct lanc_cnx_env));
                co_time_timer_init(&(p_lanc_env->p_env[conidx]->timer), (co_time_timer_cb)lanc_timer_handler,
                                   (uint8_t*) ((uint32_t) conidx));

                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_LOCATION_AND_NAVIGATION;

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_lanc_env->user_lid, 0, GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_lanc_env->p_env[conidx]->discover   = true;
                    p_lanc_env->p_env[conidx]->ctrl_pt_op = LANP_LN_CTNL_PT_RESP_RESERVED;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_lanc_env->p_env[conidx]->lns), p_lns, sizeof(lanc_lns_content_t));
                    status = GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    lanc_enable_cmp(p_lanc_env, conidx, GAP_ERR_NO_ERROR);
                }
            }
            else
            {
                status = GAP_ERR_INSUFF_RESOURCES;
            }
        }
    }

    return (status);
}

uint16_t lanc_read_features(uint8_t conidx)
{
    uint16_t status = lanc_read_val(conidx, LANC_RD_LN_FEAT);
    return (status);
}

uint16_t lanc_read_pos_q(uint8_t conidx)
{
    uint16_t status = lanc_read_val(conidx, LANC_RD_POS_Q);
    return (status);
}

uint16_t lanc_read_cfg(uint8_t conidx, uint8_t desc_code)
{
    uint16_t status;

    switch(desc_code)
    {
        case LANC_RD_WR_LOC_SPEED_CL_CFG:
        case LANC_RD_WR_LN_CTNL_PT_CFG:
        case LANC_RD_WR_NAVIGATION_CFG:  { status = lanc_read_val(conidx, desc_code);  } break;
        default:                         { status = PRF_ERR_INEXISTENT_HDL;            } break;
    }

    return (status);
}

uint16_t lanc_write_cfg(uint8_t conidx, uint8_t desc_code, uint16_t cfg_val)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    lanc_env_t* p_lanc_env = PRF_ENV_GET(LANC, lanc);

    if(p_lanc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_lanc_env->p_env[conidx] != NULL) && (!p_lanc_env->p_env[conidx]->discover))
        {
            lanc_cnx_env_t* p_con_env = p_lanc_env->p_env[conidx];
            uint16_t hdl;
            uint16_t cfg_en_val = 0;
            lanc_lns_content_t* p_lns = &(p_con_env->lns);

            switch(desc_code)
            {
                case LANC_RD_WR_LOC_SPEED_CL_CFG: { hdl        = p_lns->descs[LANC_DESC_LOC_SPEED_CL_CFG].desc_hdl;
                                                    cfg_en_val =  PRF_CLI_START_NTF;                                 } break;
                case LANC_RD_WR_NAVIGATION_CFG:   { hdl        = p_lns->descs[LANC_DESC_NAVIGATION_CL_CFG].desc_hdl;
                                                    cfg_en_val =  PRF_CLI_START_NTF;                                 } break;
                case LANC_RD_WR_LN_CTNL_PT_CFG:   { hdl        = p_lns->descs[LANC_DESC_LN_CTNL_PT_CL_CFG].desc_hdl;
                                                    cfg_en_val =  PRF_CLI_START_IND;                                 } break;
                default:                          { hdl = GATT_INVALID_HDL;                                          } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else if((cfg_val != PRF_CLI_STOP_NTFIND) && (cfg_val != cfg_en_val))
            {
                status = PRF_ERR_INVALID_PARAM;
            }
            else
            {
                // Force endianess
                cfg_val = co_htobs(cfg_val);
                status = prf_gatt_write(conidx, p_lanc_env->user_lid, desc_code, GATT_WRITE,
                                        hdl, sizeof(uint16_t), (uint8_t *)&cfg_val);
            }
        }
    }

    return (status);
}

uint16_t lanc_ctnl_pt_req(uint8_t conidx, uint8_t req_op_code, const union lanp_ln_ctnl_pt_req_val* p_value)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    lanc_env_t* p_lanc_env = PRF_ENV_GET(LANC, lanc);

    if(p_value == NULL)
    {
        status = PRF_ERR_INVALID_PARAM;
    }
    else if(p_lanc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_lanc_env->p_env[conidx] != NULL) && (!p_lanc_env->p_env[conidx]->discover))
        {
            lanc_cnx_env_t* p_con_env = p_lanc_env->p_env[conidx];
            lanc_lns_content_t* p_lns = &(p_con_env->lns);
            uint16_t hdl = p_lns->chars[LANP_LANS_LN_CTNL_PT_CHAR].val_hdl;

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            // reject if there is an ongoing control point operation
            else if(p_con_env->ctrl_pt_op != LANP_LN_CTNL_PT_RESP_RESERVED)
            {
                status = PRF_ERR_REQ_DISALLOWED;
            }
            else
            {
                co_buf_t* p_buf = NULL;

                // allocate buffer for event transmission
                if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, LANP_LAN_LN_CNTL_PT_REQ_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
                {
                    status = lanc_pack_ctnl_pt_req(p_buf, req_op_code, p_value);

                    if(status == GAP_ERR_NO_ERROR)
                    {
                        status = gatt_cli_write(conidx, p_lanc_env->user_lid, LANC_IND_LN_CTNL_PT, GATT_WRITE, hdl, 0, p_buf);
                        if(status == GAP_ERR_NO_ERROR)
                        {
                            // save on-going operation
                            p_con_env->ctrl_pt_op = req_op_code;
                        }
                    }
                    co_buf_release(p_buf);
                }
                else
                {
                    status = GAP_ERR_INSUFF_RESOURCES;
                }

            }
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
 * @brief Send a LANC_CMP_EVT message to the task which enabled the profile
 * @param[in] conidx Connection index
 * @param[in] operation Operation
 * @param[in] status Satus
 ****************************************************************************************
 */
__STATIC void lanc_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct lanc_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(LANC_CMP_EVT, PRF_DST_TASK(LANC), PRF_SRC_TASK(LANC), lanc_cmp_evt);
    if(p_evt)
    {
        p_evt->conidx     = conidx;
        p_evt->operation  = operation;
        p_evt->status     = status;

        ke_msg_send(p_evt);
    }
}

/**
 ****************************************************************************************
 * @brief  Message handler example
 *
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int lanc_enable_req_handler(ke_msg_id_t const msgid, struct lanc_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = lanc_enable(p_param->conidx, p_param->con_type, &(p_param->lns));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct lanc_enable_rsp *p_rsp = KE_MSG_ALLOC(LANC_ENABLE_RSP, src_id, dest_id, lanc_enable_rsp);
        if(p_rsp != NULL)
        {
            p_rsp->conidx = p_param->conidx;
            p_rsp->status = status;
            ke_msg_send(p_rsp);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief  Message Handler example
 * @param[in] msgid Id of the message received (probably unused).
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance (probably unused).
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int lanc_read_cmd_handler(ke_msg_id_t const msgid, struct lanc_read_cmd const *p_param,
                                   ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status;

    switch(p_param->read_code)
    {
        case LANC_RD_LN_FEAT:             { status = lanc_read_features(p_param->conidx);                } break;
        case LANC_RD_POS_Q:               { status = lanc_read_pos_q(p_param->conidx);                   } break;
        case LANC_RD_WR_LOC_SPEED_CL_CFG:
        case LANC_RD_WR_LN_CTNL_PT_CFG:
        case LANC_RD_WR_NAVIGATION_CFG:   { status = lanc_read_cfg(p_param->conidx, p_param->read_code); } break;
        default:                          { status = PRF_ERR_INEXISTENT_HDL;                             } break;
    }

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct lanc_cmp_evt *p_evt = KE_MSG_ALLOC(LANC_CMP_EVT, src_id, dest_id, lanc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = LANC_READ_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref LANC_CFG_NTFIND_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int lanc_cfg_ntfind_cmd_handler(ke_msg_id_t const msgid,
                                        struct lanc_cfg_ntfind_cmd *p_param,
                                        ke_task_id_t const dest_id,
                                        ke_task_id_t const src_id)
{
    uint16_t status = lanc_write_cfg(p_param->conidx, p_param->desc_code, p_param->ntfind_cfg);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct lanc_cmp_evt *p_evt = KE_MSG_ALLOC(LANC_CMP_EVT, src_id, dest_id, lanc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = LANC_CFG_NTF_IND_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref LANC_LN_CTNL_PT_CFG_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int lanc_ln_ctnl_pt_cfg_cmd_handler(ke_msg_id_t const msgid, struct lanc_ln_ctnl_pt_cfg_cmd *p_param,
                                             ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = lanc_ctnl_pt_req(p_param->conidx, p_param->ln_ctnl_pt.op_code, &(p_param->ln_ctnl_pt.value));

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct lanc_cmp_evt *p_evt = KE_MSG_ALLOC(LANC_CMP_EVT, src_id, dest_id, lanc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = LANC_LN_CTNL_PT_CFG_WR_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(lanc)
{
    // Note: all messages must be sorted in ID ascending order

    { LANC_ENABLE_REQ,               (ke_msg_func_t) lanc_enable_req_handler         },
    { LANC_READ_CMD,                 (ke_msg_func_t) lanc_read_cmd_handler           },
    { LANC_CFG_NTFIND_CMD,           (ke_msg_func_t) lanc_cfg_ntfind_cmd_handler     },
    { LANC_LN_CTNL_PT_CFG_CMD,       (ke_msg_func_t) lanc_ln_ctnl_pt_cfg_cmd_handler },
};


/**
 ****************************************************************************************
 * @brief Completion of enable procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] p_lns         Pointer to peer database description bond data
 ****************************************************************************************
 */
__STATIC void lanc_cb_enable_cmp(uint8_t conidx, uint16_t status, const lanc_lns_content_t* p_lns)
{
    // Send APP the details of the discovered attributes on LANC
    struct lanc_enable_rsp *p_rsp = KE_MSG_ALLOC(LANC_ENABLE_RSP, PRF_DST_TASK(LANC), PRF_SRC_TASK(LANC),
                                                 lanc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->lns), p_lns, sizeof(lanc_lns_content_t));
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of read features procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] sensor_feat   CP sensor feature
 *
 ****************************************************************************************
 */
__STATIC void lanc_cb_read_features_cmp(uint8_t conidx, uint16_t status, uint32_t features)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(LANC);
    ke_task_id_t dest_id = PRF_DST_TASK(LANC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct lanc_value_ind *p_ind = KE_MSG_ALLOC(LANC_VALUE_IND, dest_id, src_id, lanc_value_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx            = conidx;
            p_ind->att_code          = LANC_RD_LN_FEAT;
            p_ind->value.ln_feat     = features;
            ke_msg_send(p_ind);
        }
    }

    lanc_send_cmp_evt(conidx, LANC_READ_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of read position quality procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] p_pos_q       Pointer to position quality informations
 *
 ****************************************************************************************
 */
__STATIC void lanc_cb_read_pos_q_cmp(uint8_t conidx, uint16_t status, const lanp_posq_t* p_pos_q)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(LANC);
    ke_task_id_t dest_id = PRF_DST_TASK(LANC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct lanc_value_ind *p_ind = KE_MSG_ALLOC(LANC_VALUE_IND, dest_id, src_id, lanc_value_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx            = conidx;
            p_ind->att_code          = LANC_RD_POS_Q;
            memcpy(&(p_ind->value.pos_q), p_pos_q, sizeof(lanp_posq_t));
            ke_msg_send(p_ind);
        }
    }

    lanc_send_cmp_evt(conidx, LANC_READ_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of read Characteristic Configuration procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] desc_code     Descriptor Code (@see enum lanc_code)
 *                              - LANC_RD_WR_LOC_SPEED_CL_CFG: Location and Speed Client Char. Configuration Descriptor
 *                              - LANC_RD_WR_LN_CTNL_PT_CFG: Control Point Client Char. Configuration Descriptor
 *                              - LANC_RD_WR_NAVIGATION_CFG: Navigation Client Char. Configuration Descriptor
 * @param[in] cfg_val       Configuration value
 *
 ****************************************************************************************
 */
__STATIC void lanc_cb_read_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t desc_code, uint16_t cfg_val)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(LANC);
    ke_task_id_t dest_id = PRF_DST_TASK(LANC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct lanc_value_ind *p_ind = KE_MSG_ALLOC(LANC_VALUE_IND, dest_id, src_id, lanc_value_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx            = conidx;
            p_ind->att_code          = desc_code;
            p_ind->value.ntf_cfg     = cfg_val;
            ke_msg_send(p_ind);
        }
    }

    lanc_send_cmp_evt(conidx, LANC_READ_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of write Characteristic Configuration procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] desc_code     Descriptor Code (@see enum lanc_code)
 *                              - LANC_RD_WR_LOC_SPEED_CL_CFG: Location and Speed Client Char. Configuration Descriptor
 *                              - LANC_RD_WR_LN_CTNL_PT_CFG: Control Point Client Char. Configuration Descriptor
 *                              - LANC_RD_WR_NAVIGATION_CFG: Navigation Client Char. Configuration Descriptor
 *
 ****************************************************************************************
 */
__STATIC void lanc_cb_write_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t desc_code)
{
    lanc_send_cmp_evt(conidx, LANC_CFG_NTF_IND_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Function called when location and speed information is received
 *
 * @param[in] conidx         Connection index
 * @param[in] p_loc_speed    Pointer to data information
 ****************************************************************************************
 */
__STATIC void lanc_cb_loc_speed(uint8_t conidx, const lanp_loc_speed_t* p_loc_speed)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(LANC);
    ke_task_id_t dest_id = PRF_DST_TASK(LANC);

    struct lanc_value_ind *p_ind = KE_MSG_ALLOC(LANC_VALUE_IND, dest_id, src_id, lanc_value_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx            = conidx;
        p_ind->att_code          = LANC_NTF_LOC_SPEED;
        memcpy(&(p_ind->value.loc_speed), p_loc_speed, sizeof(lanp_loc_speed_t));
        ke_msg_send(p_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Function called when CP vector information is received
 *
 * @param[in] conidx         Connection index
 * @param[in] p_vector       Pointer to navigation information
 ****************************************************************************************
 */
__STATIC void lanc_cb_navigation(uint8_t conidx, const lanp_navigation_t* p_nav)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(LANC);
    ke_task_id_t dest_id = PRF_DST_TASK(LANC);

    struct lanc_value_ind *p_ind = KE_MSG_ALLOC(LANC_VALUE_IND, dest_id, src_id, lanc_value_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx            = conidx;
        p_ind->att_code          = LANC_NTF_NAVIGATION;
        memcpy(&(p_ind->value.navigation), p_nav, sizeof(lanp_navigation_t));
        ke_msg_send(p_ind);
    }
}


/**
 ****************************************************************************************
 * @brief Completion of control point request procedure
 *
 * @param[in] conidx           Connection index
 * @param[in] status           Status of the Request Send (@see enum hl_err)
 * @param[in] req_op_code      Requested Operation Code @see enum lanp_ln_ctnl_pt_code
 * @param[in] resp_value       Response Value @see enum lanp_ctnl_pt_resp_val
 * @param[in] number_of_routes Number of route (valid only if req_op_code = LANP_LN_CTNL_PT_REQ_NUMBER_OF_ROUTES)
 * @param[in] p_name           Route name (valid only if req_op_code = LANP_LN_CTNL_PT_REQ_NAME_OF_ROUTE)
 ****************************************************************************************
 */
__STATIC void lanc_cb_ctnl_pt_req_cmp(uint8_t conidx, uint16_t status, uint8_t req_op_code, uint8_t resp_value,
                                      uint16_t number_of_routes, co_buf_t* p_name)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(LANC);
    ke_task_id_t dest_id = PRF_DST_TASK(LANC);

    if(status == GAP_ERR_NO_ERROR)
    {
        uint16_t data_len = (p_name != NULL) ? co_buf_data_len(p_name) : 0;
        struct lanc_ln_ctnl_pt_ind *p_ind = KE_MSG_ALLOC_DYN(LANC_LN_CTNL_PT_IND, dest_id, src_id, lanc_ln_ctnl_pt_ind, data_len);
        if(p_ind != NULL)
        {
            p_ind->conidx            = conidx;
            p_ind->rsp.op_code       = LANP_LN_CTNL_PT_RESPONSE_CODE;
            p_ind->rsp.req_op_code   = req_op_code;
            p_ind->rsp.resp_value    = resp_value;

            if(req_op_code == LANP_LN_CTNL_PT_REQ_NUMBER_OF_ROUTES)
            {
                p_ind->rsp.value.number_of_routes = number_of_routes;
            }
            else if(req_op_code == LANP_LN_CTNL_PT_REQ_NAME_OF_ROUTE)
            {
                p_ind->rsp.value.route.length = data_len;
                if(p_name != NULL)
                {
                    co_buf_copy_data_to_mem(p_name, p_ind->rsp.value.route.name, data_len);
                }
            }

            ke_msg_send(p_ind);
        }
    }

    lanc_send_cmp_evt(conidx, LANC_LN_CTNL_PT_CFG_WR_OP_CODE, status);
}

/// Default Message handle
__STATIC const lanc_cb_t lanc_msg_cb =
{
     .cb_enable_cmp           = lanc_cb_enable_cmp,
     .cb_read_features_cmp    = lanc_cb_read_features_cmp,
     .cb_read_pos_q_cmp       = lanc_cb_read_pos_q_cmp,
     .cb_read_cfg_cmp         = lanc_cb_read_cfg_cmp,
     .cb_write_cfg_cmp        = lanc_cb_write_cfg_cmp,
     .cb_loc_speed            = lanc_cb_loc_speed,
     .cb_navigation           = lanc_cb_navigation,
     .cb_ctnl_pt_req_cmp      = lanc_cb_ctnl_pt_req_cmp,
};
#endif // (BLE_HL_MSG_API)


/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief Initialization of the Client module.
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
__STATIC uint16_t lanc_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const lanc_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        lanc_env_t* p_lanc_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(lanc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_read_features_cmp == NULL)
           || (p_cb->cb_read_pos_q_cmp == NULL) || (p_cb->cb_read_cfg_cmp == NULL) || (p_cb->cb_write_cfg_cmp == NULL)
           || (p_cb->cb_loc_speed == NULL) || (p_cb->cb_navigation == NULL) || (p_cb->cb_ctnl_pt_req_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register LANC user
        status = gatt_user_cli_register(LANP_LAN_LN_CNTL_PT_RSP_MAX_LEN + GATT_NTF_HEADER_LEN, user_prio, &lanc_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_lanc_env = (lanc_env_t*) ke_malloc(sizeof(lanc_env_t), KE_MEM_ATT_DB);

        if(p_lanc_env != NULL)
        {
            // allocate LANC required environment variable
            p_env->p_env = (prf_hdr_t *) p_lanc_env;

            // initialize environment variable
            p_lanc_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = lanc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(lanc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_lanc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_lanc_env->p_env[conidx] = NULL;
            }
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
__STATIC uint16_t lanc_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    lanc_env_t* p_lanc_env = (lanc_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_lanc_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t conidx;

        // cleanup environment variable for each task instances
        for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
        {
            if (p_lanc_env->p_env[conidx] != NULL)
            {
                if(reason != PRF_DESTROY_RESET)
                {
                    co_time_timer_stop(&(p_lanc_env->p_env[conidx]->timer));
                }

                ke_free(p_lanc_env->p_env[conidx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_lanc_env);
    }

    return (status);
}

/**
 ****************************************************************************************
 * @brief Handles Connection creation
 *
 * @param[in|out]    env          Collector or Service allocated environment data.
 * @param[in]        conidx       Connection index
 * @param[in]        p_con_param  Pointer to connection parameters information
 ****************************************************************************************
 */
__STATIC void lanc_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    // Nothing to do
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
__STATIC void lanc_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    lanc_env_t* p_lanc_env = (lanc_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_lanc_env->p_env[conidx] != NULL)
    {
        co_time_timer_stop(&(p_lanc_env->p_env[conidx]->timer));
        ke_free(p_lanc_env->p_env[conidx]);
        p_lanc_env->p_env[conidx] = NULL;
    }
}

/// LANC Task interface required by profile manager
const prf_task_cbs_t lanc_itf =
{
    .cb_init          = (prf_init_cb) lanc_init,
    .cb_destroy       = lanc_destroy,
    .cb_con_create    = lanc_con_create,
    .cb_con_cleanup   = lanc_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* lanc_prf_itf_get(void)
{
    return &lanc_itf;
}

#endif //(BLE_LN_COLLECTOR)

/// @} LAN
