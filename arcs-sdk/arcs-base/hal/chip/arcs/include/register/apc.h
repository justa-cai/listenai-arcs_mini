/*
 * apc.h
 *
 *
 */

#ifndef __APC_ARCS_H
#define __APC_ARCS_H

#define     ARCS_A0_SOC     0
#define     ARCS_B0_SOC     1
#define     ARCS_C0_SOC     2
#define     ARCS_D0_SOC     3
#define     ARCS_MP_SOC     ARCS_D0_SOC

#define ARCS_VER            ARCS_MP_SOC
#define SUPPORT_APC_PIO     0 //1 //1: CPU read/write data from/to FIFO directly and ignore USE_GPDMA!!
#define USE_GPDMA           1 // 1: use GPDMA, 0: use DW_DMAC

#include "arcs_ap.h"
#include "ClockManager.h"
//#include "cmn_sysctrl_reg.h" // for sysctrl regigers (clock etc.)
//#include "audpll_ctrl_reg.h" // for audpll registers

#if (ARCS_VER > ARCS_B0_SOC)
#include "ap_cfg_reg.h"
#endif

#if USE_GPDMA
#include "Driver_GPDMA.h"

#define DMA_CHANNEL_ANY         0xFF

#define DMA_BSIZE_1             (0)  // Burst size = 1
#define DMA_BSIZE_4             (1)  // Burst size = 4
#define DMA_BSIZE_8             (2)  // Burst size = 8
#define DMA_BSIZE_16            (3)  // Burst size = 16

#define DMA_WIDTH_BYTE          gpdma_sample_unit_byte // (0) // Width = 1 byte (8 bits)
#define DMA_WIDTH_HALFWORD      gpdma_sample_unit_halfword // (1) // Width = 2 bytes (16 bits)
#define DMA_WIDTH_WORD          gpdma_sample_unit_word // (2) // Width = 4 bytes (32 bits)

//default or preferred DMA channel definition
#define DMA_CH_ADC01L_DEF       ((uint8_t) 0)   // ADC: audio recording
#define DMA_CH_ADC01R_DEF       ((uint8_t) 4)   // ADC: audio recording
#define DMA_CH_DAC01L_DEF       ((uint8_t) 3)   // DAC: audio playback
#define DMA_CH_DAC01R_DEF       ((uint8_t) 5)   // DAC: audio playback
#define DMA_CH_I2S_RXL_DEF      ((uint8_t) 1)   // I2S recording
#define DMA_CH_I2S_RXR_DEF      ((uint8_t) 4)   // I2S recording
#define DMA_CH_I2S_TXL_DEF      ((uint8_t) 2)   // I2S playback
#define DMA_CH_I2S_TXR_DEF      ((uint8_t) 5)   // I2S playback
#define DMA_CH_ECHO_RXL_DEF     ((uint8_t) 1)   // Echo (digital)
#define DMA_CH_ECHO_RXR_DEF     ((uint8_t) 5)   // Echo (digital)

// Source Gather / Destination Scatter register definition
// Interval & Count are both in units of SRC_TR_WIDTH bits.
#define SG_INTERVAL_POS     (0)             // Interval @ bit[19:0]
#define SG_INTERVAL_MASK    (0xFFFFF << 0)  // Interval mask
#define SG_COUNT_POS        (20)            // Count @ bit[31:20]
#define SG_COUNT_MASK       (0xFFF << 20)   // Count mask
#define SG_COUNT(n)         ((n >> SG_COUNT_POS) & 0xFFF)
#define SG_INTERVAL(n)      ((n >> SG_INTERVAL_POS) & 0xFFFFF)

#else // DW_DMAC
#include "dma.h"

//default or preferred DMA channel definition //TODO: move them into dma_res.h
#define DMA_CH_ADC01L_DEF       ((uint8_t) 0)   // ADC: audio recording
#define DMA_CH_ADC01R_DEF       ((uint8_t) 4)   // ADC: audio recording
#define DMA_CH_DAC01L_DEF       ((uint8_t) 3)   // DAC: audio playback
#define DMA_CH_DAC01R_DEF       ((uint8_t) 5)   // DAC: audio playback
#define DMA_CH_I2S_RXL_DEF      ((uint8_t) 1)   // I2S recording
#define DMA_CH_I2S_RXR_DEF      ((uint8_t) 4)   // I2S recording
#define DMA_CH_I2S_TXL_DEF      ((uint8_t) 2)   // I2S playback
#define DMA_CH_I2S_TXR_DEF      ((uint8_t) 5)   // I2S playback
#define DMA_CH_ECHO_RXL_DEF     ((uint8_t) 1)   // Echo (digital)
#define DMA_CH_ECHO_RXR_DEF     ((uint8_t) 5)   // Echo (digital)

