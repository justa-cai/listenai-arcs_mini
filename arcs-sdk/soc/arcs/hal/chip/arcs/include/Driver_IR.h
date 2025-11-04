/*
 * Driver_IR.h
 *
 *  Created on: 2020年8月11日
 *      Author: USER
 */

#ifndef INCLUDE_DRIVER_DRIVER_IR_H_
#define INCLUDE_DRIVER_DRIVER_IR_H_

#include "Driver_Common.h"

#define CSK_IR_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,0)  /* API version */

// adjust pos value
#define CSK_IR_CONTROL_Pos                    0
#define CSK_IR_CONTROL_Msk                   (0xFUL << CSK_IR_CONTROL_Pos)

/*----- IR Control Codes: Miscellaneous Controls  -----*/
#define CSK_IR_CONTROL_SW_TIMEOUT_THRES      (0x1UL << CSK_IR_CONTROL_Pos)
#define CSK_IR_CONTROL_ABORT                 (0x2UL << CSK_IR_CONTROL_Pos)
#define CSK_IR_CONTROL_TX_REPEAT             (0x3UL << CSK_IR_CONTROL_Pos)
#define CSK_IR_CONTROL_CLEAR_TX_FIFO         (0x4UL << CSK_IR_CONTROL_Pos)
#define CSK_IR_CONTROL_CLEAR_RX_FIFO         (0x5UL << CSK_IR_CONTROL_Pos)

#define CSK_IR_SW_CARRY_Pos                   4
#define CSK_IR_SW_CARRY_Msk                  (1UL << CSK_IR_SW_CARRY_Pos)
#define CSK_IR_SW_CARRY_CONFIG               (1UL << CSK_IR_SW_CARRY_Pos)

#define CSK_IR_DECODE_CONTROL_Pos             5
#define CSK_IR_DECODE_CONTROL_Msk            (7UL << CSK_IR_DECODE_CONTROL_Pos)
#define CSK_IR_DECODE_CONTROL_EN             (0x1UL << CSK_IR_DECODE_CONTROL_Pos)
/// Input polarity
#define CSK_IR_DECODE_CONTROL_INPOL          (0x2UL << CSK_IR_DECODE_CONTROL_Pos)
/// Output polarity
#define CSK_IR_DECODE_CONTROL_OUTPOL         (0x3UL << CSK_IR_DECODE_CONTROL_Pos)
/// Threshold of the carrier detected
#define CSK_IR_DECODE_CONTROL_DET_THRE       (0x4UL << CSK_IR_DECODE_CONTROL_Pos)
/// Threshold of the high limited
#define CSK_IR_DECODE_CONTROL_HIGH_THRE      (0x5UL << CSK_IR_DECODE_CONTROL_Pos)
/// Threshold of the low limited
#define CSK_IR_DECODE_CONTROL_LOW_THRE       (0x6UL << CSK_IR_DECODE_CONTROL_Pos)

/************IR Event**************/
#define CSK_IR_EVENT_HW_SEND_COMPLETE                         (0x1UL << 0)
#define CSK_IR_EVENT_HW_RECEIVE_COMPLETE                      (0x1UL << 1)
#define CSK_IR_EVENT_HW_TX_REPEAT_COMPLETE                    (0x1UL << 2)
#define CSK_IR_EVENT_HW_RX_REPEAT_TRIGGER                     (0x1UL << 3)
#define CSK_IR_EVENT_HW_ADDRESS_VERIFY_ERROR                  (0x1UL << 4)
#define CSK_IR_EVENT_HW_COMMAND_VERIFY_ERROR                  (0x1UL << 5)
#define CSK_IR_EVENT_SW_SEND_COMPLETE                         (0x1UL << 6)
#define CSK_IR_EVENT_SW_RECEIVE_COMPLETE                      (0x1UL << 7)
#define CSK_IR_EVENT_SW_RECEIVE_TIMEOUT                       (0x1UL << 8)

typedef enum {
    IR_MODE_CUSTOM,
    IR_MODE_NEC,
    IR_MODE_TOSHIBA_9012,
    IR_MODE_PHILIPS_RC5,
} IR_MODE;

typedef void (*CSK_IR_SignalEvent_t) (uint32_t event, void* workspace);

int32_t  IR_Initialize(void *res, CSK_IR_SignalEvent_t cb_event, void* workspace);

int32_t  IR_Uninitialize(void* res);

int32_t  IR_PowerControl(void *res, CSK_POWER_STATE state);

int32_t  IR_Control(void *res, uint32_t control, uint32_t arg);

int32_t  IR_HW_Send(void *res, IR_MODE mode, uint16_t address, uint16_t command);

int32_t  IR_HW_Receive(void *res, IR_MODE mode, uint16_t *address, uint16_t *command);

int32_t  IR_SW_Send(void *res, uint16_t *data, uint32_t num);

int32_t  IR_SW_Receive(void *res, int16_t *data, uint32_t num);

uint32_t IR_GetTxCount(void *res);

uint32_t IR_GetRxCount(void *res);

void* IR0(void);

#endif /* INCLUDE_DRIVER_DRIVER_IR_H_ */
