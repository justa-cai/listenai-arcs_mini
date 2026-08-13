/**
 *@file ir.c
 *
 *@brief  Created on: 2020年8月12日
 *      Author: USER
 */
#include "Driver_IR.h"

#include "venusa_ap.h"
#include "dma.h"
#include "ClockManager.h"
#include "PowerManager.h"

/** @defgroup IR_Timing NEC/9012/RC5 Timing Parameters
 *  @{
 **/
/*************NEC TX TIMING***************/
#define IR_TIME_NEC_TX_S1                              (287)   ///< Start pulse width for NEC transmitter [us]
#define IR_TIME_NEC_TX_S3                              (143)   ///< Separator pulse width between bits [us]
#define IR_TIME_NEC_TX_BIT_CYCLE                       (18)    ///< Bit period duration [us]
#define IR_TIME_NEC_TX_TIME1                          (35)    ///< Logical '1' pulse width [us]
#define IR_TIME_NEC_TX_TIME2                          (71)    ///< Logical '0' pulse width [us]

/*************NEC RX TIMING***************/
#define IR_TIME_NEC_RX_S1                              (270)
#define IR_TIME_NEC_RX_S2                              (300)
#define IR_TIME_NEC_RX_S3                              (400)
#define IR_TIME_NEC_RX_S4                              (441)
#define IR_TIME_NEC_RX_S5                              (3199)
#define IR_TIME_NEC_RX_TIME1                           (10)
#define IR_TIME_NEC_RX_TIME2                           (30)
#define IR_TIME_NEC_RX_BIT_CYCLE                       (70)
/*************NEC TX REPEAT TIMING***************/
#define IR_TIME_NEC_REPEAT_S1                          (287)
#define IR_TIME_NEC_REPEAT_S2                          (71)
#define IR_TIME_NEC_REPEAT_S4                          (17)
/*************9012 TX TIMING***************/
#define IR_TIME_9012_TX_S1                             (142)
#define IR_TIME_9012_TX_S3                             (142)
#define IR_TIME_9012_TX_BIT_CYCLE                      (18)
#define IR_TIME_9012_TX_TIME1                          (35)
#define IR_TIME_9012_TX_TIME2                          (71)
/*************9012 RX TIMING***************/
#define IR_TIME_9012_RX_S1                             (140)
#define IR_TIME_9012_RX_S2                             (148)
#define IR_TIME_9012_RX_S3                             (280)
#define IR_TIME_9012_RX_S4                             (296)
#define IR_TIME_9012_RX_S5                             (3199)
#define IR_TIME_9012_RX_TIME1                          (10)
#define IR_TIME_9012_RX_TIME2                          (37)
#define IR_TIME_9012_RX_BIT_CYCLE                      (74)
/*************RC5 TX TIMING***************/
#define IR_TIME_RC5_TX_S1                              (56)
#define IR_TIME_RC5_TX_BIT_CYCLE                       (28)
#define IR_TIME_RC5_TX_TIME1                           (56)
/*************RC5 RX TIMING***************/
#define IR_TIME_RC5_RX_S1                              (26)
#define IR_TIME_RC5_RX_S2                              (35)
#define IR_TIME_RC5_RX_S3                              (112)
#define IR_TIME_RC5_RX_S4                              (140)
#define IR_TIME_RC5_RX_S5                              (3199)
#define IR_TIME_RC5_RX_TIME1                           (24)
#define IR_TIME_RC5_RX_TIME2                           (30)
#define IR_TIME_RC5_RX_BIT_CYCLE                       (56)
/*************IR Carry***************/
#define IR_CARRY_HIGH_CONFIG_38KHZ                     (103)
#define IR_CARRY_LOW_CONFIG_38KHZ                      (55)

#define IR_CARRY_HIGH_CONFIG_36KHZ                     (109)
#define IR_CARRY_LOW_CONFIG_36KHZ                      (58)

#define CSK_FLAG_INITIALIZED        (1UL << 0)
#define CSK_FLAG_POWERED            (1UL << 1)

#define CSK_IR0_TX_DMA_CH           (0)
#define CSK_IR0_TX_DMA_REQSEL       (HAL_CMN_DMA_SEL0_HSID_11_IR_TX)
#define CSK_IR0_RX_DMA_CH           (1)
#define CSK_IR0_RX_DMA_REQSEL       (HAL_CMN_DMA_SEL0_HSID_10_IR_RX)

typedef struct _IR_SW_INFO {
    uint32_t tx_num;      ///< Transmit data count
    uint32_t rx_num;      ///< Receive buffer size
    uint16_t *tx_buf;     ///< Transmit buffer pointer
    uint16_t *rx_buf;     ///< Receive buffer pointer
    uint32_t tx_cnt;     ///< Transmitted byte counter
    uint32_t rx_cnt;     ///< Received byte counter
    uint32_t thres;      ///< DMA transfer threshold
    uint8_t h_carry;     ///< Carrier high phase count
    uint8_t l_carry;     ///< Carrier low phase count
} IR_SW_INFO;

/** @struct _IR_HW_INFO
 *  @brief Hardware configuration information structure
 *  @var tx_address Target device address
 *  @var tx_command Command code to send
 *  @var rx_address Pointer to store received address
 *  @var rx_command Pointer to store received command
 */
typedef struct _IR_HW_INFO {
    uint16_t tx_address;        ///< Target device address
    uint16_t tx_command;       ///< Command code to send
    uint16_t *rx_address;      ///< Pointer to store received address
    uint16_t *rx_command;      ///< Pointer to store received command
} IR_HW_INFO;

/** @struct _IR_INFO
 *  @brief Main IR driver state structure
 *  @var cb_event Event callback function
 *  @var sw_info Software state information
 *  @var hw_info Hardware configuration information
 *  @var mode Current operating mode (NEC/9012/RC5/Custom)
 *  @var flags Status flags (initialized/powered)
 *  @var workspace User-defined work area
 *  @var busy Busy status flag
 *  @var tx_rx_c Transmit/Receive mode indicator (1=TX, 0=RX)
 */