#endif // USE_GPDMA

// APC data interface: ADC, DAC, I2S(IN & OUT), and Echo
typedef enum {
    APC_INTF_UNSET = 0, // default state
    //APC_INTF_VAD,       // [IN] VAD
    //APC_INTF_ADC,       // [IN] ADC
    APC_INTF_ADC_PDM,   // [IN] ADC_PDM (BSD:including PDM for DMIC)
    APC_INTF_DAC,       // [OUT] DAC
    APC_INTF_I2S_IN,    // [IN] I2S
    APC_INTF_I2S_OUT,   // [OUT] I2S
    APC_INTF_ECHO,      // [IN] Echo (hidden, from output audio)
    APC_INTF_TYPE_COUNT
} APC_INTF_TYPE;

// APC interface index of each interface type
typedef enum {
    APC_INTF_IDX0 = 0,
    //APC_INTF_IDX_VAD = APC_INTF_IDX0,
    //APC_INTF_IDX_ECHO = APC_INTF_IDX0,
    APC_INTF_IDX_ADC01 = APC_INTF_IDX0,
    APC_INTF_IDX_DAC = APC_INTF_IDX0,
    APC_INTF_IDX_I2S0 = APC_INTF_IDX0,
    APC_INTF_IDX_ECHO0 = APC_INTF_IDX0, // for APC_DCH_OUT0

    APC_INTF_IDX1,
    APC_INTF_IDX_I2S1 = APC_INTF_IDX1,
    APC_INTF_IDX_ECHO1 = APC_INTF_IDX1, // for APC_DCH_OUT1

} APC_INTF_INDEX;

// APC dual_channel: 2 IN/ECHO Dual_FIFOs (IN0 ~ IN1), 2 OUT Dual_FIFOs (OUT0 ~ OUT1)
// NOTE: DCH_ECHO0 indicates ECHO of DCH_OUT0, DCH_ECHO1 indicates ECHO of DCH_OUT1
typedef enum {
    APC_DCH_IN0 = 0, // [IN] FIFO 0 & 1
    //APC_DCH_VAD = APC_DCH_IN0,
    APC_DCH_ADC01 = APC_DCH_IN0,
    APC_DCH_I2S0_IN = APC_DCH_IN0,
#if (ARCS_VER < ARCS_D0_SOC)
    APC_DCH_ECHO0 = APC_DCH_IN0,
#else // D0
    APC_DCH_ECHO1 = APC_DCH_IN0,
#endif
    APC_DCH_ADC_MIN = APC_DCH_IN0,
    APC_DCH_ADC_MAX = APC_DCH_IN0,
    APC_DCH_I2S_IN_MIN = APC_DCH_IN0,
    APC_DCH_ECHO_MIN = APC_DCH_IN0,
    APC_DCH_IN_MIN = APC_DCH_IN0,

    APC_DCH_IN1,     // [IN] FIFO 2 & 3
    APC_DCH_I2S1_IN = APC_DCH_IN1,
#if (ARCS_VER < ARCS_D0_SOC)
    APC_DCH_ECHO1 = APC_DCH_IN1,
#else // D0
    APC_DCH_ECHO0 = APC_DCH_IN1,
#endif
    APC_DCH_I2S_IN_MAX = APC_DCH_IN1,
    APC_DCH_ECHO_MAX = APC_DCH_IN1,
    APC_DCH_IN_MAX = APC_DCH_IN1,

    APC_DCH_OUT0,    // [OUT] FIFO 4 & 5
    APC_DCH_DAC01 = APC_DCH_OUT0, // DAC0 only, DAC1 NOT implemented
    APC_DCH_I2S0_OUT = APC_DCH_OUT0,
    APC_DCH_DAC_MIN = APC_DCH_OUT0,
    APC_DCH_DAC_MAX = APC_DCH_OUT0,
    APC_DCH_I2S_OUT_MIN = APC_DCH_OUT0,
    APC_DCH_OUT_MIN = APC_DCH_OUT0,

    APC_DCH_OUT1,    // [OUT] FIFO 6 & 7
    APC_DCH_I2S1_OUT = APC_DCH_OUT1,
    APC_DCH_I2S_OUT_MAX = APC_DCH_OUT1,
    APC_DCH_OUT_MAX = APC_DCH_OUT1,

    APC_DCH_COUNT
} APC_DCH; // DCH = Dual CHannel (Left + Right)


