/**
 * @file    i2c.c
 * @author  Andes Technology Corporation
 * @brief   I2C Hardware Abstraction Layer (HAL) Driver Implementation
 *          This file implements the driver interface for I2C peripherals,
 *          providing functionality including initialization, master/slave transactions,
 *          power management, and interrupt handling.
 *
 * @details Manages two I2C controllers (I2C0/I2C1) with DMA support,
 *          handles bus arbitration, protocol timing configuration,
 *          and provides transaction APIs for master/slave modes.
 *
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2021 ListenAI Technology Corporation.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by Andes Technology Corporation under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

#include "Driver_I2C.h"
#include "dma.h"
#include "venusa_ap.h"
#include "ClockManager.h"
#include "PowerManager.h"
#include "assert.h"

#define OFFSET_I2C_IDREV                            0x000    ///< ID Revision Register Offset
#define BITSET_0X000_ID                             0xfffff000 ///< ID Field Bitmask
#define BITSET_0X000_REVMAJOR                      0xff0     ///< Major Revision Bitmask
#define BITSET_0X000_REVMINOR                      0xf       ///< Minor Revision Bitmask

#define OFFSET_I2C_CFG                             0x010    ///< Configuration Register Offset
#define BITSET_0X010_CFG_MASK                     0x3      ///< Configuration Field Mask
#define BITSET_0X010_FIFOSIZE                      0x3      ///< FIFO Size Field Mask
#define BITSET_0X010_FIFOSIZE_2_BYTES             0x0      ///< 2-Byte FIFO Mode
#define BITSET_0X010_FIFOSIZE_4_BYTES             0x1      ///< 4-Byte FIFO Mode
#define BITSET_0X010_FIFOSIZE_8_BYTES             0x2      ///< 8-Byte FIFO Mode
#define BITSET_0X010_FIFOSIZE_16_BYTES            0x3      ///< 16-Byte FIFO Mode

#define OFFSET_I2C_INTEN                           0x014    ///< Interrupt Enable Register Offset
#define BITSET_0X014_INTEN_MASK                   0x3ff    ///< Interrupt Enable Field Mask
#define BITSET_0X014_CMPL                         0x200    ///< Completition Interrupt Bit
#define BITSET_0X014_BYTERECV                     0x100    ///< Byte Received Interrupt Bit
#define BITSET_0X014_BYTETRANS                    0x80     ///< Byte Transmitted Interrupt Bit
#define BITSET_0X014_START                        0x40     ///< Start Condition Interrupt Bit
#define BITSET_0X014_STOP                         0x20     ///< Stop Condition Interrupt Bit
#define BITSET_0X014_ARBLOS                       0x10     ///< Arbitration Lost Interrupt Bit
#define BITSET_0X014_ADDRHIT                      0x8      ///< Address Hit Interrupt Bit
#define BITSET_0X014_FIFOHALF                     0x4      ///< FIFO Half Full Interrupt Bit
#define BITSET_0X014_FIFOFULL                     0x2      ///< FIFO Full Interrupt Bit
#define BITSET_0X014_FIFOEMPTY                    0x1      ///< FIFO Empty Interrupt Bit

#define OFFSET_I2C_STATUS                         0x018    ///< Status Register Offset
#define BITSET_0X018_STATUS_MASK                  0x7fff   ///< Status Field Mask
#define BITSET_0X018_STATUS_CLEAR                 0x3f8    ///< Clearable Status Bits Mask
#define BITSET_0X018_LINESDA                     0x4000   ///< SDA Line Status Bit
#define BITSET_0X018_LINESCL                     0x2000   ///< SCL Line Status Bit
#define BITSET_0X018_GENCALL                     0x1000   ///< General Call Status Bit
#define BITSET_0X018_BUSBUSY                     0x800    ///< Bus Busy Status Bit
#define BITSET_0X018_ACK                         0x400    ///< Acknowledge Status Bit
#define BITSET_0X018_CMPL                        0x200    ///< Completition Status Bit
#define BITSET_0X018_BYTEREVC                    0x100    ///< Byte Received Status Bit
#define BITSET_0X018_BYTETRANS                   0x80     ///< Byte Transmitted Status Bit
#define BITSET_0X018_START                       0x40     ///< Start Condition Status Bit
#define BITSET_0X018_STOP                        0x20     ///< Stop Condition Status Bit
#define BITSET_0X018_ARBLOS                      0x10     ///< Arbitration Lost Status Bit
#define BITSET_0X018_ADDRHIT                     0x8      ///< Address Hit Status Bit
#define BITSET_0X018_FIFOHALF                    0x4      ///< FIFO Half Full Status Bit
#define BITSET_0X018_FIFOFULL                    0x2      ///< FIFO Full Status Bit
#define BITSET_0X018_FIFO_EMPTY                  0x1      ///< FIFO Empty Status Bit

#define OFFSET_I2C_ADDR                          0x01C    ///< Slave Address Register Offset
#define BITSET_0X01C_ADDR_MASK                  0x3ff    ///< Address Field Mask
#define BITSET_0X01C_ADDR                       0x3ff    ///< Address Field Value

#define OFFSET_I2C_DATA                         0x020    ///< Data Register Offset
#define BITSET_0X020_DATA_MASK                  0xff    ///< Data Field Mask
#define BITSET_0X020_DATA                       0xff    ///< Data Field Value

#define OFFSET_I2C_CTRL                         0x024    ///< Control Register Offset
#define BITSET_0X024_CTRL_MASK                  0x1fff  ///< Control Field Mask
#define BITSET_0X024_PHASE_START                0x1000  ///< Phase Start Bit
#define BITSET_0X024_PHASE_ADDR                 0x800   ///< Phase Address Bit
#define BITSET_0X024_PHASE_DATA                 0x400   ///< Phase Data Bit
#define BITSET_0X024_PHASE_STOP                 0x200   ///< Phase Stop Bit
#define BITSET_0X024_DIR                       0x100   ///< Direction Bit
#define BITSET_0X024_DATACNT                   0xff    ///< Data Count Field Mask

#define OFFSET_I2C_CMD                          0x028    ///< Command Register Offset
#define BITSET_0X028_CMD_MASK                  0x7     ///< Command Field Mask
#define BITSET_0X028_CMD                       0x7     ///< Command Field Value
#define BITSET_0X028_CMD_NO_ACT               (0x0)   ///< No Action Command
#define BITSET_0X028_CMD_ISSUE_TRANSACTION     (0x1)   ///< Issue Transaction Command
#define BITSET_0X028_CMD_ACK                   (0x2)   ///< Send ACK Command
#define BITSET_0X028_CMD_NACK                  (0x3)   ///< Send NACK Command
#define BITSET_0X028_CMD_CLEAR_FIFO             (0x4)   ///< Clear FIFO Command
#define BITSET_0X028_CMD_RESET_I2C             (0x5)   ///< Reset I2C Controller Command

#define OFFSET_I2C_SETUP                       0x02C    ///< Setup Register Offset
#define BITSET_0X02C_SETUP_MASK                0x1fffffff ///< Setup Field Mask
#define BITSET_0X02C_T_SUDAT                   0x1f000000 ///< SUDA Tail Timing Field
#define BITSET_0X02C_T_SP                      0xe00000   ///< SP Rise Time Field
#define BITSET_0X02C_T_HDDAT                   0x1f0000   ///< HDDAT Hold Time Field
#define BITSET_0X02C_T_SCLRATIO                0x2000    ///< SCL Rising Ratio Field
#define BITSET_0X02C_T_SCLHI                   0x1ff0    ///< SCL High Time Field
#define BITSET_0X02C_DMAEN                     0x8       ///< DMA Enable Bit
#define BITSET_0X02C_MASTER                    0x4       ///< Master Mode Bit
#define BITSET_0X02C_ADDRESSING                0x2       ///< Addressing Mode Bit
#define BITSET_0X02C_IICEN                     0x1       ///< I2C Engine Enable Bit

#define MAX_XFER_SZ                 (256)           ///< Maximum Transfer Size (bytes)

// Standard-mode Timing Parameters
#define STANDARD_MODE_SP_MAX         50             ///< SP Rise Time Max (ns)
#define STANDARD_MODE_SETUP_MIN      250            ///< Setup Time Min (ns)
#define STANDARD_MODE_HOLD_MIN       300            ///< Hold Time Min (ns)
#define STANDARD_MODE_HIGH_MIN       4700           ///< High Time Min (ns)
#define STANDARD_MODE_RADIO_FIX      1              ///< Radio Fix Value

// Fast-mode Timing Parameters
#define FAST_MODE_SP_MAX             50             ///< SP Rise Time Max (ns)
#define FAST_MODE_SETUP_MIN          100            ///< Setup Time Min (ns)
#define FAST_MODE_HOLD_MIN           300            ///< Hold Time Min (ns)
#define FAST_MODE_HIGH_MIN           830            ///< High Time Min (ns)
#define FAST_MODE_RADIO_FIX          2              ///< Radio Fix Value

// Fast-mode Plus Timing Parameters
#define FAST_MODE_PLUS_SP_MAX        50             ///< SP Rise Time Max (ns)
#define FAST_MODE_PLUS_SETUP_MIN     50            ///< Setup Time Min (ns)
#define FAST_MODE_PLUS_HOLD_MIN      150            ///< Hold Time Min (ns)
#define FAST_MODE_PLUS_HIGH_MIN      300            ///< High Time Min (ns)
#define FAST_MODE_PLUS_RADIO_FIX     2              ///< Radio Fix Value

#define _CMD_NO_ACT                    (0x0)   ///< No Action Command
#define _CMD_ISSUE_TRANSACTION         (0x1)   ///< Issue Transaction Command
#define _CMD_ACK                       (0x2)   ///< Send ACK Command
#define _CMD_NACK                     (0x3)   ///< Send NACK Command
#define _CMD_CLEAR_FIFO                (0x4)   ///< Clear FIFO Command
#define _CMD_RESET_I2C                 (0x5)   ///< Reset I2C Controller Command

#define _FIFOSIZE_2_BYTES               0x0   ///< 2-Byte FIFO Mode
#define _FIFOSIZE_4_BYTES               0x1   ///< 4-Byte FIFO Mode
#define _FIFOSIZE_8_BYTES               0x2   ///< 8-Byte FIFO Mode
#define _FIFOSIZE_16_BYTES              0x3   ///< 16-Byte FIFO Mode

/**
 * @brief Direction Control Register Modes
 * @details Defines data direction during I2C transactions.
 */
