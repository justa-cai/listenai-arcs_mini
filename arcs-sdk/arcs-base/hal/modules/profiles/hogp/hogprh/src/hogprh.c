/**
 ****************************************************************************************
 * @addtogroup HOGPRH
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#define BLE_HID_REPORT_HOST (1)
#if (BLE_HID_REPORT_HOST)
#include <string.h>

#include "hogprh.h"
#include "ble_cli_prf.h"

/*
 * LOCAL VARIABLES DEFINITION
 ****************************************************************************************
 */
/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
hogprh_env_t hogprh_env;
hogprh_env_t* p_hogprh_env = &hogprh_env;

/// State machine used to retrieve HID Service characteristics information
const prf_char_def_t hogprh_hids_char[HOGPRH_CHAR_REPORT + 1] =
{
    /// Report Map
    [HOGPRH_CHAR_REPORT_MAP]             = {BLE_GATT_CHAR_REPORT_MAP,    ATT_REQ(PRES, MAND),  BLE_PROP(RD)},
    /// HID Information
    [HOGPRH_CHAR_HID_INFO]               = {BLE_GATT_CHAR_HID_INFO,      ATT_REQ(PRES, MAND),  BLE_PROP(RD)},
    /// HID Control Point
    [HOGPRH_CHAR_HID_CTNL_PT]            = {BLE_GATT_CHAR_HID_CTNL_PT,   ATT_REQ(PRES, MAND),  BLE_PROP(WC)},
    /// Protocol Mode
    [HOGPRH_CHAR_PROTOCOL_MODE]          = {BLE_GATT_CHAR_PROTOCOL_MODE, ATT_REQ(PRES, OPT),  (BLE_PROP(RD) | BLE_PROP(WC))},
    /// Report
    [HOGPRH_CHAR_REPORT]                 = {BLE_GATT_CHAR_REPORT,        ATT_REQ(PRES, MAND),  BLE_PROP(RD)},
};

/// State machine used to retrieve HID Service characteristic description information
const prf_desc_def_t hogprh_hids_char_desc[HOGPRH_DESC_MAX] =
{
    /// Report Map Char. External Report Reference Descriptor
    [HOGPRH_DESC_REPORT_MAP_EXT_REP_REF]   = {BLE_GATT_DESC_EXT_REPORT_REF,  ATT_REQ(PRES, OPT), HOGPRH_CHAR_REPORT},
    /// Report Char. Report Reference
    [HOGPRH_DESC_REPORT_REF]           = {BLE_GATT_DESC_REPORT_REF,   ATT_REQ(PRES, OPT), HOGPRH_CHAR_REPORT},
    /// Report Client Config
    [HOGPRH_DESC_REPORT_CFG]           = {BLE_GATT_DESC_CLIENT_CHAR_CFG,   ATT_REQ(PRES, OPT), HOGPRH_CHAR_REPORT},
};

/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */
 
/**
 ****************************************************************************************
 * @brief Send discovery results to application.
 *
 * @param p_hogprh_env    Client Role task environment
 * @param conidx        Connection index
 * @param status        Response status code
 *****************************************************************************************
 */
__STATIC void hogprh_enable_cmp(hogprh_env_t* p_hogprh_env, uint8_t conidx, uint16_t status)
{
    const hogprh_cb_t* p_cb = (const hogprh_cb_t*) p_hogprh_env->prf_env.p_cb;

    CLOGD("hogprh_enable_cmp, status:0x%x", status);
    if(p_hogprh_env != NULL)
    {
        hogprh_cnx_env_t* p_con_env = p_hogprh_env->p_env[conidx];

        if (status != BLE_GAP_ERR_NO_ERROR)
        {
            // clean-up environment variable allocated for task instance
            ke_free(p_con_env);
            p_hogprh_env->p_env[conidx] = NULL;
        }
        else
        {
            uint8_t cursor;
            p_con_env->discover = false;

            for(cursor = 0 ; cursor < p_con_env->nb_svc ; cursor++)
            {
                // Register profile handle to catch gatt indications
                gatt_cli_event_register(conidx, p_hogprh_env->user_lid, p_con_env->hids[cursor].svc.shdl,
                                        p_con_env->hids[cursor].svc.ehdl);
            }
        }
        p_cb->cb_enable_cmp(conidx, status, p_con_env->nb_svc, p_con_env->hids);
    }
}

/**
 ****************************************************************************************
 * @brief Send read result to application,.
 ****************************************************************************************
 */
