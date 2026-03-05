#ifndef __MRPC_BT_API_MSG_H__
#define __MRPC_BT_API_MSG_H__
#include <stddef.h>    // standard definitions
#include <stdint.h>    // standard integer definition
#include <stdbool.h>   // boolean definition
#include "ls_err.h"
#include "ls_bt_type.h"
#include "mrpc.h"
#include "btos_def.h"
#include "bt_ipc_api.h"

typedef struct
{
    struct mrpc_req_msg hdr;
    btos_task_id task_id;
    void * event;
    btos_msg_t * msg_body;
    uint32_t msg_body_len;
    TickType_t time_out;
    uint8_t data_buffer[];
} mrpc_btos_send_event_api_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_btos_send_event_api_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    btos_task_id task_id;
    uint16_t msg_id;
    void * msg_body;
    uint32_t msg_body_len;
    TickType_t time_out;
    uint8_t data_buffer[];
} mrpc_btos_send_at_evt_api_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_btos_send_at_evt_api_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    btos_task_id task_id;
    uint16_t msg_id;
    void * msg_body;
    uint32_t msg_body_len;
    TickType_t time_out;
    uint8_t data_buffer[];
} mrpc_btos_send_app_evt_api_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_btos_send_app_evt_api_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    struct out_bd_addr bd_addr;
} mrpc_ble_gap_set_loc_pub_addr_api_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_ble_gap_set_loc_pub_addr_api_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_llm_get_local_pub_addr_api_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    struct out_bd_addr bd_addr;
} mrpc_llm_get_local_pub_addr_api_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    uint32_t size;
} mrpc_btos_malloc_api_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
    void * buffer_ptr;
} mrpc_btos_malloc_api_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    void * ptr;
} mrpc_btos_free_api_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_btos_free_api_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_lsip_reset_api_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_lsip_reset_api_resp_t;

#endif //__MRPC_BT_API_MSG_H__