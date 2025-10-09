/*
 * Copyright (c) 2020-2025 ChipSky Technology
 * All rights reserved.
 *
 */
#include <assert.h>
#include "cmn_mailbox_reg.h"
#include "Driver_MBX.h"

//#define MBX_AP_WORK
//#define MBX_CP_WORK
//#define MBX_SIM_WORK

//--------------------------------------------------------------
// for debugging

#define MBX_DEBUG

//--------------------------------------------------------------

#define CSK_MBX_CH(ch)            (0x1UL << (ch))

typedef struct
{
	CMN_MAILBOX_RegDef *mbx_cb;
	CSK_MBX_SignalEvent_t cb_event;
	uint32_t flags;
	uint32_t *rx_buf;  // (0~6) are virtual data channels, 7 as notified channel

	CSK_MBX_SignalEvent_t cb_event1;
	CSK_MBX_SignalEvent_t cb_event2;
	CSK_MBX_SignalEvent_t cb_event3;
	// for compact layout
	uint8_t flags1;
	uint8_t flags2;
	uint8_t flags3;
} MBX_DEV;


CSK_DRIVER_VERSION MBX_GetVersion()
{
	CSK_DRIVER_VERSION version = {1, 0};
    return version;
}

#ifdef MBX_SIM_WORK
#include "venus_ap.h"
#include "venus_log.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#define MBX_QSIZE     8
#define URPC_CONN_SERVER        0x10
#define URPC_CONN_CLIENT        0x20
#define MBX_LOCAL_SEND          0x1
#define MBX_REMOTE_SEND         0x2

typedef struct MBX_SIM {
	uint32_t REG_0;
	uint32_t REG_1;
	uint32_t REG_2;
	uint32_t REG_3;
	uint16_t CTRL;
	uint16_t IRQ;
}MBX_SIM;

static QueueHandle_t server_queue, client_queue;
static MBX_SIM server_box,client_box;

static MBX_DEV mbx_res_client = {
	((CMN_MAILBOX_RegDef *)NULL),
	((CSK_MBX_SignalEvent_t)NULL),
	0,
	NULL
};

static MBX_DEV mbx_res_server = {
	((CMN_MAILBOX_RegDef *)NULL),
	((CSK_MBX_SignalEvent_t)NULL),
	0,
	NULL
};

static void task_server(void *param)
{
	MBX_SIM *mbx_sim;
	uint32_t status, i, val;

	while(1) {
		if(xQueueReceive(server_queue, &val, portMAX_DELAY) == pdPASS) { // receive a message
			mbx_sim = (MBX_SIM *)val;
			switch(mbx_sim->CTRL) {
			case MBX_REMOTE_SEND:  //send from server task
				status = mbx_sim->IRQ;
				if(status & CSK_MBX_CH(15)) {
					mbx_sim->IRQ &= ~CSK_MBX_CH(15);
					status &= ~CSK_MBX_CH(15);
					mbx_res_server.cb_event(CSK_MBX_EVENT_SEND_COMPLETE, 15);
				}
				if(status) {
					switch(status) {
						case CSK_MBX_CH(0):  i = 0;  break;
						case CSK_MBX_CH(1):  i = 1;  break;
						case CSK_MBX_CH(2):  i = 2;  break;
						case CSK_MBX_CH(3):  i = 3;  break;
						case CSK_MBX_CH(4):  i = 4;  break;
						case CSK_MBX_CH(5):  i = 5;  break;
						case CSK_MBX_CH(6):  i = 6;  break;
						case CSK_MBX_CH(7):  i = 7;  break;
						case CSK_MBX_CH(8):  i = 8;  break;
						case CSK_MBX_CH(9):  i = 9;  break;
						case CSK_MBX_CH(10): i = 10; break;
						case CSK_MBX_CH(11): i = 11; break;
						case CSK_MBX_CH(12): i = 12; break;
						case CSK_MBX_CH(13): i = 13; break;
						case CSK_MBX_CH(14): i = 14; break;
						default:             i = 15; break;
					}
					if(i < 15) {
						mbx_sim->IRQ &= ~CSK_MBX_CH(i);  //clear irq bits
						if(mbx_res_server.rx_buf) {
							mbx_res_server.rx_buf[0] = mbx_sim->REG_0;
							mbx_res_server.rx_buf[1] = mbx_sim->REG_1;
							mbx_res_server.rx_buf[2] = mbx_sim->REG_2;
							mbx_res_server.rx_buf[3] = mbx_sim->REG_3;

							mbx_res_server.rx_buf = NULL;  //avoid data overwrite

							if(!mbx_res_server.cb_event(CSK_MBX_EVENT_RECEIVE_COMPLETE, i)) {
								mbx_sim->IRQ |= 0x1U << 15;
								xQueueSend(client_queue, (uint8_t *)(&val), 0);  //notify
							}
						}
					}
				}
				break;
			default:
				break;
			}
		}
	}
}