// APC channel: 4 IN/ECHO FIFOs (IN0_L, IN0_R, IN1_L, IN1_R),
//          4 OUT FIFOs (OUT0_L, OUT0_R, OUT1_L, OUT1_R)
typedef enum {
    APC_CH_IN0_L = 0, // [IN] FIFO 0
    APC_CH_ADC0 = APC_CH_IN0_L,
    APC_CH_I2S0_IN_L = APC_CH_IN0_L,
#if (ARCS_VER < ARCS_D0_SOC)
    APC_CH_ECHO0_L = APC_CH_IN0_L,
#else // D0
    APC_CH_ECHO1_L = APC_CH_IN0_L,
#endif
    APC_CH_ADC_MIN = APC_CH_IN0_L, // Min. ADC channel
    APC_CH_I2S_IN_MIN = APC_CH_IN0_L, // Min. I2S IN channel
    APC_CH_ECHO_MIN = APC_CH_IN0_L, // Min. ECHO channel
    APC_CH_IN_MIN = APC_CH_IN0_L,

    APC_CH_IN0_R,     // [IN] FIFO 1
    APC_CH_ADC1 = APC_CH_IN0_R,
    APC_CH_I2S0_IN_R = APC_CH_IN0_R,
#if (ARCS_VER < ARCS_D0_SOC)
    APC_CH_ECHO0_R = APC_CH_IN0_R,
#else // D0
    APC_CH_ECHO1_R = APC_CH_IN0_R,
#endif
    APC_CH_ADC_MAX = APC_CH_IN0_R, // Max. ADC channel

    APC_CH_IN1_L,     // [IN] FIFO 2
    APC_CH_I2S1_IN_L = APC_CH_IN1_L,
#if (ARCS_VER < ARCS_D0_SOC)
    APC_CH_ECHO1_L = APC_CH_IN1_L,
#else //D0
    APC_CH_ECHO0_L = APC_CH_IN1_L,
#endif

    APC_CH_IN1_R,     // [IN] FIFO 3
    APC_CH_I2S1_IN_R = APC_CH_IN1_R,
#if (ARCS_VER < ARCS_D0_SOC)
    APC_CH_ECHO1_R = APC_CH_IN1_R,
#else //D0
    APC_CH_ECHO0_R = APC_CH_IN1_R,
#endif
    APC_CH_I2S_IN_MAX = APC_CH_IN1_R, // Max. I2S IN channel
    APC_CH_ECHO_MAX = APC_CH_IN1_R, // Max. ECHO channel
    APC_CH_IN_MAX = APC_CH_IN1_R,

    APC_CH_OUT0_L,    // [OUT] FIFO 4
    APC_CH_DAC0 = APC_CH_OUT0_L,
    APC_CH_I2S0_OUT_L = APC_CH_OUT0_L,
    APC_CH_DAC_MIN = APC_CH_OUT0_L, // Min. DAC channel
    APC_CH_I2S_OUT_MIN = APC_CH_OUT0_L, // Min. I2S OUT channel
    APC_CH_OUT_MIN = APC_CH_OUT0_L,

    APC_CH_OUT0_R,    // [OUT] FIFO 5
    //APC_CH_DAC1 = APC_CH_OUT0_R, // ONLY 1 DAC on ARCS!
    APC_CH_I2S0_OUT_R = APC_CH_OUT0_R,
    APC_CH_DAC_MAX = APC_CH_OUT0_R, // Max. DAC channel

    APC_CH_OUT1_L,    // [OUT] FIFO 6
    APC_CH_I2S1_OUT_L = APC_CH_OUT1_L,

    APC_CH_OUT1_R,    // [OUT] FIFO 7
    APC_CH_I2S1_OUT_R = APC_CH_OUT1_R,
    APC_CH_I2S_OUT_MAX = APC_CH_OUT1_R, // Max. I2S OUT channel
    APC_CH_OUT_MAX = APC_CH_OUT1_R,

    APC_CH_COUNT
} APC_CH;


// APC Dual_channel count & channel count
#define APC_DCH_IN_COUNT    (APC_DCH_IN_MAX - APC_DCH_IN_MIN + 1)
#define APC_DCH_OUT_COUNT   (APC_DCH_OUT_MAX - APC_DCH_OUT_MIN + 1)
#define APC_CH_IN_COUNT     (APC_CH_IN_MAX - APC_CH_IN_MIN + 1)
#define APC_CH_OUT_COUNT    (APC_CH_OUT_MAX - APC_CH_OUT_MIN + 1)