typedef struct _IR_INFO {
    CSK_IR_SignalEvent_t cb_event;   ///< Event callback function
    IR_SW_INFO * const sw_info;       ///< Software state info
    IR_HW_INFO * const hw_info;       ///< Hardware config info
    IR_MODE mode;                    ///< Current operating mode
    uint32_t flags;                  ///< Status flags
    void* workspace;                 ///< User work area
    uint8_t busy;                    ///< Busy status flag
    uint8_t tx_rx_c;                 ///< TX/RX mode indicator
} IR_INFO;

/** @struct _IR_DMA
 *  @brief DMA channel configuration structure
 *  @var channel DMA channel number
 *  @var reqsel DMA request selection
 *  @var cb_event DMA completion callback
 */
typedef struct _IR_DMA {
    uint8_t channel;               ///< DMA channel number
    uint8_t reqsel;                ///< DMA request selection
    DMA_SignalEvent_t cb_event;    ///< DMA completion callback
} IR_DMA;

/** @struct _IR_RESOURCES
 *  @brief Peripheral resource manager structure
 *  @var reg Register map base address
 *  @var irq_num Interrupt vector number
 *  @var irq_handler Interrupt service routine
 *  @var dma_tx Transmit DMA configuration
 *  @var dma_rx Receive DMA configuration
 *  @var info Main state structure
 */
typedef const struct _IR_RESOURCES {
    IR_RegDef* reg;                ///< Register map base address
    uint32_t irq_num;              ///< Interrupt vector number
    void (*irq_handler)(void);     ///< Interrupt service routine
    IR_DMA *dma_tx;                ///< Transmit DMA config
    IR_DMA *dma_rx;                ///< Receive DMA config
    IR_INFO *info;                 ///< Main state structure
} IR_RESOURCES;

// Driver version definitions
#define CSK_IR_DRV_VERSION    CSK_DRIVER_VERSION_MAJOR_MINOR(1, 0)  /* driver version */
static const CSK_DRIVER_VERSION DriverVersion = {
    CSK_IR_API_VERSION,
    CSK_IR_DRV_VERSION
};

static void
IR0_DMA_TX_Handler(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param);

static IR_DMA ir0_dma_tx_info = {
        CSK_IR0_TX_DMA_CH,
        CSK_IR0_TX_DMA_REQSEL,
        IR0_DMA_TX_Handler,
};

static void
IR0_DMA_RX_Handler(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param);

static IR_DMA ir0_dma_rx_info = {
        CSK_IR0_RX_DMA_CH,
        CSK_IR0_RX_DMA_REQSEL,
        IR0_DMA_RX_Handler,
};

static IR_SW_INFO ir_sw_info = {
        0
};

static IR_HW_INFO ir_hw_info = {
        0,
        0,
        NULL,
        NULL,
};

static IR_INFO ir_info = {
        NULL,
        &ir_sw_info,
        &ir_hw_info,
        IR_MODE_NEC,
        0,
        NULL,
        0,
        0,
};

static void
IR0_IRQ_Handler();

static IR_RESOURCES ir0_resources = {
		IP_IR,
		IRQ_IR_VECTOR,
        IR0_IRQ_Handler,
        &ir0_dma_tx_info,
        &ir0_dma_rx_info,
        &ir_info,
};

#define CHECK_RESOURCES(res)  do{\
        if(res != &ir0_resources){\
            return CSK_DRIVER_ERROR_PARAMETER;\
        }\
}while(0)

void* IR0(void){
    return (void*)&ir0_resources;
}

/** @fn CSK_IR_GetVersion(void)
 *  @brief Get driver version information
 *  @return CSK_DRIVER_VERSION Driver version structure
 */
CSK_DRIVER_VERSION CSK_IR_GetVersion(void){
    return DriverVersion;
}

/** @fn IR_Initialize(void *res, CSK_IR_SignalEvent_t cb_event, void* workspace)
 *  @brief Initialize IR peripheral
 *  @param[in] res Resource structure pointer
 *  @param[in] cb_event Signal event callback function
 *  @param[in] workspace User-defined work area
 *  @return int32_t Status code (CSK_DRIVER_OK on success)
 *  @retval CSK_DRIVER_ERROR_PARAMETER Invalid parameter error
 *  @retval CSK_DRIVER_OK Successful initialization
 *  @details This function initializes the IR peripheral with default parameters and registers the interrupt handler.
 */
int32_t IR_Initialize(void *res, CSK_IR_SignalEvent_t cb_event, void* workspace){
    CHECK_RESOURCES(res);
    IR_RESOURCES *ir = (IR_RESOURCES*)res;

    if (ir->info->flags & CSK_FLAG_INITIALIZED){
        return CSK_DRIVER_OK;
    }

    ir->info->cb_event = cb_event;
    ir->info->workspace = NULL;
    ir->info->tx_rx_c = 0x0;
    ir->info->busy = 0x0;
    ir->info->mode = IR_MODE_NEC;

    __builtin_memset(ir->info->sw_info, 0, sizeof(IR_SW_INFO));
    ir->info->sw_info->thres = 0x2000;

    ir->info->hw_info->rx_address = NULL;
    ir->info->hw_info->rx_command = 0x0;
    ir->info->hw_info->tx_address = 0x0;
    ir->info->hw_info->tx_command = 0x0;

    ir->info->flags = CSK_FLAG_INITIALIZED;
    return CSK_DRIVER_OK;
}

/** @fn IR_Uninitialize(void* res)
 *  @brief Deinitialize IR peripheral
 *  @param[in] res Resource structure pointer
 *  @return int32_t Status code (CSK_DRIVER_OK on success)
 *  @details This function resets all driver state machines and releases hardware resources.
 */
int32_t IR_Uninitialize(void* res){
    CHECK_RESOURCES(res);
    IR_RESOURCES *ir = (IR_RESOURCES*)res;

    ir->info->cb_event = NULL;
    ir->info->workspace = NULL;
    ir->info->tx_rx_c = 0x0;
    ir->info->busy = 0x0;
    ir->info->mode = IR_MODE_NEC;

    __builtin_memset(ir->info->sw_info, 0, sizeof(IR_SW_INFO));
    ir->info->hw_info->rx_address = NULL;
    ir->info->hw_info->rx_command = 0x0;
    ir->info->hw_info->tx_address = 0x0;
    ir->info->hw_info->tx_command = 0x0;

    ir->info->flags = 0;
    return CSK_DRIVER_OK;
}