__STATIC void hogprh_read_val_cmp(uint8_t conidx, uint16_t status, uint16_t dummy, uint16_t data_len, uint8_t* p_data)
{

    if(p_hogprh_env != NULL)
    {
        const hogprh_cb_t* p_cb = (const hogprh_cb_t*) p_hogprh_env->prf_env.p_cb;
        uint8_t hogprh_inst      = BLE_GETF(dummy, HOGPRH_DUMMY_HIDS_INST) & 0x0f;
        uint8_t val_id        = BLE_GETF(dummy, HOGPRH_DUMMY_VAL_ID);
        uint8_t report_idx = (BLE_GETF(dummy, HOGPRH_DUMMY_HIDS_INST) & 0xf0) >> 4;


        switch(val_id)
        {
            case HOGPRH_PROTO_MODE:
            {
                uint8_t proto_mode = 0;

                if(status == BLE_GAP_ERR_NO_ERROR)
                {
                    proto_mode = p_data[0];
                }
                p_cb->cb_read_att_val_cmp(conidx, status, hogprh_inst, val_id, report_idx, &proto_mode, sizeof(proto_mode));
            }break;
            case HOGPRH_REPORT_MAP:
            {
                if(status == BLE_GAP_ERR_NO_ERROR)
                {
                    ///
                }
                p_cb->cb_read_att_val_cmp(conidx, status, hogprh_inst, val_id, report_idx, p_data, data_len);
            }break;
            case HOGPRH_REPORT_MAP_EXT_REP_REF:
            {
                if(status == BLE_GAP_ERR_NO_ERROR)
                {
                    ///
                }
                p_cb->cb_read_att_val_cmp(conidx, status, hogprh_inst, val_id, report_idx, p_data, data_len);
            }break;
            case HOGPRH_HID_INFO:
            {
                struct hids_hid_info hid_info;

                if(status == BLE_GAP_ERR_NO_ERROR)
                {
                    hid_info.bcdHID       = ble_co_read16p(p_data);
                    hid_info.bCountryCode = p_data[2];
                    hid_info.flags        = p_data[3];
                }
                p_cb->cb_read_att_val_cmp(conidx, status, hogprh_inst, val_id, report_idx, &hid_info, sizeof(hid_info));
            }break;
            case HOGPRH_REPORT:
            {
                if(status == BLE_GAP_ERR_NO_ERROR)
                {
                    ///
                }
                p_cb->cb_read_att_val_cmp(conidx, status, hogprh_inst, val_id, report_idx, p_data, data_len);
            }break;
            case HOGPRH_REPORT_REF:
            {
                struct hogprh_report_ref report_ref;

                if(status == BLE_GAP_ERR_NO_ERROR)
                {
                    report_ref.id   = p_data[0];
                    report_ref.type = p_data[1];
                }
                p_cb->cb_read_att_val_cmp(conidx, status, hogprh_inst, val_id, report_idx, &report_ref, sizeof(report_ref));
            }break;
            case HOGPRH_REPORT_NTF_CFG:
            {
                uint16_t report_cfg;

                if(status == BLE_GAP_ERR_NO_ERROR)
                {
                    report_cfg = ble_co_read16p(p_data);
                }
                p_cb->cb_read_att_val_cmp(conidx, status, hogprh_inst, val_id, report_idx, &report_cfg, sizeof(report_cfg));
            }break;
            default: { /* Nothing to do */ } break;
        }
    }
}


/**
 ****************************************************************************************
 * @brief Perform Value read procedure.
 ****************************************************************************************
 */
