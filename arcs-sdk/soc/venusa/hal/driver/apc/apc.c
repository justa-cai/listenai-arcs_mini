/*
 * apc.c
 *
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "Driver_Common.h"
#include "cache.h"
#include "apc_inner.h"

#include "log_print.h"

#define DEBUG_LOG   1 //0 //
#if DEBUG_LOG
//#define LOGD(format, ...)   printf(format, ##__VA_ARGS__)
#define LOGD(format, ...)   CLOGD(format, ##__VA_ARGS__)
#else
#define LOGD(format, ...)   ((void)0)
#endif // DEBUG_LOG

//=============================================================================
//include only referenced functions, initialized and uninitialized data located in fast local memory
//FIXME: redefine following macros after sections in .ld is determined!!
/*
#undef _FAST_FUNC_RO
#define _FAST_FUNC_RO           _FAST_FUNC_RO_UNI(apc, __LINE__)

#undef _FAST_DATA_VI
#define _FAST_DATA_VI           _FAST_DATA_VI_UNI(apc, __LINE__)

#undef _FAST_DATA_ZI
#define _FAST_DATA_ZI           _FAST_DATA_ZI_UNI(apc, __LINE__)

#undef _DMA
#define _DMA                    _FAST_DATA_ZI
*/

//=============================================================================
_FAST_DATA_VI static const APC_CH_FIXED g_chs_fixed[APC_CH_COUNT] = {
    // APC RX (IN) channel
    { 0, 1, 0, 0, CMNDMA_HSID_APC_IN0_L, CMNDMA_HSSEL_APC, GPDMA2D_HSID_APC_IN0_L, GPDMA2D_HSSEL_APC, APC_IN_FIFO_DEPTH_DEF, APC_RX_CH0L_DATA_ADDR },
    { 1, 1, 16, 5, CMNDMA_HSID_APC_IN0_R, CMNDMA_HSSEL_APC, GPDMA2D_HSID_APC_IN0_R, GPDMA2D_HSSEL_APC, APC_IN_FIFO_DEPTH_DEF, APC_RX_CH0R_DATA_ADDR },
    { 2, 1, 0, 0, CMNDMA_HSID_APC_IN1_LR, CMNDMA_HSSEL_APC, GPDMA2D_HSID_APC_IN1_LR, GPDMA2D_HSSEL_APC, APC_IN_FIFO_DEPTH_DEF, APC_RX_CH1L_DATA_ADDR },
    { 3, 1, 16, 5, CMNDMA_HSID_APC_IN1_LR, CMNDMA_HSSEL_APC, GPDMA2D_HSID_APC_IN1_LR, GPDMA2D_HSSEL_APC, APC_IN_FIFO_DEPTH_DEF, APC_RX_CH1R_DATA_ADDR },
    { 4, 1, 0, 0, CMNDMA_HSID_APC_IN2_LR, CMNDMA_HSSEL_APC, GPDMA2D_HSID_APC_IN2_LR, GPDMA2D_HSSEL_APC, APC_IN_FIFO_DEPTH_DEF, APC_RX_CH2L_DATA_ADDR },
    { 5, 1, 16, 5, CMNDMA_HSID_APC_IN2_LR, CMNDMA_HSSEL_APC, GPDMA2D_HSID_APC_IN2_LR, GPDMA2D_HSSEL_APC, APC_IN_FIFO_DEPTH_DEF, APC_RX_CH2R_DATA_ADDR },
    // APC TX (OUT) channel
    { 6, 0, 0, 0, CMNDMA_HSID_APC_OUT0_L, CMNDMA_HSSEL_APC, GPDMA2D_HSID_APC_OUT0_L, GPDMA2D_HSSEL_APC, APC_OUT_FIFO_DEPTH_DEF, APC_TX_CH0L_DATA_ADDR },
    { 7, 0, 16, 5, 0, 0, 0, 0, APC_OUT_FIFO_DEPTH_DEF, 0xFFFFFFFF }, // NOT EXISTING!!
    { 8, 0, 0, 0, CMNDMA_HSID_APC_OUT1_LR, CMNDMA_HSSEL_APC, GPDMA2D_HSID_APC_OUT1_LR, GPDMA2D_HSSEL_APC, APC_OUT_FIFO_DEPTH_DEF, APC_TX_CH1L_DATA_ADDR },
    { 9, 0, 16, 5, CMNDMA_HSID_APC_OUT1_LR, CMNDMA_HSSEL_APC, GPDMA2D_HSID_APC_OUT1_LR, GPDMA2D_HSSEL_APC, APC_OUT_FIFO_DEPTH_DEF, APC_TX_CH1R_DATA_ADDR },
    { 10, 0, 0, 0, CMNDMA_HSID_APC_OUT2_LR, CMNDMA_HSSEL_APC, GPDMA2D_HSID_APC_OUT2_LR, GPDMA2D_HSSEL_APC, APC_OUT_FIFO_DEPTH_DEF, APC_TX_CH2L_DATA_ADDR },
    { 11, 0, 16, 5, CMNDMA_HSID_APC_OUT2_LR, CMNDMA_HSSEL_APC, GPDMA2D_HSID_APC_OUT2_LR, GPDMA2D_HSSEL_APC, APC_OUT_FIFO_DEPTH_DEF, APC_TX_CH2R_DATA_ADDR },
};

/*
_FAST_DATA_VI static const APC_DCH_FIXED g_dchs_fixed[APC_DCH_COUNT] = {
        // APC RX (IN) dual_channel
        { 25, 0 },
        { 25, 0 },
        // APC TX (OUT) dual_channel
        { 25, 0 },
        { 25, 0 },
};
*/

_FAST_DATA_VI static const uint32_t g_dch_cfg_regs[APC_DCH_COUNT] = {
        APC_RX_CH0_CFG_ADDR,
        APC_RX_CH1_CFG_ADDR,
        APC_RX_CH2_CFG_ADDR,
        APC_TX_CH0_CFG_ADDR,
        APC_TX_CH1_CFG_ADDR,
        APC_TX_CH2_CFG_ADDR,
};

_FAST_DATA_VI static const uint8_t g_dch_intr_offset[APC_DCH_COUNT] = {
        0x0C,
        0x10,
        0x14,
        0x00,
        0x04,
        0x08,
};

_FAST_DATA_ZI static APC_DEV g_apc_dev;

//#define IS_APC_CH_IN(ch)    (g_apc_dev.chs_info[ch].fixed->ch_dir)
//#define IS_APC_CH_OUT(ch)    !(g_apc_dev.chs_info[ch].fixed->ch_dir)

static volatile int32_t g_init_cnt = 0U;
volatile CSK_APC_RegDef * gApcReg = NULL;

#define IS_APC_INITIALIZED()        (g_init_cnt > 0)
#define IS_APC_DCH_INITIALIZED(dch)   (g_apc_dev.dch_array[dch].intf_type != APC_INTF_UNSET && \
                                        g_apc_dev.dch_array[dch].setup_done == 1)
#define IS_APC_DCH_OCCUPIED(dch)      (g_apc_dev.dch_array[dch].intf_type != APC_INTF_UNSET)
#define DMA_LEN_BYTES(samp_len, dma_width_bits)     ((samp_len) << (dma_width_bits))

#if USE_GPDMA2D
static void apc_gpdma2d_event(uint32_t event, void* workspace);
#endif

#if USE_CMNDMA
static void apc_cmndma_event(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param);
#endif

static void apc_irq_handler (void);

//------------------------------------------------------------------------------
#if USE_CMNDMA

// LLP (linked-list) is NOT implemented on GPDMAC NOW!!
// GPDMAC supports 2 blocks PingPong mode only!!
#define MAX_DMA_BUF_CNT     2
#define MAX_COCUR_CNT       (APC_DCH_IN_COUNT - 1) // 1

// For apc_read_quad_channels_LLP & apc_read_six_channels_LLP
// At most 2 AUDIO_BUFFER_LLI array for 2 IN Dual_CHannels, one DCH use original array,
// the other DCH may use AUDIO_BUFFER_LLI items allocated from g_lli_pool!
_FAST_DATA_ZI static AUDIO_BUFFER_LLI g_lli_pool[MAX_DMA_BUF_CNT * MAX_COCUR_CNT];

#endif //USE_CMNDMA
//=============================================================================

#define APC_DCH_R_EN           (0x1 << 16)
#define APC_DCH_L_EN           (0x1 << 0)

#define APC_DCH_R_FLU          (0x1 << 19)
#define APC_DCH_L_FLU          (0x1 << 3)

static void GET_APC_DCH_FIFO_CNT(uint8_t dch, uint8_t *lcnt_p, uint8_t *rcnt_p)
{
    uint32_t val = inw(g_dch_cfg_regs[dch]);
    uint8_t lch = dch << 1;
    uint8_t rch = lch + 1;
    if (lcnt_p != NULL)
        //*lcnt_p = (val >> (g_chs_fixed[lch].cfg_bits_pos + CFG_FIFO_CNT_POS)) & CFG_FIFO_CNT_MASK;
        *lcnt_p = (val >> (0 + CFG_FIFO_CNT_POS)) & CFG_FIFO_CNT_MASK;
    if (rcnt_p != NULL)
        //*rcnt_p = (val >> (g_chs_fixed[rch].cfg_bits_pos + CFG_FIFO_CNT_POS)) & CFG_FIFO_CNT_MASK;
        *rcnt_p = (val >> (16 + CFG_FIFO_CNT_POS)) & CFG_FIFO_CNT_MASK;
}

static inline uint8_t APC_CH_FIFO_CNT(uint8_t ch)
{
    uint8_t dch = ch >> 1;
    uint32_t val = inw(g_dch_cfg_regs[dch]);
    //return ((val >> (g_chs_fixed[ch].cfg_bits_pos + CFG_FIFO_CNT_POS)) & CFG_FIFO_CNT_MASK);
    if (ch & 0x1) // right channel
        return ((val >> (16 + CFG_FIFO_CNT_POS)) & CFG_FIFO_CNT_MASK);
    else // left channel
        return ((val >> CFG_FIFO_CNT_POS) & CFG_FIFO_CNT_MASK);
}


/*
#define APC_CH_INT_STATUS(rx_sta, tx_sta, ch)   \
    (((ch >= APC_CH_IN_COUNT ? tx_sta : rx_sta) >> g_chs_fixed[ch].intr_bits_pos) & APC_CH_INTR_MASK)
*/

// irq_apc0 is true, configure APC_INTR_AP_TX/RX_CHx_XXX, and APC IRQ is routed via apc_0
// irq_apc0 is false, configure APC_INTR_CP_TX/RX_CHx_XXX, and APC IRQ is routed via apc_1

// APC interrupt-related operations for APC channel
static inline void apc_ch_intr_enable(APC_CH ch, uint32_t intr_bits, bool irq_apc0)
{
    if (ch >= APC_CH_COUNT) return;
    intr_bits &= APC_FIFO_INTR_MASK;
    //intr_bits <<= g_chs_fixed[ch].intr_bits_pos;
    if (ch & 0x1)   intr_bits <<= 0x5;

    uint32_t reg_val, reg_addr;
    reg_addr = g_dch_intr_offset[APC_CH_TO_DCH(ch)] +
            (irq_apc0 ? APC_INTR_AP_MSK_BASE : APC_INTR_CP_MSK_BASE);
    reg_val = inw(reg_addr);
    outw(reg_addr, (reg_val & ~intr_bits));
}

static inline void apc_ch_intr_disable(APC_CH ch, uint32_t intr_bits, bool irq_apc0)
{
    if (ch >= APC_CH_COUNT) return;
    intr_bits &= APC_FIFO_INTR_MASK;
    //intr_bits <<= g_chs_fixed[ch].intr_bits_pos;
    if (ch & 0x1)   intr_bits <<= 0x5;

    uint32_t reg_val, reg_addr;
    reg_addr = g_dch_intr_offset[APC_CH_TO_DCH(ch)] +
            (irq_apc0 ? APC_INTR_AP_MSK_BASE : APC_INTR_CP_MSK_BASE);
    reg_val = inw(reg_addr);
    outw(reg_addr, (reg_val | intr_bits));
}

static inline void apc_ch_intr_clear(APC_CH ch, uint32_t intr_bits, bool irq_apc0)
{
    if (ch >= APC_CH_COUNT) return;
    intr_bits &= APC_FIFO_INTR_MASK;
    //intr_bits <<= g_chs_fixed[ch].intr_bits_pos;
    if (ch & 0x1)   intr_bits <<= 0x5;

    uint32_t reg_addr = g_dch_intr_offset[APC_CH_TO_DCH(ch)] +
            (irq_apc0 ? APC_INTR_AP_CLR_BASE : APC_INTR_CP_CLR_BASE);
    outw(reg_addr, intr_bits);
}

static inline uint32_t apc_ch_intr_status(APC_CH ch, bool irq_apc0)
{
    if (ch >= APC_CH_COUNT) return 0;
    uint32_t reg_val, reg_addr;
    reg_addr = g_dch_intr_offset[APC_CH_TO_DCH(ch)] +
            (irq_apc0 ? APC_INTR_AP_ISR_BASE : APC_INTR_CP_ISR_BASE);
    reg_val = inw(reg_addr);
    //return ((reg_val >> g_chs_fixed[ch].intr_bits_pos) & APC_FIFO_INTR_MASK);
    if (ch & 0x1)   reg_val >>= 0x5;
    return (reg_val & APC_FIFO_INTR_MASK);
}

static inline uint32_t apc_ch_intr_raw_status(APC_CH ch, bool irq_apc0)
{
    if (ch >= APC_CH_COUNT) return 0;
    uint32_t reg_val, reg_addr;
    reg_addr = g_dch_intr_offset[APC_CH_TO_DCH(ch)] +
            (irq_apc0 ? APC_INTR_AP_IRSR_BASE : APC_INTR_CP_IRSR_BASE);
    reg_val = inw(reg_addr);
    //return ((reg_val >> g_chs_fixed[ch].intr_bits_pos) & APC_FIFO_INTR_MASK);
    if (ch & 0x1)   reg_val >>= 0x5;
    return (reg_val & APC_FIFO_INTR_MASK);
}

// APC interrupt-related operations for APC dual_channel
static inline void apc_dch_intr_enable(APC_DCH dch, uint32_t intr_bits, bool irq_apc0)
{
    if (dch >= APC_DCH_COUNT) return;
    //uint8_t lch = dch << 1;
    //uint8_t rch = lch + 1;
    intr_bits &= APC_FIFO_INTR_MASK;
    //intr_bits = (intr_bits << g_chs_fixed[lch].intr_bits_pos) | (intr_bits << g_chs_fixed[rch].intr_bits_pos);
    intr_bits |= intr_bits << 5;

    uint32_t reg_val, reg_addr;
    reg_addr = g_dch_intr_offset[dch] +
            (irq_apc0 ? APC_INTR_AP_MSK_BASE : APC_INTR_CP_MSK_BASE);
    reg_val = inw(reg_addr);
    outw(reg_addr, (reg_val & ~intr_bits));
}

static inline void apc_dch_intr_disable(APC_DCH dch, uint32_t intr_bits, bool irq_apc0)
{
    if (dch >= APC_DCH_COUNT) return;
    //uint8_t lch = dch << 1;
    //uint8_t rch = lch + 1;
    intr_bits &= APC_FIFO_INTR_MASK;
    //intr_bits = (intr_bits << g_chs_fixed[lch].intr_bits_pos) | (intr_bits << g_chs_fixed[rch].intr_bits_pos);
    intr_bits |= intr_bits << 5;

    uint32_t reg_val, reg_addr;
    reg_addr = g_dch_intr_offset[dch] +
            (irq_apc0 ? APC_INTR_AP_MSK_BASE : APC_INTR_CP_MSK_BASE);
    reg_val = inw(reg_addr);
    outw(reg_addr, (reg_val | intr_bits));
}

static inline void apc_dch_intr_clear(APC_DCH dch, uint32_t intr_bits, bool irq_apc0)
{
    if (dch >= APC_DCH_COUNT) return;
    //uint8_t lch = dch << 1;
    //uint8_t rch = lch + 1;
    intr_bits &= APC_FIFO_INTR_MASK;
    //intr_bits = (intr_bits << g_chs_fixed[lch].intr_bits_pos) | (intr_bits << g_chs_fixed[rch].intr_bits_pos);
    intr_bits |= intr_bits << 5;

    uint32_t reg_addr = g_dch_intr_offset[dch] +
            (irq_apc0 ? APC_INTR_AP_CLR_BASE : APC_INTR_CP_CLR_BASE);
    outw(reg_addr, intr_bits);
}

// status of interrupts: Left channel @ bit[4:0], Right channel @ bit[9:5]
static inline uint32_t apc_dch_intr_status(APC_DCH dch, bool irq_apc0)
{
    if (dch >= APC_DCH_COUNT) return 0;
    //uint8_t lch = dch << 1;
    //uint8_t rch = lch + 1;

    uint32_t reg_val, reg_addr;
    reg_addr = g_dch_intr_offset[dch] +
            (irq_apc0 ? APC_INTR_AP_ISR_BASE : APC_INTR_CP_ISR_BASE);
    reg_val = inw(reg_addr);
    //reg_val &= (APC_FIFO_INTR_MASK << g_chs_fixed[lch].intr_bits_pos) | (APC_FIFO_INTR_MASK << g_chs_fixed[rch].intr_bits_pos);
    reg_val &= APC_FIFO_INTR_MASK | (APC_FIFO_INTR_MASK << 5);
    return reg_val;
}

// raw status of interrupts: Left channel @ bit[4:0], Right channel @ bit[9:5]
static inline uint32_t apc_dch_intr_raw_status(APC_DCH dch, bool irq_apc0)
{
    if (dch >= APC_DCH_COUNT) return 0;
    //uint8_t lch = dch << 1;
    //uint8_t rch = lch + 1;

    uint32_t reg_val, reg_addr;
    reg_addr = g_dch_intr_offset[dch] +
            (irq_apc0 ? APC_INTR_AP_IRSR_BASE : APC_INTR_CP_IRSR_BASE);
    reg_val = inw(reg_addr);
    //reg_val &= (APC_FIFO_INTR_MASK << g_chs_fixed[lch].intr_bits_pos) | (APC_FIFO_INTR_MASK << g_chs_fixed[rch].intr_bits_pos);
    reg_val &= APC_FIFO_INTR_MASK | (APC_FIFO_INTR_MASK << 5);
    return reg_val;
}

#if SUPPORT_I2S_ERR

// I2S error/timeout interrupt-related operations
void apc_i2s_err_intr_enable(uint8_t i2s_idx, bool irq_apc0)
{
    if (i2s_idx > APC_INTF_IDX_I2S1)
        return;

    uint32_t reg_val, reg_addr;
    //Clock Error may be reported by slave by mistaken!!
    //FIXME: Remove Clock Error Interrupt Enable?
    //uint32_t intr_bits = APC_INTR_I2S_ERR | APC_INTR_I2S_TIMEOUT;
    uint32_t intr_bits = APC_INTR_I2S_TIMEOUT;
    reg_addr = g_dch_intr_offset[APC_DCH_I2S0_IN + i2s_idx] +
            (irq_apc0 ? APC_INTR_AP_MSK_BASE : APC_INTR_CP_MSK_BASE);
    reg_val = inw(reg_addr);
    outw(reg_addr, (reg_val & ~intr_bits));
}

void apc_i2s_err_intr_disable(uint8_t i2s_idx, bool irq_apc0)
{
    if (i2s_idx > APC_INTF_IDX_I2S1)
        return;

    uint32_t reg_val, reg_addr;
    uint32_t intr_bits = APC_INTR_I2S_ERR | APC_INTR_I2S_TIMEOUT;
    reg_addr = g_dch_intr_offset[APC_DCH_I2S0_IN + i2s_idx] +
            (irq_apc0 ? APC_INTR_AP_MSK_BASE : APC_INTR_CP_MSK_BASE);
    reg_val = inw(reg_addr);
    outw(reg_addr, (reg_val | intr_bits));
}

static inline void apc_i2s_err_intr_clear(uint8_t i2s_idx, bool irq_apc0)
{
    if (i2s_idx > APC_INTF_IDX_I2S1)
        return;

    uint32_t intr_bits = APC_INTR_I2S_ERR | APC_INTR_I2S_TIMEOUT;
    uint32_t reg_addr = g_dch_intr_offset[APC_DCH_I2S0_IN + i2s_idx] +
            (irq_apc0 ?  APC_INTR_AP_CLR_BASE : APC_INTR_CP_CLR_BASE);
    outw(reg_addr, intr_bits);
}

static inline uint32_t apc_i2s_err_intr_status(uint8_t i2s_idx, bool irq_apc0)
{
    if (i2s_idx > APC_INTF_IDX_I2S1)
        return 0;

    uint32_t reg_val, reg_addr;
    uint32_t intr_bits = APC_INTR_I2S_ERR | APC_INTR_I2S_TIMEOUT | APC_INTR_I2S_ERR_LRCK;
    reg_addr = g_dch_intr_offset[APC_DCH_I2S0_IN + i2s_idx] +
            (irq_apc0 ? APC_INTR_AP_ISR_BASE : APC_INTR_CP_ISR_BASE);
    reg_val = inw(reg_addr) & intr_bits;
    return reg_val;
}

static inline uint32_t apc_i2s_err_intr_raw_status(uint8_t i2s_idx, bool irq_apc0)
{
    if (i2s_idx > APC_INTF_IDX_I2S1)
        return 0;

    uint32_t reg_val, reg_addr;
    uint32_t intr_bits = APC_INTR_I2S_ERR | APC_INTR_I2S_TIMEOUT;
    reg_addr = g_dch_intr_offset[APC_DCH_I2S0_IN + i2s_idx] +
            (irq_apc0 ? APC_INTR_AP_IRSR_BASE : APC_INTR_CP_IRSR_BASE);
    reg_val = inw(reg_addr) & intr_bits;
    return reg_val;
}

bool apc_i2s_timeout_set(uint8_t i2s_idx, uint32_t i2s_samp_rate)
{
    if (i2s_idx > APC_INTF_IDX_I2S1)
        return false;

    // calculate how many cycles of 2.4MHz LRCK-sampling-frequency in 1 LRCK cycle,
    // and use one and a half of above value to set TIMEOUT_CFG_VAL
    uint32_t timeout = APC_I2S_TIMEOUT_CFG_FREQ / i2s_samp_rate;
    timeout += timeout >> 1;
    if (timeout > APC_I2S_TIMEOUT_CFG_MAX)
        timeout = APC_I2S_TIMEOUT_CFG_MAX;

    outw(APC_I2S_TIMEOUT_CFG_ADDR(i2s_idx), timeout);
    return true;
}

#endif // SUPPORT_I2S_ERR

// set DMA_HS of some handshake ID of CMNDMA
static inline void set_cmndma_hs(uint8_t hsid, uint8_t hssel) {
    if (hssel == 0) {
        IP_SYSCTRL->REG_DMA_HS.all &= ~(0x1 << hsid);
    } else {
        //#define GPDMA2D_HSSEL_MASK  0x1 // 0x3
        //IP_SYSCTRL->REG_DMA_HS.all &= ~(GPDMA2D_HSSEL_MASK << hsid);
        //IP_SYSCTRL->REG_DMA_HS.all |= (hssel & GPDMA2D_HSSEL_MASK) << hsid;
        IP_SYSCTRL->REG_DMA_HS.all |= 0x1 << hsid;
    }
}

// set DMA_HS of some handshake ID of GPDMA2D
static inline void set_gpdma2d_hs(uint8_t hsid, uint8_t hssel) {
    if (hssel == 0) {
        IP_SYSCTRL->REG_DMA_HS.all &= ~(0x1 << (16 + hsid));
    } else {
        //#define GPDMA2D_HSSEL_MASK  0x1 // 0x3
        //IP_SYSCTRL->REG_DMA_HS.all &= ~(GPDMA2D_HSSEL_MASK << (16 + hsid));
        //IP_SYSCTRL->REG_DMA_HS.all |= (hssel & GPDMA2D_HSSEL_MASK) << (16 + hsid);
        IP_SYSCTRL->REG_DMA_HS.all |= 0x1 << (16 + hsid);
    }
}

static inline void apc_clk_enable() {
    IP_SYSCTRL->REG_PERI_CLK_CFG7.bit.ENA_APC_CLK = 1;
}

static inline void apc_clk_disable() {
    IP_SYSCTRL->REG_PERI_CLK_CFG7.bit.ENA_APC_CLK = 0;
}

static inline void apc_sw_reset() {
    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.APC_RESET = 1;
}

//-------------------------------------------------------------------------------