/** @fn IR_PowerControl(void *res, CSK_POWER_STATE state)
 *  @brief Power management control
 *  @param[in] res Resource structure pointer
 *  @param[in] state Power state (OFF/LOW/FULL)
 *  @return int32_t Status code
 *  @retval CSK_DRIVER_ERROR Invalid power state transition
 *  @retval CSK_DRIVER_OK Successful power state change
 *  @details Controls power states including clock gating and reset generation.
 */
int32_t IR_PowerControl(void *res, CSK_POWER_STATE state){
    CHECK_RESOURCES(res);
    IR_RESOURCES *ir = (IR_RESOURCES*)res;
    switch (state)
    {
    case CSK_POWER_OFF:
        if((ir->info->flags & CSK_FLAG_INITIALIZED) == 0U) {
            return CSK_DRIVER_ERROR;
        }

        // disable DMA
        ir->reg->REG_IR_DMA_CONFIG.bit.TX_DMA_ENABLE = 0x0;
        if((ir->info->busy) && (ir->info->mode == IR_MODE_CUSTOM)){
            if(ir->info->tx_rx_c){
                dma_channel_disable(ir->dma_tx->channel, 0x1);
            }else{
                dma_channel_disable(ir->dma_rx->channel, 0x1);
            }
        }
        __HAL_PMU_IR_RST_ENABLE();
        ir->reg->REG_IR_FIFO_CONFIG.all = 0xf;
        disable_IRQ(ir->irq_num);
        register_ISR(ir->irq_num, NULL, NULL);
        ir->info->flags &= (~CSK_FLAG_POWERED);
        dma_uninitialize();
        break;
    case CSK_POWER_LOW:
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    case CSK_POWER_FULL:
        if((ir->info->flags & CSK_FLAG_INITIALIZED) == 0U){
            return CSK_DRIVER_ERROR;
        }

        if((ir->info->flags & CSK_FLAG_POWERED) != 0U){
            return CSK_DRIVER_OK;
        }

        // reset ir
        __HAL_PMU_IR_RST_ENABLE();
        __HAL_CRM_IR_CLK_ENABLE();
        ir->reg->REG_IR_CLEAR_STATUS.all = 0x7f;
        ir->reg->REG_IR_FIFO_CONFIG.all = 0xf;
        ir->reg->REG_IR_CTRL.bit.IR_INT_EN = 0x0;
        ir->reg->REG_IR_CTRL.bit.IR_EN = 0x0;
        dma_initialize();
        ir->info->flags = CSK_FLAG_INITIALIZED | CSK_FLAG_POWERED;
        register_ISR(ir->irq_num, ir->irq_handler, NULL);
        enable_IRQ(ir->irq_num);
        break;
    }
    return CSK_DRIVER_OK;
}

/** @fn IR_Control(void *res, uint32_t control, uint32_t arg)
 *  @brief Device control interface
 *  @param[in] res Resource structure pointer
 *  @param[in] control Control command mask
 *  @param[in] arg Command argument
 *  @return int32_t Status code
 *  @details Handles various control commands including timeout thresholds, abort operations, and protocol configurations.
 */