__STATIC uint16_t hogprh_read_val(uint8_t conidx, uint8_t hogprh_instance, uint16_t val_id, uint8_t report_idx)
{
    uint16_t status = BLE_PRF_ERR_REQ_DISALLOWED;
    // Client environment
    //CLOGD("hogprh_read_val, val_id:0x%x,report_idx:%d", val_id, report_idx);
    if(p_hogprh_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_hogprh_env->p_env[conidx] != NULL) && (!p_hogprh_env->p_env[conidx]->discover))
        {
            hogprh_cnx_env_t* p_con_env = p_hogprh_env->p_env[conidx];

            if (hogprh_instance >= p_con_env->nb_svc)
            {
                status = BLE_PRF_ERR_INVALID_PARAM;
            }
            else
            {
                uint16_t hdl;
                hogprh_content_t* p_hogprh = &(p_con_env->hids[hogprh_instance]);

                switch(val_id)
                {
                    case HOGPRH_PROTO_MODE:         { hdl = p_hogprh->chars[HOGPRH_CHAR_PROTOCOL_MODE].val_hdl;              } break;
                    case HOGPRH_REPORT_MAP:              { hdl = p_hogprh->chars[HOGPRH_CHAR_REPORT_MAP].val_hdl;         } break;
                    case HOGPRH_REPORT_MAP_EXT_REP_REF: { hdl = p_hogprh->descs[HOGPRH_DESC_REPORT_MAP_EXT_REP_REF].desc_hdl; } break;
                    case HOGPRH_HID_INFO:         { hdl = p_hogprh->chars[HOGPRH_CHAR_HID_INFO].val_hdl;              } break;
                    case HOGPRH_HID_CTNL_PT:              { hdl = p_hogprh->chars[HOGPRH_CHAR_HID_CTNL_PT].val_hdl;         } break;
                    case HOGPRH_REPORT: { hdl = p_hogprh->chars[HOGPRH_CHAR_REPORT + report_idx].val_hdl; } break;
                    case HOGPRH_REPORT_REF:         { hdl = p_hogprh->descs[HOGPRH_DESC_REPORT_REF + report_idx].desc_hdl;              } break;
                    case HOGPRH_REPORT_NTF_CFG:              { hdl = p_hogprh->descs[HOGPRH_DESC_REPORT_CFG + report_idx].desc_hdl;         } break;
                    default:                        { hdl = BLE_GATT_INVALID_HDL;                                       } break;
                }

                if(hdl == BLE_GATT_INVALID_HDL)
                {
                    status = BLE_PRF_ERR_INEXISTENT_HDL;
                }
                else
                {
                    uint16_t dummy = 0;
                    BLE_SETF(dummy, HOGPRH_DUMMY_HIDS_INST, hogprh_instance | report_idx << 4);
                    BLE_SETF(dummy, HOGPRH_DUMMY_VAL_ID,   val_id);
                    // perform read request
                    status = gatt_cli_read(conidx, p_hogprh_env->user_lid, dummy, hdl, 0, 0);
                }
                //CLOGD("hogprh_read_val, hdl:%d, status:0x%x", hdl, status);
            }
        }
        else
        {

        }
    }

    return (status);
}

/**
 ****************************************************************************************
 * @brief Perform Value read procedure.
 ****************************************************************************************
 */
__STATIC uint16_t hogprh_write_val(uint8_t conidx, uint8_t hogprh_instance, uint16_t val_id, uint8_t report_idx, uint8_t write_type, 
                                            uint16_t length, const uint8_t* p_data)
{
    uint16_t status = BLE_PRF_ERR_REQ_DISALLOWED;
    // Client environment
    //CLOGD("hogprh_read_val, val_id:0x%x,report_idx:%d", val_id, report_idx);
    if(p_hogprh_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_hogprh_env->p_env[conidx] != NULL) && (!p_hogprh_env->p_env[conidx]->discover))
        {
            hogprh_cnx_env_t* p_con_env = p_hogprh_env->p_env[conidx];

            if (hogprh_instance >= p_con_env->nb_svc)
            {
                status = BLE_PRF_ERR_INVALID_PARAM;
            }
            else
            {
                uint16_t hdl;
                hogprh_content_t* p_hogprh = &(p_con_env->hids[hogprh_instance]);

                switch(val_id)
                {
                    case HOGPRH_PROTO_MODE:         { hdl = p_hogprh->chars[HOGPRH_CHAR_PROTOCOL_MODE].val_hdl;              } break;
                    case HOGPRH_HID_CTNL_PT:              { hdl = p_hogprh->chars[HOGPRH_CHAR_HID_CTNL_PT].val_hdl;         } break;
                    case HOGPRH_REPORT: { hdl = p_hogprh->chars[HOGPRH_CHAR_REPORT + report_idx].val_hdl; } break;
                    case HOGPRH_REPORT_NTF_CFG:              { hdl = p_hogprh->descs[HOGPRH_DESC_REPORT_CFG + report_idx].desc_hdl;         } break;
                    default:                        { hdl = BLE_GATT_INVALID_HDL;                                       } break;
                }

                if(hdl == BLE_GATT_INVALID_HDL)
                {
                    status = BLE_PRF_ERR_INEXISTENT_HDL;
                }
                else
                {
                    uint16_t dummy = 0;
                    BLE_SETF(dummy, HOGPRH_DUMMY_HIDS_INST, hogprh_instance | report_idx << 4);
                    BLE_SETF(dummy, HOGPRH_DUMMY_VAL_ID,   val_id);
                    // perform read request
                    status =prf_gatt_write(conidx, p_hogprh_env->user_lid, dummy, write_type, hdl, length, p_data);
                }
                CLOGD("hogprh_write_val, hdl:%d, status:0x%x, len:%d", hdl, status, length);
            }
        }
        else
        {

        }
    }

    return (status);
}

