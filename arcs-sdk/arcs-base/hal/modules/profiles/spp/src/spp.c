/**
 ****************************************************************************************
 *
 * @file spp.h
 *
 * @brief Header file - Serial Port Profile implementation.
 *
 * Copyright (C) Listenai.com 2021
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup SPP
 * @ingroup Profile
 * @brief Serial Port Profile - Native API.
 *
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "btip_config.h"            // IP configuration

#include "../api/spp.h"             // Native API
#include "../api/spp_msg.h"         // SPP Message API

#include "ble_prf.h"

#include "sdp.h"                    // SDP api
#include "sdp_msg.h"                // SDP api
#include "rfcomm.h"                 // RFCOMM api
#include "rfcomm_msg.h"             // RFCOMM api

#include "ke_task.h"
#include "ke_mem.h"

#include "co_endian.h"              // Endianess
#include "co_utils.h"               // Read/Write macros

#include <string.h>                 // For memset


/*
 * DEFINES
 ****************************************************************************************
 */

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

typedef struct spp_port {
    /// connect index
    uint8_t conidx;
    /// port index
    uint8_t port;
    /// rfcomm channel
    uint8_t channel;
    /// rfcomm dlci
    uint8_t dlci;
    /// data callback
    spp_data_cb cb;
} spp_port_t;

typedef struct spp_env {
    /// profile environment
    prf_hdr_t prf_env;
    /// max connections
    uint8_t  max_con;
    /// serial port number base
    uint8_t  base_port;
    /// server enable
    uint8_t  svr_en;
    /// rfcomm channel, must greater than RFC_DYN_CHAN_BASE
    uint8_t  svr_chan;
    /// max packet size
    uint16_t packet_size;
    /// port list
    spp_port_t ports[__ARRAY_EMPTY];
} spp_env_t;


/*
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************************
 */

__STATIC uint32_t spp_sdp_callback(struct sdp_service *service, enum sdp_cb_id id);


/*
 * LOCAL VARIABLES
 ****************************************************************************************
 */

/// sdp data
// service name
__STATIC uint8_t spp_name[SPP_NAME_MAX_LEN] = "Serial Port";
// service uuid
__STATIC uint8_t spp_uuid[16] = {0x00, 0x00, 0x11, 0x01};

/// service class list
__STATIC const struct sdp_data spp_sdp_service[2] = {
        { .type = SDP_DE_UUID,                      .size = SDP_DE_SIZE_128,    .data = spp_uuid                },
};

/// handfree profile descriptor list
__STATIC const struct sdp_data spp_sdp_spp_profile[2] = {
        { .type = SDP_DE_UUID,          .size = SDP_DE_SIZE_16,     .value = BT_SERVICE_CLASS_SERIAL_PORT       },
        { .type = SDP_DE_UINT,          .size = SDP_DE_SIZE_16,     .value = 0x0102                             },
};

/// handfree profile descriptor list
__STATIC const struct sdp_data spp_sdp_spp[1] = {
        { .type = SDP_DE_DES,           .size = 2,                  .des = spp_sdp_spp_profile                  },
};


__STATIC const struct sdp_attribute spp_sdp_spp_attrs[] = {
        {.id   = BT_ATTRIBUTE_SERVICE_CLASS_ID_LIST,
         .data = { .type = SDP_DE_DES,                  .size = 1,              .des = spp_sdp_service          }},
        {.id   = BT_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST,
         .data = { .type = SDP_DE_DES,                  .size = 2,              .des = sdp_protocol_rfcomm      }},
        {.id   = BT_ATTRIBUTE_BROWSE_GROUP_LIST,
         .data = { .type = SDP_DE_DES,                  .size = 1,              .des = sdp_browse_list          }},
        {.id   = BT_ATTRIBUTE_LANGUAGE_BASE_ATTRIBUTE_ID_LIST,
         .data = { .type = SDP_DE_DES,                  .size = 3,              .des = sdp_lang                 }},
        {.id   = BT_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST,
         .data = { .type = SDP_DE_DES,                  .size = 1,              .des = spp_sdp_spp              }},
        {.id   = BT_ATTRIBUTE_SERVICE_NAME,
         .data = { .type = SDP_DE_STRING,               .size = 0,              .data = spp_name                }},
};