static void task_client(void *param)
{
	MBX_SIM *mbx_sim;
	uint32_t status, i, val;

	while(1) {
		if(xQueueReceive(client_queue, &val, portMAX_DELAY) == pdPASS) { // receive a message
			mbx_sim = (MBX_SIM *)val;
			switch(mbx_sim->CTRL) {
			case MBX_REMOTE_SEND:  //send from server task
				status = mbx_sim->IRQ;
				if(status & CSK_MBX_CH(15)) {
					mbx_sim->IRQ &= ~CSK_MBX_CH(15);
					status &= ~CSK_MBX_CH(15);
					mbx_res_client.cb_event(CSK_MBX_EVENT_SEND_COMPLETE, 15);
				}
				if(status) {
					switch(status) {
						case CSK_MBX_CH(0):  i = 0;  break;
						case CSK_MBX_CH(1):  i = 1;  break;
						case CSK_MBX_CH(2):  i = 2;  break;
						case CSK_MBX_CH(3):  i = 3;  break;
						case CSK_MBX_CH(4):  i = 4;  break;
						case CSK_MBX_CH(5):  i = 5;  break;
						case CSK_MBX_CH(6):  i = 6;  break;
						case CSK_MBX_CH(7):  i = 7;  break;
						case CSK_MBX_CH(8):  i = 8;  break;
						case CSK_MBX_CH(9):  i = 9;  break;
						case CSK_MBX_CH(10): i = 10; break;
						case CSK_MBX_CH(11): i = 11; break;
						case CSK_MBX_CH(12): i = 12; break;
						case CSK_MBX_CH(13): i = 13; break;
						case CSK_MBX_CH(14): i = 14; break;
						default:             i = 15; break;
					}
					if(i < 15) {
						mbx_sim->IRQ &= ~CSK_MBX_CH(i);  //clear irq bits
						if(mbx_res_client.rx_buf) {
							mbx_res_client.rx_buf[0] = mbx_sim->REG_0;
							mbx_res_client.rx_buf[1] = mbx_sim->REG_1;
							mbx_res_client.rx_buf[2] = mbx_sim->REG_2;
							mbx_res_client.rx_buf[3] = mbx_sim->REG_3;

							mbx_res_client.rx_buf = NULL;  //avoid data overwrite

							if(!mbx_res_client.cb_event(CSK_MBX_EVENT_RECEIVE_COMPLETE, i)) {
								mbx_sim->IRQ |= 0x1U << 15;
								xQueueSend(server_queue, (uint8_t *)(&val), 0);  //notify
							}
						}
					}
				}
				break;
			default:
				break;
			}
		}
	}
}

// export MBX API function: MBX_Initialize
int32_t MBX_Initialize(void *mbx_dev, CSK_MBX_SignalEvent_t cb_event)
{
	static StackType_t xStack_s[2048], xStack_c[2048];
	static StaticTask_t xTaskBuffer_s, xTaskBuffer_c;
	uint32_t type = (uint32_t)mbx_dev;
	if(type == URPC_CONN_SERVER) {
		mbx_res_server.flags |= MBX_FLAG_INITIALIZED;
		mbx_res_server.cb_event = cb_event;
		server_queue = xQueueCreate(MBX_QSIZE, sizeof(uint32_t));
//		xTaskCreate(task_server, "task_serv", 2048, NULL, 30, NULL);
		xTaskCreateStatic(task_server, "task_serv", 2048, NULL, 30, xStack_s, &xTaskBuffer_s);
		return CSK_DRIVER_OK;
	} else if(type == URPC_CONN_CLIENT) {
		mbx_res_client.flags |= MBX_FLAG_INITIALIZED;
		mbx_res_client.cb_event = cb_event;
		client_queue = xQueueCreate(MBX_QSIZE, sizeof(uint32_t));
//		xTaskCreate(task_client, "task_client", 2048, NULL, 30, NULL);
		xTaskCreateStatic(task_client, "task_client", 2048, NULL, 30, xStack_c, &xTaskBuffer_c);
		return CSK_DRIVER_OK;
	} else {
		return CSK_DRIVER_ERROR;
	}
}


// export MBX API function: MBX_Uninitialize
int32_t MBX_Uninitialize(void *mbx_dev)
{
	uint32_t type = (uint32_t)mbx_dev;
	if(type == URPC_CONN_SERVER) {
		mbx_res_server.flags = 0;
		return CSK_DRIVER_OK;
	} else if(type == URPC_CONN_CLIENT) {
		mbx_res_client.flags = 0;
		return CSK_DRIVER_OK;
	} else {
		return CSK_DRIVER_ERROR;
	}
}

// export MBX API function: MBX_Send
int32_t MBX_Send(void *mbx_dev, const void *data, uint32_t ch)
{
	uint32_t status, addr;

	if(ch > 14) {
		return CSK_DRIVER_ERROR_PARAMETER;
	}

    uint32_t type = (uint32_t)mbx_dev;
    if(type == URPC_CONN_SERVER) {
    	status = server_box.IRQ;
		if(status & 0x7fff) {
			return CSK_DRIVER_ERROR_BUSY;
		}
    	server_box.REG_0 = ((uint32_t *)data)[0];
    	server_box.REG_1 = ((uint32_t *)data)[1];
    	server_box.REG_2 = ((uint32_t *)data)[2];
    	server_box.REG_3 = ((uint32_t *)data)[3];
    	server_box.IRQ = 0x1 << ch;
    	server_box.CTRL = MBX_REMOTE_SEND;
    	addr = (uint32_t)&server_box;
    	xQueueSend(client_queue, (uint8_t *)(&addr), 0);
		return CSK_DRIVER_OK;
	} else if(type == URPC_CONN_CLIENT) {
		status = client_box.IRQ;
		if(status & 0x7fff) {
			return CSK_DRIVER_ERROR_BUSY;
		}
		client_box.REG_0 = ((uint32_t *)data)[0];
		client_box.REG_1 = ((uint32_t *)data)[1];
		client_box.REG_2 = ((uint32_t *)data)[2];
		client_box.REG_3 = ((uint32_t *)data)[3];
		client_box.IRQ = 0x1 << ch;
		client_box.CTRL = MBX_REMOTE_SEND;
		addr = (uint32_t)&client_box;
		xQueueSend(server_queue, (uint8_t *)(&addr), 0);
		return CSK_DRIVER_OK;
	} else {
		return CSK_DRIVER_ERROR;
	}
}


// export MBX API function: MBX_Receive
int32_t MBX_Receive(void *mbx_dev, void *data, uint32_t ch)
{
	uint32_t type = (uint32_t)mbx_dev;
	if(type == URPC_CONN_SERVER) {
		mbx_res_server.rx_buf = data;
		return CSK_DRIVER_OK;
	} else if(type == URPC_CONN_CLIENT) {
		mbx_res_client.rx_buf = data;
		return CSK_DRIVER_OK;
	} else {
		return CSK_DRIVER_ERROR;
	}
}