int32_t IR_Control(void *res, uint32_t control, uint32_t arg){
    CHECK_RESOURCES(res);
    IR_RESOURCES *ir = (IR_RESOURCES*)res;

    if ((ir->info->flags & CSK_FLAG_POWERED) == 0){
        return CSK_DRIVER_ERROR;
    }

    switch (control & CSK_IR_CONTROL_Msk){

    case CSK_IR_CONTROL_SW_TIMEOUT_THRES:
        if (arg == 0){
            return CSK_DRIVER_ERROR_PARAMETER;
        }
        ir->info->sw_info->thres = arg;
        return CSK_DRIVER_OK;

    case CSK_IR_CONTROL_ABORT:
        // sw mode
        if(ir->info->mode == IR_MODE_CUSTOM){
            // tx
            if(ir->info->tx_rx_c){
                dma_channel_disable(ir->dma_tx->channel, 0x1);
                ir->info->sw_info->tx_cnt = dma_channel_get_count(ir->dma_tx->channel);
                // disable DMA
                ir->reg->REG_IR_DMA_CONFIG.bit.TX_DMA_ENABLE = 0x0;
            }
            // rx
            else{
                dma_channel_disable(ir->dma_rx->channel, 0x1);
                ir->info->sw_info->rx_cnt = dma_channel_get_count(ir->dma_rx->channel);
                // disable DMA
                ir->reg->REG_IR_DMA_CONFIG.bit.RX_DMA_ENABLE = 0x0;
            }

            // disable ir
            ir->reg->REG_IR_CTRL.bit.IR_EN = 0x0;

            // clear fifo
            ir->reg->REG_IR_FIFO_CONFIG.all |= (IR_IR_FIFO_CONFIG_RX_FIFO_WR_CLR_Msk |\
												IR_IR_FIFO_CONFIG_RX_FIFO_RD_CLR_Msk |\
												IR_IR_FIFO_CONFIG_TX_FIFO_WR_CLR_Msk |\
												IR_IR_FIFO_CONFIG_TX_FIFO_RD_CLR_Msk);
        }

        // hw mode
        else{
            // ignore
        }

        ir->info->busy = 0x0;

        return CSK_DRIVER_OK;

    case CSK_IR_CONTROL_TX_REPEAT:
        // busy?
        if (ir->info->busy){
            return CSK_DRIVER_ERROR_BUSY;
        }

        // only support NEC mode and tx mode
        if ((ir->info->mode != IR_MODE_NEC) || (ir->info->tx_rx_c != 0x1)){
            return CSK_DRIVER_ERROR;
        }

        // set time parameter
        ir->reg->REG_IR_TIME_1.bit.IR_TIME_S1 = IR_TIME_NEC_REPEAT_S1;
        ir->reg->REG_IR_TIME_2.bit.IR_TIME_S2 = IR_TIME_NEC_REPEAT_S2;
        ir->reg->REG_IR_TIME_4.bit.IR_TIME_S4 = IR_TIME_NEC_REPEAT_S4;
        ir->reg->REG_IR_CTRL.bit.SW_HW_MODE = 0x0;
        ir->reg->REG_IR_CTRL.bit.TXRX_MODE = 0x1;
        ir->reg->REG_IR_CTRL.bit.TX_REPEAT_MODE = 0x1;
        ir->reg->REG_IR_CTRL.bit.IR_INT_EN = 0x1;
        ir->reg->REG_IR_CTRL.bit.IR_EN = 0x1;
        ir->reg->REG_IR_TX_CONFIG.bit.TX_START = 0x1;
        return CSK_DRIVER_OK;

    case CSK_IR_CONTROL_CLEAR_TX_FIFO:
        ir->reg->REG_IR_FIFO_CONFIG.all |= 0x3;
        while(ir->reg->REG_IR_FSM.bit.IR_TX_FIFO_USEDW);
        return CSK_DRIVER_OK;

    case CSK_IR_CONTROL_CLEAR_RX_FIFO:
        ir->reg->REG_IR_FIFO_CONFIG.all |= 0xc;
        while(ir->reg->REG_IR_FSM.bit.IR_RX_FIFO_USEDW);
        return CSK_DRIVER_OK;
    }

    switch (control & CSK_IR_SW_CARRY_Msk){
    case CSK_IR_SW_CARRY_CONFIG:
    {
        uint32_t x;
        // high period : low period = 1 : 2
        x = CRM_GetIrFreq()/(arg*3);
		ir->reg->REG_IR_CARRY_CONFIG.bit.IR_CARRY_HIGH = x & 0xff;
		ir->reg->REG_IR_CARRY_CONFIG.bit.IR_CARRY_LOW = (2 * x) & 0xff;

		ir->info->sw_info->h_carry = x & 0xff;
		ir->info->sw_info->l_carry = (2 * x) & 0xff;
    }
        break;
    }

    switch (control & CSK_IR_DECODE_CONTROL_Msk){

    case CSK_IR_DECODE_CONTROL_EN:
    {
        if (arg){
            ir->reg->REG_IR_TIME_1.bit.RX_CR_EN = 0x1;
        } else {
            ir->reg->REG_IR_TIME_1.bit.RX_CR_EN = 0x0;
        }
    }
        break;

    case CSK_IR_DECODE_CONTROL_INPOL:
    {
        if (arg){
            ir->reg->REG_IR_CARRY_CONFIG.bit.IR_RX_IN_POL = 0x1;
        }else {
            ir->reg->REG_IR_CARRY_CONFIG.bit.IR_RX_IN_POL = 0x0;
        }
    }
        break;

    case CSK_IR_DECODE_CONTROL_OUTPOL:
    {
        if (arg){
            ir->reg->REG_IR_TIME_1.bit.IR_RX_POL = 0x1;
        }else {
            ir->reg->REG_IR_TIME_1.bit.IR_RX_POL = 0x0;
        }
    }
        break;

    case CSK_IR_DECODE_CONTROL_DET_THRE:
    {
        ir->reg->REG_IR_TIME_1.bit.RX_CR_DET_TH = arg;
    }
        break;

    case CSK_IR_DECODE_CONTROL_HIGH_THRE:
    {
        ir->reg->REG_IR_CARRY_CONFIG.bit.RX_CR_HI_TH = arg;
    }
        break;

    case CSK_IR_DECODE_CONTROL_LOW_THRE:
    {
        ir->reg->REG_IR_TIME_1.bit.RX_CR_LOS_TH = arg;
    }
        break;
    }
    return CSK_DRIVER_OK;
}

/** @fn IR_HW_Send(void *res, IR_MODE mode, uint16_t address, uint16_t command)
 *  @brief Hardware mode transmission
 *  @param[in] res Resource structure pointer
 *  @param[in] mode Protocol mode (NEC/9012/RC5)
 *  @param[in] address Target device address
 *  @param[in] command Command code to send
 *  @return int32_t Status code
 *  @details Sends infrared signal using hardware protocol engine with specified parameters.
 */
