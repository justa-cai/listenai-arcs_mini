/**
 ****************************************************************************************
 *
 * @file cpps.c
 *
 * @brief Cycling Power Profile Sensor implementation.
 *
 * Copyright (C) ListenAI 2020-2099
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup CPPS
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */


#include "btip_config.h"
#if (BLE_CP_SENSOR)
#include "cpps.h"
#include "gap.h"
#include "gatt.h"

#include "co_utils.h"
#include "co_endian.h"

#include <string.h>
#include "ke_mem.h"


/*
 * MACROS
 ****************************************************************************************
 */

/// Check if a specific characteristic has been added in the database (according to mask)
#define CPPS_IS_FEATURE_SUPPORTED(features, flag) ((features & flag) == flag)

/// MACRO TO CALCULATE HANDLE    shdl + idx - BCST - VECTOR
/// BCST is 1 if the broadcast mode is supported otherwise 0
/// VECTOR is 3 if the Vector characteristic is supported otherwise 0
#define CPPS_HANDLE(idx) \
    (p_cpps_env->start_hdl + (idx) - \
        ((!(CPPS_IS_FEATURE_SUPPORTED(p_cpps_env->prfl_cfg, CPPS_MEAS_BCST_MASK)) && \
                ((idx) > CPS_IDX_CP_MEAS_BCST_CFG))? (1) : (0)) - \
        ((!(CPPS_IS_FEATURE_SUPPORTED(p_cpps_env->prfl_cfg, CPPS_VECTOR_MASK)) && \
                ((idx) > CPS_IDX_VECTOR_CHAR))? (3) : (0)))


/*
 * DEFINES
 ****************************************************************************************
 */


/********************************************
 ******* CPPS Configuration Flag Masks ******
 ********************************************/

/// Mandatory Attributes (CP Measurement + CP Feature + CP Sensor Location)
#define CPPS_MANDATORY_MASK           (0x01EF)
/// Broadcast Attribute
#define CPPS_MEAS_BCST_MASK           (0x0010)
/// Vector Attributes
#define CPPS_VECTOR_MASK              (0x0E00)
/// Control Point Attributes
#define CPPS_CTNL_PT_MASK             (0x7000)

/// Broadcast supported flag
#define CPPS_BROADCASTER_SUPP_FLAG    (0x01)
/// Control Point supported flag
#define CPPS_CTNL_PT_CHAR_SUPP_FLAG    (0x02)

/// Cycling Power Service - Attribute List
enum cpps_cps_att_list
{
    /// Cycling Power Service
    CPS_IDX_SVC,
    /// CP Measurement
    CPS_IDX_CP_MEAS_CHAR,
    CPS_IDX_CP_MEAS_VAL,
    CPS_IDX_CP_MEAS_NTF_CFG,
    CPS_IDX_CP_MEAS_BCST_CFG,
    /// CP Feature
    CPS_IDX_CP_FEAT_CHAR,
    CPS_IDX_CP_FEAT_VAL,
    /// Sensor Location
    CPS_IDX_SENSOR_LOC_CHAR,
    CPS_IDX_SENSOR_LOC_VAL,
    /// CP Vector
    CPS_IDX_VECTOR_CHAR,
    CPS_IDX_VECTOR_VAL,
    CPS_IDX_VECTOR_NTF_CFG,
    /// CP Control Point
    CPS_IDX_CTNL_PT_CHAR,
    CPS_IDX_CTNL_PT_VAL,
    CPS_IDX_CTNL_PT_IND_CFG,

    /// Number of attributes
    CPS_IDX_NB,
};


/// Profile Configuration Additional Flags ()
enum cpps_prf_cfg_flag_bf
{
    /// CP Measurement - Client Char. Cfg
    CPP_PRF_CFG_FLAG_CP_MEAS_NTF_POS        = 0,
    CPP_PRF_CFG_FLAG_CP_MEAS_NTF_BIT        = CO_BIT(CPP_PRF_CFG_FLAG_CP_MEAS_NTF_POS),

    /// CP Measurement - Server Char. Cfg
    CPP_PRF_CFG_FLAG_SP_MEAS_NTF_POS        = 1,
    CPP_PRF_CFG_FLAG_SP_MEAS_NTF_BIT        = CO_BIT(CPP_PRF_CFG_FLAG_SP_MEAS_NTF_POS),

    /// CP Vector - Client Characteristic configuration
    CPP_PRF_CFG_FLAG_VECTOR_NTF_POS         = 2,
    CPP_PRF_CFG_FLAG_VECTOR_NTF_BIT         = CO_BIT(CPP_PRF_CFG_FLAG_VECTOR_NTF_POS),

    /// Control Point - Client Characteristic configuration
    CPP_PRF_CFG_FLAG_CTNL_PT_IND_POS        = 3,
    CPP_PRF_CFG_FLAG_CTNL_PT_IND_BIT        = CO_BIT(CPP_PRF_CFG_FLAG_CTNL_PT_IND_POS),

    /// Bonded data used
    CPP_PRF_CFG_PERFORMED_OK_POS            = 4,
    CPP_PRF_CFG_PERFORMED_OK_BIT            = CO_BIT(CPP_PRF_CFG_PERFORMED_OK_POS),
};

/*
 * TYPES DEFINITION
 ****************************************************************************************
 */

/// ongoing operation information
typedef struct cpps_buf_meta
{
    /// meaningful for some operation
    uint32_t  conidx_bf;
    /// Operation
    uint8_t   operation;
    /// used to know on which device interval update has been requested, and to prevent
    /// indication to be triggered on this connection index
    uint8_t   conidx;
    /// last transmitted information flag
    uint16_t  last_flag;
    /// True if measurement must be sent onto a new connection.
    bool      new;
} cpps_buf_meta_t;


/// Cycling Power Profile Sensor environment variable per connection
typedef struct cpps_cnx_env
{
    /// Measurement content mask
    uint16_t mask_meas_content;
    /// Profile Notify/Indication Flags
    uint8_t  prfl_ntf_ind_cfg;
} cpps_cnx_env_t;

/// CPPS Environment Variable
typedef struct cpps_env
{
    /// profile environment
    prf_hdr_t      prf_env;
    /// Operation Event TX wait queue
    co_list_t      wait_queue;
    /// Environment variable pointer for each connections
    cpps_cnx_env_t env[BLE_CONNECTION_MAX];
    /// Instantaneous Power
    uint32_t       inst_power;
    /// Cumulative Value
    uint32_t       cumul_wheel_rev;
    /// Feature Configuration Flags
    uint32_t       features;
    /// Service Attribute Start Handle
    uint16_t       start_hdl;
    /// Profile Configuration Flags
    uint16_t       prfl_cfg;
    /// Sensor Location
    uint8_t        sensor_loc;
    /// GATT user local identifier
    uint8_t        user_lid;
    /// Control point operation on-going (@see enum cpp_ctnl_pt_code)
    uint8_t        ctrl_pt_op;
    /// Operation On-going
    bool           op_ongoing;
    /// Prevent recursion in execute_operation function
    bool           in_exe_op;

} cpps_env_t;


/*
 * ATTRIBUTES DATABASE
 ****************************************************************************************
 */

