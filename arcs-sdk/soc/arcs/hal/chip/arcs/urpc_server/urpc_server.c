/*
 * urpc_server.c
 *
 *  Created on: Jan 30, 2016
 *      Author: rob
 */
#include <string.h>

#include "urpc_server.h"
#include "urpc.h"
#include "urpc_client.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#define URPC_ASYNC_QSIZE     16
#define URPC_SYNC_QSIZE      2
#define URPC_SYNC_TIMEOUT    500   //ms
#define URPC_SERVER_STACK_SIZE  configMINIMAL_STACK_SIZE

static QueueHandle_t async_queue, sync_queue;
static urpc_frame recv_frame __attribute__((aligned(32)));
static uint8_t server_state = URPC_STATE_RESET;
static uint8_t session_id = 0;
static urpc_cb *rpc_cb = NULL;

int8_t urpc_accept(urpc_server_stub* stub)
{
	if(server_state < URPC_STATE_IDLE) {
		return URPC_ERROR;
	}
	int8_t err = stub->accept((urpc_server*)NULL, URPC_CONN_SERVER, NULL);
	if(err == URPC_SUCCESS) {
		server_state = URPC_STATE_CONN;
	}
    return err;
}

static void urpc_sync_task_server(void *arg)
{
	urpc_frame frame;
	int8_t ret, chn;

	while(1) {
		if(xQueueReceive(sync_queue, &(frame), portMAX_DELAY) == pdPASS) { // receive a message
		    ((urpc_server_stub *)arg)->_peek(URPC_CONN_SERVER, (uint8_t *)&recv_frame, 0); // receive next package
		    ret = _urpc_handle_rpc(&frame, rpc_cb);
			if(ret) {
				// error handle
				chn = frame.header.eps;
				if(rpc_cb->endpoints[chn].handles[0].handler) {
					recv_frame.header.magic = ret;
					(rpc_cb->endpoints[chn].handles[0].handler)(&recv_frame);
				}
			}
		}
	}
}

static void urpc_async_task_server(void *arg)
{
	urpc_frame frame;
	int8_t ret, chn;

	while(1) {
		if(xQueueReceive(async_queue, &(frame), portMAX_DELAY) == pdPASS) { // receive a message
		    ((urpc_server_stub *)arg)->_peek(URPC_CONN_SERVER, (uint8_t *)&recv_frame, 0); // receive next package
			ret = _urpc_handle_rpc(&frame, rpc_cb);
			if(ret) {
				// error handle
				chn = frame.header.eps;
				if(rpc_cb->endpoints[chn].handles[0].handler) {
					recv_frame.header.magic = ret;
					(rpc_cb->endpoints[chn].handles[0].handler)(&recv_frame);
				}
			}
		}
	}
}

int8_t _urpc_recv_notify_server(urpc_server_stub* stub, urpc_connection* conn, uint8_t chn)
{
    int32_t resch, ret, err = 0;

	do {
		if(chn != recv_frame.header.eps) {
			err = URPC_INVALID_EP;
			break;
		}
		if(recv_frame.header.flags & URPC_FLAG_SYNC) {   // sync channel
			ret = xQueueSendFromISR(sync_queue, (uint8_t *)(&recv_frame), &resch);
		} else {   // async channel
			ret = xQueueSendFromISR(async_queue, (uint8_t *)(&recv_frame), &resch);
		}
		if(ret == pdTRUE) {
			portYIELD_FROM_ISR(resch);
		} else {
			err = URPC_QUE_FULL;
		}
	} while(0);

	if(err) {
		if(rpc_cb->endpoints[chn].handles[0].handler) {
			recv_frame.header.magic = err;
			(rpc_cb->endpoints[chn].handles[0].handler)(&recv_frame);
		}
	}
	// receive next one
	if(recv_frame.header.flags & URPC_FLAG_SYNC) {
		ret = xQueueIsQueueFullFromISR(sync_queue);
	} else {
		ret = xQueueIsQueueFullFromISR(async_queue);
	}

	if(!ret) {
		urpc_recv((urpc_stub*)stub, URPC_CONN_SERVER, &recv_frame);
	}
	return ret;
}

int8_t urpc_send_sync_server(urpc_server_stub* stub, urpc_frame* frame)
{
	int8_t err;

	//session_id should be the same as request, do not change it in frame
	frame->header.flags = URPC_FLAG_NOERR | URPC_FLAG_RESPONSE | URPC_FLAG_SYNC;
	err = urpc_send_check(rpc_cb, frame);
	if(err) {
		return err;
	}

    return urpc_send((urpc_stub*)stub, URPC_CONN_SERVER, frame);
}

int8_t urpc_send_async_server(urpc_server_stub* stub, urpc_frame* frame)
{
	int8_t err;
	taskENTER_CRITICAL();
	frame->header.session = ++session_id;
	taskEXIT_CRITICAL();
	frame->header.flags = URPC_FLAG_NOERR | URPC_FLAG_RESPONSE | URPC_FLAG_ASYNC;
	err = urpc_send_check(rpc_cb, frame);
	if(err) {
		return err;
	}

    return urpc_send((urpc_stub*)stub, URPC_CONN_SERVER, frame);
}

int8_t urpc_push_event_server(urpc_server_stub* stub, urpc_frame* frame)
{
	int8_t err;
	taskENTER_CRITICAL();
	frame->header.session = ++session_id;
	taskEXIT_CRITICAL();
	frame->header.flags = URPC_FLAG_PUSH | URPC_FLAG_RESPONSE | URPC_FLAG_ASYNC;
	err = urpc_send_check(rpc_cb, frame);
	if(err) {
		return err;
	}

    return urpc_send((urpc_stub*)stub, URPC_CONN_SERVER, frame);
}

int8_t urpc_init_server(urpc_server_stub* stub, urpc_cb *ctrl_block)
{
	// Update to use dynamic creation; do not revert to the original static version.

	// static StackType_t xStack[URPC_SERVER_STACK_SIZE], xStack_a[URPC_SERVER_STACK_SIZE];
	// static StaticTask_t xTaskBuffer, xTaskBuffer_a;

	server_state = URPC_STATE_RESET;
	if(urpc_init(ctrl_block)) {
		return URPC_ERROR;
	}
	rpc_cb = ctrl_block;
	async_queue = xQueueCreate(URPC_ASYNC_QSIZE, sizeof(urpc_frame));
	sync_queue = xQueueCreate(URPC_SYNC_QSIZE, sizeof(urpc_frame));

	xTaskCreate(urpc_sync_task_server, "rpc_sync_serv",
			URPC_SERVER_STACK_SIZE, stub, configMAX_PRIORITIES - 2, NULL);
	xTaskCreate(urpc_async_task_server, "rpc_async_serv",
			URPC_SERVER_STACK_SIZE, stub, configMAX_PRIORITIES - 1, NULL);

	urpc_recv((urpc_stub*)stub, URPC_CONN_SERVER, &recv_frame);
	server_state = URPC_STATE_IDLE;

    return stub->init_server((urpc_server*)NULL, ctrl_block->endpoints);
}