// APC channel bitmap
#define APC_DCH_BMP_LEFT                (0x1 << 0)
#define APC_DCH_BMP_RIGHT               (0x1 << 1)
#define APC_DCH_BMP_STEREO              (APC_DCH_BMP_LEFT | APC_DCH_BMP_RIGHT)
#define APC_DCH_BMP_MASK                APC_DCH_BMP_STEREO // 0x3

// APC channel <=> APC dual_channel
#define APC_CH_TO_DCH(ch)           (ch >> 1)
#define APC_CH_LR_IDX(ch)           (ch & 0x1)
#define APC_CH_LR_BMP(ch)           ((ch & 0x1) ? APC_DCH_BMP_RIGHT : APC_DCH_BMP_LEFT)
#define APC_DCH_TO_CH(dch, lr_idx)  ((dch << 1) + (lr_idx & 0x1))
#define APC_IS_LEFT_CH(ch)          ((ch & 0x1) ? 0 : 1)
#define APC_IS_RIGHT_CH(ch)         ((ch & 0x1) ? 1 : 0)

// channel mode
typedef enum {
    APC_CHMODE_16BITS = 0,      // 16-bit mono mode, {L1, L0} or {R1, R0} or {R1, L0}
    APC_CHMODE_24BITS_LOW = 1,  // 24-bit mono mode, {8'd0, L0} or {8'd0, R0}
    APC_CHMODE_32BITS = 2,      // 32-bit mono mode, L0 or R0
    APC_CHMODE_24BITS_HIGH = 3, // 24-bit mono mode, {L0, 8'd0} or {R0, 8'd0}
    APC_CHMODE_COUNT
} APC_CHMODE;

// APC Event Type, used by APC callback CSK_APC_SignalEvent_t (see below)
// an APC channel read/write is completed
#define APC_EVENT_TRANSFER_COMPLETE     (0x1UL << 0)
// RX FIFO overflow is found during an APC channel read/write
#define APC_EVENT_RX_FIFO_OVERRUN       (0x1UL << 1)
// TX FIFO underflow is found during an APC channel read/write
#define APC_EVENT_TX_FIFO_UNDERRUN      (0x1UL << 2)
// RX FIFO full is found during an APC channel read/write
#define APC_EVENT_RX_FIFO_FULL          (0x1UL << 3)
// TX FIFO empty is found during an APC channel read/write
#define APC_EVENT_TX_FIFO_EMPTY         (0x1UL << 4)

// an APC channel block read/write is completed
// (based on GPDMA PingPong Done or DMA block interrupt)
// refer to bit[23:16] @ event_info for detailed information
#define APC_EVENT_PIPO_DONE             (0x1UL << 5)
#define APC_EVENT_BLOCK_COMPLETE        (0x1UL << 5)

// I2S signal error, e.g. glitch on BCK/LRCK signal
#define APC_EVENT_I2S_ERROR             (0x1UL << 6)

// a DMA error is found during an APC channel read/write
#define APC_EVENT_DMA_ERROR             (0x1UL << 7)


/**
 \fn          void CSK_APC_SignalEvent_t (uint32_t event, uint32_t usr_param)
 \brief       Signal APC Events.
 \param[in]   event_info APC event and channel information
              bit[7:0] is event type, bit[15:8] is APC channel no.
              bit[23:16] is about DMA PingPong done information when event type is APC_EVENT_PIPO_DONE.
                bit[16]: 1 indicates Ping transfer is done;
                bit[17]: 1 indicates Pong transfer is done;

 \param[in]   usr_param    user parameter specified in read / write operation.
 \return      none
*/

// PingPong DMA transfer bits of event_info
#define PIPO_PING_XFER_DONE     (0x1 << 16)
#define PIPO_PONG_XFER_DONE     (0x1 << 17)
#define PIPO_XFER_DONE     (PIPO_PING_XFER_DONE | PIPO_PONG_XFER_DONE)
#define BLOCK_XFER_DONE    (PIPO_PING_XFER_DONE | PIPO_PONG_XFER_DONE)

typedef void
(*CSK_APC_SignalEvent_t)(uint32_t event_info, uint32_t usr_param);


