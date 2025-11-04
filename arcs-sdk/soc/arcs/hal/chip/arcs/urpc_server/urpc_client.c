/*
 * urpc_client.c
 *
 *  Created on: Feb 1, 2016
 *      Author: rob
 */
#include <string.h>
#include "urpc.h"
#include "urpc_client.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#define URPC_ASYNC_QSIZE     16
#define URPC_SYNC_QSIZE      2
#define URPC_SYNC_TIMEOUT    500   //ms
#define URPC_CLIENT_STACK_SIZE  2048

static QueueHandle_t async_queue, sync_queue;
static SemaphoreHandle_t sync_sema;	//, sync_sema;
static urpc_frame recv_frame;
static uint8_t client_state = URPC_STATE_RESET;
static uint8_t session_id = 0;
static urpc_cb *rpc_cb = NULL;

int8_t urpc_connect(urpc_client_stub* stub)
{
	int8_t err;
	if(client_state < URPC_STATE_IDLE) {
		return URPC_ERROR;
	}
    err = stub->connect((urpc_client* )NULL, URPC_CONN_CLIENT, NULL);
    if(err == 0) {
    	client_state = URPC_STATE_CONN;
    }
    return err;
}

static int8_t _urpc_wait_sync_client(uint32_t section_id, urpc_frame* frame)
{
	if(xQueueReceive(sync_queue, frame, URPC_SYNC_TIMEOUT * configTICK_RATE_HZ / 1000) == pdPASS) { // receive a message
		return URPC_SUCCESS;
	} else {
		return URPC_TIMEOUT;
	}
}

int8_t urpc_trans_sync_client(urpc_client_stub* stub, urpc_frame* send, urpc_frame* recv)
{
	int8_t err = URPC_SUCCESS, chn;

	if(client_state < URPC_STATE_IDLE) {
		return URPC_ERROR;
	}

	send->header.session = ++session_id;
	send->header.flags = URPC_FLAG_NOERR | URPC_FLAG_REQUEST | URPC_FLAG_SYNC;
	err = urpc_send_check(rpc_cb, send);
	if(err) {
		return err;
	}

	xSemaphoreTake(sync_sema, portMAX_DELAY);

	xQueueReset(sync_queue);  // delete invalid sync-frames
    err = urpc_send((urpc_stub*)stub, URPC_CONN_CLIENT, send);

    if(err == URPC_SUCCESS) {
    	err = _urpc_wait_sync_client(send->header.session, recv);
    	stub->_peek(URPC_CONN_CLIENT, (uint8_t *)&recv_frame, 0); // receive next package
    	xSemaphoreGive(sync_sema);
    	if(err == URPC_SUCCESS) {
    		err = urpc_recv_check(rpc_cb, recv);
			if(err == URPC_SUCCESS) {
				if(recv->header.session != send->header.session) {
					err = URPC_ERROR;
				}
			}
    	}
    	// error handler
    	if(err) {
			chn = send->header.eps;
			if(rpc_cb->endpoints[chn].handles && rpc_cb->endpoints[chn].handles[0].handler) {
				send->header.magic = err;
				(rpc_cb->endpoints[chn].handles[0].handler)(send);
			}
    	}
    } else {
    	xSemaphoreGive(sync_sema);
    }

    return err;
}

int8_t urpc_send_async_client(urpc_client_stub* stub, urpc_frame* frame)
{
	int8_t err;

	if(client_state < URPC_STATE_IDLE) {
		return URPC_ERROR;
	}
	frame->header.session = ++session_id;
	frame->header.flags = URPC_FLAG_NOERR | URPC_FLAG_REQUEST | URPC_FLAG_ASYNC;
	err = urpc_send_check(rpc_cb, frame);
	if(err) {
		return err;
	}

    return urpc_send((urpc_stub*)stub, URPC_CONN_CLIENT, frame);
}

int8_t urpc_open_event_client(urpc_client_stub* stub, urpc_frame* frame)
{
	int8_t err;

	if(client_state < URPC_STATE_IDLE) {
		return URPC_ERROR;
	}

	frame->header.session = ++session_id;
	frame->header.flags = URPC_FLAG_PUSH | URPC_FLAG_REQUEST | URPC_FLAG_ASYNC;
	err = urpc_send_check(rpc_cb, frame);
	if(err) {
		return err;
	}

    return urpc_send((urpc_stub*)stub, URPC_CONN_CLIENT, frame);
}

int8_t urpc_close_event_client(urpc_client_stub* stub, urpc_frame* frame)
{
	int8_t err;

	if(client_state < URPC_STATE_IDLE) {
		return URPC_ERROR;
	}

	frame->header.session = ++session_id;
	frame->header.flags = URPC_FLAG_NORM | URPC_FLAG_REQUEST | URPC_FLAG_ASYNC;
	err = urpc_send_check(rpc_cb, frame);
	if(err) {
		return err;
	}
    return urpc_send((urpc_stub*)stub, URPC_CONN_CLIENT, frame);
}



int8_t _urpc_recv_notify_client(urpc_client_stub* stub, urpc_connection* conn, uint8_t chn)
{
	BaseType_t resch, ret, err = 0;
	do {
		if(chn != recv_frame.header.eps) {
			err = URPC_INVALID_EP;
			break;
		}
		if(recv_frame.header.flags & URPC_FLAG_SYNC) {   // sync channel
			if(!uxQueueMessagesWaitingFromISR(sync_sema)) {
				ret = xQueueSendFromISR(sync_queue, (uint8_t *)(&recv_frame), &resch);
			} else {
				err = URPC_INVALID_SYNC;
				break;
			}
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
		urpc_recv((urpc_stub*)stub, URPC_CONN_CLIENT, &recv_frame);
	}
	return ret;
}

static void urpc_task_client(void *param)
{
	urpc_frame frame;
    int8_t ret, chn;
	while(1) {
		if(xQueueReceive(async_queue, &(frame), portMAX_DELAY) == pdPASS) { // receive a message
			((urpc_client_stub *)param)->_peek(URPC_CONN_CLIENT, (uint8_t *)&recv_frame, 0); // receive next package
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

int8_t urpc_init_client(urpc_client_stub* stub, urpc_cb *ctrl_block)
{
	static StackType_t xStack[URPC_CLIENT_STACK_SIZE];
	static StaticTask_t xTaskBuffer;
	client_state = URPC_STATE_RESET;
	if(urpc_init(ctrl_block)) {
		return URPC_ERROR;
	}
	rpc_cb = ctrl_block;
	async_queue = xQueueCreate(URPC_ASYNC_QSIZE, sizeof(urpc_frame));
	sync_queue = xQueueCreate(URPC_SYNC_QSIZE, sizeof(urpc_frame));
	sync_sema = xSemaphoreCreateBinary();
	xSemaphoreGive(sync_sema);  // set initial value as 1
//	xTaskCreate(urpc_task_client, "rpc_client", 2048, NULL, 23, NULL);
	xTaskCreateStatic(urpc_task_client, "rpc_client",
			URPC_CLIENT_STACK_SIZE, stub, configMAX_PRIORITIES-3, xStack, &xTaskBuffer);
	urpc_recv((urpc_stub*)stub, URPC_CONN_CLIENT, &recv_frame);
	client_state = URPC_STATE_IDLE;

    return stub->init_client((urpc_client*)NULL);
}

