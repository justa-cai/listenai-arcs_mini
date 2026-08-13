/**
 * @file    i8080.h
 * @brief   Internal definitions for I8080 parallel bus controller driver
 * @details This header contains hardware-specific configuration constants,
 *          state machine definitions, and private data structures required
 *          for implementing the I8080 peripheral controller interface.
 *
 *          It establishes timing parameters, electrical characteristics,
 *          register mappings, and operational constraints critical to
 *          proper functioning of the I8080 bus interface.
 *
 * @note    Contains confidential implementation details - intended for driver
 *          internal use only. Public API defined in Driver_I8080.h.
 *
 * @author  USER
 * @date    2025.05.10 : Initial release version
 * @version 1.0
 * @copyright (c) ListenAI. All rights reserved.
 *           Confidentiality Notice: This file contains proprietary information
 *           protected under applicable laws. Unauthorized reproduction prohibited.
 */

#ifndef __DRIVER_I8080_INTERNAL_H
#define __DRIVER_I8080_INTERNAL_H

#include "venusa_ap.h"
#include "ClockManager.h"
#include "log_print.h"
#include "Driver_I8080.h"

/*!< Hardware configuration constants */
/*-------------------------------------------------------------------------*/
/**
 * @brief RX FIFO depth (must leave 1 spare slot)
 *        Maximum number of receive entries before buffer becomes full.
 */
#define I8080_RXFIFO_NUM                (16-1)      ///< RX FIFO size

/**
 * @brief Transmit FIFO capacity
 *        Stores outgoing data temporarily during burst operations.
 */
#define I8080_TXFIFO_NUM                32          ///< TX FIFO size

/**
 * @brief Default receive threshold trigger level
 *        Generates interrupt/DMA request when this many bytes received.
 *        Valid range: 0~31 (matches 16-byte FIFO capacity).
 */
#define I8080_RXTHRES_DEFAULT           8           ///< Default RX threshold

/**
 * @brief Default transmit threshold trigger level
 *        Triggers XOFF flow control when remaining space reaches this value.
 *        Valid range: 0~63 (matches 32-word FIFO capacity).
 */
#define I8080_TXTHRES_DEFAULT           8           ///< Default TX threshold

/*!< Write/Read/Dummy cycle count configuration limits */
/*-------------------------------------------------------------------------*/
/**
 * @brief Maximum write cycle count (bits 0-19)
 *        Limits total consecutive write operations per transaction.
 */
#define I8080_WR_NUM_MAX                0xFFFFF     ///< bit0~19: 0xFFFFF=1048575

/**
 * @brief Maximum read cycle count (bits 20-23)
 *        Restricts consecutive read operations in sequence.
 */
#define I8080_RD_NUM_MAX                0xF         ///< bit20~23

/**
 * @brief Maximum dummy cycle count (bits 24-25)
 *        Used for bus turnaround delays between transactions.
 */
#define I8080_DUMMY_NUM_MAX             0x3         ///< bit24~25

/*!< Clock configuration parameters */
/*-------------------------------------------------------------------------*/
/**
 * @brief Maximum system clock frequency selection
 *        Upper bound for internal peripheral clock derivation.
 */
#define I8080_CLK_RUN_SEL_100MHz        BOARD_BOOTCLOCKRUN_SYSPLL_PERI_CLK   ///< Max system clock

/**
 * @brief Minimum system clock frequency selection
 *        Lower operational limit for stable peripheral behavior.
 */
#define I8080_CLK_RUN_SEL_24MHz         24000000    ///< Min system clock

/**
 * @brief Maximum clock divider ratio
 *        Slowest divisor allowed for peripheral clock generation.
 */
#define I8080_CLK_RUN_DIV_MAX           15          ///< Max clock divider

/**
 * @brief Minimum clock divider ratio
 *        Fastest direct clock connection mode.
 */
#define I8080_CLK_RUN_DIV_MIN           1           ///< Min clock divider

/**
 * @brief Calculated maximum derived clock frequency
 *        Derived from highest input clock with minimum division.
 */
#define I8080_CLK_RUN_HZ_MAX            (I8080_CLK_RUN_SEL_100MHz / I8080_CLK_RUN_DIV_MIN)

/**
 * @brief Calculated minimum derived clock frequency
 *        Resulting from lowest input clock with maximum division.
 */
#define I8080_CLK_RUN_HZ_MIN            (I8080_CLK_RUN_SEL_24MHz / I8080_CLK_RUN_DIV_MAX)

/**
 * @brief Maximum output clock divider ratio
 *        Controls external clock signal generation speed.
 */
#define I8080_CLK_OUT_DIV_MAX           0xF         ///< Max output clock divider

/**
 * @brief Minimum output clock divider ratio
 *        Direct passthrough mode for external clock output.
 */
#define I8080_CLK_OUT_DIV_MIN           0           ///< Min output clock divider

/**
 * @brief Default output clock divider setting
 *        Typical recommended value balancing stability and response time.
 */
#define I8080_CLK_OUT_DIV_DEFAULT       5

/*!< Bus timing parameters */
/*-------------------------------------------------------------------------*/
/**
 * @brief Maximum Chip Select hold time
 *        Duration CS must remain active after falling edge of clock.
 */
