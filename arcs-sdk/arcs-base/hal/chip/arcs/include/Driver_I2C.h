/*
 * Project:      I2C (Inter-Integrated Circuit) Driver definitions
 */

#ifndef __DRIVER_I2C_H
#define __DRIVER_I2C_H

#include "Driver_Common.h"

#define CSK_I2C_API_VERSION CSK_DRIVER_VERSION_MAJOR_MINOR(1,01)  /* API version */

/****** I2C Control Codes *****/

#define CSK_I2C_OWN_ADDRESS             (0x01)      ///< Set Own Slave Address; arg = address
#define CSK_I2C_BUS_SPEED               (0x02)      ///< Set Bus Speed; arg = speed
#define CSK_I2C_BUS_CLEAR               (0x03)      ///< Execute Bus clear: send nine clock pulses
#define CSK_I2C_ABORT_TRANSFER          (0x04)      ///< Abort Master/Slave Transmit/Receive

#define CSK_I2C_TRANSMIT_MODE           (0x06)      ///< If arg0 = 0, using interrupt to transmit(default)
                                                    ///< If arg0 = 1, using DMA to transmit

/*----- I2C Bus Speed -----*/
#define CSK_I2C_BUS_SPEED_STANDARD      (0x01)      ///< Standard Speed (100kHz)
#define CSK_I2C_BUS_SPEED_FAST          (0x02)      ///< Fast Speed     (400kHz)
#define CSK_I2C_BUS_SPEED_FAST_PLUS     (0x03)      ///< Fast+ Speed    (  1MHz)

/****** I2C Address Flags *****/

#define CSK_I2C_ADDRESS_10BIT           0x0400      ///< 10-bit address flag
#define CSK_I2C_ADDRESS_GC              0x8000      ///< General Call flag

/****** I2C Event *****/
#define CSK_I2C_EVENT_TRANSFER_DONE       (1UL << 0)  ///< Master/Slave Transmit/Receive finished
#define CSK_I2C_EVENT_TRANSFER_INCOMPLETE (1UL << 1)  ///< Master/Slave Transmit/Receive incomplete transfer
#define CSK_I2C_EVENT_SLAVE_TRANSMIT      (1UL << 2)  ///< Slave Transmit operation requested
#define CSK_I2C_EVENT_SLAVE_RECEIVE       (1UL << 3)  ///< Slave Receive operation requested
#define CSK_I2C_EVENT_ADDRESS_NACK        (1UL << 4)  ///< Address not acknowledged from Slave
#define CSK_I2C_EVENT_GENERAL_CALL        (1UL << 5)  ///< General Call indication
#define CSK_I2C_EVENT_ARBITRATION_LOST    (1UL << 6)  ///< Master lost arbitration
#define CSK_I2C_EVENT_BUS_ERROR           (1UL << 7)  ///< Bus error detected (START/STOP at illegal position)
#define CSK_I2C_EVENT_BUS_CLEAR           (1UL << 8)  ///< Bus clear finished
#define CSK_I2C_EVENT_ADDRESS_ACK         (1UL << 9)   ///< Address acknowledged from Slave

typedef void
(*CSK_I2C_SignalEvent_t)(uint32_t event, void* workspace); ///< Pointer to \ref CSK_I2C_SignalEvent : Signal I2C Event.

CSK_DRIVER_VERSION
I2C_GetVersion(void);

int32_t
I2C_Initialize(void* res, CSK_I2C_SignalEvent_t cb_event, void* workspace);

int32_t
I2C_Uninitialize(void* res);

int32_t
I2C_PowerControl(void* res, CSK_POWER_STATE state);

// xfer_pending = 0, will send stop condition in this transmit
// xfer_pending = 1, won't send stop condition
int32_t
I2C_MasterTransmit(void* res, uint32_t addr, const uint8_t* data, uint32_t num,
        bool xfer_pending);

int32_t
I2C_MasterReceive(void* res, uint32_t addr, uint8_t* data, uint32_t num,
        bool xfer_pending);

int32_t
I2C_SlaveTransmit(void* res, const uint8_t* data, uint32_t num);

int32_t
I2C_SlaveReceive(void* res, uint8_t* data, uint32_t num);

int32_t
I2C_GetDataCount(void* res);

int32_t
I2C_Control(void* res, uint32_t control, uint32_t arg0);

void* I2C0(void);
void* I2C1(void);

#endif /* __DRIVER_I2C_H */