/*
 * GATT USER CLIENT HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief This function is called when a full service has been found during a discovery procedure.
 ****************************************************************************************
 */
__STATIC void hogprh_svc_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint8_t disc_info,
                          uint8_t nb_att, const ble_gatt_svc_att_t* p_atts)
{
    // Get the address of the environment

    CLOGD("hogprh_svc_cb, info:0x%x,p_atts:0x%x,hdl:%d,nb_att:%d",disc_info,p_atts, hdl, nb_att);
    if(p_hogprh_env != NULL)
    {
        hogprh_cnx_env_t* p_con_env = p_hogprh_env->p_env[conidx];

        if(p_con_env != NULL)
        {
            if (p_con_env->nb_svc < HOGPRH_NB_HIDS_INST_MAX)
            {
                uint8_t i;
                prf_char_def_t hids_char[HOGPRH_CHAR_MAX];
                prf_desc_def_t hids_char_desc[HOGPRH_DESC_MAX];

                // 1. Create characteristic reference
                memcpy(hids_char, hogprh_hids_char, sizeof(struct prf_char_def) * HOGPRH_CHAR_REPORT);
                for(i = HOGPRH_CHAR_REPORT ; i < HOGPRH_CHAR_MAX ; i++)
                {
                    hids_char[i] = hogprh_hids_char[HOGPRH_CHAR_REPORT];
                }

                // 2. create descriptor reference
                // Report Map Char. External Report Reference Descriptor
                hids_char_desc[HOGPRH_DESC_REPORT_MAP_EXT_REP_REF].char_code = HOGPRH_CHAR_REPORT_MAP;
                hids_char_desc[HOGPRH_DESC_REPORT_MAP_EXT_REP_REF].req_bf  = ATT_REQ(PRES, OPT);
                hids_char_desc[HOGPRH_DESC_REPORT_MAP_EXT_REP_REF].uuid      = BLE_GATT_DESC_EXT_REPORT_REF;

                for(i = 0 ; i < HOGPRH_NB_REPORT_INST_MAX ; i++)
                {
                    // Report Char. Report Reference
                    hids_char_desc[HOGPRH_DESC_REPORT_REF + i].char_code = HOGPRH_CHAR_REPORT + i;
                    hids_char_desc[HOGPRH_DESC_REPORT_REF + i].req_bf  = ATT_REQ(PRES, OPT);
                    hids_char_desc[HOGPRH_DESC_REPORT_REF + i].uuid      = BLE_GATT_DESC_REPORT_REF;
                    // Report Client Config
                    hids_char_desc[HOGPRH_DESC_REPORT_CFG + i].char_code = HOGPRH_CHAR_REPORT + i;
                    hids_char_desc[HOGPRH_DESC_REPORT_CFG + i].req_bf  = ATT_REQ(PRES, OPT);
                    hids_char_desc[HOGPRH_DESC_REPORT_CFG + i].uuid      = BLE_GATT_DESC_CLIENT_CHAR_CFG;
                }

                // 3. Retrieve HID characteristics
                prf_extract_svc_info(hdl, nb_att, p_atts,
                                     HOGPRH_CHAR_MAX, &hids_char[0], p_con_env->hids[p_con_env->nb_svc].chars,
                                     HOGPRH_DESC_MAX, &hids_char_desc[0], p_con_env->hids[p_con_env->nb_svc].descs);

                // 4. Store service range
                if((disc_info == BLE_GATT_SVC_CMPLT) || (disc_info == BLE_GATT_SVC_START))
                {
                    p_con_env->hids[p_con_env->nb_svc].svc.shdl = hdl;
                }

                if((disc_info == BLE_GATT_SVC_CMPLT) || (disc_info == BLE_GATT_SVC_END))
                {
                    p_con_env->hids[p_con_env->nb_svc].svc.ehdl = hdl + nb_att -1;
                }

                // 5. Search for report nb
                for(i = HOGPRH_CHAR_REPORT ; i < HOGPRH_CHAR_MAX ; i++)
                {
                    if(p_con_env->hids[p_con_env->nb_svc].chars[i].val_hdl != BLE_GATT_INVALID_HDL)
                    {
                        p_con_env->hids[p_con_env->nb_svc].report_nb++;
                        //CLOGD("i:%d,hdl:%d,report_nb:%d",i, p_con_env->hids[p_con_env->nb_svc].chars[i].val_hdl ,p_con_env->hids[p_con_env->nb_svc].report_nb);
                    }
                }
                //CLOGD("hogprh_svc_cb,svc:%d, report_nb:%d",p_con_env->nb_svc ,p_con_env->hids[p_con_env->nb_svc].report_nb);

                if((disc_info == BLE_GATT_SVC_CMPLT) || (disc_info == BLE_GATT_SVC_END))
                {
                    p_con_env->nb_svc++;
                }
            }
        }
    }
}

