#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "venusa_ap.h"
#include "log_print.h"
#include "systick.h"
#include "ClockManager.h"
#include "IOMuxManager.h"
#include "Driver_GPIO.h"
#include "Driver_I2C.h"
#include "Driver_DVP.h"
#include "cache.h"
#include "check.h"

#define VIDEO_LOG
#define DELAY_MS(x) SysTick_Delay_Ms(x)
#define DELAY_US(x) SysTick_Delay_Us(x)

#ifndef DPU_VERSION
#define SENSOR_HIGH_FRAMERATE           1
#else
#define SENSOR_HIGH_FRAMERATE           0
#endif

static int32_t dvp_cmndma_init(void);
static int32_t dvp_cmndma_start(void* pbuf, uint32_t size_word);
static uint32_t dvp_cmndma_finish_cnt_get(void);
static void dvp_cmndma_finish_cnt_clear(void);
static void cmndma_reg_dump(uint8_t dma_ch);
static void dvp_reg_dump(void);

static int csk_i2c_init(uint8_t i2c_index);
static int csk_i2c_write_reg8(uint8_t i2c_index, uint16_t addr, uint8_t reg, uint8_t value);
static int csk_i2c_read(uint8_t i2c_index, uint16_t addr, uint8_t reg, uint8_t *data, uint16_t num);
static uint8_t csk_i2c_read_reg8(uint8_t i2c_index, uint16_t addr, uint8_t reg);
static void gc0328_init(uint8_t colorbar_enable);
static void ov5640_init(uint8_t colorbar_enable);


static void dvp_in_dma2d_callback(DVP_emIrqEvent event, uint32_t param)
{
    void *dvp_dev = DVP0();
    uint32_t error;

    switch(event)
    {
        case DVP_IRQ_EVENT_SOF:
            //VIDEO_LOG("[%s:%d] DVP SOF event: %d", __func__, __LINE__, event);
            break;

        case DVP_IRQ_EVENT_EOF:
            //VIDEO_LOG("[%s:%d] DVP EOF event: %d", __func__, __LINE__, event);
            break;

        case DVP_IRQ_EVENT_FRAME_FINISH:
            //VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_FRAME_FINISH", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_EOF_CNT_ABNOR:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_EOF_CNT_ABNOR", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_DMA_VIC_SINGLE:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_DMA_VIC_SINGLE", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_DMA_VIC_REQ:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_DMA_VIC_REQ", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_FIFO_UNFLOW:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_FIFO_UNFLOW", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_FIFO_OVFLOW:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_FIFO_OVFLOW", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_FIFO_RD_EMPTY:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_FIFO_RD_EMPTY", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_FIFO_WR_FULL:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_FIFO_WR_FULL", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_H_SYNC_ABNOR:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_H_SYNC_ABNOR", __func__, __LINE__);
            break;

        case DVP_IRQ_EVENT_PIXEL_ABNOR:
            VIDEO_LOG("[%s:%d] DVP_IRQ_EVENT_PIXEL_ABNOR", __func__, __LINE__);
            break;

        default:
            VIDEO_LOG("[%s:%d] DVP error event: %d", __func__, __LINE__, event);
            break;
    }

//    error = DVP_GetError(dvp_dev, &error);
//
//    if(error != DVP_ERROR_NONE)
//    {
//        /* turn off the clock out when the required frames received*/
//        DVP_DisableClockout();
//        DVP_Stop(dvp_dev);
//        VIDEO_LOG("[%s:%d] DVP Error code: %d", __func__, __LINE__, error);
//    }

    return;
}

int32_t dvp_in_start(uint16_t width, uint16_t height)
{
    int32_t ret = FAILURE;
    DVP_InitTypeDef dvp_cfg = {
        .FrameWidth = width,
        .FrameHeight = height,
        .PixelOffset = 0,
        .LineOffset = 0,
        .InputFormat = DVP_INPUT_FORM_YUV422_Y0CBY1CR,
        .PCKPolarity = DVP_POL_RISING,
        .VSPolarity = DVP_POL_RISING,
        .HSPolarity = DVP_POL_RISING,
        .DataAlign = DVP_DATA_ALIGN_LEFT,
        .BurstThreshold = 8,
    };

    /* pinmux */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_MCLK
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 23, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_PCLK
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_VS
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 21, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_HS
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 24, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_D0
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 25, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_D1
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 26, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_D2
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 27, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_D3
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 28, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_D4
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 29, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_D5
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 30, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_D6
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 31, CSK_IOMUX_FUNC_ALTER13);  // PIN_DVP_D7
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  3, CSK_IOMUX_FUNC_DEFAULT);  // PIN_DVP_RST
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  2, CSK_IOMUX_FUNC_DEFAULT);  // PIN_DVP_PWDN
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  1, CSK_IOMUX_FUNC_ALTER7);   // PIN_DVP_SCL I2C0_SCL
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B,  0, CSK_IOMUX_FUNC_ALTER7);   // PIN_DVP_SDA I2C0_SDA

    /* PIN_DVP_RST */
    GPIO_SetDir(GPIOB(), (1UL << 3), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(GPIOB(), (1UL << 3), 0);
    DELAY_MS(100);
    GPIO_PinWrite(GPIOB(), (1UL << 3), 1);
    DELAY_MS(100);

    /* PIN_DVP_PWDN */
    GPIO_SetDir(GPIOB(), (1UL << 2), CSK_GPIO_DIR_OUTPUT);
    GPIO_PinWrite(GPIOB(), (1UL << 2), 0);

    /* dvp clk enable and reset */
    IP_SYSCTRL->REG_PERI_CLK_CFG7.bit.ENA_VIC_CLK = 1;
    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.DVP_RESET   = 1;
    DELAY_MS(10);

    DVP_Stop(DVP0());
    DVP_Uninitialize(DVP0());
    DVP_DisableClockout();

    /* dvp init */
    ret = DVP_Initialize(DVP0(), dvp_in_dma2d_callback, &dvp_cfg);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);
    ret = DVP_EnableClockout(DVP_MCLK_OUT);
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);
    DELAY_MS(100);

    /* camera init */
    //gc0328_init(0);
    ov5640_init(0);
    DELAY_MS(100);
    VIDEO_LOG("[%s:%d] ", __func__, __LINE__);

    /* dvp start */
    ret = DVP_Start(DVP0());
    CHECK_RET_EQ_EXIT(ret, CSK_DRIVER_OK, error0);

	VIDEO_LOG("[%s:%d] test SUCCESS", __func__, __LINE__);

    return ret;

error0:
    dvp_reg_dump();
    DVP_Stop(DVP0());
    DVP_Uninitialize(DVP0());
    DVP_DisableClockout();

    VIDEO_LOG("[%s:%d] test FAILED", __func__, __LINE__);

    return ret;
}

void dvp_in_stop()
{
    DVP_Stop(DVP0());
    // DVP_Uninitialize(DVP0());
    // DVP_DisableClockout(DVP0());
    return;
}

int dvp_in_restart(uint8_t *dvp_image, uint32_t imgae_size)
{
    int ret = dvp_cmndma_start(dvp_image, imgae_size / sizeof(uint32_t));
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    ret = DVP_Start(DVP0());
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return CSK_DRIVER_OK;
}


/****************************************** CMN_DMA ************************************************/
/********************************************************************************************************/
/********************************************************************************************************/
/********************************************************************************************************/

#include "dma.h"

static volatile uint32_t dvp_cmndma_finish_cnt = 0;

