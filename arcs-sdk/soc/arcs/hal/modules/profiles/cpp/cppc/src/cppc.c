/**
 ****************************************************************************************
 *
 * @file cppc.c
 *
 * @brief Cycling Power Profile Collector implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @addtogroup CPPC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"
#if (BLE_CP_COLLECTOR)

#include "cppc.h"
#include "gap.h"
#include "gatt.h"

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
typedef struct cppc_cnx_env
{
    /// Control point timer
    co_time_timer_t     timer;
    /// Peer database discovered handle mapping
    cppc_cps_content_t  cps;
    /// counter used to check service uniqueness
    uint8_t             nb_svc;
    /// Client is in discovering state
    bool                discover;
    /// Control point operation on-going (@see enum cpp_ctnl_pt_code)
    uint8_t             ctrl_pt_op;
} cppc_cnx_env_t;

/// Client environment variable
typedef struct cppc_env
{
    /// profile environment
    prf_hdr_t            prf_env;
    /// Environment variable pointer for each connections
    cppc_cnx_env_t*      p_env[BLE_CONNECTION_MAX];
    /// GATT User local identifier
    uint8_t              user_lid;
} cppc_env_t;


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// State machine used to retrieve Cycling Power service characteristics information
const prf_char_def_t cppc_cps_char[CPP_CPS_CHAR_MAX] =
{
    /// CP Measurement
    [CPP_CPS_MEAS_CHAR]         = { GATT_CHAR_CP_MEAS,      ATT_REQ(PRES, MAND),  PROP(N)             },
    /// CP Feature
    [CPP_CPS_FEAT_CHAR]         = { GATT_CHAR_CP_FEAT,      ATT_REQ(PRES, MAND),  PROP(RD)            },
    /// Sensor Location
    [CPP_CPS_SENSOR_LOC_CHAR]   = { GATT_CHAR_SENSOR_LOC,   ATT_REQ(PRES, MAND),  PROP(RD)            },
    /// Vector
    [CPP_CPS_VECTOR_CHAR]       = { GATT_CHAR_CP_VECTOR,    ATT_REQ(PRES, OPT),   PROP(N)             },
    /// CP Control Point
    [CPP_CPS_CTNL_PT_CHAR]      = { GATT_CHAR_CP_CNTL_PT,   ATT_REQ(PRES, OPT),   PROP(WR) | PROP(I)  },
};

/// State machine used to retrieve Cycling Power service characteristic descriptor information
const prf_desc_def_t cppc_cps_char_desc[CPPC_DESC_MAX] =
{
    /// CP Measurement Char. - Client Characteristic Configuration
    [CPPC_DESC_CP_MEAS_CL_CFG] = { GATT_DESC_CLIENT_CHAR_CFG,   ATT_REQ(PRES, MAND), CPP_CPS_MEAS_CHAR    },
    /// CP Measurement Char. - Server Characteristic Configuration
    [CPPC_DESC_CP_MEAS_SV_CFG] = { GATT_DESC_SERVER_CHAR_CFG,   ATT_REQ(PRES, OPT),  CPP_CPS_MEAS_CHAR    },
    /// CP Vector Char. - Client Characteristic Configuration
    [CPPC_DESC_VECTOR_CL_CFG]  = { GATT_DESC_CLIENT_CHAR_CFG,   ATT_REQ(PRES, OPT),  CPP_CPS_VECTOR_CHAR  },
    /// Control Point Char. - Client Characteristic Configuration
    [CPPC_DESC_CTNL_PT_CL_CFG] = { GATT_DESC_CLIENT_CHAR_CFG,   ATT_REQ(PRES, OPT),  CPP_CPS_CTNL_PT_CHAR },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_cppc_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void cppc_enable_cmp(cppc_env_t* p_cppc_env, uint8_t conidx, uint16_t status)
{
    const cppc_cb_t* p_cb = (const cppc_cb_t*) p_cppc_env->prf_env.p_cb;

    if(p_cppc_env != NULL)
    {
        cppc_cnx_env_t* p_con_env = p_cppc_env->p_env[conidx];
        p_cb->cb_enable_cmp(conidx, status, &(p_con_env->cps));

        if (status != GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_cppc_env->p_env[conidx] = NULL;
        }
        else
        {
             p_con_env->discover = false;

             // Register profile handle to catch gatt indications
             gatt_cli_event_register(conidx, p_cppc_env->user_lid, p_con_env->cps.svc.shdl,
                                     p_con_env->cps.svc.ehdl);
        }
    }
}

/**
 ****************************************************************************************
 * @brief Send read result to application,.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the request
 * @param[in] val_id        Value Identifier (@see enum cppc_val_id)
 * @param[in] length        Length of data value)
 * @param[in] p_data        Pointer of data value
 ****************************************************************************************
 */