__STATIC struct sdp_service spp_sdp_service_spp =
{
        .cb  = spp_sdp_callback,
        .attr_count = sizeof(spp_sdp_spp_attrs)/sizeof(spp_sdp_spp_attrs[0]),
        .attrs = spp_sdp_spp_attrs,
};


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */



/*
 * LOCAL FUNCTION DEFINITIONS
 ****************************************************************************************
 */

__STATIC uint32_t spp_sdp_callback(struct sdp_service *service, enum sdp_cb_id id)
{
    uint32_t res = 0;

    switch(id)
    {
    case SDP_CB_HANDLE:
        res = service->hdl;
        break;
    case SDP_CB_RFCOMM_CHANNEL:
        {
            // Client environment
            spp_env_t* p_spp_env = PRF_ENV_GET(SPP, spp);
            if(p_spp_env != NULL)
                res = p_spp_env->svr_chan;
            else
                res = RFC_DYN_CHAN_BASE;
        }
        break;
    }
    return res;
}

__STATIC struct spp_port_t *spp_alloc_port(uint8_t conidx)
{
    uint8_t idx;
    spp_env_t* p_spp_env = PRF_ENV_GET(SPP, spp);
    spp_port_t *res = NULL;

    if(conidx < BLE_CONNECTION_MAX)
        return NULL;

    for(idx = 0; idx<p_spp_env->max_con; idx++)
    {
        if(p_spp_env->ports[idx].conidx == 0 && p_spp_env->ports[idx].dlci == 0)
        {
            p_spp_env->ports[idx].conidx = conidx;
            p_spp_env->ports[idx].dlci = 0;
            return &p_spp_env->ports[idx];
        }
    }
    return NULL;
}

__STATIC spp_port_t *spp_find_port(uint8_t conidx)
{
    uint8_t idx;
    spp_env_t* p_spp_env = PRF_ENV_GET(SPP, spp);
    spp_port_t *res = NULL;

    if(p_spp_env != NULL)
    {
        for(idx=0; idx<p_spp_env->max_con; idx++)
        {
            if(p_spp_env->ports[idx].conidx == conidx)
            {
                res = &p_spp_env->ports[idx];
                break;
            }
        }
    }
    return res;
}


__STATIC void spp_send_cmp_evt(uint8_t conidx, uint8_t port, uint16_t cmd, uint8_t result)
{
    struct spp_cmp_evt *evt = KE_MSG_ALLOC(SPP_CMP_EVT, PRF_DST_TASK(SPP), PRF_SRC_TASK(SPP), spp_cmp_evt);
    evt->port = port;
    evt->conidx = conidx;
    evt->cmd = cmd;
    evt->status = result;

    ke_msg_send(evt);
}

__STATIC void spp_send_conn_ind(uint8_t conidx, uint8_t port)
{
    struct spp_conn_ind *evt = KE_MSG_ALLOC(SPP_CONN_IND, PRF_DST_TASK(SPP), PRF_SRC_TASK(SPP), spp_conn_ind);
    evt->port = port;
    evt->conidx = conidx;

    ke_msg_send(evt);
}

__STATIC void spp_send_disc_ind(uint8_t conidx, uint8_t port)
{
    struct spp_disc_ind *evt = KE_MSG_ALLOC(SPP_DISC_IND, PRF_DST_TASK(SPP), PRF_SRC_TASK(SPP), spp_disc_ind);
    evt->port = port;
    evt->conidx = conidx;

    ke_msg_send(evt);

}

/// discovery sdp for channel
__STATIC void spp_sdp_disc_channel(uint8_t conidx, uint32_t uuid)
{
    struct sdp_search_req *req;

    req = (struct sdp_search_req*) ke_msg_alloc(SDP_SEARCH_SERVICE_REQ,
                                                TASK_SDP, PRF_SRC_TASK(SPP),
                                                sizeof(struct sdp_search_req) + sizeof(struct sdp_data) * 3);

    req->conidx = conidx;
    req->svc_handle = 0;

    req->ids[0].type = SDP_DE_UUID;
    req->ids[0].size = SDP_DE_SIZE_32;
    req->ids[0].value = uuid;

    /// get class id and pdl for rfcomm channel number
    req->svc_count = 1;
    req->attr_count = 2;
    req->ids[1].type = SDP_DE_UINT;
    req->ids[1].size = SDP_DE_SIZE_16;
    req->ids[1].value = BT_ATTRIBUTE_SERVICE_CLASS_ID_LIST;

    req->ids[2].type = SDP_DE_UINT;
    req->ids[2].size = SDP_DE_SIZE_16;
    req->ids[2].value = BT_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST;

    ke_msg_send(req);
}