/**
  \fn          int32_t apc_initialize (void)
  \brief       Initialize Audio Processing Center
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
int32_t apc_initialize (void);


/**
  \fn          int32_t apc_uninitialize (void)
  \brief       De-initialize Audio Processing Center
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
int32_t apc_uninitialize (void);


/**
  \fn          int32_t apc_channel_setup ();
  \brief       Configure APC channel to transfer data for ADC(AMIC), PDM(DMIC), DAC, and I2S etc.
               NOTE: the last setup is valid if the function is called many times
                    for the same APC channel.
  \param[in]   ch         The selected APC Channel, see definitions of APC Channel.
  \param[in]   itf_type   one of interface type, ADC_PDM, DAC, or I2S.
  \param[in]   itf_idx    the instance index of the interface type.
  \param[in]   dma_chs[2] one or two dma channels for the dual_channel,
                          dma_chs[0] for left, dma_chs[1] for right, 0xFF indicates NOT USED!!
  \param[in]   cb_event   Pointer to \ref CSK_APC_SignalEvent_t
  \param[in]   usr_param  User-defined value, acts as last parameter of cb_event
  \returns
   - \b  0: function succeeded
   - \b -1: function failed
*/
int32_t apc_dual_channel_acquire (APC_DCH    dch,
                           APC_INTF_TYPE    itf_type,
                           APC_INTF_INDEX   itf_idx,
                           uint8_t          dma_chs[2], // FOR NEW DMAC
                           CSK_APC_SignalEvent_t    cb_event,
                           uint32_t         usr_param);

// generally called in XXX_Uninitialize()
int32_t apc_dual_channel_release (APC_DCH    dch);

// retrieve interface type, device instance of the type, and channel direction for APC channel
int32_t apc_dual_channel_owner (APC_DCH dch,
                            APC_INTF_TYPE *itf_type_p,
                            APC_INTF_INDEX *itf_idx_p,
                            uint8_t *ch_dir_p); // 0=OUT, 1=IN

// generally called in XXX_Control()
//int32_t apc_dual_channel_setup (APC_DCH     dch,
//                           APC_CHMODE       ch_mode, // 16, 24 MSB, 32 or 24 LSB?
//                           uint8_t          ch_sel, // select APC_LEFT_CHANNEL, APC_RIGHT_CHANNEL, or APC_STEREO_CHANNEL?
//                           uint8_t          ch_mix, // read L/R channel as a whole, or read L/R channel respectively
//                           uint8_t          trim_16bits); // only valid for 24 MSB and 32 channel mode, when set to 1:
//                                                         // record: trim low 16bits, get 16bits audio data
//                                                         // playback: 16bits audio data is placed at high 16bits of WORD
int32_t apc_dual_channel_setup (APC_DCH     dch,
                           APC_CHMODE       ch_mode, // 16, 24 MSB, 32 or 24 LSB?
                           uint8_t          ch_sel, // select APC_LEFT_CHANNEL, APC_RIGHT_CHANNEL, or APC_STEREO_CHANNEL?
                           uint8_t          ch_mix, // read L/R channel as a whole, or read L/R channel respectively
                           uint8_t          ch_flag); //bit[0]: only valid for 24 MSB and 32 channel mode, when set to 1:
                                                         // record: trim low 16bits, get 16bits audio data
                                                         // playback: 16bits audio data is placed at high 16bits of WORD
                                                      //bit[1]: use PIO instead of DMA operation

// echo setting for TX (only some TX dual_channels support echo currently)
int32_t apc_echo_setup (APC_DCH  dch,
                        uint32_t tx_samp_rate,
                        uint32_t echo_samp_rate,
                        uint8_t auto_feed,
                        uint8_t echo_bmp);

// apc_channel_xxxxx functions are used to access single L/R channel only if only one channel
// is enabled or mono (separate) mode is set for L/R channel.
// apc_dual_channels_xxxxx functions are used to access L/R channels as a whole if
// stereo (mixed) mode is set.
// NOTE: For following apc_channel_xxxxx and apc_dual_channels_xxxxx Read/Write API functions,
//      all sample count SHOULD be EVEN for 16-bit sample.
int32_t apc_channel_read (APC_CH    ch,
                          uint32_t  *sample_data,
                          uint32_t  sample_cnt,
                          uint32_t  buf_offset, // byte offset to each buffer (default 0, NO SG support)
                          uint32_t  dst_scat); // destination scatter setting (default 0, NO SG support)

int32_t apc_channel_write (APC_CH    ch,
                           const uint32_t* sample_data,
                           uint32_t  sample_cnt,
                           uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                           uint32_t src_gath); // source gather setting (default 0, NO SG support)

int32_t apc_dual_channel_read (APC_DCH  dch,
                              uint32_t  *sample_data,
                              uint32_t  sample_cnt,
                              uint32_t  buf_offset, // byte offset to each buffer (default 0, NO SG support)
                              uint32_t  dst_scat); // destination scatter setting (default 0, NO SG support)