#define I8080_TIMING_CSHT_MAX           0xF         ///< Max CS hold time

/**
 * @brief Default Chip Select hold time
 *        Standardized setup for most memory devices.
 */
#define I8080_TIMING_CSHT_DEFAULT       2

/**
 * @brief Maximum CS to SCLK delay
 *        Time between CS activation and first clock rising edge.
 */
#define I8080_TIMING_CS2SCLK_MAX        0x7         ///< Max CS-SCLK delay

/**
 * @brief Default CS to SCLK delay
 *        Optimal timing for standard access cycles.
 */
#define I8080_TIMING_CS2SCLK_DEFAULT    1

/*!< Data transfer modes */
/*-------------------------------------------------------------------------*/
/**
 * @brief Write operation mode
 *        Data written from host to peripheral during active cycle.
 */
#define I8080_TRANS_MODE_WRITE          0

/**
 * @brief Read operation mode
 *        Data read from peripheral to host during active cycle.
 */
#define I8080_TRANS_MODE_READ           1

/**
 * @brief No data transfer mode
 *        Control signals only (no data phase).
 */
#define I8080_TRANS_MODE_NODATA         2

/**
 * @brief Dummy + Read combined mode
 *        Executes idle cycles followed by read operation.
 */
#define I8080_TRANS_MODE_DUMMY_READ     3

/*!< Data length specifications */
/*-------------------------------------------------------------------------*/
/**
 * @brief 8-bit data transfer mode
 *        Least significant byte first alignment.
 */
#define I8080_DATA_LEN_8BIT             7

/**
 * @brief 24-bit data transfer mode
 *        Middle byte aligned for 24-bit peripherals.
 */
#define I8080_DATA_LEN_24BIT            23

/**
 * @brief 32-bit data transfer mode
 *        Most significant byte first alignment.
 */
#define I8080_DATA_LEN_32BIT            31

/*!< Command length specifications */
/*-------------------------------------------------------------------------*/
/**
 * @brief 1-byte command format
 *        Single byte instruction opcode.
 */
#define I8080_CMD_LEN_1BYTE             0

/**
 * @brief 2-byte command format
 *        Two-byte extended instruction set.
 */
#define I8080_CMD_LEN_2BYTE             1

/**
 * @brief 3-byte command format
 *        Three-byte complex instruction encoding.
 */
#define I8080_CMD_LEN_3BYTE             2

/**
 * @brief 4-byte command format
 *        Four-byte superset instruction format.
 */
#define I8080_CMD_LEN_4BYTE             3

/**
 * @brief Pin output state definitions
 *        Controls electrical levels on interface pins.
 */
typedef enum {
    /**< Drive pin actively low (logic 0) */
    I8080_PIN_OUTPUT_LOW = 0,
    /**< Drive pin actively high (logic 1) */
    I8080_PIN_OUTPUT_HIGH,
    /**< Allow automatic level determination based on protocol */
    I8080_PIN_OUTPUT_AUTO,
    /**< State boundary marker (not used) */
    I8080_PIN_OUTPUT_BUTT
} I8080_emPinOut;

/**
 * @brief Driver operational states
 *        Indicates current status of the peripheral controller.
 */
typedef enum {
    /**< Peripheral uninitialized (power-on reset state) */
    I8080_STATE_RESET = 0,
    /**< Ready to accept new commands */
    I8080_STATE_READY,
    /**< Currently executing data transfer */
    I8080_STATE_BUSY,
    /**< Operation exceeded timeout duration */
    I8080_STATE_TIMEOUT,
    /**< Error condition detected during operation */
    I8080_STATE_ERROR,
    /**< State boundary marker (not used) */
    I8080_STATE_BUTT
} I8080_emState;

/**
 * @brief Transfer information structure
 *        Contains real-time status and control information during operations.
 *
 * @var state
 *        Current execution state machine status.
 * @var cs
 *        Chip Select (CS#) pin output state.
 * @var rs
 *        Register Select (RS#) pin output state.
 * @var rx_buf
 *        Pointer to current receive buffer location.
 * @var rx_size
 *        Number of bytes expected in current receive operation.
 */
typedef struct {
    volatile I8080_emState state;       ///< Current transfer state
    volatile I8080_emPinOut cs;         ///< CS pin state
    volatile I8080_emPinOut rs;         ///< RS pin state
    volatile uint8_t *rx_buf;           ///< RX buffer pointer
    volatile uint8_t rx_size;           ///< RX data size
} I8080_INFO;

/**
 * @brief I8080 device control block
 *        Manages hardware register mapping and event handling.
 *
 * @var Instance
 *        Base address of mapped peripheral registers.
 * @var cb_event
 *        Event handler callback function pointer.
 * @var info
 *        Runtime status information container.
 */
typedef struct {
    I8080_OUT_RegDef *Instance;          ///< Register base address
    I8080_SignalEvent_t cb_event;        ///< Event callback function
    uint32_t usr_param;                  // user parameter of event callback
    I8080_INFO info;                     ///< Device information
} I8080_DEV;

#endif /* __DRIVER_I8080_INTERNAL_H */