// export MBX API function: MBX_Control
int32_t MBX_Control(void *mbx_dev, uint32_t control, void *arg)
{
	MBX_SIM *mbx_sim;
	uint32_t val;

	switch(control) {
	case CSK_MBX_AP_CTRL_NOTIFY:
		val = (uint32_t)&server_box;
		mbx_sim = &server_box;
		if((mbx_sim->IRQ & CSK_MBX_CH(15)) == 0) {
			mbx_sim->IRQ |= 0x1U << 15;
			xQueueSend(server_queue, (uint8_t *)(&val), 0);  //notify
		}
		break;
	case CSK_MBX_AP_CTRL_GET_RX:
		*((uint32_t *)arg) = (uint32_t)mbx_res_client.rx_buf;
		break;
	case CSK_MBX_CP_CTRL_NOTIFY:
		val = (uint32_t)&client_box;
		mbx_sim = &client_box;
		if((mbx_sim->IRQ & CSK_MBX_CH(15)) == 0) {
			mbx_sim->IRQ |= 0x1U << 15;
			xQueueSend(client_queue, (uint8_t *)(&val), 0);  //notify
		}
		break;
	case CSK_MBX_CP_CTRL_GET_RX:
		*((uint32_t *)arg) = (uint32_t)mbx_res_server.rx_buf;
		break;
	default:
		break;
	}

	return CSK_DRIVER_OK;
}
#endif

#ifdef MBX_AP_WORK
#include "arcs_ap.h"
#include "log_print.h"

static MBX_DEV mbx_res = {
	((CMN_MAILBOX_RegDef *)CMN_MAILBOX_BASE),
	((CSK_MBX_SignalEvent_t)NULL),
	0,
	NULL,

	((CSK_MBX_SignalEvent_t)NULL),
	((CSK_MBX_SignalEvent_t)NULL),
	((CSK_MBX_SignalEvent_t)NULL),
	0,
	0,
	0,
};

#ifdef MBX_DEBUG
volatile int g_mbx_isr_entry_counter = 0;
volatile int g_mbx_irq_event_counter = 0;
#endif

static void mbx_irq_handler0(void)
{
	uint32_t status, i, val = 0;

#ifdef MBX_DEBUG
	g_mbx_isr_entry_counter++;
#endif

	// channel 0-7, 寄存器 00-07
	// channel {1-6, 7}/urpc 可能同时存在, 1-6 由urpc同步机制保证只会出现1个
	status = mbx_res.mbx_cb->REG_CP_MAILBOX_IRQ.all & 0x000000ff;

	// sub interrupt handler: 处理 channel 7 中断
	if(status & CSK_MBX_CH(7)) {  // send completely, notified interrupt
		mbx_res.mbx_cb->REG_CP_MAILBOX_IRQ.all = CSK_MBX_CH(7);  //clear irq bits
//		__COMPILER_BARRIER();
		mbx_res.cb_event(CSK_MBX_EVENT_SEND_COMPLETE, 7);
		status &= ~CSK_MBX_CH(7);

#ifdef MBX_DEBUG
		g_mbx_irq_event_counter++;
#endif
	}

	// sub interrupt handler: 处理 channel 1-6 中断
	if(status) {  // data receive interrupt
		switch(status) {
			case CSK_MBX_CH(0):  i = 0;  break;
			case CSK_MBX_CH(1):  i = 1;  break;
			case CSK_MBX_CH(2):  i = 2;  break;
			case CSK_MBX_CH(3):  i = 3;  break;
			case CSK_MBX_CH(4):  i = 4;  break;
			case CSK_MBX_CH(5):  i = 5;  break;
			case CSK_MBX_CH(6):  i = 6;  break;
			default:             i = 7; break;
		}
		if(i < 7) {
//			__COMPILER_BARRIER();
			mbx_res.mbx_cb->REG_CP_MAILBOX_IRQ.all = 0x1U << i;  //clear irq bits
//			__COMPILER_BARRIER();

			if(mbx_res.rx_buf) {
				mbx_res.rx_buf[0] = mbx_res.mbx_cb->REG_CP_MAILBOX_WORD0.all;
				mbx_res.rx_buf[1] = mbx_res.mbx_cb->REG_CP_MAILBOX_WORD1.all;
				mbx_res.rx_buf[2] = mbx_res.mbx_cb->REG_CP_MAILBOX_WORD2.all;
				mbx_res.rx_buf[3] = mbx_res.mbx_cb->REG_CP_MAILBOX_WORD3.all;

				mbx_res.rx_buf = NULL;  //avoid data overwrite
				if(!mbx_res.cb_event(CSK_MBX_EVENT_RECEIVE_COMPLETE, i)) {
					// call back successful, notify CP to send next package
					mbx_res.mbx_cb->REG_AP_MAILBOX_SET.all = 0x1U << 7;  //notify

#ifdef MBX_DEBUG
					g_mbx_irq_event_counter++;
#endif
				}
			}
		}
	}
}

static void mbx_irq_handler1(void)
{
	uint32_t status;
    int32_t i;

	// channel 8-15, 寄存器 10-17
	status = mbx_res.mbx_cb->REG_CP_MAILBOX_IRQ.all & 0x0000ff00;

    for (i = 8; i < 16; i++) {
        if (status & (0x1U << i)) {
            mbx_res.cb_event1(CSK_MBX_EVENT_RECEIVE_COMPLETE, i);
            // irq 寄存器中清除 channel 14, write 1 bit to clear 1 bit
            mbx_res.mbx_cb->REG_CP_MAILBOX_IRQ.all = 0x1U << i;  //clear irq bits
        }
    }
}