static void dvp_cmndma_callback(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    //VIDEO_LOG("[%s]: event = %d, channel = %d, xfer_bytes = %d", __func__, event_info & 0xFF, (event_info >> 8) & 0xFF, xfer_bytes);
    if(event_info & DMA_EVENT_TRANSFER_COMPLETE){
        dvp_cmndma_finish_cnt++;
    }
}

static uint32_t dvp_cmndma_finish_cnt_get(void)
{
    return dvp_cmndma_finish_cnt;
}

static void dvp_cmndma_finish_cnt_clear(void)
{
    dvp_cmndma_finish_cnt = 0;
}

static int32_t dvp_cmndma_init(void)
{
    dvp_cmndma_finish_cnt = 0;

    IP_SYSCTRL->REG_PERI_CLK_CFG7.bit.ENA_CMNDMAC_CLK = 1;
    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.CMNDMA_RESET     = 1;

    dma_initialize();

    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_03 = 1;
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_15 = 1;
    IP_SYSCTRL->REG_DMA_HS.bit.DMA_HS_SEL_19 = 1;

    return CSK_DRIVER_OK;
}


static int32_t dvp_cmndma_start(void* pbuf, uint32_t size_word)
{
    int32_t ret = 0;
    uint8_t dma_ch = 0;
    uint32_t control, config_low, config_high;
    uint32_t src_addr = DVP0_Buf();
    uint32_t dst_addr = (uint32_t)pbuf;

    CHECK_POINT_NOT_NULL(pbuf);

    control = DMA_CH_CTLL_INT_EN | DMA_CH_CTLL_DST_WIDTH(DMA_WIDTH_WORD) | DMA_CH_CTLL_SRC_WIDTH(DMA_WIDTH_WORD) |
            DMA_CH_CTLL_DST_BSIZE(DMA_BSIZE_8) | DMA_CH_CTLL_SRC_BSIZE(DMA_BSIZE_8) |
            DMA_CH_CTLL_TTFC_P2M | DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(1);

    control |=  DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_DST_INC;

    config_low = DMA_CH_CFGL_CH_PRIOR(0);
    config_high = DMA_CH_CFGH_FIFO_MODE | DMA_CH_CFGH_SRC_PER(3); // DMA_CH_CFGH_SRC_PER(x) | DMA_CH_CFGH_DST_PER

    ret = dma_channel_select(&dma_ch, dvp_cmndma_callback, 0, DMA_CACHE_SYNC_NOP);
    if (ret == DMA_CHANNEL_ANY) {
        VIDEO_LOG("[FAILED] NO free DMA channel!!");
        return ret;
    }
    //VIDEO_LOG("[%s:%d] dma_ch=%d", __func__, __LINE__, dma_ch);

    ret = dma_channel_configure (dma_ch, src_addr, dst_addr, size_word, control, config_low, config_high, 0, 0);
    CHECK_RET_EQ(ret, CSK_DRIVER_OK);

    return ret;
}


//--------------------------------------------------------------------------
// For DWORD (64bit) register, low WORD(32bit) is valid and high WORD(32bit) is not used.
#define DWORD_REG(name)     uint32_t name; uint32_t __pad_##name

// Per-channel hardware register definitions
typedef struct {
    // The first 6 DWORD(64bit) registers are same as DMA_LLI
    __IO DWORD_REG(SAR);    // Source Address Register
    __IO DWORD_REG(DAR);    // Destination Address Register
    __IO DWORD_REG(LLP);    // Linked List Pointer
    __IO uint32_t CTL_LO;   // Control Register Low WORD
    __IO uint32_t CTL_HI;   // Control Register High WORD
    __IO DWORD_REG(SSTAT);  // Source Status Register, unimplemented, set to 0
    __IO DWORD_REG(DSTAT);  // Destination Status Register, unimplemented, set to 0
    // The following registers are NOT in DMA_LLI
    __IO DWORD_REG(SSTATAR); // Source Status Address Register, unused
    __IO DWORD_REG(DSTATAR); // Destination Status Address Register, unused
    __IO uint32_t CFG_LO;   // Configuration Register Low WORD
    __IO uint32_t CFG_HI;   // Configuration Register High WORD
    __IO DWORD_REG(SGR);    // Source Gather Register
    __IO DWORD_REG(DSR);    // Destination Scatter Register
} DMA_CHANNEL_REG;

// Interrupt register definitions
typedef struct {
    __IO DWORD_REG(XFER);
    __IO DWORD_REG(BLOCK);
    __IO DWORD_REG(SRC_TRAN);
    __IO DWORD_REG(DST_TRAN);
    __IO DWORD_REG(ERROR);
} DMA_IRQ_REG;

// Overall register memory map
typedef struct {
    // 0x000 ~ 0x2b8 N Channels' Registers
    DMA_CHANNEL_REG   CHANNEL[DMA_MAX_NR_CHANNELS];

    DMA_IRQ_REG     RAW;    // [RO] 0x2c0 ~ 0x2e0 raw
    DMA_IRQ_REG     STATUS; // [RO] 0x2e8 ~ 0x308 (raw & mask)
    DMA_IRQ_REG     MASK;   // [RW] 0x310 ~ 0x330 (set = irq enabled)
    DMA_IRQ_REG     CLEAR;  // [WO] 0x338 ~ 0x358 (clear raw and status)

    // [RO] 0x360 Combined Interrupt Status Register
    __IO DWORD_REG(STA_INT);

    // 0x368 ~ 0x390 software handshaking
    __IO DWORD_REG(REQ_SRC);
    __IO DWORD_REG(REQ_DST);
    __IO DWORD_REG(SGL_REQ_SRC);
    __IO DWORD_REG(SGL_REQ_DST);
    __IO DWORD_REG(LAST_SRC);
    __IO DWORD_REG(LAST_DST);

    // 0x398 ~ 0x3b0 miscellaneous
    __IO DWORD_REG(CFG);
    __IO DWORD_REG(CH_EN);
    __IO DWORD_REG(ID);
    __IO DWORD_REG(TEST);

    // 0x3b8 ~ 0x3c0 reserved
    __IO DWORD_REG(__RSVD0);
    __IO DWORD_REG(__RSVD1);

    // 0x3c8 ~ 0x3f0 hardware configuration parameters
    __I uint64_t COMP_PARAMS[6];

    // 0x3f8 Component version register
    __I uint64_t COMP_VER;

} DMA_RegMap;

#define IP_DMA              ((DMA_RegMap *) DMAC_BASE)

