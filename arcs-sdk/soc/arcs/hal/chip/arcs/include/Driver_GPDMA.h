/*
 * Driver_GPDMA.h
 *
 *  Created on: Sep 22, 2023
 *      Author: USER
 */

#ifndef ListenAI_INCLUDE_DRIVER_GPDMA_H_
#define ListenAI_INCLUDE_DRIVER_GPDMA_H_

#include "Driver_Common.h"

#define GPDMAC_ARCS_D0     1 // 0

// None image mode, burst must be consistency

#define CSK_GPDMA_MAX_CHANNEL_NUM              6

#define GPDMA_SCATTER_GATHER_COUNTER_MIN       0x1
#define GPDMA_SCATTER_GATHER_COUNTER_MAX       0x40

#define GPDMA_SCATTER_GATHER_INTERVAL_MIN      0x0
#define GPDMA_SCATTER_GATHER_INTERVAL_MAX      0x3FF

typedef enum _csk_gpdma_status {
    gpdma_status_none = 0UL,
    gpdma_status_init = 1UL,
    gpdma_status_config,
    gpdma_status_busy,
    gpdma_status_stopped,
} csk_gpdma_status_t;

typedef enum _csk_gpdma_ch {
    gp_dma_ch0 = 0UL,
    gp_dma_ch1,
    gp_dma_ch2,
    gp_dma_ch3,
    gp_dma_ch4,
    gp_dma_ch5,
} csk_gpdma_ch_t;

typedef enum _csk_gpdma_burst_len {
    gpdma_burst_len_1spl = 0UL,
    gpdma_burst_len_2spl,
    gpdma_burst_len_4spl,
    gpdma_burst_len_8spl,
} csk_gpdma_burst_len_t;

typedef enum _csk_gpdma_sample_unit {
    gpdma_sample_unit_word = 2UL,
    gpdma_sample_unit_byte = 0UL,
    gpdma_sample_unit_halfword = 1UL,
} csk_gpdma_sample_unit_t;

typedef enum _csk_tfr_mode {
    tfr_mode_p2m = 0UL,
    tfr_mode_m2p,
    tfr_mode_m2m,
} csk_tfr_mode_t;

typedef enum _csk_inc_mode {
    inc_mode_increase = 0UL,
    inc_mode_fix,
} csk_inc_mode_t;

typedef enum _csk_address_mode {
    address_mode_normal = 0UL,
    address_mode_pipo,
} csk_address_mode_t;

typedef enum _csk_prio_mode {
    prio_mode_vhigh = 0UL,
    prio_mode_high,
    prio_mode_normal,
    prio_mode_low,
} csk_prio_mode_t;

typedef enum _csk_handshake_num {
    hs_none = 0xff,

    // dma select 0
    qspi_hs_num0 = 0x0,
    d2out_hs_num1,
    d2mask_hs_num2,
    d2back_hs_num3,
    d2fore_hs_num4,
    dvp_hs_num5,
    jpg_e_hs_num6,
    jpg_p_hs_num7,
    apc_tx0_hs_num8,
    apc_tx1_hs_num9,
    apc_tx2_hs_num10,
    apc_rx0_hs_num11,
    apc_rx1_hs_num12,
    apc_rx2_hs_num13,
    apc_rx3_hs_num14,
    apc_tx3_hs_num15,

    // dma select 1
    aes_ingress_hs_num6 = 0x16,
    aes_engress_hs_num7 = 0x17,

    // dma select 2
    aes_ingress_hs_num14 = 0x2E,
    aes_engress_hs_num15 = 0x2F,

    hs_max = 0x30,
} csk_handshake_num_t;

#define CSK_GPDMA_EVENT_TRANSFER_DONE                         (1UL << 0)
#define CSK_GPDMA_EVENT_PIPO0_DONE                            (1UL << 1)
#define CSK_GPDMA_EVENT_PIPO1_DONE                            (1UL << 2)

typedef void
(*CSK_GPDMA_SignalEvent_t)(uint32_t event, void* workspace);

#define CSK_GPDMA_STATUS_ERROR         (CSK_DRIVER_ERROR_SPECIFIC - 1)
#define CSK_GPDMA_MODE_ERROR           (CSK_DRIVER_ERROR_SPECIFIC - 2)
#define CSK_GPDMA_UNKNOWN_ERROR        (CSK_DRIVER_ERROR_SPECIFIC - 3)