/**
 ****************************************************************************************
 * @brief This function is called when GATT client user discovery procedure is over.
 ****************************************************************************************
 */
__STATIC void hogprh_discover_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    CLOGD("hogprh_discover_cmp_cb,sta:0x%x", status);
    if(p_hogprh_env != NULL)
    {
        hogprh_cnx_env_t* p_con_env = p_hogprh_env->p_env[conidx];

        if (p_con_env->nb_svc > 0)
        {
            uint8_t cursor;
            uint8_t i;
            prf_char_def_t hids_char[HOGPRH_CHAR_MAX];
            prf_desc_def_t hids_char_desc[HOGPRH_DESC_MAX];
            
            // 1. Create characteristic reference
            memcpy(hids_char, hogprh_hids_char, sizeof(struct prf_char_def) * HOGPRH_CHAR_REPORT);
            for(i = HOGPRH_CHAR_REPORT ; i < HOGPRH_CHAR_MAX ; i++)
            {
                hids_char[i] = hogprh_hids_char[HOGPRH_CHAR_REPORT];
            }
            
            // 2. create descriptor reference
            // Report Map Char. External Report Reference Descriptor
            hids_char_desc[HOGPRH_DESC_REPORT_MAP_EXT_REP_REF].char_code = HOGPRH_CHAR_REPORT_MAP;
            hids_char_desc[HOGPRH_DESC_REPORT_MAP_EXT_REP_REF].req_bf  = ATT_REQ(PRES, OPT);
            hids_char_desc[HOGPRH_DESC_REPORT_MAP_EXT_REP_REF].uuid      = BLE_GATT_DESC_EXT_REPORT_REF;
            
            for(i = 0 ; i < HOGPRH_NB_REPORT_INST_MAX ; i++)
            {
                // Report Char. Report Reference
                hids_char_desc[HOGPRH_DESC_REPORT_REF + i].char_code = HOGPRH_CHAR_REPORT + i;
                hids_char_desc[HOGPRH_DESC_REPORT_REF + i].req_bf  = ATT_REQ(PRES, OPT);
                hids_char_desc[HOGPRH_DESC_REPORT_REF + i].uuid      = BLE_GATT_DESC_REPORT_REF;
                // Report Client Config
                hids_char_desc[HOGPRH_DESC_REPORT_CFG + i].char_code = HOGPRH_CHAR_REPORT + i;
                hids_char_desc[HOGPRH_DESC_REPORT_CFG + i].req_bf  = ATT_REQ(PRES, OPT);
                hids_char_desc[HOGPRH_DESC_REPORT_CFG + i].uuid      = BLE_GATT_DESC_CLIENT_CHAR_CFG;
            }
            
            for (cursor = 0; (cursor < p_con_env->nb_svc) && (status == BLE_GAP_ERR_NO_ERROR); cursor++)
            {
                status = prf_check_svc_char_validity(HOGPRH_CHAR_MAX, p_con_env->hids[cursor].chars, &hids_char[0]);
                if(status != BLE_GAP_ERR_NO_ERROR) break;

                status = prf_check_svc_desc_validity(HOGPRH_DESC_MAX, p_con_env->hids[cursor].descs, &hids_char_desc[0],
                                                          p_con_env->hids[cursor].chars);
            }
        }
        else
        {
            status = BLE_PRF_ERR_STOP_DISC_CHAR_MISSING;
        }

        hogprh_enable_cmp(p_hogprh_env, conidx, status);
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
__STATIC void hogprh_att_val_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t hdl, uint16_t offset,
                              void* p_data)
{
    hogprh_read_val_cmp(conidx, BLE_GAP_ERR_NO_ERROR, dummy, ble_co_buf_data_len(p_data), ble_co_buf_data(p_data));
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
__STATIC void hogprh_read_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(status != BLE_GAP_ERR_NO_ERROR)
    {
        hogprh_read_val_cmp(conidx, status, dummy, 0, NULL);
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
__STATIC void hogprh_write_cmp_cb(uint8_t conidx, uint8_t user_lid, uint16_t dummy, uint16_t status)
{
    if(p_hogprh_env != NULL)
    {
        uint8_t hogprh_inst      = BLE_GETF(dummy, HOGPRH_DUMMY_HIDS_INST) & 0x0f;
        uint8_t val_id        = BLE_GETF(dummy, HOGPRH_DUMMY_VAL_ID);
        uint8_t report_idx = (BLE_GETF(dummy, HOGPRH_DUMMY_HIDS_INST) & 0xf0) >> 4;

        const hogprh_cb_t* p_cb = (const hogprh_cb_t*) p_hogprh_env->prf_env.p_cb;
        p_cb->cb_write_att_val_cmp(conidx, status, hogprh_inst, val_id, report_idx);
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
__STATIC void hogprh_att_val_evt_cb(uint8_t conidx, uint8_t user_lid, uint16_t token, uint8_t evt_type, bool complete,
                                  uint16_t hdl, void* p_data)
{
    //CLOGD("hogprh_att_val_evt_cb ,evt_type:%d,hdl:%d", evt_type, hdl);

    if(p_hogprh_env != NULL)
    {
        hogprh_cnx_env_t* p_con_env = p_hogprh_env->p_env[conidx];

        //CLOGD("hogprh_att_val_evt_cb,hdl:%d", hdl);

        if(p_con_env != NULL)
        {
            uint8_t hogprh_inst, report_idx,found = 0;
            const hogprh_cb_t* p_cb = (const hogprh_cb_t*) p_hogprh_env->prf_env.p_cb;

            // Search for report.
            for (hogprh_inst = 0; (hogprh_inst < p_con_env->nb_svc); hogprh_inst++)
            {
                for(report_idx = 0; (report_idx < p_con_env->hids[hogprh_inst].report_nb); report_idx++)
                {
                    if (hdl == p_con_env->hids[hogprh_inst].chars[HOGPRH_CHAR_REPORT + report_idx].val_hdl)
                    {
                    
                        //CLOGD("hogprh_att_val_evt_cb,hdl:%d,%d,i:%d", hdl, p_con_env->hids[hogprh_inst].chars[HOGPRH_CHAR_REPORT + report_idx].val_hdl);
                        uint16_t len = ble_co_buf_data_len(p_data);
                        p_cb->cb_att_val_ntf_upd(conidx, hogprh_inst, HOGPRH_REPORT, report_idx, ble_co_buf_data(p_data), len);
                        found = 1;
                        break;
                    }
                }
            }
            if(found == 0)                    
            {
                CLOGD("hogprh_att_val_evt_cb error,hdl:%d", hdl);
            }
        }
    }

    // confirm event reception
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
__STATIC void hogprh_svc_changed_cb(uint8_t conidx, uint8_t user_lid, bool out_of_sync, uint16_t start_hdl, uint16_t end_hdl)
{
    // Do Nothing
}

/// Client callback hander
__STATIC const ble_gatt_cli_cb_t hogprh_cb =
{
    .cb_discover_cmp    = hogprh_discover_cmp_cb,
    .cb_read_cmp        = hogprh_read_cmp_cb,
    .cb_write_cmp       = hogprh_write_cmp_cb,
    .cb_att_val_get     = NULL,
    .cb_svc             = hogprh_svc_cb,
    .cb_svc_info        = NULL,
    .cb_inc_svc         = NULL,
    .cb_char            = NULL,
    .cb_desc            = NULL,
    .cb_att_val         = hogprh_att_val_cb,
    .cb_att_val_evt     = hogprh_att_val_evt_cb,
    .cb_svc_changed     = hogprh_svc_changed_cb,
};

/*
 * PROFILE NATIVE API
 ****************************************************************************************
 */

uint16_t hogprh_enable(uint8_t conidx, uint8_t con_type, uint8_t nb_hogprh, const hogprh_content_t* p_hogprh)
{
    uint16_t status = BLE_PRF_ERR_REQ_DISALLOWED;

    if((nb_hogprh > HOGPRH_NB_HIDS_INST_MAX) || ((con_type == PRF_CON_NORMAL) && (p_hogprh == NULL)))
    {
        status = BLE_PRF_ERR_INVALID_PARAM;
    }
    else if(p_hogprh_env != NULL)
    {
        if ((conidx < BLE_CONNECTION_MAX) && (p_hogprh_env->p_env[conidx] == NULL))
        {
            // allocate environment variable for task instance
            p_hogprh_env->p_env[conidx] = (hogprh_cnx_env_t *)plf_malloc(sizeof(hogprh_cnx_env_t));

            if(p_hogprh_env->p_env[conidx] != NULL)
            {
                memset(p_hogprh_env->p_env[conidx], 0, sizeof(hogprh_cnx_env_t));
                // Config connection, start discovering
                if (con_type == PRF_CON_DISCOVERY)
                {
                    uint16_t gatt_svc_uuid = BLE_GATT_SVC_HID;
                    CLOGD("start discovery");
                    // start discovery
                    status = ble_gatt_cli_discover_svc(conidx, p_hogprh_env->user_lid, 0, BLE_GATT_DISCOVER_SVC_PRIMARY_BY_UUID, true,
                                                   BLE_GATT_MIN_HDL, BLE_GATT_MAX_HDL, BLE_GATT_UUID_16, (uint8_t*) &gatt_svc_uuid);

                    // Go to DISCOVERING state
                    p_hogprh_env->p_env[conidx]->discover = true;
                }
                // normal connection, get saved att details
                else
                {
                    p_hogprh_env->p_env[conidx]->nb_svc = nb_hogprh;
                    memcpy(p_hogprh_env->p_env[conidx]->hids, p_hogprh, sizeof(hogprh_content_t) *nb_hogprh);
                    status = BLE_GAP_ERR_NO_ERROR;

                    // send APP confirmation that can start normal connection to TH
                    hogprh_enable_cmp(p_hogprh_env, conidx, BLE_GAP_ERR_NO_ERROR);
                }
            }
            else
            {
                status = BLE_GAP_ERR_INSUFF_RESOURCES;
            }
        }
    }

    CLOGD("hogprh_enable,sta:0x%x", status);

    return (status);
}


#if (BLE_HL_MSG_API)
/**
 ****************************************************************************************
 * @brief Completion of Enable procedure
 ****************************************************************************************
 */
void hogph_cb_enable_cmp(uint8_t conidx, uint16_t status, uint8_t hogprh_nb, const hogprh_content_t* p_hogprh)
{
    ///
}
/**
 ****************************************************************************************
 * @brief Inform that att val read procedure is over
 ****************************************************************************************
 */
void hogph_cb_read_att_val_cmp(uint8_t conidx, uint16_t status, uint8_t hogprh_instance, uint8_t hogprh_idx, uint8_t *p_data, uint16_t data_len)
{
    ///
}
/**
 ****************************************************************************************
 * @brief Inform that att val read procedure is over
 ****************************************************************************************
 */
void hogph_cb_write_att_val_cmp(uint8_t conidx, uint16_t status, uint8_t hogprh_instance, uint8_t hogprh_idx)
{
    ///
}
/**
 ****************************************************************************************
 * @brief Inform that att val read procedure is over
 ****************************************************************************************
 */
void hogph_cb_att_val_ntf_upd(uint8_t conidx, uint8_t hogprh_instance, uint8_t hogprh_idx, uint8_t *p_data, uint16_t data_len)
{
    ///
}

/// Default Message handle
__STATIC const hogprh_cb_t hogprh_msg_cb =
{
    .cb_enable_cmp           = hogph_cb_enable_cmp,
    .cb_read_att_val_cmp  = hogph_cb_read_att_val_cmp,
    .cb_write_att_val_cmp     = hogph_cb_write_att_val_cmp,
    .cb_att_val_ntf_upd = hogph_cb_att_val_ntf_upd,
};
#endif
/**
 ****************************************************************************************
 * @brief Initialization of the HOGPRH module.
 * This function performs all the initializations of the Profile module.
 *  - Creation of datahide (if it's a service)
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
static uint8_t hogprh_init(uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                          const void* p_params, const hogprh_cb_t* p_cb)
{
        uint8_t conidx;
        // DB Creation Status
        uint16_t status = BLE_GAP_ERR_NO_ERROR;
        uint8_t user_lid = BLE_GATT_INVALID_USER_LID;
    
        do
        {
        #if (BLE_HL_MSG_API)
            if(p_cb == NULL)
            {
                p_cb = &(hogprh_msg_cb);
            }
        #endif // (BLE_HL_MSG_API)
    
            if(   (p_params == NULL) || (p_cb == NULL) || (p_cb->cb_enable_cmp == NULL)
               || (p_cb->cb_read_att_val_cmp == NULL) || (p_cb->cb_write_att_val_cmp == NULL)
               || (p_cb->cb_att_val_ntf_upd == NULL))
            {
                status = BLE_GAP_ERR_INVALID_PARAM;
                break;
            }
    
            // register HOGPRH user
            status = gatt_user_cli_register(HOGPRH_REPORT_MAP_MAX_LEN, user_prio, &hogprh_cb, &user_lid);
            if(status != BLE_GAP_ERR_NO_ERROR) break;
    
            if(p_hogprh_env != NULL)
            {
                // initialize environment variable
                p_hogprh_env->prf_env.p_cb    = p_cb;
                p_hogprh_env->user_lid = user_lid;
                for (conidx = 0; conidx < BLE_CONNECTION_MAX; conidx++)
                {
                    p_hogprh_env->p_env[conidx] = NULL;
                }
            }
        } while(0);
    
    
        if((status != BLE_GAP_ERR_NO_ERROR) && (user_lid != BLE_GATT_INVALID_USER_LID))
        {
            ble_gatt_user_unregister(user_lid);
        }
    
        return (status);
}

/**
 ****************************************************************************************
 * @brief Destruction of the HOGPRH module - due to a reset for instance.
 * This function clean-up allocated memory (attribute datahide is destroyed by another
 * procedure)
 *
 * @param[in|out]    env        Collector or Service allocated environment data.
 ****************************************************************************************
 */
static uint16_t hogprh_destroy(uint8_t reason)
{
    uint16_t status = BLE_GAP_ERR_NO_ERROR;
    //if(reason != BLE_PRF_DESTROY_RESET)
    {
        status = ble_gatt_user_unregister(p_hogprh_env->user_lid);
    }

    if(status == BLE_GAP_ERR_NO_ERROR)
    {
        uint8_t idx;

        // cleanup environment variable for each task instances
        for (idx = 0; idx < BLE_CONNECTION_MAX; idx++)
        {
            if (p_hogprh_env->p_env[idx] != NULL)
            {
                ke_free(p_hogprh_env->p_env[idx]);
            }
        }
    }

    return (status);
}

/**
 ****************************************************************************************
 * @brief Handles Disconnection
 *
 * @param[in]        conidx     Connection index
 * @param[in]        reason     Detach reason
 ****************************************************************************************
 */
static void hogprh_cleanup(uint8_t conidx, uint8_t reason)
{
    // clean-up environment variable allocated for task instance
    if (p_hogprh_env->p_env[conidx] != NULL)
    {
        ke_free(p_hogprh_env->p_env[conidx]);
        p_hogprh_env->p_env[conidx] = NULL;
    }
}

/*
 * GLOBAL FUNCTIONS DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialization of the HOGPRH module.
 *
 * @return status code to know if profile initialization succeed or not.
 ****************************************************************************************
 */
uint16_t ble_hogprh_init(const hogprh_cb_t* p_cb)
{
    uint8_t db_cfg_params = 0;
    uint16_t start_hdl = 0;
    return hogprh_init(&start_hdl, 0, 0, &db_cfg_params, p_cb);
}

/**
 ****************************************************************************************
 * @brief The function enables the HOGPRH.
 *
 * @return status code to know if profile enable succeed or not.
 ****************************************************************************************
 */
uint16_t ble_hogprh_enable(uint8_t conidx, uint8_t con_type, const hogprh_content_t* p_hogprh)
{
    hogprh_enable(conidx, con_type, 1, p_hogprh);
}

/**
 ****************************************************************************************
 * @brief Cleanup the HOGPRH module.
 *
 * @return None.
 ****************************************************************************************
 */
void ble_hogprh_cleanup(uint8_t conidx)
{
    hogprh_cleanup(conidx, 0);
}

/**
 ****************************************************************************************
 * @brief Destory the HOGPRH module.
 *
 * @return status code to know if profile destory succeed or not.
 ****************************************************************************************
 */
uint16_t ble_hogprh_destory(void)
{
    return hogprh_destroy(0);
}
/**
 ****************************************************************************************
 * @brief Read the HOGPRH module.
 *
 * @return None.
 ****************************************************************************************
 */
void ble_hogprh_read_val(uint8_t conidx, uint8_t hogprh_instance, uint16_t val_id, uint8_t report_idx)
{
    hogprh_read_val(conidx, hogprh_instance, val_id, report_idx);
}

/**
 ****************************************************************************************
 * @brief Read the HOGPRH module.
 *
 * @return None.
 ****************************************************************************************
 */
uint16_t ble_hogprh_write_val(uint8_t conidx, uint8_t hogprh_instance, uint16_t val_id, uint8_t report_idx,
                                uint16_t length, const uint8_t* p_data)
{
    return hogprh_write_val(conidx, hogprh_instance, val_id, report_idx, 0x01, length, p_data); //GATT_WRITE_NO_RESP
}

#endif /* (BLE_HID_REPORT_HOST) */

/// @} HOGPRH