static void cmndma_reg_dump(uint8_t dma_ch)
{
    VIDEO_LOG("[SYS] CFG7       *0x%08x = 0x%08x", &IP_SYSCTRL->REG_PERI_CLK_CFG7.all, IP_SYSCTRL->REG_PERI_CLK_CFG7.all);
    VIDEO_LOG("[SYS] RESET_CFG2 *0x%08x = 0x%08x", &IP_SYSCTRL->REG_SW_RESET_CFG2.all, IP_SYSCTRL->REG_SW_RESET_CFG2.all);
    VIDEO_LOG("[SYS] DMA_HS     *0x%08x = 0x%08x", &IP_SYSCTRL->REG_DMA_HS.all, IP_SYSCTRL->REG_DMA_HS.all);
    VIDEO_LOG("[DMA] SAR[%d]    *0x%08x = 0x%08x", dma_ch, &IP_DMA->CHANNEL[dma_ch].SAR, IP_DMA->CHANNEL[dma_ch].SAR);
    VIDEO_LOG("[DMA] DAR[%d]    *0x%08x = 0x%08x", dma_ch, &IP_DMA->CHANNEL[dma_ch].DAR, IP_DMA->CHANNEL[dma_ch].DAR);
    VIDEO_LOG("[DMA] CTL_LO[%d] *0x%08x = 0x%08x", dma_ch, &IP_DMA->CHANNEL[dma_ch].CTL_LO, IP_DMA->CHANNEL[dma_ch].CTL_LO);
    VIDEO_LOG("[DMA] CTL_HI[%d] *0x%08x = 0x%08x", dma_ch, &IP_DMA->CHANNEL[dma_ch].CTL_HI, IP_DMA->CHANNEL[dma_ch].CTL_HI);
    VIDEO_LOG("[DMA] CFG_LO[%d] *0x%08x = 0x%08x", dma_ch, &IP_DMA->CHANNEL[dma_ch].CFG_LO, IP_DMA->CHANNEL[dma_ch].CFG_LO);
    VIDEO_LOG("[DMA] CFG_HI[%d] *0x%08x = 0x%08x", dma_ch, &IP_DMA->CHANNEL[dma_ch].CFG_HI, IP_DMA->CHANNEL[dma_ch].CFG_HI);
    VIDEO_LOG("[DMA] CFG        *0x%08x = 0x%08x", &IP_DMA->CFG, IP_DMA->CFG);
    VIDEO_LOG("[DMA] CH_EN      *0x%08x = 0x%08x", &IP_DMA->CH_EN, IP_DMA->CH_EN);
    VIDEO_LOG("[DMA] MASK       *0x%08x = 0x%08x", &IP_DMA->MASK.XFER, IP_DMA->MASK.XFER);
    VIDEO_LOG("[DMA] RAW        *0x%08x = 0x%08x", &IP_DMA->RAW.XFER, IP_DMA->RAW.XFER);
    VIDEO_LOG("[DMA] STATUS     *0x%08x = 0x%08x", &IP_DMA->STATUS.XFER, IP_DMA->STATUS.XFER);
}


static void dvp_reg_dump(void)
{
    VIDEO_LOG("[DVP] F_HOR          *0x%08x = 0x%08x", &IP_DVP_IN->REG_F_HOR.all, IP_DVP_IN->REG_F_HOR.all);
    VIDEO_LOG("[DVP] F_VER          *0x%08x = 0x%08x", &IP_DVP_IN->REG_F_VER.all, IP_DVP_IN->REG_F_VER.all);
    VIDEO_LOG("[DVP] P_OFFSET       *0x%08x = 0x%08x", &IP_DVP_IN->REG_P_OFFSET.all, IP_DVP_IN->REG_P_OFFSET.all);
    VIDEO_LOG("[DVP] L_OFFSET       *0x%08x = 0x%08x", &IP_DVP_IN->REG_L_OFFSET.all, IP_DVP_IN->REG_L_OFFSET.all);
    VIDEO_LOG("[DVP] CTRL           *0x%08x = 0x%08x", &IP_DVP_IN->REG_IMAGE_VIC_CTRL.all, IP_DVP_IN->REG_IMAGE_VIC_CTRL.all);
    //VIDEO_LOG("[DVP] FREQ_OUT       *0x%08x = 0x%08x", &IP_DVP_IN->REG_FREQ_OUT.all, IP_DVP_IN->REG_FREQ_OUT.all);
    VIDEO_LOG("[DVP] INPUT_FORM     *0x%08x = 0x%08x", &IP_DVP_IN->REG_INPUT_FORM.all, IP_DVP_IN->REG_INPUT_FORM.all);
    VIDEO_LOG("[DVP] VIC_EN         *0x%08x = 0x%08x", &IP_DVP_IN->REG_VIC_EN.all, IP_DVP_IN->REG_VIC_EN.all);
    VIDEO_LOG("[DVP] DMA_BURST_THD  *0x%08x = 0x%08x", &IP_DVP_IN->REG_DMA_BURST_THD.all, IP_DVP_IN->REG_DMA_BURST_THD.all);
    VIDEO_LOG("[DVP] INTR_MSK       *0x%08x = 0x%08x", &IP_DVP_IN->REG_IMAGE_VIC_INTR_MASK.all, IP_DVP_IN->REG_IMAGE_VIC_INTR_MASK.all);
    VIDEO_LOG("[DVP] INTR_CLR       *0x%08x = 0x%08x", &IP_DVP_IN->REG_IMAGE_VIC_INTR_CLR.all, IP_DVP_IN->REG_IMAGE_VIC_INTR_CLR.all);
    VIDEO_LOG("[DVP] VIC_IRQ        *0x%08x = 0x%08x", &IP_DVP_IN->REG_IMAGE_VIC_IRQ.all, IP_DVP_IN->REG_IMAGE_VIC_IRQ.all);
    VIDEO_LOG("[DVP] VIC_INT_STATUS *0x%08x = 0x%08x", &IP_DVP_IN->REG_IMAGE_VIC_INT_STATUS.all, IP_DVP_IN->REG_IMAGE_VIC_INT_STATUS.all);
    VIDEO_LOG("[DVP] VIC_INT_RAW    *0x%08x = 0x%08x", &IP_DVP_IN->REG_IMAGE_VIC_INT_RAW_STATUS.all, IP_DVP_IN->REG_IMAGE_VIC_INT_RAW_STATUS.all);
    VIDEO_LOG("[DVP] VIC_DEBUG      *0x%08x = 0x%08x", &IP_DVP_IN->REG_IMAGE_VIC_DEBUG.all, IP_DVP_IN->REG_IMAGE_VIC_DEBUG.all);
    VIDEO_LOG("[DVP] ST_DEBUG       *0x%08x = 0x%08x", &IP_DVP_IN->REG_ST_DEBUG.all, IP_DVP_IN->REG_ST_DEBUG.all);
}


/**************************************** I2C ***********************************************************/
/********************************************************************************************************/
/********************************************************************************************************/
/********************************************************************************************************/

static void* I2C_Handle[2] = {NULL};
static volatile uint32_t I2C_Event[2] = {0};

static inline void CLR_XFER_DONE(uint8_t i2c_index)
{
    I2C_Event[i2c_index] = 0;
}

static inline bool GET_XFER_DONE(uint8_t i2c_index)
{
    return (I2C_Event[i2c_index] & CSK_I2C_EVENT_TRANSFER_DONE);
}


static bool wait_xfer_done_timeout(uint8_t i2c_index, uint32_t max_wait_ms)
{
    bool ret = false;
    uint32_t cnt = 0;

    while(cnt++ < (max_wait_ms * 1000))
    {
        if(GET_XFER_DONE(i2c_index))
        {
            CLR_XFER_DONE(i2c_index);
            ret = true;
            break;
        }
        DELAY_US(1);
    }
    CLR_XFER_DONE(i2c_index);

    return ret;
}


static void I2C0_EventCallback(uint32_t event, void* workspace){
    I2C_Event[0] |= event;
}

static void I2C1_EventCallback(uint32_t event, void* workspace){
    I2C_Event[1] |= event;
}


/**
  * @brief  Initializes Camera low level.
  * @retval None
  */