typedef enum _I2C_CTRL_REG_ITEM_DIR {
    I2C_MASTER_TX = 0x0,   ///< Master Transmitter Mode
    I2C_MASTER_RX = 0x1,   ///< Master Receiver Mode
    I2C_SLAVE_TX = 0x1,    ///< Slave Transmitter Mode
    I2C_SLAVE_RX = 0x0     ///< Slave Receiver Mode
} I2C_CTRL_REG_ITEM_DIR;

/**
 * @brief I2C Driver Operation States
 * @details Tracks current driver activity state machine.
 */
typedef enum _I2C_DRIVER_STATE {
    I2C_DRV_NONE = 0x0,      ///< No Active Operation
    I2C_DRV_INIT = 0x1,      ///< Initialization Phase
    I2C_DRV_POWER = 0x2,     ///< Power Management Phase
    I2C_DRV_CFG_PARAM = 0x4, ///< Parameter Configuration Phase
    I2C_DRV_MASTER_TX = 0x8, ///< Master Transmission Active
    I2C_DRV_MASTER_RX = 0x10,///< Master Reception Active
    I2C_DRV_SLAVE_TX = 0x20, ///< Slave Transmission Active
    I2C_DRV_SLAVE_RX = 0x40, ///< Slave Reception Active
    I2C_DRV_MASTER_TX_CMPL = 0x80, ///< Master TX Complete
    I2C_DRV_MASTER_RX_CMPL = 0x100,///< Master RX Complete
    I2C_DRV_SLAVE_TX_CMPL = 0x200,///< Slave TX Complete
    I2C_DRV_SLAVE_RX_CMPL = 0x400 ///< Slave RX Complete
} I2C_DRIVER_STATE;

/**
 * @brief I2C Status Register Bitfields
 * @details Maps physical status bits to logical names.
 */
typedef struct _CSK_I2C_STATUS {
    uint32_t busy :1;                   ///< Busy Flag
    uint32_t mode :1;                  ///< Mode: 0=Slave, 1=Master
    uint32_t direction :1;             ///< Direction: 0=Transmitter, 1=Receiver
    uint32_t general_call :1;          ///< General Call Indication
    uint32_t arbitration_lost :1;      ///< Arbitration Lost Flag
    uint32_t bus_error :1;             ///< Bus Error Detected
    uint32_t slave_rx_over_flow :1;    ///< Slave RX Overflow
} CSK_I2C_STATUS;

/**
 * @brief I2C Transfer Information Structure
 * @details Tracks ongoing transfer operations and buffer pointers.
 */
typedef struct _I2C_TRANSFER_INFO {
    uint8_t* tx_buf;                   ///< Transmit Buffer Pointer
    uint32_t tx_num;                   ///< Total Transmit Count
    uint32_t tx_buf_pointer;           ///< Current Transmit Pointer

    uint8_t* rx_buf;                   ///< Receive Buffer Pointer
    uint32_t rx_num;                   ///< Total Receive Count
    uint32_t rx_buf_pointer;           ///< Current Receive Pointer

    uint32_t cmpl_count;              ///< Completed Transfer Count

    uint8_t  slave_read_mid_buf[MAX_XFER_SZ]; ///< Slave Read Mid Buffer
    uint32_t slave_read_mid_buf_pointer;      ///< Mid Buffer Pointer
    uint32_t slave_read_last_rx_data_count;  ///< Last RX Data Count
} I2C_TRANSFER_INFO;

/**
 * @brief I2C Peripheral Control Block
 * @details Maintains runtime state and configuration information.
 */
typedef struct _I2C_INFO {
    CSK_I2C_SignalEvent_t cb_event;   ///< Event Callback Function

    volatile I2C_DRIVER_STATE state;  ///< Current Driver State
    CSK_POWER_STATE pwr_state;       ///< Power State

    I2C_TRANSFER_INFO trans_info;    ///< Transfer Information

    uint32_t slave_address;         ///< Slave Address

    volatile CSK_I2C_STATUS status; ///< Status Register

    uint8_t fifo_depth;             ///< FIFO Depth
    uint8_t inter_en;               ///< Interrupt/DMA Mode Select
    void* workspace;                ///< User Workspace Pointer
} I2C_INFO;

// I2C DMA Configuration
#define _I2C_TX_DMA_WIDTH      DMA_WIDTH_BYTE   ///< Transmit DMA Data Width
#define _I2C_TX_DMA_BSIZE      DMA_BSIZE_1     ///< Transmit DMA Burst Size

#define _I2C_RX_DMA_WIDTH      DMA_WIDTH_BYTE   ///< Receive DMA Data Width
#define _I2C_RX_DMA_BSIZE      DMA_BSIZE_1     ///< Receive DMA Burst Size

/**
 * @brief I2C DMA Channel Configuration
 * @details Defines DMA channel settings for transmit/receive operations.
 */
typedef struct _I2C_DMA {
    uint8_t channel;       ///< DMA Channel Number
    uint8_t reqsel;        ///< DMA Request Selector
    DMA_SignalEvent_t cb_event; ///< DMA Completion Callback
} I2C_DMA;

/**
 * @brief I2C Peripheral Resource Configuration
 * @details Groups all hardware resources required for I2C operation.
 */
typedef const struct {
    I2C_RegDef* reg;                ///< Register Block Base Address
    uint32_t irq_num;               ///< Interrupt Vector Number
    void (*irq_handler)(void);      ///< IRQ Handler Function
    I2C_DMA* dma_tx;                ///< Transmit DMA Configuration
    I2C_DMA* dma_rx;                ///< Receive DMA Configuration
    I2C_INFO* info;                 ///< Run-Time Control Block
} I2C_RESOURCES;

/********************************
 *          IIC0 Configuration
 * *****************************/
#define I2C0_DMA_TX_CH        (2)    ///< I2C0 Transmit DMA Channel
#define I2C0_DMA_RX_CH        (3)    ///< I2C0 Receive DMA Channel

// I2C0 Control Block
static I2C_INFO I2C0_Info = { 0 };

static void i2c0_dma_tx_event(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param);
static void i2c0_dma_rx_event(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param);
static void i2c0_irq_handler(void);

static void i2c0_dma_tx_event (uint32_t event, uint32_t xfer_bytes, uint32_t usr_param);
static I2C_DMA i2c0_dma_tx = {
    I2C0_DMA_TX_CH,
    HAL_CMN_DMA_SEL0_HSID_13_I2C0,
    i2c0_dma_tx_event
};

static void i2c0_dma_rx_event (uint32_t event, uint32_t xfer_bytes, uint32_t usr_param);
static I2C_DMA i2c0_dma_rx = {
    I2C0_DMA_RX_CH,
    HAL_CMN_DMA_SEL0_HSID_13_I2C0,
    i2c0_dma_rx_event
};

static void i2c0_irq_handler(void);
// I2C0 Resource Block
static I2C_RESOURCES i2c0_resources = {
    IP_I2C0,
    IRQ_I2C0_VECTOR,
    i2c0_irq_handler,
    &i2c0_dma_tx,
    &i2c0_dma_rx,
    &I2C0_Info
};

/********************************
 *          IIC1 Configuration
 * *****************************/
#define I2C1_DMA_TX_CH        (0)    ///< I2C1 Transmit DMA Channel
#define I2C1_DMA_RX_CH        (1)    ///< I2C1 Receive DMA Channel

// I2C1 Control Block
static I2C_INFO I2C1_Info = { 0 };

static void i2c1_dma_tx_event(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param);
static void i2c1_dma_rx_event(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param);
static void i2c1_irq_handler(void);

static void i2c1_dma_tx_event (uint32_t event, uint32_t xfer_bytes, uint32_t usr_param);
static I2C_DMA i2c1_dma_tx = {
    I2C1_DMA_TX_CH,
    HAL_CMN_DMA_SEL0_HSID_14_I2C1,
    i2c1_dma_tx_event
};

static void i2c1_dma_rx_event (uint32_t event, uint32_t xfer_bytes, uint32_t usr_param);
static I2C_DMA i2c1_dma_rx = {
    I2C1_DMA_RX_CH,
    HAL_CMN_DMA_SEL0_HSID_14_I2C1,
    i2c1_dma_rx_event
};

static void i2c1_irq_handler(void);
// I2C1 Resource Block
static I2C_RESOURCES i2c1_resources = {
    IP_I2C1,
    IRQ_I2C1_VECTOR,
    i2c1_irq_handler,
    &i2c1_dma_tx,
    &i2c1_dma_rx,
    &I2C1_Info
};

/**
 * @fn I2C0
 * @brief Get Address of I2C0 Resource Block
 * @return Pointer to I2C0 resource structure
 */
void* I2C0(void) {
    return (void*)&i2c0_resources;
}

/**
 * @fn I2C1
 * @brief Get Address of I2C1 Resource Block
 * @return Pointer to I2C1 resource structure
 */
void* I2C1(void) {
    return (void*)&i2c1_resources;
}

/**
 * @brief Resource Validation Macro
 * @details Checks if provided resource pointer matches valid I2C instances (I2C0/I2C1).
 * @param res Pointer to I2C resources structure to validate
 */
#define CHECK_RESOURCES(res)  do{\
        if((res != &i2c0_resources) && (res != &i2c1_resources)){\
            return CSK_DRIVER_ERROR_PARAMETER;\
        }\
}while(0)

static void i2cx_master_fifo_write(I2C_RESOURCES* i2c);
static void i2cx_slave_fifo_write(I2C_RESOURCES* i2c);
static void i2cx_master_fifo_read(I2C_RESOURCES* i2c);
static void i2cx_slave_fifo_read(I2C_RESOURCES* i2c, uint8_t is_fifo_full);

static void __I2C_RES_CLK_ENABLE(void* res) {
    if (res == &i2c0_resources) {
        __HAL_CRM_I2C0_CLK_ENABLE();
    } else if (res == &i2c1_resources) {
        // Clocking handled elsewhere for I2C1
    } else {
        // Error condition - invalid resource
    }
}