// TODO:  
// 对于已经初始化完毕的MBX, 是否单独增加cb_event_fast?  
// 场景: urpc共存场景

// export MBX API function: MBX_Initialize
int32_t MBX_Initialize(void* mbx_dev, CSK_MBX_SignalEvent_t cb_event)
{
	if((mbx_res.flags & MBX_FLAG_INITIALIZED) == 0U) {
		// channel 0-7
		uint32_t val = 0xff;
		MBX_Control(NULL, CSK_MBX_AP_CTRL_SET_CHS, &val);
		mbx_res.cb_event = cb_event;

//		mbx_res.usr_param = usr_param;
		mbx_res.flags |= MBX_FLAG_INITIALIZED;
		register_ISR(IRQ_MAILBOX_0_VECTOR, mbx_irq_handler0, NULL);
		//clear interrupt bits, and enable, channel 0-7, W1C
		mbx_res.mbx_cb->REG_CP_MAILBOX_IRQ.all = val;
		enable_IRQ(IRQ_MAILBOX_0_VECTOR);
	}

    return CSK_DRIVER_OK;
}

int32_t MBX1_Initialize(void* mbx_dev, CSK_MBX_SignalEvent_t cb_event)
{
	if((mbx_res.flags1 & MBX_FLAG_INITIALIZED) == 0U) {
		// TODO: 暂时仅用 channel 14
		// channel 8-15
		uint32_t val = 0xff00;
		MBX_Control(NULL, CSK_MBX_AP_CTRL_SET_CHS, &val);
		mbx_res.cb_event1 = cb_event;

		mbx_res.flags1 |= MBX_FLAG_INITIALIZED;
		register_ISR(IRQ_MAILBOX_1_VECTOR, mbx_irq_handler1, NULL);
		//clear interrupt bits, and enable, channel 8-15, W1C
		mbx_res.mbx_cb->REG_CP_MAILBOX_IRQ.all = val;
		enable_IRQ(IRQ_MAILBOX_1_VECTOR);
	}

	return CSK_DRIVER_OK;
}

// export MBX API function: MBX_Uninitialize
int32_t MBX_Uninitialize(void *mbx_dev)
{
	mbx_res.flags = 0;
    return CSK_DRIVER_OK;
}

int32_t MBX1_Uninitialize(void *mbx_dev)
{
	mbx_res.flags1 = 0;
    return CSK_DRIVER_OK;
}

//--------------------------------------------------------------

static void mbx_irq_handler2(void)
{
	uint32_t status;

	// channel 16-23, 寄存器 20-27
	status = mbx_res.mbx_cb->REG_CP_MAILBOX_IRQ.all & 0x00ff0000;

	//	__COMPILER_BARRIER();

	//clear irq bits
	mbx_res.mbx_cb->REG_CP_MAILBOX_IRQ.all = status;

	//	__COMPILER_BARRIER();

	mbx_res.cb_event2(CSK_MBX_EVENT_INTERRUPTS, status);
}

int32_t MBX2_Initialize(void* mbx_dev, CSK_MBX_SignalEvent_t cb_event)
{
	if((mbx_res.flags2 & MBX_FLAG_INITIALIZED) == 0U) {
		// channel 16-23, 寄存器 20-27
		uint32_t val = 0xff0000;
		MBX_Control(NULL, CSK_MBX_AP_CTRL_SET_CHS, &val);
		mbx_res.cb_event2 = cb_event;

		mbx_res.flags2 |= MBX_FLAG_INITIALIZED;
		register_ISR(IRQ_MAILBOX_2_VECTOR, mbx_irq_handler2, NULL);
		//clear interrupt bits, and enable, channel 16-23, W1C
		mbx_res.mbx_cb->REG_CP_MAILBOX_IRQ.all = val;
		enable_IRQ(IRQ_MAILBOX_2_VECTOR);
	}

	return CSK_DRIVER_OK;
}

int32_t MBX2_Uninitialize(void *mbx_dev)
{
	mbx_res.flags2 = 0;
    return CSK_DRIVER_OK;
}

//--------------------------------------------------------------

static void mbx_irq_handler3(void)
{
	uint32_t status;

	// channel 24-31, 寄存器 30-37
	status = mbx_res.mbx_cb->REG_CP_MAILBOX_IRQ.all & 0xff000000;

	//	__COMPILER_BARRIER();

	//clear irq bits
	mbx_res.mbx_cb->REG_CP_MAILBOX_IRQ.all = status;

	//	__COMPILER_BARRIER();

	mbx_res.cb_event3(CSK_MBX_EVENT_INTERRUPTS, status);
}

int32_t MBX3_Initialize(void* mbx_dev, CSK_MBX_SignalEvent_t cb_event)
{
	if((mbx_res.flags3 & MBX_FLAG_INITIALIZED) == 0U) {
		// channel 24-31, 寄存器 30-37
		uint32_t val = 0xff000000;
		MBX_Control(NULL, CSK_MBX_AP_CTRL_SET_CHS, &val);
		mbx_res.cb_event3 = cb_event;

		mbx_res.flags3 |= MBX_FLAG_INITIALIZED;
		register_ISR(IRQ_MAILBOX_3_VECTOR, mbx_irq_handler3, NULL);
		//clear interrupt bits, and enable, channel 16-23, W1C
		mbx_res.mbx_cb->REG_CP_MAILBOX_IRQ.all = val;
		enable_IRQ(IRQ_MAILBOX_3_VECTOR);
	}

	return CSK_DRIVER_OK;
}

int32_t MBX3_Uninitialize(void *mbx_dev)
{
	mbx_res.flags3 = 0;
    return CSK_DRIVER_OK;
}

//--------------------------------------------------------------