static int csk_i2c_init(uint8_t i2c_index)
{
    if (i2c_index > 1) {
        VIDEO_LOG("[%s:%d] i2c_index=%d error, must be 0/1", __func__, __LINE__, i2c_index);
        return -1;
    }

    IP_SYSCTRL->REG_PERI_CLK_CFG7.bit.ENA_I2C0_CLK    = 1; // bit 0~0
    IP_SYSCTRL->REG_PERI_CLK_CFG7.bit.ENA_I2C1_CLK    = 1; // bit 1~1

//    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_C,  1, CSK_IOMUX_FUNC_ALTER7);   // PIN_DVP_SCL
//    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_C,  0, CSK_IOMUX_FUNC_ALTER7);   // PIN_DVP_SDA

    if (i2c_index == 0) {
        I2C_Handle[i2c_index] = I2C0();
        I2C_Initialize(I2C_Handle[i2c_index], I2C0_EventCallback, NULL);
    } else {
        I2C_Handle[i2c_index] = I2C1();
        I2C_Initialize(I2C_Handle[i2c_index], I2C1_EventCallback, NULL);
    }

    I2C_PowerControl(I2C_Handle[i2c_index], CSK_POWER_FULL);

    /* CSK_I2C_TRANSMIT_MODE:  arg0 = 1, means DMA mode ;  arg0 = 0, means Interrupt mode */
    I2C_Control(I2C_Handle[i2c_index], CSK_I2C_TRANSMIT_MODE, 0);
    I2C_Control(I2C_Handle[i2c_index], CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);  // CSK_I2C_BUS_SPEED_STANDARD/CSK_I2C_BUS_SPEED_FAST
    I2C_Control(I2C_Handle[i2c_index], CSK_I2C_BUS_CLEAR, 0);

    CLR_XFER_DONE(i2c_index);

    return 0;
}


/**
  * @brief  Camera writes single data.
  * @param  Addr: I2C address
  * @param  Reg: Register address
  * @param  Value: Data to be written
  * @retval None
  */
static int csk_i2c_write_reg8(uint8_t i2c_index, uint16_t addr, uint8_t reg, uint8_t value)
{
    int32_t ret = 0;
    uint8_t i2c_data[2];

    if (i2c_index > 1) {
        VIDEO_LOG("[%s:%d] i2c_index=%d error, must be 0/1", __func__, __LINE__, i2c_index);
        return -1;
    }

    if (I2C_Handle[i2c_index] == NULL) {
        VIDEO_LOG("[%s:%d] i2c_index=%d handle is NULL, not init", __func__, __LINE__, i2c_index);
        return -1;
    }

    CLR_XFER_DONE(i2c_index);

    i2c_data[0] = reg;
    i2c_data[1] = value;
    ret = I2C_MasterTransmit(I2C_Handle[i2c_index], addr, i2c_data, 2, 0);
    if (CSK_DRIVER_OK != ret) {
        VIDEO_LOG("[%s:%d] I2C error ret=%d", __func__, __LINE__, ret);
        return -1;
    }
    if (!wait_xfer_done_timeout(i2c_index, 1000)) { // 1000ms
        VIDEO_LOG("[%s:%d] I2C transfer is timeout!!", __func__, __LINE__);
        return -1;
    }

    return 0;
}

/**
  * @brief  Camera writes single data.
  * @param  Addr: I2C address
  * @param  Reg: Register address
  * @param  Value: Data to be written
  * @retval None
  */
int csk_i2c_write_reg16(uint8_t i2c_index, uint16_t addr, uint16_t reg, uint8_t value)
{
    int32_t ret = 0;
    uint8_t i2c_data[3];

    if (i2c_index > 1) {
        VIDEO_LOG("[%s:%d] i2c_index=%d error, must be 0/1", __func__, __LINE__, i2c_index);
        return -1;
    }

    if (I2C_Handle[i2c_index] == NULL) {
        VIDEO_LOG("[%s:%d] i2c_index=%d handle is NULL, not init", __func__, __LINE__, i2c_index);
        return -1;
    }

    CLR_XFER_DONE(i2c_index);

    i2c_data[0] = (reg >> 8) & 0xff;
    i2c_data[1] = reg & 0xff;
    i2c_data[2] = value;
    ret = I2C_MasterTransmit(I2C_Handle[i2c_index], addr, i2c_data, 3, 0);
    if (CSK_DRIVER_OK != ret) {
        VIDEO_LOG("[%s:%d] I2C error ret=%d", __func__, __LINE__, ret);
        return -1;
    }
    if (!wait_xfer_done_timeout(i2c_index, 1000)) { // 1000ms
        VIDEO_LOG("[%s:%d] I2C transfer is timeout!!", __func__, __LINE__);
        return -1;
    }

    return 0;
}

/**
  * @brief  Camera reads single data.
  * @param  Addr: I2C address
  * @param  Reg: Register address
  * @retval Read data number
  */
static int csk_i2c_read(uint8_t i2c_index, uint16_t addr, uint8_t reg, uint8_t *data, uint16_t num)
{
    int32_t ret = 0;

    if (i2c_index > 1) {
        VIDEO_LOG("[%s:%d] i2c_index=%d error, must be 0/1", __func__, __LINE__, i2c_index);
        return -1;
    }

    if (I2C_Handle[i2c_index] == NULL) {
        VIDEO_LOG("[%s:%d] i2c_index=%d handle is NULL, not init", __func__, __LINE__, i2c_index);
        return -1;
    }

    CLR_XFER_DONE(i2c_index);

    ret = I2C_MasterTransmit(I2C_Handle[i2c_index], addr, &reg, 1, 1);
    if (CSK_DRIVER_OK != ret) {
        VIDEO_LOG("[%s:%d] I2C error ret=%d", __func__, __LINE__, ret);
        return -1;
    }
    if (!wait_xfer_done_timeout(i2c_index, 1000)) { // 1000ms
        VIDEO_LOG("[%s:%d] I2C transfer is timeout!!", __func__, __LINE__);
        return -1;
    }

    ret = I2C_MasterReceive(I2C_Handle[i2c_index], addr, data, num, 0);
    if (CSK_DRIVER_OK != ret) {
        VIDEO_LOG("[%s:%d] I2C error ret=%d", __func__, __LINE__, ret);
        return -1;
    }
    if (!wait_xfer_done_timeout(i2c_index, 1000)) { // 1000ms
        VIDEO_LOG("[%s:%d] I2C transfer is timeout!!", __func__, __LINE__);
        return -1;
    }

    return num;
}


/**
  * @brief  Camera reads single data.
  * @param  Addr: I2C address
  * @param  Reg: Register address
  * @retval Read data number
  */
static uint8_t csk_i2c_read_reg8(uint8_t i2c_index, uint16_t addr, uint8_t reg)
{
    uint8_t data;

    if(csk_i2c_read(i2c_index, addr, reg, &data, 1)) {
        return data;
    } else {
        return 0;
    }
}


/**
  * @brief  Camera reads single data.
  * @param  Addr: I2C address
  * @param  Reg: Register address
  * @retval Read data number
  */