int32_t  IR_HW_Send(void *res, IR_MODE mode, uint16_t address, uint16_t command){
    CHECK_RESOURCES(res);
    IR_RESOURCES *ir = (IR_RESOURCES*)res;

    if(!(ir->info->flags & CSK_FLAG_POWERED)){
        return CSK_DRIVER_ERROR;
    }

    if(ir->info->busy){
        return CSK_DRIVER_ERROR_BUSY;
    }

    ir->reg->REG_IR_CTRL.bit.IR_EN = 0x0;
    ir->info->busy = 0x1;
    ir->info->hw_info->tx_address = address;
    ir->info->hw_info->tx_command = command;
    ir->info->mode = mode;
    ir->info->tx_rx_c = 1;

    // configure the mode timing
    switch (ir->info->mode){
    case IR_MODE_CUSTOM:
        return CSK_DRIVER_ERROR_PARAMETER;
    case IR_MODE_NEC:
        // nec mode
        ir->reg->REG_IR_CTRL.bit.IR_MODE = 0x0;
        // 38khz carry
        ir->reg->REG_IR_CARRY_CONFIG.bit.IR_CARRY_HIGH = IR_CARRY_HIGH_CONFIG_38KHZ;
        ir->reg->REG_IR_CARRY_CONFIG.bit.IR_CARRY_LOW = IR_CARRY_LOW_CONFIG_38KHZ;
        // hardware parameter
        ir->reg->REG_IR_TIME_1.bit.IR_TIME_S1 = IR_TIME_NEC_TX_S1;
        ir->reg->REG_IR_TIME_3.bit.IR_TIME_S3 = IR_TIME_NEC_TX_S3;
        ir->reg->REG_IR_CTRL.bit.BIT_TIME_1 = IR_TIME_NEC_TX_TIME1;
        ir->reg->REG_IR_CTRL.bit.BIT_TIME_2 = IR_TIME_NEC_TX_TIME2;
        ir->reg->REG_IR_CTRL.bit.IR_BIT_CYCLE = IR_TIME_NEC_TX_BIT_CYCLE;
        break;
    case IR_MODE_TOSHIBA_9012:
        // 9012 mode
        ir->reg->REG_IR_CTRL.bit.IR_MODE = 0x1;
        // 38khz carry
        ir->reg->REG_IR_CARRY_CONFIG.bit.IR_CARRY_HIGH = IR_CARRY_HIGH_CONFIG_38KHZ;
        ir->reg->REG_IR_CARRY_CONFIG.bit.IR_CARRY_LOW = IR_CARRY_LOW_CONFIG_38KHZ;
        // hardware parameter
        ir->reg->REG_IR_TIME_1.bit.IR_TIME_S1 = IR_TIME_9012_TX_S1;
        ir->reg->REG_IR_TIME_3.bit.IR_TIME_S3 = IR_TIME_9012_TX_S3;
        ir->reg->REG_IR_CTRL.bit.BIT_TIME_1 = IR_TIME_9012_TX_TIME1;
        ir->reg->REG_IR_CTRL.bit.BIT_TIME_2 = IR_TIME_9012_TX_TIME2;
        ir->reg->REG_IR_CTRL.bit.IR_BIT_CYCLE = IR_TIME_9012_TX_BIT_CYCLE;
        break;
    case IR_MODE_PHILIPS_RC5:
        // rc5 mode
        ir->reg->REG_IR_CTRL.bit.IR_MODE = 0x2;
        // 36khz carry
        ir->reg->REG_IR_CARRY_CONFIG.bit.IR_CARRY_HIGH = IR_CARRY_HIGH_CONFIG_36KHZ;
        ir->reg->REG_IR_CARRY_CONFIG.bit.IR_CARRY_LOW = IR_CARRY_LOW_CONFIG_36KHZ;
        // hardware parameter
        ir->reg->REG_IR_TIME_1.bit.IR_TIME_S1 = IR_TIME_RC5_TX_S1;
        ir->reg->REG_IR_CTRL.bit.IR_BIT_CYCLE = IR_TIME_RC5_TX_BIT_CYCLE;
        ir->reg->REG_IR_CTRL.bit.BIT_TIME_1 = IR_TIME_RC5_TX_TIME1;
        break;
    }
    // hardware mode
    ir->reg->REG_IR_CTRL.bit.SW_HW_MODE = 0x0;
    ir->reg->REG_IR_TX_CODE.bit.IR_TX_USERCODE = address;
    ir->reg->REG_IR_TX_CODE.bit.IR_TX_DATACODE = command;
    // set to tx mode
    ir->reg->REG_IR_CTRL.bit.TXRX_MODE = 0x1;
    // enable ir interrupt
    ir->reg->REG_IR_CTRL.bit.IR_INT_EN = 0x1;
    // enable ir
    ir->reg->REG_IR_CTRL.bit.IR_EN = 0x1;
    // start send message
    ir->reg->REG_IR_TX_CONFIG.bit.TX_START = 0x1;
    ir->reg->REG_IR_TX_CONFIG.bit.TX_START = 0x0;

    return CSK_DRIVER_OK;
}

/** @fn IR_HW_Receive(void *res, IR_MODE mode, uint16_t *address, uint16_t *command)
 *  @brief Hardware mode reception
 *  @param[in] res Resource structure pointer
 *  @param[in] mode Protocol mode (NEC/9012/RC5)
 *  @param[out] address Store received address here
 *  @param[out] command Store received command here
 *  @return int32_t Status code
 *  @details Configures hardware receiver for specified protocol and stores results in provided pointers.
 */
int32_t  IR_HW_Receive(void *res, IR_MODE mode, uint16_t *address, uint16_t *command){
    CHECK_RESOURCES(res);
    IR_RESOURCES *ir = (IR_RESOURCES*)res;

    if(!(ir->info->flags & CSK_FLAG_POWERED)){
        return CSK_DRIVER_ERROR;
    }

    if(ir->info->busy){
        return CSK_DRIVER_ERROR_BUSY;
    }

    ir->reg->REG_IR_CTRL.bit.IR_EN = 0x0;
    ir->info->busy = 0x1;
    ir->info->hw_info->rx_address = address;
    ir->info->hw_info->rx_command = command;
    ir->info->tx_rx_c = 0;
    ir->info->mode = mode;

    //*********************register configure
    switch (ir->info->mode){
    case IR_MODE_CUSTOM:
        return CSK_DRIVER_ERROR_PARAMETER;
    case IR_MODE_NEC:
        // nec mode
        ir->reg->REG_IR_CTRL.bit.IR_MODE = 0x0;
        ir->reg->REG_IR_TIME_1.bit.IR_TIME_S1 = IR_TIME_NEC_RX_S1;
        ir->reg->REG_IR_TIME_2.bit.IR_TIME_S2 = IR_TIME_NEC_RX_S2;
        ir->reg->REG_IR_TIME_3.bit.IR_TIME_S3 = IR_TIME_NEC_RX_S3;
        ir->reg->REG_IR_TIME_4.bit.IR_TIME_S4 = IR_TIME_NEC_RX_S4;
        ir->reg->REG_IR_TIME_5.bit.IR_TIME_S5 = IR_TIME_NEC_RX_S5;
        ir->reg->REG_IR_CTRL.bit.BIT_TIME_1 = IR_TIME_NEC_RX_TIME1;
        ir->reg->REG_IR_CTRL.bit.BIT_TIME_2 = IR_TIME_NEC_RX_TIME2;
        ir->reg->REG_IR_CTRL.bit.IR_BIT_CYCLE = IR_TIME_NEC_RX_BIT_CYCLE;
        break;
    case IR_MODE_TOSHIBA_9012:
        // 9012 mode
        ir->reg->REG_IR_CTRL.bit.IR_MODE = 0x1;
        ir->reg->REG_IR_TIME_1.bit.IR_TIME_S1 = IR_TIME_9012_RX_S1;
        ir->reg->REG_IR_TIME_2.bit.IR_TIME_S2 = IR_TIME_9012_RX_S2;
        ir->reg->REG_IR_TIME_3.bit.IR_TIME_S3 = IR_TIME_9012_RX_S3;
        ir->reg->REG_IR_TIME_4.bit.IR_TIME_S4 = IR_TIME_9012_RX_S4;
        ir->reg->REG_IR_TIME_5.bit.IR_TIME_S5 = IR_TIME_9012_RX_S5;
        ir->reg->REG_IR_CTRL.bit.BIT_TIME_1 = IR_TIME_9012_RX_TIME1;
        ir->reg->REG_IR_CTRL.bit.BIT_TIME_2 = IR_TIME_9012_RX_TIME2;
        ir->reg->REG_IR_CTRL.bit.IR_BIT_CYCLE = IR_TIME_9012_RX_BIT_CYCLE;
        break;
    case IR_MODE_PHILIPS_RC5:
        // rc5 mode
        ir->reg->REG_IR_CTRL.bit.IR_MODE = 0x2;
        ir->reg->REG_IR_TIME_1.bit.IR_TIME_S1 = IR_TIME_RC5_RX_S1;
        ir->reg->REG_IR_TIME_2.bit.IR_TIME_S2 = IR_TIME_RC5_RX_S2;
        ir->reg->REG_IR_TIME_3.bit.IR_TIME_S3 = IR_TIME_RC5_RX_S3;
        ir->reg->REG_IR_TIME_4.bit.IR_TIME_S4 = IR_TIME_RC5_RX_S4;
        ir->reg->REG_IR_TIME_5.bit.IR_TIME_S5 = IR_TIME_RC5_RX_S5;
        ir->reg->REG_IR_CTRL.bit.BIT_TIME_1 = IR_TIME_RC5_RX_TIME1;
        ir->reg->REG_IR_CTRL.bit.BIT_TIME_2 = IR_TIME_RC5_RX_TIME2;
        ir->reg->REG_IR_CTRL.bit.IR_BIT_CYCLE = IR_TIME_RC5_RX_BIT_CYCLE;
        break;
    }
    // hardware mode
    ir->reg->REG_IR_CTRL.bit.SW_HW_MODE = 0x0;
    // enable ir interrupt
    ir->reg->REG_IR_CTRL.bit.IR_INT_EN = 0x1;
    // set to rx mode
    ir->reg->REG_IR_CTRL.bit.TXRX_MODE = 0x0;
    ir->reg->REG_IR_CTRL.bit.IR_EN = 0x1;

    return CSK_DRIVER_OK;
}