// 只用于urpc的数据发送
// export MBX API function: MBX_Send
int32_t MBX_Send(void *mbx_dev, const void *data, uint32_t ch)
{
	uint32_t status;

	if(ch > 6) {
		return CSK_DRIVER_ERROR_PARAMETER;
	}
	status = mbx_res.mbx_cb->REG_AP_MAILBOX_IRQ.all;
	// channel 0-6 mask
	if(status & 0x7f) {
		return CSK_DRIVER_ERROR_BUSY;
	}
	mbx_res.mbx_cb->REG_AP_MAILBOX_WORD0.all = ((uint32_t *)data)[0];
	mbx_res.mbx_cb->REG_AP_MAILBOX_WORD1.all = ((uint32_t *)data)[1];
	mbx_res.mbx_cb->REG_AP_MAILBOX_WORD2.all = ((uint32_t *)data)[2];
	mbx_res.mbx_cb->REG_AP_MAILBOX_WORD3.all = ((uint32_t *)data)[3];

	// memory barrier: write_release
	__DMB();

	// 只触发单个channel中断, write 1 bit to pulse.
	mbx_res.mbx_cb->REG_AP_MAILBOX_SET.all = 0x1 << ch;
    return CSK_DRIVER_OK;
}

// thread-safe: need not be in lock, only an atomic write 1 bit to pulse
int32_t MBX_Trigger(void *mbx_dev, uint32_t ch)
{
	uint32_t status;

	// read to check 1 bit, thread-safe
	status = mbx_res.mbx_cb->REG_AP_MAILBOX_IRQ.all;
	if(status & (0x1U << ch) ) {
		return CSK_DRIVER_ERROR_BUSY;
	}

	// 不用, 使用者决定 memory barrier: write_release
	// __DMB();

	// 只触发单个channel中断, write 1 bit to pulse.
	mbx_res.mbx_cb->REG_AP_MAILBOX_SET.all = 0x1 << ch;
    return CSK_DRIVER_OK;
}

// 使能单个 channel 中断
// not thread-safe, 须使用者保证独占.
// !!! 当只有一个fast interrupt, 使用场景确保了race condition不存在.
// !!! 当多个fast Interrupt, 需要使用者逻辑确保 无 race condition.
// 不做层数嵌套管理
int32_t MBX_EnableInterrupt_l(void *mbx_dev, uint32_t ch)
{
	(void) mbx_dev;

	// 获取原 32 bit enable, 更新置位 1, 写回. 属性: RW
	// not thread-safe, 多个读写, 须使用者保证独占
	// TODO: 当rtos环境下, 此处内置互斥.
	uint32_t enable = mbx_res.mbx_cb->REG_CP_MAILBOX_CTRL.all;
	enable |= CSK_MBX_CH(ch);
	mbx_res.mbx_cb->REG_CP_MAILBOX_CTRL.all = enable;

    return CSK_DRIVER_OK;
}

// 禁单个 channel 中断
// not thread-safe, 须使用者保证独占.
// !!! 当只有一个fast interrupt, 使用场景确保了race condition不存在.
// !!! 当多个fast Interrupt, 需要使用者逻辑确保 无 race condition.
// 不做层数嵌套管理
int32_t MBX_DisableInterrupt_l(void *mbx_dev, uint32_t ch)
{
	(void) mbx_dev;

	// 获取原 32 bit enable, 更新置位 0, 写回. 属性: RW
	// not thread-safe, 多个读写, 须使用者保证独占
	// TODO: 当rtos环境下, 此处内置互斥.
	uint32_t enable = mbx_res.mbx_cb->REG_CP_MAILBOX_CTRL.all;
	enable &= ~CSK_MBX_CH(ch);
	mbx_res.mbx_cb->REG_CP_MAILBOX_CTRL.all = enable;

    return CSK_DRIVER_OK;
}

// get address of the mutual exclusive shared register
volatile uint32_t* MBX_GetAddrOfSharedExRegister(void *mbx_dev, int id)
{
	(void) mbx_dev;
	(void) id;

	volatile uint32_t* mutexRegsBase = &(mbx_res.mbx_cb->REG_MAIL_FLAG_00.all);

	return &mutexRegsBase[id];
}

// export MBX API function: MBX_Receive
int32_t MBX_Receive(void *mbx_dev, void *data, uint32_t ch)
{
	mbx_res.rx_buf = data;
    return CSK_DRIVER_OK;
}


// export MBX API function: MBX_Control
int32_t MBX_Control(void *mbx_dev, uint32_t control, void *arg)
{
	switch(control) {
	case CSK_MBX_AP_CTRL_SET_CHS:
		// shared by MBX0/1/2/3, 8 bit/MBX, each assures legal channel args.
		mbx_res.mbx_cb->REG_AP_MAILBOX_CTRL.all |= *((uint32_t *)arg);
		break;
	case CSK_MBX_AP_CTRL_GET_CHS:
		// 32 bit enable
		*((uint32_t *)arg) = mbx_res.mbx_cb->REG_AP_MAILBOX_CTRL.all;
		break;
	case CSK_MBX_AP_CTRL_SET_LK:
		if(*((uint32_t *)arg)) { // set lock
			if((mbx_res.mbx_cb->REG_AP_MAILBOX_LOCK.all & 0x1) == 0) {
				mbx_res.mbx_cb->REG_AP_MAILBOX_LOCK.all = 0x5A5A;
			}
		} else { // set unlock
			if(mbx_res.mbx_cb->REG_AP_MAILBOX_LOCK.all & 0x1) {
				mbx_res.mbx_cb->REG_AP_MAILBOX_LOCK.all = 0x5A5A;
			}
		}
		break;
	case CSK_MBX_AP_CTRL_GET_LK:
		*((uint32_t *)arg) = mbx_res.mbx_cb->REG_AP_MAILBOX_LOCK.all & 0x1;
		break;
	case CSK_MBX_AP_CTRL_NOTIFY:
		// 当 irq 空闲时,
		if((mbx_res.mbx_cb->REG_AP_MAILBOX_IRQ.all & CSK_MBX_CH(7)) == 0) {
			// 只触发单个channel中断, write 1 bit to pulse.
			mbx_res.mbx_cb->REG_AP_MAILBOX_SET.all = 0x1U << (7);  //notify
		}
		break;
	case CSK_MBX_AP_CTRL_GET_RX:
		*((uint32_t *)arg) = (uint32_t)mbx_res.rx_buf;
		break;
	default:
		return CSK_DRIVER_ERROR_PARAMETER;
		break;
	}

    return CSK_DRIVER_OK;
}
#endif