uint8_t csk_i2c_read_reg16(uint8_t i2c_index, uint16_t addr, uint16_t reg)
{
    int32_t ret = 0;
    uint8_t data[2];

    if (i2c_index > 1) {
        VIDEO_LOG("[%s:%d] i2c_index=%d error, must be 0/1", __func__, __LINE__, i2c_index);
        return -1;
    }

    if (I2C_Handle[i2c_index] == NULL) {
        VIDEO_LOG("[%s:%d] i2c_index=%d handle is NULL, not init", __func__, __LINE__, i2c_index);
        return -1;
    }

    CLR_XFER_DONE(i2c_index);

    data[0] = (reg >> 8) & 0xff;
    data[1] = reg & 0xff;
    ret = I2C_MasterTransmit(I2C_Handle[i2c_index], addr, &data[0], 2, 1);
    if (CSK_DRIVER_OK != ret) {
        VIDEO_LOG("[%s:%d] I2C error ret=%d", __func__, __LINE__, ret);
        return -1;
    }
    if (!wait_xfer_done_timeout(i2c_index, 1000)) { // 1000ms
        VIDEO_LOG("[%s:%d] I2C transfer is timeout!!", __func__, __LINE__);
        return -1;
    }

    ret = I2C_MasterReceive(I2C_Handle[i2c_index], addr, &data[0], 1, 0);
    if (CSK_DRIVER_OK != ret) {
        VIDEO_LOG("[%s:%d] I2C error ret=%d", __func__, __LINE__, ret);
        return -1;
    }
    if (!wait_xfer_done_timeout(i2c_index, 1000)) { // 1000ms
        VIDEO_LOG("[%s:%d] I2C transfer is timeout!!", __func__, __LINE__);
        return -1;
    }

    return data[0];
}


/**************************************** sensor ***********************************************************/
/********************************************************************************************************/
/********************************************************************************************************/
/********************************************************************************************************/

static void ov5640_init(uint8_t colorbar_enable)
{
    uint16_t pid = 0;
    uint8_t i2c_index = 0;
    uint8_t i2c_addr = 0x3c;  // 7bit

    csk_i2c_init(i2c_index);
    DELAY_MS(10);

    /* chip id */
    pid = csk_i2c_read_reg16(i2c_index, i2c_addr, 0x300A);
    pid = (pid << 8) | csk_i2c_read_reg16(i2c_index, i2c_addr, 0x300B);
    if(pid != 0x5640) {
        VIDEO_LOG("[%s:%d] not detect OV5640, read=0x%x", __func__, __LINE__, pid);
        return;
    } else {
        VIDEO_LOG("[%s:%d] detect OV5640", __func__, __LINE__);
    }

    /* sw reset */
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3008, 0x82);
    DELAY_MS(100);
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3103, 0x11);
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3008, 0x82);
    DELAY_MS(100);

    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3008, 0x42 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3103, 0x3  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3017, 0xff );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3018, 0xff );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3034, 0x18 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3035, 0x21 );
//    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3036, 0x20 );// 17fps
//    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3036, 0x48 );// 40fps
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3036, 0x60 ); // 50fps
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3037, 0x13 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3108, 0x1  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3824, 0x1  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3630, 0x36 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3631, 0xe  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3632, 0xe2 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3633, 0x12 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3621, 0xe0 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3704, 0xa0 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3703, 0x5a );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3715, 0x78 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3717, 0x1  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x370b, 0x60 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3705, 0x1a );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3905, 0x2  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3906, 0x10 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3901, 0xa  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3731, 0x12 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3600, 0x8  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3601, 0x33 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3620, 0x52 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x371b, 0x20 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x471c, 0x50 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a13, 0x43 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a18, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a19, 0x88 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3635, 0x13 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3636, 0x3  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3634, 0x40 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3622, 0x1  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3c01, 0x34 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3c04, 0x28 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3c05, 0x98 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3c06, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3c07, 0x8  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3c08, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3c09, 0x1c );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3c0a, 0x9c );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3c0b, 0x40 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3814, 0x31 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3815, 0x31 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3800, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3801, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3802, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3803, 0x4  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3804, 0xa  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3805, 0x3f );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3806, 0x7  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3807, 0x9b );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3808, 0x2  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3809, 0x80 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380a, 0x1  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380b, 0xe0 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380c, 0x7  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380d, 0x68 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380e, 0x3  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380f, 0xd8 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3810, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3811, 0x10 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3812, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3813, 0x6  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3618, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3612, 0x29 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3708, 0x64 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3709, 0x52 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x370c, 0x3  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a00, 0x78 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a02, 0x3  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a03, 0xd8 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a08, 0x1  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a09, 0x27 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a0a, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a0b, 0xf6 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a0e, 0x3  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a0d, 0x4  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a14, 0x3  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a15, 0xd8 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4001, 0x2  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4004, 0x2  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3000, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3002, 0x1c );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3004, 0xff );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3006, 0xc3 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x300e, 0x58 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x302c, 0x42 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4300, 0x30 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x501f, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4713, 0x3  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4407, 0x4  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x440e, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x460b, 0x35 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x460c, 0x20 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4837, 0x22 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5000, 0xa7 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5001, 0xa3 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3406, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5180, 0xff );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5181, 0xf2 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5182, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5183, 0x14 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5184, 0x25 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5185, 0x24 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5186, 0x16 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5187, 0x16 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5188, 0x16 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5189, 0x6e );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x518a, 0x68 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x518b, 0xe0 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x518c, 0xb2 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x518d, 0x42 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x518e, 0x3e );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x518f, 0x4c );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5190, 0x56 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5191, 0xf8 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5192, 0x4  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5193, 0x70 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5194, 0xf0 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5195, 0xf0 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5196, 0x3  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5197, 0x1  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5198, 0x4  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5199, 0x12 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x519a, 0x4  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x519b, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x519c, 0x6  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x519d, 0x82 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x519e, 0x38 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5381, 0x1e );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5382, 0x5b );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5383, 0x14 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5384, 0x5  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5385, 0x77 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5386, 0x7c );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5387, 0x72 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5388, 0x58 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5389, 0x1a );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x538a, 0x1  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x538b, 0x98 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5300, 0x8  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5301, 0x30 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5302, 0x30 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5303, 0x10 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5308, 0x25 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5304, 0x8  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5305, 0x30 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5306, 0x1c );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5307, 0x2c );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5309, 0x8  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x530a, 0x30 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x530b, 0x4  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x530c, 0x6  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5480, 0x1  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5481, 0x6  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5482, 0x12 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5483, 0x1e );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5484, 0x4a );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5485, 0x58 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5486, 0x65 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5487, 0x72 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5488, 0x7d );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5489, 0x88 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x548a, 0x92 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x548b, 0xa3 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x548c, 0xb2 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x548d, 0xc8 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x548e, 0xdd );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x548f, 0xf0 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5490, 0x15 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5580, 0x6  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5583, 0x40 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5584, 0x10 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5589, 0x10 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x558a, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x558b, 0xf8 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x501d, 0x40 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5800, 0x15 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5801, 0x10 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5802, 0xd  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5803, 0xd  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5804, 0xf  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5805, 0x15 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5806, 0xa  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5807, 0x7  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5808, 0x5  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5809, 0x5  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x580a, 0x7  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x580b, 0xb  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x580c, 0x7  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x580d, 0x3  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x580e, 0x1  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x580f, 0x1  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5810, 0x3  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5811, 0x7  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5812, 0x7  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5813, 0x3  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5814, 0x1  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5815, 0x1  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5816, 0x3  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5817, 0x6  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5818, 0xd  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5819, 0x8  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x581a, 0x6  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x581b, 0x6  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x581c, 0x7  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x581d, 0xb  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x581e, 0x14 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x581f, 0x13 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5820, 0xe  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5821, 0xe  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5822, 0x12 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5823, 0x12 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5824, 0x46 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5825, 0x26 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5826, 0x6  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5827, 0x46 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5828, 0x44 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5829, 0x26 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x582a, 0x24 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x582b, 0x42 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x582c, 0x24 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x582d, 0x46 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x582e, 0x24 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x582f, 0x42 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5830, 0x60 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5831, 0x42 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5832, 0x24 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5833, 0x26 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5834, 0x24 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5835, 0x24 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5836, 0x24 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5837, 0x46 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5838, 0x44 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5839, 0x46 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x583a, 0x26 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x583b, 0x48 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x583c, 0x44 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x583d, 0xbf );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a0f, 0x30 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a10, 0x28 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a1b, 0x30 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a1e, 0x26 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a11, 0x60 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a1f, 0x14 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3035, 0x21 );
//    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3036, 0x20 );
//    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3036, 0x48 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3036, 0x60 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3820, 0x46 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3821, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3034, 0x1a );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3037, 0x13 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3108, 0x1  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3824, 0x1  );
    DELAY_MS(10);

    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3808, 0x7  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3809, 0x80 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380a, 0x4  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380b, 0x40 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380c, 0x9  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380d, 0xc4 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380e, 0x4  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380f, 0x60 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a08, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a09, 0x54 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a0a, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a0b, 0x46 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a0e, 0xd  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a0d, 0x10 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3503, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x350c, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x350d, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3c07, 0x7  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3814, 0x11 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3815, 0x11 );