/// Full CPPS Database Description - Used to add attributes into the database
__STATIC const gatt_att16_desc_t cpps_att_db[CPS_IDX_NB] =
{
    // Cycling Power Service Declaration
    [CPS_IDX_SVC]                =   { GATT_DECL_PRIMARY_SERVICE,   PROP(RD),            0                                           },

    // CP Measurement Characteristic Declaration
    [CPS_IDX_CP_MEAS_CHAR]       =   { GATT_DECL_CHARACTERISTIC,    PROP(RD),            0                                           },
    // CP Measurement Characteristic Value
    [CPS_IDX_CP_MEAS_VAL]        =   { GATT_CHAR_CP_MEAS,           PROP(N),             CPP_CP_MEAS_MAX_LEN                         },
    // CP Measurement Characteristic - Client Characteristic Configuration Descriptor
    [CPS_IDX_CP_MEAS_NTF_CFG]    =   { GATT_DESC_CLIENT_CHAR_CFG,   PROP(RD) | PROP(WR), OPT(NO_OFFSET)                              },
    // CP Measurement Characteristic - Server Characteristic Configuration Descriptor
    [CPS_IDX_CP_MEAS_BCST_CFG]   =   { GATT_DESC_SERVER_CHAR_CFG,   PROP(RD) | PROP(WR), OPT(NO_OFFSET)                              },

    // CP Feature Characteristic Declaration
    [CPS_IDX_CP_FEAT_CHAR]       =   { GATT_DECL_CHARACTERISTIC,    PROP(RD),            0                                           },
    // CP Feature Characteristic Value
    [CPS_IDX_CP_FEAT_VAL]        =   { GATT_CHAR_CP_FEAT,           PROP(RD),            OPT(NO_OFFSET)                              },

    // Sensor Location Characteristic Declaration
    [CPS_IDX_SENSOR_LOC_CHAR]    =   { GATT_DECL_CHARACTERISTIC,    PROP(RD),            0                                           },
    // Sensor Location Characteristic Value
    [CPS_IDX_SENSOR_LOC_VAL]     =   { GATT_CHAR_SENSOR_LOC,        PROP(RD),            OPT(NO_OFFSET)                              },

    // CP Vector Characteristic Declaration
    [CPS_IDX_VECTOR_CHAR]        =   { GATT_DECL_CHARACTERISTIC,    PROP(RD),            0                                           },
    // CP Vector Characteristic Value
    [CPS_IDX_VECTOR_VAL]         =   { GATT_CHAR_CP_VECTOR,         PROP(N),             CPP_CP_VECTOR_MAX_LEN                       },
    // CP Vector Characteristic - Client Characteristic Configuration Descriptor
    [CPS_IDX_VECTOR_NTF_CFG]     =   { GATT_DESC_CLIENT_CHAR_CFG,   PROP(RD) | PROP(WR), OPT(NO_OFFSET)                              },

    // CP Control Point Characteristic Declaration
    [CPS_IDX_CTNL_PT_CHAR]       =   { GATT_DECL_CHARACTERISTIC,    PROP(RD),            0                                           },
    // CP Control Point Characteristic Value - The response has the maximal length
    [CPS_IDX_CTNL_PT_VAL]        =   { GATT_CHAR_CP_CNTL_PT,        PROP(I) | PROP(WR),  OPT(NO_OFFSET) | CPP_CP_CNTL_PT_REQ_MAX_LEN },
    // CP Control Point Characteristic - Client Characteristic Configuration Descriptor
    [CPS_IDX_CTNL_PT_IND_CFG]    =   { GATT_DESC_CLIENT_CHAR_CFG,   PROP(RD) | PROP(WR), OPT(NO_OFFSET)                              },
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
__STATIC void cpps_exe_operation(cpps_env_t* p_cpps_env);


/// Get database attribute index
__STATIC uint8_t cpps_idx_get(cpps_env_t* p_cpps_env, uint16_t hdl)
{
    uint8_t att_idx = hdl - p_cpps_env->start_hdl;

    if(   (att_idx >= CPS_IDX_CP_MEAS_BCST_CFG)
       && !CPPS_IS_FEATURE_SUPPORTED(p_cpps_env->prfl_cfg, CPPS_MEAS_BCST_MASK))
    {
        att_idx += 1;
    }

    if(   (att_idx >= CPS_IDX_VECTOR_CHAR)
       && !CPPS_IS_FEATURE_SUPPORTED(p_cpps_env->prfl_cfg, CPPS_VECTOR_MASK))
    {
        att_idx += 3;
    }

    return att_idx;
}


/**
 ****************************************************************************************
 * @brief Packs measurement data
 *
 * @param[in]     p_cpps_env   Environment data
 * @param[in]     p_buf        Pointer to output buffer
 * @param[in]     p_meas       pointer to measurement information
 * @param[in]     max_size     Maximum PDU size that can be transmitted
 * @param[in]     mask_content Measurement content that should be masked
 * @param[in|out] p_last_flags Pointer to last flags set notified (if split)
 *
 * @return True if all packing succeed, false if packet split.
 ****************************************************************************************
 */
__STATIC bool cpps_pack_meas(cpps_env_t *p_cpps_env, co_buf_t* p_buf, const cpp_cp_meas_t *p_meas,
                             uint16_t max_size, uint16_t mask_content, uint16_t* p_last_flags)
{
    uint16_t meas_flags = p_meas->flags;
    uint16_t pkt_flag;
    bool complete = false;

    ASSERT_ERR(max_size >= (L2CAP_LE_MTU_MIN - GATT_NTF_HEADER_LEN));

    // Mask unwanted fields if supported
    if (GETB(p_cpps_env->features, CPP_FEAT_CP_MEAS_CH_CONTENT_MASKING_SUP))
    {
        meas_flags &= ~mask_content;
    }

    // keep mandatory flags
    pkt_flag = (meas_flags & (  CPP_MEAS_PEDAL_POWER_BALANCE_REFERENCE_BIT | CPP_MEAS_ACCUM_TORQUE_SOURCE_BIT
                              | CPP_MEAS_OFFSET_COMPENSATION_INDICATOR_BIT));

    // remove flags already transmitted
    meas_flags &= ~(*p_last_flags);

    do
    {
        // prepare 4 bytes for header
        co_buf_tail_reserve(p_buf, CPP_CP_MEAS_NTF_MIN_LEN);
        co_buf_head_release(p_buf, CPP_CP_MEAS_NTF_MIN_LEN);
        max_size -= 4;

        // Check provided flags
        if (GETB(meas_flags, CPP_MEAS_PEDAL_POWER_BALANCE_PRESENT)) // 5
        {
            if (GETB(p_cpps_env->features, CPP_FEAT_PEDAL_POWER_BALANCE_SUP))
            {
                if(max_size < 1) break; // Stop packing
                // Pack Pedal Power Balance info
                SETB(pkt_flag, CPP_MEAS_PEDAL_POWER_BALANCE_PRESENT, 1);
                co_buf_tail(p_buf)[0] = p_meas->pedal_power_balance;
                co_buf_tail_reserve(p_buf, 1);
                max_size -= 1;
            }
        }

        if (GETB(meas_flags, CPP_MEAS_ACCUM_TORQUE_PRESENT)) // 7
        {
            if (GETB(p_cpps_env->features, CPP_FEAT_ACCUM_TORQUE_SUP))
            {
                if(max_size < 2) break; // Stop packing
                // Pack Accumulated Torque info
                SETB(pkt_flag, CPP_MEAS_ACCUM_TORQUE_PRESENT, 1);
                co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->accum_torque));
                co_buf_tail_reserve(p_buf, 2);
                max_size -= 2;
            }
        }

        if (GETB(meas_flags, CPP_MEAS_WHEEL_REV_DATA_PRESENT)) // 13
        {
            if (GETB(p_cpps_env->features, CPP_FEAT_WHEEL_REV_DATA_SUP))
            {
                if(max_size < 6) break; // Stop packing
                // Pack Wheel Revolution Data (Cumulative Wheel & Last Wheel Event Time)
                SETB(pkt_flag, CPP_MEAS_WHEEL_REV_DATA_PRESENT, 1);
                co_write32p(co_buf_tail(p_buf), co_htobl(p_cpps_env->cumul_wheel_rev));
                co_buf_tail_reserve(p_buf, 4);
                co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->last_wheel_evt_time));
                co_buf_tail_reserve(p_buf, 2);
                max_size -= 6;
            }
        }

        if (GETB(meas_flags, CPP_MEAS_CRANK_REV_DATA_PRESENT)) // 17
        {
            if (GETB(p_cpps_env->features, CPP_FEAT_CRANK_REV_DATA_SUP))
            {
                if(max_size < 4) break; // Stop packing
                // Pack Crank Revolution Data (Cumulative Crank & Last Crank Event Time)
                SETB(pkt_flag, CPP_MEAS_CRANK_REV_DATA_PRESENT, 1);
                co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->cumul_crank_rev));
                co_buf_tail_reserve(p_buf, 2);
                co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->last_crank_evt_time));
                co_buf_tail_reserve(p_buf, 2);
                max_size -= 4;
            }
        }

        if (GETB(meas_flags, CPP_MEAS_EXTREME_FORCE_MAGNITUDES_PRESENT)) // 21 - Greater than Min packet
        {
            if (GETB(p_cpps_env->features, CPP_FEAT_EXTREME_MAGNITUDES_SUP)
                    && (!GETB(p_cpps_env->features, CPP_FEAT_SENSOR_MEAS_CONTEXT)))
            {
                if(max_size < 4) break; // Stop packing

                // Pack Extreme Force Magnitudes (Maximum Force Magnitude & Minimum Force Magnitude)
                SETB(pkt_flag, CPP_MEAS_EXTREME_FORCE_MAGNITUDES_PRESENT, 1);
                co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->max_force_magnitude));
                co_buf_tail_reserve(p_buf, 2);
                co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->min_force_magnitude));
                co_buf_tail_reserve(p_buf, 2);
                max_size -= 4;
            }
        }

        if (GETB(meas_flags, CPP_MEAS_EXTREME_TORQUE_MAGNITUDES_PRESENT)) // 25
        {
            if (GETB(p_cpps_env->features, CPP_FEAT_EXTREME_MAGNITUDES_SUP)
                    && GETB(p_cpps_env->features, CPP_FEAT_SENSOR_MEAS_CONTEXT))
            {
                if(max_size < 4) break; // Stop packing

                // Pack Extreme Force Magnitudes (Maximum Force Magnitude & Minimum Force Magnitude)
                SETB(pkt_flag, CPP_MEAS_EXTREME_TORQUE_MAGNITUDES_PRESENT, 1);
                co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->max_torque_magnitude));
                co_buf_tail_reserve(p_buf, 2);
                co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->min_torque_magnitude));
                co_buf_tail_reserve(p_buf, 2);
                max_size -= 4;
            }
        }

        if (GETB(meas_flags, CPP_MEAS_EXTREME_ANGLES_PRESENT)) // 28
        {
            if (GETB(p_cpps_env->features, CPP_FEAT_EXTREME_ANGLES_SUP))
            {
                uint32_t angle;
                if(max_size < 3) break; // Stop packing

                // Pack Extreme Angles (Maximum Angle & Minimum Angle)
                // Force to 12 bits
                SETB(pkt_flag, CPP_MEAS_EXTREME_ANGLES_PRESENT, 1);
                angle = (uint32_t) ((p_meas->max_angle & 0x0FFF) | ((p_meas->min_angle & 0x0FFF) << 12));
                co_write24p(co_buf_tail(p_buf), co_htobl(angle));
                co_buf_tail_reserve(p_buf, 3);
                max_size -= 3;
            }
        }

        if (GETB(meas_flags, CPP_MEAS_TOP_DEAD_SPOT_ANGLE_PRESENT)) // 30
        {
            if (GETB(p_cpps_env->features, CPP_FEAT_TOPBOT_DEAD_SPOT_ANGLES_SUP))
            {
                if(max_size < 2) break; // Stop packing

                // Pack Top Dead Spot Angle
                SETB(pkt_flag, CPP_MEAS_TOP_DEAD_SPOT_ANGLE_PRESENT, 1);
                co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->top_dead_spot_angle));
                co_buf_tail_reserve(p_buf, 2);
                max_size -= 2;
            }
        }

        if (GETB(meas_flags, CPP_MEAS_BOTTOM_DEAD_SPOT_ANGLE_PRESENT)) // 32
        {
            if (GETB(p_cpps_env->features, CPP_FEAT_TOPBOT_DEAD_SPOT_ANGLES_SUP))
            {
                if(max_size < 2) break; // Stop packing

                // Pack Bottom Dead Spot Angle
                SETB(pkt_flag, CPP_MEAS_BOTTOM_DEAD_SPOT_ANGLE_PRESENT, 1);
                co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->bot_dead_spot_angle));
                co_buf_tail_reserve(p_buf, 2);
                max_size -= 2;
            }
        }

        if (GETB(meas_flags, CPP_MEAS_ACCUM_ENERGY_PRESENT))  // 34
        {
            if (GETB(p_cpps_env->features, CPP_FEAT_ACCUM_ENERGY_SUP))
            {
                if(max_size < 2) break; // Stop packing

                // Pack Accumulated Energy
                SETB(pkt_flag, CPP_MEAS_ACCUM_ENERGY_PRESENT, 1);
                co_write16p(co_buf_tail(p_buf), co_htobs(p_meas->accum_energy));
                co_buf_tail_reserve(p_buf, 2);
                max_size -= 2;
            }
        }

        complete = true;
    } while(0);

    // Instant Power (Mandatory)
    co_buf_head_reserve(p_buf, 2);
    co_write16p(co_buf_data(p_buf), co_htobs(p_meas->inst_power));

    // Flags value
    co_buf_head_reserve(p_buf, 2);
    co_write16p(co_buf_data(p_buf), co_htobs(pkt_flag));

    *p_last_flags = pkt_flag;

    return (complete);
}