int32_t apc_dual_channel_write (APC_DCH dch,
                               const uint32_t *sample_data,
                               uint32_t sample_cnt,
                               uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                               uint32_t src_gath); // source gather setting (default 0, NO SG support)


// read mixed data of APC channels 0 ~ 2 (3 IN channels)
// sample_cnt is the count of uint16 or uint32, and SHOULD be EVEN when 16-bit sample!
typedef enum {
    TCH_IN012 = 0,
         TCH_ADC01_ECHO_DAC = TCH_IN012, // 2ch ADC01 or DMIC01 + 1ch ECHO (DAC)
         TCH_I2S0_ECHO_DAC = TCH_IN012, // 2ch I2S0(LR) + 1ch ECHO (DAC)
    TCH_TYPE_COUNT
} TCH_TYPE;

int32_t apc_read_tri_channels (TCH_TYPE tch_type, uint32_t* sample_data, uint32_t sample_cnt, bool init_chk);


// read mixed data of APC channels 0 ~ 3 (4 IN channels)
// sample_cnt is the count of uint16 or uint32, and SHOULD be EVEN when 16-bit sample!
typedef enum {
    QCH_DCH_IN0_IN1 = 0,
         QCH_ADC01_ECHO_I2S0 = QCH_DCH_IN0_IN1, // 2ch ADC01 or DMIC01 + 2ch ECHO (I2S0 OUT)
         QCH_I2S0_ECHO_I2S0 = QCH_DCH_IN0_IN1, // 2ch I2S0(LR) + 2ch ECHO (I2S0 OUT)
    QCH_DCH_IN1_IN0,
        QCH_I2S1_ECHO_I2S1 = QCH_DCH_IN1_IN0, // 2ch I2S1(LR) + 2ch ECHO (I2S1 OUT)
    QCH_TYPE_COUNT
} QCH_TYPE;

int32_t apc_read_quad_channels (QCH_TYPE qch_type, uint32_t* sample_data, uint32_t sample_cnt, bool init_chk);


// Audio buffer used in data transfer (read/write) based on DMA LLP
/*
#if USE_GPDMA // GPDMA
typedef struct {
    uint32_t* sample_data;
    uint32_t  sample_cnt; // SHOULD be EVEN when 16-bit sample!!
    //DMA_LLI  dma_lli; // DON'T TOUCH IT! reserved for DMA driver only
} AUDIO_BUFFER_LLI;
#else // DW DMA
// NOTE: AUDIO_BUFFER_LLI (array) SHOULD NOT in the stack, and SHOULD NOT
//      be released until data transfer is completed or aborted!
// See Also: AUDIO_BUFFER_USER (defined in Driver_Common.h, for driver's caller)
typedef struct {
    uint32_t* sample_data;
    uint32_t  sample_cnt; // SHOULD be EVEN when 16-bit sample!!
    DMA_LLI  dma_lli; // DON'T TOUCH IT! reserved for DMA driver only
} AUDIO_BUFFER_LLI;
#endif // !USE_GPDMA
*/

//NOTE: ONLY infinite PingPong block transfer is supported with GPDMAC on ARCS C0 (and later), and
// PingPong block counting is NOT supported with GPDMAC IP now. It's quite difficult to support
// multi-buffer transfer based on PingPong block transfer, so _LLP API functions will not be implemented from now on...

#if USE_GPDMA

// pipo_lr_bmp:
// bit[0] = 1, indicates APC_DCH_BMP_LEFT is used (for ADC0, I2Sx Left channel etc.)
// bit[1] = 1, indicates APC_DCH_BMP_RIGHT is used (for ADC1, I2Sx Right channel etc.)

// bit[6] = 1, indicates the parameters of write_pipo are fully checked, usually used for first calling
//
//#define PIPO_FLAG_QUICK_CHECK   (0x1 << 6)

int32_t
apc_dch_read_pipo (APC_DCH dch, uint8_t lr_bmp,
                    PIPO_IN_BLOCK *aud_blks, uint8_t *blk_cnt_p,
                    uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
//                    uint32_t src_gath); // source gather setting (default 0, NO SG support)
                    uint32_t dst_scat); // destination scatter setting (default 0, NO SG support)

int32_t
apc_dch_write_pipo (APC_DCH dch, uint8_t lr_bmp,
                    PIPO_OUT_BLOCK *aud_blks, uint8_t *blk_cnt_p,
                    uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                    uint32_t src_gath); // source gather setting (default 0, NO SG support)

// return count of transferred block if >= 0, else return the error value.
int32_t apc_dch_get_pipo_blks(APC_DCH dch, uint8_t lr_bmp,
                                  PIPO_IO_BLOCK *aud_blks, uint8_t blk_cnt);

