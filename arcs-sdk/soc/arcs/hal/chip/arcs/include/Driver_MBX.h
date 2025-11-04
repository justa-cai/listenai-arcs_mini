/*
 * Copyright (c) 2020-2025 ChipSky Technology
 * All rights reserved.
 *
 */
#include <assert.h>
#include "Driver_Common.h"

#define MBX_FLAG_INITIALIZED          (1UL << 0)     // MBX initialized
#define MBX_FLAG_POWERED              (1UL << 1)     // MBX powered on
#define MBX_FLAG_CONFIGURED           (1UL << 2)     // MBX configured

#define CSK_MBX_AP_LOCK               (0x1UL)
#define CSK_MBX_AP_UNLOCK             (0x0UL)
#define CSK_MBX_ENA_CH_00             (0x1UL << 0)
#define CSK_MBX_ENA_CH_01             (0x1UL << 1)
#define CSK_MBX_ENA_CH_02             (0x1UL << 2)
#define CSK_MBX_ENA_CH_03             (0x1UL << 3)
#define CSK_MBX_ENA_CH_04             (0x1UL << 4)
#define CSK_MBX_ENA_CH_05             (0x1UL << 5)
#define CSK_MBX_ENA_CH_06             (0x1UL << 6)
#define CSK_MBX_ENA_CH_07             (0x1UL << 7)
#define CSK_MBX_ENA_CH_08             (0x1UL << 8)
#define CSK_MBX_ENA_CH_09             (0x1UL << 9)
#define CSK_MBX_ENA_CH_10             (0x1UL << 10)
#define CSK_MBX_ENA_CH_11             (0x1UL << 11)
#define CSK_MBX_ENA_CH_12             (0x1UL << 12)
#define CSK_MBX_ENA_CH_13             (0x1UL << 13)
#define CSK_MBX_ENA_CH_14             (0x1UL << 14)
#define CSK_MBX_ENA_CH_15             (0x1UL << 15)
#define CSK_MBX_ENA_CH_ALL            (0xffffUL)

#define CSK_MBX_AP_CTRL_SET_CHS       (0x01)   //set channels to enable
#define CSK_MBX_AP_CTRL_GET_CHS       (0x02)   //get enabled channels
#define CSK_MBX_AP_CTRL_SET_LK        (0x03)   //set lock status of AP mailbox
#define CSK_MBX_AP_CTRL_GET_LK        (0x04)   //get lock status of AP mailbox
#define CSK_MBX_AP_CTRL_NOTIFY        (0x05)   //notify CP, the next package can receive
#define CSK_MBX_AP_CTRL_GET_RX        (0x06)   //get receive buffer

#define CSK_MBX_CP_CTRL_SET_CHS       (0x10)   //set channels to enable
#define CSK_MBX_CP_CTRL_GET_CHS       (0x20)   //get enabled channels
#define CSK_MBX_CP_CTRL_SET_LK        (0x30)   //set lock status of CP mailbox
#define CSK_MBX_CP_CTRL_GET_LK        (0x40)   //get lock status of CP mailbox
#define CSK_MBX_CP_CTRL_NOTIFY        (0x50)   //notify AP, the next package can receive
#define CSK_MBX_CP_CTRL_GET_RX        (0x60)   //get receive buffer

#define CSK_MBX_EVENT_SEND_COMPLETE     (1UL << 0)  // Data Send completed
#define CSK_MBX_EVENT_RECEIVE_COMPLETE  (1UL << 1)  // Data Receive completed
#define CSK_MBX_EVENT_INTERRUPTS        (1UL << 2)  // inter-cores interrupts

typedef int8_t (*CSK_MBX_SignalEvent_t)(uint32_t event, uint32_t param);


// export MBX API function: MBX_GetVersion
CSK_DRIVER_VERSION MBX_GetVersion();

// export MBX API function: MBX_Initialize
int32_t MBX_Initialize(void* mbx_dev, CSK_MBX_SignalEvent_t cb_event);

// export MBX API function: MBX_Uninitialize
int32_t MBX_Uninitialize(void *mbx_dev);

// exclusive registers and interrupt between cores
int32_t MBX1_Initialize(void* mbx_dev, CSK_MBX_SignalEvent_t cb_event);

int32_t MBX1_Uninitialize(void *mbx_dev);

// only interrupt between cores
int32_t MBX2_Initialize(void* mbx_dev, CSK_MBX_SignalEvent_t cb_event);

int32_t MBX2_Uninitialize(void *mbx_dev);

// only interrupt between cores
int32_t MBX3_Initialize(void* mbx_dev, CSK_MBX_SignalEvent_t cb_event);

int32_t MBX3_Uninitialize(void *mbx_dev);

// export MBX API function: MBX_Send
int32_t MBX_Send(void *mbx_dev, const void *data, uint32_t ch);


// export MBX API function: MBX_Receive
int32_t MBX_Receive(void *mbx_dev, void *data, uint32_t ch);


// export MBX API function: MBX_Control
int32_t MBX_Control(void *mbx_dev, uint32_t control, void *arg);

#define MBX_MUTEX_CHANNEL		(14)
// export MBX API function: MBX_Trigger
int32_t MBX_Trigger(void *mbx_dev, uint32_t ch);

int32_t MBX_EnableInterrupt_l(void *mbx_dev, uint32_t ch);

int32_t MBX_DisableInterrupt_l(void *mbx_dev, uint32_t ch);

#define MBX_MUTEX_REGS_COUNT   	(16)
// param id: 0 - 15
volatile uint32_t* MBX_GetAddrOfSharedExRegister(void *mbx_dev, int id);