/**
 * @fn I2C_Initialize
 * @brief Initialize I2C Peripheral
 * @param[in] res Pointer to I2C resources structure
 * @param[in] cb_event Event callback function
 * @param[in] workspace User context data pointer
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details Sets up initial state, registers callback, and enables clock.
 */
int32_t
I2C_Initialize(void* res, CSK_I2C_SignalEvent_t cb_event, void* workspace)
{
    CHECK_RESOURCES(res);

    I2C_RESOURCES* i2c = (I2C_RESOURCES*)res;

    if (i2c->info->state & I2C_DRV_INIT)
    {
        return CSK_DRIVER_OK;
    }

    i2c->info->cb_event = cb_event;
    i2c->info->workspace = workspace;

    i2c->info->state |= I2C_DRV_INIT;
    return CSK_DRIVER_OK;
}

/**
 * @brief Uninitialize I2C resources
 *
 * This function reverses the initialization process by cleaning up allocated
 * resources and resetting driver state. It handles both interrupt and DMA modes.
 *
 * @param[in] res   Pointer to I2C resource structure
 *
 * @pre CHECK_RESOURCES(res) must validate the input pointer before execution
 *
 * @return CSK_DRIVER_OK on success
 * @retval CSK_DRIVER_ERROR if resource validation fails
 *
 * @details Operations performed:
 *          - Clears event callback function
 *          - Calls dma_uninitialize() if interrupt mode is disabled
 *          - Sets driver state to NONE
 */
int32_t
I2C_Uninitialize(void* res)
{
    CHECK_RESOURCES(res);

    I2C_RESOURCES* i2c = (I2C_RESOURCES*)res;

    i2c->info->cb_event = NULL;

    if(!i2c->info->inter_en){
        dma_uninitialize();
    }

    // clear & set driver state to none
    i2c->info->state = I2C_DRV_NONE;

    return CSK_DRIVER_OK;
}

/**
 * @brief Control I2C power states
 *
 * Manages hardware power sequencing based on target power state. Supports three
 * power levels: FULL (active), LOW (sleep), and OFF (deep sleep). Performs
 * clock gating, reset isolation, and interrupt management per state transition.
 *
 * @param[in] res     Pointer to I2C resource structure
 * @param[in] state   Target power state (@ref CSK_POWER_STATE)
 *
 * @pre CHECK_RESOURCES(res) must validate the input pointer before execution
 *
 * @return CSK_DRIVER_OK on successful state transition
 * @retval CSK_DRIVER_ERROR_PARAMETER for invalid controller instance
 * @retval CSK_DRIVER_ERROR_UNSUPPORTED for unsupported power states
 *
 * @details State Machine:
 *          POWER_OFF: Disables controller, applies reset isolation, ungates clock
 *          POWER_FULL: Reverses POWER_OFF actions, initializes FIFO and interrupts
 *          POWER_LOW: Currently empty - reserved for future optimizations
 */