__STATIC void cppc_read_val_cmp(uint8_t conidx, uint16_t status, uint8_t val_id, uint16_t length, const uint8_t* p_data)
{
    cppc_env_t* p_cppc_env = PRF_ENV_GET(CPPC, cppc);

    if(p_cppc_env != NULL)
    {
        const cppc_cb_t* p_cb = (const cppc_cb_t*) p_cppc_env->prf_env.p_cb;
        switch (val_id)
        {
            // Read CP Feature Characteristic value
            case (CPPC_RD_CP_FEAT):
            {
                uint32_t sensor_feat = 0;
                if(status == GAP_ERR_NO_ERROR)
                {
                    sensor_feat = co_btohl(co_read32p(p_data));
                }
                p_cb->cb_read_sensor_feat_cmp(conidx, status, sensor_feat);
            } break;

            // Read Sensor Location Characteristic value
            case (CPPC_RD_SENSOR_LOC):
            {
                uint8_t sensor_loc = 0;

                if(status == GAP_ERR_NO_ERROR)
                {
                    sensor_loc = p_data[0];
                }

                p_cb->cb_read_sensor_loc_cmp(conidx, status, sensor_loc);

            } break;

            // Read Client Characteristic Configuration Descriptor value
            case (CPPC_RD_WR_CP_MEAS_CL_CFG):
            case (CPPC_RD_WR_CP_MEAS_SV_CFG):
            case (CPPC_RD_WR_VECTOR_CFG):
            case (CPPC_RD_WR_CTNL_PT_CFG):
            {
                uint16_t cfg_val = 0;

                if(status == GAP_ERR_NO_ERROR)
                {
                    cfg_val = co_btohs(co_read16p(p_data));
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
 * @param[in] val_id        Value Identifier (@see enum cppc_info)
 ****************************************************************************************
 */
__STATIC uint16_t cppc_read_val(uint8_t conidx, uint16_t val_id)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    cppc_env_t* p_cppc_env = PRF_ENV_GET(CPPC, cppc);

    if(p_cppc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_cppc_env->p_env[conidx] != NULL) && (!p_cppc_env->p_env[conidx]->discover))
        {
            cppc_cnx_env_t* p_con_env = p_cppc_env->p_env[conidx];
            uint16_t hdl;
            cppc_cps_content_t* p_cps = &(p_con_env->cps);

            switch(val_id)
            {
                case CPPC_RD_CP_FEAT:           { hdl = p_cps->chars[CPP_CPS_FEAT_CHAR].val_hdl;         } break;
                case CPPC_RD_SENSOR_LOC:        { hdl = p_cps->chars[CPP_CPS_SENSOR_LOC_CHAR].val_hdl;   } break;
                case CPPC_RD_WR_CP_MEAS_CL_CFG: { hdl = p_cps->descs[CPPC_DESC_CP_MEAS_CL_CFG].desc_hdl; } break;
                case CPPC_RD_WR_CP_MEAS_SV_CFG: { hdl = p_cps->descs[CPPC_DESC_CP_MEAS_SV_CFG].desc_hdl; } break;
                case CPPC_RD_WR_VECTOR_CFG:     { hdl = p_cps->descs[CPPC_DESC_VECTOR_CL_CFG].desc_hdl;  } break;
                case CPPC_RD_WR_CTNL_PT_CFG:    { hdl = p_cps->descs[CPPC_DESC_CTNL_PT_CL_CFG].desc_hdl; } break;
                default:                        { hdl = GATT_INVALID_HDL;                                } break;
            }

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            else
            {
                // perform read request
                status = gatt_cli_read(conidx, p_cppc_env->user_lid, val_id, hdl, 0, 0);
            }
        }
    }

    return (status);
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
__STATIC uint16_t cppc_pack_ctnl_pt_req(co_buf_t* p_buf, uint8_t op_code, const union cpp_ctnl_pt_req_val* p_value)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    // Set the operation code
    co_buf_tail(p_buf)[0] = op_code;
    co_buf_tail_reserve(p_buf, 1);

    // Fulfill the message according to the operation code
    switch (op_code)
    {
        case (CPP_CTNL_PT_SET_CUMUL_VAL):
        {
            // Set the cumulative value
            co_write32p(co_buf_tail(p_buf), co_htobl(p_value->cumul_val));
            co_buf_tail_reserve(p_buf, 4);
        } break;

        case (CPP_CTNL_PT_UPD_SENSOR_LOC):
        {
            // Set the sensor location
            co_buf_tail(p_buf)[0] = p_value->sensor_loc;
            co_buf_tail_reserve(p_buf, 1);
        } break;

        case (CPP_CTNL_PT_SET_CRANK_LENGTH):
        {
            // Set the crank length
            co_write16p(co_buf_tail(p_buf), co_htobs(p_value->crank_length));
            co_buf_tail_reserve(p_buf, 2);
        } break;

        case (CPP_CTNL_PT_SET_CHAIN_LENGTH):
        {
            // Set the chain length
            co_write16p(co_buf_tail(p_buf), co_htobs(p_value->chain_length));
            co_buf_tail_reserve(p_buf, 2);
        } break;

        case (CPP_CTNL_PT_SET_CHAIN_WEIGHT):
        {
            // Set the chain weight
            co_write16p(co_buf_tail(p_buf), co_htobs(p_value->chain_weight));
            co_buf_tail_reserve(p_buf, 2);
        } break;

        case (CPP_CTNL_PT_SET_SPAN_LENGTH):
        {
            // Set the span length
            co_write16p(co_buf_tail(p_buf), co_htobs(p_value->span_length));
            co_buf_tail_reserve(p_buf, 2);
        } break;

        case (CPP_CTNL_MASK_CP_MEAS_CH_CONTENT):
        {
            // Set the Content Mask
            co_write16p(co_buf_tail(p_buf), co_htobs(p_value->mask_content));
            co_buf_tail_reserve(p_buf, 2);
        } break;

        case (CPP_CTNL_PT_REQ_SUPP_SENSOR_LOC):
        case (CPP_CTNL_PT_REQ_CRANK_LENGTH):
        case (CPP_CTNL_PT_REQ_CHAIN_LENGTH):
        case (CPP_CTNL_PT_REQ_CHAIN_WEIGHT):
        case (CPP_CTNL_PT_REQ_SPAN_LENGTH):
        case (CPP_CTNL_PT_START_OFFSET_COMP):
        case (CPP_CTNL_REQ_SAMPLING_RATE):
        case (CPP_CTNL_REQ_FACTORY_CALIBRATION_DATE):
        case (CPP_CTNL_START_ENHANCED_OFFSET_COMP):
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
 * @param[in] p_cppc_env    Environment variable
 * @param[in] conidx        Connection index
 * @param[in] p_buf         Pointer of input buffer received
 ****************************************************************************************
 */
__STATIC void cppc_unpack_meas(cppc_env_t* p_cppc_env, uint8_t conidx, co_buf_t* p_buf)
{
    const cppc_cb_t* p_cb = (const cppc_cb_t*) p_cppc_env->prf_env.p_cb;
    cpp_cp_meas_t meas;
    memset(&meas, 0, sizeof(cpp_cp_meas_t));

    // Flags
    meas.flags = co_btohs(co_read16p(co_buf_data(p_buf)));
    co_buf_head_release(p_buf, 2);
    // Instant power
    meas.inst_power = co_btohs(co_read16p(co_buf_data(p_buf)));
    co_buf_head_release(p_buf, 2);

    if (GETB(meas.flags, CPP_MEAS_PEDAL_POWER_BALANCE_PRESENT))
    {
        // Unpack Pedal Power Balance info
        meas.pedal_power_balance = co_buf_data(p_buf)[0];
        co_buf_head_release(p_buf, 1);
    }

    if (GETB(meas.flags, CPP_MEAS_ACCUM_TORQUE_PRESENT))
    {
        // Unpack Accumulated Torque info
        meas.accum_torque = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    if (GETB(meas.flags, CPP_MEAS_WHEEL_REV_DATA_PRESENT))
    {
        // Unpack Wheel Revolution Data (Cumulative Wheel & Last Wheel Event Time)
        meas.cumul_wheel_rev = co_btohl(co_read32p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 4);
        meas.last_wheel_evt_time = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    if (GETB(meas.flags, CPP_MEAS_CRANK_REV_DATA_PRESENT))
    {
        // Unpack Crank Revolution Data (Cumulative Crank & Last Crank Event Time)
        meas.cumul_crank_rev = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
        meas.last_crank_evt_time = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    if (GETB(meas.flags, CPP_MEAS_EXTREME_FORCE_MAGNITUDES_PRESENT))
    {
        // Unpack Extreme Force Magnitudes (Maximum Force Magnitude & Minimum Force Magnitude)
        meas.max_force_magnitude = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
        meas.min_force_magnitude = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    else if (GETB(meas.flags, CPP_MEAS_EXTREME_TORQUE_MAGNITUDES_PRESENT))
    {
        // Unpack Extreme Force Magnitudes (Maximum Force Magnitude & Minimum Force Magnitude)
        meas.max_torque_magnitude = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
        meas.min_torque_magnitude = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    if (GETB(meas.flags, CPP_MEAS_EXTREME_ANGLES_PRESENT))
    {
        // Unpack Extreme Angles (Maximum Angle & Minimum Angle)
        uint32_t angle = co_btoh24(co_read24p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 3);

        //Force to 12 bits
        meas.max_angle = (angle & (0x0FFF));
        meas.min_angle = ((angle>>12) & 0x0FFF);
    }

    if (GETB(meas.flags, CPP_MEAS_TOP_DEAD_SPOT_ANGLE_PRESENT))
    {
        // Unpack Top Dead Spot Angle
        meas.top_dead_spot_angle = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    if (GETB(meas.flags, CPP_MEAS_BOTTOM_DEAD_SPOT_ANGLE_PRESENT))
    {
        // Unpack Bottom Dead Spot Angle
        meas.bot_dead_spot_angle = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    if (GETB(meas.flags, CPP_MEAS_ACCUM_ENERGY_PRESENT))
    {
        // Unpack Accumulated Energy
        meas.accum_energy = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    // Inform application about received vector
    p_cb->cb_meas(conidx, &meas);
}

/**
 ****************************************************************************************
 * @brief Unpacks Vector data and sends the indication
 * @param[in] p_cppc_env    Environment variable
 * @param[in] conidx        Connection index
 * @param[in] p_buf         Pointer of input buffer received
 ****************************************************************************************
 */
__STATIC void cppc_unpack_vector(cppc_env_t* p_cppc_env, uint8_t conidx, co_buf_t* p_buf)
{
    const cppc_cb_t* p_cb = (const cppc_cb_t*) p_cppc_env->prf_env.p_cb;
    cpp_cp_vector_t vector;
    memset(&vector, 0, sizeof(cpp_cp_vector_t));

    // Flags
    vector.flags = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    if (GETB(vector.flags, CPP_VECTOR_CRANK_REV_DATA_PRESENT))
    {
        // Unpack Crank Revolution Data
        vector.cumul_crank_rev = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
        // Unpack Last Crank Evt time
        vector.last_crank_evt_time = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    if (GETB(vector.flags, CPP_VECTOR_FIRST_CRANK_MEAS_ANGLE_PRESENT))
    {
        // Unpack First Crank Measurement Angle
        vector.first_crank_meas_angle = co_btohs(co_read16p(co_buf_data(p_buf)));
        co_buf_head_release(p_buf, 2);
    }

    if (!GETB(vector.flags, CPP_VECTOR_INST_FORCE_MAGNITUDE_ARRAY_PRESENT) !=
            !GETB(vector.flags, CPP_VECTOR_INST_TORQUE_MAGNITUDE_ARRAY_PRESENT))
    {
        // Unpack Force or Torque magnitude (mutually excluded)

        while((co_buf_data_len(p_buf) >= 2) && (vector.nb < CPP_MAX_TORQUE_NB))
        {
            // Handle the array buffer to extract parameters
            vector.force_torque_magnitude[vector.nb] = co_btohs(co_read16p(co_buf_data(p_buf)));
            co_buf_head_release(p_buf, 2);
            vector.nb += 1;
        }
    }

    // Inform application about received vector
    p_cb->cb_vector(conidx, &vector);
}


/**
 ****************************************************************************************
 * @brief Unpacks Control Point data and sends the indication
 * @param[in] p_cppc_env    Environment variable
 * @param[in] conidx        Connection index
 * @param[in] p_buf         Pointer of input buffer received
 ****************************************************************************************
 */
__STATIC void cppc_unpack_ctln_pt_rsp(cppc_env_t* p_cppc_env, uint8_t conidx, co_buf_t* p_buf)
{
    bool valid = (co_buf_data_len(p_buf) >= CPP_CP_CNTL_PT_RSP_MIN_LEN);

    uint8_t op_code;
    uint8_t req_op_code;
    uint8_t resp_value;
    union cpp_ctnl_pt_rsp_val value;
    memset(&value, 0, sizeof(union cpp_ctnl_pt_rsp_val));

    // Response Op code
    op_code = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Requested operation code
    req_op_code = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    // Response value
    resp_value = co_buf_data(p_buf)[0];
    co_buf_head_release(p_buf, 1);

    if(valid && (op_code == CPP_CTNL_PT_RSP_CODE) && (req_op_code == p_cppc_env->p_env[conidx]->ctrl_pt_op))
    {
        const cppc_cb_t* p_cb = (const cppc_cb_t*) p_cppc_env->prf_env.p_cb;

        if (resp_value == CPP_CTNL_PT_RESP_SUCCESS)
        {
            switch (req_op_code)
            {
                case (CPP_CTNL_PT_REQ_SUPP_SENSOR_LOC):
                {
                   // Location
                   value.supp_sensor_loc = 0;
                   while(co_buf_data_len(p_buf) > 0)
                   {
                       value.supp_sensor_loc |= (1 << co_buf_data(p_buf)[0]);
                       co_buf_head_release(p_buf, 1);
                   }
                } break;

                case (CPP_CTNL_PT_REQ_CRANK_LENGTH):
                {
                    value.crank_length = co_btohs(co_read16p(co_buf_data(p_buf)));
                    co_buf_head_release(p_buf, 2);
                } break;

                case (CPP_CTNL_PT_REQ_CHAIN_LENGTH):
                {
                    value.chain_length = co_btohs(co_read16p(co_buf_data(p_buf)));
                    co_buf_head_release(p_buf, 2);
                } break;

                case (CPP_CTNL_PT_REQ_CHAIN_WEIGHT):
                {
                    value.chain_weight = co_btohs(co_read16p(co_buf_data(p_buf)));
                    co_buf_head_release(p_buf, 2);
                } break;

                case (CPP_CTNL_PT_REQ_SPAN_LENGTH):
                {
                    value.span_length = co_btohs(co_read16p(co_buf_data(p_buf)));
                    co_buf_head_release(p_buf, 2);
                } break;

                case (CPP_CTNL_PT_START_OFFSET_COMP):
                {
                    value.offset_comp = co_btohs(co_read16p(co_buf_data(p_buf)));
                    co_buf_head_release(p_buf, 2);
                } break;

                case (CPP_CTNL_REQ_SAMPLING_RATE):
                {
                    value.sampling_rate = co_buf_data(p_buf)[0];
                    co_buf_head_release(p_buf, 2);
                } break;

                case (CPP_CTNL_REQ_FACTORY_CALIBRATION_DATE):
                {
                    prf_unpack_date_time(p_buf, &(value.factory_calibration));
                } break;

                case (CPP_CTNL_START_ENHANCED_OFFSET_COMP):
                {
                    uint8_t i;
                    value.enhanced_offset_comp.comp_offset = co_btohs(co_read16p(co_buf_data(p_buf)));
                    co_buf_head_release(p_buf, 2);

                    value.enhanced_offset_comp.manu_comp_id = co_btohs(co_read16p(co_buf_data(p_buf)));
                    co_buf_head_release(p_buf, 2);

                    value.enhanced_offset_comp.length = co_min(CPP_MAX_MANF_DATA_LEN, co_buf_data(p_buf)[0]);
                    co_buf_head_release(p_buf, 1);

                    for (i = 0; i < value.enhanced_offset_comp.length; i++)
                    {
                        value.enhanced_offset_comp.data[i] = co_buf_data(p_buf)[0];
                        co_buf_head_release(p_buf, 1);
                    }
                } break;

                case (CPP_CTNL_PT_SET_CUMUL_VAL):
                case (CPP_CTNL_PT_UPD_SENSOR_LOC):
                case (CPP_CTNL_PT_SET_CRANK_LENGTH):
                case (CPP_CTNL_PT_SET_CHAIN_LENGTH):
                case (CPP_CTNL_PT_SET_CHAIN_WEIGHT):
                case (CPP_CTNL_PT_SET_SPAN_LENGTH):
                case (CPP_CTNL_MASK_CP_MEAS_CH_CONTENT):
                {
                    // No parameters
                } break;

                default:
                {

                } break;
            }
        }
        // Response value is not success
        else
        {
            // Start Enhanced Offset Compensation Op code
            if (req_op_code == CPP_CTNL_START_ENHANCED_OFFSET_COMP)
            {
                // obtain response parameter
                value.enhanced_offset_comp.rsp_param = co_buf_data(p_buf)[0];
                co_buf_head_release(p_buf, 1);

                if (value.enhanced_offset_comp.rsp_param == CPP_CTNL_PT_ERR_RSP_PARAM_MANUF_SPEC_ERR_FOLLOWS)
                {
                    uint8_t i;

                    // obtain manufacturer compensation id
                    value.enhanced_offset_comp.manu_comp_id = co_btohs(co_read16p(co_buf_data(p_buf)));
                    co_buf_head_release(p_buf, 2);

                    value.enhanced_offset_comp.length = co_min(CPP_MAX_MANF_DATA_LEN, co_buf_data(p_buf)[0]);
                    co_buf_head_release(p_buf, 1);

                    for (i = 0; i < value.enhanced_offset_comp.length; i++)
                    {
                        value.enhanced_offset_comp.data[i] = co_buf_data(p_buf)[0];
                        co_buf_head_release(p_buf, 1);
                    }
                }
                // else response value is CPP_CTNL_PT_RESP_INV_PARAM
            }
        }

        p_cppc_env->p_env[conidx]->ctrl_pt_op = CPP_CTNL_PT_RESERVED;
        // stop timer
        co_time_timer_stop(&(p_cppc_env->p_env[conidx]->timer));
        // provide control point response
        p_cb->cb_ctnl_pt_req_cmp(conidx, GAP_ERR_NO_ERROR, req_op_code, resp_value, &value);
    }
}

/**
 ****************************************************************************************
 * @brief Function to called once timer expires
 *
 * @param[in] conidx Connection index
 ****************************************************************************************
 */
__STATIC void cppc_timer_handler(uint32_t conidx)
{
    // Get the address of the environment
    cppc_env_t *p_cppc_env = PRF_ENV_GET(CPPC, cppc);

    if (p_cppc_env != NULL)
    {
        cppc_cnx_env_t* p_con_env = p_cppc_env->p_env[conidx];
        ASSERT_ERR(p_con_env != NULL);
        if(p_con_env->ctrl_pt_op != CPP_CTNL_PT_RESERVED)
        {
            const cppc_cb_t* p_cb = (const cppc_cb_t*) p_cppc_env->prf_env.p_cb;
            uint8_t op_code = p_con_env->ctrl_pt_op;
            p_con_env->ctrl_pt_op = CPP_CTNL_PT_RESERVED;

            p_cb->cb_ctnl_pt_req_cmp((uint8_t)conidx, PRF_ERR_PROC_TIMEOUT, op_code, 0, NULL);
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
__STATIC void cppc_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const gatt_svc_att_t* p_atts)
{
    // Get the address of the environment
    cppc_env_t* p_cppc_env = PRF_ENV_GET(CPPC, cppc);

    if(p_cppc_env != NULL)
    {
        cppc_cnx_env_t* p_con_env = p_cppc_env->p_env[conidx];

        ASSERT_INFO(p_con_env != NULL, conidx, user_lid);

        if (p_con_env->nb_svc == 0)
        {
            //Even if we get multiple responses we only store 1 range
            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_START))
            {
                p_con_env->cps.svc.shdl = hdl;
            }

            if((disc_info == GATT_SVC_CMPLT) || (disc_info == GATT_SVC_END))
            {
                p_con_env->cps.svc.ehdl = hdl + nb_att -1;
            }

            // Retrieve characteristics
            prf_extract_svc_info(hdl, nb_att, p_atts,
                                 CPP_CPS_CHAR_MAX, &cppc_cps_char[0],      &(p_con_env->cps.chars[0]),
                                 CPPC_DESC_MAX,    &cppc_cps_char_desc[0], &(p_con_env->cps.descs[0]));
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
__STATIC void cppc_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Get the address of the environment
    cppc_env_t* p_cppc_env = PRF_ENV_GET(CPPC, cppc);

    if(p_cppc_env != NULL)
    {
        cppc_cnx_env_t* p_con_env = p_cppc_env->p_env[conidx];

        if (p_con_env->nb_svc ==  1)
        {
            status = prf_check_svc_validity(CPP_CPS_CHAR_MAX, p_con_env->cps.chars, cppc_cps_char,
                                            CPPC_DESC_MAX, p_con_env->cps.descs, cppc_cps_char_desc);
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

        cppc_enable_cmp(p_cppc_env, conidx, status);
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
__STATIC void cppc_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              co_buf_t* p_data)
{
    cppc_read_val_cmp(conidx, GAP_ERR_NO_ERROR, (uint8_t) dummy, co_buf_data_len(p_data), co_buf_data(p_data));
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
__STATIC void cppc_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != GAP_ERR_NO_ERROR)
    {
        cppc_read_val_cmp(conidx, status, (uint8_t) dummy, 0, NULL);
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
__STATIC void cppc_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    cppc_env_t* p_cppc_env = PRF_ENV_GET(CPPC, cppc);

    if(p_cppc_env != NULL)
    {
        const cppc_cb_t* p_cb = (const cppc_cb_t*) p_cppc_env->prf_env.p_cb;

        switch(dummy)
        {
            // Config control
            case CPPC_RD_WR_CP_MEAS_CL_CFG:
            case CPPC_RD_WR_CP_MEAS_SV_CFG:
            case CPPC_RD_WR_VECTOR_CFG:
            case CPPC_RD_WR_CTNL_PT_CFG:
            {
                p_cb->cb_write_cfg_cmp(conidx, status, dummy);
            } break;
            // Control point commands
            case CPPC_IND_CTNL_PT:
            {
                if(status != GAP_ERR_NO_ERROR)
                {
                    uint8_t opcode = p_cppc_env->p_env[conidx]->ctrl_pt_op;
                    p_cppc_env->p_env[conidx]->ctrl_pt_op = CPP_CTNL_PT_RESERVED;
                    p_cb->cb_ctnl_pt_req_cmp(conidx, status, opcode, 0, NULL);
                }
                else
                {
                    // Start Timeout Procedure - wait for Indication reception
                    co_time_timer_set(&(p_cppc_env->p_env[conidx]->timer), CPP_CP_TIMEOUT);
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
__STATIC void cppc_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                        uint16_t hdl, co_buf_t* p_data)
{
    cppc_env_t* p_cppc_env = PRF_ENV_GET(CPPC, cppc);

    if(p_cppc_env != NULL)
    {
        cppc_cnx_env_t* p_con_env = p_cppc_env->p_env[conidx];
        cppc_cps_content_t* p_cps = &(p_con_env->cps);

        if (hdl == p_cps->chars[CPP_CPS_MEAS_CHAR].val_hdl)
        {
            //Unpack measurement
            cppc_unpack_meas(p_cppc_env, conidx, p_data);
        }
        else if (hdl == p_cps->chars[CPP_CPS_VECTOR_CHAR].val_hdl)
        {
            //Unpack vector
            cppc_unpack_vector(p_cppc_env, conidx, p_data);
        }
        else if (hdl == p_cps->chars[CPP_CPS_CTNL_PT_CHAR].val_hdl)
        {
            // Unpack control point
            cppc_unpack_ctln_pt_rsp(p_cppc_env, conidx, p_data);
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
__STATIC void cppc_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do Nothing
}

/// Client callback hander
__STATIC const gatt_cli_cb_t cppc_cb =
{
    .cb_discover_cmp    = cppc_discover_cmp_cb,
    .cb_read_cmp        = cppc_read_cmp_cb,
    .cb_write_cmp       = cppc_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = cppc_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = cppc_att_val_cb,
    .cb_att_val_evt     = cppc_att_val_evt_cb,
    .cb_svc_changed     = cppc_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t cppc_enable(uint8_t conidx, uint8_t con_type, const cppc_cps_content_t* p_cps)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    cppc_env_t* p_cppc_env = PRF_ENV_GET(CPPC, cppc);

    if(p_cppc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_cppc_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_cppc_env->p_env[conidx] = (struct cppc_cnx_env *) ke_malloc(sizeof(struct cppc_cnx_env), KE_MEM_ATT_DB);

            if(p_cppc_env->p_env[conidx] != NULL)
            {
                memset(p_cppc_env->p_env[conidx], 0, sizeof(struct cppc_cnx_env));
                co_time_timer_init(&(p_cppc_env->p_env[conidx]->timer), (co_time_timer_cb)cppc_timer_handler,
                                   (uint8_t*) ((uint32_t) conidx));

                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = GATT_SVC_CYCLING_POWER;

                    // start discovery
                    status = gatt_cli_discover_svc(conidx, p_cppc_env->user_lid, 0, GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   GATT_MIN_HDL, GATT_MAX_HDL, GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_cppc_env->p_env[conidx]->discover   = true;
                    p_cppc_env->p_env[conidx]->ctrl_pt_op = CPP_CTNL_PT_RESERVED;
                }
                // normal connection, get saved att details
                else
                {
                    memcpy(&(p_cppc_env->p_env[conidx]->cps), p_cps, sizeof(cppc_cps_content_t));
                    status = GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    cppc_enable_cmp(p_cppc_env, conidx, GAP_ERR_NO_ERROR);
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

uint16_t cppc_read_sensor_feat(uint8_t conidx)
{
    uint16_t status = cppc_read_val(conidx, CPPC_RD_CP_FEAT);
    return (status);
}

uint16_t cppc_read_sensor_loc(uint8_t conidx)
{
    uint16_t status = cppc_read_val(conidx, CPPC_RD_SENSOR_LOC);
    return (status);
}

uint16_t cppc_read_cfg(uint8_t conidx, uint8_t desc_code)
{
    uint16_t status;

    switch(desc_code)
    {
        case CPPC_RD_WR_CP_MEAS_CL_CFG:
        case CPPC_RD_WR_CP_MEAS_SV_CFG:
        case CPPC_RD_WR_VECTOR_CFG:
        case CPPC_RD_WR_CTNL_PT_CFG:    { status = cppc_read_val(conidx, desc_code);  } break;
        default:                        { status = PRF_ERR_INEXISTENT_HDL;            } break;
    }

    return (status);
}

uint16_t cppc_write_cfg(uint8_t conidx, uint8_t desc_code, uint16_t cfg_val)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    cppc_env_t* p_cppc_env = PRF_ENV_GET(CPPC, cppc);

    if(p_cppc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_cppc_env->p_env[conidx] != NULL) && (!p_cppc_env->p_env[conidx]->discover))
        {
            cppc_cnx_env_t* p_con_env = p_cppc_env->p_env[conidx];
            uint16_t hdl;
            uint16_t cfg_en_val = 0;
            cppc_cps_content_t* p_cps = &(p_con_env->cps);

            switch(desc_code)
            {
                case CPPC_RD_WR_CP_MEAS_CL_CFG: { hdl        = p_cps->descs[CPPC_DESC_CP_MEAS_CL_CFG].desc_hdl;
                                                  cfg_en_val =  PRF_CLI_START_NTF;                              } break;
                case CPPC_RD_WR_CP_MEAS_SV_CFG: { hdl        = p_cps->descs[CPPC_DESC_CP_MEAS_SV_CFG].desc_hdl;
                                                  cfg_en_val =  PRF_SRV_START_BCST;                             } break;
                case CPPC_RD_WR_VECTOR_CFG:     { hdl        = p_cps->descs[CPPC_DESC_VECTOR_CL_CFG].desc_hdl;
                                                  cfg_en_val =  PRF_CLI_START_NTF;                              } break;
                case CPPC_RD_WR_CTNL_PT_CFG:    { hdl        = p_cps->descs[CPPC_DESC_CTNL_PT_CL_CFG].desc_hdl;
                                                  cfg_en_val =  PRF_CLI_START_IND;                              } break;
                default:                        { hdl = GATT_INVALID_HDL;                                       } break;
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
                status = prf_gatt_write(conidx, p_cppc_env->user_lid, desc_code, GATT_WRITE,
                                        hdl, sizeof(uint16_t), (uint8_t *)&cfg_val);
            }
        }
    }

    return (status);
}

uint16_t cppc_ctnl_pt_req(uint8_t conidx, uint8_t req_op_code, const union cpp_ctnl_pt_req_val* p_value)
{
    uint16_t status = PRF_ERR_REQ_DISALLOWED;
    // Client environment
    cppc_env_t* p_cppc_env = PRF_ENV_GET(CPPC, cppc);

    if(p_value == NULL)
    {
        status = PRF_ERR_INVALID_PARAM;
    }
    else if(p_cppc_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_cppc_env->p_env[conidx] != NULL) && (!p_cppc_env->p_env[conidx]->discover))
        {
            cppc_cnx_env_t* p_con_env = p_cppc_env->p_env[conidx];
            cppc_cps_content_t* p_cps = &(p_con_env->cps);
            uint16_t hdl = p_cps->chars[CPP_CPS_CTNL_PT_CHAR].val_hdl;

            if(hdl == GATT_INVALID_HDL)
            {
                status = PRF_ERR_INEXISTENT_HDL;
            }
            // reject if there is an ongoing control point operation
            else if(p_con_env->ctrl_pt_op != CPP_CTNL_PT_RESERVED)
            {
                status = PRF_ERR_REQ_DISALLOWED;
            }
            else
            {
                co_buf_t* p_buf = NULL;

                // allocate buffer for event transmission
                if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, CPP_CP_CNTL_PT_REQ_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
                {
                    status = cppc_pack_ctnl_pt_req(p_buf, req_op_code, p_value);

                    if(status == GAP_ERR_NO_ERROR)
                    {
                        status = gatt_cli_write(conidx, p_cppc_env->user_lid, CPPC_IND_CTNL_PT, GATT_WRITE, hdl, 0, p_buf);
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
 * @brief Send a CPPC_CMP_EVT message to the task which enabled the profile
 * @param[in] conidx Connection index
 * @param[in] operation Operation
 * @param[in] status Satus
 ****************************************************************************************
 */
__STATIC void cppc_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    struct cppc_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(CPPC_CMP_EVT, PRF_DST_TASK(CPPC), PRF_SRC_TASK(CPPC), cppc_cmp_evt);
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
__STATIC int cppc_enable_req_handler(ke_msg_id_t const msgid, struct cppc_enable_req const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = cppc_enable(p_param->conidx, p_param->con_type, &(p_param->cps));

    // send an error if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct cppc_enable_rsp *p_rsp = KE_MSG_ALLOC(CPPC_ENABLE_RSP, src_id, dest_id, cppc_enable_rsp);
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
__STATIC int cppc_read_cmd_handler(ke_msg_id_t const msgid, struct cppc_read_cmd const *p_param,
                                   ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status;

    switch(p_param->read_code)
    {
        case CPPC_RD_CP_FEAT:           { status = cppc_read_sensor_feat(p_param->conidx);              } break;
        case CPPC_RD_SENSOR_LOC:        { status = cppc_read_sensor_loc(p_param->conidx);               } break;
        case CPPC_RD_WR_CP_MEAS_CL_CFG:
        case CPPC_RD_WR_CP_MEAS_SV_CFG:
        case CPPC_RD_WR_VECTOR_CFG:
        case CPPC_RD_WR_CTNL_PT_CFG:    { status = cppc_read_cfg(p_param->conidx, p_param->read_code);  } break;
        default:                        { status = PRF_ERR_INEXISTENT_HDL;                              } break;
    }

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct cppc_cmp_evt *p_evt = KE_MSG_ALLOC(CPPC_CMP_EVT, src_id, dest_id, cppc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = CPPC_READ_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref CPPC_CFG_NTFIND_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int cppc_cfg_ntfind_cmd_handler(ke_msg_id_t const msgid,
                                        struct cppc_cfg_ntfind_cmd *p_param,
                                        ke_task_id_t const dest_id,
                                        ke_task_id_t const src_id)
{
    uint16_t status = cppc_write_cfg(p_param->conidx, p_param->desc_code, p_param->ntfind_cfg);

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct cppc_cmp_evt *p_evt = KE_MSG_ALLOC(CPPC_CMP_EVT, src_id, dest_id, cppc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = CPPC_CFG_NTF_IND_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief Handles reception of the @ref CPPC_CTNL_PT_CFG_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int cppc_ctnl_pt_cfg_cmd_handler(ke_msg_id_t const msgid, struct cppc_ctnl_pt_cfg_cmd *p_param,
                                          ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = cppc_ctnl_pt_req(p_param->conidx, p_param->op_code, &(p_param->value));

    // send error response if request fails
    if (status != GAP_ERR_NO_ERROR)
    {
        struct cppc_cmp_evt *p_evt = KE_MSG_ALLOC(CPPC_CMP_EVT, src_id, dest_id, cppc_cmp_evt);
        if(p_evt)
        {
            p_evt->conidx     = p_param->conidx;
            p_evt->operation  = CPPC_CTNL_PT_CFG_WR_OP_CODE;
            p_evt->status     = status;

            ke_msg_send(p_evt);
        }
    }

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(cppc)
{
    // Note: all messages must be sorted in ID ascending order

    { CPPC_ENABLE_REQ,               (ke_msg_func_t) cppc_enable_req_handler         },
    { CPPC_READ_CMD,                 (ke_msg_func_t) cppc_read_cmd_handler           },
    { CPPC_CFG_NTFIND_CMD,           (ke_msg_func_t) cppc_cfg_ntfind_cmd_handler     },
    { CPPC_CTNL_PT_CFG_CMD,          (ke_msg_func_t) cppc_ctnl_pt_cfg_cmd_handler    },
};


/**
 ****************************************************************************************
 * @brief Completion of enable procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] p_cps         Pointer to peer database description bond data
 ****************************************************************************************
 */
__STATIC void cppc_cb_enable_cmp(uint8_t conidx, uint16_t status, const cppc_cps_content_t* p_cps)
{
    // Send APP the details of the discovered attributes on CPPC
    struct cppc_enable_rsp *p_rsp = KE_MSG_ALLOC(CPPC_ENABLE_RSP, PRF_DST_TASK(CPPC), PRF_SRC_TASK(CPPC),
                                                 cppc_enable_rsp);
    if(p_rsp != NULL)
    {
        p_rsp->conidx = conidx;
        p_rsp->status = status;
        memcpy(&(p_rsp->cps), p_cps, sizeof(cppc_cps_content_t));
        ke_msg_send(p_rsp);
    }
}

/**
 ****************************************************************************************
 * @brief Completion of read sensor feature procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] sensor_feat   CP sensor feature
 *
 ****************************************************************************************
 */
__STATIC void cppc_cb_read_sensor_feat_cmp(uint8_t conidx, uint16_t status, uint32_t sensor_feat)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(CPPC);
    ke_task_id_t dest_id = PRF_DST_TASK(CPPC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct cppc_value_ind *p_ind = KE_MSG_ALLOC(CPPC_VALUE_IND, dest_id, src_id, cppc_value_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx            = conidx;
            p_ind->att_code          = CPPC_RD_CP_FEAT;
            p_ind->value.sensor_feat = sensor_feat;
            ke_msg_send(p_ind);
        }
    }

    cppc_send_cmp_evt(conidx, CPPC_READ_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of read sensor location procedure.
 *
 * Wait for @see cb_read_loc_cmp execution before starting a new procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] sensor_loc    Sensor Location
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void cppc_cb_read_sensor_loc_cmp(uint8_t conidx, uint16_t status, uint8_t sensor_loc)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(CPPC);
    ke_task_id_t dest_id = PRF_DST_TASK(CPPC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct cppc_value_ind *p_ind = KE_MSG_ALLOC(CPPC_VALUE_IND, dest_id, src_id, cppc_value_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx            = conidx;
            p_ind->att_code          = CPPC_RD_SENSOR_LOC;
            p_ind->value.sensor_loc  = sensor_loc;
            ke_msg_send(p_ind);
        }
    }

    cppc_send_cmp_evt(conidx, CPPC_READ_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of read Characteristic Configuration procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] desc_code     Descriptor Code (@see enum cppc_code)
 *                              - CPPC_RD_WR_CP_MEAS_CL_CFG: CP Measurement Client Char. Configuration
 *                              - CPPC_RD_WR_CP_MEAS_SV_CFG: CP Measurement Server Char. Configuration
 *                              - CPPC_RD_WR_VECTOR_CFG:     Vector Client Char. Configuration
 *                              - CPPC_RD_WR_CTNL_PT_CFG:    CP Control Point Client Char. Configuration
 * @param[in] cfg_val       Configuration value
 *
 * @return Status of the function execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void cppc_cb_read_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t desc_code, uint16_t cfg_val)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(CPPC);
    ke_task_id_t dest_id = PRF_DST_TASK(CPPC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct cppc_value_ind *p_ind = KE_MSG_ALLOC(CPPC_VALUE_IND, dest_id, src_id, cppc_value_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx            = conidx;
            p_ind->att_code          = desc_code;
            p_ind->value.ntf_cfg     = cfg_val;
            ke_msg_send(p_ind);
        }
    }

    cppc_send_cmp_evt(conidx, CPPC_READ_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of write Characteristic Configuration procedure.
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the procedure execution (@see enum hl_err)
 * @param[in] desc_code     Descriptor Code (@see enum cppc_code)
 *                              - CPPC_RD_WR_CP_MEAS_CL_CFG: CP Measurement Client Char. Configuration
 *                              - CPPC_RD_WR_CP_MEAS_SV_CFG: CP Measurement Server Char. Configuration
 *                              - CPPC_RD_WR_VECTOR_CFG:     Vector Client Char. Configuration
 *                              - CPPC_RD_WR_CTNL_PT_CFG:    CP Control Point Client Char. Configuration
 *
 ****************************************************************************************
 */
__STATIC void cppc_cb_write_cfg_cmp(uint8_t conidx, uint16_t status, uint8_t desc_code)
{
    cppc_send_cmp_evt(conidx, CPPC_CFG_NTF_IND_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Function called when CP measurement information is received
 *
 * @param[in] conidx         Connection index
 * @param[in] p_meas         Pointer to CP measurement information
 ****************************************************************************************
 */
__STATIC void cppc_cb_meas(uint8_t conidx, const cpp_cp_meas_t* p_meas)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(CPPC);
    ke_task_id_t dest_id = PRF_DST_TASK(CPPC);

    struct cppc_value_ind *p_ind = KE_MSG_ALLOC(CPPC_VALUE_IND, dest_id, src_id, cppc_value_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx            = conidx;
        p_ind->att_code          = CPPC_NTF_CP_MEAS;
        memcpy(&(p_ind->value.cp_meas), p_meas, sizeof(cpp_cp_meas_t));
        ke_msg_send(p_ind);
    }
}

/**
 ****************************************************************************************
 * @brief Function called when CP vector information is received
 *
 * @param[in] conidx         Connection index
 * @param[in] p_vector       Pointer to CP vector information
 ****************************************************************************************
 */
__STATIC void cppc_cb_vector(uint8_t conidx, const cpp_cp_vector_t* p_vector)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(CPPC);
    ke_task_id_t dest_id = PRF_DST_TASK(CPPC);

    struct cppc_value_ind *p_ind = KE_MSG_ALLOC(CPPC_VALUE_IND, dest_id, src_id, cppc_value_ind);
    if(p_ind != NULL)
    {
        p_ind->conidx            = conidx;
        p_ind->att_code          = CPPC_NTF_CP_VECTOR;
        memcpy(&(p_ind->value.cp_vector), p_vector, sizeof(cpp_cp_vector_t));
        ke_msg_send(p_ind);
    }
}


/**
 ****************************************************************************************
 * @brief Completion of control point request procedure
 *
 * @param[in] conidx        Connection index
 * @param[in] status        Status of the Request Send (@see enum hl_err)
 * @param[in] req_op_code   Requested Operation Code @see enum cpp_ctnl_pt_code
 * @param[in] resp_value    Response Value @see enum cpp_ctnl_pt_resp_val
 * @param[in] p_value       Pointer to response data content
 ****************************************************************************************
 */
__STATIC void cppc_cb_ctnl_pt_req_cmp(uint8_t conidx, uint16_t status, uint8_t req_op_code, uint8_t resp_value,
        const union cpp_ctnl_pt_rsp_val* p_value)
{
    ke_task_id_t src_id  = PRF_SRC_TASK(CPPC);
    ke_task_id_t dest_id = PRF_DST_TASK(CPPC);

    if(status == GAP_ERR_NO_ERROR)
    {
        struct cppc_ctnl_pt_ind *p_ind = KE_MSG_ALLOC(CPPC_CTNL_PT_IND, dest_id, src_id, cppc_ctnl_pt_ind);
        if(p_ind != NULL)
        {
            p_ind->conidx            = conidx;
            p_ind->op_code           = CPP_CTNL_PT_RSP_CODE;
            p_ind->req_op_code       = req_op_code;
            p_ind->resp_value        = resp_value;
            memcpy(&(p_ind->value),    p_value, sizeof(union cpp_ctnl_pt_rsp_val));;
            ke_msg_send(p_ind);
        }
    }

    cppc_send_cmp_evt(conidx, CPPC_CTNL_PT_CFG_WR_OP_CODE, status);
}

/// Default Message handle
__STATIC const cppc_cb_t cppc_msg_cb =
{
     .cb_enable_cmp           = cppc_cb_enable_cmp,
     .cb_read_sensor_feat_cmp = cppc_cb_read_sensor_feat_cmp,
     .cb_read_sensor_loc_cmp  = cppc_cb_read_sensor_loc_cmp,
     .cb_read_cfg_cmp         = cppc_cb_read_cfg_cmp,
     .cb_write_cfg_cmp        = cppc_cb_write_cfg_cmp,
     .cb_meas                 = cppc_cb_meas,
     .cb_vector               = cppc_cb_vector,
     .cb_ctnl_pt_req_cmp      = cppc_cb_ctnl_pt_req_cmp,
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
__STATIC uint16_t cppc_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const cppc_cb_t* p_cb)
{
    uint8_t conidx;
    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        cppc_env_t* p_cppc_env;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(cppc_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL) || (p_cb->cb_read_sensor_feat_cmp == NULL)
           || (p_cb->cb_read_sensor_loc_cmp == NULL) || (p_cb->cb_read_cfg_cmp == NULL) || (p_cb->cb_write_cfg_cmp == NULL)
           || (p_cb->cb_meas == NULL) || (p_cb->cb_vector == NULL) || (p_cb->cb_ctnl_pt_req_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register CPPC user
        status = gatt_user_cli_register(CPP_CP_MEAS_MAX_LEN + 3, user_prio, &cppc_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_cppc_env = (cppc_env_t*) ke_malloc(sizeof(cppc_env_t), KE_MEM_ATT_DB);

        if(p_cppc_env != NULL)
        {
            // allocate CPPC required environment variable
            p_env->p_env = (prf_hdr_t *) p_cppc_env;

            // initialize environment variable
            p_cppc_env->prf_env.p_cb    = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab = cppc_msg_handler_tab;
            p_env->desc.msg_cnt         = ARRAY_LEN(cppc_msg_handler_tab);
            #endif // (BLE_HL_MSG_API)

            p_cppc_env->user_lid = user_lid;
            for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
            {
                p_cppc_env->p_env[conidx] = NULL;
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
__STATIC uint16_t cppc_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    cppc_env_t* p_cppc_env = (cppc_env_t*) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_cppc_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        uint8_t conidx;

        // cleanup environment variable for each task instances
        for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
        {
            if (p_cppc_env->p_env[conidx] != NULL)
            {
                if(reason != PRF_DESTROY_RESET)
                {
                    co_time_timer_stop(&(p_cppc_env->p_env[conidx]->timer));
                }

                ke_free(p_cppc_env->p_env[conidx]);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_cppc_env);
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
__STATIC void cppc_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void cppc_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    cppc_env_t* p_cppc_env = (cppc_env_t*) p_env->p_env;

    // clean-up environment variable allocated for task instance
    if (p_cppc_env->p_env[conidx] != NULL)
    {
        co_time_timer_stop(&(p_cppc_env->p_env[conidx]->timer));
        ke_free(p_cppc_env->p_env[conidx]);
        p_cppc_env->p_env[conidx] = NULL;
    }
}

/// CPPC Task interface required by profile manager
const prf_task_cbs_t cppc_itf =
{
    .cb_init          = (prf_init_cb) cppc_init,
    .cb_destroy       = cppc_destroy,
    .cb_con_create    = cppc_con_create,
    .cb_con_cleanup   = cppc_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* cppc_prf_itf_get(void)
{
    return &cppc_itf;
}

#endif //(BLE_CP_COLLECTOR)

/// @} CPP