/** @fn IR_SW_Send(void *res, uint16_t *data, uint32_t num)
 *  @brief Software mode transmission
 *  @param[in] res Resource structure pointer
 *  @param[in] data Data buffer pointer
 *  @param[in] num Number of data items
 *  @return int32_t Status code
 *  @details Uses DMA for software-controlled transmission with custom carrier waveform.
 */
int32_t IR_SW_Send(void *res, uint16_t *data, uint32_t num){
    CHECK_RESOURCES(res);
    IR_RESOURCES *ir = (IR_RESOURCES*)res;

    if(!(ir->info->flags & CSK_FLAG_POWERED)){
        return CSK_DRIVER_ERROR;
    }

    if(ir->info->busy){
        return CSK_DRIVER_ERROR_BUSY;
    }

    // disable ir
    ir->reg->REG_IR_CTRL.bit.IR_EN = 0x0;

    //********************software information configure
    // set busy
    ir->info->busy = 0x1;
    ir->info->tx_rx_c = 0x1;

    ir->info->mode = IR_MODE_CUSTOM;

    ir->info->sw_info->tx_buf = data;
    ir->info->sw_info->tx_num = num;
    ir->info->sw_info->tx_cnt = 0;

    //*********************register configure
    // software mode
    ir->reg->REG_IR_CTRL.bit.SW_HW_MODE = 0x1;
    // carry
    ir->reg->REG_IR_CARRY_CONFIG.bit.IR_CARRY_HIGH = ir->info->sw_info->h_carry;
    ir->reg->REG_IR_CARRY_CONFIG.bit.IR_CARRY_LOW = ir->info->sw_info->l_carry;
    // tx mode
    ir->reg->REG_IR_CTRL.bit.TXRX_MODE = 0x1;
    // enable tx dma
    // 8-fifo_use >= threshold
    ir->reg->REG_IR_DMA_CONFIG.bit.TX_DMA_THRES_SEL = 0x7;
    ir->reg->REG_IR_DMA_CONFIG.bit.TX_DMA_ENABLE = 0x1;

    //*********************DMA configure
    {
        int32_t stat;

        dma_channel_select(
            &ir->dma_tx->channel,
            ir->dma_tx->cb_event,
            0,
            DMA_CACHE_SYNC_SRC);
        if (ir->dma_tx->channel == DMA_CHANNEL_ANY) {
            return CSK_DRIVER_ERROR;
        }

        stat = dma_channel_configure (ir->dma_tx->channel,
                    (uint32_t) ir->info->sw_info->tx_buf,
                    (uint32_t) (&(ir->reg->REG_IR_TX_FIFO.all)),
                    ir->info->sw_info->tx_num,
                    DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_HALFWORD) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_HALFWORD) |\
                  DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_1) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_1) |\
                  DMA_CH_CTLL_DST_FIX | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_TTFC_M2P |\
                  DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN, // control
                  DMA_CH_CFGL_CH_PRIOR(1), // config_low
                  DMA_CH_CFGH_FIFO_MODE | DMA_CH_CFGH_DST_PER(ir->dma_tx->reqsel), // config_high
                  0, 0);

        if(stat == -1){
            return CSK_DRIVER_ERROR;
        }
    }

    // enable ir interrupt
    ir->reg->REG_IR_CTRL.bit.IR_INT_EN = 0x1;
    // enable ir
    ir->reg->REG_IR_CTRL.bit.IR_EN = 0x1;

    return CSK_DRIVER_OK;
}

/** @fn IR_SW_Receive(void *res, uint16_t *data, uint32_t num)
 *  @brief Software mode reception
 *  @param[in] res Resource structure pointer
 *  @param[in] data Data buffer pointer
 *  @param[in] num Number of data items
 *  @return int32_t Status code
 *  @details Uses DMA for software-controlled reception with timeout detection.
 */
