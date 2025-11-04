/*
 * Project:      KEYPAD
 */

#ifndef __DRIVER_KEYPAD_H
#define __DRIVER_KEYPAD_H

#include "Driver_Common.h"

#define CSK_KEYPAD_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(0x1,0x1)

/********************  KEYPAD CONTROL CODES  *****************************/
/*!< Specifies the interval of IRQ generation:
(kp_itv_time + 1)* (kp_dbn_time + 1) * scan_time
scan_time = 1/clk * Number of enabled KeyOut * 5 */
#define KEYPAD_INTERVAL_VALUE_Pos                0
#define KEYPAD_INTERVAL_VALUE_Mask               (0x1 << KEYPAD_INTERVAL_VALUE_Pos)
#define KEYPAD_INTERVAL_VALUE_SET                (0x1 << KEYPAD_INTERVAL_VALUE_Pos)

/*!< Specifies the debounce time:
(kp_dnb_time + 1) * scan_time
scan_time = 1/clk * Number of enabled KeyOut * 5 */
#define KEYPAD_DEBOUNCE_VALUE_Pos                1
#define KEYPAD_DEBOUNCE_VALUE_Mask               (0x1 << KEYPAD_DEBOUNCE_VALUE_Pos)
#define KEYPAD_DEBOUNCE_VALUE_SET                (0x1 << KEYPAD_DEBOUNCE_VALUE_Pos)

// In high polarity
/* Only high pulse useful, so the out pin must with a pull down, keep it invalid
 * Logic waveform:
 *___      ___      ___
 *| |      | |      | |
 *| |______| |______| |______*/

//In low polarity
/* Only low pulse useful, so the out pin must with a pull up, keep it invalid
 * Logic waveform:
 * ________ ________ ________ __
 *        | |      | |      | |
 *        |_|      |_|      |_|*/
#define KEYPAD_POLARITY_Pos                      4
#define KEYPAD_POLARITY_Mask                     (0x3 << KEYPAD_POLARITY_Pos)
#define KEYPAD_POLARITY_HIGH                     (0x1 << KEYPAD_POLARITY_Pos)
#define KEYPAD_POLARITY_LOW                      (0x2 << KEYPAD_POLARITY_Pos)

#define KEYPAD_SET_INTR_Pos                      6
#define KEYPAD_SET_INTR_Mask                     (0x3 << KEYPAD_SET_INTR_Pos)
#define KEYPAD_SET_INTR_PRESS_RELEASE            (0x1 << KEYPAD_SET_INTR_Pos)    /*!< Key press or release event */
#define KEYPAD_SET_INTR_INTERVAL                 (0x2 << KEYPAD_SET_INTR_Pos)    /*!< Keypad interval event      */

/********************  KEYPAD INTERRUPT CAUSES  *************************/
#define KEYPAD_EVENT_PRESS_RELEASE               (0x1 << 0)    /*!< Key press or release event */
#define KEYPAD_EVENT_INTERVAL                    (0x1 << 1)    /*!< Keypad interval event      */

/********************  KEYPAD LINE CODES  *****************************/
#define KEYPAD_LINEX_IN_0                        (0x1 << 0)
#define KEYPAD_LINEX_IN_1                        (0x1 << 1)
#define KEYPAD_LINEX_IN_2                        (0x1 << 2)
#define KEYPAD_LINEX_IN_3                        (0x1 << 3)
#define KEYPAD_LINEX_IN_4                        (0x1 << 4)
#define KEYPAD_LINEX_IN_5                        (0x1 << 5)
#define KEYPAD_LINEX_IN_6                        (0x1 << 6)

#define KEYPAD_LINEX_OUT_0                       (0x1 << 0)
#define KEYPAD_LINEX_OUT_1                       (0x1 << 1)
#define KEYPAD_LINEX_OUT_2                       (0x1 << 2)
#define KEYPAD_LINEX_OUT_3                       (0x1 << 3)
#define KEYPAD_LINEX_OUT_4                       (0x1 << 4)
#define KEYPAD_LINEX_OUT_5                       (0x1 << 5)
#define KEYPAD_LINEX_OUT_6                       (0x1 << 6)
#define KEYPAD_LINEX_OUT_7                       (0x1 << 7)
#define KEYPAD_LINEX_OUT_8                       (0x1 << 8)
#define KEYPAD_LINEX_OUT_9                       (0x1 << 9)
#define KEYPAD_LINEX_OUT_10                      (0x1 << 10)
#define KEYPAD_LINEX_OUT_11                      (0x1 << 11)
#define KEYPAD_LINEX_OUT_12                      (0x1 << 12)
#define KEYPAD_LINEX_OUT_13                      (0x1 << 13)
#define KEYPAD_LINEX_OUT_14                      (0x1 << 14)

typedef enum _KEYPAD_PAD_STATUS{
    keypad_pad_idle            = 0x0,
    keypad_pad_pressed,
} KEYPAD_PAD_STATUS;

typedef enum _KEYPAD_LINE_STATE{
    keypad_in_line_enable      = 0x0,
    keypad_in_line_disable,
    keypad_out_line_enable,
    keypad_out_line_disable,
    keypad_all_enable,
    keypad_all_disable,
} KEYPAD_LINE_STATE;


typedef void
(*CSK_KEYPAD_SignalEvent_t)(uint32_t event, void* workspace);

CSK_DRIVER_VERSION
KEYPAD_GetVersion(void);

int32_t
KEYPAD_Initialize(void* res, CSK_KEYPAD_SignalEvent_t cb_event, void* workspace);

int32_t
KEYPAD_Uninitialize(void* res);

int32_t
KEYPAD_PowerControl(void* res, CSK_POWER_STATE state);

int32_t
KEYPAD_Control(void* res, uint32_t control, uint32_t arg0);

int32_t
KEYPAD_LineState(void* res, KEYPAD_LINE_STATE control, uint32_t line);

int32_t
KEYPAD_Start(void* res);

int32_t
KEYPAD_Stop(void* res);

KEYPAD_PAD_STATUS
KEYPAD_KeyGetReason(void* res, uint8_t keyin, uint8_t keyout);

void* KEYPAD0(void);

#endif /* __DRIVER_KEYPAD_H */