typedef struct _csk_gpdma_init {
    csk_gpdma_ch_t dma_ch;

    csk_gpdma_burst_len_t burst_len;
    csk_gpdma_sample_unit_t sample_unit;

    csk_address_mode_t src_mode;
    csk_address_mode_t dst_mode;

    csk_tfr_mode_t tfr_mode;
    csk_inc_mode_t src_inc_mode;
    csk_inc_mode_t dst_inc_mode;

    // sample unit
    uint8_t gather_en;
    uint8_t gather_counter;
    uint16_t gather_interval;
    uint8_t scatter_en;
    uint8_t scatter_counter;
    uint16_t scatter_interval;

    csk_prio_mode_t prio_lvl;

    csk_handshake_num_t handshake;
} csk_gpdma_init_t;

typedef struct _csk_gpdma_scatt_gath {
    // scatter gather
    uint8_t gather_en;
    uint8_t gather_counter;
    uint16_t gather_interval;
    uint8_t scatter_en;
    uint8_t scatter_counter;
    uint16_t scatter_interval;
    // address mode (ignore if 0xff)
    csk_address_mode_t src_mode;
    csk_address_mode_t dst_mode;
} csk_gpdma_scatt_gath_t;

typedef struct _csk_gpdma_info {
    csk_gpdma_status_t status;
    uint32_t cur_src_addr;
    uint32_t cur_dst_addr;
} csk_gpdma_info_t;

int32_t
GPDMA_Initialize(void);

int32_t
GPDMA_Uninitialize(void);

int32_t
GPDMA_Config(csk_gpdma_init_t* res, CSK_GPDMA_SignalEvent_t cb_event, void* workspace);

int32_t
GPDMA_Config_Scatt_Gath(csk_gpdma_ch_t ch, csk_gpdma_scatt_gath_t* sg); // Configure Scatter-Gather ONLY!

int32_t // src_mode / dst_mode = 0xff, don't change its original value...
GPDMA_Config_Addr_Mode(csk_gpdma_ch_t ch, csk_address_mode_t src_mode, csk_address_mode_t dst_mode); // Configure address_mode ONLY!

int32_t // src_mode_p / dst_mode_p = NULL, don't care its address mode...
GPDMA_Get_Addr_Mode(csk_gpdma_ch_t ch, csk_address_mode_t *src_mode_p, csk_address_mode_t *dst_mode_p);

int32_t
GPDMA_Start_Normal(csk_gpdma_ch_t ch, void* src, void* dst, uint32_t sample_len);


#if GPDMAC_ARCS_D0
int32_t
GPDMA_Start_PiPoEx(csk_gpdma_ch_t ch, void* src0, void* src1, void* dst0, void* dst1, uint32_t sample_len, uint32_t sample_len1);

int32_t // flags = 1 indicates Stop PingPing after this block transfer
GPDMA_PiPo_ReloadEx(csk_gpdma_ch_t ch, void* src, void* dst, uint32_t sample_len, uint32_t flags);

// Obsolete API, compatible with ARCS C0. GPDMA_Start_PiPoEx is recommended on ARCS D0.
#define GPDMA_Start_PiPo(ch, src0, src1, dst0, dst1, sample_len)   \
    GPDMA_Start_PiPoEx(ch, src0, src1, dst0, dst1, sample_len, sample_len)

// Obsolete API, compatible with ARCS C0. GPDMA_PiPo_ReloadEx is recommended on ARCS D0.
#define GPDMA_PiPo_Reload(...)  \
    GPDMA_PiPo_ReloadEx(__VA_ARGS__, 0, 0)

#else
int32_t
GPDMA_Start_PiPo(csk_gpdma_ch_t ch, void* src0, void* src1, void* dst0, void* dst1, uint32_t sample_len);

int32_t
GPDMA_PiPo_Reload(csk_gpdma_ch_t ch, void* src, void* dst);

#endif // GPDMAC_ARCS_D0

int32_t
GPDMA_Stop(csk_gpdma_ch_t ch);

int32_t
GPDMA_Resume(csk_gpdma_ch_t ch);

uint32_t
GPDMA_GetCnt(csk_gpdma_ch_t ch, uint32_t* sample_len);

// pipo_sel: 0 indicates ping block, 1 indicates pong block, -1 indicate last done block (ping or pong)
// src/dst : return source/destination address of the ping/pong block if NOT NULL
uint32_t
GPDMA_GetCnt_PiPoBlk(csk_gpdma_ch_t ch, int8_t pipo_sel, uint32_t *src, uint32_t *dst);

int32_t
GPDMA_GetStatus(csk_gpdma_ch_t ch, csk_gpdma_info_t* info);

#endif /* ListenAI_INCLUDE_DRIVER_GPDMA_H_ */