int32_t IR_SW_Receive(void *res, uint16_t *data, uint32_t num){
    CHECK_RESOURCES(res);
    IR_RESOURCES *ir = (IR_RESOURCES*)res;

    if(!(ir->info->flags & CSK_FLAG_POWERED)){
        return CSK_DRIVER_ERROR;
    }

    if(ir->info->busy){
        return CSK_DRIVER_ERROR_BUSY;
    }

    // disable ir
    ir->reg->REG_IR_CTRL.bit.IR_EN = 0x0;

    //********************software information configure
    // set busy
    ir->info->busy = 0x1;
    ir->info->tx_rx_c = 0x0;

    ir->info->mode = IR_MODE_CUSTOM;

    ir->info->sw_info->rx_buf = data;
    ir->info->sw_info->rx_num = num;
    ir->info->sw_info->rx_cnt = 0;

    //*********************register configure
    // software mode
    ir->reg->REG_IR_CTRL.bit.SW_HW_MODE = 0x1;
    // rx mode
    ir->reg->REG_IR_CTRL.bit.TXRX_MODE = 0x0;
    // rx time out
    ir->reg->REG_IR_IDLE_THRES.bit.THRES = ir->info->sw_info->thres;
    // enable rx dma
    // fifo_use >= threshold
    ir->reg->REG_IR_DMA_CONFIG.bit.RX_DMA_THRES_SEL = 0x1;
    ir->reg->REG_IR_DMA_CONFIG.bit.RX_DMA_ENABLE = 0x1;

    //*********************DMA configure
    {
        int32_t stat;

        dma_channel_select(
            &ir->dma_rx->channel,
            ir->dma_rx->cb_event,
            0,
            DMA_CACHE_SYNC_DST);
        if (ir->dma_rx->channel == DMA_CHANNEL_ANY) {
            return CSK_DRIVER_ERROR;
        }

        stat = dma_channel_configure (ir->dma_rx->channel,
                    (uint32_t) (&(ir->reg->REG_IR_RX_FIFO.all)),
                    (uint32_t) ir->info->sw_info->rx_buf,
                    ir->info->sw_info->rx_num,
                    DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_HALFWORD) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_HALFWORD) |\
                  DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_1) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_1) |\
                  DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_TTFC_P2M |\
                  DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN, // control
                  DMA_CH_CFGL_CH_PRIOR(1), // config_low
                  DMA_CH_CFGH_FIFO_MODE | DMA_CH_CFGH_SRC_PER(ir->dma_rx->reqsel), // config_high
                  0, 0);

        if(stat == -1){
            return CSK_DRIVER_ERROR;
        }
    }

    // enable ir interrupt
    ir->reg->REG_IR_CTRL.bit.IR_INT_EN = 0x1;
    // enable ir
    ir->reg->REG_IR_CTRL.bit.IR_EN = 0x1;

    return CSK_DRIVER_OK;
}

/** @fn IR_GetTxCount(void *res)
 *  @brief Get transmitted byte count
 *  @param[in] res Resource structure pointer
 *  @return uint32_t Number of transmitted bytes
 */
uint32_t IR_GetTxCount(void *res){
    CHECK_RESOURCES(res);
    IR_RESOURCES *ir = (IR_RESOURCES*)res;
    return ir->info->sw_info->tx_cnt;
}

/** @fn IR_GetRxCount(void *res)
 *  @brief Get received byte count
 *  @param[in] res Resource structure pointer
 *  @return uint32_t Number of received bytes
 */
uint32_t IR_GetRxCount(void *res){
    CHECK_RESOURCES(res);
    IR_RESOURCES *ir = (IR_RESOURCES*)res;
    return ir->info->sw_info->rx_cnt;
}

/** @fn IR_IRQ_Handler(IR_RESOURCES *ir)
 *  @brief Interrupt service routine
 *  @param[in] ir Resource structure pointer
 *  @details Processes all protocol events and triggers corresponding callbacks.
 */