/**
 ****************************************************************************************
 * @brief Unpack control point data and process it
 *
 * @param[in] p_cpps_env Environment
 * @param[in] conidx     connection index
 * @param[in] p_buf      pointer to input data
 ****************************************************************************************
 */
__STATIC uint16_t cpps_unpack_ctnl_point_req(cpps_env_t *p_cpps_env, uint8_t conidx, co_buf_t* p_buf)
{
    uint8_t op_code;
    uint8_t ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_NOT_SUPP;
    uint16_t status = GAP_ERR_NO_ERROR;
    union cpp_ctnl_pt_req_val value;
    memset(&value, 0, sizeof(union cpp_ctnl_pt_req_val));
    op_code =  co_buf_data(p_buf)[0];

    if(co_buf_head_release(p_buf, 1) == CO_BUF_ERR_NO_ERROR)
    {
        // Operation Code
        switch (op_code)
        {
            case (CPP_CTNL_PT_SET_CUMUL_VAL):
            {
                // Check if the Wheel Revolution Data feature is supported
                if (GETB(p_cpps_env->features, CPP_FEAT_WHEEL_REV_DATA_SUP))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 4)
                    {
                        // The request can be handled
                        ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_SUCCESS;
                        // Update the environment
                        p_cpps_env->ctrl_pt_op = op_code;
                        // Cumulative value
                        value.cumul_val = co_btohl(co_read32p(co_buf_data(p_buf)));
                    }
                }
            } break;

            case (CPP_CTNL_PT_UPD_SENSOR_LOC):
            {
                // Check if the Multiple Sensor Location feature is supported
                if (GETB(p_cpps_env->features, CPP_FEAT_MULT_SENSOR_LOC_SUP))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 1)
                    {
                        uint8_t sensor_loc = co_buf_data(p_buf)[0];

                        // Check the sensor location value
                        if (sensor_loc < CPP_LOC_MAX)
                        {
                            value.sensor_loc = sensor_loc;
                            // The request can be handled
                            ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_SUCCESS;
                            // Update the environment
                            p_cpps_env->ctrl_pt_op = op_code;
                        }
                    }
                }
            } break;

            case (CPP_CTNL_PT_REQ_SUPP_SENSOR_LOC):
            {
                // Check if the Multiple Sensor Location feature is supported
                if (GETB(p_cpps_env->features, CPP_FEAT_MULT_SENSOR_LOC_SUP))
                {
                    // The request can be handled
                    ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_SUCCESS;
                    // Update the environment
                    p_cpps_env->ctrl_pt_op = op_code;
                }
            } break;

            case (CPP_CTNL_PT_SET_CRANK_LENGTH):
            {
                // Check if the Crank Length Adjustment feature is supported
                if (GETB(p_cpps_env->features, CPP_FEAT_CRANK_LENGTH_ADJ_SUP))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 2)
                    {
                        // The request can be handled
                        ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_SUCCESS;
                        // Update the environment
                        p_cpps_env->ctrl_pt_op = op_code;
                        // Crank Length
                        value.crank_length = co_btohs(co_read16p(co_buf_data(p_buf)));
                    }
                }
            } break;

            case (CPP_CTNL_PT_REQ_CRANK_LENGTH):
            {
                // Optional even if feature not supported
                ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_SUCCESS;
                // Update the environment
                p_cpps_env->ctrl_pt_op = op_code;
            } break;

            case (CPP_CTNL_PT_SET_CHAIN_LENGTH):
            {
                // Check if the Chain Length Adjustment feature is supported
                if (GETB(p_cpps_env->features, CPP_FEAT_CHAIN_LENGTH_ADJ_SUP))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 2)
                    {
                        // The request can be handled
                        ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_SUCCESS;
                        // Update the environment
                        p_cpps_env->ctrl_pt_op = op_code;
                        // Chain Length
                        value.chain_length = co_btohs(co_read16p(co_buf_data(p_buf)));
                    }
                }
            } break;

            case (CPP_CTNL_PT_REQ_CHAIN_LENGTH):
            {
                // Optional even if feature not supported
                ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_SUCCESS;
                // Update the environment
                p_cpps_env->ctrl_pt_op = op_code;
            } break;

            case (CPP_CTNL_PT_SET_CHAIN_WEIGHT):
            {
                // Check if the Chain Weight Adjustment feature is supported
                if (GETB(p_cpps_env->features, CPP_FEAT_CHAIN_WEIGHT_ADJ_SUP))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 2)
                    {
                        // The request can be handled
                        ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_SUCCESS;
                        // Update the environment
                        p_cpps_env->ctrl_pt_op = op_code;
                        // Chain Weight
                        value.chain_weight = co_btohs(co_read16p(co_buf_data(p_buf)));
                    }
                }
            } break;

            case (CPP_CTNL_PT_REQ_CHAIN_WEIGHT):
            {
                // Optional even if feature not supported
                ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_SUCCESS;
                // Update the environment
                p_cpps_env->ctrl_pt_op = op_code;
            } break;

            case (CPP_CTNL_PT_SET_SPAN_LENGTH):
            {
                // Check if the Span Length Adjustment feature is supported
                if (GETB(p_cpps_env->features, CPP_FEAT_SPAN_LENGTH_ADJ_SUP))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_INV_PARAM;

                    if (co_buf_data_len(p_buf) == 2)
                    {
                        // The request can be handled
                        ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_SUCCESS;
                        // Update the environment
                        p_cpps_env->ctrl_pt_op = op_code;
                        // Span Length
                        value.span_length = co_btohs(co_read16p(co_buf_data(p_buf)));
                    }
                }

            } break;

            case (CPP_CTNL_PT_REQ_SPAN_LENGTH):
            {
                // Optional even if feature not supported
                ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_SUCCESS;
                // Update the environment
                p_cpps_env->ctrl_pt_op = op_code;
            } break;

            case (CPP_CTNL_PT_START_OFFSET_COMP):
            {
                // Check if the Offset Compensation feature is supported
                if (GETB(p_cpps_env->features, CPP_FEAT_OFFSET_COMP_SUP))
                {
                    // The request can be handled
                    ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_SUCCESS;
                    // Update the environment
                    p_cpps_env->ctrl_pt_op = op_code;
                }
            } break;

            case (CPP_CTNL_MASK_CP_MEAS_CH_CONTENT):
            {
                // Check if the CP Masking feature is supported
                if (GETB(p_cpps_env->features, CPP_FEAT_CP_MEAS_CH_CONTENT_MASKING_SUP))
                {
                    // Provided parameter in not within the defined range
                    ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_INV_PARAM;

                    if  (co_buf_data_len(p_buf) == 2)
                    {
                        // The request can be handled
                        ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_SUCCESS;
                        // Update the environment
                        p_cpps_env->ctrl_pt_op = op_code;
                        // Mask content
                        value.mask_content = co_btohs(co_read16p(co_buf_data(p_buf)));
                    }
                }
            } break;

            case (CPP_CTNL_REQ_SAMPLING_RATE):
            {
                // Optional even if feature not supported
                ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_SUCCESS;
                // Update the environment
                p_cpps_env->ctrl_pt_op = op_code;
            } break;

            case (CPP_CTNL_REQ_FACTORY_CALIBRATION_DATE):
            {
                // Optional even if feature not supported
                ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_SUCCESS;
                // Update the environment
                p_cpps_env->ctrl_pt_op = op_code;
            } break;

            case (CPP_CTNL_START_ENHANCED_OFFSET_COMP):
            {
                // Check if the Enhanced Offset Compensation feature is supported
                if (GETB(p_cpps_env->features, CPP_FEAT_ENHANCED_OFFSET_COMPENS_SUP))
                {
                    // The request can be handled
                    ctnl_pt_rsp_status = CPP_CTNL_PT_RESP_SUCCESS;
                    // Update the environment
                    p_cpps_env->ctrl_pt_op = op_code;
                }
            } break;

            default:
            {
                // Operation Code is invalid, status is already CPP_CTNL_PT_RESP_NOT_SUPP
            } break;
        }

        // If no error raised, inform the application about the request
        if (ctnl_pt_rsp_status == CPP_CTNL_PT_RESP_SUCCESS)
        {
            const cpps_cb_t* p_cb  = (const cpps_cb_t*) p_cpps_env->prf_env.p_cb;

            // inform application about control point request
            p_cb->cb_ctnl_pt_req(conidx, op_code, &value);
        }
        else
        {
            co_buf_t* p_out_buf = NULL;

            if(co_buf_alloc(&p_out_buf, GATT_BUFFER_HEADER_LEN, 0, CPP_CP_CNTL_PT_RSP_MIN_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
            {
                cpps_buf_meta_t* p_meta = (cpps_buf_meta_t*)co_buf_metadata(p_out_buf);

                p_cpps_env->ctrl_pt_op    = CPP_CTNL_PT_RSP_CODE;
                co_buf_tail(p_out_buf)[0] = CPP_CTNL_PT_RSP_CODE;
                co_buf_tail_reserve(p_out_buf, 1);
                co_buf_tail(p_out_buf)[0] = op_code;
                co_buf_tail_reserve(p_out_buf, 1);
                co_buf_tail(p_out_buf)[0] = ctnl_pt_rsp_status;
                co_buf_tail_reserve(p_out_buf, 1);

                p_meta->conidx    = conidx;
                p_meta->operation = CPPS_CTNL_PT_RESP_OP_CODE;

                // put event on wait queue
                co_list_push_back(&(p_cpps_env->wait_queue), &(p_out_buf->hdr));
                // execute operation
                cpps_exe_operation(p_cpps_env);
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
 * @brief Packs Vector data
 *
 * @param[in]     p_cpps_env   Environment data
 * @param[in]     p_buf        Pointer to output buffer
 * @param[in]     p_vector     Pointer to vector information
 ****************************************************************************************
 */
__STATIC void cpps_pack_vector(cpps_env_t *p_cpps_env, co_buf_t* p_buf, const cpp_cp_vector_t *p_vector)
{
    uint8_t flags = p_vector->flags;
    bool force_torque_magnitude_present = false;

    // reseve 1 byte for flags
    co_buf_tail_reserve(p_buf, 1);

    // Check provided flags
    if (GETB(flags, CPP_VECTOR_CRANK_REV_DATA_PRESENT))
    {
        if (GETB(p_cpps_env->features, CPP_FEAT_CRANK_REV_DATA_SUP))
        {
            // Pack Crank Revolution Data (Cumulative Crank & Last Crank Event Time)
            co_write16p(co_buf_tail(p_buf), co_htobs(p_vector->cumul_crank_rev));
            co_buf_tail_reserve(p_buf, 2);
            co_write16p(co_buf_tail(p_buf), co_htobs(p_vector->last_crank_evt_time));
            co_buf_tail_reserve(p_buf, 2);
        }
        else // Not supported by the profile
        {
            // Force to not supported
            SETB(flags, CPP_VECTOR_CRANK_REV_DATA_PRESENT, 0);
        }
    }

    if (GETB(flags, CPP_VECTOR_FIRST_CRANK_MEAS_ANGLE_PRESENT))
    {
        if (GETB(p_cpps_env->features, CPP_FEAT_EXTREME_ANGLES_SUP))
        {
            // Pack First Crank Measurement Angle
            co_write16p(co_buf_tail(p_buf), co_htobs(p_vector->first_crank_meas_angle));
            co_buf_tail_reserve(p_buf, 2);
        }
        else // Not supported by the profile
        {
            // Force to not supported
            SETB(flags, CPP_VECTOR_FIRST_CRANK_MEAS_ANGLE_PRESENT, 0);
        }
    }

    if (GETB(flags, CPP_VECTOR_INST_FORCE_MAGNITUDE_ARRAY_PRESENT))
    {
        if (!GETB(p_cpps_env->features, CPP_FEAT_SENSOR_MEAS_CONTEXT))
        {
            force_torque_magnitude_present = true;
        }
        else // Not supported by the profile
        {
            // Force to not supported
            SETB(flags, CPP_VECTOR_INST_FORCE_MAGNITUDE_ARRAY_PRESENT, 0);
        }
    }

    if (GETB(flags, CPP_VECTOR_INST_TORQUE_MAGNITUDE_ARRAY_PRESENT))
    {
        if (GETB(p_cpps_env->features, CPP_FEAT_SENSOR_MEAS_CONTEXT))
        {
            force_torque_magnitude_present = true;
        }
        else // Not supported by the profile
        {
            // Force to not supported
            SETB(flags, CPP_VECTOR_INST_TORQUE_MAGNITUDE_ARRAY_PRESENT, 0);
        }
    }

    if(force_torque_magnitude_present)
    {
        uint8_t cursor;
        uint8_t max_nb = co_min(CPP_MAX_TORQUE_NB, p_vector->nb);
        // Pack Instantaneous Torque Magnitude Array
        for(cursor = 0 ; cursor < max_nb ; cursor++)
        {
            // Pack First Crank Measurement Angle
            co_write16p(co_buf_tail(p_buf), co_htobs(p_vector->force_torque_magnitude[cursor]));
            co_buf_tail_reserve(p_buf, 2);
        }
    }

    // Flags value
    co_buf_data(p_buf)[0] = p_vector->flags;
}


/**
 ****************************************************************************************
 * @brief Packs control point response
 * @param[in]     p_cpps_env   Environment data
 * @param[in]     conidx       Connection Index
 * @param[in]     p_buf        Pointer to output buffer
 * @param[in]     op_code      Operation Code
 * @param[in]     resp_val     Operation result
 * @param[in]     p_value      Pointer to operation response parameters
 ****************************************************************************************
 */
void cpps_pack_ctnl_point_rsp(cpps_env_t *p_cpps_env, uint8_t conidx, co_buf_t* p_buf, uint8_t op_code, uint8_t resp_val,
                               const union cpp_ctnl_pt_rsp_val* p_value)
{
    // Set the Response Code
    co_buf_tail(p_buf)[0] = CPP_CTNL_PT_RSP_CODE;
    co_buf_tail_reserve(p_buf, 1);

    // Set the request operation code
    co_buf_tail(p_buf)[0] = p_cpps_env->ctrl_pt_op;
    co_buf_tail_reserve(p_buf, 1);

    if (resp_val == CPP_CTNL_PT_RESP_SUCCESS)
    {
        co_buf_tail(p_buf)[0] = resp_val;
        co_buf_tail_reserve(p_buf, 1);

        switch (p_cpps_env->ctrl_pt_op)
        {
            case (CPP_CTNL_PT_SET_CUMUL_VAL):
                {
                // Save in the environment
                p_cpps_env->cumul_wheel_rev = p_value->cumul_wheel_rev;
            } break;

            case (CPP_CTNL_PT_UPD_SENSOR_LOC):
            {
                // Store the new value in the environment
                p_cpps_env->sensor_loc = p_value->sensor_loc;
            } break;

            case (CPP_CTNL_PT_REQ_SUPP_SENSOR_LOC):
            {
                // Set the list of supported location
                for (uint8_t counter = 0; counter < CPP_LOC_MAX; counter++)
                {
                    if ((p_value->supp_sensor_loc >> counter) & 0x0001)
                    {
                        co_buf_tail(p_buf)[0] = counter;
                        co_buf_tail_reserve(p_buf, 1);
                    }
                }
            } break;
            case (CPP_CTNL_PT_SET_CRANK_LENGTH):{ /* Nothing to do */ } break;
            case (CPP_CTNL_PT_REQ_CRANK_LENGTH):
            {
                // Set the response parameter
                co_write16p(co_buf_tail(p_buf), co_htobs(p_value->crank_length));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case (CPP_CTNL_PT_SET_CHAIN_LENGTH):{ /* Nothing to do */ } break;
            case (CPP_CTNL_PT_REQ_CHAIN_LENGTH):
            {
                // Set the response parameter
                co_write16p(co_buf_tail(p_buf), co_htobs(p_value->chain_length));
                co_buf_tail_reserve(p_buf, 2);
            } break;
            case (CPP_CTNL_PT_SET_CHAIN_WEIGHT):{ /* Nothing to do */ } break;
            case (CPP_CTNL_PT_REQ_CHAIN_WEIGHT):
            {
                // Set the response parameter
                co_write16p(co_buf_tail(p_buf), co_htobs(p_value->chain_weight));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case (CPP_CTNL_PT_SET_SPAN_LENGTH): { /* Nothing to do */ } break;
            case (CPP_CTNL_PT_REQ_SPAN_LENGTH):
            {
                // Set the response parameter
                co_write16p(co_buf_tail(p_buf), co_htobs(p_value->span_length));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case (CPP_CTNL_PT_START_OFFSET_COMP):
            {
                // Set the response parameter
                co_write16p(co_buf_tail(p_buf), co_htobs(p_value->offset_comp));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case (CPP_CTNL_MASK_CP_MEAS_CH_CONTENT):
            {
                uint16_t cpp_mask_cp_meas_flags [] =
                {
                    CPP_MEAS_PEDAL_POWER_BALANCE_PRESENT_BIT,
                    CPP_MEAS_ACCUM_TORQUE_PRESENT_BIT,
                    CPP_MEAS_WHEEL_REV_DATA_PRESENT_BIT,
                    CPP_MEAS_CRANK_REV_DATA_PRESENT_BIT,
                    CPP_MEAS_EXTREME_FORCE_MAGNITUDES_PRESENT_BIT |
                    CPP_MEAS_EXTREME_TORQUE_MAGNITUDES_PRESENT_BIT,
                    CPP_MEAS_EXTREME_ANGLES_PRESENT_BIT,
                    CPP_MEAS_TOP_DEAD_SPOT_ANGLE_PRESENT_BIT,
                    CPP_MEAS_BOTTOM_DEAD_SPOT_ANGLE_PRESENT_BIT,
                    CPP_MEAS_ACCUM_ENERGY_PRESENT_BIT,
                };

                uint16_t mask = 0;

                for (uint8_t count = 0; count < 9; count++)
                {
                    if ((p_value->mask_meas_content >> count) & 0x0001)
                    {
                        mask |= cpp_mask_cp_meas_flags[count];
                    }
                }

                p_cpps_env->env[conidx].mask_meas_content = mask;
            } break;

            case (CPP_CTNL_REQ_SAMPLING_RATE):
            {
                // Set the response parameter
                co_buf_tail(p_buf)[0] = p_value->sampling_rate;
                co_buf_tail_reserve(p_buf, 1);
            } break;

            case (CPP_CTNL_REQ_FACTORY_CALIBRATION_DATE):
            {
                // Set the response parameter
                prf_pack_date_time(p_buf, &(p_value->factory_calibration));
            } break;

            case (CPP_CTNL_START_ENHANCED_OFFSET_COMP):
            {
                uint8_t cursor;
                uint8_t length;

                // Set the response parameter
                co_write16p(co_buf_tail(p_buf), co_htobs(p_value->enhanced_offset_comp.comp_offset));
                co_buf_tail_reserve(p_buf, 2);
                // manufacturer company ID
                co_write16p(co_buf_tail(p_buf), co_htobs(p_value->enhanced_offset_comp.manu_comp_id));
                co_buf_tail_reserve(p_buf, 2);
                // length
                length = co_min(p_value->enhanced_offset_comp.length,
                                CPP_CP_CNTL_PT_RSP_MAX_LEN - co_buf_data_len(p_buf) - 1);

                co_buf_tail(p_buf)[0] = length;
                co_buf_tail_reserve(p_buf, 1);

                for (cursor = 0; cursor < length ; cursor++)
                {
                    co_buf_tail(p_buf)[0] = p_value->enhanced_offset_comp.data[cursor];
                    co_buf_tail_reserve(p_buf, 1);
                }
            } break;
            default: { /* Nothing to do */ } break;
        }
    }
    else
    {
        // Operation results in an error condition
        if(   (p_cpps_env->ctrl_pt_op == CPP_CTNL_START_ENHANCED_OFFSET_COMP)
           && (p_value->enhanced_offset_comp.rsp_param != CPP_CTNL_PT_ERR_RSP_PARAM_INCORRECT_CALIB_POS)
           && (p_value->enhanced_offset_comp.rsp_param != CPP_CTNL_PT_ERR_RSP_PARAM_MANUF_SPEC_ERR_FOLLOWS))
        {
            resp_val = CPP_CTNL_PT_RESP_INV_PARAM;
        }

        // Set the Response Value
        co_buf_tail(p_buf)[0] = (resp_val > CPP_CTNL_PT_RESP_FAILED) ? CPP_CTNL_PT_RESP_FAILED : resp_val;
        co_buf_tail_reserve(p_buf, 1);


        if(p_cpps_env->ctrl_pt_op == CPP_CTNL_START_ENHANCED_OFFSET_COMP)
        {
            // Response parameter
            co_buf_tail(p_buf)[0] = p_value->enhanced_offset_comp.rsp_param;
            co_buf_tail_reserve(p_buf, 1);

            if (p_value->enhanced_offset_comp.rsp_param == CPP_CTNL_PT_ERR_RSP_PARAM_MANUF_SPEC_ERR_FOLLOWS)
            {
                uint8_t cursor;
                uint8_t length;

                // manufacturer company ID
                co_write16p(co_buf_tail(p_buf), co_htobs(p_value->enhanced_offset_comp.manu_comp_id));
                co_buf_tail_reserve(p_buf, 2);
                // length
                length = co_min(p_value->enhanced_offset_comp.length,
                                CPP_CP_CNTL_PT_RSP_MAX_LEN - co_buf_data_len(p_buf) - 1);

                co_buf_tail(p_buf)[0] = length;
                co_buf_tail_reserve(p_buf, 1);

                for (cursor = 0; cursor < length ; cursor++)
                {
                    co_buf_tail(p_buf)[0] = p_value->enhanced_offset_comp.data[cursor];
                    co_buf_tail_reserve(p_buf, 1);
                }
            }
        }
    }
}

/**
 ****************************************************************************************
 * @brief  This function fully manages notification of measurement and vector
 ****************************************************************************************
 */
__STATIC void cpps_exe_operation(cpps_env_t* p_cpps_env)
{
    if(!p_cpps_env->in_exe_op)
    {
        p_cpps_env->in_exe_op = true;

        while(!co_list_is_empty(&(p_cpps_env->wait_queue)) && !(p_cpps_env->op_ongoing))
        {
            uint16_t status = GAP_ERR_NO_ERROR;
            co_buf_t* p_buf = (co_buf_t*) co_list_pick(&(p_cpps_env->wait_queue));
            cpps_buf_meta_t* p_meta = (cpps_buf_meta_t*) co_buf_metadata(p_buf);

            switch(p_meta->operation)
            {
                case CPPS_NTF_VECTOR_OP_CODE:
                {
                    uint8_t  conidx;
                    uint32_t conidx_bf = 0;

                    // remove buffer from queue
                    co_list_pop_front(&(p_cpps_env->wait_queue));

                    // check connection that support notification reception
                    for(conidx = 0 ; conidx < BLE_CONNECTION_MAX ; conidx++)
                    {
                        if(GETB(p_cpps_env->env[conidx].prfl_ntf_ind_cfg, CPP_PRF_CFG_FLAG_VECTOR_NTF))
                        {
                            conidx_bf |= CO_BIT(conidx);
                        }
                    }

                    // send notification only on selected connections
                    conidx_bf &= p_meta->conidx_bf;

                    if(conidx_bf != 0)
                    {
                        // send multipoint notification
                        status = gatt_srv_event_mtp_send(conidx_bf, p_cpps_env->user_lid, CPPS_NTF_VECTOR_OP_CODE,
                                                         GATT_NOTIFY, CPPS_HANDLE(CPS_IDX_VECTOR_VAL), p_buf, true);
                        if(status == GAP_ERR_NO_ERROR)
                        {
                            p_cpps_env->op_ongoing = true;
                        }
                    }

                    co_buf_release(p_buf);

                    if(!p_cpps_env->op_ongoing)
                    {
                        const cpps_cb_t* p_cb = (const cpps_cb_t*) p_cpps_env->prf_env.p_cb;
                        // Inform application that event has been sent
                        p_cb->cb_vector_send_cmp(status);
                    }
                } break;

                case CPPS_NTF_MEAS_OP_CODE:
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
                            if (GETB(p_cpps_env->env[conidx].prfl_ntf_ind_cfg, CPP_PRF_CFG_FLAG_CP_MEAS_NTF))
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
                        att_info.hdl    = CPPS_HANDLE(CPS_IDX_CP_MEAS_VAL);;
                        att_info.length = CPP_CP_MEAS_MAX_LEN;

                        status = gatt_srv_event_reliable_send(p_meta->conidx, p_cpps_env->user_lid, CPPS_NTF_MEAS_OP_CODE,
                                                              GATT_NOTIFY, 1, &att_info);

                        if(status == GAP_ERR_NO_ERROR)
                        {
                            p_cpps_env->op_ongoing = true;
                        }
                    }
                    else
                    {
                        const cpps_cb_t* p_cb = (const cpps_cb_t*) p_cpps_env->prf_env.p_cb;
                        // Inform application that event has been sent
                        p_cb->cb_meas_send_cmp(GAP_ERR_NO_ERROR);

                        // remove buffer from queue
                        co_list_pop_front(&(p_cpps_env->wait_queue));
                        co_buf_release(p_buf);
                    }
                } break;

                default:
                {
                    uint8_t conidx = p_meta->conidx;
                    // remove buffer from queue
                    co_list_pop_front(&(p_cpps_env->wait_queue));

                    status = gatt_srv_event_send(conidx, p_cpps_env->user_lid, p_meta->operation, GATT_INDICATE,
                                                 CPPS_HANDLE(CPS_IDX_CTNL_PT_VAL), p_buf);

                    if(status == GAP_ERR_NO_ERROR)
                    {
                        p_cpps_env->op_ongoing = true;
                    }
                    else
                    {
                        // Inform application that control point response has been sent
                        if (p_cpps_env->ctrl_pt_op != CPP_CTNL_PT_RSP_CODE)
                        {
                            const cpps_cb_t* p_cb = (const cpps_cb_t*) p_cpps_env->prf_env.p_cb;
                            p_cb->cb_ctnl_pt_rsp_send_cmp(conidx, status);
                        }

                        // consider control point operation done
                        p_cpps_env->ctrl_pt_op = CPP_CTNL_PT_RESERVED;
                    }

                    co_buf_release(p_buf);
                } break;
            }
        }

        p_cpps_env->in_exe_op = false;
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
__STATIC void cpps_cb_att_read_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                   uint16_t max_length)
{
    cpps_env_t *p_cpps_env = PRF_ENV_GET(CPPS, cpps);
    // retrieve value attribute
    co_buf_t* p_buf       = NULL;
    uint16_t  status      = GAP_ERR_NO_ERROR;
    uint16_t  att_val_len = 0;

    if(p_cpps_env == NULL)
    {
        status = PRF_APP_ERROR;
    }
    else if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0,  CPP_CP_CNTL_PT_RSP_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
    {
        uint8_t att_idx = cpps_idx_get(p_cpps_env, hdl);

        switch (att_idx)
        {
            case CPS_IDX_CP_MEAS_NTF_CFG:
            {
                uint16_t ntf_cfg = GETB(p_cpps_env->env[conidx].prfl_ntf_ind_cfg, CPP_PRF_CFG_FLAG_CP_MEAS_NTF)
                                 ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ntf_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case CPS_IDX_CP_MEAS_BCST_CFG:
            {
                // Broadcast feature is profile specific
                if (CPPS_IS_FEATURE_SUPPORTED(p_cpps_env->prfl_cfg, CPPS_MEAS_BCST_MASK))
                {
                    uint16_t bcst_cfg = GETB(p_cpps_env->env[conidx].prfl_ntf_ind_cfg, CPP_PRF_CFG_FLAG_SP_MEAS_NTF)
                                      ? PRF_SRV_START_BCST : PRF_SRV_STOP_BCST;

                    co_write16p(co_buf_tail(p_buf), co_htobs(bcst_cfg));
                    co_buf_tail_reserve(p_buf, 2);
                }
                else
                {
                    status = ATT_ERR_ATTRIBUTE_NOT_FOUND;
                }
            } break;

            case CPS_IDX_VECTOR_NTF_CFG:
            {
                uint16_t ntf_cfg = GETB(p_cpps_env->env[conidx].prfl_ntf_ind_cfg, CPP_PRF_CFG_FLAG_VECTOR_NTF)
                                 ? PRF_CLI_START_NTF : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ntf_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case CPS_IDX_CTNL_PT_IND_CFG:
            {
                uint16_t ind_cfg = GETB(p_cpps_env->env[conidx].prfl_ntf_ind_cfg, CPP_PRF_CFG_FLAG_CTNL_PT_IND)
                                 ? PRF_CLI_START_IND : PRF_CLI_STOP_NTFIND;

                co_write16p(co_buf_tail(p_buf), co_htobs(ind_cfg));
                co_buf_tail_reserve(p_buf, 2);
            } break;

            case CPS_IDX_CP_FEAT_VAL:
            {
                co_write32p(co_buf_tail(p_buf), co_htobl(p_cpps_env->features));
                co_buf_tail_reserve(p_buf, 4);
            } break;

            case CPS_IDX_SENSOR_LOC_VAL:
            {
                co_buf_tail(p_buf)[0] = p_cpps_env->sensor_loc;
                co_buf_tail_reserve(p_buf, 1);
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
__STATIC void cpps_cb_att_val_set(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t hdl, uint16_t offset,
                                  co_buf_t* p_data)
{
    cpps_env_t *p_cpps_env = PRF_ENV_GET(CPPS, cpps);
    uint16_t  status      = PRF_APP_ERROR;

    if(p_cpps_env != NULL)
    {
        uint8_t cfg_upd_flag  = 0;
        uint16_t cfg_en_val = 0;

        switch (cpps_idx_get(p_cpps_env, hdl))
        {
            case CPS_IDX_CP_MEAS_NTF_CFG:
            {
                cfg_upd_flag = CPP_PRF_CFG_FLAG_CP_MEAS_NTF_BIT;
                cfg_en_val   = PRF_CLI_START_NTF;
            } break;

            case CPS_IDX_CP_MEAS_BCST_CFG:
            {
                cfg_upd_flag = CPP_PRF_CFG_FLAG_SP_MEAS_NTF_BIT;
                cfg_en_val   = PRF_SRV_START_BCST;
            } break;

            case CPS_IDX_VECTOR_NTF_CFG:
            {
                cfg_upd_flag = CPP_PRF_CFG_FLAG_VECTOR_NTF_BIT;
                cfg_en_val   = PRF_CLI_START_NTF;
            } break;

            case CPS_IDX_CTNL_PT_IND_CFG:
            {
                cfg_upd_flag = CPP_PRF_CFG_FLAG_CTNL_PT_IND_BIT;
                cfg_en_val   = PRF_CLI_START_IND;
            } break;

            case CPS_IDX_CTNL_PT_VAL:
            {
                // Check if sending of indications has been enabled
                if (!GETB(p_cpps_env->env[conidx].prfl_ntf_ind_cfg, CPP_PRF_CFG_FLAG_CTNL_PT_IND))
                {
                    // CPP improperly configured
                    status = PRF_CCCD_IMPR_CONFIGURED;
                }
                else if (p_cpps_env->ctrl_pt_op != CPP_CTNL_PT_RESERVED)
                {
                    // A procedure is already in progress
                    status = CPP_ERROR_PROC_IN_PROGRESS;
                }
                else
                {
                    // Unpack Control Point parameters
                    status = cpps_unpack_ctnl_point_req(p_cpps_env, conidx, p_data);
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
                const cpps_cb_t* p_cb  = (const cpps_cb_t*) p_cpps_env->prf_env.p_cb;

                if(cfg == PRF_CLI_STOP_NTFIND)
                {
                    p_cpps_env->env[conidx].prfl_ntf_ind_cfg &= ~cfg_upd_flag;
                }
                else
                {
                    p_cpps_env->env[conidx].prfl_ntf_ind_cfg |= cfg_upd_flag;
                }

                // inform application about update
                p_cb->cb_bond_data_upd(conidx, cfg_upd_flag, cfg);
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
__STATIC void cpps_cb_att_event_get(uint8_t conidx, uint8_t user_lid, uint16_t token, uint16_t dummy, uint16_t hdl,
                                      uint16_t max_length)
{
    cpps_env_t *p_cpps_env = PRF_ENV_GET(CPPS, cpps);
    co_buf_t*   p_buf       = NULL;
    uint16_t    status      = GAP_ERR_NO_ERROR;
    uint16_t    att_length  = 0;

    // allocate buffer for event transmission
    if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, CPP_CP_MEAS_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
    {
        if(p_cpps_env == NULL)
        {
            status = PRF_APP_ERROR;
        }
        else
        {
            co_buf_t* p_meas = (co_buf_t*) co_list_pick(&(p_cpps_env->wait_queue));
            cpps_buf_meta_t* p_meta = (cpps_buf_meta_t*) co_buf_metadata(p_meas);

            ASSERT_ERR(p_meas != NULL);

            // pack measurement
            p_meta->new = cpps_pack_meas(p_cpps_env, p_buf, (cpp_cp_meas_t*)co_buf_head(p_meas), max_length,
                                         p_cpps_env->env[conidx].mask_meas_content, &(p_meta->last_flag));

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
__STATIC void cpps_cb_event_sent(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    // Consider job done
    cpps_env_t *p_cpps_env = PRF_ENV_GET(CPPS, cpps);
    if(p_cpps_env != NULL)
    {
        const cpps_cb_t* p_cb  = (const cpps_cb_t*) p_cpps_env->prf_env.p_cb;
        p_cpps_env->op_ongoing = false;

        switch(dummy)
        {
            case CPPS_NTF_VECTOR_OP_CODE:
            {
                p_cb->cb_vector_send_cmp(status);
            } break;
            case CPPS_NTF_MEAS_OP_CODE: { /* Nothing to do */ } break;
            default:
            {
                // Inform application that control point response has been sent
                if (p_cpps_env->ctrl_pt_op != CPP_CTNL_PT_RSP_CODE)
                {
                    p_cb->cb_ctnl_pt_rsp_send_cmp(conidx, status);
                }

                p_cpps_env->ctrl_pt_op = CPP_CTNL_PT_RESERVED;
            } break;
        }

        // continue operation execution
        cpps_exe_operation(p_cpps_env);
    }
}

/// Service callback hander
__STATIC const gatt_srv_cb_t cpps_cb =
{
        .cb_event_sent    = cpps_cb_event_sent,
        .cb_att_read_get  = cpps_cb_att_read_get,
        .cb_att_event_get = cpps_cb_att_event_get,
        .cb_att_info_get  = NULL,
        .cb_att_val_set   = cpps_cb_att_val_set,
};

/*
 * PROFILE NATIVE HANDLERS
 ****************************************************************************************
 */

uint16_t cpps_enable(uint8_t conidx, uint8_t ntf_ind_cfg)
{
    cpps_env_t* p_cpps_env = PRF_ENV_GET(CPPS, cpps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_cpps_env != NULL)
    {
        // check state of the task
        if (gapc_get_conhdl(conidx) != GAP_INVALID_CONHDL)
        {
            p_cpps_env->env[conidx].prfl_ntf_ind_cfg = ntf_ind_cfg;
            status = GAP_ERR_NO_ERROR;
        }
    }

    return (status);
}

uint16_t cpps_adv_data_pack(co_buf_t* p_buf, const cpp_cp_meas_t* p_meas)
{
    cpps_env_t* p_cpps_env = PRF_ENV_GET(CPPS, cpps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_buf == NULL)
    {
        status = PRF_ERR_INVALID_PARAM;
    }
    else if(co_buf_tail_len(p_buf) < CPP_CP_MEAS_ADV_MAX_LEN)
    {
        status = GAP_ERR_INSUFF_RESOURCES;
    }
    else if(p_cpps_env != NULL)
    {
        // Check Broadcast is supported
        if (CPPS_IS_FEATURE_SUPPORTED(p_cpps_env->prfl_cfg, CPPS_MEAS_BCST_MASK))
        {
            uint16_t flags = 0;
            uint16_t mask_content = 0;
            co_buf_tail_reserve(p_buf, CPP_CP_ADV_HEADER_LEN + CPP_CP_ADV_LENGTH_LEN);
            co_buf_head_release(p_buf, CPP_CP_ADV_HEADER_LEN + CPP_CP_ADV_LENGTH_LEN);

            // Pack Cp Measurement
            cpps_pack_meas(p_cpps_env, p_buf, p_meas, CPP_CP_MEAS_ADV_MAX_LEN, mask_content, &flags);

            // Pack UUID of CPS
            co_buf_head_reserve(p_buf, 2);
            co_write16p(co_buf_data(p_buf), co_btohs(GATT_SVC_CYCLING_POWER));

            // Pack Service Data AD type
            co_buf_head_reserve(p_buf, 1);
            co_buf_data(p_buf)[0] = GAP_AD_TYPE_SERVICE_16_BIT_DATA;

            // Set AD length
            co_buf_head_reserve(p_buf, 1);
            co_buf_data(p_buf)[0] = (uint8_t) (co_buf_data_len(p_buf) - 1);

            status = GAP_ERR_NO_ERROR;
        }
    }

    return (status);
}

uint16_t cpps_meas_send(uint32_t conidx_bf, int16_t cumul_wheel_rev, const cpp_cp_meas_t* p_meas)
{
    cpps_env_t* p_cpps_env = PRF_ENV_GET(CPPS, cpps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_meas == NULL)
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_cpps_env != NULL)
    {
        co_buf_t* p_buf;
        // Should be updated just once
        if (GETB(p_meas->flags, CPP_MEAS_WHEEL_REV_DATA_PRESENT))
        {
            if (GETB(p_cpps_env->features, CPP_FEAT_WHEEL_REV_DATA_SUP))
            {
                // Update the cumulative wheel revolutions value stored in the environment
                // The value shall not decrement below zero
                if (cumul_wheel_rev < 0)
                {
                    p_cpps_env->cumul_wheel_rev = (co_abs(cumul_wheel_rev) > p_cpps_env->cumul_wheel_rev)
                                                ? 0 : (p_cpps_env->cumul_wheel_rev + cumul_wheel_rev);
                }
                else
                {
                    p_cpps_env->cumul_wheel_rev += cumul_wheel_rev;
                }
            }
        }

        if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, sizeof(cpp_cp_meas_t), GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            cpps_buf_meta_t* p_buf_meta = (cpps_buf_meta_t*) co_buf_metadata(p_buf);
            p_buf_meta->operation = CPPS_NTF_MEAS_OP_CODE;
            p_buf_meta->conidx    = 0;
            p_buf_meta->conidx_bf = conidx_bf & ((1 << BLE_CONNECTION_MAX) - 1);
            p_buf_meta->new       = true;

            // copy structure info - use buffer head to ensure that buffer is 32-bit aligned
            memcpy(co_buf_head(p_buf), p_meas, sizeof(cpp_cp_meas_t));
            // put event on wait queue
            co_list_push_back(&(p_cpps_env->wait_queue), &(p_buf->hdr));
            // execute operation
            cpps_exe_operation(p_cpps_env);
            status = GAP_ERR_NO_ERROR;
        }
        else
        {
            status = GAP_ERR_INSUFF_RESOURCES;
        }
    }

    return (status);

}

uint16_t cpps_vector_send(uint32_t conidx_bf, const cpp_cp_vector_t* p_vector)
{
    cpps_env_t* p_cpps_env = PRF_ENV_GET(CPPS, cpps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_vector == NULL)
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_cpps_env != NULL)
    {
        if(! CPPS_IS_FEATURE_SUPPORTED(p_cpps_env->prfl_cfg, CPPS_VECTOR_MASK))
        {
            status = PRF_ERR_FEATURE_NOT_SUPPORTED;
        }
        else
        {
            co_buf_t* p_buf;

            if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, CPP_CP_VECTOR_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
            {
                cpps_buf_meta_t* p_buf_meta = (cpps_buf_meta_t*) co_buf_metadata(p_buf);
                p_buf_meta->operation = CPPS_NTF_VECTOR_OP_CODE;
                p_buf_meta->conidx    = GAP_INVALID_CONIDX;
                p_buf_meta->conidx_bf = conidx_bf & ((1 << BLE_CONNECTION_MAX) - 1);
                p_buf_meta->new       = true;

                // Pack structure
                cpps_pack_vector(p_cpps_env, p_buf, p_vector);
                // put event on wait queue
                co_list_push_back(&(p_cpps_env->wait_queue), &(p_buf->hdr));
                // execute operation
                cpps_exe_operation(p_cpps_env);
                status = GAP_ERR_NO_ERROR;
            }
            else
            {
                status = GAP_ERR_INSUFF_RESOURCES;
            }
        }
    }

    return (status);
}

uint16_t cpps_ctnl_pt_rsp_send(uint8_t conidx, uint8_t op_code, uint8_t resp_val, const union cpp_ctnl_pt_rsp_val* p_value)
{
    cpps_env_t* p_cpps_env = PRF_ENV_GET(CPPS, cpps);
    uint16_t status = PRF_ERR_REQ_DISALLOWED;

    if(p_value == NULL)
    {
        status = GAP_ERR_INVALID_PARAM;
    }
    else if(p_cpps_env != NULL)
    {
        do
        {
            co_buf_t* p_buf = NULL;

            // Check the current operation
            if (p_cpps_env->ctrl_pt_op ==  CPP_CTNL_PT_RESERVED)
            {
                // The confirmation has been sent without request indication, ignore
                break;
            }

            // The CP Control Point Characteristic must be supported if we are here
            if (!CPPS_IS_FEATURE_SUPPORTED(p_cpps_env->prfl_cfg, CPPS_CTNL_PT_MASK))
            {
                status = PRF_ERR_REQ_DISALLOWED;
                break;
            }

            // Check if sending of indications has been enabled
            if (!GETB(p_cpps_env->env[conidx].prfl_ntf_ind_cfg, CPP_PRF_CFG_FLAG_CTNL_PT_IND))
            {
                // mark operation done
                p_cpps_env->ctrl_pt_op = CPP_CTNL_PT_RESERVED;
                // CPP improperly configured
                status = PRF_CCCD_IMPR_CONFIGURED;
                break;
            }

            if(co_buf_alloc(&p_buf, GATT_BUFFER_HEADER_LEN, 0, CPP_CP_CNTL_PT_RSP_MAX_LEN + GATT_BUFFER_TAIL_LEN) == CO_BUF_ERR_NO_ERROR)
            {
                cpps_buf_meta_t* p_buf_meta = (cpps_buf_meta_t*) co_buf_metadata(p_buf);
                p_buf_meta->operation = CPPS_CTNL_PT_RESP_OP_CODE;
                p_buf_meta->conidx    = conidx;
                p_buf_meta->new       = true;

                // Pack structure
                cpps_pack_ctnl_point_rsp(p_cpps_env, conidx, p_buf, op_code, resp_val, p_value);
                // put event on wait queue
                co_list_push_back(&(p_cpps_env->wait_queue), &(p_buf->hdr));
                // execute operation
                cpps_exe_operation(p_cpps_env);
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
 * @brief Send a CPPS_CMP_EVT message to the application.
 * @param[in] conidx connection index
 * @param[in] operation Operation completed
 * @param[in] status status of the operation
 ****************************************************************************************
 */
__STATIC void cpps_send_cmp_evt(uint8_t conidx, uint8_t operation, uint16_t status)
{
    // Get the address of the environment
    struct cpps_cmp_evt *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(CPPS_CMP_EVT, PRF_DST_TASK(CPPS), PRF_SRC_TASK(CPPS), cpps_cmp_evt);

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
 * @brief Handles reception of the @ref CPPS_ENABLE_REQ message.
 * @param[in] msgid Id of the message received
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int cpps_enable_req_handler(ke_msg_id_t const msgid, struct cpps_enable_req *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    struct cpps_enable_rsp *p_cmp_evt;
    uint16_t status = cpps_enable(p_param->conidx, p_param->prfl_ntf_ind_cfg);

    // send completed information to APP task that contains error status
    p_cmp_evt = KE_MSG_ALLOC(CPPS_ENABLE_RSP, src_id, dest_id, cpps_enable_rsp);

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
 * @brief Handles reception of the @ref CPPS_GET_ADV_DATA_REQ message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int cpps_get_adv_data_req_handler(ke_msg_id_t const msgid, struct cpps_get_adv_data_req *p_param,
                                           ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    co_buf_t* p_buf = NULL;

    // Allocate the message
    struct cpps_get_adv_data_rsp *p_rsp = KE_MSG_ALLOC_DYN(CPPS_GET_ADV_DATA_RSP, src_id, dest_id,
                                                           cpps_get_adv_data_rsp, CPP_CP_MEAS_ADV_MAX_LEN);

    if(p_rsp != NULL)
    {
        if(co_buf_alloc(&p_buf, 0, 0, CPP_CP_MEAS_ADV_MAX_LEN) == CO_BUF_ERR_NO_ERROR)
        {
            // pack data
            p_rsp->status = cpps_adv_data_pack(p_buf, &(p_param->parameters));
            p_rsp->data_len = co_buf_data_len(p_buf);
            // copy in response
            co_buf_copy_data_to_mem(p_buf, p_rsp->adv_data, p_rsp->data_len);
            // free buffer
            co_buf_release(p_buf);
        }
        else
        {
            p_rsp->status = GAP_ERR_INSUFF_RESOURCES;
            p_rsp->data_len = 0;
        }
        ke_msg_send(p_rsp);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref CPPS_NTF_CP_MEAS_CMD message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int cpps_ntf_cp_meas_cmd_handler(ke_msg_id_t const msgid, struct cpps_ntf_cp_meas_cmd *p_param,
                                          ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = cpps_meas_send(p_param->conidx_bf, p_param->cumul_wheel_rev, &(p_param->parameters));

    if(status != GAP_ERR_NO_ERROR)
    {
        cpps_send_cmp_evt(GAP_INVALID_CONIDX, CPPS_NTF_MEAS_OP_CODE, status);
    }

    return (KE_MEM_KE_MSG);
}

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref CPPS_NTF_CP_VECTOR_CMDREQ message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int cpps_ntf_cp_vector_cmd_handler(ke_msg_id_t const msgid, struct cpps_ntf_cp_vector_cmd *p_param,
                                            ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    uint16_t status = cpps_vector_send(p_param->conidx_bf, &(p_param->parameters));

    if(status != GAP_ERR_NO_ERROR)
    {
        cpps_send_cmp_evt(GAP_INVALID_CONIDX, CPPS_NTF_VECTOR_OP_CODE, status);
    }

    return (KE_MEM_KE_MSG);
}



/**
 ****************************************************************************************
 * @brief Handles reception of the @ref CPPS_CTNL_PT_CFM message.
 * @param[in] msgid Id of the message received.
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */
__STATIC int cpps_ctnl_pt_rsp_send_cmd_handler(ke_msg_id_t const msgid, struct cpps_ctnl_pt_rsp_send_cmd *p_param,
                                             ke_task_id_t const dest_id,  ke_task_id_t const src_id)
{
    uint16_t status = cpps_ctnl_pt_rsp_send(p_param->conidx, p_param->op_code, p_param->resp_val, &(p_param->value));

    if(status != GAP_ERR_NO_ERROR)
    {
        cpps_send_cmp_evt(p_param->conidx, CPPS_CTNL_PT_RESP_OP_CODE, status);
    }

    return (KE_MEM_KE_MSG);
}


/// Default State handlers definition
KE_MSG_HANDLER_TAB(cpps)
{
    // Note: all messages must be sorted in ID ascending order

    { CPPS_ENABLE_REQ,               (ke_msg_func_t) cpps_enable_req_handler            },
    { CPPS_GET_ADV_DATA_REQ,         (ke_msg_func_t) cpps_get_adv_data_req_handler      },
    { CPPS_NTF_CP_MEAS_CMD,          (ke_msg_func_t) cpps_ntf_cp_meas_cmd_handler       },
    { CPPS_NTF_CP_VECTOR_CMD,        (ke_msg_func_t) cpps_ntf_cp_vector_cmd_handler     },
    { CPPS_CTNL_PT_RSP_SEND_CMD,     (ke_msg_func_t) cpps_ctnl_pt_rsp_send_cmd_handler  },
};

/**
 ****************************************************************************************
 * @brief Completion of measurement transmission
 *
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void cpps_cb_meas_send_cmp(uint16_t status)
{
    cpps_send_cmp_evt(GAP_INVALID_CONIDX, CPPS_NTF_MEAS_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Completion of vector transmission
 *
 * @param[in] status Status of the procedure execution (@see enum hl_err)
 ****************************************************************************************
 */
__STATIC void cpps_cb_vector_send_cmp(uint16_t status)
{
    cpps_send_cmp_evt(GAP_INVALID_CONIDX, CPPS_NTF_VECTOR_OP_CODE, status);
}

/**
 ****************************************************************************************
 * @brief Inform that bond data updated for the connection.
 *
 * @param[in] conidx        Connection index
 * @param[in] char_code     Characteristic Code (CP Measurement or Control Point)
 * @param[in] cfg_val       Stop/notify/indicate value to configure into the peer characteristic
 ****************************************************************************************
 */
__STATIC void cpps_cb_bond_data_upd(uint8_t conidx, uint8_t char_code, uint16_t cfg_val)
{
    struct cpps_cfg_ntfind_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(CPPS_CFG_NTFIND_IND, PRF_DST_TASK(CPPS),
                         PRF_SRC_TASK(CPPS), cpps_cfg_ntfind_ind);

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
 * @brief Inform that peer device requests an action using control point
 *
 * @note control point request must be answered using @see cpps_ctnl_pt_rsp_send function
 *
 * @param[in] conidx        Connection index
 * @param[in] op_code       Operation Code (@see enum cpp_ctnl_pt_code)
 * @param[in] p_value       Pointer to control point request value
 ****************************************************************************************
 */
__STATIC void cpps_cb_ctnl_pt_req(uint8_t conidx, uint8_t op_code, const union cpp_ctnl_pt_req_val* p_value)
{
    struct cpps_ctnl_pt_req_recv_ind *p_evt;

    // Send the message
    p_evt = KE_MSG_ALLOC(CPPS_CTNL_PT_REQ_RECV_IND, PRF_DST_TASK(CPPS),
                         PRF_SRC_TASK(CPPS), cpps_ctnl_pt_req_recv_ind);

    if(p_evt != NULL)
    {
        p_evt->conidx     = conidx;
        p_evt->op_code    = op_code;
        memcpy(&(p_evt->value), p_value, sizeof(union cpp_ctnl_pt_req_val));
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
__STATIC void cpps_cb_ctnl_pt_rsp_send_cmp(uint8_t conidx, uint16_t status)
{
    cpps_send_cmp_evt(conidx, CPPS_CTNL_PT_RESP_OP_CODE, status);
}

/// Default Message handle
__STATIC const cpps_cb_t cpps_msg_cb =
{
        .cb_meas_send_cmp        = cpps_cb_meas_send_cmp,
        .cb_vector_send_cmp      = cpps_cb_vector_send_cmp,
        .cb_bond_data_upd        = cpps_cb_bond_data_upd,
        .cb_ctnl_pt_req          = cpps_cb_ctnl_pt_req,
        .cb_ctnl_pt_rsp_send_cmp = cpps_cb_ctnl_pt_rsp_send_cmp,
};
#endif // (BLE_HL_MSG_API)

/*
 * PROFILE DEFAULT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the CPPS module.
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
__STATIC uint16_t cpps_init(prf_data_t *p_env, uint16_t *p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          struct cpps_db_cfg *p_params, const cpps_cb_t* p_cb)
{
    //------------------ create the attribute database for the profile -------------------

    // DB Creation Status
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t user_lid = GATT_INVALID_USER_LID;

    do
    {
        cpps_env_t* p_cpps_env;
        // Service content flag
        uint32_t cfg_flag = CPPS_MANDATORY_MASK;

        #if (BLE_HL_MSG_API)
        if(p_cb == NULL)
        {
            p_cb = &(cpps_msg_cb);
        }
        #endif // (BLE_HL_MSG_API)

        if(   (p_params == NULL) || (p_start_hdl == NULL) || (p_cb == NULL) || (p_cb->cb_meas_send_cmp == NULL)
           || (p_cb->cb_vector_send_cmp == NULL)  || (p_cb->cb_bond_data_upd == NULL)  || (p_cb->cb_ctnl_pt_req == NULL)
           || (p_cb->cb_ctnl_pt_rsp_send_cmp == NULL))
        {
            status = GAP_ERR_INVALID_PARAM;
            break;
        }

        // register CPPS user
        status = gatt_user_srv_register(CPP_CP_MEAS_MAX_LEN + 3, user_prio, &cpps_cb, &user_lid);
        if(status != GAP_ERR_NO_ERROR) break;

        // Check if Broadcaster role shall be added.
        if (CPPS_IS_FEATURE_SUPPORTED(p_params->prfl_config, CPPS_BROADCASTER_SUPP_FLAG))
        {
            //Add configuration to the database
            cfg_flag |= CPPS_MEAS_BCST_MASK;
        }

        // Check if the CP Vector characteristic shall be added.
        // Mandatory if at least one Vector procedure is supported, otherwise excluded.
        if (   GETB(p_params->cp_feature, CPP_FEAT_CRANK_REV_DATA_SUP)
            || GETB(p_params->cp_feature, CPP_FEAT_EXTREME_ANGLES_SUP)
            || GETB(p_params->cp_feature, CPP_FEAT_INSTANT_MEAS_DIRECTION_SUP))
        {
            cfg_flag |= CPPS_VECTOR_MASK;
        }

        // Check if the Control Point characteristic shall be added
        // Mandatory if server supports:
        //     - Wheel Revolution Data
        //     - Multiple Sensor Locations
        //     - Configurable Settings (CPP_CTNL_PT_SET codes)
        //     - Offset Compensation
        //     - Server allows to be requested for parameters (CPP_CTNL_PT_REQ codes)
        if (   CPPS_IS_FEATURE_SUPPORTED(p_params->prfl_config, CPPS_CTNL_PT_CHAR_SUPP_FLAG)
            || GETB(p_params->cp_feature, CPP_FEAT_WHEEL_REV_DATA_SUP)
            || GETB(p_params->cp_feature, CPP_FEAT_MULT_SENSOR_LOC_SUP)
            || GETB(p_params->cp_feature, CPP_FEAT_CRANK_LENGTH_ADJ_SUP)
            || GETB(p_params->cp_feature, CPP_FEAT_CHAIN_LENGTH_ADJ_SUP)
            || GETB(p_params->cp_feature, CPP_FEAT_CHAIN_WEIGHT_ADJ_SUP)
            || GETB(p_params->cp_feature, CPP_FEAT_SPAN_LENGTH_ADJ_SUP)
            || GETB(p_params->cp_feature, CPP_FEAT_OFFSET_COMP_SUP))
        {
            cfg_flag |= CPPS_CTNL_PT_MASK;
        }

        // Add GAP service
        status = gatt_db_svc16_add(user_lid, sec_lvl, GATT_SVC_CYCLING_POWER, CPS_IDX_NB,
                                   (uint8_t *)&cfg_flag, &(cpps_att_db[0]), CPS_IDX_NB, p_start_hdl);
        if(status != GAP_ERR_NO_ERROR) break;

        //-------------------- allocate memory required for the profile  ---------------------
        p_cpps_env = (cpps_env_t *) ke_malloc(sizeof(cpps_env_t), KE_MEM_ATT_DB);

        if(p_cpps_env != NULL)
        {
            // allocate CPPS required environment variable
            p_env->p_env = (prf_hdr_t *) p_cpps_env;
            p_cpps_env->start_hdl       = *p_start_hdl;
            p_cpps_env->user_lid        = user_lid;
            p_cpps_env->prfl_cfg        = cfg_flag;
            p_cpps_env->features        = p_params->cp_feature;
            p_cpps_env->sensor_loc      = p_params->sensor_loc;
            p_cpps_env->cumul_wheel_rev = p_params->wheel_rev;
            p_cpps_env->op_ongoing      = false;
            p_cpps_env->in_exe_op       = false;
            p_cpps_env->ctrl_pt_op      = CPP_CTNL_PT_RESERVED;
            memset(p_cpps_env->env, 0, sizeof(p_cpps_env->env));
            co_list_init(&(p_cpps_env->wait_queue));

            // Check if the Broadcaster role shall be added.
            if (CPPS_IS_FEATURE_SUPPORTED(p_cpps_env->prfl_cfg, CPPS_MEAS_BCST_MASK))
            {
                // Add Broadcast property
                gatt_db_att_info_set(user_lid, CPPS_HANDLE(CPS_IDX_CP_MEAS_VAL), (PROP(N) | PROP(B)));
            }

            // initialize profile environment variable
            p_cpps_env->prf_env.p_cb     = p_cb;
            #if (BLE_HL_MSG_API)
            p_env->desc.msg_handler_tab  = cpps_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(cpps_msg_handler_tab);
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
__STATIC uint16_t cpps_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    cpps_env_t *p_cpps_env = (cpps_env_t *) p_env->p_env;

    if(reason != PRF_DESTROY_RESET)
    {
        status = gatt_user_unregister(p_cpps_env->user_lid);
    }

    if(status == GAP_ERR_NO_ERROR)
    {
        if(reason != PRF_DESTROY_RESET)
        {
            // remove buffer in wait queue
            while(!co_list_is_empty(&p_cpps_env->wait_queue))
            {
                co_buf_t* p_buf = (co_buf_t*) co_list_pop_front(&p_cpps_env->wait_queue);
                co_buf_release(p_buf);
            }
        }

        // free profile environment variables
        p_env->p_env = NULL;
        ke_free(p_cpps_env);
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
__STATIC void cpps_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
{
    cpps_env_t *p_cpps_env = (cpps_env_t *) p_env->p_env;
    memset(&(p_cpps_env->env[conidx]), 0, sizeof(cpps_cnx_env_t));
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
__STATIC void cpps_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    cpps_env_t *p_cpps_env = (cpps_env_t *) p_env->p_env;
    // clean-up environment variable allocated for task instance
    memset(&(p_cpps_env->env[conidx]), 0, sizeof(cpps_cnx_env_t));
}



/// CPPS Task interface required by profile manager
const prf_task_cbs_t cpps_itf =
{
    .cb_init          = (prf_init_cb) cpps_init,
    .cb_destroy       = cpps_destroy,
    .cb_con_create    = cpps_con_create,
    .cb_con_cleanup   = cpps_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve service profile interface
 *
 * @return service profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t *cpps_prf_itf_get(void)
{
    return &cpps_itf;
}

#endif //(BLE_CP_SENSOR)

/// @} CPPS