int32_t
I2C_PowerControl(void* res, CSK_POWER_STATE state)
{
    CHECK_RESOURCES(res);

    I2C_RESOURCES* i2c = (I2C_RESOURCES*)res;

    i2c->info->pwr_state = state;

    switch (state)
    {
    case CSK_POWER_OFF:
        disable_IRQ(i2c->irq_num);

        // I2C reset controller
        i2c->reg->REG_CMD.bit.CMD = _CMD_RESET_I2C;

        // I2C disable
        i2c->reg->REG_SETUP.bit.IICEN = 0x0;

        // reset i2c & enable i2c clock
        if (i2c == &i2c0_resources){
            __HAL_PMU_I2C0_RST_ENABLE();
            __HAL_CRM_I2C0_CLK_DISABLE();
        }  else if (i2c == &i2c1_resources) {
            __HAL_PMU_I2C1_RST_ENABLE();
            __HAL_CRM_I2C1_CLK_DISABLE();
        }
        else {
            return CSK_DRIVER_ERROR_PARAMETER;
        }

        i2c->info->state &= (~I2C_DRV_POWER);

        register_ISR(i2c->irq_num , NULL, NULL);

        break;

    case CSK_POWER_LOW:
        break;

    case CSK_POWER_FULL:

        // reset i2c & enable i2c clock
        if (i2c == &i2c0_resources){
            __HAL_PMU_I2C0_RST_ENABLE();
            __HAL_CRM_I2C0_CLK_ENABLE();
        } else if (i2c == &i2c1_resources){
            __HAL_PMU_I2C1_RST_ENABLE();
            __HAL_CRM_I2C1_CLK_ENABLE();
        }
        else {
            return CSK_DRIVER_ERROR_PARAMETER;
        }

        // I2C query FIFO depth
        // read only FIFO size config
        switch (i2c->reg->REG_CFG.bit.FIFOSIZE)
        {
        case _FIFOSIZE_2_BYTES:
            i2c->info->fifo_depth = 2;
            break;
        case _FIFOSIZE_4_BYTES:
            i2c->info->fifo_depth = 4;
            break;
        case _FIFOSIZE_8_BYTES:
            i2c->info->fifo_depth = 8;
            break;
        case _FIFOSIZE_16_BYTES:
            i2c->info->fifo_depth = 16;
            break;
        }

        // I2C reset controller
        i2c->reg->REG_CMD.all = _CMD_RESET_I2C;

        // I2C setting: slave mode(default), FIFO(CPU) mode, 7-bit slave address, Ctrl enable
        i2c->reg->REG_SETUP.all = 0x0;
        i2c->reg->REG_SETUP.bit.IICEN = 0x1;

        // I2C setting: enable completion interrupt & address hit interrupt
        // For slave device
        i2c->reg->REG_INTEN.all = (BITSET_0X014_CMPL | BITSET_0X014_ADDRHIT);

        // clear status
        i2c->info->status.busy = 0;
        // define mode => 0:slave / 1:master
        i2c->info->status.mode = 0;
        // define direction => 0:tx / 1:rx
        i2c->info->status.direction = 0;
        i2c->info->status.arbitration_lost = 0;
        i2c->info->status.bus_error = 0;

        i2c->info->state = I2C_DRV_POWER;

        register_ISR(i2c->irq_num, i2c->irq_handler, NULL);

        enable_IRQ(i2c->irq_num);
        break;

    default:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    return CSK_DRIVER_OK;
}

/**
 * @brief Initiate master mode transmission
 *
 * Starts an I2C master write transaction with comprehensive error checking and
 * automatic mode configuration. Supports both interrupt and DMA transfer modes.
 *
 * @param[in] res      Pointer to I2C resource structure
 * @param[in] addr     Target slave address (7/10-bit)
 * @param[in] data     Pointer to transmit buffer
 * @param[in] num      Number of bytes to transmit
 * @param[in] xfer_pending Flag indicating ongoing transfer status
 *
 * @pre CHECK_RESOURCES(res) must validate the input pointer before execution
 * @pre Power state must be CSK_POWER_FULL
 * @pre Bus must not be busy (i2c->info->status.busy == 0)
 *
 * @return CSK_DRIVER_OK on successful transaction initiation
 * @retval CSK_DRIVER_ERROR_PARAMETER for invalid address or parameters
 * @retval CSK_DRIVER_ERROR_UNSUPPORTED when not in FULL power state
 * @retval CSK_DRIVER_ERROR_BUSY if bus is occupied
 * @retval CSK_DRIVER_ERROR during DMA setup failure
 *
 * @details Key steps:
 *          - Validates slave address range (<= 0x3FF)
 *          - Checks power state and bus availability
 *          - Configures master transmit mode (MASTER=1, DIRECTION=0)
 *          - Sets phase controls based on xfer_pending flag
 *          - Programs FIFO threshold and DMA/Interrupt mode selection
 *          - Issues transaction command via REG_CMD
 */
int32_t
I2C_MasterTransmit(void* res, uint32_t addr, const uint8_t* data, uint32_t num,
        bool xfer_pending)
{
    CHECK_RESOURCES(res);

    I2C_RESOURCES* i2c = (I2C_RESOURCES*)res;

    // max 10-bit address(0x3FF), null data or num is no payload for acknowledge polling
    // If no I2C payload, set Phase_data=0x0
    if (addr > 0x3FF)
    {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (i2c->info->pwr_state != CSK_POWER_FULL)
    {
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    // Transfer operation in progress, or slave stalled
    if (i2c->info->status.busy)
    {
        return CSK_DRIVER_ERROR_BUSY;
    }

    i2c->info->status.busy = 1;
    // define mode => 0:slave / 1:master
    i2c->info->status.mode = 1;
    // define direction => 0:tx / 1:rx
    i2c->info->status.direction = 0;
    i2c->info->status.arbitration_lost = 0;
    i2c->info->status.bus_error = 0;

    //clear & set driver state to master tx before issue transaction
    i2c->info->state = I2C_DRV_MASTER_TX;

    // I2C reset controller, including disable all I2C interrupts & clear fifo
    i2c->reg->REG_CMD.bit.CMD = _CMD_RESET_I2C;

    // I2C master, FIFO(CPU) mode, Ctrl enable
    if (i2c->info->inter_en)
    {
        // Interrupt mode
        i2c->reg->REG_SETUP.bit.DMAEN = 0x0;
    }
    else
    {
        i2c->reg->REG_SETUP.bit.DMAEN = 0x1;
    }

    i2c->reg->REG_SETUP.bit.MASTER = 0x1;
    i2c->reg->REG_SETUP.bit.IICEN = 0x1;

    // I2C phase start enable, phase addr enable, phase data enable, phase stop enable.
    // If I2C data transaction w/o I2C payload, remember to clear data bit.
    // xfer_pending: Transfer operation is pending - Stop condition will not be generated.
    // The bus is busy when a START condition is on bus and it ends when a STOP condition is seen.
    // 10-bit slave address must set STOP bit.
    // I2C direction : master tx, set xfer data count.
    {
        uint32_t temp_para = 0;
        temp_para = (BITSET_0X024_PHASE_START | BITSET_0X024_PHASE_ADDR | (!xfer_pending << 9)
                    | (num & BITSET_0X024_DATACNT));

        if(num){
            temp_para |= BITSET_0X024_PHASE_DATA;
        }

        i2c->reg->REG_CTRL.all = temp_para;
    }

    i2c->info->slave_address = addr;

    i2c->info->trans_info.tx_num = num;
    i2c->info->trans_info.cmpl_count = 0;
    i2c->info->trans_info.tx_buf_pointer = 0;
    i2c->info->trans_info.tx_buf = (uint8_t*)data;

    // I2C slave address, general call address = 0x0(7-bit or 10-bit)
    i2c->reg->REG_ADDR.all = i2c->info->slave_address & BITSET_0X01C_ADDR_MASK;

    {
        uint32_t temp_para = (BITSET_0X014_CMPL | BITSET_0X014_ARBLOS);

        if (i2c->info->inter_en)
        {
            // I2C write a patch of data(FIFO_Depth) to FIFO,
            // it will be consumed empty if data is actually issued on I2C bus,
            // currently FIFO is not empty, will not trigger FIFO_EMPTY interrupt
            i2cx_master_fifo_write(i2c);

            if (num){
                // enable
                temp_para |= BITSET_0X014_FIFOEMPTY;
            } else {
                // disable
                temp_para &= (~ BITSET_0X014_FIFOEMPTY);
            }
        } else {
            int32_t stat;

//          IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_15 = 0x1;
//          IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_11 = 0x1;
//          mmio_write32_field(CMN_SYSCFG_BASE + 0x98, 0, 1, 13);
//          mmio_write32_field(CMN_SYSCFG_BASE + 0x98, 0, 1, 14);

            dma_channel_select(
                &i2c->dma_tx->channel,
                i2c->dma_tx->cb_event,
                0,
                DMA_CACHE_SYNC_SRC);
            if (i2c->dma_tx->channel == DMA_CHANNEL_ANY) {
                return CSK_DRIVER_ERROR;
            }

            stat = dma_channel_configure (i2c->dma_tx->channel,
                        (uint32_t) i2c->info->trans_info.tx_buf,
                        (uint32_t) (&(i2c->reg->REG_DATA.all)),
                        i2c->info->trans_info.tx_num,
                        DMA_CH_CTLL_DST_WIDTH(_I2C_TX_DMA_WIDTH) | DMA_CH_CTLL_SRC_WIDTH(_I2C_TX_DMA_WIDTH) |\
                      DMA_CH_CTLL_DST_BSIZE(_I2C_TX_DMA_BSIZE) | DMA_CH_CTLL_SRC_BSIZE(_I2C_TX_DMA_BSIZE) |\
                      DMA_CH_CTLL_DST_FIX | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_TTFC_M2P |\
                      DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN, // control
                      DMA_CH_CFGL_CH_PRIOR(1), // config_low
                      DMA_CH_CFGH_FIFO_MODE | DMA_CH_CFGH_DST_PER(i2c->dma_tx->reqsel), // config_high
                      0, 0);

            if(stat == -1){
                return CSK_DRIVER_ERROR;
            }
        }

        i2c->reg->REG_INTEN.all = temp_para;
    }

    // I2C Write 0x1 to the Command register to issue the transaction
    i2c->reg->REG_CMD.bit.CMD = _CMD_ISSUE_TRANSACTION;

    return CSK_DRIVER_OK;
}

/**
 * @brief Initiate master mode reception
 *
 * Starts an I2C master read transaction with similar validation logic as transmit
 * but configured for receive direction. Includes specialized DMA channel setup
 * for peripheral-to-memory transfers.
 *
 * @param[in] res      Pointer to I2C resource structure
 * @param[in] addr     Target slave address (7/10-bit)
 * @param[in] data     Pointer to receive buffer
 * @param[in] num      Number of bytes to receive
 * @param[in] xfer_pending Flag indicating ongoing transfer status
 *
 * @pre CHECK_RESOURCES(res) must validate the input pointer before execution
 * @pre Power state must be CSK_POWER_FULL
 * @pre Bus must not be busy (i2c->info->status.busy == 0)
 *
 * @return CSK_DRIVER_OK on successful transaction initiation
 * @retval CSK_DRIVER_ERROR_PARAMETER for invalid address or parameters
 * @retval CSK_DRIVER_ERROR_UNSUPPORTED when not in FULL power state
 * @retval CSK_DRIVER_ERROR_BUSY if bus is occupied
 * @retval CSK_DRIVER_ERROR during DMA setup failure
 *
 * @details Differences from MasterTransmit:
 *          - Sets DIRECTION=1 for receive operation
 *          - Uses different DMA source/destination mapping (P2M)
 *          - Adds BITSET_0X024_DIR flag in REG_CTRL configuration
 *          - Different DMA channel configuration parameters
 */
int32_t
I2C_MasterReceive(void* res, uint32_t addr, uint8_t* data, uint32_t num,
        bool xfer_pending)
{
    CHECK_RESOURCES(res);

    I2C_RESOURCES* i2c = (I2C_RESOURCES*)res;

    // max 10-bit address(0x3FF), null data or num is no payload for acknowledge polling
    // If no I2C payload, set Phase_data=0x0
    if (addr > 0x3FF)
    {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (i2c->info->pwr_state != CSK_POWER_FULL)
    {
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    // Transfer operation in progress, or slave stalled
    if (i2c->info->status.busy)
    {
        return CSK_DRIVER_ERROR_BUSY;
    }

    i2c->info->status.busy = 1;
    // define mode => 0:slave / 1:master
    i2c->info->status.mode = 1;
    // define direction => 0:tx / 1:rx
    i2c->info->status.direction = 1;
    i2c->info->status.arbitration_lost = 0;
    i2c->info->status.bus_error = 0;

    //clear & set driver state to master rx before issue transaction
    i2c->info->state = I2C_DRV_MASTER_RX;

    // I2C reset controller, including disable all I2C interrupts & clear fifo
    i2c->reg->REG_CMD.bit.CMD = _CMD_RESET_I2C;

    // I2C master, FIFO(CPU) mode, Ctrl enable
    if (i2c->info->inter_en)
    {
        // Interrupt mode
        i2c->reg->REG_SETUP.bit.DMAEN = 0x0;
    }
    else
    {
        i2c->reg->REG_SETUP.bit.DMAEN = 0x1;
    }

    i2c->reg->REG_SETUP.bit.MASTER = 0x1;
    i2c->reg->REG_SETUP.bit.IICEN = 0x1;

    // I2C phase start enable, phase addr enable, phase data enable, phase stop enable.
    // If I2C data transaction w/o I2C payload, remember to clear data bit.
    // xfer_pending: Transfer operation is pending - Stop condition will not be generated.
    // The bus is busy when a START condition is on bus and it ends when a STOP condition is seen.
    // 10-bit slave address must set STOP bit.
    // I2C direction : master rx, set xfer data count.
    {
        uint32_t temp_para = 0;
        temp_para = (BITSET_0X024_PHASE_START | BITSET_0X024_PHASE_ADDR | (!xfer_pending << 9)
                    | (num & BITSET_0X024_DATACNT) | BITSET_0X024_DIR);

        if(num){
            temp_para |= BITSET_0X024_PHASE_DATA;
        }

        i2c->reg->REG_CTRL.all = temp_para;
    }

    i2c->info->slave_address = addr;

    i2c->info->trans_info.rx_num = num;
    i2c->info->trans_info.cmpl_count = 0;
    i2c->info->trans_info.rx_buf_pointer = 0;
    i2c->info->trans_info.rx_buf = (uint8_t*)data;

    // I2C slave address, general call address = 0x0(7-bit or 10-bit)
    i2c->reg->REG_ADDR.all = i2c->info->slave_address & BITSET_0X01C_ADDR_MASK;

    // I2C Enable the Completion Interrupt, Enable the FIFO Full Interrupt
    // I2C Enable the Arbitration Lose Interrupt, master mode only
    {
        uint32_t temp_para = (BITSET_0X014_CMPL | BITSET_0X014_ARBLOS);

        if (i2c->info->inter_en)
        {
            temp_para |= BITSET_0X014_FIFOFULL;
        } else {
            int32_t stat;

//			IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_15 = 0x1;
//			IP_SYSCTRL->REG_CP_DMA_HS.bit.CP_DMA_HS_SEL_11 = 0x1;

            dma_channel_select(
                &i2c->dma_rx->channel,
                i2c->dma_rx->cb_event,
                0,
                DMA_CACHE_SYNC_DST);
            if (i2c->dma_rx->channel == DMA_CHANNEL_ANY) {
                return CSK_DRIVER_ERROR;
            }

            stat = dma_channel_configure (i2c->dma_rx->channel,
                        (uint32_t) (&(i2c->reg->REG_DATA.all)),
                        (uint32_t) i2c->info->trans_info.rx_buf,
                        i2c->info->trans_info.rx_num,
                        DMA_CH_CTLL_DST_WIDTH(_I2C_RX_DMA_WIDTH) | DMA_CH_CTLL_SRC_WIDTH(_I2C_RX_DMA_WIDTH) |\
                      DMA_CH_CTLL_DST_BSIZE(_I2C_RX_DMA_BSIZE) | DMA_CH_CTLL_SRC_BSIZE(_I2C_RX_DMA_BSIZE) |\
                      DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_TTFC_P2M |\
                      DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN, // control
                      DMA_CH_CFGL_CH_PRIOR(1), // config_low
                      DMA_CH_CFGH_FIFO_MODE | DMA_CH_CFGH_SRC_PER(i2c->dma_rx->reqsel), // config_high
                      0, 0);

            if(stat == -1){
                return CSK_DRIVER_ERROR;
            }
        }

        i2c->reg->REG_INTEN.all = temp_para;
    }

    // I2C Write 0x1 to the Command register to issue the transaction
    i2c->reg->REG_CMD.bit.CMD = _CMD_ISSUE_TRANSACTION;

    return CSK_DRIVER_OK;
}

/**
 * @brief Initiate slave mode transmission with dynamic byte counting
 *
 * This function starts a slave transmit operation where data is sent one byte at a time.
 * The transmission continues until completion or NACK is detected. It sets up the necessary
 * state machine flags and initializes the transfer information structure.
 *
 * @param[in] res   Pointer to I2C resource structure
 * @param[in] data  Pointer to transmit buffer
 * @param[in] num   Number of bytes to transmit
 *
 * @pre CHECK_RESOURCES(res) must validate the input pointer before execution
 * @pre Power state must be CSK_POWER_FULL
 * @pre Bus must not be busy (i2c->info->status.busy == 0)
 *
 * @return CSK_DRIVER_OK on successful initialization
 * @retval CSK_DRIVER_ERROR_UNSUPPORTED when not in FULL power state
 * @retval CSK_DRIVER_ERROR_BUSY if bus is occupied
 *
 * @details Key operations:
 *          - Sets slave mode (MODE=0) and transmit direction (DIRECTION=0)
 *          - Stores transfer parameters in trans_info structure
 *          - Writes initial data to slave FIFO buffer
 *          - Sets driver state to I2C_DRV_SLAVE_TX
 */
int32_t
I2C_SlaveTransmit(void* res, const uint8_t* data, uint32_t num)
{
    CHECK_RESOURCES(res);

    I2C_RESOURCES* i2c = (I2C_RESOURCES*)res;

    if (i2c->info->pwr_state != CSK_POWER_FULL)
    {
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    // Transfer operation in progress, or slave stalled
    if (i2c->info->status.busy)
    {
        return CSK_DRIVER_ERROR_BUSY;
    }

    i2c->info->status.busy = 1;
    // define mode => 0:slave / 1:master
    i2c->info->status.mode = 0;
    // define direction => 0:tx / 1:rx
    i2c->info->status.direction = 0;
    i2c->info->status.arbitration_lost = 0;
    i2c->info->status.bus_error = 0;

    // clear & set driver state to slave tx before issue transaction
    i2c->info->state = I2C_DRV_SLAVE_TX;

    // I2C xfer data count
    // If DMA is not enabled, DataCnt is the number of
    // bytes transmitted/received from the bus master.
    // It is reset to 0 when the controller is addressed
    // and then increased by one for each byte of data
    // transmitted/received
    i2c->info->trans_info.tx_num = num;
    i2c->info->trans_info.cmpl_count = 0;
    i2c->info->trans_info.tx_buf_pointer = 0;
    i2c->info->trans_info.tx_buf = (uint8_t*)data;

    i2cx_slave_fifo_write(i2c);

    return CSK_DRIVER_OK;
}

/**
 * @brief Read data from slave receive buffer
 *
 * Retrieves received data from the intermediate buffer used by the middleware.
 * Does not affect the slave receiver state machine directly. Designed to work
 * independently from the actual slave receive operation.
 *
 * @param[in] res   Pointer to I2C resource structure
 * @param[out] data Pointer to receive buffer
 * @param[in] num   Number of bytes to read
 *
 * @pre CHECK_RESOURCES(res) must validate the input pointer before execution
 * @pre Power state must be CSK_POWER_FULL
 *
 * @return CSK_DRIVER_OK on successful read
 * @retval CSK_DRIVER_ERROR_PARAMETER if requested size exceeds MAX_XFER_SZ
 *
 * @details Special considerations:
 *          - Does NOT set busy flag to avoid interfering with slave TX
 *          - Uses direct memory copy from middleware's intermediate buffer
 *          - Updates internal read pointer tracking
 */
int32_t
I2C_SlaveReceive(void* res, uint8_t* data, uint32_t num)
{
    CHECK_RESOURCES(res);

    I2C_RESOURCES* i2c = (I2C_RESOURCES*)res;

    if (i2c->info->pwr_state != CSK_POWER_FULL)
    {
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    // since middleware just read data from Xfer_Data_Rd_Buf,
    // no set busy flag, driver slave rx is independent of
    // middleware slave rx, if set busy flag will affect
    // slave tx behavior
    // define mode => 0:slave / 1:master
    i2c->info->status.mode = 0;
    // define direction => 0:tx / 1:rx
    i2c->info->status.direction = 1;
    i2c->info->status.bus_error = 0;

    // I2C xfer data count
    // If DMA is not enabled, DataCnt is the number of
    // bytes transmitted/received from the bus master.
    // It is reset to 0 when the controller is addressed
    // and then increased by one for each byte of data
    // transmitted/received

    // no I2C reset controller, no I2C clear fifo since middleware just read data from Xfer_Data_Rd_Buf
    // w/ minimal change I2C HW setting

    // Xfer_Data_Rd_Buf already read the data from hw fifo and keep,
    // currently middleware able to take from the buffer */
    // error hit
    if (num > MAX_XFER_SZ)
    {
        return CSK_DRIVER_ERROR_PARAMETER;
    }
    __builtin_memcpy (data, &i2c->info->trans_info.slave_read_mid_buf[i2c->info->trans_info.slave_read_mid_buf_pointer], num);

    i2c->info->trans_info.slave_read_mid_buf_pointer += num;

    return CSK_DRIVER_OK;
}

/**
 * @brief Get current data transfer count
 *
 * Returns the number of bytes successfully transferred in the current operation.
 * Works for both transmit and receive operations across all modes.
 *
 * @param[in] res   Pointer to I2C resource structure
 *
 * @pre CHECK_RESOURCES(res) must validate the input pointer before execution
 *
 * @return Number of completed bytes in current transfer
 * @retval Negative values indicate errors (via global error codes)
 *
 * @note This reflects actual hardware counter readings adjusted for different modes
 */
int32_t
I2C_GetDataCount(void* res)
{
    CHECK_RESOURCES(res);

    I2C_RESOURCES* i2c = (I2C_RESOURCES*)res;

    return (i2c->info->trans_info.cmpl_count);
}

/**
 * @brief General purpose I2C control interface
 *
 * Centralized control mechanism supporting various configuration options including:
 * - Own address setup (7/10-bit selection)
 * - Bus speed configuration (Standard/Fast/Fast+)
 * - Bus clearing and transaction abortion
 * - DMA/Interrupt mode switching
 *
 * @param[in] res     Pointer to I2C resource structure
 * @param[in] control Control command (@ref CSK_I2C_CONTROL)
 * @param[in] arg0    Command-specific argument
 *
 * @pre CHECK_RESOURCES(res) must validate the input pointer before execution
 * @pre Power state must be CSK_POWER_FULL for most operations
 *
 * @return CSK_DRIVER_OK on successful control operation
 * @retval CSK_DRIVER_ERROR_UNSUPPORTED for invalid controls or power states
 * @retval CSK_DRIVER_ERROR_PARAMETER for invalid arguments
 *
 * @details Supported commands:
 *          CSK_I2C_OWN_ADDRESS: Set own slave address (supports 7/10-bit)
 *          CSK_I2C_BUS_SPEED: Configure bus timing parameters
 *          CSK_I2C_BUS_CLEAR: Reset controller and clear FIFO
 *          CSK_I2C_ABORT_TRANSFER: Terminate ongoing transfers
 *          CSK_I2C_TRANSMIT_MODE: Switch between DMA/Interrupt modes
 */
int32_t
I2C_Control(void* res, uint32_t control, uint32_t arg0)
{
    CHECK_RESOURCES(res);

    I2C_RESOURCES* i2c = (I2C_RESOURCES*)res;

    int32_t status = CSK_DRIVER_OK;

    if (i2c->info->pwr_state != CSK_POWER_FULL)
    {
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    switch (control)
    {
    // middleware use control code
    case CSK_I2C_OWN_ADDRESS:
        if (arg0 & CSK_I2C_ADDRESS_10BIT)
        {
            // I2C 10-bit slave address
            i2c->reg->REG_SETUP.bit.ADDRESSING = 0x1;
        }
        else
        {
            // I2C 7-bit slave address
            i2c->reg->REG_SETUP.bit.ADDRESSING = 0x0;
        }

        // I2C slave address, general call address = 0x0(7-bit or 10-bit)
        i2c->reg->REG_ADDR.bit.ADDR = (arg0 & (BITSET_0X01C_ADDR_MASK));
        break;
    case CSK_I2C_BUS_SPEED:
    {
        uint32_t temp_para;
        temp_para = i2c->reg->REG_SETUP.all;
        // clear previous setting
        temp_para &= (~(BITSET_0X02C_T_SUDAT | BITSET_0X02C_T_SP | BITSET_0X02C_T_HDDAT
                    | BITSET_0X02C_T_SCLRATIO | BITSET_0X02C_T_SCLHI));

        // TODO
        uint32_t pclk;
        if (i2c == &i2c0_resources){
            pclk = 1000000000 / CRM_GetCmn_pclkFreq();
        }else if (i2c == &i2c1_resources){
            pclk = 1000000000 / CRM_GetCmn_pclkFreq();
        }else {
            return CSK_DRIVER_ERROR_PARAMETER;
        }

        uint16_t sudat, sp, hddat, ratio, sclhi = 0;

        switch (arg0)
        {
        case CSK_I2C_BUS_SPEED_STANDARD:
            // I2C speed standard
            sp = STANDARD_MODE_SP_MAX / pclk;
            sudat = (STANDARD_MODE_SETUP_MIN / pclk) - (4 + sp);
            hddat = (STANDARD_MODE_HOLD_MIN / pclk) - (4 + sp);
            sclhi = (STANDARD_MODE_HIGH_MIN / pclk) - (4 + sp);
            ratio = STANDARD_MODE_RADIO_FIX - 1;
            break;
        case CSK_I2C_BUS_SPEED_FAST:
            // I2C speed fast
            sp = FAST_MODE_SP_MAX / pclk;
            sudat = (FAST_MODE_SETUP_MIN / pclk) - (4 + sp);
            hddat = (FAST_MODE_HOLD_MIN / pclk) - (4 + sp);
            sclhi = (FAST_MODE_HIGH_MIN / pclk) - (4 + sp);
            ratio = FAST_MODE_RADIO_FIX - 1;
            break;
        case CSK_I2C_BUS_SPEED_FAST_PLUS:
            // I2C speed fast plus
            sp = FAST_MODE_PLUS_SP_MAX / pclk;
            sudat = (FAST_MODE_PLUS_SETUP_MIN / pclk) - (4 + sp);
            hddat = (FAST_MODE_PLUS_HOLD_MIN / pclk) - (4 + sp);
            sclhi = (FAST_MODE_PLUS_HIGH_MIN / pclk) - (4 + sp);
            ratio = FAST_MODE_PLUS_RADIO_FIX - 1;
            break;
        default:
            return CSK_DRIVER_ERROR_UNSUPPORTED;
        }

        temp_para |= ((sudat << 24) |
                  (sp << 21) |
                  (hddat << 16) |
                  (ratio << 13) |
                  (sclhi << 4));

        // apply
        i2c->reg->REG_SETUP.all = temp_para;
        break;
    }
    case CSK_I2C_BUS_CLEAR:
        // I2C reset controller, including disable all I2C interrupts & clear fifo
        i2c->reg->REG_CMD.bit.CMD = _CMD_RESET_I2C;
        break;
    case CSK_I2C_ABORT_TRANSFER:
        // I2C reset controller ??
        // I2C reset controller, including disable all I2C interrupts & clear fifo

        // in dma mode
        // TODO get count
        if(!i2c->info->inter_en){
            if((i2c->info->state & I2C_DRV_MASTER_TX) || (i2c->info->state & I2C_DRV_SLAVE_TX)){
                i2c->info->trans_info.cmpl_count = dma_channel_get_count(i2c->dma_tx->channel) - i2c->reg->REG_CTRL.bit.DATACNT;
                dma_channel_disable(i2c->dma_tx->channel, 0);
            }else if((i2c->info->state & I2C_DRV_MASTER_RX) || (i2c->info->state & I2C_DRV_SLAVE_RX)){
                i2c->info->trans_info.cmpl_count = dma_channel_get_count(i2c->dma_rx->channel);
                dma_channel_disable(i2c->dma_rx->channel, 0);
            }
        } else {
            if(i2c->info->state & I2C_DRV_MASTER_TX){
                i2c->info->trans_info.cmpl_count = i2c->info->trans_info.tx_num - i2c->reg->REG_CTRL.bit.DATACNT;
            }
            else if (i2c->info->state & I2C_DRV_SLAVE_TX){
                i2c->info->trans_info.cmpl_count = i2c->reg->REG_CTRL.bit.DATACNT;
            }
            else if(i2c->info->state & I2C_DRV_MASTER_RX){
                i2c->info->trans_info.cmpl_count = i2c->info->trans_info.rx_num - i2c->reg->REG_CTRL.bit.DATACNT;
            }
            else if (i2c->info->state & I2C_DRV_SLAVE_RX){
                i2c->info->trans_info.cmpl_count = i2c->reg->REG_CTRL.bit.DATACNT;
            }

        }

        i2c->info->status.busy = 0;
        // reset i2c
        i2c->reg->REG_CMD.bit.CMD = _CMD_RESET_I2C;

        return status;

    case CSK_I2C_TRANSMIT_MODE:
        // arg0 = 1, means DMA mode
        if(arg0){
            i2c->info->inter_en = 0x0;
            i2c->reg->REG_SETUP.bit.DMAEN = 0x1;
            dma_initialize();
        // arg0 = 0, means Interrupt mode
        }else{
            i2c->info->inter_en = 0x1;
            i2c->reg->REG_SETUP.bit.DMAEN = 0x0;
        }
        break;
    default:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    i2c->info->state = I2C_DRV_CFG_PARAM;

    return status;
}

/**
 * @brief Completion handler for I2C transactions
 *
 * Processes completion interrupts from hardware, updating state machines and
 * managing data flow for both master and slave modes. Handles different modes:
 * - Master transmit/receive
 * - Slave transmit/receive
 * - DMA vs Interrupt modes
 *
 * @param[in] i2c Pointer to I2C resource structure
 *
 * @details State management logic:
 *          - Master mode: Disables interrupts after completion
 *          - Slave mode: Maintains interrupt enables for continued operation
 *          - Updates completion counts based on hardware registers
 *          - Clears busy flags when operations complete
 *          - Flushes FIFO on slave mode completions
 */
static void
i2c_cmpl_handler(I2C_RESOURCES* i2c)
{
    // master mode
    if (i2c->reg->REG_SETUP.bit.MASTER == 1)
    {
        // I2C disable all Interrupts in the Interrupt Enable Register
        i2c->reg->REG_INTEN.all = (~ BITSET_0X014_INTEN_MASK);
    }
    else
    {
        // I2C no disable all Interrupts in the Interrupt Enable Register,
        // keep previous setting for slave tx */
    }

    // check the DataCnt field of the Control Register
    // to know if all data are successfully transmitted.
    // -> Master: The number of bytes to transmit/receive.
    // 0 means 256 bytes. DataCnt will be decreased by one
    // for each byte transmitted/received.
    if ((i2c->info->state & I2C_DRV_MASTER_TX) || (i2c->info->state & I2C_DRV_MASTER_RX)) {

        if (i2c->info->state & I2C_DRV_MASTER_TX) {
            if (i2c->info->inter_en)
            {
                i2c->info->trans_info.cmpl_count = i2c->info->trans_info.tx_num - i2c->reg->REG_CTRL.bit.DATACNT;
            }
            else
            {
                i2c->info->trans_info.tx_buf_pointer = dma_channel_get_count(i2c->dma_tx->channel);
                i2c->info->trans_info.cmpl_count = dma_channel_get_count(i2c->dma_tx->channel);
            }

            // clear & set driver state to master tx complete
            i2c->info->state = I2C_DRV_MASTER_TX_CMPL;

            // clear busy bit on i2c complete event as master dma/cpu tx
            i2c->info->status.busy = 0;
        }

        if (i2c->info->state & I2C_DRV_MASTER_RX) {
            if (i2c->info->inter_en) {
                i2cx_master_fifo_read(i2c);

                i2c->info->trans_info.cmpl_count = i2c->info->trans_info.rx_num - i2c->reg->REG_CTRL.bit.DATACNT;

                // clear & set driver state to master rx complete
                i2c->info->state = I2C_DRV_MASTER_RX_CMPL;

                // clear busy bit on i2c complete event as master cpu rx
                i2c->info->status.busy = 0;
            }
            else
            {
                i2c->info->trans_info.rx_buf_pointer = dma_channel_get_count(i2c->dma_rx->channel);
                i2c->info->trans_info.cmpl_count = dma_channel_get_count(i2c->dma_rx->channel);

                // clear & set driver state to master rx complete
                i2c->info->state = I2C_DRV_MASTER_RX_CMPL;
            }


        }
    }

    // check the DataCnt field of the Control Register
    // to know if all data are successfully transmitted.
    // -> Slave: the meaning of DataCnt depends on the
    // DMA mode:
    // If DMA is not enabled, DataCnt is the number of
    // bytes transmitted/received from the bus master.
    // It is reset to 0 when the controller is addressed
    // and then increased by one for each byte of data
    // transmitted/received.
    // If DMA is enabled, DataCnt is the number of
    // bytes to transmit/receive. It will not be reset to 0
    // when the slave is addressed and it will be
    // decreased by one for each byte of data
    // transmitted/received..
    if ((i2c->info->state & I2C_DRV_SLAVE_TX) || (i2c->info->state & I2C_DRV_SLAVE_RX)) {
        // I2C_FIFO mode
        if (i2c->info->inter_en) {
            if (i2c->info->state & I2C_DRV_SLAVE_TX) {
                // I2C Disable the Byte Transmit Interrupt in the Interrupt Enable Register
                i2c->reg->REG_INTEN.all &= (~ BITSET_0X014_BYTETRANS);

                // clear & set driver state to slave tx complete
                i2c->info->state = I2C_DRV_SLAVE_TX_CMPL;
            }

            if (i2c->info->state & I2C_DRV_SLAVE_RX) {
                i2cx_slave_fifo_read(i2c, 0);

                // I2C Disable the FIFO Full Interrupt in the Interrupt Enable Register
                i2c->reg->REG_INTEN.all &= (~ BITSET_0X014_FIFOFULL);

                // keypoint for middleware to query
                i2c->info->trans_info.cmpl_count = i2c->info->trans_info.slave_read_last_rx_data_count;

                // clear & set driver state to slave rx complete
                i2c->info->state = I2C_DRV_SLAVE_RX_CMPL;
            }

            // clear busy bit on i2c complete event as slave cpu tx/rx
            i2c->info->status.busy = 0;
        }
        // I2C_DMA mode
        else
        {
            if (i2c->info->state & I2C_DRV_SLAVE_TX)
            {
                // keypoint for middleware to query
                i2c->info->trans_info.tx_buf_pointer = dma_channel_get_count(i2c->dma_tx->channel);
                i2c->info->trans_info.cmpl_count = dma_channel_get_count(i2c->dma_tx->channel);

                dma_channel_disable(i2c->dma_tx->channel, 1);

                // clear & set driver state to slave tx complete
                i2c->info->state = I2C_DRV_SLAVE_TX_CMPL;

                // clear busy bit on i2c complete event as slave dma tx
                i2c->info->status.busy = 0;
            }

            if (i2c->info->state & I2C_DRV_SLAVE_RX)
            {
                // keypoint for middleware to query
                i2c->info->trans_info.cmpl_count = dma_channel_get_count(i2c->dma_rx->channel);

                // abort dma channel since MAX_XFER_SZ-read and expect complete in cmpl_handler
                dma_channel_disable(i2c->dma_rx->channel, 1);

                // clear & set driver state to slave rx complete
                i2c->info->state = I2C_DRV_SLAVE_RX_CMPL;

                // clear busy bit on i2c complete event as slave dma rx since read MAX_XFER_SZ
                i2c->info->status.busy = 0;
            }
        }

        // if the Completion Interrupt asserts, clear the FIFO and go next transaction.
        i2c->reg->REG_CMD.bit.CMD = _CMD_CLEAR_FIFO;
    }
}


/** @file i2c_driver.c
 *  @brief I2C Driver Source Code with Doxygen Comments
 *         Contains implementation of I2C master/slave operations, interrupt handlers,
 *         and DMA configurations for embedded systems.
 */

/** @struct I2C_RESOURCES
 *  @brief Main structure containing all I2C hardware resources and operational state
 *         This structure aggregates register maps, transfer information, and control flags.
 */

static void
/**
 * @brief Write data from transmit buffer to I2C master FIFO
 *
 * This function calculates the optimal number of bytes to write to the FIFO based on:
 *   - Remaining transmit count (tx_num - tx_buf_pointer)
 *   - FIFO depth limitation
 * It performs byte-by-byte writes until either FIFO capacity is reached or transmission completes.
 * Disables FIFO empty interrupt when transmission finishes.
 *
 * @param[in] i2c Pointer to I2C resource structure
 */
i2cx_master_fifo_write(I2C_RESOURCES* i2c)
{
    uint32_t write_fifo_count = 0;

    write_fifo_count = ((i2c->info->trans_info.tx_num - i2c->info->trans_info.tx_buf_pointer)\
            >= i2c->info->fifo_depth) ?
                    i2c->info->fifo_depth :
                    (i2c->info->trans_info.tx_num - i2c->info->trans_info.tx_buf_pointer);

    {
        uint32_t i;

        for (i = 0; i < write_fifo_count; i++) {

            i2c->reg->REG_DATA.all = (\
                    i2c->info->trans_info.tx_buf[i2c->info->trans_info.tx_buf_pointer]\
                    & (BITSET_0X020_DATA_MASK));

            i2c->info->trans_info.tx_buf_pointer++;

            if (i2c->info->trans_info.tx_buf_pointer == i2c->info->trans_info.tx_num) {
                // I2C disable the FIFO Empty Interrupt in the Interrupt Enable Register
                i2c->reg->REG_INTEN.bit.FIFOEMPTY = 0x0;
                break;
            }
        }
    }
}

static void
/**
 * @brief Write data from transmit buffer to I2C slave FIFO
 *
 * Operates differently based on interrupt enable status:
 *   - Interrupt Mode: Writes single byte per call (polled approach)
 *   - DMA Mode: Configures DMA channel for bulk transfer, sets appropriate control registers
 * Special handling for 10-bit address test case bug fix through DATACNT masking.
 *
 * @param[in] i2c Pointer to I2C resource structure
 */
i2cx_slave_fifo_write(I2C_RESOURCES* i2c)
{
    // interrupt mode
    if (i2c->info->inter_en)
    {
        // slave TX 1 byte each time, since no information got
        // about how many bytes of master rx should be,
        // check nack_assert to complete slave tx
        uint32_t write_fifo_count = 1;

        uint32_t i;
        // I2C write a patch of data(FIFO_Depth) to FIFO,
        // it will be consumed empty if data is actually issued on I2C bus
        for (i = 0; i < write_fifo_count; i++){

            // I2C write data to FIFO through data port register
            i2c->reg->REG_DATA.bit.DATA = i2c->info->trans_info.tx_buf[i2c->info->trans_info.tx_buf_pointer] & (BITSET_0X020_DATA_MASK);

            i2c->info->trans_info.tx_buf_pointer++;
        }
    }
    else
    {
        int8_t stat;
        // If DMA is enabled, DataCnt is the number of
        // bytes to transmit/receive. It will not be reset to 0
        // when the slave is addressed and it will be
        // decreased by one for each byte of data
        // transmitted/received.*/
        // fix bug of dma-slave 10bit address test(rx 3 bytes flash addr)
        i2c->reg->REG_CTRL.bit.DATACNT = i2c->info->trans_info.tx_num & BITSET_0X024_DATACNT;

        dma_channel_select(
            &i2c->dma_tx->channel,
            i2c->dma_tx->cb_event,
            0,
            DMA_CACHE_SYNC_SRC);

        if (i2c->dma_tx->channel == DMA_CHANNEL_ANY) {
            return;
        }

        stat = dma_channel_configure (i2c->dma_tx->channel,
                    (uint32_t) (&i2c->info->trans_info.tx_buf[0]),
                    (uint32_t) (&(i2c->reg->REG_DATA.all)),
                    i2c->info->trans_info.tx_num,
                    DMA_CH_CTLL_DST_WIDTH(_I2C_TX_DMA_WIDTH) | DMA_CH_CTLL_SRC_WIDTH(_I2C_TX_DMA_WIDTH) |\
                  DMA_CH_CTLL_DST_BSIZE(_I2C_TX_DMA_BSIZE) | DMA_CH_CTLL_SRC_BSIZE(_I2C_TX_DMA_BSIZE) |\
                  DMA_CH_CTLL_DST_FIX | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_TTFC_M2P |\
                  DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN, // control
                  DMA_CH_CFGL_CH_PRIOR(1), // config_low
                  DMA_CH_CFGH_FIFO_MODE | DMA_CH_CFGH_DST_PER(i2c->dma_tx->reqsel), // config_high
                  0, 0);

        if(stat == -1){
            return;
        }

    }
}

static void
/**
 * @brief Handle FIFO empty event for both master and slave modes
 *
 * Checks power state and current operation mode (master TX/slave TX) to dispatch
 * to appropriate FIFO writing function. Acts as central dispatcher for FIFO underflow conditions.
 *
 * @param[in] i2c Pointer to I2C resource structure
 */
i2cx_fifo_empty_handler(I2C_RESOURCES* i2c)
{
    if (i2c->info->pwr_state != CSK_POWER_FULL){
        return;
    }

    if (i2c->info->state & I2C_DRV_MASTER_TX){
        i2cx_master_fifo_write(i2c);
    } else if (i2c->info->state & I2C_DRV_SLAVE_TX){
        i2cx_slave_fifo_write(i2c);
    }
}

static void
/**
 * @brief Read data from I2C master FIFO into receive buffer
 *
 * Determines maximum readable bytes based on remaining receive count and FIFO depth.
 * Updates receive buffer pointer and disables FIFO full interrupt when buffer fills completely.
 * Uses bitmasking to ensure valid data width during reads.
 *
 * @param[in] i2c Pointer to I2C resource structure
 */
i2cx_master_fifo_read(I2C_RESOURCES* i2c)
{
    uint32_t i = 0, read_fifo_count = 0;

    read_fifo_count =
            ((i2c->info->trans_info.rx_num - i2c->info->trans_info.rx_buf_pointer)
                    >= i2c->info->fifo_depth) ?
                    i2c->info->fifo_depth :
                    (i2c->info->trans_info.rx_num - i2c->info->trans_info.rx_buf_pointer);

    // I2C read a patch of data(FIFO_Depth) from FIFO,
    // it will be consumed empty if data is actually read out by driver
    for (i = 0; i < read_fifo_count; i++) {
        // I2C read data from FIFO through data port register
        i2c->info->trans_info.rx_buf[i2c->info->trans_info.rx_buf_pointer] = i2c->reg->REG_DATA.all & (BITSET_0X020_DATA_MASK);

        i2c->info->trans_info.rx_buf_pointer++;

        // If all data are read from the FIFO, disable the FIFO Full Interrupt. Otherwise, repeat
        if (i2c->info->trans_info.rx_buf_pointer == i2c->info->trans_info.rx_num)
        {
            // I2C disable the FIFO Full Interrupt in the Interrupt Enable Register
            i2c->reg->REG_INTEN.bit.FIFOFULL = 0x0;
        }
    }
}

static void
/**
 * @brief Read data from I2C slave FIFO into intermediate buffer
 *
 * Implements special slave receive logic including:
 *   - Tracking received data count via DATACNT register
 *   - Handling overflow conditions with MAX_XFER_SZ limit
 *   - Supporting both interrupt and DMA modes
 *   - Managing circular buffer pointers with wraparound protection
 *
 * @param[in] i2c Pointer to I2C resource structure
 * @param[in] is_fifo_full Flag indicating if FIFO is full (triggers max read)
 */
i2cx_slave_fifo_read(I2C_RESOURCES* i2c, uint8_t is_fifo_full)
{
    uint32_t i = 0, read_fifo_count = 0, curr_rx_data_count = 0;

    // slave rx data count is accumulated and depend on
    // master tx data count of one transaction(start-addr-data-stop),
    // possible larger than fifo length(4 bytes)
    // slave: If DMA is not enabled(FIFO mode), DataCnt is the number of
    // bytes transmitted/received from the bus master.
    // It is reset to 0 when the controller is addressed
    // and then increased by one for each byte of data
    // transmitted/received
     curr_rx_data_count = i2c->reg->REG_CTRL.bit.DATACNT;

    // error hit
    if (curr_rx_data_count > MAX_XFER_SZ)
    {
        assert(0);
    }

    if (is_fifo_full)
    {
        read_fifo_count = i2c->info->fifo_depth;
    }
    else
    {
        read_fifo_count = curr_rx_data_count - i2c->info->trans_info.slave_read_last_rx_data_count;
    }

    if (read_fifo_count > MAX_XFER_SZ)
    {
        assert(0);
    }

    // I2C read a patch of data(FIFO_Depth) from FIFO,
    // it will be consumed empty if data is actually read out by driver */
    for (i = 0; i < read_fifo_count; i++) {
        // I2C read data from FIFO through data port register
        i2c->info->trans_info.slave_read_mid_buf[i2c->info->trans_info.slave_read_last_rx_data_count] =
                (i2c->reg->REG_DATA.all & BITSET_0X020_DATA_MASK);

        i2c->info->trans_info.slave_read_last_rx_data_count++;

        if (i2c->info->trans_info.slave_read_last_rx_data_count == MAX_XFER_SZ)
        {
            // slave rx buffer overwrite
            i2c->info->status.slave_rx_over_flow = 0x1;
            i2c->info->trans_info.slave_read_last_rx_data_count = 0;
        }
    }
}

static void
/**
 * @brief Handle FIFO full event for both master and slave modes
 *
 * Checks power state and current operation mode (master RX/slave RX) to dispatch
 * to appropriate FIFO reading function. Acts as central dispatcher for FIFO overflow conditions.
 *
 * @param[in] i2c Pointer to I2C resource structure
 */
i2cx_fifo_full_handler(I2C_RESOURCES* i2c)
{
    if (i2c->info->pwr_state != CSK_POWER_FULL) {
        return;
    }

    if (i2c->info->state & I2C_DRV_MASTER_RX) {
        i2cx_master_fifo_read(i2c);
    } else if (i2c->info->state & I2C_DRV_SLAVE_RX) {
        i2cx_slave_fifo_read(i2c, 1);
    }
}

static void
/**
 * @brief Handle slave address hit event
 *
 * Performs critical initialization sequence upon slave address match including:
 *   - Clearing FIFO to prevent corruption
 *   - Resetting receive buffer pointers
 *   - Enabling appropriate interrupts/DMA based on configuration
 *   - Setting up DMA channels for bulk transfers when enabled
 *   - Configuring control registers for slave response direction
 *
 * @param[in] i2c Pointer to I2C resource structure
 */
i2cx_slave_addr_hit_handler(I2C_RESOURCES* i2c)
{
    uint32_t Tmp_C = 0;
    int32_t stat = 0;

    if (i2c->info->pwr_state != CSK_POWER_FULL)
    {
        return;
    }

    // I2C clear fifo first to prevent mistake i2cx_slave_fifo_read()
    // if the Completion Interrupt asserts, clear the FIFO and go next transaction.
    i2c->reg->REG_CMD.bit.CMD = _CMD_CLEAR_FIFO;

    // slave mode Rx: if address hit, fifo may not be full state
    if (i2c->info->state & I2C_DRV_SLAVE_RX)
    {
        // A new I2C data transaction(start-addr-data-stop)
        // clear slave read software
        i2c->info->trans_info.slave_read_mid_buf_pointer = 0;
        i2c->info->trans_info.slave_read_last_rx_data_count = 0;
        __builtin_memset (i2c->info->trans_info.slave_read_mid_buf, 0, sizeof(i2c->info->trans_info.slave_read_mid_buf));

        if (i2c->info->inter_en)
        {
            // I2C Enable the FIFO Full Interrupt in the Interrupt Enable Register
            i2c->reg->REG_INTEN.bit.FIFOFULL = 0x1;
        }
        else
        {
            // If DMA is enabled, DataCnt is the number of
            // bytes to transmit/receive. It will not be reset to 0
            // when the slave is addressed and it will be
            // decreased by one for each byte of data
            // transmitted/received.
            // fix bug of dma-slave 10bit address test(rx 3 bytes flash addr)
            i2c->reg->REG_CTRL.bit.DATACNT = (MAX_XFER_SZ & BITSET_0X024_DATACNT);

            dma_channel_select(
                &i2c->dma_rx->channel,
                i2c->dma_rx->cb_event,
                0,
                DMA_CACHE_SYNC_DST);
            if (i2c->dma_rx->channel == DMA_CHANNEL_ANY) {
                return;
            }

            stat = dma_channel_configure (i2c->dma_rx->channel,
                        (uint32_t) (&(i2c->reg->REG_DATA.all)),
                        (uint32_t) (&i2c->info->trans_info.slave_read_mid_buf[0]),
                        MAX_XFER_SZ,
                        DMA_CH_CTLL_DST_WIDTH(_I2C_RX_DMA_WIDTH) | DMA_CH_CTLL_SRC_WIDTH(_I2C_RX_DMA_WIDTH) |\
                      DMA_CH_CTLL_DST_BSIZE(_I2C_RX_DMA_BSIZE) | DMA_CH_CTLL_SRC_BSIZE(_I2C_RX_DMA_BSIZE) |\
                      DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_TTFC_P2M |\
                      DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN, // control
                      DMA_CH_CFGL_CH_PRIOR(1), // config_low
                      DMA_CH_CFGH_FIFO_MODE | DMA_CH_CFGH_SRC_PER(i2c->dma_rx->reqsel), // config_high
                      0, 0);

            if(stat == -1){
                return;
            }
        }
    }
    // slave mode Tx: if address hit, fifo may not be empty state
    else if (i2c->info->state & I2C_DRV_SLAVE_TX)
    {
        if (i2c->info->inter_en){
            // I2C Enable the Byte Transmit Interrupt in the Interrupt Enable Register
            // for status.busy flag support
            i2c->reg->REG_INTEN.bit.BYTETRANS = 0x1;
            i2c->reg->REG_SETUP.bit.DMAEN = 0x0;
        }else{
            i2c->reg->REG_SETUP.bit.DMAEN = 0x1;

            i2c->reg->REG_INTEN.all |= (BITSET_0X014_CMPL | BITSET_0X014_ARBLOS);
        }
    }
}

static void
/**
 * @brief Main I2C interrupt handler
 *
 * Processes all relevant I2C events including:
 *   - Completion events (success/failure)
 *   - Address acknowledge/not acknowledged
 *   - FIFO empty/full conditions
 *   - Slave address hits and general calls
 *   - Arbitration loss detection
 *   - Byte transfer notifications
 * Dispatches events to specialized handlers and triggers user callbacks.
 *
 * @param[in] i2c Pointer to I2C resource structure
 */
i2c_irq_handler(I2C_RESOURCES* i2c)
{
    uint32_t iir, event = 0;
    iir = i2c->reg->REG_STATUS.all;

    // write 1 clear for those interrupts be able to W1C
    i2c->reg->REG_STATUS.all = (iir & BITSET_0X018_STATUS_CLEAR);

    if (iir & BITSET_0X018_CMPL)
    {
        i2c_cmpl_handler(i2c);

        event |= CSK_I2C_EVENT_TRANSFER_DONE;

        // In master mode
        if (i2c->reg->REG_SETUP.bit.MASTER == 0x1){
            // Address hit
            if (iir & BITSET_0X018_ADDRHIT){
                event |= CSK_I2C_EVENT_ADDRESS_ACK;
            } else {
                event |= CSK_I2C_EVENT_ADDRESS_NACK;
            }
        }
    }
    else
    {
        event |= CSK_I2C_EVENT_TRANSFER_INCOMPLETE;
    }

    if (iir & BITSET_0X018_FIFO_EMPTY)
    {
        i2cx_fifo_empty_handler(i2c);
    }

    if (iir & BITSET_0X018_FIFOFULL)
    {
        i2cx_fifo_full_handler(i2c);
    }

    // Here is the entry for slave mode driver to detect
    // slave RX/TX action depend on master TX/RX action.
    // Addr hit is W1C bit, so during payload transaction,
    // it is not set again.
    // A new I2C data transaction(start-addr-data-stop)
    if (iir & BITSET_0X018_ADDRHIT)
    {
        // slave mode
        if (i2c->reg->REG_SETUP.bit.MASTER == 0x0)
        {
            // Indicates that the address of the current
            // transaction is a general call address.
            // This status is only valid in slave mode.
            // A new I2C data transaction(start-addr-data-stop)
            if (iir & BITSET_0X018_GENCALL){
                i2c->info->status.general_call = 1;

                event |= CSK_I2C_EVENT_GENERAL_CALL;
            }

            if (i2c->reg->REG_CTRL.bit.DIR == I2C_SLAVE_RX)
            {
                // notify middleware to do slave rx action
                event |= CSK_I2C_EVENT_SLAVE_RECEIVE;

                //clear & set driver state to slave rx before data transaction
                i2c->info->state = I2C_DRV_SLAVE_RX;
            }
            else if (i2c->reg->REG_CTRL.bit.DIR == I2C_SLAVE_TX)
            {
                // notify middleware to do slave tx action
                event |= CSK_I2C_EVENT_SLAVE_TRANSMIT;

                //clear & set driver state to slave tx before data transaction
                i2c->info->state = I2C_DRV_SLAVE_TX;
            }

            // A new I2C data transaction(start-addr-data-stop)
            i2cx_slave_addr_hit_handler(i2c);
        }
    }

    if ((iir & BITSET_0X018_ARBLOS) && (i2c->reg->REG_SETUP.bit.MASTER == 0x0))
    {
        i2c->info->status.arbitration_lost = 1;

        event |= CSK_I2C_EVENT_ARBITRATION_LOST;
    }

    if ((iir & BITSET_0X018_BYTETRANS) && (i2c->reg->REG_SETUP.bit.MASTER == 0x0))
    {
        // I2C clear fifo first to prevent mistake i2cx_slave_fifo_read()
        // if the Completion Interrupt asserts, clear the FIFO and go next transaction.
        i2c->reg->REG_CMD.bit.CMD = _CMD_CLEAR_FIFO;

        if (i2c->info->inter_en){
            i2c->info->trans_info.cmpl_count++;
        }

        // set on start of next slave operation,
        // cleared on slave tx 1 byte done or data transaction complete.
        i2c->info->status.busy = 0;
    }

    // invoke callback function
    if (i2c->info->cb_event)
    {
        i2c->info->cb_event(event, i2c->info->workspace);
    }
}


static void i2cx_dma_tx_event (uint32_t event, I2C_RESOURCES* i2c)
{
    switch(event)
    {
        case DMA_EVENT_TRANSFER_COMPLETE:
        break;
        case DMA_EVENT_ERROR:
        default:
        break;
    }
}

static void i2cx_dma_rx_event (uint32_t event, I2C_RESOURCES* i2c)
{
    switch (event)
    {
        case DMA_EVENT_TRANSFER_COMPLETE:
        // clear busy bit on dma complete event as master dma rx
        i2c->info->status.busy = 0;
        break;
        case DMA_EVENT_ERROR:
        default:
        break;
    }
}

static void i2c0_dma_tx_event (uint32_t event, uint32_t xfer_bytes, uint32_t usr_param)
{
    i2cx_dma_tx_event((event & 0xff), &i2c0_resources);
}

static void i2c0_dma_rx_event (uint32_t event, uint32_t xfer_bytes, uint32_t usr_param)
{
    i2cx_dma_rx_event((event & 0xff), &i2c0_resources);
}

static void i2c0_irq_handler(void){
    i2c_irq_handler(&i2c0_resources);
}

static void i2c1_dma_tx_event (uint32_t event, uint32_t xfer_bytes, uint32_t usr_param)
{
    i2cx_dma_tx_event((event & 0xff), &i2c1_resources);
}

static void i2c1_dma_rx_event (uint32_t event, uint32_t xfer_bytes, uint32_t usr_param)
{
    i2cx_dma_rx_event((event & 0xff), &i2c1_resources);
}

static void i2c1_irq_handler(void){
    i2c_irq_handler(&i2c1_resources);
}