static void IR_IRQ_Handler(IR_RESOURCES *ir){
    uint32_t c_status = 0;
    uint32_t event = 0;
    c_status = ir->reg->REG_IR_FSM.all;

    // TODO clear pending

    // clear status
    ir->reg->REG_IR_CLEAR_STATUS.all = 0x7f;

    // NEC Receive Mode Handling
    if ((ir->info->mode == IR_MODE_NEC) && (ir->info->tx_rx_c == 0)){
        // check repeat mode
        if (c_status & IR_IR_FSM_IR_REPEAT_Msk){
            event |= CSK_IR_EVENT_HW_RX_REPEAT_TRIGGER;
        }

        // receive ok and busy
        if ((c_status & IR_IR_FSM_IR_RECEIVED_OK_Msk) && (ir->info->busy == 1)){
            ir->info->busy = 0;

            *(ir->info->hw_info->rx_address) = (uint16_t)(ir->reg->REG_IR_RX_CODE.bit.IR_RX_USERCODE & 0xffff);
            *(ir->info->hw_info->rx_command) = (uint16_t)(ir->reg->REG_IR_RX_CODE.bit.IR_RX_DATACODE & 0xffff);

            event |= CSK_IR_EVENT_HW_RECEIVE_COMPLETE;
        }
    }

    // in nec transmit mode
    if ((ir->info->mode == IR_MODE_NEC) && (ir->info->tx_rx_c == 1)){
        // check tx repeat mode
        if (c_status & IR_IR_FSM_IR_TX_REPEAT_Msk){
            event |= CSK_IR_EVENT_HW_TX_REPEAT_COMPLETE;

            // disable repeat mode
            ir->reg->REG_IR_CTRL.bit.TX_REPEAT_MODE = 0x0;
        }

        // transmit ok and busy
        if ((c_status & IR_IR_FSM_IR_TRANSMIT_OK_Msk) && (ir->info->busy == 1)){
            event |= CSK_IR_EVENT_HW_SEND_COMPLETE;
            ir->info->busy = 0;
        }
    }

    // in 9012 receive mode
    if ((ir->info->mode == IR_MODE_TOSHIBA_9012) && (ir->info->tx_rx_c == 0)){

        // receive ok and busy
        if ((c_status & IR_IR_FSM_IR_RECEIVED_OK_Msk) && (ir->info->busy == 1)){
            ir->info->busy = 0;

            *(ir->info->hw_info->rx_address) = (uint16_t)(ir->reg->REG_IR_RX_CODE.bit.IR_RX_USERCODE & 0xffff);
            *(ir->info->hw_info->rx_command) = (uint16_t)(ir->reg->REG_IR_RX_CODE.bit.IR_RX_DATACODE & 0xffff);

            ir->reg->REG_IR_CTRL.bit.IR_INT_EN = 0x0;

            event |= CSK_IR_EVENT_HW_RECEIVE_COMPLETE;
        }
    }

    // in 9012 transmit mode
    if ((ir->info->mode == IR_MODE_TOSHIBA_9012) && (ir->info->tx_rx_c == 1)){

        // transmit ok and busy
        if ((c_status & IR_IR_FSM_IR_TRANSMIT_OK_Msk) && (ir->info->busy == 1)){

            event |= CSK_IR_EVENT_HW_SEND_COMPLETE;
            ir->info->busy = 0;

            ir->reg->REG_IR_CTRL.bit.IR_INT_EN = 0x0;
        }
    }

    // in rc5 receive mode
    if ((ir->info->mode == IR_MODE_PHILIPS_RC5) && (ir->info->tx_rx_c == 0)){

        // receive ok and busy
        if ((c_status & IR_IR_FSM_IR_RECEIVED_OK_Msk) && (ir->info->busy == 1)){
            ir->info->busy = 0;

            *(ir->info->hw_info->rx_address) = (uint16_t)(ir->reg->REG_IR_RX_CODE.bit.IR_RX_USERCODE & 0xffff);
            *(ir->info->hw_info->rx_command) = (uint16_t)(ir->reg->REG_IR_RX_CODE.bit.IR_RX_DATACODE & 0xffff);

            ir->reg->REG_IR_CTRL.bit.IR_INT_EN = 0x0;

            event |= CSK_IR_EVENT_HW_RECEIVE_COMPLETE;
        }
    }

    // in rc5 transmit mode
    if ((ir->info->mode == IR_MODE_PHILIPS_RC5) && (ir->info->tx_rx_c == 1)){

        // transmit ok and busy
        if ((c_status & IR_IR_FSM_IR_TRANSMIT_OK_Msk) && (ir->info->busy == 1)){

            event |= CSK_IR_EVENT_HW_SEND_COMPLETE;
            ir->info->busy = 0;

            ir->reg->REG_IR_CTRL.bit.IR_INT_EN = 0x0;
        }
    }


    // because ir timeout , interrupt should ignore the first
    // timeout interrupt
    // in custom receive mode
    if ((ir->info->mode == IR_MODE_CUSTOM) && (ir->info->tx_rx_c == 0)){
        // get DMA receive count
        ir->info->sw_info->rx_cnt = dma_channel_get_count(ir->dma_rx->channel);

        if ((c_status & IR_IR_FSM_IR_RECEIVED_OK_Msk) && (ir->info->busy == 1)){
            // TIME OUT
            if (ir->info->sw_info->rx_cnt != ir->info->sw_info->rx_num){
                while(!ir->reg->REG_IR_FSM.bit.IR_RX_FIFO_EMPTY);

                dma_channel_disable(ir->dma_rx->channel, 0x1);

                // in power up the first receive data equal 0x80000000, it's wrong
                ir->info->sw_info->rx_buf[0] = 0x8004;

                ir->reg->REG_IR_CTRL.bit.IR_INT_EN = 0x0;
                ir->reg->REG_IR_DMA_CONFIG.bit.RX_DMA_ENABLE = 0x0;

                event |= CSK_IR_EVENT_SW_RECEIVE_TIMEOUT;

                ir->info->busy = 0x0;
            }
            // ignore
            else {
                return;
            }
        }
    }

    // Custom Transmit Mode Handling
    if ((ir->info->mode == IR_MODE_CUSTOM) && (ir->info->tx_rx_c == 1)){
        if((c_status & IR_IR_FSM_IR_TRANSMIT_OK_Msk) && (ir->info->busy == 1)){
            while(!ir->reg->REG_IR_FSM.bit.IR_TX_FIFO_EMPTY);
            ir->info->sw_info->tx_cnt = dma_channel_get_count(ir->dma_tx->channel);
            event |= CSK_IR_EVENT_SW_SEND_COMPLETE;
            ir->reg->REG_IR_CTRL.bit.IR_INT_EN = 0x0;
            ir->info->busy = 0;
        }
    }

    if (event == 0){
        return;
    }

    if(ir->info->cb_event){
        ir->info->cb_event(event, ir->info->workspace);
    }
}
/** @fn IR_DMA_TX_Handler(uint32_t event, IR_RESOURCES *ir)
 *  @brief DMA transmit completion handler
 *  @param[in] event DMA event type
 *  @param[in] ir Resource structure pointer
 */
static void IR_DMA_TX_Handler(uint32_t event, IR_RESOURCES *ir){
    switch (event){
    case DMA_EVENT_TRANSFER_COMPLETE:
        while(ir->reg->REG_IR_FSM.bit.IR_TX_FIFO_FULL);
        ir->reg->REG_IR_TX_FIFO.all = 0x8000;
        break;
    case DMA_EVENT_ERROR:
    default:
        break;
    }
}

/** @fn IR_DMA_RX_Handler(uint32_t event, IR_RESOURCES *ir)
 *  @brief DMA receive completion handler
 *  @param[in] event DMA event type
 *  @param[in] ir Resource structure pointer
 */
static void IR_DMA_RX_Handler(uint32_t event, IR_RESOURCES *ir){
    switch (event){
    case DMA_EVENT_TRANSFER_COMPLETE:
        ir->info->sw_info->rx_cnt = dma_channel_get_count(ir->dma_rx->channel);
        ir->info->busy = 0x0;
        ir->info->sw_info->rx_buf[0] = 0x8004;
        if(ir->info->cb_event)
            ir->info->cb_event(CSK_IR_EVENT_SW_RECEIVE_COMPLETE, ir->info->workspace);
        break;
    case DMA_EVENT_ERROR:
    default:
        break;
    }
}

/* IR0 static function implementations */
static void IR0_IRQ_Handler(){
    IR_IRQ_Handler(&ir0_resources);
}

static void IR0_DMA_TX_Handler(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param){
    IR_DMA_TX_Handler((event & 0xFF), &ir0_resources);
}

static void IR0_DMA_RX_Handler(uint32_t event, uint32_t xfer_bytes, uint32_t usr_param){
    IR_DMA_RX_Handler((event & 0xFF), &ir0_resources);
}