#ifdef MBX_CP_WORK
#include "arcs_ap.h"

static MBX_DEV mbx_res = {
	((CMN_MAILBOX_RegDef *)CMN_MAILBOX_BASE),
	((CSK_MBX_SignalEvent_t)NULL),
	0,
	NULL,

	((CSK_MBX_SignalEvent_t)NULL),
	((CSK_MBX_SignalEvent_t)NULL),
	((CSK_MBX_SignalEvent_t)NULL),
	0,
	0,
	0,
};

#ifdef MBX_DEBUG
volatile int g_mbx_isr_entry_counter = 0;
volatile int g_mbx_irq_event_counter = 0;
#endif

static void mbx_irq_handler0(void)
{
	uint32_t status, i, val = 0;

#ifdef MBX_DEBUG
	g_mbx_isr_entry_counter++;
#endif

	// channel 0-7, 寄存器 00-07
	// channel {1-6, 7}/urpc 可能同时存在, 1-6 由urpc同步机制保证只会出现1个
	status = mbx_res.mbx_cb->REG_AP_MAILBOX_IRQ.all & 0x000000ff;

	// sub interrupt handler: 处理 channel 7 中断
	if(status & CSK_MBX_CH(7)) {  // send completely, notified interrupt
		mbx_res.mbx_cb->REG_AP_MAILBOX_IRQ.all = CSK_MBX_CH(7);  //clear irq bits
//		__COMPILER_BARRIER();
		mbx_res.cb_event(CSK_MBX_EVENT_SEND_COMPLETE, 7);
		status &= ~CSK_MBX_CH(7);

#ifdef MBX_DEBUG
		g_mbx_irq_event_counter++;
#endif
	}

	// sub interrupt handler: 处理 channel 1-13 中断
	if(status) {  // data receive interrupt
		switch(status) {
			case CSK_MBX_CH(0):  i = 0;  break;
			case CSK_MBX_CH(1):  i = 1;  break;
			case CSK_MBX_CH(2):  i = 2;  break;
			case CSK_MBX_CH(3):  i = 3;  break;
			case CSK_MBX_CH(4):  i = 4;  break;
			case CSK_MBX_CH(5):  i = 5;  break;
			case CSK_MBX_CH(6):  i = 6;  break;
			default:             i = 7; break;
		}
		if(i < 7) {
//			__COMPILER_BARRIER();
			mbx_res.mbx_cb->REG_AP_MAILBOX_IRQ.all = 0x1U << i;  //clear irq bits
//			__COMPILER_BARRIER();
			if(mbx_res.rx_buf) {
				mbx_res.rx_buf[0] = mbx_res.mbx_cb->REG_AP_MAILBOX_WORD0.all;
				mbx_res.rx_buf[1] = mbx_res.mbx_cb->REG_AP_MAILBOX_WORD1.all;
				mbx_res.rx_buf[2] = mbx_res.mbx_cb->REG_AP_MAILBOX_WORD2.all;
				mbx_res.rx_buf[3] = mbx_res.mbx_cb->REG_AP_MAILBOX_WORD3.all;

				mbx_res.rx_buf = NULL;  //avoid data overwrite
				if(!mbx_res.cb_event(CSK_MBX_EVENT_RECEIVE_COMPLETE, i)) {
					// call back successful, notify AP to send next package
					mbx_res.mbx_cb->REG_CP_MAILBOX_SET.all = 0x1U << 7;  //notify

#ifdef MBX_DEBUG
					g_mbx_irq_event_counter++;
#endif
				}
			}
		}
	}
}

static void mbx_irq_handler1(void)
{
	uint32_t status;
    int32_t i;

	// channel 8-15, 寄存器 10-17
	status = mbx_res.mbx_cb->REG_AP_MAILBOX_IRQ.all & 0x0000ff00;

	for (i = 8; i < 16; i++) {
		if (status & (0x1U << i)) {
			mbx_res.cb_event1(CSK_MBX_EVENT_RECEIVE_COMPLETE, i);

			// irq 寄存器中清除 channel 14, write 1 bit to clear 1 bit
			mbx_res.mbx_cb->REG_AP_MAILBOX_IRQ.all = 0x1U << i;  //clear irq bits
		}
    }

}

// export MBX API function: MBX_Initialize
int32_t MBX_Initialize(void* mbx_dev, CSK_MBX_SignalEvent_t cb_event)
{
	if((mbx_res.flags & MBX_FLAG_INITIALIZED) == 0U) {
		// channel 0-7
		uint32_t val = 0xff;
		MBX_Control(NULL, CSK_MBX_CP_CTRL_SET_CHS, &val);
		mbx_res.cb_event = cb_event;

//		mbx_res.usr_param = usr_param;
		mbx_res.flags |= MBX_FLAG_INITIALIZED;
		register_ISR(IRQ_MAILBOX_0_VECTOR, mbx_irq_handler0, NULL);
		//clear interrupt bits, and enable, channel 0-7, W1C
		mbx_res.mbx_cb->REG_AP_MAILBOX_IRQ.all = val;
		enable_IRQ(IRQ_MAILBOX_0_VECTOR);
	}

    return CSK_DRIVER_OK;
}