/// create rfcomm connection
__STATIC void spp_conn_rfc(uint8_t conidx, uint8_t channel)
{
    struct rfc_chan_create_cmd *cmd;

    cmd = KE_MSG_ALLOC(RFC_CMD, TASK_RFCOMM, PRF_SRC_TASK(SPP), rfc_chan_create_cmd);

    cmd->code = RFC_CHAN_CREATE;
    cmd->conidx = conidx;
    cmd->dlci = 0;

    cmd->cfg.srv_chan = channel;
    cmd->cfg.flow_control = 1;
    cmd->cfg.init_credits = RFC_INIT_CREDIT;
    cmd->cfg.frame_size = RFC_DEFAULT_FRAME_SIZE;

    ke_msg_send(cmd);
}

/// accept rfcomm connect
__STATIC uint8_t spp_connect_accept(uint8_t conidx, uint8_t dlci, bool accept)
{
    struct rfc_chan_ack_cmd *cmd = KE_MSG_ALLOC(RFC_CMD, TASK_RFCOMM, PRF_SRC_TASK(SPP), rfc_chan_ack_cmd);

    cmd->code = RFC_CHAN_ACK;
    cmd->conidx = conidx;
    cmd->dlci = dlci;
    cmd->accept = accept;

    ke_msg_send(cmd);

    return GAP_ERR_NO_ERROR;
}

/// rfcomm data callback
__STATIC void spp_rfc_data_cb(uint8_t conidx, uint8_t dlci, co_buf_t* p_sdu)
{
    uint8_t  *pdu = co_buf_data(p_sdu);
    uint16_t  len = co_buf_data_len(p_sdu);
    uint16_t  idx, start;



}

/// rfcomm channel created
__STATIC void spp_link_created(uint8_t conidx, uint8_t dlci)
{
    spp_port_t *p_port = spp_find_port(conidx);

    if(p_port == NULL)
        p_port = spp_alloc_port(conidx);
    p_port->dlci = dlci;

    rfc_set_channel_cb(p_port->conidx, p_port->dlci, spp_rfc_data_cb);

    // send msc
    {
        struct rfc_chan_msc* cmd = KE_MSG_ALLOC(RFC_CMD, TASK_RFCOMM, PRF_SRC_TASK(SPP), rfc_chan_msc);

        cmd->code = RFC_CHAN_MSC;
        cmd->conidx = conidx;
        cmd->dlci = dlci;
        cmd->msc = 0x8d;

        ke_msg_send(cmd);
    }

    // send connected message
    spp_send_conn_ind(conidx, p_port->port);
}

/*
 * MESSAGE HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Handles reception of the @ref SDP_SEARCH_SERVICE_RSP message.
 * @param[in] msgid Id of the message received
 * @param[in] p_param Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance
 * @param[in] src_id ID of the sending task instance.
 * @return If the message was consumed or not.
 ****************************************************************************************
 */

__STATIC int spp_sdp_rsp_msg_handler(ke_msg_id_t const msgid, struct sdp_search_rsp *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    spp_port_t *p_port = spp_find_port(p_param->conidx);

    if(p_port != NULL)
    {
        if(p_port->channel != 0)
        {
            spp_conn_rfc(p_param->conidx, p_port->channel);
        }
        else
        {
            spp_send_cmp_evt(p_port->conidx, p_port->port, SPP_CONN_CMD, GAP_ERR_NOT_FOUND);
        }
    }

    return (KE_MSG_CONSUMED);
}