#if 0  // 1920x1080
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3800, 0x01 ); //x address start high byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3801, 0x50 ); //x address start low byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3802, 0x01 ); //y address start high byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3803, 0xb2 ); //y address start low byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3804, 0x08 ); //x address end high byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3805, 0xef ); //x address end low byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3806, 0x05 ); //y address end high byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3807, 0xf9 ); //y address end low byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3810, 0x00 ); //isp hortizontal offset high byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3811, 0x10 ); //isp hortizontal offset low byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3812, 0x00 ); //isp vertical offset high byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3813, 0x04 ); //isp vertical offset low byte
#endif

#if 0  // 640x480
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3800, 0x0  ); //x address start high byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3801, 0x0  ); //x address start low byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3802, 0x0  ); //y address start high byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3803, 0x0  ); //y address start low byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3804, 0x2  ); //x address end high byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3805, 0x9f ); //x address end low byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3806, 0x1  ); //y address end high byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3807, 0xf7 ); //y address end low byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3810, 0x0  ); //isp hortizontal offset high byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3811, 0x10 ); //isp hortizontal offset low byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3812, 0x0  ); //isp vertical offset high byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3813, 0x4  ); //isp vertical offset low byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3808, 0x2  ); //H size MSB
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3809, 0x80 ); //H size LSB
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380a, 0x1  ); //V size MSB
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380b, 0xe3 ); //V size LSB
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380c, 0x5  ); //HTS MSB
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380d, 0x90 ); //HTS LSB
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380e, 0x2  ); //VTS MSB
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380f, 0x0  ); //VTS LSB
#endif

    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4002, 0x45 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4005, 0x18 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3618, 0x4  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3612, 0x2b );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3709, 0x12 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x370c, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a02, 0x4  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a03, 0x60 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a14, 0x4  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a15, 0x60 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4004, 0x6  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3002, 0x1c );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3006, 0xc3 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x460b, 0x37 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x460c, 0x20 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4837, 0x16 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5001, 0x83 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x302c, 0x42 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a18, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a19, 0x80 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x471b, 0x2  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x471d, 0x2  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4740, 0x21 );

#if 1  // 640x480
    /* 0x3035=0x21 0x3036=0xea : 50fps PCLK=95.5MHz */
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3034, 0x1a);
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3035, 0x21); // PLL
//    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3036, 0x20 );// 17fps
//    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3036, 0x48 );// 40fps
//    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3036, 0x60 ); // 50fps
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3036, 0xea); // 50fps
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3c07, 0x08); // light meter 1 threshold [7:0]
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3820, 0x47); // Sensor flip off, ISP flip on
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3821, 0x01); // Sensor mirror on, ISP mirror on, H binning on
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3814, 0x71); // X INC
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3815, 0x31); // Y INC
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3800, 0x02); // HS
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3801, 0x00); // HS
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3802, 0x00); // VS
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3803, 0x04); // VS
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3804, 0x0a); // HW (HE)
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3805, 0x3f); // HW (HE)
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3806, 0x07); // VH (VE)
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3807, 0x9b); // VH (VE)
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3808, 0x02); // DVPHO
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3809, 0x80); // DVPHO
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380a, 0x01); // DVPVO
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380b, 0xe0); // DVPVO
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380c, 0x07); // HTS
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380d, 0x68); // HTS
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380e, 0x03); // VTS
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x380f, 0xd8); // VTS
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3813, 0x06); // Timing Voffset

    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3618, 0x00);
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3612, 0x29);
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3709, 0x52);
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x370c, 0x03);
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a02, 0x0b); // 60Hz max exposure, night mode 5fps
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a03, 0x88); // 60Hz max exposure
    // banding filters are calculated automatically in camera driver
    //csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a08, 0x01);	// B50 step
    //csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a09, 0x27);	// B50 step
    //csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a0a, 0x00);	// B60 step
    //csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a0b, 0xf6);	// B60 step
    //csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a0e, 0x03);	// 50Hz max band
    //csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a0d, 0x04);	// 60Hz max band

    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a14, 0x0b); // 50Hz max exposure, night mode 5fps
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3a15, 0x88); // 50Hz max exposure
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4004, 0x02); // BLC 2 lines
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3002, 0x1c); // reset JFIFO, SFIFO, JPEG
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3006, 0xc3); // disable clock of JPEG2x, JPEG
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4713, 0x03); // JPEG mode 3
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4407, 0x04); // Quantization scale
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x460b, 0x35);
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x460c, 0x22);
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4837, 0x22); // DVP CLK divider
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3824, 0x02); // DVP CLK divider
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5001, 0xa3); // SDE on, scale on, UV average off, color matrix on, AWB on
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3503, 0x00); // AEC/AGC on

    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3500, 0x00); // Exposure high byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3501, 0x10); // Exposure mid byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3502, 0x00); // Exposure low byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3503, 0x03); // bit0:AEC on   bit1:AGC on    =0: Auto enable  =1: Manual enable
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x350A, 0x01); // Gain high byte
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x350B, 0x00); // Gain low byte
#endif

    /* test pattern */
    if (colorbar_enable) {
        csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4741, 0x07);
    }

    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5025, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x503d, 0x0  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x3008, 0x2  );
    DELAY_MS(10);

    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4006, 0x2c );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4046, 0x28 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x801e, 0x80 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x801e, 0xc0 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x601e, 0x60 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0xa01e, 0x40 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x401e, 0xa0 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x601d, 0x3  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x4f48, 0x4b );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x010b, 0x98 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x0360, 0x8  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x585c, 0x4b );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x9801, 0x1d );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x0a03, 0x60 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x5664, 0xe  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x1d98, 0x60 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x6c0b, 0x77 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x1060, 0x1  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x601d, 0x3  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x8478, 0x7d );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x0112, 0x98 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x0360, 0xd  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x8a91, 0x76 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x9801, 0x1d );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x0e03, 0x90 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x8096, 0x16 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x1d98, 0x60 );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x9c10, 0xac );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x178b, 0x1  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x601d, 0x3  );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0xb9a8, 0xaf );
    csk_i2c_write_reg16(i2c_index, i2c_addr, 0x0119, 0x98 );
    DELAY_MS(100);
}


