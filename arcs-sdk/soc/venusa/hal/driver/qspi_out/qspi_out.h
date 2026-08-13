/*
 * qspi_out.h
 *
 *  Created on: Oct 09, 2023
 *
 *  @brief   Internal header for QSPI output driver implementation
 *           Contains private definitions, structures and configurations
 *           used by the QSPI output subsystem. This file should not be
 *           directly included by application code.
 */

#ifndef __DRIVER_QSPI_OUT_INTERNAL_H
#define __DRIVER_QSPI_OUT_INTERNAL_H

#include "dma.h"
#include "venusa_ap.h"
#include "Driver_QSPI_OUT.h"

/** Buffer address offset within QSPI block */
#define QSPI_OUT_TX_BUF                 (QSPI_OUT_BASE + 0x2C)

/** Transmit FIFO depth in words */
#define QSPI_OUT_TXFIFO_DEPTH           16  // words

/** Single data line (standard SPI) 1line */
#define QSPI_OUT_LANE_NUM_SINGLE        0

/** Dual data lines (double throughput) 2line */
#define QSPI_OUT_LANE_NUM_DUAL          1

/** Quad data lines (quadrupled throughput) 4line */
#define QSPI_OUT_LANE_NUM_QUAD          2

typedef enum {
    QSPI_OUT_STATE_RESET = 0,       /**< Peripheral uninitialized (power-on reset state) */
    QSPI_OUT_STATE_READY,           /**< Ready to accept new commands */
    QSPI_OUT_STATE_BUSY,            /**< Currently executing data transfer */
    QSPI_OUT_STATE_TIMEOUT,         /**< Operation exceeded timeout duration */
    QSPI_OUT_STATE_ERROR,           /**< Error condition detected during operation */
    QSPI_OUT_STATE_BUTT             /**< State boundary marker (not used) */
} QSPI_OUT_emState;

typedef struct {
    QSPI_LCD_RegDef *Instance;          ///< Register base address
    QSPI_OUT_SignalEvent_t cb_event;    ///< Event callback function
    uint32_t usr_param;                 // user parameter of event callback
    volatile QSPI_OUT_emState state;    ///< Current transfer state
    uint8_t txfifo_depth;               // TX FIFO depth, in words
    uint8_t data_bits;                  // length of each data unit in bits (see DataLen of TRANSFMT)
    const void *tx_buf;                 // pointer to out data buffer
    uint32_t tx_cnt;                    // number of data sent (see TRANSFMT register bit[12:8])
    uint32_t req_tx_cnt;                // number of data requested to send (see TRANSFMT register bit[12:8])
} QSPI_OUT_DEV;

#endif /* __DRIVER_QSPI_OUT_INTERNAL_H */
