/*
 * urpc_server_eth_udp.c
 *
 *  Created on: Jan 30, 2016
 *      Author: rob
 */

#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#include "Driver_MBX.h"
#include "urpc.h"
#include "urpc_mbox.h"
#include "urpc_mbox_server.h"
#include "urpc_server.h"

static void ping_handler(urpc_frame* frame);
static urpc_server_stub URPC_MBOX_SERVER_STUB;
static SemaphoreHandle_t send_sema;

static uint8_t state = 0;
static urpc_handle rpc_ep0_handles[] = {
		{NULL, URPC_FLAG_RESPONSE | URPC_FLAG_ASYNC, URPC_DESC("id0")},
		{ping_handler, URPC_FLAG_RESPONSE | URPC_FLAG_SYNC, URPC_DESC("ping")}
      };

static int8_t mbox_cb(uint32_t event, uint32_t param)
{
	BaseType_t resch;
	int8_t ret = 0;
	switch(event) {
	case CSK_MBX_EVENT_RECEIVE_COMPLETE:
		ret = _urpc_recv_notify_server((urpc_server_stub *)&URPC_MBOX_SERVER_STUB, URPC_CONN_SERVER, param);
		break;
	case CSK_MBX_EVENT_SEND_COMPLETE:
		xSemaphoreGiveFromISR(send_sema, &resch);
		portYIELD_FROM_ISR(resch);
		break;
	default:
//		CLOGD("rx err\n");
		break;
	}
	return ret;
}

int8_t urpc_mbox_send_server(urpc_connection* s_conn, uint8_t* buf, uint16_t chn)
{
	xSemaphoreTake(send_sema, portMAX_DELAY);
	return urpc_mbox_send(s_conn, buf, chn);
}

static void ping_handler(urpc_frame* frame)
{
	uint8_t id;

	if(frame->header.flags & URPC_FLAG_SYNC) {
		//response
		state = URPC_STATE_CONN;
		id = frame->rpc.dst_id;
		frame->rpc.dst_id = frame->rpc.src_id;
		frame->rpc.src_id = id;
		urpc_send_sync_server(&URPC_MBOX_SERVER_STUB, frame);
	} else {
		//error
		state = 0;
	}

}

urpc_server_stub* urpc_mbox_get_server_stub(void)
{
    return &URPC_MBOX_SERVER_STUB;
}

int8_t urpc_mbox_init_server(urpc_server* server, urpc_endpoint* s_endpoint)
{
	state = 0;
	send_sema = xSemaphoreCreateBinary();
	xSemaphoreGive(send_sema);
    //attach end point 0 handers
    s_endpoint[0].handles = rpc_ep0_handles;
    s_endpoint[0].handles_cnt = sizeof(rpc_ep0_handles)/sizeof(urpc_handle);
    MBX_Initialize(URPC_CONN_SERVER, mbox_cb);

    return URPC_SUCCESS;
}

int8_t urpc_mbox_accept(urpc_server* server, urpc_connection* conn, urpc_frame* frame)
{
    if(state) {
    	return URPC_SUCCESS;
    } else {
    	return URPC_ERROR;
    }
}

int8_t urpc_mbox_peek_server(urpc_connection* s_conn, uint8_t* buf, uint16_t chn)
{
	uint32_t arg;

	taskENTER_CRITICAL();
	MBX_Control(s_conn, CSK_MBX_CP_CTRL_GET_RX, &arg);
	if(!arg) {
		// receive next
		MBX_Receive(s_conn, buf, chn);
		MBX_Control(s_conn, CSK_MBX_CP_CTRL_NOTIFY, NULL);
	}
	taskEXIT_CRITICAL();
    return URPC_SUCCESS;
}

/* Note that this would be PROGMEM for embedded platformds */
static urpc_server_stub URPC_MBOX_SERVER_STUB = {
    ._send       = &urpc_mbox_send_server,
    ._peek       = &urpc_mbox_peek_server,
    ._recv       = &urpc_mbox_recv,
    .init_server = &urpc_mbox_init_server,
    .accept      = &urpc_mbox_accept,
};