/**
  \fn          int32_t apc_initialize (void)
  \brief       Initialize Audio Processing Center
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
int32_t apc_initialize (void)
{
    uint32_t i;
    APC_DCH_INFO *pdci;

    // Check if already initialized
    gApcReg = CSK_APC;
    //g_init_cnt++;
    //if (g_init_cnt > 1U) { return CSK_DRIVER_OK; }
    if (__AMOADD_W(&g_init_cnt, 1) > 1)  { return CSK_DRIVER_OK; }

    // enable APC Clock & SW reset in all
    apc_clk_enable();
    apc_sw_reset();

    // Reset APC RX & TX (disable APC at first) in all
    //CSK_APC->REG_APC_CFG.all = APC_RX_PATH_RESET_BIT | APC_TX_PATH_RESET_BIT;
    CSK_APC->REG_APC_RX_PATH_RESET.all = APC_RX_PATH_RESET_BIT;
    CSK_APC->REG_APC_TX_PATH_RESET.all = APC_TX_PATH_RESET_BIT;

    memset(&g_apc_dev, 0, sizeof(APC_DEV));
    g_apc_dev.reg = CSK_APC;

    // initialize owned APC channels' information
    for (i=0; i<APC_DCH_COUNT && DCH_IS_OWNED(i); i++) {
        pdci = &g_apc_dev.dch_array[i];
        pdci->cfg_reg_p= (uint32_t *)g_dch_cfg_regs[i];
        *pdci->cfg_reg_p |= APC_DCH_R_FLU | APC_DCH_L_FLU; // flush each L/R channel
        *pdci->cfg_reg_p &= ~(APC_DCH_R_EN | APC_DCH_L_EN); // disable each L/R channel

        pdci->fixed_lr[0] = &g_chs_fixed[i*2];
        pdci->fixed_lr[1] = &g_chs_fixed[i*2+1];
        pdci->dma_ch_lr[0] = DMA_CHANNEL_ANY;
        pdci->dma_ch_lr[1] = DMA_CHANNEL_ANY;
    }


    // disable / get raw / clear owned interrupts
    for (i=0; i<APC_DCH_COUNT && DCH_IS_OWNED(i); i++) {
        apc_dch_intr_disable(i, APC_FIFO_INTR_ALL, ON_IRQ_APC0);
        if (apc_dch_intr_raw_status(i, ON_IRQ_APC0))
            apc_dch_intr_clear(i, APC_FIFO_INTR_ALL, ON_IRQ_APC0);
    }

#if SUPPORT_I2S_ERR
    // clear I2S0/1 error if any
    if (apc_i2s_err_intr_raw_status(APC_INTF_IDX_I2S0, ON_IRQ_APC0))
        apc_i2s_err_intr_clear(APC_INTF_IDX_I2S0, ON_IRQ_APC0);
    if (apc_i2s_err_intr_raw_status(APC_INTF_IDX_I2S1, ON_IRQ_APC0))
        apc_i2s_err_intr_clear(APC_INTF_IDX_I2S1, ON_IRQ_APC0);
#endif // SUPPORT_I2S_ERR

    // Register APC ISR
    register_ISR(IRQ_APC_VECTOR, (ISR)apc_irq_handler, NULL);

    // Enable APC IRQ
    enable_IRQ(IRQ_APC_VECTOR);

    // Initialize DMA Controller
#if USE_GPDMA2D
    DMA2D_Initialize();
#endif

#if USE_CMNDMA
    dma_initialize();
#endif

    // Enable APC
    CSK_APC->REG_APC_CFG.all |= APC_ENABLE_BIT; //  | APC_AUTO_CLK_GATING_BIT

    return CSK_DRIVER_OK;
}


/**
  \fn          int32_t apc_uninitialize (void)
  \brief       De-initialize Audio Processing Center
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
int32_t apc_uninitialize (void)
{
    // Check if DMA is initialized
    if (g_init_cnt == 0U) { return CSK_DRIVER_ERROR; }

    //g_init_cnt--;
    //if (g_init_cnt != 0U) { return CSK_DRIVER_OK; }
    if (__AMOADD_W(&g_init_cnt, -1) != 0U)  { return CSK_DRIVER_OK; }

    // Abort all transfers if any
    uint32_t i;
    for (i=0; i<APC_DCH_COUNT && DCH_IS_OWNED(i); i++)
        apc_dual_channel_abort(i);

    if (ALL_DCH_OWNED) {
        // Disable APC
        CSK_APC->REG_APC_CFG.all &= ~APC_ENABLE_BIT;

        // disable APC Clock
        apc_clk_disable();
    }

    // Disable APC IRQ
    disable_IRQ(IRQ_APC_VECTOR);

    // Unregister APC ISR
    register_ISR(IRQ_APC_VECTOR, NULL, NULL);

    // Uninitialize DMA Controller
#if USE_GPDMA2D
    //FIXME: why is there no DMA2D_Uninitialize?
    //GPDMA_Uninitialize();
#endif

#if USE_CMNDMA
    dma_uninitialize();
#endif

    return CSK_DRIVER_OK;
}


/**
  \fn          int32_t apc_channel_setup ();
  \brief       Configure APC channel to transfer data for ADC(AMIC, DMIC), DAC, and I2S etc.
               NOTE: the last setup is vaild if the function is called many times
                    for the same APC channel.
  \param[in]   ch           The selected APC Channel, see definitions of APC Channel.
  \param[in]   itf_type     one of interface type, ADC, DAC, or I2S.
  \param[in]   itf_idx    the instance index of the interface type.
  \param[in]   dma_chs[2] one or two dma channels for the dual_channel,
                          dma_chs[0] for left, dma_chs[1] for right, 0xFF indicates NOT USED!!
  \param[in]   cb_event  Pointer to \ref CSK_APC_SignalEvent_t
  \param[in]   usr_param  User-defined value, acts as last parameter of cb_event
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/

static bool apc_dch_try_set_inf(APC_DCH dch, APC_INTF_TYPE itf_type)
{
    bool ret = false;
    uint8_t gie = GINT_enabled();

    if (!DCH_IS_OWNED(dch))
        return false;
    if (g_apc_dev.dch_array[dch].intf_type == itf_type)
        return true;

    // disable Global interrupt (NO interrupts, NO task scheduling)
    if (gie)    disable_GINT();
    if (g_apc_dev.dch_array[dch].intf_type == APC_INTF_UNSET ||
        itf_type == APC_INTF_UNSET) {
        g_apc_dev.dch_array[dch].intf_type = itf_type;
        ret = true;
    }
    // enable Global interrupt
    if (gie)    enable_GINT();
    return ret;
}

// generally called in XXX_Initialize()
int32_t apc_dual_channel_acquire (APC_DCH    dch,
                           APC_INTF_TYPE    itf_type,
                           APC_INTF_INDEX   itf_idx,
                           uint8_t          dma_chs[2],
                           CSK_APC_SignalEvent_t    cb_event,
                           uint32_t         usr_param)
{
    int32_t ret = CSK_DRIVER_OK;

    // Check if APC is initialized
    if ( !IS_APC_INITIALIZED() )
        return CSK_DRIVER_ERROR;

    // Check if parameters are valid, i.e. APC dual_channel is valid
    if (dch >= APC_DCH_COUNT || !DCH_IS_OWNED(dch) || itf_type >= APC_INTF_TYPE_COUNT)
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check dma channel of left/right APC channel
    // only dual_channel APC_DCH_IN0 can own 2 dma channels, that is,
    // dma_chs[1] (for right APC channel) SHOULD be 0xFF except APC_DCH_IN0!
    if (dch != APC_DCH_IN0 && dma_chs[1] != 0xFF) {
        LOGD("%s:only right one of dual_channel 0 CAN have its own dma channel!\r\n", __func__);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    // Check if APC channel is occupied
    if (!apc_dch_try_set_inf(dch, itf_type)) {
        LOGD("%s: APC dual_channel %d has been used for Interface type %d !!\r\n",
                __func__, dch, g_apc_dev.dch_array[dch].intf_type);
        return CSK_DRIVER_ERROR;
    }

    // Check the correspondence between dual_channel and interface type & index
    switch (dch) {
    case APC_DCH_IN0: {
        /*
        //struct APC_REG_APC_RX_CH0_CFG_BITS *pCfg;
        //pCfg = (struct APC_REG_APC_RX_CH0_CFG_BITS *)g_dch_cfg_regs[dch];
        switch (itf_type) {
        case APC_INTF_ADC_PDM:
            if (itf_idx != APC_INTF_IDX_ADC01) {
                ret = CSK_DRIVER_ERROR_PARAMETER;
                break;
            }
            //pCfg->RX_CH0_SRC_SEL = RX_CH0_SRC_ADC01;
            break;
        default:
            ret = CSK_DRIVER_ERROR_PARAMETER;
            break;
        }
        */
        if (itf_type != APC_INTF_ADC_PDM || itf_idx != APC_INTF_IDX_ADC01) {
            ret = CSK_DRIVER_ERROR_PARAMETER;
        }
        break;
    }

    case APC_DCH_IN1: {
        struct APC_REG_APC_RX_CH1_CFG_BITS *pCfg;
        pCfg = (struct APC_REG_APC_RX_CH1_CFG_BITS *)g_dch_cfg_regs[dch];
        switch (itf_type) {
        case APC_INTF_I2S_IN:
            if (itf_idx != APC_INTF_IDX_I2S0) {
                ret = CSK_DRIVER_ERROR_PARAMETER;
                break;
            }
            pCfg->RX_CH1_SRC_SEL = RX_CH1_SRC_I2S0;
            break;
        case APC_INTF_ECHO:
            if (itf_idx == APC_INTF_IDX_ECHO_DAC) // ECHO for APC_DCH_OUT0
                pCfg->RX_CH1_SRC_SEL = RX_CH1_SRC_ECHO_DAC;
            else if (itf_idx == APC_INTF_IDX_ECHO_I2S1) // ECHO for APC_DCH_OUT2
                pCfg->RX_CH1_SRC_SEL = RX_CH1_SRC_ECHO_I2S1OUT;
            else
                ret = CSK_DRIVER_ERROR_PARAMETER;
            break;
        default:
            ret = CSK_DRIVER_ERROR_PARAMETER;
            break;
        }
        break;
    }

    case APC_DCH_IN2: {
        /*
        //struct APC_REG_APC_RX_CH2_CFG_BITS *pCfg;
        //pCfg = (struct APC_REG_APC_RX_CH2_CFG_BITS *)g_dch_cfg_regs[dch];
        switch (itf_type) {
        case APC_INTF_I2S_IN:
            if (itf_idx != APC_INTF_IDX_I2S1) {
                ret = CSK_DRIVER_ERROR_PARAMETER;
                break;
            }
            //pCfg->RX_CH2_SRC_SEL = RX_CH0_SRC_I2S1;
            break;
        default:
            ret = CSK_DRIVER_ERROR_PARAMETER;
            break;
        }
        */
        if (itf_type != APC_INTF_I2S_IN || itf_idx != APC_INTF_IDX_I2S1) {
            ret = CSK_DRIVER_ERROR_PARAMETER;
        }
        break;
    }

    case APC_DCH_OUT0: {
        /*
        //struct APC_REG_APC_TX_CH0_CFG_BITS *pCfg;
        //pCfg = (struct APC_REG_APC_TX_CH0_CFG_BITS *)g_dch_cfg_regs[dch];
        switch (itf_type) {
        case APC_INTF_DAC:
            if (itf_idx != APC_INTF_IDX_DAC) {
                ret = CSK_DRIVER_ERROR_PARAMETER;
                break;
            }
            //pCfg->TX_CH0_DST_SEL = TX_CH0_DST_DAC;
            break;
        default:
            ret = CSK_DRIVER_ERROR_PARAMETER;
        }
        */
        if (itf_type != APC_INTF_DAC || itf_idx != APC_INTF_IDX_DAC) {
            ret = CSK_DRIVER_ERROR_PARAMETER;
        }
        break;
    }

    case APC_DCH_OUT1: {
        if (itf_type != APC_INTF_I2S_OUT || itf_idx != APC_INTF_IDX_I2S0) {
            ret = CSK_DRIVER_ERROR_PARAMETER;
        }
        break;
    }

    case APC_DCH_OUT2: {
        if (itf_type != APC_INTF_I2S_OUT || itf_idx != APC_INTF_IDX_I2S1) {
            ret = CSK_DRIVER_ERROR_PARAMETER;
        }
        break;
    }

    default:
        ret = CSK_DRIVER_ERROR_PARAMETER;

    } // end switch (dch)

    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    if (ret == CSK_DRIVER_OK) {
//        pdci->intf_type = itf_type;
        pdci->intf_idx = itf_idx;
        pdci->cb_event = cb_event;
        pdci->usr_param = usr_param;
        pdci->dma_ch_lr[0] = dma_chs[0];
        pdci->dma_ch_lr[1] = dma_chs[1];
    } else {
        pdci->intf_type = APC_INTF_UNSET;
    }

    return ret; // CSK_DRIVER_OK
}


// generally called in XXX_Uninitialize()
int32_t apc_dual_channel_release (APC_DCH    dch)
{
    // Check if APC is initialized
    if ( !IS_APC_INITIALIZED() )
        return CSK_DRIVER_ERROR;

    // Check if parameters are valid, i.e. APC dual_channel is valid
    if (dch >= APC_DCH_COUNT || !DCH_IS_OWNED(dch))
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check if APC channel is occupied
    if ( !IS_APC_DCH_OCCUPIED(dch) ) {
        LOGD("%s: APC dual_channel %d has NOT been occupied, do nothing...\r\n",
                __func__, dch);
        return CSK_DRIVER_OK;
    }

    // Abort current transfer if necessary
    apc_dual_channel_abort(dch);

    // Disable all types of interrupts
    apc_dch_intr_disable(dch, APC_FIFO_INTR_ALL, ON_IRQ_APC0);

    // Disable L/R channel of dual_channel and Flush L/R channel FIFO
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    uint32_t cfg_val = *(pdci->cfg_reg_p);
    cfg_val &= ~(APC_DCH_R_EN | APC_DCH_L_EN); // disable each L/R channel;
    cfg_val |= (APC_DCH_L_FLU | APC_DCH_R_FLU); // flush TX/RX L/R FIFO
    *(pdci->cfg_reg_p) = cfg_val;

    //TODO: other dual_channel cleanup operations?

//    uint8_t gie = GINT_enabled();
//
//    // disable Global interrupt (NO interrupts, NO task scheduling)
//    if (gie)    disable_GINT();

    // Clear current dual_channel settings
    memset(pdci, 0, sizeof(APC_DCH_INFO));
    pdci->cfg_reg_p= (uint32_t *)g_dch_cfg_regs[dch];
    pdci->fixed_lr[0] = &g_chs_fixed[dch*2];
    pdci->fixed_lr[1] = &g_chs_fixed[dch*2+1];

//    // enable Global interrupt
//    if (gie)    enable_GINT();

    return CSK_DRIVER_OK;
}


// retrieve interface type, channel no. of the type, and channel direction for APC channel
int32_t apc_dual_channel_owner (APC_DCH dch,
                            APC_INTF_TYPE *itf_type_p,
                            APC_INTF_INDEX *itf_idx_p,
                            uint8_t *ch_dir_p)
{
    // Check if APC is initialized
    if ( !IS_APC_INITIALIZED() )
        return CSK_DRIVER_ERROR;

    // Check if parameters are valid, i.e. APC channel is valid
    if (dch >= APC_DCH_COUNT || !DCH_IS_OWNED(dch))
        return CSK_DRIVER_ERROR_PARAMETER;

    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    if (itf_type_p != NULL)
        *itf_type_p = pdci->intf_type;
    if (itf_idx_p != NULL)
        *itf_idx_p = pdci->intf_idx;
    if (ch_dir_p != NULL)
        *ch_dir_p = pdci->fixed_lr[0]->ch_dir;

    return CSK_DRIVER_OK;
}


// generally called in XXX_Control()
// ch_mix   0: no mix, 1: mix @ left channel (by default), 2: mix @ right channel
int32_t apc_dual_channel_setup (APC_DCH     dch,
                           APC_CHMODE       ch_mode, // 16, 24 LSB, 32 or 24 MSB?
                           uint8_t          ch_sel, // select APC_DCH_BMP_LEFT, APC_DCH_BMP_RIGHT, or APC_DCH_BMP_STEREO
                           uint8_t          ch_mix, // read L/R channel as a whole, or read L/R channel respectively
                           uint8_t          ch_flag) //bit[0]: only valid for 24 MSB and 32 channel mode, when set to 1:
                                                         // record: trim low 16bits, get 16bits audio data
                                                         // playback: 16bits audio data is placed at high 16bits of WORD
                                                      //bit[1]: use PIO instead of DMA operation
                                                      //bit[2]: use which DMAC? 0 is CMNDMA, 1 is GPDMA2D