__STATIC int spp_sdp_res_msg_handler(ke_msg_id_t const msgid, struct sdp_search_res *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    if(p_param->attr_count >= 0)
    {
        spp_port_t *p_port = spp_find_port(p_param->conidx);
        struct sdp_attribute *attr;
        struct sdp_data *data;
        if(p_port != NULL)
        {
            attr = sdp_find_res_attr(p_param, BT_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
            // valid service
            if(attr != NULL && sdp_find_res_uuid(attr, co_ntohl(co_read32p(spp_uuid))) != NULL)
            {
                attr = sdp_find_res_attr(p_param, BT_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
                if(attr != NULL)
                {
                    data = sdp_find_res_uuid(attr, BT_PROTOCOL_RFCOMM);
                    if(data != NULL && (data+1)->type == SDP_DE_UINT)
                    {
                        p_port->channel = (data+1)->value;
                    }
                }
            }
        }
    }

    return (KE_MSG_CONSUMED);
}

__STATIC int spp_rfc_rsp_msg_handler(ke_msg_id_t const msgid, struct rfc_rsp *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    spp_port_t *p_port = spp_find_port(p_param->conidx);

    switch(p_param->code)
    {
        case RFC_CHAN_CREATE:
        {
            spp_send_cmp_evt(p_port->conidx, p_port->port, SPP_CONN_CMD, p_param->result);
        }
        break;
        case RFC_CHAN_DISC:
        {
            spp_send_cmp_evt(p_param->conidx, 0, SPP_DISC_CMD, p_param->result);
        }
        break;
        default:
        {
            if(p_port != NULL)
                spp_send_cmp_evt(p_port->conidx, p_port->port, SPP_RFC_CMD, p_param->result);
        }
        break;

    }

    return (KE_MSG_CONSUMED);
}


__STATIC int spp_rfc_ind_msg_handler(ke_msg_id_t const msgid, struct rfc_msg *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
     spp_env_t* p_spp_env = PRF_ENV_GET(SPP, spp);
     spp_port_t *p_port = spp_find_port(p_param->conidx);

     switch(p_param->code)
     {
         case RFC_CHAN_CON_IND:
         {
             if(p_port == NULL)
                 p_port = spp_alloc_port(p_param->conidx);

             if((!p_spp_env->svr_en) ||
                     (p_spp_env->svr_chan != (p_param->dlci>>1)) ||
                     (p_port == NULL) ||
                     (p_port->dlci != 0))
             {
                 /// server not enable or invalid channel number, or link have existed, reject connect
                 spp_connect_accept(p_param->conidx, p_param->dlci, false);
             }
             else
             {
                 spp_connect_accept(p_param->conidx, p_param->dlci, true);
             }
         }
         break;

         case RFC_CHAN_CREATED:
         {
             spp_link_created(p_param->conidx, p_param->dlci);
         }
         break;

         case RFC_CHAN_CLOSED:
         {
             if(p_port != NULL)
             {
                 spp_send_disc_ind(p_port->conidx, p_port->port);
                 p_port->dlci = 0;
                 p_port->conidx = 0;
             }
         }
         break;

         case RFC_DATA_IND:
         {
             // use callback, no come here
         }
         break;

         case RFC_CHAN_FLOW_IND:
         {

         }
         break;

         case RFC_CHAN_RPN_IND:
         {

         }
         break;

         default:
             break;
     }
    return (KE_MSG_CONSUMED);
}

__STATIC int spp_connect_cmd_handler(ke_msg_id_t const msgid, struct spp_conn_cmd *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    spp_env_t* p_spp_env = PRF_ENV_GET(SPP, spp);
    spp_port_t *p_port;
    uint16_t result = GAP_ERR_NO_ERROR;

    do{
        if(p_spp_env == NULL || p_param->port < p_spp_env->base_port || p_param->port >= p_spp_env->base_port + p_spp_env->max_con)
        {
            result = GAP_ERR_INVALID_PARAM;
            break;
        }

        p_port = &(p_spp_env->ports[p_param->port - p_spp_env->base_port]);

        if(p_port->conidx != 0)
        {
            result = GAP_ERR_COMMAND_DISALLOWED;
            break;
        }

        p_port->conidx = p_param->conidx;

        spp_sdp_disc_channel(p_param->conidx, co_ntohl(co_read32p(p_param->uuid)));

    }while(0);

    if(result != GAP_ERR_NO_ERROR)
        spp_send_cmp_evt(p_param->conidx, p_param->port, SPP_CONN_CMD, result);

    return (KE_MSG_CONSUMED);
}

__STATIC int spp_disconnect_cmd_handler(ke_msg_id_t const msgid, struct spp_disc_cmd *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    spp_port_t *p_port = spp_find_port(p_param->conidx);
    uint16_t result = GAP_ERR_NO_ERROR;

    if(p_port == NULL)
        result = GAP_ERR_INVALID_PARAM;
    else
    {
        struct rfc_msg *cmd;

        cmd = KE_MSG_ALLOC(RFC_CMD, TASK_RFCOMM, PRF_SRC_TASK(SPP), rfc_msg);

        cmd->code = RFC_CHAN_DISC;
        cmd->conidx = p_port->conidx;
        cmd->dlci = p_port->dlci;

        ke_msg_send(cmd);
    }

    if(result != GAP_ERR_NO_ERROR)
        spp_send_cmp_evt(p_param->conidx, p_param->port, SPP_DISC_CMD, result);

    return (KE_MSG_CONSUMED);
}

__STATIC int spp_send_data_cmd_handler(ke_msg_id_t const msgid, struct spp_data_msg *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    spp_port_t *p_port = spp_find_port(p_param->conidx);
    co_buf_t   *p_sdu = NULL;
    uint16_t    result = GAP_ERR_NO_ERROR;

    do
    {
        if(p_port == NULL)
        {
            result = GAP_ERR_INVALID_PARAM;
            break;
        }

        if(co_buf_alloc(&p_sdu, L2CAP_HEADER_LEN + RFC_HEAD_SIZE, p_param->length ,RFC_TAIL_SIZE) != CO_BUF_ERR_NO_ERROR)
        {
            result = GAP_ERR_INSUFF_RESOURCES;
            break;
        }

        // copy data
        memcpy(co_buf_data(p_sdu), p_param->data, p_param->length);

        // send data
        result = rfc_send_data(p_port->conidx, p_port->dlci, p_sdu);
    }
    while(0);

    if(result != GAP_ERR_NO_ERROR)
        spp_send_cmp_evt(p_param->conidx, p_param->port, SPP_DISC_CMD, result);

    return (KE_MSG_CONSUMED);
}

__STATIC int spp_send_rfc_cmd_handler(ke_msg_id_t const msgid, struct rfc_msg *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    spp_port_t *p_port = spp_find_port(p_param->conidx);
    struct rfc_chan_rpn *cmd;
    uint16_t    result = GAP_ERR_NO_ERROR;

    if(p_port == NULL)
        result = GAP_ERR_INVALID_PARAM;
    else
    {
        /// rpn is the longest rfc command
        cmd = KE_MSG_ALLOC(RFC_CMD, TASK_RFCOMM, PRF_SRC_TASK(SPP), rfc_chan_rpn);

        memcpy(cmd, p_param, sizeof(struct rfc_chan_rpn));
        cmd->conidx = p_port->conidx;
        cmd->dlci = p_port->dlci;

        ke_msg_send(cmd);
    }

    if(result != GAP_ERR_NO_ERROR)
        spp_send_cmp_evt(p_param->conidx, p_param->dlci, SPP_DISC_CMD, result);


    return (KE_MSG_CONSUMED);
}

__STATIC int spp_sdp_set_cmd_handler(ke_msg_id_t const msgid, struct spp_sdp_set *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{

    memcpy(spp_name, p_param->name, SPP_NAME_MAX_LEN);
    memcpy(spp_uuid, p_param->uuid, 16);

    spp_send_cmp_evt(0, 0, SPP_SDP_SET, GAP_ERR_NO_ERROR);

    return (KE_MSG_CONSUMED);
}

__STATIC int spp_default_msg_handler(ke_msg_id_t const msgid, void const *p_param,
                                     ke_task_id_t const dest_id, ke_task_id_t const src_id)
{

    return (KE_MSG_CONSUMED);
}

/// Default State handlers definition
KE_MSG_HANDLER_TAB(spp)
{
    // Note: all messages must be sorted in ID ascending order
    { SDP_SEARCH_SERVICE_RSP,                       (ke_msg_func_t) spp_sdp_rsp_msg_handler                 },
    { SDP_SEARCH_SERVICE_RES,                       (ke_msg_func_t) spp_sdp_res_msg_handler                 },

    { RFC_RSP,                                      (ke_msg_func_t) spp_rfc_rsp_msg_handler                 },
    { RFC_IND,                                      (ke_msg_func_t) spp_rfc_ind_msg_handler                 },

    { SPP_CONN_CMD,                                 (ke_msg_func_t) spp_connect_cmd_handler                 },
    { SPP_DISC_CMD,                                 (ke_msg_func_t) spp_disconnect_cmd_handler              },
    { SPP_SEND_DATA,                                (ke_msg_func_t) spp_send_data_cmd_handler               },
    { SPP_RFC_CMD,                                  (ke_msg_func_t) spp_send_rfc_cmd_handler                },

    { SPP_SDP_SET,                                  (ke_msg_func_t) spp_sdp_set_cmd_handler                 },

    { KE_MSG_DEFAULT_HANDLER,                       (ke_msg_func_t) spp_default_msg_handler                 },

};


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
__STATIC uint16_t spp_init(prf_data_t* p_env, uint16_t* p_start_hdl, uint8_t sec_lvl, uint8_t user_prio,
                            const struct spp_prf_cfg *p_params, const void* p_cb)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    uint8_t idx;

    do
    {
        spp_env_t* p_spp_env;

        //-------------------- allocate memory required for the profile  ---------------------
        p_spp_env = (spp_env_t*) ke_malloc(sizeof(spp_env_t) + sizeof(spp_port_t) * p_params->max_con, KE_MEM_ENV);

        if(p_spp_env != NULL)
        {
            memset(p_spp_env, 0, sizeof(spp_env_t) + sizeof(spp_port_t) * p_params->max_con);

            // allocate SPP required environment variable
            p_env->p_env = (prf_hdr_t *) p_spp_env;

            // initialize environment variable
            p_spp_env->max_con = p_params->max_con;
            p_spp_env->base_port = p_params->base_port;
            p_spp_env->svr_en = p_params->svr_en;
            p_spp_env->svr_chan = p_params->svr_chan;
            p_spp_env->packet_size = p_params->packet_size;

            for(idx=0; idx<p_params->max_con; idx++)
            {
                p_spp_env->ports[idx].port = p_params->base_port + idx;
            }

            // register sdp
            sdp_register_service(&spp_sdp_service_spp);

            // start rfcomm channel
            if(p_params->svr_en)
            {
                struct rfc_cfg cfg;

                cfg.srv_chan = p_params->svr_chan;
                cfg.frame_size = p_params->packet_size;
                cfg.flow_control = RFC_DEFAULT_FRAME_SIZE;
                cfg.init_credits = RFC_INIT_CREDIT;

                rfc_register_server_channel(&cfg, PRF_SRC_TASK(SPP));
            }

            // init profile message handler
            p_env->desc.msg_handler_tab  = spp_msg_handler_tab;
            p_env->desc.msg_cnt          = ARRAY_LEN(spp_msg_handler_tab);
        }
    } while(0);

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
__STATIC uint16_t spp_destroy(prf_data_t *p_env, uint8_t reason)
{
    uint16_t status = GAP_ERR_NO_ERROR;
    spp_env_t* p_spp_env = (spp_env_t*) p_env->p_env;

    // free profile environment variables
    p_env->p_env = NULL;
    ke_free(p_spp_env);

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
__STATIC void spp_con_create(prf_data_t *p_env, uint8_t conidx, const gap_con_param_t* p_con_param)
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
__STATIC void spp_con_cleanup(prf_data_t *p_env, uint8_t conidx, uint16_t reason)
{
    spp_port_t *p_port = spp_find_port(conidx);

    if(p_port != NULL)
    {
        p_port->conidx = 0;
        p_port->cb = NULL;
        p_port->dlci = 0;
    }

}


/*
 * GLOBAL FUNCTIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief set spp data receive callback
 *
 * @param[in] conidx    connection index
 * @param[in] port      Port id
 * @param[in] cb        data receive callback
 *
 * @return              error code
 ****************************************************************************************
 */
uint16_t spp_set_data_cb(uint8_t conidx, uint8_t port, spp_data_cb cb)
{

    return GAP_ERR_NO_ERROR;
}

/**
 ****************************************************************************************
 * @brief Send data to spp port
 *
 * @param[in] conidx    connection index
 * @param[in] port      Port id
 * @param[in] p_sdu     data to send
 *
 * @return              error code
 ****************************************************************************************
 */
uint16_t spp_send_data(uint8_t conidx, uint8_t port, co_buf_t* p_sdu)
{
    return GAP_ERR_NO_ERROR;
}


/// SPP Task interface required by profile manager
const prf_task_cbs_t spp_itf =
{
    .cb_init          = (prf_init_cb) spp_init,
    .cb_destroy       = spp_destroy,
    .cb_con_create    = spp_con_create,
    .cb_con_cleanup   = spp_con_cleanup,
    .cb_con_upd       = NULL,
};

/**
 ****************************************************************************************
 * @brief Retrieve client profile interface
 *
 * @return Client profile interface
 ****************************************************************************************
 */
const prf_task_cbs_t* spp_prf_itf_get(void)
{
    return &spp_itf;
}


/// @} SPP