int32_t MBX1_Initialize(void* mbx_dev, CSK_MBX_SignalEvent_t cb_event)
{
	if((mbx_res.flags1 & MBX_FLAG_INITIALIZED) == 0U) {
		// TODO: 暂时仅用 channel 14
		// channel 8-15
		uint32_t val = 0xff00;
		MBX_Control(NULL, CSK_MBX_CP_CTRL_SET_CHS, &val);
		mbx_res.cb_event1 = cb_event;

		mbx_res.flags1 |= MBX_FLAG_INITIALIZED;
		register_ISR(IRQ_MAILBOX_1_VECTOR, mbx_irq_handler1, NULL);
		//clear interrupt bits, and enable, channel 8-15, W1C
		mbx_res.mbx_cb->REG_AP_MAILBOX_IRQ.all = val;
		enable_IRQ(IRQ_MAILBOX_1_VECTOR);

		// 互斥量 clean: 仅 CP(主) 执行一次, 硬件上电默认 = 0
		volatile uint32_t* mutexRegsBase = &(mbx_res.mbx_cb->REG_MAIL_FLAG_00.all);

		for (size_t i = 0; i < MBX_MUTEX_REGS_COUNT; i++) {
			mutexRegsBase[i] = 0;
		}

		__FENCE(iorw,iorw);
	}

	return CSK_DRIVER_OK;
}

// export MBX API function: MBX_Uninitialize
int32_t MBX_Uninitialize(void *mbx_dev)
{
	mbx_res.flags = 0;
    return CSK_DRIVER_OK;
}

int32_t MBX1_Uninitialize(void *mbx_dev)
{
	mbx_res.flags1 = 0;
    return CSK_DRIVER_OK;
}

//--------------------------------------------------------------

static void mbx_irq_handler2(void)
{
	uint32_t status;

	// channel 16-23, 寄存器 20-27
	status = mbx_res.mbx_cb->REG_AP_MAILBOX_IRQ.all & 0x00ff0000;

	//	__COMPILER_BARRIER();

	//clear irq bits
	mbx_res.mbx_cb->REG_AP_MAILBOX_IRQ.all = status;

	//	__COMPILER_BARRIER();

	mbx_res.cb_event2(CSK_MBX_EVENT_INTERRUPTS, status);
}

int32_t MBX2_Initialize(void* mbx_dev, CSK_MBX_SignalEvent_t cb_event)
{
	if((mbx_res.flags2 & MBX_FLAG_INITIALIZED) == 0U) {
		// channel 16-23, 寄存器 20-27
		uint32_t val = 0xff0000;
		MBX_Control(NULL, CSK_MBX_CP_CTRL_SET_CHS, &val);
		mbx_res.cb_event2 = cb_event;

		mbx_res.flags2 |= MBX_FLAG_INITIALIZED;
		register_ISR(IRQ_MAILBOX_2_VECTOR, mbx_irq_handler2, NULL);
		//clear interrupt bits, and enable, channel 16-23, W1C
		mbx_res.mbx_cb->REG_AP_MAILBOX_IRQ.all = val;
		enable_IRQ(IRQ_MAILBOX_2_VECTOR);
	}

	return CSK_DRIVER_OK;
}

int32_t MBX2_Uninitialize(void *mbx_dev)
{
	mbx_res.flags2 = 0;
    return CSK_DRIVER_OK;
}

//--------------------------------------------------------------

static void mbx_irq_handler3(void)
{
	uint32_t status;

	// channel 24-31, 寄存器 30-37
	status = mbx_res.mbx_cb->REG_AP_MAILBOX_IRQ.all & 0xff000000;

	//	__COMPILER_BARRIER();

	//clear irq bits
	mbx_res.mbx_cb->REG_AP_MAILBOX_IRQ.all = status;

	//	__COMPILER_BARRIER();

	mbx_res.cb_event3(CSK_MBX_EVENT_INTERRUPTS, status);
}

int32_t MBX3_Initialize(void* mbx_dev, CSK_MBX_SignalEvent_t cb_event)
{
	if((mbx_res.flags3 & MBX_FLAG_INITIALIZED) == 0U) {
		// channel 24-31, 寄存器 30-37
		uint32_t val = 0xff000000;
		MBX_Control(NULL, CSK_MBX_CP_CTRL_SET_CHS, &val);
		mbx_res.cb_event3 = cb_event;

		mbx_res.flags3 |= MBX_FLAG_INITIALIZED;
		register_ISR(IRQ_MAILBOX_3_VECTOR, mbx_irq_handler3, NULL);
		//clear interrupt bits, and enable, channel 16-23, W1C
		mbx_res.mbx_cb->REG_AP_MAILBOX_IRQ.all = val;
		enable_IRQ(IRQ_MAILBOX_3_VECTOR);
	}

	return CSK_DRIVER_OK;
}

int32_t MBX3_Uninitialize(void *mbx_dev)
{
	mbx_res.flags3 = 0;
    return CSK_DRIVER_OK;
}

//--------------------------------------------------------------

// 只用于urpc的数据发送
// export MBX API function: MBX_Send
int32_t MBX_Send(void *mbx_dev, const void *data, uint32_t ch)
{
    uint32_t status;

	if(ch > 6) {
		return CSK_DRIVER_ERROR_PARAMETER;
	}
	status = mbx_res.mbx_cb->REG_CP_MAILBOX_IRQ.all;
	// channel 0-6 mask
	if(status & 0x7f) {
		return CSK_DRIVER_ERROR_BUSY;
	}
	mbx_res.mbx_cb->REG_CP_MAILBOX_WORD0.all = ((uint32_t *)data)[0];
	mbx_res.mbx_cb->REG_CP_MAILBOX_WORD1.all = ((uint32_t *)data)[1];
	mbx_res.mbx_cb->REG_CP_MAILBOX_WORD2.all = ((uint32_t *)data)[2];
	mbx_res.mbx_cb->REG_CP_MAILBOX_WORD3.all = ((uint32_t *)data)[3];

	// memory barrier: write_release
	__DMB();

	// 只触发单个channel中断, write 1 bit to pulse.
	mbx_res.mbx_cb->REG_CP_MAILBOX_SET.all = 0x1 << ch;
    return CSK_DRIVER_OK;
}

