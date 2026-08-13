#include "PSRAMUnified.h"

#include "ClockManager.h"
#include "cache.h"

#include "PSRAM_Unified_Common.h"
#include "./die/winbond/winbond_config.h"
#include "./die/xccela/xccela_config.h"

static __psram_unified_die_type_t __psram_die_type =
    __psram_unified_die_unknown;
static uint32_t __psram_die_density = 0;

static int32_t psram_unified_validate_init(__psram_unified_init_t *init_params)
{
    if (init_params == NULL) {
        PSRAM_LOGE("PSRAM Unified init failed: NULL init_params");
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((init_params->search == 0) &&
        ((init_params->read_delay == NULL) ||
         (init_params->write_delay == NULL))) {
        PSRAM_LOGE("PSRAM Unified init failed: NULL delay pointer");
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((init_params->die_type != __psram_unified_die_any) &&
        (init_params->die_type != __psram_unified_die_xccela) &&
        (init_params->die_type != __psram_unified_die_winbond)) {
        PSRAM_LOGE("PSRAM Unified unsupported die type: %d",
                   init_params->die_type);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    return CSK_DRIVER_OK;
}

static void psram_unified_reset_controller(void)
{
    IP_SYSCTRL->REG_SW_RESET_CP2.bit.PSRAM_CTRL_RESET = 0x1;
}

static void psram_unified_configure_io(void)
{
#if PSRAM_UNIFIED_CONTROLLER_DRV_STR
    IP_CMN_SYS->REG_PSRAMIO_CFG0.bit.PSRAMIO_DQ0_DRV_CFG =
        PSRAM_UNIFIED_CFG_IO_DRV_DQ0;
    IP_CMN_SYS->REG_PSRAMIO_CFG0.bit.PSRAMIO_DQ1_DRV_CFG =
        PSRAM_UNIFIED_CFG_IO_DRV_DQ1;
    IP_CMN_SYS->REG_PSRAMIO_CFG0.bit.PSRAMIO_DQ2_DRV_CFG =
        PSRAM_UNIFIED_CFG_IO_DRV_DQ2;
    IP_CMN_SYS->REG_PSRAMIO_CFG0.bit.PSRAMIO_DQ3_DRV_CFG =
        PSRAM_UNIFIED_CFG_IO_DRV_DQ3;
    IP_CMN_SYS->REG_PSRAMIO_CFG0.bit.PSRAMIO_DQ4_DRV_CFG =
        PSRAM_UNIFIED_CFG_IO_DRV_DQ4;
    IP_CMN_SYS->REG_PSRAMIO_CFG0.bit.PSRAMIO_DQ5_DRV_CFG =
        PSRAM_UNIFIED_CFG_IO_DRV_DQ5;
    IP_CMN_SYS->REG_PSRAMIO_CFG0.bit.PSRAMIO_DQ6_DRV_CFG =
        PSRAM_UNIFIED_CFG_IO_DRV_DQ6;
    IP_CMN_SYS->REG_PSRAMIO_CFG0.bit.PSRAMIO_DQ7_DRV_CFG =
        PSRAM_UNIFIED_CFG_IO_DRV_DQ7;
    IP_CMN_SYS->REG_PSRAMIO_CFG1.bit.PSRAMIO_DQSDM_DRV_CFG =
        PSRAM_UNIFIED_CFG_IO_DRV_DQS;
    IP_CMN_SYS->REG_PSRAMIO_CFG1.bit.PSRAMIO_CEN_DRV_CFG =
        PSRAM_UNIFIED_CFG_IO_DRV_CEN;
    IP_CMN_SYS->REG_PSRAMIO_CFG1.bit.PSRAMIO_CLK_DRV_CFG =
        PSRAM_UNIFIED_CFG_IO_DRV_CLK;
#endif

#if PSRAM_UNIFIED_RX_DIFF_EN
    PSRAM_LOG("PSRAM DIFF MODE IO");
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_DQS_RX_DIFF_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_DM_RX_DIFF_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_CEN_RX_DIFF_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ0_RX_DIFF_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ1_RX_DIFF_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ2_RX_DIFF_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ3_RX_DIFF_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ4_RX_DIFF_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ5_RX_DIFF_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ6_RX_DIFF_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ7_RX_DIFF_EN = 1;

    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_DQS_RX_COMP_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_DM_RX_COMP_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_CEN_RX_COMP_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ0_RX_COMP_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ1_RX_COMP_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ2_RX_COMP_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ3_RX_COMP_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ4_RX_COMP_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ5_RX_COMP_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ6_RX_COMP_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ7_RX_COMP_EN = 0;
#else
    PSRAM_LOG("PSRAM COMP MODE IO");
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_DQS_RX_DIFF_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_DM_RX_DIFF_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_CEN_RX_DIFF_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ0_RX_DIFF_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ1_RX_DIFF_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ2_RX_DIFF_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ3_RX_DIFF_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ4_RX_DIFF_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ5_RX_DIFF_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ6_RX_DIFF_EN = 0;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ7_RX_DIFF_EN = 0;

    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_DQS_RX_COMP_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_DM_RX_COMP_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG3.bit.PSRAMIO_CEN_RX_COMP_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ0_RX_COMP_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ1_RX_COMP_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ2_RX_COMP_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ3_RX_COMP_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ4_RX_COMP_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ5_RX_COMP_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ6_RX_COMP_EN = 1;
    IP_CMN_SYS->REG_PSRAMIO_CFG4.bit.PSRAMIO_DQ7_RX_COMP_EN = 1;
#endif
}

static void psram_unified_configure_prefetch(__psram_unified_init_t *init_params)
{
    IP_PSRAM_CTRL->REG_RXBUFMSTID.bit.RXBUF0_MSTRID =
        init_params->fifo0_master;
    IP_PSRAM_CTRL->REG_RXBUFMSTID.bit.RXBUF1_MSTRID =
        init_params->fifo1_master;

    IP_PSRAM_CTRL->REG_RXBUFPFEN.bit.RXBUF0_PREFETCH_EN =
        init_params->fifo0_enable;
    IP_PSRAM_CTRL->REG_RXBUFPFEN.bit.RXBUF1_PREFETCH_EN =
        init_params->fifo1_enable;
}

static int32_t psram_unified_reduce_clock(uint32_t *saved_div)
{
    uint32_t psram_clock = CRM_GetPsramFreq();
    uint32_t div_m = 0;

    HAL_CRM_GetPsramClkConfig(&div_m);
    *saved_div = div_m;

    if (div_m == 0) {
        div_m = 1;
    }

    while ((psram_clock > 24000000UL) && (div_m < 31)) {
        div_m *= 2;
        if (div_m > 31) {
            div_m = 31;
        }

        if (HAL_CRM_SetPsramClkDiv(div_m) != 0) {
            return CSK_DRIVER_ERROR;
        }

        psram_clock = CRM_GetPsramFreq();
    }

    PSRAM_LOG("PSRAM reduce to %d", psram_clock);
    return CSK_DRIVER_OK;
}

static int32_t psram_unified_restore_clock(uint32_t saved_div)
{
    if (HAL_CRM_SetPsramClkDiv(saved_div) != 0) {
        return CSK_DRIVER_ERROR;
    }

    PSRAM_LOG("PSRAM recover to %d", CRM_GetPsramFreq());
    return CSK_DRIVER_OK;
}

static int32_t psram_unified_xccela_detect(uint32_t *density)
{
    __psram_xccela_controller_die_type();
    __psram_xccela_mr_seq_configure();

    if (__psram_xccela_info_extra(density) != CSK_DRIVER_OK) {
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    PSRAM_LOG("PSRAM Unified Die Detect: Xccela");
    return CSK_DRIVER_OK;
}

int32_t PSRAMUnified_Initialize(__psram_unified_init_t *init_params)
{
    int32_t ret = psram_unified_validate_init(init_params);
    uint32_t psram_clock = 0;
    uint32_t saved_div = 0;
    uint32_t psram_src_data[PSRAM_SEARCH_DQS_NUM];

    if (ret != CSK_DRIVER_OK) {
        return ret;
    }

    __psram_die_type = __psram_unified_die_unknown;
    __psram_die_density = 0;

    psram_unified_reset_controller();

    __HAL_CRM_PSRAM_CLK_ENABLE();
    psram_clock = CRM_GetPsramFreq();
    PSRAM_LOG("PSRAM Unified Clock Freq: %d", psram_clock);

    if (psram_clock > PSRAM_UNIFIED_XCCELA_MAX_FREQ) {
        PSRAM_LOGE("PSRAM Unified init failed: clock too high %d",
                   psram_clock);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    psram_unified_configure_io();
    psram_unified_configure_prefetch(init_params);

    if (init_params->die_type == __psram_unified_die_winbond) {
        __psram_winbond_controller_die_type();
        __psram_winbond_mr_seq_configure();
        __psram_winbond_ahb_seq_configure();
        __psram_winbond_controller_timing_configure(psram_clock);
    } else if (init_params->die_type == __psram_unified_die_xccela) {
        __psram_xccela_controller_die_type();
        __psram_xccela_mr_seq_configure();
        __psram_xccela_controller_timing_configure(psram_clock);
    } else {
        PSRAM_LOGE("PSRAM Unified init failed: unsupported die type %d",
                   init_params->die_type);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    ret = psram_unified_reduce_clock(&saved_div);
    if (ret != CSK_DRIVER_OK) {
        return ret;
    }

    ret = __psram_unified_dll_lock();
    if (ret != CSK_DRIVER_OK) {
        return ret;
    }

    if (init_params->die_type == __psram_unified_die_winbond) {
        ret = __psram_winbond_info_extra(&__psram_die_density);
        if (ret != CSK_DRIVER_OK) {
            return ret;
        }

        __psram_die_type = __psram_unified_die_winbond;
        __psram_winbond_controller_die_page_size(__psram_die_density);
        __psram_winbond_die_para_configure(psram_clock, __psram_die_density);
        __psram_winbond_mr_print();
    } else if (init_params->die_type == __psram_unified_die_xccela) {
        if (psram_unified_xccela_detect(&__psram_die_density) != CSK_DRIVER_OK) {
            PSRAM_LOGE("PSRAM Unified init failed: Xccela detect error");
            return CSK_DRIVER_ERROR_UNSUPPORTED;
        }
        __psram_die_type = __psram_unified_die_xccela;

        __psram_xccela_ahb_seq_configure(psram_clock, __psram_die_density);
        IP_PSRAM_CTRL->REG_RDWRCTRL.bit.RD_LOOKUP_TX_BUF = 0;
        __psram_xccela_controller_die_page_size(__psram_die_density);
        __psram_xccela_die_para_configure(psram_clock, __psram_die_density);
        __psram_xccela_mr_print();
    } else {
        PSRAM_LOGE("PSRAM Unified init failed: unsupported die type %d",
                   init_params->die_type);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    if (init_params->search) {
        for (uint32_t i = 0; i < PSRAM_SEARCH_DQS_NUM; i++) {
            psram_src_data[i] = 0x5aa55aa5;
        }

        psram_memcpy((uint32_t *)PSRAM_UNIFIED_BASE_ADDRESS,
                     psram_src_data,
                     PSRAM_SEARCH_DQS_NUM * sizeof(uint32_t));
        HAL_FlushInvalidateDCache_by_Addr(
            (uint32_t *)PSRAM_UNIFIED_BASE_ADDRESS,
            PSRAM_SEARCH_DQS_NUM * sizeof(uint32_t));

        for (uint32_t j = 0; j < 300; j++) {
            __NOP();
        }
    }

    ret = psram_unified_restore_clock(saved_div);
    if (ret != CSK_DRIVER_OK) {
        return ret;
    }

    ret = __psram_unified_dll_lock();
    if (ret != CSK_DRIVER_OK) {
        return ret;
    }

    if (init_params->search) {
        ret |= __psram_unified_search_read_dqs_delay(psram_src_data,
                                                     init_params->read_delay);
        ret |= __psram_unified_search_write_dqs_delay(init_params->write_delay);
    } else {
        IP_PSRAM_CTRL->REG_DLLDELAY.bit.RDLVL_DELAY =
            *init_params->read_delay;
        IP_PSRAM_CTRL->REG_DLLDELAY.bit.WRLVL_DELAY =
            *init_params->write_delay;
        IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 0x1;
    }

    return ret;
}

int32_t PSRAMUnified_GetInformation(uint32_t *density,
                                    __psram_unified_die_type_t *die_type)
{
    if ((density == NULL) || (die_type == NULL)) {
        PSRAM_LOGE("PSRAM Unified GetInformation failed: NULL output");
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    *density = __psram_die_density;
    *die_type = __psram_die_type;

    return CSK_DRIVER_OK;
}

int32_t PSRAMUnified_EnterSleepMode(__psram_unified_sleep_mode_t sleep_mode)
{
    if (__psram_die_type != __psram_unified_die_xccela) {
        PSRAM_LOGE("PSRAM Unified sleep failed: unsupported die type %d",
                   __psram_die_type);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    return __psram_xccela_enter_sleep_mode(sleep_mode);
}

int32_t PSRAMUnified_ExitSleepMode(__psram_unified_init_t *init_params,
                                   uint32_t density)
{
    int32_t ret = psram_unified_validate_init(init_params);
    uint32_t psram_clock = 0;

    if (ret != CSK_DRIVER_OK) {
        return ret;
    }

    if (density == 0) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    if ((init_params->die_type == __psram_unified_die_winbond) ||
        (__psram_die_type == __psram_unified_die_winbond)) {
        PSRAM_LOGE("PSRAM Unified exit sleep failed: Winbond unsupported");
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    psram_unified_reset_controller();

    __HAL_CRM_PSRAM_CLK_ENABLE();
    psram_clock = CRM_GetPsramFreq();
    PSRAM_LOG("PSRAM Unified ExitSleep Clock Freq: %d", psram_clock);

    if (psram_clock > PSRAM_UNIFIED_XCCELA_MAX_FREQ) {
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    psram_unified_configure_io();
    psram_unified_configure_prefetch(init_params);

    __psram_xccela_controller_die_type();
    __psram_xccela_mr_seq_configure();
    __psram_xccela_controller_timing_configure(psram_clock);

    ret = __psram_unified_dll_lock();
    if (ret != CSK_DRIVER_OK) {
        return ret;
    }

    __psram_xccela_exit_sleep_mode();

    __psram_die_type = __psram_unified_die_xccela;
    __psram_die_density = density;

    __psram_xccela_ahb_seq_configure(psram_clock, __psram_die_density);
    IP_PSRAM_CTRL->REG_RDWRCTRL.bit.RD_LOOKUP_TX_BUF = 0;
    __psram_xccela_controller_die_page_size(__psram_die_density);
    __psram_xccela_die_para_configure(psram_clock, __psram_die_density);
    __psram_xccela_refresh_rate_normal_set();
    __psram_xccela_mr_print();

    if ((init_params->read_delay == NULL) ||
        (init_params->write_delay == NULL)) {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    IP_PSRAM_CTRL->REG_DLLDELAY.bit.RDLVL_DELAY = *init_params->read_delay;
    IP_PSRAM_CTRL->REG_DLLDELAY.bit.WRLVL_DELAY = *init_params->write_delay;
    IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 0x1;

    return CSK_DRIVER_OK;
}
