/**
 *  Created on: Oct 09, 2023
 *
 *  @file     qspi_in.h
 *  @author
 *  @copyright
 *  @brief    This file contains internal driver definitions for QSPI input interface.
 *            It provides low-level configuration parameters and hardware abstraction layer.
 */

#ifndef __DRIVER_QSPI_IN_INTERNAL_H
#define __DRIVER_QSPI_IN_INTERNAL_H

#include "venusa_ap.h"
#include "Driver_QSPI_IN.h"

/*!< Clock configuration parameters */
/*-------------------------------------------------------------------------*/
/**
 * @brief Maximum system clock frequency selection
 *        Upper bound for internal peripheral clock derivation.
 */
#define QSPI_IN_CLK_RUN_SEL_100MHz        100000000   ///< Max system clock

/**
 * @brief Minimum system clock frequency selection
 *        Lower operational limit for stable peripheral behavior.
 */
#define QSPI_IN_CLK_RUN_SEL_24MHz         24000000    ///< Min system clock

/**
 * @brief Maximum clock divider ratio
 *        Slowest divisor allowed for peripheral clock generation.
 */
#define QSPI_IN_CLK_RUN_DIV_MAX           15          ///< Max clock divider

/**
 * @brief Minimum clock divider ratio
 *        Fastest direct clock connection mode.
 */
#define QSPI_IN_CLK_RUN_DIV_MIN           1           ///< Min clock divider

/**
 * @brief Calculated maximum derived clock frequency
 *        Derived from highest input clock with minimum division.
 */
#define QSPI_IN_CLK_RUN_HZ_MAX            (QSPI_IN_CLK_RUN_SEL_100MHz / QSPI_IN_CLK_RUN_DIV_MIN)

/**
 * @brief Calculated minimum derived clock frequency
 *        Resulting from lowest input clock with maximum division.
 */
#define QSPI_IN_CLK_RUN_HZ_MIN            (QSPI_IN_CLK_RUN_SEL_24MHz)


/*----- QSPI Transfer mode -----*/
#define CSK_QSPI_IN_TRANSMODE_SINGLE       (0x0)  /**< Single SPI (standard 1-wire) transfer mode */
#define CSK_QSPI_IN_TRANSMODE_DUAL         (0x1)  /**< Dual SPI (2-wire parallel) transfer mode */
#define CSK_QSPI_IN_TRANSMODE_QUAD         (0x2)  /**< Quad SPI (4-wire parallel) transfer mode */

#define CSK_QSPI_IN_TRANSMODE_READ_ONLY    (0x2)  /**< read only transfer mode */

#define CSK_QSPI_IN_TRANSFORM_1P1B         (0x0)  /**< The Transfer Format of Image 1 pixel 1 byte */
#define CSK_QSPI_IN_TRANSFORM_1P2B         (0x1)  /**< The Transfer Format of Image 1 pixel 2 bytes */
#define CSK_QSPI_IN_TRANSFORM_1P3B         (0x2)  /**< The Transfer Format of Image 1 pixel 3 bytes */

#define CSK_QSPI_IN_BUF      (QSPI_IN_BASE + 0x2C)  /**< Base address of QSPI input buffer register block */

#define CSK_QSPI_IN_RXFIFO_DEPTH    32      /**< Number of 32-bit words in receive FIFO buffer */

#define QSPI_IN_INTRST_LINE_START_INT_Pos       10  /**< Line start detection interrupt position */
#define QSPI_IN_INTRST_LINE_START_INT_Msk       0x400  /**< Line start detection interrupt mask */

#define QSPI_IN_INTRST_LINE_END_INT_Pos         9   /**< Line end detection interrupt position */
#define QSPI_IN_INTRST_LINE_END_INT_Msk         0x200  /**< Line end detection interrupt mask */

#define QSPI_IN_INTRST_FRAME_END_INT_Pos        8   /**< Frame end detection interrupt position */
#define QSPI_IN_INTRST_FRAME_END_INT_Msk        0x100  /**< Frame end detection interrupt mask */

#define QSPI_IN_INTRST_FRAME_START_INT_Pos      7   /**< Frame start detection interrupt position */
#define QSPI_IN_INTRST_FRAME_START_INT_Msk      0x80  /**< Frame start detection interrupt mask */

#define QSPI_IN_INTRST_CRC_ERR_INT_Pos          6   /**< CRC error detection interrupt position */
#define QSPI_IN_INTRST_CRC_ERR_INT_Msk          0x40  /**< CRC error detection interrupt mask */

#define QSPI_IN_INTRST_SLVCMDINT_Pos            5   /**< Slave command completion interrupt position */
#define QSPI_IN_INTRST_SLVCMDINT_Msk            0x20  /**< Slave command completion interrupt mask */

#define QSPI_IN_INTRST_ENDINT_Pos               4   /**< End of transaction interrupt position */
#define QSPI_IN_INTRST_ENDINT_Msk               0x10  /**< End of transaction interrupt mask */

#define QSPI_IN_INTRST_MTK_TRANS_ERR_INT_Pos    3   /**< MediaTek specific transport error interrupt position */
#define QSPI_IN_INTRST_MTK_TRANS_ERR_INT_Msk    0x8  /**< MediaTek specific transport error interrupt mask */

#define QSPI_IN_INTRST_RXFIFOINT_Pos            2   /**< Receive FIFO threshold reached interrupt position */
#define QSPI_IN_INTRST_RXFIFOINT_Msk            0x4  /**< Receive FIFO threshold reached interrupt mask */

#define QSPI_IN_INTRST_RXFIFOORINT_Pos          0   /**< Receive FIFO overrun/underflow interrupt position */
#define QSPI_IN_INTRST_RXFIFOORINT_Msk          0x1  /**< Receive FIFO overrun/underflow interrupt mask */

/**
 * @brief QSPI Input Device Driver State Structure
 *
 * This structure maintains the complete state of a QSPI input device instance,
 * including register mappings, initialization parameters, and event callbacks.
 */
typedef struct __QSPI_IN_DEV
{
    QSPI_SENSOR_IN_RegDef      *Instance;           /*!< @brief Register base address for QSPI peripheral */
                                                      /*!< Points to memory-mapped peripheral registers */

    QSPI_IN_InitTypeDef        Init;                /*!< @brief Device initialization parameters */
                                                      /*!< Contains configured operational settings */

    QSPI_IN_SignalEvent_t      cb_event;            /*!< @brief Event handler callback function */
                                                      /*!< Called when significant events occur during operation */
} QSPI_IN_DEV;

#endif /* __DRIVER_QSPI_IN_INTERNAL_H */