static void gc0328_init(uint8_t colorbar_enable)
{
    uint8_t data = 0;
    uint8_t i2c_index = 0;

    csk_i2c_init(i2c_index);
    DELAY_MS(10);

    /* chip id */
    data = csk_i2c_read_reg8(i2c_index, 0x21, 0xf0);
    if(data != 0x9D) {
        VIDEO_LOG("[%s:%d] not detect gc0328, read=0x%x", __func__, __LINE__, data);
    } else {
        VIDEO_LOG("[%s:%d] detect gc0328", __func__, __LINE__);
    }

    /* sw reset */
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0xf0);
    DELAY_MS(100);

#ifndef DPU_VERSION
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x80);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x80);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfc, 0x16);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfc, 0x16);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfc, 0x16);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfc, 0x16);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4f, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x42, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x3 ,0x0  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4 ,0xc0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x77, 0x62);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x78, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x79, 0x4d);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x5 ,0x1  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x6 ,0x32 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x7 ,0x0  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x8 ,0xc  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x29, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x2a, 0x78);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x2b, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x2c, 0xe0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x2d, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x2e, 0xe0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x2f, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x30, 0xe0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x31, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x32, 0xe0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4f, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4c, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x51, 0x80);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x52, 0x12);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x53, 0x80);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x54, 0x60);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x55, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x56, 0x6 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x5b, 0x2 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x61, 0xdc);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x62, 0xdc);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x7c, 0x71);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x7d, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x76, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x79, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x7b, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x70, 0xff);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x71, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x72, 0x10);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x73, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x74, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x50, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4f, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4c, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4f, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4f, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4f, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4d, 0x36);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4e, 0x2 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4e, 0x2 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4d, 0x44);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4e, 0x2 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4e, 0x2 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4e, 0x2 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4e, 0x2 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4d, 0x53);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4e, 0x8 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4e, 0x8 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4e, 0x2 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4d, 0x63);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4e, 0x8 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4e, 0x8 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4d, 0x73);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4e, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4d, 0x83);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4e, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4f, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x50, 0x88);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x27, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x2a, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x2b, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x2c, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x2d, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x9 ,0x0  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xa ,0x0  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb ,0x0  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc ,0x0  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xd ,0x1  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xe ,0xe8 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xf ,0x2  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x10, 0x88);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x16, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x17, 0x14);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x18, 0xe );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x19, 0x6 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x1b, 0x48);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x1f, 0xc8);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x20, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x21, 0x78);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x22, 0xb0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x23, 0x4 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x24, 0x3f);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x26, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x50, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x70, 0x85);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x40, 0x7f);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x41, 0x26);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x42, 0xff);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x45, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x44, 0x6 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x46, 0x2 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4b, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x50, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x7e, 0xa );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x7f, 0x3 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x80, 0x27);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x81, 0x15);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x82, 0x90);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x83, 0x2 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x84, 0x23);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x90, 0x2c);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x92, 0x2 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x94, 0x2 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x95, 0x35);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xd1, 0x32);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xd2, 0x32);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xdd, 0x18);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xde, 0x32);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xe4, 0x88);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xe5, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xd7, 0xe );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xbf, 0x10);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc0, 0x1c);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc1, 0x33);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc2, 0x48);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc3, 0x5a);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc4, 0x6b);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc5, 0x7b);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc6, 0x95);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc7, 0xab);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc8, 0xbf);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc9, 0xcd);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xca, 0xd9);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xcb, 0xe3);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xcc, 0xeb);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xcd, 0xf7);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xce, 0xfd);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xcf, 0xff);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x63, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x64, 0x5 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x65, 0xc );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x66, 0x1a);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x67, 0x29);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x68, 0x39);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x69, 0x4b);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x6a, 0x5e);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x6b, 0x82);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x6c, 0xa4);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x6d, 0xc5);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x6e, 0xe5);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x6f, 0xff);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x18, 0x2 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x98, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x9b, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x9c, 0x80);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xa4, 0x10);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xa8, 0xb0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xaa, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xa2, 0x23);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xad, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x9c, 0x2 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x8 ,0xa0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x9 ,0xe8 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x10, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x11, 0x11);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x12, 0x10);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x13, 0x80);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x15, 0xfc);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x18, 0x3 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x21, 0xc0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x22, 0x60);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x23, 0x30);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x25, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x24, 0x14);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc0, 0x10);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc1, 0xc );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc2, 0xa );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc6, 0xe );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc7, 0xb );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc8, 0xa );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xba, 0x26);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xbb, 0x1c);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xbc, 0x1d);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb4, 0x23);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb5, 0x1c);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb6, 0x1a);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc3, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc4, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc5, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc9, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xca, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xcb, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xbd, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xbe, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xbf, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb7, 0x7 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb8, 0x5 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb9, 0x5 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xa8, 0x7 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xa9, 0x6 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xaa, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xab, 0x4 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xac, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xad, 0x2 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xae, 0xd );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xaf, 0x5 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb0, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb1, 0x7 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb2, 0x3 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb3, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xa4, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xa5, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xa6, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xa7, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xa1, 0x3c);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xa2, 0x50);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb1, 0x4 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb2, 0xfd);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb3, 0xfc);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb4, 0xf0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb5, 0x5 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb6, 0xf0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x27, 0xf7);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x28, 0x7f);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x29, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x33, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x34, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x35, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x36, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x32, 0x8 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x47, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x48, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x79, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x7d, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x50, 0x88);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x5b, 0xc );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x76, 0x8f);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x80, 0x70);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x81, 0x70);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x82, 0xb0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x70, 0xff);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x71, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x72, 0x28);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x73, 0xb );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x74, 0xb );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x70, 0x45);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4f, 0x1 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xf1, 0x7 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xf2, 0x1 );
    DELAY_MS(100);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0);


#if SENSOR_HIGH_FRAMERATE
    /* Setting frame size to 320x240 */
    csk_i2c_write_reg8(i2c_index, 0x21, 0x70, 0xFF);  // global_gain
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x5 ,0x0  );  // HB high  // 0x01
    csk_i2c_write_reg8(i2c_index, 0x21, 0x6 ,0x80 );  // HB low  // 0x32
    csk_i2c_write_reg8(i2c_index, 0x21, 0x7 ,0x0  );  // VB  // 0x00
    csk_i2c_write_reg8(i2c_index, 0x21, 0x8 ,0xc  );  // VB  // 0x0c
    csk_i2c_write_reg8(i2c_index, 0x21, 0x9 ,0x0  );  // Row start  // 0x00
    csk_i2c_write_reg8(i2c_index, 0x21, 0xa ,0x0  );  // Row start  // 0x00
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb ,0x0  );  // Col start  // 0x00
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc ,0x0  );  // Col start  // 0x00
    csk_i2c_write_reg8(i2c_index, 0x21, 0xd ,0x1  );  // Window height  // 0x01
    csk_i2c_write_reg8(i2c_index, 0x21, 0xe ,0x00 );  // Window height  // 0xe8
    csk_i2c_write_reg8(i2c_index, 0x21, 0xf ,0x1  );  // Window width  // 0x02
    csk_i2c_write_reg8(i2c_index, 0x21, 0x10,0x60);   // Window width  // 0x88

    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x51, 0x0);  // Crop _win_y1
    csk_i2c_write_reg8(i2c_index, 0x21, 0x52, 0x0);  // Crop _win_y1  // 0x78
    csk_i2c_write_reg8(i2c_index, 0x21, 0x53, 0x0);  // Crop _win_x1
    csk_i2c_write_reg8(i2c_index, 0x21, 0x54, 0x0);  // Crop _win_x1  // 0xa0
    csk_i2c_write_reg8(i2c_index, 0x21, 0x55, 0x0);  // Crop_win_height
    csk_i2c_write_reg8(i2c_index, 0x21, 0x56, 0xf0); // Crop_win_height
    csk_i2c_write_reg8(i2c_index, 0x21, 0x57, 0x1);  // Crop_win_width
    csk_i2c_write_reg8(i2c_index, 0x21, 0x58, 0x40); // Crop_win_width