{
    // Check if APC is initialized
    if ( !IS_APC_INITIALIZED() )
        return CSK_DRIVER_ERROR;

    // Check if parameters are valid, i.e. APC channel is valid
    if (dch >= APC_DCH_COUNT || !DCH_IS_OWNED(dch) || ch_mode >= APC_CHMODE_COUNT)
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check if APC channel is occupied
    if ( !IS_APC_DCH_OCCUPIED(dch) ) {
        LOGD("%s: APC dual_channel %d has NOT been occupied, "
                "SHOULD call apc_channel_acquire() first!!\r\n",
                __func__, dch);
        return CSK_DRIVER_ERROR;
    }

    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    pdci->ch_sel = ch_sel & APC_DCH_BMP_MASK;
    if ((ch_sel & APC_DCH_BMP_MASK) == APC_DCH_BMP_STEREO && ch_mix) {
        pdci->mix_mode = APC_DCH_MIXED_MODE;
        pdci->mix_target = ch_mix & 0x2 ? 1 : 0; // 1 @ right, 0 @ left (or NOT mixed)
    } else {
        pdci->mix_mode = APC_DCH_SEPARATE_MODE;
        pdci->mix_target = 0; // 0 indicates NOT mixed
    }

    pdci->ch_mode = ch_mode;
    switch (ch_mode) {
    case APC_CHMODE_16BITS:
        pdci->samp_bits = 16;
        break;
    case APC_CHMODE_24BITS_LOW:
    case APC_CHMODE_24BITS_HIGH:
        pdci->samp_bits = 24;
        break;
    case APC_CHMODE_32BITS:
        pdci->samp_bits = 32;
        break;
    default:
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint8_t trim_16bits = ch_flag & 0x1;
    uint8_t use_pio = ch_flag & 0x2;
    uint8_t use_gpdma = ch_flag & 0x4;

    // bit width is fixed on WORD
    pdci->dma_width_bits = DMA_WIDTH_WORD;
    if (trim_16bits != 0 &&
        (ch_mode == APC_CHMODE_24BITS_HIGH || ch_mode == APC_CHMODE_32BITS))
        pdci->dma_width_bits = DMA_WIDTH_HALFWORD;

    uint32_t fifo_depth, val, entries;

    // FIFO depth of left channel is supposed to be same as one of right channel
    fifo_depth = pdci->fixed_lr[0]->fifo_depth;

    // write setup parameters into register
    APC_DCH_CFG_REG cfg_reg;

    cfg_reg.all = inw(g_dch_cfg_regs[dch]);
    if (pdci->ch_sel & APC_DCH_BMP_LEFT)
       cfg_reg.bit.CH_L_FIFO_FLUSH = 1; // flush FIFO of left channel
    if (pdci->ch_sel & APC_DCH_BMP_RIGHT)
       cfg_reg.bit.CH_R_FIFO_FLUSH = 1; // flush FIFO of right channel

    cfg_reg.bit.CH_L_MODE = pdci->ch_mode;
    cfg_reg.bit.CH_R_MODE = pdci->ch_mode;
    cfg_reg.bit.DCH_STEREO_MODE = pdci->mix_mode;

#if SUPPORT_APC_PIO
    if (use_pio) {
        val = DMA_BSIZE_1;
        pdci->dma_bsize_bits = gpdma_burst_len_1spl;
        // set interrupt level & priority higher than default, make APC interrupt (READY_TO_XFER) respond quickly!
        //ECLIC_SetLevelIRQ(IRQ_APC_VECTOR, DEF_INTERRUPT_LEVEL + 1);
        //ECLIC_SetPriorityIRQ(IRQ_APC_VECTOR, DEF_INTERRUPT_PRIORITY + 1);
    } else
#endif

    if (use_gpdma == 0) { // CMNDMA is used
    #if USE_CMNDMA
        if (pdci->mix_mode == APC_DCH_MIXED_MODE) {
            // try setting DMA burst size to FIFO depth of APC channel
            entries = fifo_depth; // 1; // 4; // 8; //
            // For mixed/stereo mode, APC DMA request threshold is the sum of LEFT channel and RIGHT channel.
            // When sample bits is 24 or 32, data of burst size (fifo_depth) are divided equally into two parts,
            // one for LEFT channel and the other for RIGHT channel.
            // When sample bits is 16, one word (32-bit) of data is divided equally into two parts,
            // one 16-bit for LEFT channel and the other 16-bit for RIGHT channel, and each 16bit occupies one FIFO entry,
            // so data words of burst size (fifo_depth) are split equally, 16-bit data of burst size for LEFT channel and
            // 16-bit data of burst size for RIGHT channel.
            val = ITEMS_TO_BSIZE(entries);
            pdci->dma_bsize_bits = (pdci->samp_bits > 16 ? val : ITEMS_TO_BSIZE(entries >> 1));
        } else {
            // try setting DMA burst size to half FIFO depth of APC channel
            entries = fifo_depth >> 1;
            // For separated/mono mode, APC DMA request threshold is set for LEFT channel or RIGHT channel respectively.
            // When sample bits is 16, one word (32-bit) of data includes two 16-bit samples, and occupies one FIFO entry,
            // so length (word count) of burst size is just half of FIFO depth.
            val = ITEMS_TO_BSIZE(entries);
            //pdci->dma_bsize_bits = (pdci->samp_bits > 16 ? val : ITEMS_TO_BSIZE(entries >> 1));
            pdci->dma_bsize_bits = val;
        }
        assert(pdci->dma_bsize_bits <= DMA_BSIZE_16);
    #endif // USE_CMNDMA

    } else { // GPDMA2D is used
        val = DMA_BSIZE_8;
    #if USE_GPDMA2D // GP DMAC
        pdci->dma_bsize_bits = dma2d_burst_len_8spl;
    #endif
    } // end GPDMA2D is used

    cfg_reg.bit.DCH_DMA_THD_SEL = val & 0x3;

    assert(pdci->cfg_reg_p != NULL);
    *pdci->cfg_reg_p = cfg_reg.all;

#if 0 //1 // CANNOT write data into APC channel before it's enabled with current IP design!!
    // Fill TX FIFO with 0 (L/R channel has the same direction)
    if (pdci->fixed_lr[0]->ch_dir == 0) {
        uint32_t i;
        *pdci->cfg_reg_p |= APC_DCH_R_EN | APC_DCH_L_EN; // enable APC channel temporarily
        if (pdci->mix_mode == APC_DCH_SEPARATE_MODE) {
            for (i=0; i<pdci->fixed_lr[0]->fifo_depth; i++) { // /2
                outw(pdci->fixed_lr[0]->data_addr, 0);
                outw(pdci->fixed_lr[1]->data_addr, 0);
            }
        } else {
            for (i=0; i<pdci->fixed_lr[0]->fifo_depth * 2; i++) {
                outw(pdci->fixed_lr[0]->data_addr, 0);
            }
        }
        *pdci->cfg_reg_p &= ~(APC_DCH_R_EN | APC_DCH_L_EN); // disable APC channel
        //while(1); // TEST ONLY!!
    }

    // Clear all interrupts' status (MUST BE LAST STEP!)
    apc_dch_intr_clear(dch, APC_FIFO_INTR_MASK, ON_IRQ_APC0);
#endif

    if (use_pio == 0) { // use DMA
        if (pdci->dma_ch_lr[0] == 0xFF && pdci->dma_ch_lr[1] == 0xFF) {
            LOGD("NO DMA channel is assigned for either APC channel!\n");
            return CSK_DRIVER_ERROR;
        }
    }

    if (use_gpdma) {
    #if USE_GPDMA2D //FIXME: use DMA2D API instead of original GPDMA!!
        uint32_t usr_param;

        // APC channel FIFO is 16-word in depth, so always use max burst size (8-word)
        csk_dma2d_init_t gpdma_para = {
                .src_burst_len = pdci->dma_bsize_bits, //gpdma_burst_len_8spl
                .dst_burst_len = pdci->dma_bsize_bits, //gpdma_burst_len_8spl
                .src_basic_unit = dma2d_sample_unit_word,
                .dst_basic_unit = dma2d_sample_unit_word,
                .src_inc_mode = inc_mode_increase,
                .dst_inc_mode = inc_mode_increase,
                .prio_lvl = prio_mode_high,
        };

        if (dch > APC_DCH_IN_MAX) { // OUT
            gpdma_para.tfr_mode = tfr_mode_m2p;  // memory => fifo
            gpdma_para.dst_inc_mode = inc_mode_fix; // dst is data register
        } else { // IN
            gpdma_para.tfr_mode = tfr_mode_p2m; // fifo => memory
            gpdma_para.src_inc_mode = inc_mode_fix; // src is data register
        }

        if (pdci->dma_width_bits == DMA_WIDTH_HALFWORD) { // DMA width
            gpdma_para.src_basic_unit = dma2d_sample_unit_halfword;
            gpdma_para.dst_basic_unit = dma2d_sample_unit_halfword;
        }

        // value[3:0] of csk_handshake_num_t type is the actual handshake ID
        // value[7:4] of csk_handshake_num_t type is the actual handshake group
        if (pdci->dma_ch_lr[0] != 0xFF) { // left channel
            gpdma_para.dma_ch =  pdci->dma_ch_lr[0];
            gpdma_para.handshake = pdci->fixed_lr[0]->gpdma_hsid;
            //set_gpdma2d_hs(gpdma_para.handshake, pdci->fixed_lr[0]->gpdma_hssel); // hs_sel = 0
            set_gpdma2d_hs(gpdma_para.handshake, 0); // hs_sel = 0
            usr_param = (dch << 8) | (pdci->mix_mode ? APC_DCH_BMP_STEREO : APC_DCH_BMP_LEFT);
            DMA2D_Config(&gpdma_para, apc_gpdma2d_event, (void*)usr_param);
        }
        if (pdci->dma_ch_lr[1] != 0xFF && !pdci->mix_mode) { // right channel and separate mode
            gpdma_para.dma_ch =  pdci->dma_ch_lr[1];
            gpdma_para.handshake = pdci->fixed_lr[1]->gpdma_hsid;
            //set_gpdma2d_hs(gpdma_para.handshake, pdci->fixed_lr[1]->gpdma_hssel); // hs_sel = 0
            set_gpdma2d_hs(gpdma_para.handshake, 0); // hs_sel = 0
            usr_param = (dch << 8) | APC_DCH_BMP_RIGHT; //lr_bmp
            DMA2D_Config(&gpdma_para, apc_gpdma2d_event, (void*)usr_param);
        }
    #else // !USE_GPDMA2D
        LOGD("%s: USE_GPDMA2D is defined as 0, do nothing!!!\n", __func__);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    #endif //GPDMA

    } else { // CMNDMA is used
    #if USE_CMNDMA
        // For common DMA do nothing here...
        // DMA channel setup etc. operations are deferred into first R/W operation!
    #endif // USE_CMNDMA
    } //end CMNDMA is used

    if (use_pio) { // use PIO
        pdci->rt_flag_lr[0] |= RT_FLAG_USE_PIO;
        pdci->rt_flag_lr[1] |= RT_FLAG_USE_PIO;
    } else if (use_gpdma) { // use GPDMA
        pdci->rt_flag_lr[0] |= RT_FLAG_USE_GPDMA;
        pdci->rt_flag_lr[1] |= RT_FLAG_USE_GPDMA;
    }

/*
#if SUPPORT_I2S_ERR
    // Set I2S timeout configuration for I2S
    if (pdci->intf_type == APC_INTF_I2S_IN || pdci->intf_type == APC_INTF_I2S_OUT) {
        apc_i2s_timeout_set(pdci->intf_idx, APC_I2S_TIMEOUT_CFG_VAL);
    }
#endif // SUPPORT_I2S_ERR
*/

    // APC channel setup is done
    pdci->setup_done = 1;

    return CSK_DRIVER_OK;
}


/*
// SRC MODE
#define APC_SRC_48K_8KHZ    0 // 48KHz->8KHz
#define APC_SRC_48K_16KHZ   1 // 48KHz->16KHz
// SRC CHANNEL ENABLE (LR channels SHOULD be both enabled or disabled)
#define APC_SRC_DISABLE     0x0
#define APC_SRC_ENABLE      0x3
// Sample Rate
#define APC_SAMP_RATE_48KHZ     48000
#define APC_SAMP_RATE_16KHZ     16000
#define APC_SAMP_RATE_8KHZ      8000
*/

//NOTE: apc_echo_setup() SHOULD be called after apc_dual_channel_setup() for the ECHO dual_channel!!
int32_t apc_echo_setup (APC_DCH  dch,
                        uint32_t tx_samp_rate,
                        uint32_t echo_samp_rate,
                        uint8_t auto_feed,
                        uint8_t echo_bmp)
{
    int32_t ret;
    if (dch != APC_DCH_ECHO || !DCH_IS_OWNED(dch)) {
        LOGD("%s: Invalid ECHO dual_channel (dch = %d)!\n",  __func__, dch);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    if (pdci->setup_done == 0) {
        LOGD("%s: SHOULD be called after apc_dual_channel_setup()!\n",  __func__);
        return CSK_DRIVER_ERROR;
    }

    struct APC_REG_APC_RX_CH1_CFG_BITS *rx_cfg_p;
    rx_cfg_p = (struct APC_REG_APC_RX_CH1_CFG_BITS *)APC_RX_CH1_CFG_ADDR;
    assert( (uint32_t)pdci->cfg_reg_p == (uint32_t)rx_cfg_p );

    // setup SRC (Sampling Rate Conversion)
    ret = CSK_DRIVER_OK;
    if (echo_samp_rate != tx_samp_rate) { // need SRC
        //only ECHO (@DCH0 on VENUSA) supports SRC !!
        if (dch != APC_DCH_ECHO || tx_samp_rate != APC_SAMP_RATE_48KHZ ||
            (echo_samp_rate != APC_SAMP_RATE_8KHZ && echo_samp_rate != APC_SAMP_RATE_16KHZ)) {
            LOGD("%s: only ECHO0 (from TX_CH0) supports SRC48KHz => 8/16KHz!\n",  __func__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }

        rx_cfg_p->SRC_CH_EN = APC_SRC_DISABLE;

        // write 1 to generate single pulse to clear SRC data path
        // NOTE: clear SRC should be done prior to SRC_MODE & SRC_CH_EN settings
        rx_cfg_p->SRC_CLR = 1;

        switch (echo_samp_rate) {
        case APC_SAMP_RATE_16KHZ:
            rx_cfg_p->SRC_MODE = APC_SRC_48K_16KHZ;
            rx_cfg_p->SRC_CH_EN = APC_SRC_ENABLE & echo_bmp;
            break;
        case APC_SAMP_RATE_8KHZ:
            rx_cfg_p->SRC_MODE = APC_SRC_48K_8KHZ;
            rx_cfg_p->SRC_CH_EN = APC_SRC_ENABLE & echo_bmp;
            break;
        default:
            ret = CSK_DRIVER_ERROR;
            break;
        } // end switch

        if (ret != CSK_DRIVER_OK) {
            CLOGW("%s: %d => %d Hz SRC is NOT supported!\n",
                    __func__, tx_samp_rate, echo_samp_rate);
            return ret;
        }

    } //end SRC

    // whether to set Auto feed when reading TX channel?
    if (rx_cfg_p->RX_CH1_SRC_SEL == RX_CH1_SRC_ECHO_DAC) { //DAC echo
        struct APC_REG_APC_TX_CH0_CFG_BITS *tx_cfg_p;
        tx_cfg_p = (struct APC_REG_APC_TX_CH0_CFG_BITS *)APC_TX_CH0_CFG_ADDR;
        tx_cfg_p->TX_CH0_RD_AUTOFEED = (auto_feed ? 1 : 0);
    } else if (rx_cfg_p->RX_CH1_SRC_SEL == RX_CH1_SRC_ECHO_I2S1OUT) { //I2S1 OUT echo
        struct APC_REG_APC_TX_CH2_CFG_BITS *tx_cfg_p;
        tx_cfg_p = (struct APC_REG_APC_TX_CH2_CFG_BITS *)APC_TX_CH0_CFG_ADDR;
        tx_cfg_p->TX_CH2_RD_AUTOFEED = (auto_feed ? 1 : 0);
    }

    return CSK_DRIVER_OK;
}

#if USE_CMNDMA
static int32_t
apc_channel_dma_read_inclusive(APC_DCH  dch,
                          uint8_t lr_idx,
                          uint32_t *sample_data,
                          uint32_t  sample_cnt,
                          uint32_t  buf_offset, // byte offset to each buffer (default 0, NO SG support)
                          uint32_t  dst_scat) // destination scatter setting (default 0, NO SG support)
{
    int ret = CSK_DRIVER_OK;
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];

    //uint32_t usr_param = (dch << 8) | (pdci->mix_mode ? APC_DCH_BMP_STEREO : APC_DCH_BMP_LEFT);
    uint32_t usr_param = (dch << 8) | (pdci->mix_mode ? APC_DCH_BMP_STEREO : (0x1 << lr_idx));
    uint8_t *dma_ch_p = &pdci->dma_ch_lr[lr_idx];

    // select some free DMA channel
    *dma_ch_p = dma_channel_select( dma_ch_p,
                                    apc_cmndma_event,
                                    usr_param,
                (pdci->rt_flag_lr[lr_idx] & RT_FLAG_NSYNCA) ? DMA_CACHE_SYNC_NOP : DMA_CACHE_SYNC_DST);
    if (*dma_ch_p == DMA_CHANNEL_ANY) {
        LOGD("%s: NO free DMA channel!!\r\n", __func__);
        return CSK_DRIVER_ERROR;
    }

    // setup DMA channel
    //NOTE: VENUSA CMNDMA has 2 master ports, and they are exactly the same. see DMS/SMS setting.
    uint32_t control, config_low, config_high;
    uint8_t dma_hsid = pdci->fixed_lr[lr_idx]->dma_hsid;
    control = DMA_CH_CTLL_DST_WIDTH(pdci->dma_width_bits) | DMA_CH_CTLL_SRC_WIDTH(pdci->dma_width_bits) |
            DMA_CH_CTLL_DST_BSIZE(pdci->dma_bsize_bits) | DMA_CH_CTLL_SRC_BSIZE(pdci->dma_bsize_bits) |
            DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_TTFC_P2M |
            DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(1) | DMA_CH_CTLL_INT_EN | (dst_scat ? DMA_CH_CTLL_D_SCAT_EN : 0);
    config_low = DMA_CH_CFGL_CH_PRIOR(1); // channel priority is higher than 0
    config_high = DMA_CH_CFGH_SRC_PER(dma_hsid); // DMA_CH_CFGH_PROTCTL(1) | DMA_CH_CFGH_FIFO_MODE |

    ret = dma_channel_setup (*dma_ch_p, DMA_CH_EN_XFER_INT, control, config_low, config_high, 0, dst_scat);
    if (ret < 0) {
        LOGD("%s: Failed to call dma_channel_setup!!\r\n", __func__);
        dma_channel_disable(*dma_ch_p, false);
        //*dma_ch_p = DMA_CHANNEL_ANY;
        return ret;
    }

    //set_cmndma_hs(dma_hsid, pdci->fixed_lr[lr_idx]->dma_hssel); // hs_sel = 1
    set_cmndma_hs(dma_hsid, 1); // hs_sel = 1

    // start DMA transfer
    uint32_t src_addr = pdci->fixed_lr[lr_idx]->data_addr;
    if (pdci->dma_width_bits == DMA_WIDTH_HALFWORD) // trim low 16bits
        src_addr += 2; // high 16bits address

    ret = dma_channel_start_block (*dma_ch_p,
                        DMACH_CFG_FLAG_BOTH_ADDR,
                        src_addr,
                        (uint32_t) sample_data + buf_offset,
                        pdci->samp_bits > 16 ? sample_cnt : sample_cnt/2);
    if (ret < 0) {
        //LOGD("%s: Failed to call dma_channel_start_block!!\r\n", __func__);
        dma_channel_disable(*dma_ch_p, false);
        //*dma_ch_p = DMA_CHANNEL_ANY;
        return ret;
    }

    return ret;
}
#endif

// sample_cnt SHOULD be EVEN when 16-bit sample!
static int32_t
apc_channel_read_internal (APC_DCH  dch,
                          uint8_t   lr_bmp,
                          uint32_t *sample_data,
                          uint32_t  sample_cnt,
                          uint32_t  buf_offset, // byte offset to each buffer (default 0, NO SG support)
                          uint32_t  dst_scat) // destination scatter setting (default 0, NO SG support)
{
    // Check if parameters are valid, i.e. APC channel is valid
    if (dch > APC_DCH_IN_MAX || !DCH_IS_OWNED(dch) || sample_data == NULL || sample_cnt == 0)
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check if APC and specified dual_channel are both initialized
    if (!IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
        return CSK_DRIVER_ERROR;

    lr_bmp &= APC_DCH_BMP_MASK;
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    // 0 = left, 1 = right (use left or right channel for dual_channel)
    uint8_t lr_idx = (lr_bmp & APC_DCH_BMP_LEFT)? 0 : 1;
    //if (pdci->mix_target == 1) // mixed @ right channel
    //    lr_idx = 1;

    // sample_cnt should be even when 16-bit sample
    if (pdci->ch_mode == APC_CHMODE_16BITS && (sample_cnt & 0x1) != 0) {
        CLOGE("%s: sample count (%d) of 16bit sample should be even!!\r\n",
                __func__, sample_cnt);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    // for single channel, Only STEREO_MODE (separated L/R channel data) is supported
    // for dual channel, Only MONO_MODE (mixed L/R channel data) is supported
    if ((lr_bmp != APC_DCH_BMP_STEREO && pdci->mix_mode == APC_DCH_MIXED_MODE) ||
        (lr_bmp == APC_DCH_BMP_STEREO && pdci->mix_mode == APC_DCH_SEPARATE_MODE)) {
        CLOGE("%s: 1 channel requires mono mode, 2 channels requires stereo mode!!\r\n", __func__);
        return CSK_DRIVER_ERROR;
    }

    // Only ADC / I2S IN / ECHO interface can be read
    if (//pdci->intf_type != APC_INTF_VAD &&
        pdci->intf_type != APC_INTF_ADC_PDM &&
        pdci->intf_type != APC_INTF_I2S_IN &&
        pdci->intf_type != APC_INTF_ECHO)
        return CSK_DRIVER_ERROR;

    // just return busy if the channel is busy
    if (pdci->busy_lr[lr_idx] != 0) {
        CLOGE("%s: apc dch %d (lr_idx = %d) is busy!!\r\n", __func__, dch, lr_idx);
        return CSK_DRIVER_ERROR_BUSY;
    }

    pdci->samps_lr[lr_idx] = 0;

    // PIO is used
    if (pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_PIO) {
    #if SUPPORT_APC_PIO
        pdci->sambuf_lr[lr_idx] = sample_data;
        pdci->samcnt_lr[lr_idx] = sample_cnt;
        pdci->busy_lr[lr_idx] = 1;
        return CSK_DRIVER_OK;
    #else
        LOGD("%s: SUPPORT_APC_PIO is defined as 0, do nothing for PIO read!\n", __func__);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    #endif
    }

    // DMA (CMNDMA or GPDMA2D) is used
    uint8_t dma_ch = pdci->dma_ch_lr[lr_idx];
    uint8_t dma_hsid = pdci->fixed_lr[lr_idx]->dma_hsid;
    int32_t stat;

    if ((pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_GPDMA) == 0) {// CMNDMA is used
    #if USE_CMNDMA
        if (dma_channel_check_select(dma_ch, DMA_TT_P2M, dma_hsid)) {
            stat = dma_channel_start_block(dma_ch, DMACH_CFG_FLAG_DST_ADDR,
                                    0, (uint32_t) sample_data + buf_offset,
                                    pdci->samp_bits > 16 ? sample_cnt : sample_cnt/2);
            if (stat < 0) {
                //LOGD("%s: Failed to call dma_channel_start_block!!\r\n", __func__);
                return stat;
            }
        } else {
            stat = apc_channel_dma_read_inclusive(dch, lr_idx, sample_data, sample_cnt, buf_offset, dst_scat);
            if (stat < 0) {
                //LOGD("%s: Failed to call apc_channel_dma_read_inclusive!!\r\n", __func__);
                return stat;
            }
        }
    #else // !USE_CMNDMA
        LOGD("%s: USE_CMNDMA is defined as 0, do nothing for CMNDMA read!\n", __func__);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    #endif

    } else { // GPDMA2D is used
    #if USE_GPDMA2D
        uint32_t src_addr = pdci->fixed_lr[lr_idx]->data_addr;
        if (pdci->dma_width_bits == DMA_WIDTH_HALFWORD) // trim low 16bits
            src_addr += 2; // high 16bits address
        uint32_t dst_addr = (uint32_t) sample_data + buf_offset;
        uint32_t samp_len = pdci->samp_bits > 16 ? sample_cnt : sample_cnt/2;

        csk_gather_scatter_info_t scatter = { 0 };
        if (dst_scat) {
            scatter.enable = csk_func_enable;
            scatter.interval = SG_INTERVAL(dst_scat) << pdci->dma_width_bits;
            scatter.counter = SG_COUNT(dst_scat) << pdci->dma_width_bits;
        }
        DMA2D_Config_DstScatter(dma_ch, &scatter);

        stat = DMA2D_Start_Normal(dma_ch, (void*)src_addr, (void*)dst_addr, samp_len << pdci->dma_width_bits); // in byte
        if (stat != CSK_DRIVER_OK) {
            LOGD("%s: Failed to call GPDMA_Start_Normal, return %d!!\r\n", __func__, stat);
            return stat;
        }
    #else // !USE_GPDMA2D
        LOGD("%s: USE_GPDMA2D is defined as 0, do nothing for GPDMA2D read!\n", __func__);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    #endif

    } // end GPDMA2D is used

    pdci->busy_lr[lr_idx] = 1;
    return CSK_DRIVER_OK;
}


// read APC channel (sample_cnt is the count of uint16 or uint32, and SHOULD be EVEN when 16-bit sample!)
int32_t apc_channel_read (APC_CH    ch,
                          uint32_t *sample_data,
                          uint32_t  sample_cnt,
                          uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                          uint32_t dst_scat) // destination scatter setting (default 0, NO SG support)
{
    return apc_channel_read_internal (APC_CH_TO_DCH(ch),
                                      APC_CH_LR_BMP(ch),
                                      sample_data,
                                      sample_cnt,
                                      buf_offset,
                                      dst_scat);
}


// read APC dual_channel (sample_cnt is the count of uint16 or uint32, and SHOULD be EVEN when 16-bit sample!)
int32_t apc_dual_channel_read (APC_DCH  dch,
                              uint32_t  *sample_data,
                              uint32_t  sample_cnt,
                              uint32_t  buf_offset, // byte offset to each buffer (default 0, NO SG support)
                              uint32_t  dst_scat) // destination scatter setting (default 0, NO SG support)
{
    return apc_channel_read_internal (dch,
                                      APC_DCH_BMP_STEREO,
                                      sample_data,
                                      sample_cnt,
                                      buf_offset,
                                      dst_scat);
}


// read mixed data of APC channels (0,2), (1,2) or (4,2) (2 IN channels)
// sample_cnt is the count of uint16 or uint32, and SHOULD be EVEN when 16-bit sample!
int32_t apc_read_twi_channels (TWCH_TYPE twch_type, uint32_t* sample_data, uint32_t sample_cnt, bool init_chk)
{
    int32_t ret = CSK_DRIVER_OK;

    if (sample_data == NULL || sample_cnt < 2)
        return CSK_DRIVER_ERROR_PARAMETER;

    uint8_t i, dch_set[2];
    APC_DCH_INFO *pdci_set[2];

    switch (twch_type) {
    case TWCH_IN02:
    case TWCH_IN12:
        dch_set[0] = 0; dch_set[1] = 1;
        pdci_set[0] = &g_apc_dev.dch_array[0];
        pdci_set[1] = &g_apc_dev.dch_array[1];
        break;
    case TWCH_IN42:
        dch_set[0] = 2; dch_set[1] = 1;
        pdci_set[0] = &g_apc_dev.dch_array[2];
        pdci_set[1] = &g_apc_dev.dch_array[1];
        break;
    default:
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (init_chk) {
        if (!DCH_IS_OWNED(dch_set[0]) || !DCH_IS_OWNED(dch_set[1]))
            return CSK_DRIVER_ERROR_PARAMETER;

        if ((pdci_set[0]->ch_sel & APC_DCH_BMP_MASK) == APC_DCH_BMP_STEREO || pdci_set[0]->mix_mode == APC_DCH_MIXED_MODE ||
            (pdci_set[1]->ch_sel & APC_DCH_BMP_MASK) != APC_DCH_BMP_LEFT || pdci_set[1]->mix_mode != APC_DCH_SEPARATE_MODE   )
            return CSK_DRIVER_ERROR;

        if (pdci_set[0]->ch_mode == APC_CHMODE_16BITS || pdci_set[1]->ch_mode == APC_CHMODE_16BITS) { // dual_16bits as a word
            LOGD("%s: DON'T support dual_16bit channel mode!", __func__);
            return CSK_DRIVER_ERROR_PARAMETER;

        } else if (pdci_set[0]->dma_width_bits == DMA_WIDTH_HALFWORD && pdci_set[1]->dma_width_bits == DMA_WIDTH_HALFWORD) { // trim low 16bits
            pdci_set[0]->scat_gath_lr[0] = (1 << SG_INTERVAL_POS) | (1 << SG_COUNT_POS); // count = 1, interval = 1
            pdci_set[0]->sg_bytes_lr[0] = 2; // 1 halfword
            pdci_set[1]->scat_gath_lr[0] = (1 << SG_INTERVAL_POS) | (1 << SG_COUNT_POS); // count = 1, interval = 1
            pdci_set[1]->sg_bytes_lr[0] = 2; // 1 halfword

        } else if (pdci_set[0]->dma_width_bits == DMA_WIDTH_WORD && pdci_set[1]->dma_width_bits == DMA_WIDTH_WORD) { // 32bits (word)
            pdci_set[0]->scat_gath_lr[0] = (1 << SG_INTERVAL_POS) | (1 << SG_COUNT_POS); // count = 1, interval = 1
            pdci_set[0]->sg_bytes_lr[0] = 4; // 1 word
            pdci_set[1]->scat_gath_lr[0] = (1 << SG_INTERVAL_POS) | (1 << SG_COUNT_POS); // count = 1, interval = 1
            pdci_set[1]->sg_bytes_lr[0] = 4; // 1 word
        } else {
            LOGD("DON'T support %s for current configuration!", __func__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }

        pdci_set[0]->sg_offset_lr[0] = 0;
        pdci_set[1]->sg_offset_lr[0] = pdci_set[0]->sg_bytes_lr[0];
    } // init check

    // sample_cnt Should be even
    if (sample_cnt & 0x1) {
        //return CSK_DRIVER_ERROR_PARAMETER;
        LOGD("%s: sample count should be even!\r\n", __func__);
        sample_cnt &= ~0x1;
    }

    // trigger DMA transfer for 1st APC dual_channel
    uint8_t lr_bmp = APC_DCH_BMP_LEFT;
    if (twch_type == TWCH_IN12)
        lr_bmp = APC_DCH_BMP_RIGHT;
    ret = apc_channel_read_internal(dch_set[0], lr_bmp, sample_data, sample_cnt/2,
                                    pdci_set[0]->sg_offset_lr[0], pdci_set[0]->scat_gath_lr[0]);
    if (ret != CSK_DRIVER_OK) {
        apc_dual_channel_abort(dch_set[0]);
        return ret;
    }

    // trigger DMA transfer for 2nd APC dual_channel
    ret = apc_channel_read_internal(dch_set[1], APC_DCH_BMP_LEFT, sample_data, sample_cnt/2,
                                    pdci_set[1]->sg_offset_lr[0], pdci_set[1]->scat_gath_lr[0]);
    if (ret != CSK_DRIVER_OK) {
        apc_dual_channel_abort(dch_set[1]);
        apc_dual_channel_abort(dch_set[0]);
        return ret;
    }

    return CSK_DRIVER_OK;
}


// read mixed data of APC channels (0, 1, 2) or (4, 5, 2) (3 IN channels)
// sample_cnt is the count of uint16 or uint32, and SHOULD be EVEN when 16-bit sample!
int32_t apc_read_tri_channels (TCH_TYPE tch_type, uint32_t* sample_data, uint32_t sample_cnt, bool init_chk)
{
    int32_t ret = CSK_DRIVER_OK;

    if (sample_data == NULL || sample_cnt < 3)
        return CSK_DRIVER_ERROR_PARAMETER;

    uint8_t dch_set[2];
    APC_DCH_INFO *pdci_set[2], *pdci;

    switch (tch_type) {
    case TCH_IN012:
        dch_set[0] = 0; dch_set[1] = 1;
        pdci_set[0] = &g_apc_dev.dch_array[0];
        pdci_set[1] = &g_apc_dev.dch_array[1];
        break;
    case TCH_IN452:
        dch_set[0] = 2; dch_set[1] = 1;
        pdci_set[0] = &g_apc_dev.dch_array[2];
        pdci_set[1] = &g_apc_dev.dch_array[1];
        break;
    default:
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (init_chk) {
        if (!DCH_IS_OWNED(dch_set[0]) || !DCH_IS_OWNED(dch_set[1]))
            return CSK_DRIVER_ERROR_PARAMETER;

        if ((pdci_set[0]->ch_sel & APC_DCH_BMP_MASK) != APC_DCH_BMP_STEREO || pdci_set[0]->mix_mode != APC_DCH_MIXED_MODE ||
            (pdci_set[1]->ch_sel & APC_DCH_BMP_MASK) != APC_DCH_BMP_LEFT || pdci_set[1]->mix_mode != APC_DCH_SEPARATE_MODE   )
            return CSK_DRIVER_ERROR;

        if (pdci_set[0]->ch_mode == APC_CHMODE_16BITS || pdci_set[1]->ch_mode == APC_CHMODE_16BITS) { // dual_16bits as a word
            LOGD("%s: DON'T support dual_16bit channel mode!", __func__);
            return CSK_DRIVER_ERROR_PARAMETER;

        } else if (pdci_set[0]->dma_width_bits == DMA_WIDTH_HALFWORD && pdci_set[1]->dma_width_bits == DMA_WIDTH_HALFWORD) { // trim low 16bits
            pdci_set[0]->scat_gath_lr[0] = (1 << SG_INTERVAL_POS) | (2 << SG_COUNT_POS); // count = 2, interval = 1
            pdci_set[0]->sg_bytes_lr[0] = 4; // 2 halfwords
            pdci_set[1]->scat_gath_lr[0] = (2 << SG_INTERVAL_POS) | (1 << SG_COUNT_POS); // count = 1, interval = 2
            pdci_set[1]->sg_bytes_lr[0] = 2; // 1 halfword

        } else if (pdci_set[0]->dma_width_bits == DMA_WIDTH_WORD && pdci_set[1]->dma_width_bits == DMA_WIDTH_WORD) { // 32bits (word)
            pdci_set[0]->scat_gath_lr[0] = (1 << SG_INTERVAL_POS) | (2 << SG_COUNT_POS); // count = 2, interval = 1
            pdci_set[0]->sg_bytes_lr[0] = 8; // 2 words
            pdci_set[1]->scat_gath_lr[0] = (2 << SG_INTERVAL_POS) | (1 << SG_COUNT_POS); // count = 1, interval = 2
            pdci_set[1]->sg_bytes_lr[0] = 4; // 1 word
        } else {
            LOGD("DON'T support %s for current configuration!", __func__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }

        pdci_set[0]->sg_offset_lr[0] = 0;
        pdci_set[1]->sg_offset_lr[0] = pdci_set[0]->sg_bytes_lr[0];
    } // init check

    // sample_cnt Should be multiple of 3
    if (sample_cnt % 0x3) {
        LOGD("%s: sample count should be multiple of 3!\r\n", __func__);
        //sample_cnt = sample_cnt / 3 * 3;
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    // trigger DMA transfer for each APC dual_channel
    sample_cnt /= 3;
    ret = apc_channel_read_internal(dch_set[0], APC_DCH_BMP_STEREO, sample_data, sample_cnt << 0x1,
                                    pdci_set[0]->sg_offset_lr[0], pdci_set[0]->scat_gath_lr[0]);
    if (ret != CSK_DRIVER_OK) {
        apc_dual_channel_abort(dch_set[0]);
        return ret;
    }

    ret = apc_channel_read_internal(dch_set[1], APC_DCH_BMP_LEFT, sample_data, sample_cnt,
                                    pdci_set[1]->sg_offset_lr[0], pdci_set[1]->scat_gath_lr[0]);
    if (ret != CSK_DRIVER_OK) {
        apc_dual_channel_abort(dch_set[1]);
        apc_dual_channel_abort(dch_set[0]);
        return ret;
    }

    return CSK_DRIVER_OK;
}


// read mixed data of APC channels (0 ~ 3) or (4,5,2,3) (4 IN channels)
// sample_cnt is the count of uint16 or uint32, and SHOULD be EVEN when 16-bit sample!
int32_t apc_read_quad_channels (QCH_TYPE qch_type, uint32_t* sample_data, uint32_t sample_cnt, bool init_chk)
{
//    uint32_t dst_scat, dch_bytes;
    uint32_t dch_bytes = 0;
    int32_t ret = CSK_DRIVER_OK;

    if (sample_data == NULL || sample_cnt < 4)
        return CSK_DRIVER_ERROR_PARAMETER;

    uint8_t i, dch_set[2];
    APC_DCH_INFO *pdci_set[2], *pdci;

    switch (qch_type) {
    case QCH_DCH_IN0_IN1:
        dch_set[0] = 0; dch_set[1] = 1;
        pdci_set[0] = &g_apc_dev.dch_array[0];
        pdci_set[1] = &g_apc_dev.dch_array[1];
        break;
    case QCH_DCH_IN2_IN1:
        dch_set[0] = 2; dch_set[1] = 1;
        pdci_set[0] = &g_apc_dev.dch_array[2];
        pdci_set[1] = &g_apc_dev.dch_array[1];
        break;
    default:
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (init_chk) {
    if (!DCH_IS_OWNED(dch_set[0]) || !DCH_IS_OWNED(dch_set[1]))
        return CSK_DRIVER_ERROR_PARAMETER;
    for (i=0; i<2; i++) {
        pdci = pdci_set[i];
        if ((pdci->ch_sel & APC_DCH_BMP_MASK) != APC_DCH_BMP_STEREO || pdci->mix_mode != APC_DCH_MIXED_MODE)
            return CSK_DRIVER_ERROR;

        if (pdci->ch_mode == APC_CHMODE_16BITS) {
            // count = 1, interval = 1
            pdci->scat_gath_lr[0] = (1 << SG_INTERVAL_POS) | (1 << SG_COUNT_POS);
            pdci->sg_bytes_lr[0] = 4; // 1 word
        } else if (pdci->dma_width_bits == DMA_WIDTH_HALFWORD) { // trim low 16bits
            // count = 2, interval = 2
            pdci->scat_gath_lr[0] = (2 << SG_INTERVAL_POS) | (2 << SG_COUNT_POS);
            pdci->sg_bytes_lr[0] = 4; // 2 halfwords
        } else {
            // count = 2, interval = 2
            pdci->scat_gath_lr[0] = (2 << SG_INTERVAL_POS) | (2 << SG_COUNT_POS);
            pdci->sg_bytes_lr[0] = 8; // 2 words
        }

        pdci->sg_offset_lr[0] = dch_bytes;
        dch_bytes += pdci->sg_bytes_lr[0];
    } // end for
    } // init check

    // sample_cnt Should be multiple of 4
    if (sample_cnt & 0x3) {
        //return CSK_DRIVER_ERROR_PARAMETER;
        LOGD("%s: sample count should be multiple of 4!\r\n", __func__);
        sample_cnt &= ~0x3;
    }

    // trigger DMA transfer for 1st APC dual_channel
    ret = apc_channel_read_internal(dch_set[0], APC_DCH_BMP_STEREO, sample_data, sample_cnt/2,
                                    pdci_set[0]->sg_offset_lr[0], pdci_set[0]->scat_gath_lr[0]);
    if (ret != CSK_DRIVER_OK) {
        apc_dual_channel_abort(dch_set[0]);
        return ret;
    }

    // trigger DMA transfer for 2nd APC dual_channel
    ret = apc_channel_read_internal(dch_set[1], APC_DCH_BMP_STEREO, sample_data, sample_cnt/2,
                                    pdci_set[1]->sg_offset_lr[0], pdci_set[1]->scat_gath_lr[0]);
    if (ret != CSK_DRIVER_OK) {
        apc_dual_channel_abort(dch_set[1]);
        apc_dual_channel_abort(dch_set[0]);
        return ret;
    }

    return CSK_DRIVER_OK;
}

#if USE_CMNDMA
static int32_t
apc_channel_dma_write_inclusive(APC_DCH  dch,
                          uint8_t lr_idx,
                          const uint32_t *sample_data,
                          uint32_t  sample_cnt,
                          uint32_t  buf_offset, // byte offset to each buffer (default 0, NO SG support)
                          uint32_t  src_gath) // source gather setting (default 0, NO SG support)
{
    int ret = CSK_DRIVER_OK;
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];

    //uint32_t usr_param = (dch << 8) | (pdci->mix_mode ? APC_DCH_BMP_STEREO : APC_DCH_BMP_LEFT);
    uint32_t usr_param = (dch << 8) | (pdci->mix_mode ? APC_DCH_BMP_STEREO : (0x1 << lr_idx));
    uint8_t *dma_ch_p = &pdci->dma_ch_lr[lr_idx];

    // select some free DMA channel
    *dma_ch_p = dma_channel_select( dma_ch_p,
                                    apc_cmndma_event,
                                    usr_param,
                (pdci->rt_flag_lr[lr_idx] & RT_FLAG_NSYNCA) ? DMA_CACHE_SYNC_NOP : DMA_CACHE_SYNC_SRC);
    if (*dma_ch_p == DMA_CHANNEL_ANY) {
        LOGD("%s: NO free DMA channel!!\r\n", __func__);
        return CSK_DRIVER_ERROR;
    }

    // setup DMA channel
    //NOTE: VENUSA CMNDMA has 2 master ports, and they are exactly the same. see DMS/SMS setting.
    uint32_t control, config_low, config_high;
    uint8_t dma_hsid = pdci->fixed_lr[lr_idx]->dma_hsid;
    control = DMA_CH_CTLL_DST_WIDTH(pdci->dma_width_bits) | DMA_CH_CTLL_SRC_WIDTH(pdci->dma_width_bits) |
            DMA_CH_CTLL_DST_BSIZE(pdci->dma_bsize_bits) | DMA_CH_CTLL_SRC_BSIZE(pdci->dma_bsize_bits) |
            DMA_CH_CTLL_DST_FIX | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_TTFC_M2P |
            DMA_CH_CTLL_DMS(1) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN | (src_gath ? DMA_CH_CTLL_S_GATH_EN : 0);
    config_low = DMA_CH_CFGL_CH_PRIOR(1); // channel priority is higher than 0
    config_high = DMA_CH_CFGH_DST_PER(dma_hsid);// DMA_CH_CFGH_PROTCTL(1) | DMA_CH_CFGH_FIFO_MODE |

    ret = dma_channel_setup (*dma_ch_p, DMA_CH_EN_XFER_INT, control, config_low, config_high, src_gath, 0);
    if (ret < 0) {
        LOGD("%s: Failed to call dma_channel_setup!!\r\n", __func__);
        dma_channel_disable(*dma_ch_p, false);
        //*dma_ch_p = DMA_CHANNEL_ANY;
        return ret;
    }

    //set_cmndma_hs(dma_hsid, pdci->fixed_lr[lr_idx]->dma_hssel); // hs_sel = 1
    set_cmndma_hs(dma_hsid, 1); // hs_sel = 1

    // start DMA transfer
    uint32_t src_addr = pdci->fixed_lr[lr_idx]->data_addr;
    if (pdci->dma_width_bits == DMA_WIDTH_HALFWORD) // trim low 16bits
        src_addr += 2; // high 16bits address

    ret = dma_channel_start_block (*dma_ch_p,
                        DMACH_CFG_FLAG_BOTH_ADDR,
                        (uint32_t) sample_data + buf_offset,
                        src_addr,
                        pdci->samp_bits > 16 ? sample_cnt : sample_cnt/2);
   if (ret < 0) {
        //LOGD("%s: Failed to call dma_channel_start_block!!\r\n", __func__);
        dma_channel_disable(*dma_ch_p, false);
        //*dma_ch_p = DMA_CHANNEL_ANY;
        return ret;
    }

    return ret;
}
#endif

// sample_cnt SHOULD be EVEN when 16-bit sample!
static int32_t
apc_channel_write_internal (APC_DCH dch,
                           uint8_t  lr_bmp,
                           const uint32_t *sample_data,
                           uint32_t sample_cnt,
                           uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                           uint32_t src_gath) // source gather setting (default 0, NO SG support)

{
    // Check if parameters are valid, i.e. APC channel is valid
    if (dch < APC_DCH_OUT_MIN || dch > APC_DCH_OUT_MAX || !DCH_IS_OWNED(dch) || sample_data == NULL || sample_cnt == 0)
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check if APC and specified dual_channel are both initialized
    if (!IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
        return CSK_DRIVER_ERROR;

    lr_bmp &= APC_DCH_BMP_MASK;
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    // 0 = left, 1 = right (use left or right channel for dual_channel)
    uint8_t lr_idx = (lr_bmp & APC_DCH_BMP_LEFT)? 0 : 1;
    //if (pdci->mix_target == 1) // mixed @ right channel
    //    lr_idx = 1;

    // sample_cnt should be even when 16-bit sample
    if (pdci->ch_mode == APC_CHMODE_16BITS && (sample_cnt & 0x1) != 0)
        return CSK_DRIVER_ERROR_PARAMETER;

    // for single channel, Only MONO_MODE (separated L/R channel data) is supported
    // for dual channel, Only STEREO_MODE (mixed L/R channel data) is supported
    if ((lr_bmp != APC_DCH_BMP_STEREO && pdci->mix_mode == APC_DCH_MIXED_MODE) ||
        (lr_bmp == APC_DCH_BMP_STEREO && pdci->mix_mode == APC_DCH_SEPARATE_MODE))
        return CSK_DRIVER_ERROR;

    // Only DAC / I2S OUT interface can be written
    if (pdci->intf_type != APC_INTF_DAC &&
        pdci->intf_type != APC_INTF_I2S_OUT)
        return CSK_DRIVER_ERROR;

    // just return busy if the channel is busy
    if (pdci->busy_lr[lr_idx] != 0)
        return CSK_DRIVER_ERROR_BUSY;

    pdci->samps_lr[lr_idx] = 0;
    uint8_t *dma_ch_p = &pdci->dma_ch_lr[lr_idx];

    if (pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_PIO) {
    #if SUPPORT_APC_PIO
        pdci->sambuf_lr[lr_idx] = (uint32_t *)sample_data;
        pdci->samcnt_lr[lr_idx] = sample_cnt;
        pdci->busy_lr[lr_idx] = 1;
        return CSK_DRIVER_OK;
    #else
        LOGD("%s: SUPPORT_APC_PIO is defined as 0, do nothing for PIO read!\n", __func__);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    #endif
    }

    // DMA (CMNDMA or GPDMA2D) is used
    uint8_t dma_ch = pdci->dma_ch_lr[lr_idx];
    uint8_t dma_hsid = pdci->fixed_lr[lr_idx]->dma_hsid;
    int32_t stat;

    if ((pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_GPDMA) == 0) {// CMNDMA is used
    #if USE_CMNDMA
        if (dma_channel_check_select(dma_ch, DMA_TT_M2P, dma_hsid)) {
            stat = dma_channel_start_block(dma_ch, DMACH_CFG_FLAG_SRC_ADDR,
                                    (uint32_t) sample_data + buf_offset, 0,
                                    pdci->samp_bits > 16 ? sample_cnt : sample_cnt/2);
            if (stat < 0) {
                //LOGD("%s: Failed to call dma_channel_start_block!!\r\n", __func__);
                return stat;
            }
        } else {
            stat = apc_channel_dma_write_inclusive(dch, lr_idx, sample_data, sample_cnt, buf_offset, src_gath);
            if (stat < 0) {
                //LOGD("%s: Failed to call apc_channel_dma_read_inclusive!!\r\n", __func__);
                return stat;
            }
        }
    #else // !USE_CMNDMA
        LOGD("%s: USE_CMNDMA is defined as 0, do nothing for CMNDMA write!\n", __func__);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    #endif

    } else { // GPDMA2D is used
#if USE_GPDMA2D
    uint32_t dst_addr = pdci->fixed_lr[lr_idx]->data_addr;
    if (pdci->dma_width_bits == DMA_WIDTH_HALFWORD) // trim low 16bits
        dst_addr += 2; // high 16bits address
    uint32_t src_addr = (uint32_t) sample_data + buf_offset;
    uint32_t samp_len = pdci->samp_bits > 16 ? sample_cnt : sample_cnt/2;

    csk_gather_scatter_info_t gather = { 0 };
    if (src_gath) {
        gather.enable = csk_func_enable;
        gather.interval = SG_INTERVAL(src_gath) << pdci->dma_width_bits;
        gather.counter = SG_COUNT(src_gath) << pdci->dma_width_bits;
    }
    DMA2D_Config_SrcGather(dma_ch, &gather);

    stat = DMA2D_Start_Normal(dma_ch, (void*)src_addr, (void*)dst_addr, samp_len << pdci->dma_width_bits); // in byte
    if (stat != CSK_DRIVER_OK) {
        LOGD("%s: Failed to call GPDMA_Start_Normal!!\r\n", __func__);
        return stat;
    }
#else // !USE_GPDMA2D
    LOGD("%s: USE_GPDMA2D is defined as 0, do nothing for GPDMA2D write!\n", __func__);
    return CSK_DRIVER_ERROR_UNSUPPORTED;
#endif

    } //end GPDMA2D is used

    pdci->busy_lr[lr_idx] = 1;
    return CSK_DRIVER_OK;
}


// write APC channel (sample_cnt is the count of uint16 or uint32, and SHOULD be EVEN when 16-bit sample!)
int32_t apc_channel_write (APC_CH   ch,
                           const uint32_t *sample_data,
                           uint32_t sample_cnt,
                           uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                           uint32_t src_gath) // source gather setting (default 0, NO SG support)
{
    return apc_channel_write_internal (APC_CH_TO_DCH(ch),
                                       APC_CH_LR_BMP(ch),
                                       sample_data,
                                       sample_cnt,
                                       buf_offset,
                                       src_gath);
}


// write APC dual_channel (sample_cnt is the count of uint16 or uint32, and SHOULD be EVEN when 16-bit sample!)
int32_t apc_dual_channel_write (APC_DCH dch,
                               const uint32_t *sample_data,
                               uint32_t sample_cnt,
                               uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                               uint32_t src_gath) // source gather setting (default 0, NO SG support)
{
    return apc_channel_write_internal (dch,
                                       APC_DCH_BMP_STEREO,
                                       sample_data,
                                       sample_cnt,
                                       buf_offset,
                                       src_gath);
}

#if USE_CMNDMA
// FOR PingPong Transfer (IN)
static int32_t apc_dch_read_pipo_inclusive( APC_DCH dch,
                                            uint8_t lr_idx,
                                            PIPO_IN_BLOCK *aud_blks,
                                            uint8_t *blk_cnt_p,
                                            uint32_t  buf_offset, // byte offset to each buffer (default 0, NO SG support)
                                            uint32_t  dst_scat) // destination scatter setting (default 0, NO SG support)

{
    int ret = CSK_DRIVER_OK;
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];

    //uint32_t usr_param = (dch << 8) | (pdci->mix_mode ? APC_DCH_BMP_STEREO : APC_DCH_BMP_LEFT);
    uint32_t usr_param = (dch << 8) | (pdci->mix_mode ? APC_DCH_BMP_STEREO : (0x1 << lr_idx));
    uint8_t *dma_ch_p = &pdci->dma_ch_lr[lr_idx];

    // select some free DMA channel
    *dma_ch_p = dma_channel_select(dma_ch_p,
                                    apc_cmndma_event,
                                    usr_param,
                (pdci->rt_flag_lr[lr_idx] & RT_FLAG_NSYNCA) ? DMA_CACHE_SYNC_NOP : DMA_CACHE_SYNC_DST);
    if (*dma_ch_p == DMA_CHANNEL_ANY) {
        LOGD("%s: NO free DMA channel!!\r\n", __func__);
        return CSK_DRIVER_ERROR;
    }

    // setup DMA channel
    //NOTE: VENUSA CMNDMA has 2 master ports, and they are exactly the same. see DMS/SMS setting.
    uint32_t control, config_low, config_high;
    uint8_t dma_hsid = pdci->fixed_lr[lr_idx]->dma_hsid;
    control = DMA_CH_CTLL_DST_WIDTH(pdci->dma_width_bits) | DMA_CH_CTLL_SRC_WIDTH(pdci->dma_width_bits) |
            DMA_CH_CTLL_DST_BSIZE(pdci->dma_bsize_bits) | DMA_CH_CTLL_SRC_BSIZE(pdci->dma_bsize_bits) |
            DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_TTFC_P2M |
            DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(1) | DMA_CH_CTLL_INT_EN | (dst_scat ? DMA_CH_CTLL_D_SCAT_EN : 0);
    config_low = DMA_CH_CFGL_CH_PRIOR(1); // channel priority is higher than 0
    config_high = DMA_CH_CFGH_SRC_PER(dma_hsid); // DMA_CH_CFGH_PROTCTL(1) | DMA_CH_CFGH_FIFO_MODE |

    ret = dma_channel_setup (*dma_ch_p, DMA_CH_EN_XFER_INT | DMA_CH_EN_BLK_INT | DMA_CH_EN_PIPO,
                            control, config_low, config_high, 0, dst_scat);
    if (ret < 0) {
        LOGD("%s: Failed to call dma_channel_setup!!\r\n", __func__);
        dma_channel_disable(*dma_ch_p, false);
        //*dma_ch_p = DMA_CHANNEL_ANY;
        return ret;
    }

    //set_cmndma_hs(dma_hsid, pdci->fixed_lr[lr_idx]->dma_hssel); // hs_sel = 1
    set_cmndma_hs(dma_hsid, 1); // hs_sel = 1

    // start DMA transfer
    uint32_t src_addr = pdci->fixed_lr[lr_idx]->data_addr;
    if (pdci->dma_width_bits == DMA_WIDTH_HALFWORD) // trim low 16bits
        src_addr += 2; // high 16bits address

    for (uint8_t i = 0; i < *blk_cnt_p; i++) {
        aud_blks[i].reserved = src_addr;
        //FIXME: adjust start position of buffer & sample count
        aud_blks[i].sample_data = (uint8_t *)aud_blks[i].sample_data + buf_offset;
        if (pdci->samp_bits == 16)
            aud_blks[i].sample_cnt >>= 1;
    }

    pdci->samps_lr[lr_idx] = 0; //FIXME: for mixed mode, set samps for another channel?
    ret = dma_channel_start_pipo (*dma_ch_p, (DMA_PIPO_BLK *)aud_blks, blk_cnt_p);
    if (ret < 0) {
        //LOGD("%s: Failed to call dma_channel_start_pipo!!\r\n", __func__);
        dma_channel_disable(*dma_ch_p, false);
        //*dma_ch_p = DMA_CHANNEL_ANY;
        return ret;
    }

    return ret;
}
#endif

// sample_cnt SHOULD be EVEN when 16-bit sample!
int32_t
apc_dch_read_pipo (APC_DCH dch, uint8_t lr_bmp,
                    PIPO_IN_BLOCK *aud_blks, uint8_t *blk_cnt_p,
                    uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                    uint32_t dst_scat) // destination scatter setting (default 0, NO SG support)
{
    // Check if parameters are valid, i.e. APC channel is valid
    if (dch < APC_DCH_IN_MIN || dch > APC_DCH_IN_MAX || !DCH_IS_OWNED(dch) ||
        aud_blks == NULL || blk_cnt_p == NULL || *blk_cnt_p > 2)
        return CSK_DRIVER_ERROR_PARAMETER;

    lr_bmp &= APC_DCH_BMP_MASK;

    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    // 0 = left, 1 = right (use left or right channel for dual_channel)
    uint8_t lr_idx = (lr_bmp & APC_DCH_BMP_LEFT)? 0 : 1;
    uint8_t use_gpdma = (pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_GPDMA);
    uint8_t dma_ch = pdci->dma_ch_lr[lr_idx];
    uint8_t dma_hsid = pdci->fixed_lr[lr_idx]->dma_hsid;
    int32_t stat;

    uint8_t full_chk = (pdci->busy_lr[lr_idx] == 0);
    if (full_chk) {
        // Check if APC and specified dual_channel are both initialized
        if (!IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
            return CSK_DRIVER_ERROR;

        if (pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_PIO) {
            // PingPong mode is only supported for DMA!!
            return CSK_DRIVER_ERROR_UNSUPPORTED;
        }
        // for single channel, Only MONO_MODE (separated L/R channel data) is supported
        // for dual channel, Only STEREO_MODE (mixed L/R channel data) is supported
        if ((lr_bmp != APC_DCH_BMP_STEREO && pdci->mix_mode == APC_DCH_MIXED_MODE) ||
            (lr_bmp == APC_DCH_BMP_STEREO && pdci->mix_mode == APC_DCH_SEPARATE_MODE))
            return CSK_DRIVER_ERROR;

        // Only ADC / DMIC / I2S IN / ECHO interface can be read
        if (pdci->intf_type != APC_INTF_ADC_PDM &&
            pdci->intf_type != APC_INTF_I2S_IN &&
            pdci->intf_type != APC_INTF_ECHO)
            return CSK_DRIVER_ERROR;

    #if USE_GPDMA2D //FIXME: use DMA2D API instead of original GPDMA!!
        // Configure Scatter/Gather only once (when started)?
        if (use_gpdma) {
            csk_gather_scatter_info_t scatter = { 0 };
            if (dst_scat) {
                scatter.enable = csk_func_enable;
                scatter.interval = SG_INTERVAL(dst_scat) << pdci->dma_width_bits;
                scatter.counter = SG_COUNT(dst_scat) << pdci->dma_width_bits;
            }
            DMA2D_Config_DstScatter(dma_ch, &scatter);
        }
    #endif

    } // full_chk

    // sample_cnt should be even when 16-bit sample
    if (pdci->ch_mode == APC_CHMODE_16BITS) {
        for (uint8_t i = 0; i < *blk_cnt_p; i++) {
            if (aud_blks[i].sample_cnt & 0x1)
                return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    // just return busy if the channel is busy (Don't check busy if NOT pipo_both)
    //if (pdci->busy_lr[lr_idx] != 0)
    //    return CSK_DRIVER_ERROR_BUSY;

    if (use_gpdma == 0) { // CMNDMA is used
    #if USE_CMNDMA
        if (dma_channel_check_select(dma_ch, DMA_TT_P2M | DMA_XFER_FLAG_PIPO, dma_hsid)) {
            //FIXME: adjust start position of buffer & sample count
            for (uint8_t i = 0; i < *blk_cnt_p; i++) {
                aud_blks[i].sample_data = (uint8_t *)aud_blks[i].sample_data + buf_offset;
                if (pdci->samp_bits == 16)
                    aud_blks[i].sample_cnt >>= 1;
            }
            //LOGD("%s: call dma_channel_start_pipo!\n", __func__);
            stat = dma_channel_start_pipo(dma_ch, (DMA_PIPO_BLK *)aud_blks, blk_cnt_p);
            if (stat < 0) {
                //LOGD("%s: Failed to call dma_channel_start_pipo!!\r\n", __func__);
                return stat;
            }
        } else {
            //LOGD("%s: call apc_dch_read_pipo_inclusive!\n", __func__);
            stat = apc_dch_read_pipo_inclusive(dch, lr_idx, aud_blks, blk_cnt_p, buf_offset, dst_scat);
            if (stat < 0) {
                //LOGD("%s: Failed to call apc_dch_read_pipo_inclusive!!\r\n", __func__);
                return stat;
            }
        }
    #else // !USE_CMNDMA
        LOGD("%s: USE_CMNDMA is defined as 0, do nothing for CMNDMA read!\n", __func__);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    #endif

    } else { // GPDMA2D is used
    #if USE_GPDMA2D
        uint32_t src_addr = pdci->fixed_lr[lr_idx]->data_addr;
        if (pdci->dma_width_bits == DMA_WIDTH_HALFWORD) // trim low 16bits
            src_addr += 2; // high 16bits address
        uint32_t dst_addr = (uint32_t) aud_blks[0].sample_data + buf_offset;
        uint32_t len_bytes = aud_blks[0].sample_cnt << pdci->dma_width_bits;
        //if (pdci->samp_bits == 16)  len_bytes >>= 1;

        uint8_t pipo_both = (*blk_cnt_p == 2);
        if (pipo_both) { // first call of read_pipo
            pdci->samps_lr[lr_idx] = 0;
            uint32_t dst_addr2 = (uint32_t) aud_blks[1].sample_data + buf_offset;
            uint32_t len2_bytes = aud_blks[1].sample_cnt << pdci->dma_width_bits;
            //if (pdci->samp_bits == 16)  len2_bytes >>= 1;
            //NOTE: Double DMA2D_Start_Normal are called to trigger PingPong operations on GPDMA2D.
            // In PingPong mode the transfer length SHOULD NOT be more than the maximum block size (2^24 bytes)!
            stat = DMA2D_Start_Normal(dma_ch, (void*)src_addr, (void*)dst_addr, len_bytes); //ping
            stat = DMA2D_Start_Normal(dma_ch, (void*)src_addr, (void*)dst_addr2, len2_bytes); //pong
        } else { // subsequent calls of read_pipo
            stat = DMA2D_Start_Normal(dma_ch, (void*)src_addr, (void*)dst_addr, len_bytes); //ping or pong
        }
        if (stat != CSK_DRIVER_OK) {
            LOGD("%s: Failed to Start DMA2D PiPo!!\r\n", __func__);
            return stat;
        }
    #else // !USE_GPDMA2D
        LOGD("%s: USE_GPDMA2D is defined as 0, do nothing for GPDMA2D read!\n", __func__);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    #endif

    } // end GPDMA2D is used

    pdci->busy_lr[lr_idx] = 1;
    return CSK_DRIVER_OK;
}

//NOTE: GPDMA2D may differ with CMNDMA(DWDMA) on Scatter/Gather data unit length!!

// read mixed data of APC channels (2 IN channels, NOT dual_channel) in PingPong mode
int32_t apc_read_twi_channels_pipo (TWCH_TYPE twch_type, PIPO_IN_BLOCK *aud_blks, uint8_t *blk_cnt_p, bool init_chk)
{
    int32_t ret = CSK_DRIVER_OK;

    if (aud_blks == NULL || blk_cnt_p == NULL || *blk_cnt_p > MAX_PIPO_BLKS)
        return CSK_DRIVER_ERROR_PARAMETER;

    uint8_t j, blk_cnt, dch_set[2];
    APC_DCH_INFO *pdci_set[2];
    PIPO_IN_BLOCK aud_blks_bk[MAX_PIPO_BLKS];

    switch (twch_type) {
    case TWCH_IN02:
    case TWCH_IN12:
        dch_set[0] = 0; dch_set[1] = 1;
        pdci_set[0] = &g_apc_dev.dch_array[0];
        pdci_set[1] = &g_apc_dev.dch_array[1];
        break;
    case TWCH_IN42:
        dch_set[0] = 2; dch_set[1] = 1;
        pdci_set[0] = &g_apc_dev.dch_array[2];
        pdci_set[1] = &g_apc_dev.dch_array[1];
        break;
    default:
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (init_chk) {
        if (!DCH_IS_OWNED(dch_set[0]) || !DCH_IS_OWNED(dch_set[1]))
            return CSK_DRIVER_ERROR_PARAMETER;

        if ((pdci_set[0]->ch_sel & APC_DCH_BMP_MASK) == APC_DCH_BMP_STEREO || pdci_set[0]->mix_mode == APC_DCH_MIXED_MODE ||
            (pdci_set[1]->ch_sel & APC_DCH_BMP_MASK) != APC_DCH_BMP_LEFT || pdci_set[1]->mix_mode != APC_DCH_SEPARATE_MODE   )
            return CSK_DRIVER_ERROR;

        if (pdci_set[0]->ch_mode == APC_CHMODE_16BITS || pdci_set[1]->ch_mode == APC_CHMODE_16BITS) { // dual_16bits as a word
            LOGD("%s: DON'T support dual_16bit channel mode!", __func__);
            return CSK_DRIVER_ERROR_PARAMETER;

        } else if (pdci_set[0]->dma_width_bits == DMA_WIDTH_HALFWORD && pdci_set[1]->dma_width_bits == DMA_WIDTH_HALFWORD) { // trim low 16bits
            pdci_set[0]->scat_gath_lr[0] = (1 << SG_INTERVAL_POS) | (1 << SG_COUNT_POS); // count = 1, interval = 1
            pdci_set[0]->sg_bytes_lr[0] = 2; // 1 halfword
            pdci_set[1]->scat_gath_lr[0] = (1 << SG_INTERVAL_POS) | (1 << SG_COUNT_POS); // count = 1, interval = 1
            pdci_set[1]->sg_bytes_lr[0] = 2; // 1 halfword

        } else if (pdci_set[0]->dma_width_bits == DMA_WIDTH_WORD && pdci_set[1]->dma_width_bits == DMA_WIDTH_WORD) { // 32bits (word)
            pdci_set[0]->scat_gath_lr[0] = (1 << SG_INTERVAL_POS) | (1 << SG_COUNT_POS); // count = 1, interval = 1
            pdci_set[0]->sg_bytes_lr[0] = 4; // 1 word
            pdci_set[1]->scat_gath_lr[0] = (1 << SG_INTERVAL_POS) | (1 << SG_COUNT_POS); // count = 1, interval = 1
            pdci_set[1]->sg_bytes_lr[0] = 4; // 1 word
        } else {
            LOGD("DON'T support %s for current configuration!", __func__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }

        pdci_set[0]->sg_offset_lr[0] = 0;
        pdci_set[1]->sg_offset_lr[0] = pdci_set[0]->sg_bytes_lr[0];
    } // init check

    blk_cnt = *blk_cnt_p;
    for (j=0; j<blk_cnt; j++) {
        // sample_cnt Should be even
        if (aud_blks[j].sample_cnt & 0x1) {
            LOGD("%s: sample count should be EVEN!\r\n", __func__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
        // sample_cnt / 2 for read_pipo half of total sample count
        aud_blks[j].sample_cnt >>= 0x1;
    }
    memcpy(aud_blks_bk, aud_blks, sizeof(PIPO_IN_BLOCK) * blk_cnt);

    // trigger DMA transfer for 1st APC dual_channel
    uint8_t lr_bmp = APC_DCH_BMP_LEFT;
    if (twch_type == TWCH_IN12)
        lr_bmp = APC_DCH_BMP_RIGHT;
    ret = apc_dch_read_pipo(dch_set[0], lr_bmp, aud_blks, blk_cnt_p,
                           pdci_set[0]->sg_offset_lr[0], pdci_set[0]->scat_gath_lr[0]);
    if (ret != CSK_DRIVER_OK) {
        apc_dual_channel_abort(dch_set[0]);
        return ret;
    }

    // trigger DMA transfer for 2nd APC dual_channel
    ret = apc_dch_read_pipo(dch_set[1], APC_DCH_BMP_LEFT, aud_blks_bk, &blk_cnt,
                           pdci_set[1]->sg_offset_lr[0], pdci_set[1]->scat_gath_lr[0]);
    if (ret != CSK_DRIVER_OK) {
        apc_dual_channel_abort(dch_set[1]);
        apc_dual_channel_abort(dch_set[0]);
        return ret;
    }

    return CSK_DRIVER_OK;
}


// read mixed data of APC channels (0, 1, 2) or (4, 5, 2) (3 IN channels) in PingPong mode
int32_t apc_read_tri_channels_pipo (TCH_TYPE tch_type, PIPO_IN_BLOCK *aud_blks, uint8_t *blk_cnt_p, bool init_chk)
{
    int32_t ret = CSK_DRIVER_OK;

    if (aud_blks == NULL || blk_cnt_p == NULL || *blk_cnt_p > MAX_PIPO_BLKS)
        return CSK_DRIVER_ERROR_PARAMETER;

    uint8_t j, blk_cnt, dch_set[2];
    APC_DCH_INFO *pdci_set[2], *pdci;
    PIPO_IN_BLOCK aud_blks_bk[MAX_PIPO_BLKS];

    switch (tch_type) {
    case TCH_IN012:
        dch_set[0] = 0; dch_set[1] = 1;
        pdci_set[0] = &g_apc_dev.dch_array[0];
        pdci_set[1] = &g_apc_dev.dch_array[1];
        break;
    case TCH_IN452:
        dch_set[0] = 2; dch_set[1] = 1;
        pdci_set[0] = &g_apc_dev.dch_array[2];
        pdci_set[1] = &g_apc_dev.dch_array[1];
        break;
    default:
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (init_chk) {
        if ((pdci_set[0]->ch_sel & APC_DCH_BMP_MASK) != APC_DCH_BMP_STEREO || pdci_set[0]->mix_mode != APC_DCH_MIXED_MODE ||
            (pdci_set[1]->ch_sel & APC_DCH_BMP_MASK) != APC_DCH_BMP_LEFT || pdci_set[1]->mix_mode != APC_DCH_SEPARATE_MODE   )
            return CSK_DRIVER_ERROR;

        if (pdci_set[0]->ch_mode == APC_CHMODE_16BITS || pdci_set[1]->ch_mode == APC_CHMODE_16BITS) { // dual_16bits as a word
            LOGD("%s: DON'T support dual_16bit channel mode!", __func__);
            return CSK_DRIVER_ERROR_PARAMETER;

        } else if (pdci_set[0]->dma_width_bits == DMA_WIDTH_HALFWORD && pdci_set[1]->dma_width_bits == DMA_WIDTH_HALFWORD) { // trim low 16bits
            pdci_set[0]->scat_gath_lr[0] = (1 << SG_INTERVAL_POS) | (2 << SG_COUNT_POS); // count = 2, interval = 1
            pdci_set[0]->sg_bytes_lr[0] = 4; // 2 halfwords
            pdci_set[1]->scat_gath_lr[0] = (2 << SG_INTERVAL_POS) | (1 << SG_COUNT_POS); // count = 1, interval = 2
            pdci_set[1]->sg_bytes_lr[0] = 2; // 1 halfword

        } else if (pdci_set[0]->dma_width_bits == DMA_WIDTH_WORD && pdci_set[1]->dma_width_bits == DMA_WIDTH_WORD) { // 32bits (word)
            pdci_set[0]->scat_gath_lr[0] = (1 << SG_INTERVAL_POS) | (2 << SG_COUNT_POS); // count = 2, interval = 1
            pdci_set[0]->sg_bytes_lr[0] = 8; // 2 words
            pdci_set[1]->scat_gath_lr[0] = (2 << SG_INTERVAL_POS) | (1 << SG_COUNT_POS); // count = 1, interval = 2
            pdci_set[1]->sg_bytes_lr[0] = 4; // 1 word
        } else {
            LOGD("DON'T support %s for current configuration!", __func__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }

        pdci_set[0]->sg_offset_lr[0] = 0;
        pdci_set[1]->sg_offset_lr[0] = pdci_set[0]->sg_bytes_lr[0];
    } // init check

    blk_cnt = *blk_cnt_p;
    for (j=0; j<blk_cnt; j++) {
        // sample_cnt Should be multiple of 3
        if (aud_blks[j].sample_cnt % 0x3) {
            LOGD("%s: sample count should be multiple of 3!\r\n", __func__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
        // sample_cnt x 2 / 3 for read_pipo APC_DCH_BMP_STEREO
        aud_blks[j].sample_cnt = (aud_blks[j].sample_cnt << 0x1) / 3;
    }
    memcpy(aud_blks_bk, aud_blks, sizeof(PIPO_IN_BLOCK) * blk_cnt);

    // trigger DMA transfer for APC_DCH_BMP_STEREO (dual channels)
    ret = apc_dch_read_pipo(dch_set[0], APC_DCH_BMP_STEREO, aud_blks, blk_cnt_p,
                    pdci_set[0]->sg_offset_lr[0], pdci_set[0]->scat_gath_lr[0]);
    if (ret != CSK_DRIVER_OK) {
        apc_dual_channel_abort(dch_set[0]);
        return ret;
    }

    // sample_cnt / 3 for read_pipo APC_DCH_BMP_STEREO
    for (j=0; j<blk_cnt; j++) {
        aud_blks_bk[j].sample_cnt >>= 0x1;
    }

    // trigger DMA transfer for APC_DCH_BMP_LEFT (single channel)
    ret = apc_dch_read_pipo(dch_set[1], APC_DCH_BMP_LEFT, aud_blks_bk, &blk_cnt,
                    pdci_set[1]->sg_offset_lr[0], pdci_set[1]->scat_gath_lr[0]);
    if (ret != CSK_DRIVER_OK) {
        apc_dual_channel_abort(dch_set[1]);
        apc_dual_channel_abort(dch_set[0]);
        return ret;
    }

    return CSK_DRIVER_OK;
}


// read mixed data of APC channels (0 ~ 3) or (4,5,2,3) (4 IN channels) in PingPong mode
int32_t apc_read_quad_channels_pipo (QCH_TYPE qch_type, PIPO_IN_BLOCK *aud_blks, uint8_t *blk_cnt_p, bool init_chk)
{
    uint32_t dch_bytes = 0;
    int32_t ret = CSK_DRIVER_OK;

    if (aud_blks == NULL || blk_cnt_p == NULL || *blk_cnt_p > MAX_PIPO_BLKS)
        return CSK_DRIVER_ERROR_PARAMETER;

    uint8_t j, blk_cnt, dch_set[2];
    APC_DCH_INFO *pdci_set[2], *pdci;
    PIPO_IN_BLOCK aud_blks_bk[MAX_PIPO_BLKS];

    switch (qch_type) {
    case QCH_DCH_IN0_IN1:
        dch_set[0] = 0; dch_set[1] = 1;
        pdci_set[0] = &g_apc_dev.dch_array[0];
        pdci_set[1] = &g_apc_dev.dch_array[1];
        break;
    case QCH_DCH_IN2_IN1:
        dch_set[0] = 2; dch_set[1] = 1;
        pdci_set[0] = &g_apc_dev.dch_array[2];
        pdci_set[1] = &g_apc_dev.dch_array[1];
        break;
    default:
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (init_chk) {
    for (j=0; j<2; j++) {
        pdci = pdci_set[j];
        if ((pdci->ch_sel & APC_DCH_BMP_MASK) != APC_DCH_BMP_STEREO || pdci->mix_mode != APC_DCH_MIXED_MODE)
            return CSK_DRIVER_ERROR;

        if (pdci->ch_mode == APC_CHMODE_16BITS) {
            // count = 1, interval = 1
            pdci->scat_gath_lr[0] = (1 << SG_INTERVAL_POS) | (1 << SG_COUNT_POS);
            pdci->sg_bytes_lr[0] = 4; // 1 word
        } else if (pdci->dma_width_bits == DMA_WIDTH_HALFWORD) { // trim low 16bits
            // count = 2, interval = 2
            pdci->scat_gath_lr[0] = (2 << SG_INTERVAL_POS) | (2 << SG_COUNT_POS);
            pdci->sg_bytes_lr[0] = 4; // 2 halfwords
        } else {
            // count = 2, interval = 2
            pdci->scat_gath_lr[0] = (2 << SG_INTERVAL_POS) | (2 << SG_COUNT_POS);
            pdci->sg_bytes_lr[0] = 8; // 2 words
        }

        pdci->sg_offset_lr[0] = dch_bytes;
        dch_bytes += pdci->sg_bytes_lr[0];
    } // end for
    } // init check

    blk_cnt = *blk_cnt_p;
    for (j=0; j<blk_cnt; j++) {
        // sample_cnt Should be multiple of 4
        if (aud_blks[j].sample_cnt & 0x3) {
            LOGD("%s: sample count should be multiple of 4!\r\n", __func__);
            return CSK_DRIVER_ERROR_PARAMETER;
        }
        // sample_cnt / 2 for read_pipo half of total sample count
        aud_blks[j].sample_cnt >>= 0x1;
    }
    memcpy(aud_blks_bk, aud_blks, sizeof(PIPO_IN_BLOCK) * blk_cnt);

    // trigger DMA transfer for 1st APC dual_channel
    ret = apc_dch_read_pipo(dch_set[0], APC_DCH_BMP_STEREO, aud_blks, blk_cnt_p,
                           pdci_set[0]->sg_offset_lr[0], pdci_set[0]->scat_gath_lr[0]);
    if (ret != CSK_DRIVER_OK) {
        apc_dual_channel_abort(dch_set[0]);
        return ret;
    }

    // trigger DMA transfer for 2nd APC dual_channel
    ret = apc_dch_read_pipo(dch_set[1], APC_DCH_BMP_STEREO, aud_blks_bk, &blk_cnt,
                           pdci_set[1]->sg_offset_lr[0], pdci_set[1]->scat_gath_lr[0]);
    if (ret != CSK_DRIVER_OK) {
        apc_dual_channel_abort(dch_set[1]);
        apc_dual_channel_abort(dch_set[0]);
        return ret;
    }

    return CSK_DRIVER_OK;
}

#if USE_CMNDMA
// FOR PingPong Transfer (OUT)
static int32_t apc_dch_write_pipo_inclusive( APC_DCH dch,
                                            uint8_t lr_idx,
                                            PIPO_OUT_BLOCK *aud_blks,
                                            uint8_t *blk_cnt_p,
                                            uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                                            uint32_t src_gath)
{
    int ret = CSK_DRIVER_OK;
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];

    //uint32_t usr_param = (dch << 8) | (pdci->mix_mode ? APC_DCH_BMP_STEREO : APC_DCH_BMP_LEFT);
    uint32_t usr_param = (dch << 8) | (pdci->mix_mode ? APC_DCH_BMP_STEREO : (0x1 << lr_idx));
    uint8_t *dma_ch_p = &pdci->dma_ch_lr[lr_idx];

    // select some free DMA channel
    *dma_ch_p = dma_channel_select(dma_ch_p,
                                    apc_cmndma_event,
                                    usr_param,
                (pdci->rt_flag_lr[lr_idx] & RT_FLAG_NSYNCA) ? DMA_CACHE_SYNC_NOP : DMA_CACHE_SYNC_DST);
    if (*dma_ch_p == DMA_CHANNEL_ANY) {
        LOGD("%s: NO free DMA channel!!\r\n", __func__);
        return CSK_DRIVER_ERROR;
    }

    // setup DMA channel
    //NOTE: VENUSA CMNDMA has 2 master ports, and they are exactly the same. see DMS/SMS setting.
    uint32_t control, config_low, config_high;
    uint8_t dma_hsid = pdci->fixed_lr[lr_idx]->dma_hsid;
    control = DMA_CH_CTLL_DST_WIDTH(pdci->dma_width_bits) | DMA_CH_CTLL_SRC_WIDTH(pdci->dma_width_bits) |
            DMA_CH_CTLL_DST_BSIZE(pdci->dma_bsize_bits) | DMA_CH_CTLL_SRC_BSIZE(pdci->dma_bsize_bits) |
            DMA_CH_CTLL_DST_FIX | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_TTFC_M2P |
            DMA_CH_CTLL_DMS(1) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN | (src_gath ? DMA_CH_CTLL_S_GATH_EN : 0);
    config_low = DMA_CH_CFGL_CH_PRIOR(1); // channel priority is higher than 0
    config_high = DMA_CH_CFGH_DST_PER(dma_hsid);// DMA_CH_CFGH_PROTCTL(1) | DMA_CH_CFGH_FIFO_MODE |

    ret = dma_channel_setup (*dma_ch_p, DMA_CH_EN_XFER_INT | DMA_CH_EN_BLK_INT | DMA_CH_EN_PIPO,
                            control, config_low, config_high, src_gath, 0);
    if (ret < 0) {
        LOGD("%s: Failed to call dma_channel_setup!!\r\n", __func__);
        dma_channel_disable(*dma_ch_p, false);
        //*dma_ch_p = DMA_CHANNEL_ANY;
        return ret;
    }

    //set_cmndma_hs(dma_hsid, pdci->fixed_lr[lr_idx]->dma_hssel); // hs_sel = 1
    set_cmndma_hs(dma_hsid, 1); // hs_sel = 1

    // start DMA transfer
    uint32_t dst_addr = pdci->fixed_lr[lr_idx]->data_addr;
    if (pdci->dma_width_bits == DMA_WIDTH_HALFWORD) // trim low 16bits
        dst_addr += 2; // high 16bits address

    for (uint8_t i = 0; i < *blk_cnt_p; i++) {
        aud_blks[i].reserved = dst_addr;
        //FIXME: adjust start position of buffer & sample count
        aud_blks[i].sample_data = (uint8_t *)aud_blks[i].sample_data + buf_offset;
        if (pdci->samp_bits == 16)
            aud_blks[i].sample_cnt >>= 1;
    }

    pdci->samps_lr[lr_idx] = 0; //FIXME: for mixed mode, set samps for another channel?
    ret = dma_channel_start_pipo (*dma_ch_p, (DMA_PIPO_BLK *)aud_blks, blk_cnt_p);
    if (ret < 0) {
        //LOGD("%s: Failed to call dma_channel_start_pipo!!\r\n", __func__);
        dma_channel_disable(*dma_ch_p, false);
        //*dma_ch_p = DMA_CHANNEL_ANY;
        return ret;
    }

    return ret;
}
#endif

// sample_cnt SHOULD be EVEN when 16-bit sample!
int32_t
apc_dch_write_pipo (APC_DCH dch, uint8_t lr_bmp, // uint8_t pipo_lr_bmp,
                    PIPO_OUT_BLOCK *aud_blks, uint8_t *blk_cnt_p,
                    uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                    uint32_t src_gath)
{
    // Check if parameters are valid, i.e. APC channel is valid
    if (dch < APC_DCH_OUT_MIN || dch > APC_DCH_OUT_MAX || !DCH_IS_OWNED(dch) ||
        aud_blks == NULL || blk_cnt_p == NULL || *blk_cnt_p > 2)
        return CSK_DRIVER_ERROR_PARAMETER;

    lr_bmp &= APC_DCH_BMP_MASK;

    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    // 0 = left, 1 = right (use left or right channel for dual_channel)
    uint8_t lr_idx = (lr_bmp & APC_DCH_BMP_LEFT)? 0 : 1;
    uint8_t use_gpdma = (pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_GPDMA);
    uint8_t dma_ch = pdci->dma_ch_lr[lr_idx];
    uint8_t dma_hsid = pdci->fixed_lr[lr_idx]->dma_hsid;
    int32_t stat;

    uint8_t full_chk = (pdci->busy_lr[lr_idx] == 0);
    if (full_chk) {
        // Check if APC and specified dual_channel are both initialized
        if (!IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
            return CSK_DRIVER_ERROR;

        if (pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_PIO) {
            // PingPong mode is only supported for DMA!!
            return CSK_DRIVER_ERROR_UNSUPPORTED;
        }
        // for single channel, Only MONO_MODE (separated L/R channel data) is supported
        // for dual channel, Only STEREO_MODE (mixed L/R channel data) is supported
        if ((lr_bmp != APC_DCH_BMP_STEREO && pdci->mix_mode == APC_DCH_MIXED_MODE) ||
            (lr_bmp == APC_DCH_BMP_STEREO && pdci->mix_mode == APC_DCH_SEPARATE_MODE))
            return CSK_DRIVER_ERROR;

        // Only DAC / I2S OUT interface can be written
        if (pdci->intf_type != APC_INTF_DAC &&
            pdci->intf_type != APC_INTF_I2S_OUT)
            return CSK_DRIVER_ERROR;

    #if USE_GPDMA2D //FIXME: use DMA2D API instead of original GPDMA!!
        // Configure Scatter/Gather only once (when started)?
        if (use_gpdma) {
            csk_gather_scatter_info_t gather = { 0 };
            if (src_gath) {
                gather.enable = csk_func_enable;
                gather.counter = SG_COUNT(src_gath) << pdci->dma_width_bits;
                gather.interval = SG_INTERVAL(src_gath) << pdci->dma_width_bits;
            }
            DMA2D_Config_SrcGather(dma_ch, &gather);
        }
    #endif //USE_GPDMA2D

    } // full_chk

    // sample_cnt should be even when 16-bit sample
    if (pdci->ch_mode == APC_CHMODE_16BITS) {
        for (uint8_t i = 0; i < *blk_cnt_p; i++) {
            if (aud_blks[i].sample_cnt & 0x1)
                return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    // just return busy if the channel is busy (Don't check busy if NOT pipo_both)
    //if (pdci->busy_lr[lr_idx] != 0)
    //    return CSK_DRIVER_ERROR_BUSY;

    if (use_gpdma == 0) { // CMNDMA is used
    #if USE_CMNDMA
        if (dma_channel_check_select(dma_ch, DMA_TT_M2P | DMA_XFER_FLAG_PIPO, dma_hsid)) {
            //FIXME: adjust start position of buffer & sample count
            for (uint8_t i = 0; i < *blk_cnt_p; i++) {
                aud_blks[i].sample_data = (uint8_t *)aud_blks[i].sample_data + buf_offset;
                if (pdci->samp_bits == 16)
                    aud_blks[i].sample_cnt >>= 1;
            }
            //LOGD("%s: call dma_channel_start_pipo!\n", __func__);
            stat = dma_channel_start_pipo(dma_ch, (DMA_PIPO_BLK *)aud_blks, blk_cnt_p);
            if (stat < 0) {
                //LOGD("%s: Failed to call dma_channel_start_pipo!!\r\n", __func__);
                return stat;
            }
        } else {
            //LOGD("%s: call apc_dch_write_pipo_inclusive!\n", __func__);
            stat = apc_dch_write_pipo_inclusive(dch, lr_idx, aud_blks, blk_cnt_p, buf_offset, src_gath);
            if (stat < 0) {
                //LOGD("%s: Failed to call apc_dch_write_pipo_inclusive!!\r\n", __func__);
                return stat;
            }
        }
    #else // !USE_CMNDMA
        LOGD("%s: USE_CMNDMA is defined as 0, do nothing for CMNDMA write!\n", __func__);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    #endif

    } else { // GPDMA2D is used
    #if USE_GPDMA2D //FIXME: use DMA2D API instead of original GPDMA!!
        uint32_t dst_addr = pdci->fixed_lr[lr_idx]->data_addr;
        if (pdci->dma_width_bits == DMA_WIDTH_HALFWORD) // trim low 16bits
            dst_addr += 2; // high 16bits address
        uint32_t src_addr = (uint32_t) aud_blks[0].sample_data + buf_offset;
        uint32_t len_bytes = aud_blks[0].sample_cnt << pdci->dma_width_bits;
        //if (pdci->samp_bits == 16)  len_bytes >>= 1;

        uint8_t pipo_both = (*blk_cnt_p == 2);
        if (pipo_both) { // first call of write_pipo
            pdci->samps_lr[lr_idx] = 0;
            uint32_t src_addr2 = (uint32_t) aud_blks[1].sample_data + buf_offset;
            uint32_t len2_bytes = aud_blks[1].sample_cnt << pdci->dma_width_bits;
            //if (pdci->samp_bits == 16)  len2_bytes >>= 1;
            //NOTE: Double DMA2D_Start_Normal are called to trigger PingPong operations on GPDMA2D.
            // In PingPong mode the transfer length SHOULD NOT be more than the maximum block size (2^24 bytes)!
			stat = DMA2D_Start_Normal(dma_ch, (void*)src_addr, (void*)dst_addr, len_bytes); //ping
            stat = DMA2D_Start_Normal(dma_ch, (void*)src_addr2, (void*)dst_addr, len2_bytes); //pong
        } else { // subsequent calls of write_pipo
            stat = DMA2D_Start_Normal(dma_ch, (void*)src_addr, (void*)dst_addr, len_bytes); //ping or pong
        }
        if (stat != CSK_DRIVER_OK) {
            LOGD("%s: Failed to Start DMA2D PiPo!!\r\n", __func__);
            return stat;
        }
    #else // !USE_GPDMA2D
        LOGD("%s: USE_GPDMA2D is defined as 0, do nothing for GPDMA2D write!\n", __func__);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    #endif

    } // end GPDMA2D is used

    pdci->busy_lr[lr_idx] = 1;
    return CSK_DRIVER_OK;
}


// return count of transferred block if >= 0, else return the error value.
int32_t apc_dch_get_pipo_blks(APC_DCH dch, uint8_t lr_bmp,
                                  PIPO_IO_BLOCK *aud_blks, uint8_t blk_cnt)
{    // Check if parameters are valid, i.e. APC channel is valid
    if (dch >= APC_DCH_COUNT || !DCH_IS_OWNED(dch) || aud_blks == NULL || blk_cnt == 0)
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check if APC and specified channel are both initialized
    if (!IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
        return CSK_DRIVER_ERROR;

    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    // 0 = left, 1 = right (use left or right channel for dual_channel)
    uint8_t lr_idx = (lr_bmp & APC_DCH_BMP_LEFT)? 0 : 1;

    if (pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_PIO) {
        // PingPong mode is only supported for DMA!!
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }
    // for single channel, Only MONO_MODE (separated L/R channel data) is supported
    // for dual channel, Only STEREO_MODE (mixed L/R channel data) is supported
    if ((lr_bmp != APC_DCH_BMP_STEREO && pdci->mix_mode == APC_DCH_MIXED_MODE) ||
        (lr_bmp == APC_DCH_BMP_STEREO && pdci->mix_mode == APC_DCH_SEPARATE_MODE))
        return CSK_DRIVER_ERROR;

    int32_t ret = 0;
    aud_blks->xfer.flags = 0;

    if ((pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_GPDMA) == 0) { // CMNDMA is used
    #if USE_CMNDMA
        ret = dma_channel_get_pipo_blks(pdci->dma_ch_lr[lr_idx], (DMA_PIPO_BLK *)aud_blks, blk_cnt);
        if (ret < 0)
            return ret;
    #else // !USE_CMNDMA
        LOGD("%s: USE_CMNDMA is defined as 0, do nothing for CMNDMA write!\n", __func__);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    #endif

    } else { // GPDMA2D is used
    #if USE_GPDMA2D
        csk_dma2d_work_param_t last_blk;
        ret = DMA2D_Get_LastBlock(pdci->dma_ch_lr[lr_idx], &last_blk);
        if (ret < 0)
            return ret;
        aud_blks->xfer.cnt = last_blk.blk_len >> pdci->dma_width_bits; // byte => sample count
        aud_blks->xfer.src = (uint32_t)last_blk.src_addr;
        aud_blks->xfer.dst = (uint32_t)last_blk.dst_addr;
        ret = 1; // only 1, ping or pong block
    #else // !USE_GPDMA2D
        LOGD("%s: USE_GPDMA2D is defined as 0, do nothing for GPDMA2D write!\n", __func__);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    #endif

    } // end GPDMA2D is used

    if (pdci->ch_mode == APC_CHMODE_16BITS) {
        for (int32_t i = 0; i < ret; i++) {
            // adjust sample count if 16bit alone
            // sample_cnt is the 3rd word of PIPO_IN_BLOCK or PIPO_OUT_BLOCK struct
            aud_blks[0].xfer.cnt <<= 1;
        }
    }

    return ret;
}


#if 0 //USE_CMNDMA //TODO: only CMNDMA support LLP

// GPDMA doesn't support LLP, but supports 2 blocks PingPong mode...
/*
// read channel data based on DMA Linked List Pointer (multiple buffers)
static int32_t
apc_channel_read_LLP_internal (APC_DCH dch,
                            uint8_t lr_bmp,
                            AUDIO_BUFFER_LLI * bufs,
                            uint32_t buf_cnt,
                            uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                            uint32_t dst_scat) // destination scatter setting (default 0, NO SG support)
{
    // Check if parameters are valid, i.e. APC dual_channel is valid
    if (dch < APC_DCH_OUT_MIN || dch > APC_DCH_OUT_MAX || bufs == NULL || buf_cnt == 0)
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check if APC and specified dual_channel are both initialized
    if (!IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
        return CSK_DRIVER_ERROR;

    lr_bmp &= APC_DCH_BMP_MASK;
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    uint32_t i;

    // 0 = left, 1 = right (use left or right channel for dual_channel)
    uint8_t lr_idx = (lr_bmp & APC_DCH_BMP_LEFT)? 0 : 1;
    //if (pdci->mix_target == 1) // mixed @ right channel
    //    lr_idx = 1;

    // sample_cnt should be even when 16-bit sample
    if (pdci->ch_mode == APC_CHMODE_16BITS) {
        for (i=0; i<buf_cnt; i++) {
            if ((bufs[i].sample_cnt & 0x1) != 0)
                return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    // for single channel, Only MONO_MODE (separated L/R channel data) is supported
    // for dual channel, Only STEREO_MODE (mixed L/R channel data) is supported
    if ((lr_bmp != APC_DCH_BMP_STEREO && pdci->mix_mode == APC_DCH_STEREO_MODE) ||
        (lr_bmp == APC_DCH_BMP_STEREO && pdci->mix_mode == APC_DCH_SEPARATE_MODE))
        return CSK_DRIVER_ERROR;

    // Only ADC / I2S IN / ECHO interface can be read
    if (//pdci->intf_type != APC_INTF_VAD &&
        pdci->intf_type != APC_INTF_ADC_PDM &&
        pdci->intf_type != APC_INTF_I2S_IN &&
        pdci->intf_type != APC_INTF_ECHO)
        return CSK_DRIVER_ERROR;

    if (pdci->busy_lr[lr_idx] != 0)
        return CSK_DRIVER_ERROR_BUSY;

    uint8_t *dma_ch_p = &pdci->dma_ch_lr[lr_idx];

    // select some free DMA channel

    uint32_t usr_param = (dch << 8) | lr_bmp;
    *dma_ch_p = dma_channel_select(dma_ch_p,
                                    apc_dma_event,
                                    usr_param,
                (pdci->rt_flag_lr[lr_idx] & RT_FLAG_NSYNCA) ? DMA_CACHE_SYNC_NOP : DMA_CACHE_SYNC_DST);
    if (*dma_ch_p == DMA_CHANNEL_ANY) {
        LOGD("%s: NO free DMA channel!!\r\n", __func__);
        return CSK_DRIVER_ERROR;
    }

    uint32_t control, config_low, config_high;
    DMA_LLP ip_pre, ip;

    control = DMA_CH_CTLL_DST_WIDTH(pdci->dma_width_bits) | DMA_CH_CTLL_SRC_WIDTH(pdci->dma_width_bits) |
            DMA_CH_CTLL_DST_BSIZE(pdci->dma_bsize_bits) | DMA_CH_CTLL_SRC_BSIZE(pdci->dma_bsize_bits) |
            DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_TTFC_P2M |
            DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(1) | DMA_CH_CTLL_INT_EN | (dst_scat ? DMA_CH_CTLL_D_SCAT_EN : 0);
    config_low = DMA_CH_CFGL_CH_PRIOR(1); // channel priority is higher than 0
    config_high = DMA_CH_CFGH_SRC_PER(pdci->fixed_lr[lr_idx]->dma_hsid); // DMA_CH_CFGH_FIFO_MODE |// DMA_CH_CFGH_PROTCTL(1) |

    uint32_t src_addr = pdci->fixed_lr[lr_idx]->data_addr;
    if (pdci->dma_width_bits == DMA_WIDTH_HALFWORD) // trim low 16bits
        src_addr += 2; // high 16bits address

    ip_pre = NULL;
    for (i=0; i<buf_cnt; i++) {
        ip = &bufs[i].dma_lli;
        memset(ip, 0, sizeof(DMA_LLI));
        ip->SAR = src_addr; //pdci->fixed_lr[lr_idx]->data_addr;
        ip->DAR = (uint32_t)(bufs[i].sample_data) + buf_offset;
        if (ip_pre != NULL)
            ip_pre->LLP = (uint32_t)ip;
        ip->CTL_LO = control;
        ip->u.SIZE = (pdci->samp_bits > 16 ? bufs[i].sample_cnt : bufs[i].sample_cnt/2);
        ip_pre = ip; // save current DMA_LLI pointer
    }

    // DMA_LLI list (stored in AUDIO_BUFFER_LLI array) is to be accessed by DMAC,
    // So the AUDIO_BUFFER_LLI array memory space SHOULD be do cache sync operation
    if (range_is_cacheable((unsigned long)bufs, sizeof(AUDIO_BUFFER_LLI) * buf_cnt))
        dcache_flush_range((unsigned long)bufs, (unsigned long)(bufs + buf_cnt));

    // configure DMA channel
    int32_t stat = dma_channel_configure_LLP (*dma_ch_p, &bufs->dma_lli, config_low, config_high, 0, dst_scat);
    if (stat == -1) {
        LOGD("%s: Failed to call dma_channel_configure!!\r\n", __func__);
        dma_channel_disable(*dma_ch_p, false);
        *dma_ch_p = DMA_CHANNEL_ANY;
        return CSK_DRIVER_ERROR;
    }

    pdci->busy_lr[lr_idx] = 1;
    return CSK_DRIVER_OK;
}


int32_t apc_channel_read_LLP(APC_CH ch,
                            AUDIO_BUFFER_LLI * bufs,
                            uint32_t buf_cnt,
                            uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                            uint32_t dst_scat) // destination scatter setting (default 0, NO SG support)
{
    return apc_channel_read_LLP_internal (APC_CH_TO_DCH(ch),
                                          APC_CH_LR_BMP(ch),
                                          bufs,
                                          buf_cnt,
                                          buf_offset,
                                          dst_scat);
}


int32_t apc_dual_channel_read_LLP (APC_DCH dch,
                                AUDIO_BUFFER_LLI * bufs,
                                uint32_t buf_cnt,
                                uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                                uint32_t dst_scat) // destination scatter setting (default 0, NO SG support)
{
    return apc_channel_read_LLP_internal (dch,
                                          APC_DCH_BMP_STEREO,
                                          bufs,
                                          buf_cnt,
                                          buf_offset,
                                          dst_scat);
}


int32_t apc_read_quad_channels_LLP (QCH_TYPE qch_type,
                                AUDIO_BUFFER_LLI * bufs,
                                uint32_t buf_cnt,
                                bool init_chk)
{
    //uint32_t dst_scat, dch_bytes;
    uint32_t dch_bytes = 0;
    int32_t ret = CSK_DRIVER_OK;
    AUDIO_BUFFER_LLI * bufs_list[2];
    uint8_t i, j, dch_set[2];
    APC_DCH_INFO *pdci_set[2], *pdci;

    // We need operate on 2 dual_channels for quad_channels read.
    // Besides the original AUDIO_BUFFER_LLI array, We need duplicate the array for 2nd dual_channel,
    // so at most only MAX_DMA_BUF_CNT items (buf_cnt) of AUDIO_BUFFER_LLI are supported here to simplify the case!
    if (buf_cnt > MAX_DMA_BUF_CNT) {
        LOGD("%s: at most %d buffers are supported now!\n", __func__, MAX_DMA_BUF_CNT);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    switch (qch_type) {
    case QCH_DCH_IN0_IN1:
        dch_set[0] = 0; dch_set[1] = 1;
        pdci_set[0] = &g_apc_dev.dch_array[0];
        pdci_set[1] = &g_apc_dev.dch_array[1];
        break;
    case QCH_DCH_IN1_IN0:
        dch_set[0] = 1; dch_set[1] = 0;
        pdci_set[0] = &g_apc_dev.dch_array[1];
        pdci_set[1] = &g_apc_dev.dch_array[0];
        break;
    default:
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (init_chk) {
    for (i=0; i<2; i++) {
        pdci = pdci_set[i];
        if ((pdci->ch_sel & APC_DCH_BMP_MASK) != APC_DCH_BMP_STEREO || pdci->mix_mode != APC_DCH_MIXED_MODE)
            return CSK_DRIVER_ERROR;

        if (pdci->ch_mode == APC_CHMODE_16BITS) {
            // count = 1, interval = 1
            pdci->scat_gath_lr[0] = (1 << SG_INTERVAL_POS) | (1 << SG_COUNT_POS);
            pdci->sg_bytes_lr[0] = 4; // 1 word
        } else if (pdci->dma_width_bits == DMA_WIDTH_HALFWORD) { // trim low 16bits
            // count = 2, interval = 2
            pdci->scat_gath_lr[0] = (2 << SG_INTERVAL_POS) | (2 << SG_COUNT_POS);
            pdci->sg_bytes_lr[0] = 4; // 2 halfwords
        } else {
            // count = 2, interval = 2
            pdci->scat_gath_lr[0] = (2 << SG_INTERVAL_POS) | (2 << SG_COUNT_POS);
            pdci->sg_bytes_lr[0] = 8; // 2 words
        }

        if (dch_bytes == 0) // save latest sg_bytes_lr if NOT initialized
            dch_bytes = pdci->sg_bytes_lr[0];
        else if (dch_bytes != pdci->sg_bytes_lr[0]) // compare with new one and return error if unequal
            return CSK_DRIVER_ERROR;

        pdci->sg_offset_lr[0] = i * dch_bytes;
    } // end for
    } // init check

    // sample_cnt Should be multiple of 4, and change sample_cnt to sample_cnt/2 if OK
    for (i=0; i<buf_cnt; i++) {
        if ((bufs[i].sample_cnt & 0x3) != 0) {
            LOGD("%s: sample count of each buffer should be multiple of 4!\r\n", __func__);
            return CSK_DRIVER_ERROR_PARAMETER;
        } else { // sample_cnt => sample_cnt/2
            bufs[i].sample_cnt /= 2;
        }
    }

    //duplicate AUDIO_BUFFER_LLI array for another IN dual_channel
    bufs_list[0] = bufs;
    bufs_list[1] = g_lli_pool;
    memcpy(g_lli_pool, bufs, sizeof(AUDIO_BUFFER_LLI) * buf_cnt);

    // trigger DMA transfer for each APC dual_channel
    for (i=0; i<2; i++) {
        ret = apc_channel_read_LLP_internal(dch_set[i], APC_DCH_BMP_STEREO, bufs_list[i], buf_cnt,
                                            pdci_set[i]->sg_offset_lr[0], pdci_set[i]->scat_gath_lr[0]);
        if (ret != CSK_DRIVER_OK)
            break;
    }

    if (ret == CSK_DRIVER_OK)
        return ret;

    // there's some error, abort all triggered DMA transfer
    for (j=0; j<i; j++) {
        apc_dual_channel_abort(dch_set[j]);
    }

    return ret;
}


// write channel data based on DMA Linked List Pointer (multiple buffers)
static int32_t
apc_channel_write_LLP_internal (APC_DCH dch,
                              uint8_t lr_bmp,
                              AUDIO_BUFFER_LLI * bufs,
                              uint32_t buf_cnt,
                              uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                              uint32_t src_gath) // source gather setting (default 0, NO SG support)
{
    // Check if parameters are valid, i.e. APC dual_channel is valid
    if (dch < APC_DCH_OUT_MIN || dch > APC_DCH_OUT_MAX || bufs == NULL || buf_cnt == 0)
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check if APC and specified dual_channel are both initialized
    if (!IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
        return CSK_DRIVER_ERROR;

    lr_bmp &= APC_DCH_BMP_MASK;
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    uint32_t i;
    // 0 = left, 1 = right (use left or right channel for dual_channel)
    uint8_t lr_idx = (lr_bmp & APC_DCH_BMP_LEFT)? 0 : 1;
    //if (pdci->mix_target == 1) // mixed @ right channel
    //    lr_idx = 1;

    // sample_cnt should be even when 16-bit sample
    if (pdci->ch_mode == APC_CHMODE_16BITS) {
        for (i=0; i<buf_cnt; i++) {
            if ((bufs[i].sample_cnt & 0x1) != 0)
                return CSK_DRIVER_ERROR_PARAMETER;
        }
    }

    // for single channel, Only MONO_MODE (separated L/R channel data) is supported
    // for dual channel, Only STEREO_MODE (mixed L/R channel data) is supported
    if ((lr_bmp != APC_DCH_BMP_STEREO && pdci->mix_mode == APC_DCH_MIXED_MODE) ||
        (lr_bmp == APC_DCH_BMP_STEREO && pdci->mix_mode == APC_DCH_SEPARATE_MODE))
        return CSK_DRIVER_ERROR;

    // Only DAC / I2S OUT interface can be written
    if (pdci->intf_type != APC_INTF_DAC &&
        pdci->intf_type != APC_INTF_I2S_OUT)
        return CSK_DRIVER_ERROR;

    if (pdci->busy_lr[lr_idx] != 0)
        return CSK_DRIVER_ERROR_BUSY;

    uint8_t *dma_ch_p = &pdci->dma_ch_lr[lr_idx];
    int32_t stat;

    // select some free DMA channel
    uint32_t usr_param = (dch << 8) | lr_bmp;
    *dma_ch_p = dma_channel_select(dma_ch_p,
                                    apc_dma_event,
                                    usr_param,
               (pdci->rt_flag_lr[lr_idx] & RT_FLAG_NSYNCA) ? DMA_CACHE_SYNC_NOP : DMA_CACHE_SYNC_SRC);
    if (*dma_ch_p == DMA_CHANNEL_ANY) {
        LOGD("%s: NO free DMA channel!!\r\n", __func__);
        return CSK_DRIVER_ERROR;
    }

    uint32_t control, config_low, config_high;
    DMA_LLP ip_pre, ip;

    control = DMA_CH_CTLL_DST_WIDTH(pdci->dma_width_bits) | DMA_CH_CTLL_SRC_WIDTH(pdci->dma_width_bits) |
            DMA_CH_CTLL_DST_BSIZE(pdci->dma_bsize_bits) | DMA_CH_CTLL_SRC_BSIZE(pdci->dma_bsize_bits) |
            DMA_CH_CTLL_DST_FIX | DMA_CH_CTLL_SRC_INC | DMA_CH_CTLL_TTFC_M2P |
            DMA_CH_CTLL_DMS(2) | DMA_CH_CTLL_SMS(3) | DMA_CH_CTLL_INT_EN | (src_gath ? DMA_CH_CTLL_S_GATH_EN : 0);
    config_low = DMA_CH_CFGL_CH_PRIOR(1); // channel priority is higher than 0
    config_high = DMA_CH_CFGH_DST_PER(pdci->fixed_lr[lr_idx]->dma_hsid); // DMA_CH_CFGH_FIFO_MODE |// DMA_CH_CFGH_PROTCTL(1) |

    uint32_t dst_addr = pdci->fixed_lr[lr_idx]->data_addr;
    if (pdci->dma_width_bits == DMA_WIDTH_HALFWORD) // trim low 16bits
        dst_addr += 2; // high 16bits address

    ip_pre = NULL;
    for (i=0; i<buf_cnt; i++) {
        ip = &bufs[i].dma_lli;
        memset(ip, 0, sizeof(DMA_LLI));
        ip->SAR = (uint32_t)(bufs[i].sample_data) + buf_offset;
        ip->DAR = dst_addr; //pdci->fixed_lr[lr_idx]->data_addr;
        if (ip_pre != NULL)
            ip_pre->LLP = (uint32_t)ip;
        ip->CTL_LO = control;
        ip->u.SIZE = (pdci->samp_bits > 16 ? bufs[i].sample_cnt : bufs[i].sample_cnt/2);
        ip_pre = ip; // save current DMA_LLI pointer
    }

    // DMA_LLI list (stored in AUDIO_BUFFER_LLI array) is to be accessed by DMAC,
    // So the AUDIO_BUFFER_LLI array memory space SHOULD be do cache sync operation
    if (range_is_cacheable((unsigned long)bufs, sizeof(AUDIO_BUFFER_LLI) * buf_cnt))
        dcache_flush_range((unsigned long)bufs, (unsigned long)(bufs + buf_cnt));

    // configure DMA channel
    stat = dma_channel_configure_LLP (*dma_ch_p, &bufs->dma_lli, config_low, config_high, src_gath, 0);
    if (stat == -1) {
        LOGD("%s: Failed to call dma_channel_configure!!\r\n", __func__);
        dma_channel_disable(*dma_ch_p, false);
        *dma_ch_p = DMA_CHANNEL_ANY;
        return CSK_DRIVER_ERROR;
    }

    pdci->busy_lr[lr_idx] = 1;
    return CSK_DRIVER_OK;
}


// write channel data based on DMA Linked List Pointer (multiple buffers)
int32_t apc_channel_write_LLP (APC_CH ch,
                              AUDIO_BUFFER_LLI * bufs,
                              uint32_t buf_cnt,
                              uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                              uint32_t src_gath) // source gather setting (default 0, NO SG support)
{
    return apc_channel_write_LLP_internal (APC_CH_TO_DCH(ch),
                                            APC_CH_LR_BMP(ch),
                                            bufs,
                                            buf_cnt,
                                            buf_offset,
                                            src_gath);
}


// write channel data based on DMA Linked List Pointer (multiple buffers)
int32_t apc_dual_channel_write_LLP (APC_DCH dch,
                                  AUDIO_BUFFER_LLI * bufs,
                                  uint32_t buf_cnt,
                                  uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                                  uint32_t src_gath) // source gather setting (default 0, NO SG support)
{
    return apc_channel_write_LLP_internal (dch,
                                            APC_DCH_BMP_STEREO,
                                            bufs,
                                            buf_cnt,
                                            buf_offset,
                                            src_gath);
}
*/
#endif // USE_CMNDMA


// clear interrupts => unmask desired interrupts => enable APC channel
int32_t apc_channel_enable (APC_CH ch)
{
    APC_DCH dch = APC_CH_TO_DCH(ch);
    uint8_t lr_idx = APC_CH_LR_IDX(ch);

    // Check if parameters are valid, i.e. APC channel is valid
    if (dch >= APC_DCH_COUNT || !DCH_IS_OWNED(dch))
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check if APC and specified channel are both initialized
    if (!IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
        return CSK_DRIVER_ERROR;

    uint32_t cfg_val, en_val, intr_bits = 0;
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    if ((pdci->ch_sel & APC_CH_LR_BMP(ch)) == 0) {
        LOGD("%s: APC channel %d is not selected!", __func__, ch);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

#if SUPPORT_I2S_ERR
    // check if I2S device
    bool is_i2s = (pdci->intf_type == APC_INTF_I2S_IN || pdci->intf_type == APC_INTF_I2S_OUT);
#endif

    if (pdci->fixed_lr[lr_idx]->ch_dir) // IN (left and right are same)
        //intr_bits = APC_INTR_READY_TO_XFER | APC_INTR_FIFO_OVERRUN | APC_INTR_FIFO_FULL;
        intr_bits = APC_INTR_FIFO_OVERRUN | APC_INTR_FIFO_FULL;
    else // OUT (left and right are same)
        //intr_bits = APC_INTR_READY_TO_XFER | APC_INTR_FIFO_UNDERRUN | APC_INTR_FIFO_EMPTY;
        intr_bits = APC_INTR_FIFO_UNDERRUN | APC_INTR_FIFO_EMPTY;

#if SUPPORT_APC_PIO
    if ((pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_PIO) != 0) { // use PIO
        intr_bits |= APC_INTR_READY_TO_XFER;
    }
#endif // SUPPORT_APC_PIO

    cfg_val = *pdci->cfg_reg_p;
    en_val = (lr_idx == 0 ? APC_DCH_L_EN : APC_DCH_R_EN);
    if ((cfg_val & en_val) == en_val) {
        // re-enable interrupts even if already enabled
        apc_ch_intr_enable(ch, intr_bits, ON_IRQ_APC0);
        //LOGD("%s: %s channel already enabled!\n", __func__, (lr_idx == 0 ? "L" : "R"));
        return CSK_DRIVER_OK;
    }

#if SUPPORT_I2S_ERR
    // clear & enable i2s error interrupts for i2s
    if (is_i2s) {
        apc_i2s_err_intr_clear(pdci->intf_idx, ON_IRQ_APC0);
        apc_i2s_err_intr_enable(pdci->intf_idx, ON_IRQ_APC0);
    }
#endif

    // clear all interrupts' status
    apc_ch_intr_clear(ch, APC_FIFO_INTR_MASK, ON_IRQ_APC0);

    // enable all interrupts of APC channel
    apc_ch_intr_enable(ch, intr_bits, ON_IRQ_APC0);

    *pdci->cfg_reg_p = cfg_val | en_val;

    return CSK_DRIVER_OK;
}

int32_t apc_channel_disable (APC_CH ch) // disable APC channel only
{
    APC_DCH dch = APC_CH_TO_DCH(ch);
    uint8_t lr_idx = APC_CH_LR_IDX(ch);

    // Check if parameters are valid, i.e. APC channel is valid
    if (dch >= APC_DCH_COUNT || !DCH_IS_OWNED(dch))
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check if APC and specified channel are both initialized
    if (!IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
        return CSK_DRIVER_ERROR;

    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    if ((pdci->ch_sel & APC_CH_LR_BMP(ch)) == 0) {
        LOGD("%s: APC channel %d is not selected!", __func__, ch);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    // disable all interrupts of APC channel
    apc_ch_intr_disable(ch, APC_FIFO_INTR_MASK, ON_IRQ_APC0);

    // disable Left or Right APC channel
    *pdci->cfg_reg_p &= ~(lr_idx == 0 ? APC_DCH_L_EN : APC_DCH_R_EN);

    return CSK_DRIVER_OK;
}

int32_t apc_channel_abort (APC_CH ch) // abort APC channel data transfer only
{
    int32_t ret = CSK_DRIVER_OK;
    APC_DCH dch = APC_CH_TO_DCH(ch);
    uint8_t lr_idx = APC_CH_LR_IDX(ch);

    // Check if parameters are valid, i.e. APC channel is valid
    if (dch >= APC_DCH_COUNT || !DCH_IS_OWNED(dch))
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check if APC and specified channel are both initialized
    if (!IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
        return CSK_DRIVER_ERROR;

    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    if ((pdci->ch_sel & APC_CH_LR_BMP(ch)) == 0) {
        LOGD("%s: APC channel %d is not selected!", __func__, ch);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

#if SUPPORT_I2S_ERR
    // check if I2S device
    bool is_i2s = (pdci->intf_type == APC_INTF_I2S_IN || pdci->intf_type == APC_INTF_I2S_OUT);
    // disable & clear i2s error interrupts for i2s
    if (is_i2s) {
        apc_i2s_err_intr_disable(pdci->intf_idx, ON_IRQ_APC0);
        apc_i2s_err_intr_clear(pdci->intf_idx, ON_IRQ_APC0);
    }
#endif

    // disable both APC channel and its interrupts
    apc_ch_intr_disable(ch, APC_FIFO_INTR_MASK, ON_IRQ_APC0);

    uint32_t cfg_val = *pdci->cfg_reg_p;
    if (lr_idx == 0) { // Left channel
        cfg_val &= ~APC_DCH_L_EN; // disable channel
        cfg_val |= APC_DCH_L_FLU; // flush FIFO
    } else { // Right channel
        cfg_val &= ~APC_DCH_R_EN; // disable channel
        cfg_val |= APC_DCH_R_FLU; // flush FIFO
    }
    *pdci->cfg_reg_p = cfg_val;
    apc_ch_intr_clear(ch, APC_FIFO_INTR_MASK, ON_IRQ_APC0);

    // if busy and DMA is used
    if (pdci->busy_lr[lr_idx] && !(pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_PIO)) { //use DMA
        uint8_t use_gpdma = (pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_GPDMA);
        if (use_gpdma == 0) { // CMNDMA is used
        #if USE_CMNDMA
            // abort and don't wait until done
            ret = dma_channel_disable(pdci->dma_ch_lr[lr_idx], false);
            //ret = dma_channel_disable(pdci->dma_ch_lr[lr_idx], true);
        #else // !USE_CMNDMA
            LOGD("%s: USE_CMNDMA is defined as 0, do nothing!\n", __func__);
            return CSK_DRIVER_ERROR_UNSUPPORTED;
        #endif
        } else { // GPDMA2D is used
        #if USE_GPDMA2D //FIXME: use DMA2D API instead of original GPDMA!!
            DMA2D_Stop(pdci->dma_ch_lr[lr_idx]);
        #else // !USE_GPDMA2D
            LOGD("%s: USE_GPDMA2D is defined as 0, do nothing!\n", __func__);
            return CSK_DRIVER_ERROR_UNSUPPORTED;
        #endif
        } // end GPDMA2D is used
    } // end use_pio == 0

    pdci->busy_lr[lr_idx] = 0;

/*
    // disable & clear all interrupts of APC channel
    apc_ch_intr_disable(ch, APC_FIFO_INTR_MASK, ON_IRQ_APC0);
    apc_ch_intr_clear(ch, APC_FIFO_INTR_MASK, ON_IRQ_APC0);

    uint32_t cfg_val = *pdci->cfg_reg_p;
    if (lr_idx == 0) { // Left channel
        cfg_val &= ~APC_DCH_L_EN; // disable channel
        cfg_val |= APC_DCH_L_FLU; // flush FIFO
    } else { // Right channel
        cfg_val &= ~APC_DCH_R_EN; // disable channel
        cfg_val |= APC_DCH_R_FLU; // flush FIFO
    }
    *pdci->cfg_reg_p = cfg_val;
*/

    return ret;
}

/*
// en = 1, set NOT_SYNC_CACHE; en = 0, clear NOT_SYNC_CACHE
void apc_channel_set_nsynca (APC_CH ch, uint8_t en)
{
    APC_DCH dch = APC_CH_TO_DCH(ch);
    uint8_t lr_idx = APC_CH_LR_IDX(ch);

    // Check if parameters are valid, i.e. APC channel is valid
    if (dch >= APC_DCH_COUNT || !DCH_IS_OWNED(dch) ||
            !IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
        return;

    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    if (en)
        pdci->rt_flag_lr[lr_idx] |= RT_FLAG_NSYNCA;
    else
        pdci->rt_flag_lr[lr_idx] &= ~RT_FLAG_NSYNCA;
}
*/


/*
// enable APC channel => clear interrupts => unmask desired interrupts
int32_t apc_dual_channel_enable (APC_DCH dch) // enable APC dual_channel only
{
    // Check if parameters are valid, i.e. APC channel is valid
    if (dch >= APC_DCH_COUNT || !DCH_IS_OWNED(dch))
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check if APC and specified channel are both initialized
    if (!IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
        return CSK_DRIVER_ERROR;

    uint32_t cfg_val, reg_val, intr_bits = 0;
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    uint8_t ch_sel = pdci->ch_sel & APC_DCH_BMP_MASK;
    APC_CH ch;

    if (ch_sel == 0) {
        LOGD("%s: neither of APC dual channels (dch=%d) is not selected!",
                __func__, dch);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (pdci->fixed_lr[0]->ch_dir) // IN (left and right are same)
        //intr_bits = APC_INTR_READY_TO_XFER | APC_INTR_FIFO_OVERRUN | APC_INTR_FIFO_FULL;
        intr_bits = APC_INTR_FIFO_OVERRUN | APC_INTR_FIFO_FULL;
    else // OUT (left and right are same)
        //intr_bits = APC_INTR_READY_TO_XFER | APC_INTR_FIFO_UNDERRUN | APC_INTR_FIFO_EMPTY;
        intr_bits = APC_INTR_FIFO_UNDERRUN | APC_INTR_FIFO_EMPTY;

#if SUPPORT_APC_PIO
    if ((pdci->rt_flag_lr[0] & RT_FLAG_USE_PIO) != 0) { // use PIO, enable READY_TO_XFER intr.
        //intr_bits |= APC_INTR_READY_TO_XFER;
        if (ch_sel & APC_DCH_BMP_LEFT) // left or stereo
            apc_ch_intr_enable(APC_DCH_TO_CH(dch, 0), APC_INTR_READY_TO_XFER, ON_IRQ_APC0);
        else // right
            apc_ch_intr_enable(APC_DCH_TO_CH(dch, 1), APC_INTR_READY_TO_XFER, ON_IRQ_APC0);
    }
#endif // SUPPORT_APC_PIO

    cfg_val = *pdci->cfg_reg_p;
    if (ch_sel == APC_DCH_BMP_STEREO) { // both channels
        reg_val = APC_DCH_R_EN(dch) | APC_DCH_L_EN(dch);
        if ((cfg_val & reg_val) == reg_val) {
            //LOGD("%s: L/R channel already enabled!\n", __func__);
            return CSK_DRIVER_OK;
        }

        //TODO: wait until there's at least one data in the TX FIFO ?...
        apc_dch_intr_clear(dch, APC_FIFO_INTR_MASK, ON_IRQ_APC0); // Clear all interrupts' status

        // stereo, mixed (data via LEFT channel by default)
        apc_ch_intr_enable(APC_DCH_TO_CH(dch, 0), intr_bits, ON_IRQ_APC0); // enable interrupts of LEFT channel

        *pdci->cfg_reg_p = cfg_val | reg_val; // | APC_DCH_R_FLU | APC_DCH_L_FLU

    } else if (ch_sel == APC_DCH_BMP_LEFT) { // Left channel
        reg_val = APC_DCH_L_EN(dch);
        if ((cfg_val & reg_val) == reg_val) {
            //LOGD("%s: Left channel already enabled!\n", __func__);
            return CSK_DRIVER_OK;
        }

        //TODO: wait until there's at least one data in the TX FIFO ?...
        ch = APC_DCH_TO_CH(dch, 0);
        apc_ch_intr_clear(ch, APC_FIFO_INTR_MASK, ON_IRQ_APC0); // Clear all interrupts' status
        apc_ch_intr_enable(ch, intr_bits, ON_IRQ_APC0); // enable interrupts
        *pdci->cfg_reg_p = cfg_val | reg_val; // | APC_DCH_L_FLU

    } else if (ch_sel == APC_DCH_BMP_RIGHT) { // Right channel
        reg_val = APC_DCH_R_EN(dch);
        if ((cfg_val & reg_val) == reg_val) {
            //LOGD("%s: Right channel already enabled!\n", __func__);
            return CSK_DRIVER_OK;
        }

        //TODO: wait until there's at least one data in the TX FIFO ?...
        ch = APC_DCH_TO_CH(dch, 1);
        apc_ch_intr_clear(ch, APC_FIFO_INTR_MASK, ON_IRQ_APC0); // Clear all interrupts' status
        apc_ch_intr_enable(ch, intr_bits, ON_IRQ_APC0); // enable interrupts
        *pdci->cfg_reg_p = cfg_val | reg_val; // | APC_DCH_R_FLU
    }

    return CSK_DRIVER_OK;
}
*/

// clear interrupts => unmask desired interrupts => enable APC channel
int32_t apc_dual_channel_enable (APC_DCH dch)
{
    // Check if parameters are valid, i.e. APC channel is valid
    if (dch >= APC_DCH_COUNT || !DCH_IS_OWNED(dch))
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check if APC and specified channel are both initialized
    if (!IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
        return CSK_DRIVER_ERROR;

    uint32_t cfg_val, reg_val, intr_bits = 0;
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    uint8_t ch_sel = pdci->ch_sel & APC_DCH_BMP_MASK;
#if SUPPORT_I2S_ERR
    bool is_i2s = (pdci->intf_type == APC_INTF_I2S_IN || pdci->intf_type == APC_INTF_I2S_OUT);
#endif
    APC_CH ch;

    if (ch_sel == 0) {
        LOGD("%s: neither of APC dual channels (dch=%d) is not selected!",
                __func__, dch);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (pdci->fixed_lr[0]->ch_dir) // IN (left and right are same)
        intr_bits = APC_INTR_FIFO_OVERRUN | APC_INTR_FIFO_FULL;
    else // OUT (left and right are same)
        intr_bits = APC_INTR_FIFO_UNDERRUN | APC_INTR_FIFO_EMPTY;

#if SUPPORT_APC_PIO
    if ((pdci->rt_flag_lr[0] & RT_FLAG_USE_PIO) != 0) // use PIO, enable READY_TO_XFER intr.
        intr_bits |= APC_INTR_READY_TO_XFER;
#endif

    cfg_val = *pdci->cfg_reg_p;
    if (ch_sel == APC_DCH_BMP_STEREO) { // both channels
        ch = APC_DCH_TO_CH(dch, 0);
        reg_val = APC_DCH_R_EN | APC_DCH_L_EN; // | APC_DCH_R_FLU | APC_DCH_L_FLU
        if ((cfg_val & reg_val) == reg_val) { // already enabled
            // re-enable interrupts (of LEFT channel) even if already enabled
            apc_dch_intr_enable(dch, intr_bits, ON_IRQ_APC0);
            //LOGD("%s: L/R channel already enabled!\n", __func__);
            return CSK_DRIVER_OK;
        }

        // clear all interrupts' status
        apc_dch_intr_clear(dch, APC_FIFO_INTR_MASK, ON_IRQ_APC0);
        // stereo, mixed (data via LEFT channel by default)
        // enable interrupts of LEFT channel
        apc_dch_intr_enable(dch, intr_bits, ON_IRQ_APC0);
        //*pdci->cfg_reg_p = cfg_val | reg_val;

    } else if (ch_sel == APC_DCH_BMP_LEFT) { // Left channel
        ch = APC_DCH_TO_CH(dch, 0);
        reg_val = APC_DCH_L_EN; // | APC_DCH_L_FLU
        if ((cfg_val & reg_val) == reg_val) { // already enabled
            // re-enable interrupts even if already enabled
            apc_ch_intr_enable(ch, intr_bits, ON_IRQ_APC0);
            //LOGD("%s: Left channel already enabled!\n", __func__);
            return CSK_DRIVER_OK;
        }

        apc_ch_intr_clear(ch, APC_FIFO_INTR_MASK, ON_IRQ_APC0);
        apc_ch_intr_enable(ch, intr_bits, ON_IRQ_APC0);
        //*pdci->cfg_reg_p = cfg_val | reg_val;

    } else if (ch_sel == APC_DCH_BMP_RIGHT) { // Right channel
        ch = APC_DCH_TO_CH(dch, 1);
        reg_val = APC_DCH_R_EN; // | APC_DCH_R_FLU
        if ((cfg_val & reg_val) == reg_val) {
            // re-enable interrupts even if already enabled
            apc_ch_intr_enable(ch, intr_bits, ON_IRQ_APC0);
            //LOGD("%s: Right channel already enabled!\n", __func__);
            return CSK_DRIVER_OK;
        }

        apc_ch_intr_clear(ch, APC_FIFO_INTR_MASK, ON_IRQ_APC0);
        apc_ch_intr_enable(ch, intr_bits, ON_IRQ_APC0);
        //*pdci->cfg_reg_p = cfg_val | reg_val;
    }

#if SUPPORT_I2S_ERR
    // clear & enable i2s error interrupts for i2s
    if (is_i2s) {
        apc_i2s_err_intr_clear(pdci->intf_idx, ON_IRQ_APC0);
        apc_i2s_err_intr_enable(pdci->intf_idx, ON_IRQ_APC0);
    }
#endif // SUPPORT_I2S_ERR

    // start apc channel here
    *pdci->cfg_reg_p = cfg_val | reg_val;

    return CSK_DRIVER_OK;
}

int32_t apc_dual_channel_disable (APC_DCH dch) // disable APC dual_channel only
{
    // Check if parameters are valid, i.e. APC channel is valid
    if (dch >= APC_DCH_COUNT || !DCH_IS_OWNED(dch))
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check if APC and specified channel are both initialized
    if (!IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
        return CSK_DRIVER_ERROR;

    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    uint8_t ch_sel = pdci->ch_sel & APC_DCH_BMP_MASK;

    if (ch_sel == 0) {
        LOGD("%s: neither of APC dual channels (dch=%d) is not selected!",
                __func__, dch);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if (ch_sel == APC_DCH_BMP_STEREO) { // both channels
        apc_dch_intr_disable(dch, APC_FIFO_INTR_MASK, ON_IRQ_APC0); // disable all interrupts
        *pdci->cfg_reg_p &= ~(APC_DCH_R_EN | APC_DCH_L_EN);
        apc_dch_intr_clear(dch, APC_FIFO_INTR_MASK, ON_IRQ_APC0); // clear all interrupts
    } else if (ch_sel == APC_DCH_BMP_LEFT) { // Left channel
        apc_ch_intr_disable(APC_DCH_TO_CH(dch, 0), APC_FIFO_INTR_MASK, ON_IRQ_APC0); // disable all interrupts
        *pdci->cfg_reg_p &= ~APC_DCH_L_EN;
        apc_ch_intr_clear(APC_DCH_TO_CH(dch, 0), APC_FIFO_INTR_MASK, ON_IRQ_APC0); // clear all interrupts
    } else if (ch_sel == APC_DCH_BMP_RIGHT) { // Right channel
        apc_ch_intr_disable(APC_DCH_TO_CH(dch, 1), APC_FIFO_INTR_MASK, ON_IRQ_APC0); // disable all interrupts
        *pdci->cfg_reg_p &= ~APC_DCH_R_EN;
        apc_ch_intr_clear(APC_DCH_TO_CH(dch, 1), APC_FIFO_INTR_MASK, ON_IRQ_APC0); // clear all interrupts
    }
    return CSK_DRIVER_OK;
}

int32_t apc_dual_channel_abort (APC_DCH dch)
{
    // Check if parameters are valid, i.e. APC channel is valid
    if (dch >= APC_DCH_COUNT || !DCH_IS_OWNED(dch))
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check if APC and specified channel are both initialized
    if (!IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
        return CSK_DRIVER_ERROR;

    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    if ((pdci->ch_sel & APC_DCH_BMP_MASK) == 0) {
        LOGD("%s: neither of APC L/R channels (dch=%d) is not selected!",
                __func__, dch);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

#if SUPPORT_I2S_ERR
    // check if I2S device
    bool is_i2s = (pdci->intf_type == APC_INTF_I2S_IN || pdci->intf_type == APC_INTF_I2S_OUT);
    // disable & clear i2s error interrupts for i2s
    if (is_i2s) {
        apc_i2s_err_intr_disable(pdci->intf_idx, ON_IRQ_APC0);
        apc_i2s_err_intr_clear(pdci->intf_idx, ON_IRQ_APC0);
    }
#endif

    // disable both APC channels and their all interrupts
    apc_dch_intr_disable(dch, APC_FIFO_INTR_MASK, ON_IRQ_APC0);

    *pdci->cfg_reg_p &= ~(APC_DCH_R_EN | APC_DCH_L_EN);
    *pdci->cfg_reg_p |= (APC_DCH_R_FLU | APC_DCH_L_FLU);// flush channels' FIFOs

    apc_dch_intr_clear(dch, APC_FIFO_INTR_MASK, ON_IRQ_APC0);

    int32_t ret0 = CSK_DRIVER_OK, ret = CSK_DRIVER_OK;
    uint8_t use_gpdma;
    if (pdci->busy_lr[0] != 0 || pdci->busy_lr[1] != 0) {
        if (pdci->busy_lr[0] != 0 && (pdci->rt_flag_lr[0] & RT_FLAG_USE_PIO) == 0) { // left channel, use DMA
            use_gpdma = (pdci->rt_flag_lr[0] & RT_FLAG_USE_GPDMA);
            if (use_gpdma == 0) { // CMNDMA is used
            #if USE_CMNDMA
                // abort and don't wait until done
                ret0 = dma_channel_disable(pdci->dma_ch_lr[0], false);
                //ret0 = dma_channel_disable(pdci->dma_ch_lr[0], true);
            #endif
            } else { // GPDMA2D is used
            #if USE_GPDMA2D
                ret0 = DMA2D_Stop(pdci->dma_ch_lr[0]);
            #endif
            } // end GPDMA2D is used

            pdci->busy_lr[0] = 0;
            if (ret0 != CSK_DRIVER_OK)
                ret = ret0;
        }
        if (pdci->busy_lr[1] != 0 && (pdci->rt_flag_lr[1] & RT_FLAG_USE_PIO) == 0) { // right channel, use DMA
            use_gpdma = (pdci->rt_flag_lr[1] & RT_FLAG_USE_GPDMA);
            if (use_gpdma == 0) { // CMNDMA is used
            #if USE_CMNDMA
                // abort and don't wait until done
                ret0 = dma_channel_disable(pdci->dma_ch_lr[1], false);
                //ret0 = dma_channel_disable(pdci->dma_ch_lr[1], true);
            #endif
            } else { // GPDMA2D is used
            #if USE_GPDMA2D
                ret0 = DMA2D_Stop(pdci->dma_ch_lr[1]);
            #endif
            } // end GPDMA2D is used

            pdci->busy_lr[1] = 0;
            if (ret0 != CSK_DRIVER_OK)
                ret = ret0;
        }

/*
        // disable both APC channels and their all interrupts
        apc_dch_intr_disable(dch, APC_FIFO_INTR_MASK, ON_IRQ_APC0);
        *pdci->cfg_reg_p &= ~(APC_DCH_R_EN | APC_DCH_L_EN);
        *pdci->cfg_reg_p |= (APC_DCH_R_FLU | APC_DCH_L_FLU);// flush channels' FIFOs
        apc_dch_intr_clear(dch, APC_FIFO_INTR_MASK, ON_IRQ_APC0);
*/

    } // Left or Right busy

    return ret;
}

/*
// en = 1, set NOT_SYNC_CACHE; en = 0, clear NOT_SYNC_CACHE
void apc_dual_channel_set_nsynca (APC_DCH dch, uint8_t en)
{
    // Check if parameters are valid, i.e. APC channel is valid
    if (dch >= APC_DCH_COUNT || !DCH_IS_OWNED(dch) ||
            !IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
        return;

    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    if (en) {
        pdci->rt_flag_lr[0] |= RT_FLAG_NSYNCA;
        pdci->rt_flag_lr[1] |= RT_FLAG_NSYNCA;
    } else {
        pdci->rt_flag_lr[0] &= ~RT_FLAG_NSYNCA;
        pdci->rt_flag_lr[1] &= ~RT_FLAG_NSYNCA;
    }
}
*/


/**
  \fn          uint32_t apc_channel_get_count (APC_CH ch)
  \brief       Get number of transferred data items
  \param[in]   ch Channel number
  \returns     Number of transferred data items
*/
uint32_t apc_channel_get_count (APC_CH ch)
{
    APC_DCH dch = APC_CH_TO_DCH(ch); // dual_channel
    uint8_t lr_idx = APC_CH_LR_IDX(ch); // 0 = left, 1 = right

    // Check if parameters are valid, i.e. APC channel is valid
    if (ch >= APC_CH_COUNT || !DCH_IS_OWNED(dch))
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check if APC and specified dual_channel are both initialized
    if (!IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
        return CSK_DRIVER_ERROR;

    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    uint32_t count = 0;
    if (pdci->busy_lr[lr_idx] != 0 && (pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_PIO) == 0) {
        uint8_t use_gpdma = (pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_GPDMA);
        if (use_gpdma == 0) { // CMNDMA is used
        #if USE_CMNDMA
            count = dma_channel_get_count(pdci->dma_ch_lr[lr_idx]);
            if (pdci->samp_bits == 16)
                count <<= 1; // WORD count => HALFWORD count
        #endif
        } else { // GPDMA2D is used
        #if USE_GPDMA2D
            DMA2D_Get_XferredCnt(pdci->dma_ch_lr[lr_idx], &count);
            count >>= pdci->dma_width_bits; // byte count => sample count
        #endif
        } // end GPDMA2D is used

//        if (pdci->samp_bits == 16)
//            count <<= 1; // WORD count => HALFWORD count
    } else {
        count = pdci->samps_lr[lr_idx];
    }

    return count;
}

uint32_t apc_dual_channel_get_count (APC_DCH dch)
{
    // Check if parameters are valid, i.e. APC dual_channel is valid
    if (dch >= APC_DCH_COUNT || !DCH_IS_OWNED(dch))
        return CSK_DRIVER_ERROR_PARAMETER;

    // Check if APC and specified dual_channel are both initialized
    if (!IS_APC_INITIALIZED() || !IS_APC_DCH_INITIALIZED(dch))
        return CSK_DRIVER_ERROR;

    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];

    // Only STEREO_MODE (mixed L/R channel data) is supported for dual_channel
    if (pdci->mix_mode != APC_DCH_MIXED_MODE) {
        LOGD("%s: NOT mixed mode, CANNOT get count of dual_channel!\r\n", __func__);
        return 0;
    }

    // mixed @ left or right channel
    uint8_t lr_idx = 0; //pdci->mix_target == 1 ? 1 : 0;
    uint32_t count = 0;

    if (pdci->busy_lr[lr_idx] != 0 && (pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_PIO) == 0) { // via left or right channel
        uint8_t use_gpdma = (pdci->rt_flag_lr[lr_idx] & RT_FLAG_USE_GPDMA);
        if (use_gpdma == 0) { // CMNDMA is used
        #if USE_CMNDMA
            count = dma_channel_get_count(pdci->dma_ch_lr[lr_idx]);
            if (pdci->samp_bits == 16)
                count <<= 1; // WORD count => HALFWORD count
        #endif
        } else { // GPDMA2D is used
        #if USE_GPDMA2D
            DMA2D_Get_XferredCnt(pdci->dma_ch_lr[lr_idx], &count);
            count >>= pdci->dma_width_bits; // byte count => sample count
        #endif
        } // end GPDMA2D is used

//        if (pdci->samp_bits == 16)
//            count <<= 1; // WORD count => HALFWORD count
    } else {
        return pdci->samps_lr[lr_idx];
    }

    return count;
}

/*
#define EQ_COEF_MAX_COUNT       50
#define EQ_STAGE_MAX_COUNT      10
int32_t apc_eq_set_coef_array(APC_DCH dch, uint32_t *eqcoefs, uint32_t num)
{
//    if (dch != APC_DCH_OUT0) {
//        CLOGW("%s: ONLY APC_TX_CH0 supports EQ function on ARCS!\n", __func__);
//        return CSK_DRIVER_ERROR_PARAMETER;
//    }
    assert(dch == APC_DCH_OUT0);
    if (num == 0 || num > EQ_COEF_MAX_COUNT) {
        CLOGW("%s: at most %d (cur=%d) EQ coefficients are supported!\n",
                __func__, EQ_COEF_MAX_COUNT, num);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    uint32_t i;
    uint32_t eqcoef_addr = APC_TX_CH0_EQCOEF_BASE;

    for (i = 0; i < EQ_COEF_MAX_COUNT; i++, eqcoef_addr += 4) {
        outw(eqcoef_addr, eqcoefs[i] & 0xFFFFFF);
    } // end for i

    return CSK_DRIVER_OK;
}

int32_t apc_eq_set_coef(APC_DCH dch, uint32_t index, uint32_t eqcoef)
{
    assert(dch == APC_DCH_OUT0);
    if (index >= EQ_COEF_MAX_COUNT) {
        CLOGW("%s: at most %d (index=%d) EQ coefficients are supported!\n",
                __func__, EQ_COEF_MAX_COUNT, index);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    outw(APC_TX_CH0_EQCOEF_BASE + (index << 2), eqcoef);
    return CSK_DRIVER_OK;
}

int32_t apc_eq_enable(APC_DCH dch, uint32_t stages)
{
    if (stages == 0 || stages > EQ_STAGE_MAX_COUNT) {
        CLOGW("%s: at most %d (cur=%d) EQ stages are supported!\n",
                __func__, EQ_STAGE_MAX_COUNT, stages);
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    assert(dch == APC_DCH_OUT0);
    struct APC_REG_APC_TX_CH0_CFG_BITS *tx_cfg_p;
    tx_cfg_p = (struct APC_REG_APC_TX_CH0_CFG_BITS *)APC_TX_CH0_CFG_ADDR;

    tx_cfg_p->EQ_STAGE = stages;
    tx_cfg_p->EQ_BYPASS_REG = 0;
    tx_cfg_p->EQ_CH_EN = 0x3; // enable both LEFT & RIGHT channel
    return CSK_DRIVER_OK;
}

int32_t apc_eq_disble(APC_DCH dch)
{
    assert(dch == APC_DCH_OUT0);
    struct APC_REG_APC_TX_CH0_CFG_BITS *tx_cfg_p;
    tx_cfg_p = (struct APC_REG_APC_TX_CH0_CFG_BITS *)APC_TX_CH0_CFG_ADDR;

    tx_cfg_p->EQ_BYPASS_REG = 1;
    tx_cfg_p->EQ_CH_EN = 0x0; // disable both LEFT & RIGHT channel
    return CSK_DRIVER_OK;
}

int32_t apc_eq_clear(APC_DCH dch, uint8_t wait_done)
{
    assert(dch == APC_DCH_OUT0);
    struct APC_REG_APC_TX_CH0_CFG_BITS *tx_cfg_p;
    tx_cfg_p = (struct APC_REG_APC_TX_CH0_CFG_BITS *)APC_TX_CH0_CFG_ADDR;

    tx_cfg_p->EQ_CLR = 1;
    if (wait_done) {
        while (tx_cfg_p->EQ_CLR_DONE == 0);
    }
    return CSK_DRIVER_OK;
}
*/

// reset APC TX and/or RX path (NOT reset registers, only internal circuit logic)
// bit[0] = 1, reset RX path; bit[1] = 1, reset TX path
void apc_reset_path(uint8_t rst_flag)
{
    switch (rst_flag & 0x3) {
    case 0x3: // both RX & TX
        //CSK_APC->REG_APC_CFG.all |= APC_RX_PATH_RESET_BIT | APC_TX_PATH_RESET_BIT;
        CSK_APC->REG_APC_RX_PATH_RESET.all = APC_RX_PATH_RESET_BIT;
        CSK_APC->REG_APC_TX_PATH_RESET.all = APC_TX_PATH_RESET_BIT;
        break;
    case 0x1: // RX
        //CSK_APC->REG_APC_CFG.all |= APC_RX_PATH_RESET_BIT;
        CSK_APC->REG_APC_RX_PATH_RESET.all = APC_RX_PATH_RESET_BIT;
        break;
    case 0x2: // TX
        //CSK_APC->REG_APC_CFG.all |= APC_TX_PATH_RESET_BIT;
        CSK_APC->REG_APC_TX_PATH_RESET.all = APC_TX_PATH_RESET_BIT;
        break;
    default:
        break;
    }
}


#if USE_GPDMA2D

/*
// Global mtime record array
#define MTIME_RECORD_SIZE 128   // Record buffer size, adjustable as needed
static uint64_t g_mtime_records[MTIME_RECORD_SIZE];
static uint32_t g_mtime_count = 0;
static uint32_t g_mtime_index = 0;
*/

_FAST_FUNC_RO static void
apc_gpdma2d_event(uint32_t event, void* workspace)
{
    uint32_t usr_param = (uint32_t) workspace;
    APC_DCH dch = (usr_param >> 8) & 0xFF;
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];

    // use Left channel if mixed L/R channel data
    uint8_t lr_idx = ((usr_param & APC_DCH_BMP_MASK) & APC_DCH_BMP_LEFT) ? 0 : 1;
    APC_CH ch = APC_DCH_TO_CH(dch, lr_idx);
    uint8_t dma_ch = pdci->dma_ch_lr[lr_idx];
    uint32_t tmp = 0;

    if (event & CSK_DMA2D_EVENT_BLOCK_DONE) { // block done
/*
        uint64_t current_mtime = SysTimer_GetLoadValue(); // Get current mtime timestamp
        // Simple mtime recording to global array
        if (g_mtime_count < MTIME_RECORD_SIZE) {
            g_mtime_records[g_mtime_count++] = current_mtime;
        } else {
            // Array full, use circular buffer overwrite
            g_mtime_records[g_mtime_index] = current_mtime;
            g_mtime_index = (g_mtime_index + 1) % MTIME_RECORD_SIZE;
        }
*/
        //DMA2D_Get_XferredCnt(pdci->dma_ch_lr[lr_idx], &tmp);
        //tmp >>= pdci->dma_width_bits; // byte count => sample count
        //pdci->samps_lr[lr_idx] = tmp;

        if (pdci->cb_event != NULL) {
            //tmp = (ch << 8) | APC_EVENT_BLOCK_COMPLETE;
            tmp = APC_EVI(APC_EVENT_BLOCK_COMPLETE, ch);
            pdci->cb_event(tmp, pdci->usr_param);
        }
    } // EVENT_BLOCK_DONE

    if (event & CSK_DMA2D_EVENT_TRANSFER_DONE) {

        //NOTE: CANNOT disable i2s err interrupts here for continuous transfers!!
    #if 0 //SUPPORT_I2S_ERR
        // To avoid I2S clock/timeout error reported at then end of data transfer
        // (I2S master may stop sending BCK/LRCK at that moment and then errors
        // are detected by I2S slave), disable I2S clock/timeout error here...
        //
        // disable i2s error interrupts if I2S device
        if (pdci->intf_type == APC_INTF_I2S_IN || pdci->intf_type == APC_INTF_I2S_OUT) {
            apc_i2s_err_intr_disable(pdci->intf_idx, ON_IRQ_APC0);
        }
    #endif

        pdci->busy_lr[lr_idx] = 0;

        // disable  APC channel and its interrupts
        //apc_ch_intr_disable(ch, APC_FIFO_INTR_MASK);
        //if (lr_idx == 0) { // Left channel
        //    pdci->cfg_reg_p->bit.CH_L_EN = 0;
        //} else { // Right channel
        //    pdci->cfg_reg_p->bit.CH_R_EN = 0;
        //}

        DMA2D_Get_XferredCnt(dma_ch, &tmp);
        tmp >>= pdci->dma_width_bits; // byte count => sample count
        pdci->samps_lr[lr_idx] = tmp;
        //LOGD("%s: XferCnt = %d\n", __func__, tmp);

        if (pdci->cb_event != NULL) {
            //tmp = (ch << 8) | APC_EVENT_BLOCK_COMPLETE;
            uint32_t tmp = APC_EVI(APC_EVENT_TRANSFER_COMPLETE, ch);
            pdci->cb_event(tmp, pdci->usr_param);
        }
    }
}

#endif // USE_GPDMA2D


#if USE_CMNDMA
_FAST_FUNC_RO static void
apc_cmndma_event(uint32_t event_info, uint32_t xfer_bytes, uint32_t usr_param)
{
    uint8_t event_type = event_info & 0xFF;
    APC_DCH dch = (usr_param >> 8) & 0xFF;
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];

    // use Left channel if mixed L/R channel data
    uint8_t lr_idx = ((usr_param & APC_DCH_BMP_MASK) & APC_DCH_BMP_LEFT) ? 0 : 1;
    //if (pdci->mix_target == 1) // mixed @ right channel
    //    lr_idx = 1;

    APC_CH ch = APC_DCH_TO_CH(dch, lr_idx);
    uint32_t tmp;

    switch (event_type) {
    case DMA_EVENT_BLOCK_COMPLETE:
        assert(pdci->dma_ch_lr[lr_idx] < DMA_NUMBER_OF_CHANNELS);
        //NOTE: size of each block has been recorded in DMA driver, so don't accumulate the size!
        //
        //tmp = dma_channel_get_count(pdci->dma_ch_lr[lr_idx]);
        //if (pdci->samp_bits == 16)
        //    tmp <<= 1; // WORD count => HALFWORD count
        //pdci->samps_lr[lr_idx] += tmp;

        if (pdci->cb_event != NULL) {
            //tmp = (ch << 8) | APC_EVENT_BLOCK_COMPLETE;
            tmp = APC_EVI(APC_EVENT_BLOCK_COMPLETE, ch);
            pdci->cb_event(tmp, pdci->usr_param);
        }
        break;

    case DMA_EVENT_TRANSFER_COMPLETE:

        //NOTE: CANNOT disable i2s err interrupts here for continuous transfers!!
    #if 0 //SUPPORT_I2S_ERR
        // To avoid I2S clock/timeout error reported at then end of data transfer
        // (I2S master may stop sending BCK/LRCK at that moment and then errors
        // are detected by I2S slave), disable I2S clock/timeout error here...
        //
        // disable i2s error interrupts if I2S device
        if (pdci->intf_type == APC_INTF_I2S_IN || pdci->intf_type == APC_INTF_I2S_OUT) {
            apc_i2s_err_intr_disable(pdci->intf_idx, ON_IRQ_APC0);
            apc_i2s_err_intr_clear(pdci->intf_idx, ON_IRQ_APC0);
        }
    #endif

        //FIXME: is it OK to replace dma_channel_get_count() call with xfer_bytes?
        assert(pdci->dma_ch_lr[lr_idx] < DMA_NUMBER_OF_CHANNELS);
        tmp = dma_channel_get_count(pdci->dma_ch_lr[lr_idx]);
        if (pdci->samp_bits == 16)
            tmp <<= 1; // WORD count => HALFWORD count
        pdci->samps_lr[lr_idx] = tmp;
        pdci->busy_lr[lr_idx] = 0;

        // disable & clear  APC channel's interrupts
        //apc_ch_intr_disable(ch, APC_FIFO_INTR_MASK, ON_IRQ_APC0);
        //apc_ch_intr_clear(ch, APC_FIFO_INTR_MASK, ON_IRQ_APC0);

        // disable APC channel
        //if (lr_idx == 0) { // Left channel
        //    pdci->cfg_reg_p->bit.CH_L_EN = 0;
        //} else { // Right channel
        //    pdci->cfg_reg_p->bit.CH_R_EN = 0;
        //}

        if (pdci->cb_event != NULL) {
            //tmp = (ch << 8) | APC_EVENT_TRANSFER_COMPLETE;
            tmp = APC_EVI(APC_EVENT_TRANSFER_COMPLETE, ch);
            pdci->cb_event(tmp, pdci->usr_param);
        }
        break;

    case DMA_EVENT_ERROR:
        //TODO: do something here?
        pdci->busy_lr[lr_idx] = 0;
        if (pdci->cb_event != NULL) {
            //tmp = (APC_DCH_TO_CH(dch, lr_idx) << 8) | APC_EVENT_DMA_ERROR;
            tmp = APC_EVI(APC_EVENT_DMA_ERROR, APC_DCH_TO_CH(dch, lr_idx));
            pdci->cb_event(tmp, pdci->usr_param);
        }
        break;

    default:
        break;
    }
}
#endif // USE_CMNDMA


#if SUPPORT_APC_PIO
_FAST_FUNC_RO static uint32_t apc_read_rx_fifo(APC_DCH dch, uint8_t lr_idx, int32_t xn)
{
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    uint8_t i, cnt_to_read, lcnt = 0, rcnt = 0;
    uint32_t data_addr;

    uint32_t *rbuf = pdci->sambuf_lr[lr_idx];
    uint32_t samps = pdci->samps_lr[lr_idx];

    // start point to write
    if (pdci->samp_bits == 16) {
        rbuf += samps >> 1;
        xn >>= 1;
    } else {
        rbuf += samps;
    }

    GET_APC_DCH_FIFO_CNT(dch, &lcnt, &rcnt);
    if (lr_idx == 0) { // left or L/R mixed
        data_addr = pdci->fixed_lr[0]->data_addr;
        cnt_to_read = lcnt;
        if (pdci->mix_mode == 1) {
            cnt_to_read += rcnt;
            // only high 16bit of WORD is valid, and
            // decrease to half for stereo, 16bit case
            if (pdci->samp_bits == 16)
                cnt_to_read >>= 1;
        }
    } else { // right
        data_addr = pdci->fixed_lr[1]->data_addr;
        cnt_to_read = rcnt;
    }

    // read count
    if (cnt_to_read > xn)
        cnt_to_read = xn;

    for (i = 0; i < cnt_to_read; i++) {
        *rbuf++ = inw(data_addr);
    }

    if (pdci->samp_bits == 16)
        cnt_to_read <<= 1;
    pdci->samps_lr[lr_idx] += cnt_to_read;

    return cnt_to_read;
}

// xn = samples requested to transfer
_FAST_FUNC_RO static uint32_t apc_write_tx_fifo(APC_DCH dch, uint8_t lr_idx, int32_t xn)
{
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    uint8_t i, cnt_to_write, lcnt = 0, rcnt = 0;
    uint32_t data_addr;

    GET_APC_DCH_FIFO_CNT(dch, &lcnt, &rcnt);
    if (lr_idx == 0) { // left or L/R mixed
        data_addr = pdci->fixed_lr[0]->data_addr;
        cnt_to_write = pdci->fixed_lr[0]->fifo_depth - lcnt;
        if (pdci->mix_mode == 1)
            cnt_to_write += pdci->fixed_lr[1]->fifo_depth - rcnt;
    } else { // right
        data_addr = pdci->fixed_lr[1]->data_addr;
        cnt_to_write = pdci->fixed_lr[1]->fifo_depth - rcnt;
    }

    const uint32_t *wbuf = pdci->sambuf_lr[lr_idx];
    uint32_t samps = pdci->samps_lr[lr_idx];

    // start point to write
    if (pdci->samp_bits == 16) {
        // only high 16bit of WORD is valid for R channel FIFO, and
        // only low 16bit of WORD is valid for L channel FIFO, and
        // decrease to half for stereo, 16bit case
        if (pdci->mix_mode == 1)
            cnt_to_write >>= 1;
        wbuf += samps >> 1;
        xn >>= 1;
    } else {
        wbuf += samps;
    }

    // write count
    if (cnt_to_write > xn)
        cnt_to_write = xn;

    for (i = 0; i < cnt_to_write; i++) {
        outw(data_addr, *wbuf++);
    }

    if (pdci->samp_bits == 16)
        cnt_to_write <<= 1;
    pdci->samps_lr[lr_idx] += cnt_to_write;

    return cnt_to_write;
}

#endif // SUPPORT_APC_PIO

// get current sample count in the specified APC L/R FIFO or both (if mixed)
uint32_t apc_get_fifo_samp_cnt(APC_DCH dch, uint8_t chbmp)
{
    APC_DCH_INFO *pdci = &g_apc_dev.dch_array[dch];
    uint32_t samp_cnt = 0;
    uint8_t lcnt = 0, rcnt = 0;

    GET_APC_DCH_FIFO_CNT(dch, &lcnt, &rcnt);
    if ((chbmp & APC_DCH_BMP_LEFT) == APC_DCH_BMP_LEFT) { // left or L/R mixed
        samp_cnt = lcnt;
        if (pdci->mix_mode == 1)
            samp_cnt += rcnt;
    } else { // right
        samp_cnt = rcnt;
    }

    if (pdci->samp_bits == 16) {
        // For 16bit, stereo mode, TX,
        // only high 16bit of WORD is valid for R channel FIFO, and
        // only low 16bit of WORD is valid for L channel FIFO.
        // For 16bit, stereo mode, RX,
        // only high 16bit of WORD is valid for L or R channel FIFO.
        // For 16bit , mono mode, whether TX or RX,
        // two 16bit samples make one WORD (one entry of channel FIFO).
        if (pdci->mix_mode == 0)
            samp_cnt <<= 1;
    }
    return samp_cnt;
}


// reset APC L/R FIFO or both (if mixed)
uint32_t apc_reset_fifo(APC_DCH dch, uint8_t chbmp)
{
    // Check if parameters are valid, i.e. APC dual_channel is valid
    if (dch >= APC_DCH_COUNT || !DCH_IS_OWNED(dch))
        return CSK_DRIVER_ERROR_PARAMETER;

    volatile uint32_t *cfg_reg_p= (uint32_t *)g_dch_cfg_regs[dch];

    chbmp &= APC_DCH_BMP_STEREO;
    switch(chbmp) {
    case APC_DCH_BMP_LEFT:
        *cfg_reg_p |= APC_DCH_L_FLU;
        break;
    case APC_DCH_BMP_RIGHT:
        *cfg_reg_p |= APC_DCH_R_FLU;
        break;
    case APC_DCH_BMP_STEREO:
        *cfg_reg_p |= APC_DCH_R_FLU | APC_DCH_L_FLU;
        break;
    default:
        break;
    }

    return CSK_DRIVER_OK;
}


/**
  \fn          void apc_irq_handler (void)
  \brief       APC interrupt handler
*/
_FAST_FUNC_RO static void apc_irq_handler (void)
{
    //TODO: Read interrupt status register, and check if
    // there are following interrupt conditions:
    // 1) FIFO is ready to transfer (TX below threshold or RX above threshold)
    // 2) TX FIFO Empty
    // 3) RX FIFO Full
    // 4) TX FIFO Underflow (no data in FIFO, and send is ongoing)
    // 5) RX FIFO Overflow (FIFO is full, and recv is ongoing)

    uint32_t i, event_info, status, status_raw, i2s_err;
    APC_DCH_INFO *pdci;
    uint8_t dch, lr_idx, mixed;

    for (i = 0; i < APC_CH_COUNT; i++) {
        if (i == APC_CH_OUT0_R) continue;  // NO APC_CH_OUT0_R, skip it!!

        status_raw = apc_ch_intr_raw_status(i, ON_IRQ_APC0);
        status = apc_ch_intr_status(i, ON_IRQ_APC0);

        dch = APC_CH_TO_DCH(i);
        pdci = &g_apc_dev.dch_array[dch];
        i2s_err = 0;

    #if SUPPORT_I2S_ERR
        // Check i2s error or timeout only on LEFT channel (NOT on RIGHT channel!)
        if (!(i & 0x1) && (pdci->intf_type == APC_INTF_I2S_IN || pdci->intf_type == APC_INTF_I2S_OUT)) {
            i2s_err = apc_i2s_err_intr_status(pdci->intf_idx, ON_IRQ_APC0);
        }
    #endif

        if (!status && !IS_SET_I2S_INTR(i2s_err))    continue;

        mixed = pdci->mix_mode;
        lr_idx = APC_CH_LR_IDX(i);

        //BSD: reset event_info for each APC channel, and change report way, that is,
        // report one channel's all interrupt status in one event_callback call
        event_info = 0;

    #if SUPPORT_I2S_ERR
        // check if there's any I2S clock glitch or timeout (slave) error
        if (IS_SET_I2S_INTR(i2s_err)) {
            if(IS_SET_I2S_ERR(i2s_err)) {
                //event_info |= (i << 8) | APC_EVENT_I2S_ERROR;
                if(IS_SET_I2S_ERR_LRCK(i2s_err))
                    event_info |= APC_EVENT_I2S_ERR_LRCK;
                else
                    event_info |= APC_EVENT_I2S_ERR_BCK;
            }
            if(IS_SET_I2S_TIMEOUT(i2s_err)) {
                //event_info |= (i << 8) | APC_EVENT_I2S_TIMEOUT;
                event_info |= APC_EVENT_I2S_TIMEOUT;
            }
        }
    #endif //SUPPORT_I2S_ERR

        // PIO: FIFO reach to threshold
        if (IS_SET_READY_TO_XFER(status)) {
        #if SUPPORT_APC_PIO
            apc_ch_intr_clear(i, APC_INTR_READY_TO_XFER);
            int32_t req_samps = pdci->samcnt_lr[lr_idx] - pdci->samps_lr[lr_idx];
            if (req_samps > 0) {
                if (g_chs_fixed[i].ch_dir == 0) //TX
                    apc_write_tx_fifo(dch, lr_idx, req_samps);
                else //RX
                    apc_read_rx_fifo(dch, lr_idx, req_samps);

                if (pdci->samps_lr[lr_idx] >= pdci->samcnt_lr[lr_idx]) {
                    pdci->samcnt_lr[lr_idx] = 0; // clear requested samples count
                    apc_ch_intr_disable(i, APC_INTR_READY_TO_XFER, ON_IRQ_APC0);
                    //apc_ch_intr_clear(i, APC_INTR_READY_TO_XFER, ON_IRQ_APC0);
                    pdci->busy_lr[lr_idx] = 0;
                    //event_info |= (i << 8) | APC_EVENT_TRANSFER_COMPLETE;
                    event_info |= APC_EVENT_TRANSFER_COMPLETE;
                }
            }
        #endif // SUPPORT_APC_PIO
        }

        // RX FIFO full
        if ( i < APC_CH_IN_COUNT && IS_SET_RX_FULL (status) ) {
            // re-check current FIFO count, do nothing if NOT FULL now
            apc_ch_intr_clear(i, APC_INTR_FIFO_FULL, ON_IRQ_APC0);
            if ( APC_CH_FIFO_CNT(i) == APC_IN_FIFO_DEPTH_DEF ) {
                apc_ch_intr_disable(i, APC_INTR_FIFO_FULL, ON_IRQ_APC0);
                //CLOGW("APC CH %d FULL! mask INT\n", i);
                //event_info |= (i << 8) | APC_EVENT_RX_FIFO_FULL;
                event_info |= APC_EVENT_RX_FIFO_FULL;
            }

/*
            //FOR TEST ONLY
            uint32_t j, rx_data[8];
            for (j=0; j<8; j++) {
                rx_data[j] = inw(pdci->fixed_lr[0]->data_addr);
            }
            CLOGW("APC CH %d FULL! \n0x%08x 0x%08x 0x%08x 0x%08x", i, rx_data[0],rx_data[1],rx_data[2],rx_data[3]);
            CLOGW("0x%08x 0x%08x 0x%08x 0x%08x \r\n", rx_data[4],rx_data[5],rx_data[6],rx_data[7]);
*/
        }

        // RX FIFO overflow
        if ( IS_SET_RX_OVERRUN (status) ) {
            //BSD: DON'T disable channels on initiative, and let upper layer
            // decide whether to disable channel via abort API...
            /*
            if (mixed) { // both Left & Right channels
                *pdci->cfg_reg_p &= ~(APC_DCH_R_EN(dch) | APC_DCH_L_EN(dch));
            } else if (lr_idx == 0) { // Left channel
                *pdci->cfg_reg_p &= ~APC_DCH_L_EN(dch);
            } else { // Right channel
                *pdci->cfg_reg_p &= ~APC_DCH_R_EN(dch);
            }
            */
            apc_ch_intr_disable(i, APC_INTR_FIFO_OVERRUN, ON_IRQ_APC0);
            apc_ch_intr_clear(i, APC_INTR_FIFO_OVERRUN | APC_INTR_FIFO_DMA_REQ, ON_IRQ_APC0);
//            CLOGW("APC CH %d OVERRUN! disable CH(mixed=%d)\n", i, mixed);

            //event_info |= (i << 8) | APC_EVENT_RX_FIFO_OVERRUN;
            event_info |= APC_EVENT_RX_FIFO_OVERRUN;
        }

        // TX FIFO empty
        if ( i >= APC_CH_IN_COUNT && IS_SET_TX_EMPTY (status) ) { // OUT
            // re-check current FIFO count, do nothing if NOT EMPTY now
            apc_ch_intr_clear(i, APC_INTR_FIFO_EMPTY, ON_IRQ_APC0);
            if ( APC_CH_FIFO_CNT(i) == 0 ) {
                apc_ch_intr_disable(i, APC_INTR_FIFO_EMPTY, ON_IRQ_APC0);
                //CLOGW("APC CH %d EMPTY! mask INT\n", i);
                //event_info |= (i << 8) | APC_EVENT_TX_FIFO_EMPTY;
                event_info |= APC_EVENT_TX_FIFO_EMPTY;
            }
        }

        // TX FIFO underflow
        if ( IS_SET_TX_UNDERRUN (status) ) {
            // According to ZhaoRui, for mixed mode, all interrupts occur on the specified channel,
            // that is, interrupts NEVER occur on the other channel
            //
            //BSD: DON'T disable channels on initiative, and let upper layer
            // decide whether to disable channel via abort API...
            /*
            if (mixed) { // both Left & Right channels
                *pdci->cfg_reg_p &= ~(APC_DCH_R_EN | APC_DCH_L_EN);
            } else if (lr_idx == 0) { // Left channel
                *pdci->cfg_reg_p &= ~APC_DCH_L_EN;
            } else { // Right channel
                *pdci->cfg_reg_p &= ~APC_DCH_R_EN;
            }
            */
            apc_ch_intr_disable(i, APC_INTR_FIFO_UNDERRUN, ON_IRQ_APC0);
            apc_ch_intr_clear(i, APC_INTR_FIFO_UNDERRUN | APC_INTR_FIFO_DMA_REQ, ON_IRQ_APC0);
//            CLOGW("APC CH %d UNDERRUN! disable CH(mixed=%d)\n", i, mixed);

            //event_info |= (i << 8) | APC_EVENT_TX_FIFO_UNDERRUN;
            event_info |= APC_EVENT_TX_FIFO_UNDERRUN;
        }

        if (pdci->cb_event != NULL && event_info != 0) {
            event_info = APC_EVI(event_info, i);
            pdci->cb_event(event_info, pdci->usr_param);
        }

        if (status_raw != 0)
            apc_ch_intr_clear(i, status_raw, ON_IRQ_APC0);

    #if SUPPORT_I2S_ERR
        if (IS_SET_I2S_INTR(i2s_err))
            apc_i2s_err_intr_clear(pdci->intf_idx, ON_IRQ_APC0);
    #endif

    } // end for i (each APC channel)

//    // Clear specified interrupts
//    CSK_APC->REG_APC_INTR_RX_CLR.all = rx_status_raw;
//    CSK_APC->REG_APC_INTR_TX_CLR.all = tx_status_raw;
}