// thread-safe: need not be in lock, only an atomic write 1 bit to pulse
int32_t MBX_Trigger(void *mbx_dev, uint32_t ch)
{
	uint32_t status;

	// read to check 1 bit, thread-safe
	status = mbx_res.mbx_cb->REG_CP_MAILBOX_IRQ.all;
	if(status & (0x1U << ch) ) {
		return CSK_DRIVER_ERROR_BUSY;
	}

	// 不用, 使用者决定 memory barrier: write_release
	// __DMB();

	// 只触发单个channel中断, write 1 bit to pulse.
	mbx_res.mbx_cb->REG_CP_MAILBOX_SET.all = 0x1 << ch;
    return CSK_DRIVER_OK;
}

// 使能单个 channel 中断
// not thread-safe, 须使用者保证独占.
// !!! 当只有一个fast interrupt, 使用场景确保了race condition不存在.
// !!! 当多个fast Interrupt, 需要使用者逻辑确保 无 race condition.
// 不做层数嵌套管理
int32_t MBX_EnableInterrupt_l(void *mbx_dev, uint32_t ch)
{
	(void) mbx_dev;

	// 获取原 32 bit enable, 更新置位 0, 写回. 属性: RW
	// not thread-safe, 多个读写, 须使用者保证独占
	// TODO: 当rtos环境下, 此处内置互斥.
	uint32_t enable = mbx_res.mbx_cb->REG_AP_MAILBOX_CTRL.all;
	enable |= CSK_MBX_CH(ch);
	mbx_res.mbx_cb->REG_AP_MAILBOX_CTRL.all = enable;

    return CSK_DRIVER_OK;
}

// 禁单个 channel 中断
// not thread-safe, 须使用者保证独占.
// !!! 当只有一个fast interrupt, 使用场景确保了race condition不存在.
// !!! 当多个fast Interrupt, 需要使用者逻辑确保 无 race condition.
// 不做层数嵌套管理
int32_t MBX_DisableInterrupt_l(void *mbx_dev, uint32_t ch)
{
	(void) mbx_dev;

	// 获取原 16 bit enable, 更新置位 1, 写回. 属性: RW
	// not thread-safe, 多个读写, 须使用者保证独占
	// TODO: 当rtos环境下, 此处内置互斥.
	uint32_t enable = mbx_res.mbx_cb->REG_AP_MAILBOX_CTRL.all;
	enable &= ~CSK_MBX_CH(ch);
	mbx_res.mbx_cb->REG_AP_MAILBOX_CTRL.all = enable;

    return CSK_DRIVER_OK;
}

// get address of the mutual exclusive shared register
volatile uint32_t* MBX_GetAddrOfSharedExRegister(void *mbx_dev, int id)
{
	(void) mbx_dev;
	(void) id;

	volatile uint32_t* mutexRegsBase = &(mbx_res.mbx_cb->REG_MAIL_FLAG_00.all);

	return &mutexRegsBase[id];
}

// export MBX API function: MBX_Receive
int32_t MBX_Receive(void *mbx_dev, void *data, uint32_t ch)
{
	mbx_res.rx_buf = data;
	return CSK_DRIVER_OK;
}


// export MBX API function: MBX_Control
int32_t MBX_Control(void *mbx_dev, uint32_t control, void *arg)
{
	switch(control) {
	case CSK_MBX_CP_CTRL_SET_CHS:
		// shared by MBX0/1/2/3, 8 bit/MBX, each assures legal channel args.
		mbx_res.mbx_cb->REG_CP_MAILBOX_CTRL.all |= *((uint32_t *)arg);
		break;
	case CSK_MBX_CP_CTRL_GET_CHS:
		// 32 bit enable
		*((uint32_t *)arg) = mbx_res.mbx_cb->REG_CP_MAILBOX_CTRL.all;
		break;
	case CSK_MBX_CP_CTRL_SET_LK:
		if(*((uint32_t *)arg)) { // set lock
			if((mbx_res.mbx_cb->REG_CP_MAILBOX_LOCK.all & 0x1) == 0) {
				mbx_res.mbx_cb->REG_CP_MAILBOX_LOCK.all = 0x5A5A;
			}
		} else { // set unlock
			if(mbx_res.mbx_cb->REG_CP_MAILBOX_LOCK.all & 0x1) {
				mbx_res.mbx_cb->REG_CP_MAILBOX_LOCK.all = 0x5A5A;
			}
		}
		break;
	case CSK_MBX_CP_CTRL_GET_LK:
		*((uint32_t *)arg) = mbx_res.mbx_cb->REG_CP_MAILBOX_LOCK.all & 0x1;
		break;
	case CSK_MBX_CP_CTRL_NOTIFY:
		// 当 irq 空闲时,
		if((mbx_res.mbx_cb->REG_CP_MAILBOX_IRQ.all & CSK_MBX_CH(7)) == 0) {
			// 只触发单个channel中断, write 1 bit to pulse.
			mbx_res.mbx_cb->REG_CP_MAILBOX_SET.all = 0x1U << (7);  //notify
		}
		break;
	case CSK_MBX_CP_CTRL_GET_RX:
		*((uint32_t *)arg) = (uint32_t)mbx_res.rx_buf;
		break;
	default:
		return CSK_DRIVER_ERROR_PARAMETER;
		break;
	}

    return CSK_DRIVER_OK;
}
#endif