// read mixed data of APC channels 0 ~ 2 (3 IN channels) in PingPong mode
int32_t apc_read_tri_channels_pipo (TCH_TYPE tch_type, PIPO_IN_BLOCK *aud_blks, uint8_t *blk_cnt_p, bool init_chk);

// read mixed data of APC channels 0 ~ 3 (4 IN channels) in PingPong mode
int32_t apc_read_quad_channels_pipo (QCH_TYPE qch_type, PIPO_IN_BLOCK *aud_blks, uint8_t *blk_cnt_p, bool init_chk);

#else // !USE_GPDMA

// NOTE: AUDIO_BUFFER_LLI (array) SHOULD NOT in the stack, and SHOULD NOT
//      be released until data transfer is completed or aborted!
// See Also: AUDIO_BUFFER_USER (defined in Driver_Common.h, for driver's caller)
typedef struct {
    uint32_t* sample_data;
    uint32_t  sample_cnt; // SHOULD be EVEN when 16-bit sample!!
    DMA_LLI  dma_lli; // DON'T TOUCH IT! reserved for DMA driver only
} AUDIO_BUFFER_LLI;

// read channel data based on DMA Linked List Pointer (multiple buffers)
int32_t apc_channel_read_LLP (APC_CH ch,
                            AUDIO_BUFFER_LLI * bufs,
                            uint32_t buf_cnt,
                            uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                            uint32_t dst_scat); // destination scatter setting (default 0, NO SG support)

// write channel data based on DMA Linked List Pointer (multiple buffers)
int32_t apc_channel_write_LLP (APC_CH ch,
                              AUDIO_BUFFER_LLI * bufs,
                              uint32_t buf_cnt,
                              uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                              uint32_t src_gath); // source gather setting (default 0, NO SG support)

// read channel data based on DMA Linked List Pointer (multiple buffers)
int32_t apc_dual_channel_read_LLP (APC_DCH dch,
                                AUDIO_BUFFER_LLI * bufs,
                                uint32_t buf_cnt,
                                uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                                uint32_t dst_scat); // destination scatter setting (default 0, NO SG support)

// write channel data based on DMA Linked List Pointer (multiple buffers)
int32_t apc_dual_channel_write_LLP (APC_DCH dch,
                                  AUDIO_BUFFER_LLI * bufs,
                                  uint32_t buf_cnt,
                                  uint32_t buf_offset, // byte offset to each buffer (default 0, NO SG support)
                                  uint32_t src_gath); // source gather setting (default 0, NO SG support)

int32_t apc_read_quad_channels_LLP (QCH_TYPE qch_type, AUDIO_BUFFER_LLI * bufs, uint32_t buf_cnt, bool init_chk);

#endif // !USE_GPDMA



/**
  \fn          uint32_t apc_channel_get_count (APC_CH ch)
  \brief       Get number of transferred data items
  \param[in]   ch Channel number
  \returns     Number of transferred data items
*/
uint32_t apc_channel_get_count (APC_CH ch);
uint32_t apc_dual_channel_get_count (APC_DCH dch);

int32_t apc_channel_enable (APC_CH ch); // enable APC channel only
int32_t apc_channel_disable (APC_CH ch); // disable APC channel only
int32_t apc_channel_abort (APC_CH ch); // abort APC channel data transfer only
// en = 1, set NOT_SYNC_CACHE; en = 0, clear NOT_SYNC_CACHE
void apc_channel_set_nsynca (APC_CH ch, uint8_t en);

int32_t apc_dual_channel_enable (APC_DCH dch); // enable APC dual_channel only
int32_t apc_dual_channel_disable (APC_DCH dch); // disable APC dual_channel only
int32_t apc_dual_channel_abort (APC_DCH dch); // abort APC dual_channel data transfer only
// en = 1, set NOT_SYNC_CACHE; en = 0, clear NOT_SYNC_CACHE
void apc_dual_channel_set_nsynca (APC_DCH dch, uint8_t en);

int32_t apc_eq_set_coef_array(APC_DCH dch, uint32_t *eqcoefs, uint32_t num);
int32_t apc_eq_set_coef(APC_DCH dch, uint32_t index, uint32_t eqcoef);
int32_t apc_eq_enable(APC_DCH dch, uint32_t stages);
int32_t apc_eq_disble(APC_DCH dch);
int32_t apc_eq_clear(APC_DCH dch, uint8_t wait_done);