#else
    /* Setting frame size to 640x480 */
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x5 ,0x1  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x6 ,0x32 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x7 ,0x0  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x8 ,0xc  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x9 ,0x0  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xa ,0x0  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xb ,0x0  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc ,0x0  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xd ,0x1  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xe ,0x18 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0xf ,0x2  );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x10, 0x88);

    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x51, 0x0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x52, 0x0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x53, 0x0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x54, 0x0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x55, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x56, 0x10);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x57, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x58, 0x80);
#endif    

    /* Set pixel format */
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x44, 0x2);   // 0x00:YUV422-CbYCrY 0x01:CrYCbY 0x02:YCbYCr 0x03:YCrYCb  0x06:RGB565

    /* colorbar 0x4C bit0 */
    if(colorbar_enable) {
        csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x0);
        csk_i2c_write_reg8(i2c_index, 0x21, 0x4c, 0x1);
    }
#else
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x80);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x80);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfc, 0x16);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfc, 0x16);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfc, 0x16);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfc, 0x16);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xF1, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xF2, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xfe, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4f, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x42, 0x0 );
    csk_i2c_write_reg8(i2c_index, 0x21, 0x3, 0x0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4, 0xc0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x77, 0x5A);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x78, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x79, 0x56);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x0D, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x0E, 0xE8);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x0F, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x10, 0x88);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x09, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x0A, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x0B, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x0C, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x16, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x17, 0x14);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x18, 0x0E);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x19, 0x06);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x1B, 0x48);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x1F, 0xC8);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x20, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x21, 0x78);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x22, 0xB0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x23, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x24, 0x11);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x26, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x59, 0x22);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x5b, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x5c, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x5d, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x5e, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x5f, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x60, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x61, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x62, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x5a, 0x0e);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x50, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x51, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x52, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x53, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x54, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x55, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x56, 0xf0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x57, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x58, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x70, 0x45);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x05, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x06, 0xDE);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x07, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x08, 0x24);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x29, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x2A, 0x83);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x2B, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x2C, 0x0C);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x2D, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x2E, 0x0C);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x2F, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x30, 0x0C);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x31, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x32, 0x0C);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x50, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4F, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4C, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4F, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4F, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4F, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4F, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4F, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4D, 0x30);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4D, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4D, 0x50);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4D, 0x60);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4D, 0x70);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4F, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x50, 0x88);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x27, 0xB7);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x28, 0x7F);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x29, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x33, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x34, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x35, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x36, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x32, 0x08);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x3B, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x3C, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x3D, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x3E, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x47, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x48, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x40, 0x7F);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x41, 0x26);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x42, 0xFF);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x44, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x45, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x46, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4F, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4B, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x50, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x7E, 0x0A);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x7F, 0x03);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x81, 0x15);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x82, 0x90);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x83, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x84, 0xE5);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x90, 0x2C);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x92, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x94, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x95, 0x22);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xd1, 0x32);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xd2, 0x32);
     
    csk_i2c_write_reg8(i2c_index, 0x21, 0xd3, 0x38);
     
    csk_i2c_write_reg8(i2c_index, 0x21, 0xdd, 0x78);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xDE, 0x32);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xE4, 0x88);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xE5, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xD7, 0x0E);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x00);
     
    csk_i2c_write_reg8(i2c_index, 0x21, 0xbf, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc0, 0x30);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc1, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc2, 0x50);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc3, 0x5a);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc4, 0x6b);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc5, 0x7b);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc6, 0x95);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc7, 0xab);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc8, 0xbf);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xc9, 0xcd);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xca, 0xd9);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xcb, 0xe3);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xcc, 0xeb);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xcd, 0xf7);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xCE, 0xFD);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xCF, 0xFF);
     
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x63, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x64, 0x05);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x65, 0x0c);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x66, 0x1a);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x67, 0x29);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x68, 0x39);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x69, 0x4b);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x6a, 0x5e);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x6b, 0x82);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x6c, 0xa4);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x6d, 0xc5);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x6e, 0xe5);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x6f, 0xFF);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x18, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x97, 0x30);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x98, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x9B, 0x60);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x9C, 0x60);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xA4, 0x50);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xA8, 0x80);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xAA, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xA2, 0x23);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xAD, 0x28);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x9C, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x9E, 0xC0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x9F, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x08, 0xA0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x09, 0xE8);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x10, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x11, 0x11);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x12, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x13, 0x90);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x15, 0xfc);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x18, 0x03);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x21, 0xf0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x22, 0x80);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x23, 0x30);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x25, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x24, 0x14);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x3D, 0x80);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x3E, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x51, 0x88);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x52, 0x12);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x53, 0x80);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x54, 0x60);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x55, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x56, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x58, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x5B, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x5E, 0xA4);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x5F, 0x8A);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x61, 0xDC);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x62, 0xDC);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x70, 0xFC);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x71, 0x10);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x72, 0x30);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x73, 0x0B);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x74, 0x0B);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x75, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x76, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x77, 0x40);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x78, 0x70);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x79, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x7B, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x7C, 0x71);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x7D, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x80, 0x70);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x81, 0x58);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x82, 0x98);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x83, 0x60);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x84, 0x58);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x85, 0x50);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xC0, 0x10);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xC1, 0x0C);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xC2, 0x0A);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xC6, 0x0E);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xC7, 0x0B);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xC8, 0x0A);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xBA, 0x26);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xBB, 0x1C);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xBC, 0x1D);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xB4, 0x23);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xB5, 0x1C);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xB6, 0x1A);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xC3, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xC4, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xC5, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xC9, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xCA, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xCB, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xBD, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xBE, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xBF, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xB7, 0x07);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xB8, 0x05);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xB9, 0x05);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xA8, 0x07);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xA9, 0x06);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xAA, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xAB, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xAC, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xAD, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xAE, 0x0D);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xAF, 0x05);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xB0, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xB1, 0x07);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xB2, 0x03);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xB3, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xA4, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xA5, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xA6, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xA7, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xA1, 0x3C);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xA2, 0x50);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xB1, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xB2, 0xfd);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xB3, 0xfc);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xB4, 0xf0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xB5, 0x05);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xB6, 0xf0);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x50, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4F, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4C, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4F, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4F, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4F, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4D, 0x34);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x02);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4D, 0x44);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4D, 0x53);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4D, 0x65);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x04);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4D, 0x73);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4D, 0x83);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4E, 0x20);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x4F, 0x01);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x50, 0x88);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xFE, 0x00);
    csk_i2c_write_reg8(i2c_index, 0x21, 0x58, 0x82);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xF1, 0x07);
    csk_i2c_write_reg8(i2c_index, 0x21, 0xF2, 0x01);


#endif
}