//------------------------------------------------------------
// adc/dac clock related API
//------------------------------------------------------------
#if (ARCS_VER > ARCS_B0_SOC)
#define AP_CFG     ((AP_CFG_RegDef*) AP_CFG_BASE)
#endif

#define CSK_APC_RegDef      APC_RegDef
#define CSK_APC             ((APC_RegDef *) AP_APC_BASE)
#define APC_BASE            AP_APC_BASE // alias for convenience

/*
#define SYSCTRL_CFG     ((CMN_SYSCTRL_RegDef*) CMN_SYSCTRL_BASE)
#define AUDPLL_CFG      ((AUDPLL_CTRL_RegDef*) CMN_AUDPLL_CTRL_BASE)

typedef enum {
    AUDIO_CLK_SRC_XTAL = 0, // from XTAL 24MHz [default]
    AUDIO_CLK_SRC_AUDPLL = 1 // from AUDPLL output
} AUDIO_CLK_SRC;

static inline void adc_clk_enable() {
    SYSCTRL_CFG->REG_MISC_CLK_CFG2.bit.ENA_CODEC_CLK_ADC = 1;
}

static inline void adc_clk_disable() {
    SYSCTRL_CFG->REG_MISC_CLK_CFG2.bit.ENA_CODEC_CLK_ADC = 0;
}

static inline void dac_clk_enable() {
    SYSCTRL_CFG->REG_MISC_CLK_CFG2.bit.ENA_CODEC_CLK_DAC = 1;
}

static inline void dac_clk_disable() {
    SYSCTRL_CFG->REG_MISC_CLK_CFG2.bit.ENA_CODEC_CLK_DAC = 0;
}

static inline void adc_clk_select(AUDIO_CLK_SRC src) {
    SYSCTRL_CFG->REG_MISC_CLK_CFG2.bit.SEL_CODEC_CLK_ADC = src & 0x1;
}

static inline void dac_clk_select(AUDIO_CLK_SRC src) {
    SYSCTRL_CFG->REG_MISC_CLK_CFG2.bit.SEL_CODEC_CLK_DAC = src & 0x1;
}

static inline AUDIO_CLK_SRC adc_clk_src() {
    return (AUDIO_CLK_SRC)SYSCTRL_CFG->REG_MISC_CLK_CFG2.bit.SEL_CODEC_CLK_ADC;
}

static inline AUDIO_CLK_SRC dac_clk_src() {
    return (AUDIO_CLK_SRC)SYSCTRL_CFG->REG_MISC_CLK_CFG2.bit.SEL_CODEC_CLK_DAC;
}


#define DEF_AUDPLL_VCO_FREQ         1190700000  //Hz, 1.1907GHz
#define DEF_AUDPLL_AUDIO_FREQ       22050000    //Hz, 22.05MHz

//initialize AUDPLL if necessary, and set audio post div
void init_audpll_audio_clk(uint32_t req_freq);
*/

static inline void codec_clk_enable() {
    //Enable Audio Codec Clock
#if (ARCS_VER == ARCS_B0_SOC)
    IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.ENA_CODEC_CLK = 0x1;
#elif (ARCS_VER == ARCS_C0_SOC)
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_CODEC_CLK = 0x1;
#endif
}

static inline void codec_clk_disable() {
    //Disable Audio Codec Clock
#if (ARCS_VER == ARCS_B0_SOC)
    IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.ENA_CODEC_CLK = 0x0;
#elif (ARCS_VER == ARCS_C0_SOC)
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_CODEC_CLK = 0x0;
#endif
}


#if (ARCS_VER == ARCS_D0_SOC)

static inline void codec_adc_clk_enable() {
    //Enable Audio Codec ADC Clock
    __HAL_CRM_ADC_CLK_ENABLE();
}

static inline void codec_dac_clk_enable() {
    //Enable Audio Codec DAC Clock
    __HAL_CRM_DAC_CLK_ENABLE();
}

static inline void codec_adc_clk_disable() {
    //Disable Audio Codec Clock
    __HAL_CRM_ADC_CLK_DISABLE();
}

static inline void codec_dac_clk_disable() {
    //Disable Audio Codec Clock
    __HAL_CRM_DAC_CLK_DISABLE();
}

#endif //ARCS_D0_SOC

// reset APC TX and/or RX path (NOT reset registers, only internal circuit logic)
// bit[0] = 1, reset RX path; bit[1] = 1, reset TX path
void apc_reset_path(uint8_t rst_flag);

// get current sample count in the specified APC L/R FIFO or both (if mixed)
uint32_t apc_get_fifo_samp_cnt(APC_DCH dch, uint8_t chbmp);

#endif /* __APC_ARCS_H */
