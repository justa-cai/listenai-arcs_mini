#include "PSRAMUnified.h"
#include "ClockManager.h"
#include "log_print.h"
#include "cache.h"

#include "PSRAM_Unified_Common.h"

#include "./die/xccela/xccela_config.h"
#include "./die/apm/apm_config.h"

/* --------------------------------------------------------------------------
 * PSRAM Unified Driver Implementation
 * -------------------------------------------------------------------------- */
static __psram_unified_die_type_t __psram_die_type = __psram_unified_die_unknown;
static uint32_t __psram_die_density = 0;

static __psram_unified_die_type_t __psram_die_auto_detect(void){
    __psram_xccela_controller_die_type();
    __psram_xccela_mr_seq_configure();

    if (__psram_xccela_info_extra(&__psram_die_density) == 0){
        PSRAM_LOG("PSRAM Unified Die Auto-detect: XCCELA");
        return __psram_unified_die_xccela;
    }

    __psram_apm_controller_die_type();
    __psram_apm_mr_seq_configure();
    if (__psram_apm_info_extra(&__psram_die_density) == 0){
        PSRAM_LOG("PSRAM Unified Die Auto-detect: APM");
        return __psram_unified_die_apm;
    }

    return __psram_unified_die_unknown;
}

int32_t PSRAMUnified_Initialize(__psram_unified_init_t* init_params){
    int32_t ret = CSK_DRIVER_OK;

    if (init_params == NULL){
        PSRAM_LOGE("PSRAM Unified Init failed! NULL pointer of init_params");
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.PSRAM_CTRL_PRESET = 1;
    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.PSRAM_CTRL_RESET = 1; // Reset PSRAM controller

    __psram_die_type = init_params->die_type;

    uint32_t __psram_unified_clock_freq = 0;
    {
        // Enable PSRAM controller clock
        __HAL_CRM_PSRAM_CLK_ENABLE();
        __psram_unified_clock_freq = CRM_GetPsramFreq();
        PSRAM_LOG("PSRAM Unified Clock Freq: %d", __psram_unified_clock_freq);
    }

    /////////// Driver Strength ///////////
#if PSRAM_UNIFIED_CONTROLLER_DRV_STR
	// Driver strength
    IP_CMN_SYS->REG_PSRAMIO_CFG0.all = (PSRAM_UNIFIED_CFG_IO_DRV_DQ0 | 
                                        (PSRAM_UNIFIED_CFG_IO_DRV_DQ1 << 3) |
                                        (PSRAM_UNIFIED_CFG_IO_DRV_DQ2 << 6) |
                                        (PSRAM_UNIFIED_CFG_IO_DRV_DQ3 << 9) |
                                        (PSRAM_UNIFIED_CFG_IO_DRV_DQ4 << 12) |
                                        (PSRAM_UNIFIED_CFG_IO_DRV_DQ5 << 15) |
                                        (PSRAM_UNIFIED_CFG_IO_DRV_DQ6 << 18) |
                                        (PSRAM_UNIFIED_CFG_IO_DRV_DQ7 << 21));
    IP_CMN_SYS->REG_PSRAMIO_CFG1.all = (PSRAM_UNIFIED_CFG_IO_DRV_DQS |
                                        (PSRAM_UNIFIED_CFG_IO_DRV_CEN << 6) |
                                        (PSRAM_UNIFIED_CFG_IO_DRV_CLK << 9));
#endif

    // MASTER configure
    {
        IP_PSRAM_CTRL->REG_RXBUFMSTID.bit.RXBUF0_MSTRID = init_params->fifo0_master;
        IP_PSRAM_CTRL->REG_RXBUFMSTID.bit.RXBUF1_MSTRID = init_params->fifo1_master;
        IP_PSRAM_CTRL->REG_RXBUFMSTID.bit.RXBUF2_MSTRID = init_params->fifo2_master;
    }

    // Prefetch
    {
        IP_PSRAM_CTRL->REG_RXBUFPFCTRL.bit.RXBUF0_PREFETCH_EN = init_params->fifo0_enable;
        IP_PSRAM_CTRL->REG_RXBUFPFCTRL.bit.RXBUF1_PREFETCH_EN = init_params->fifo1_enable;
        IP_PSRAM_CTRL->REG_RXBUFPFCTRL.bit.RXBUF2_PREFETCH_EN = init_params->fifo2_enable;
    }

    {
        IP_PSRAM_CTRL->REG_BUSPRIORCTRL.all = (PSRAM_UNIFIED_BUS_PRIORITY_MASTER0 |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER1 << 2) |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER2 << 4) |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER3 << 6) |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER4 << 8) |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER5 << 10) |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER6 << 12) |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER7 << 14) |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER8 << 16) |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER9 << 18));

        PSRAM_LOG("PSRAM unified priority parameter: 0x%x", IP_PSRAM_CTRL->REG_BUSPRIORCTRL.all);
    }

    clock_src_name_t __psram_unified_src;
    // Redeuce PSRAM clock to 24Mhz XTAL for configure MR registers
    {
		// Set low psram clock
		uint32_t div_m;
		HAL_CRM_GetPsramClkConfig(&__psram_unified_src, &div_m);
		if (__psram_unified_src == CRM_IpSrcSyspllPsram){
			// Change clock source to xtal 24m
			HAL_CRM_SetPsramClkSrc(CRM_IpSrcCORE24M);

			PSRAM_LOG("PSRAM REDUCE TO 24Mhz");
		} else {
			// Keep the clock source
			PSRAM_LOG("PSRAM clock is always 24Mhz");
		}
    }

    {
        ret = __psram_unified_dll_lock();
        if (ret < 0){
            return ret;
        }
    }

    if (__psram_die_type == __psram_unified_die_any){
        // Auto-detect die type
        __psram_unified_die_type_t __die_type = __psram_die_auto_detect();

        if (__die_type == __psram_unified_die_unknown){
            PSRAM_LOGE("PSRAM Unified Init failed! Unknown die type");
            return CSK_DRIVER_ERROR_UNSUPPORTED;
        } else {
            __psram_die_type = __die_type;
        }
    }

    ///// Check PSRAM clock /////
    if (__psram_die_type == __psram_unified_die_xccela){
        if (__psram_unified_clock_freq > PSRAM_UNIFIED_XCCELA_MAX_FREQ){
#if PSRAM_UNIFIED_FREQ_CORRECTION
            CRM_InitSyspllPsram(PSRAM_UNIFIED_XCCELA_MAX_FREQ_PARA);

            __psram_unified_clock_freq = PSRAM_UNIFIED_XCCELA_MAX_FREQ;
#else
            PSRAM_LOGE("PSRAM Unified Init failed! Exceed max clock freq for Xccela die");
            return CSK_DRIVER_ERROR_UNSUPPORTED;
#endif
        }
    } else if (__psram_die_type == __psram_unified_die_apm){
        if (__psram_unified_clock_freq > PSRAM_UNIFIED_APM_MAX_FREQ){
#if PSRAM_UNIFIED_FREQ_CORRECTION
            CRM_InitSyspllPsram(PSRAM_UNIFIED_APM_MAX_FREQ_PARA);

            __psram_unified_clock_freq = PSRAM_UNIFIED_APM_MAX_FREQ;
#else
            PSRAM_LOGE("PSRAM Unified Init failed! Exceed max clock freq for APM die");
            return CSK_DRIVER_ERROR_UNSUPPORTED;
#endif
        }
    }

    if (__psram_die_type == __psram_unified_die_xccela){
        __psram_xccela_controller_die_type();

        __psram_xccela_mr_seq_configure();

        if (__psram_xccela_info_extra(&__psram_die_density) != 0){
            PSRAM_LOGE("PSRAM Unified Init failed! Xccela die info extra error");
            return CSK_DRIVER_ERROR_UNSUPPORTED;
        }

        __psram_xccela_ahb_seq_configure(__psram_unified_clock_freq, __psram_die_density);

        __psram_xccela_controller_die_page_size(__psram_die_density);

        __psram_xccela_controller_timing_configure(__psram_unified_clock_freq);

        __psram_xccela_die_para_configure(__psram_unified_clock_freq, __psram_die_density);

        __psram_xccela_mr_print();
    } else if (__psram_die_type == __psram_unified_die_apm){
        __psram_apm_controller_die_type();

        __psram_apm_mr_seq_configure();

        if (__psram_apm_info_extra(&__psram_die_density) != 0){
            PSRAM_LOGE("PSRAM Unified Init failed! APM die info extra error");
            return CSK_DRIVER_ERROR_UNSUPPORTED;
        }

        __psram_apm_ahb_seq_configure(__psram_unified_clock_freq, __psram_die_density);

        __psram_apm_controller_die_page_size(__psram_die_density);

        __psram_apm_controller_timing_configure(__psram_unified_clock_freq);

        __psram_apm_die_para_configure(__psram_unified_clock_freq, __psram_die_density);

        __psram_apm_mr_print();
    } else {
        PSRAM_LOGE("PSRAM Unified Init failed! Unsupported die type %d", __psram_die_type);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    ///////// Write Golden data to PSRAM
    uint32_t psram_src_data[PSRAM_SEARCH_DQS_NUM];
    {
        if (init_params->search){ // Search read delay and write delay

            // generate training data
            // psram_data_init(psram_src_data); // NOTE: Replace with FIX value 0x5aa55aa5
            for (int i = 0; i < PSRAM_SEARCH_DQS_NUM; i++) {
                psram_src_data[i] = 0x5aa55aa5;
            }

            psram_memcpy((uint32_t *)PSRAM_UNIFIED_BASE_ADDRESS, psram_src_data,         PSRAM_SEARCH_DQS_NUM * sizeof(uint32_t));

            // Write back from cache
            HAL_FlushInvalidateDCache_by_Addr((uint32_t *)PSRAM_UNIFIED_BASE_ADDRESS, PSRAM_SEARCH_DQS_NUM * sizeof(uint32_t));

            //wait for psram write data done
            for (int j = 0; j < 3000; j++) // maybe can use fence.i
                __NOP();
        }
    }

    ///////// Restore PSRAM clock
    {
        HAL_CRM_SetPsramClkSrc(__psram_unified_src);

        PSRAM_LOG("PSRAM RECOVER TO %d", CRM_GetPsramFreq());

        {
            ret = __psram_unified_dll_lock();
            if (ret < 0){
                return ret;
            }
        }
    }

    // EYE DIAGRAM search start ######################################################
    if (init_params->search){ // Search read delay and write delay
        ret |= __psram_unified_search_read_dqs_delay(psram_src_data, init_params->read_delay);

        ret |= __psram_unified_search_write_dqs_delay(init_params->write_delay);

    } else { // Configure read delay and write delay immediately

        IP_PSRAM_CTRL->REG_DLLDELAY.bit.RDLVL_DELAY = *init_params->read_delay;
        IP_PSRAM_CTRL->REG_DLLDELAY.bit.WRLVL_DELAY = *init_params->write_delay;

        // resync DLL (0x28)
        IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 0x1;

    }

    // EYE DIAGRAM search end ########################################################

    return ret;
}

int32_t PSRAMUnified_GetInformation(uint32_t* density, __psram_unified_die_type_t* die_type){
    if (density == NULL || die_type == NULL){
        PSRAM_LOGE("PSRAM Unified GetInformation failed! NULL pointer of output parameters");
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    *density = __psram_die_density;
    *die_type = __psram_die_type;

    return CSK_DRIVER_OK;
}

int32_t PSRAMUnified_EnterSleepMode(__psram_unified_sleep_mode_t sleep_mode){
    if (__psram_die_type == __psram_unified_die_xccela){
        __psram_xccela_enter_sleep_mode(sleep_mode);
    } else if (__psram_die_type == __psram_unified_die_apm){
        __psram_apm_enter_sleep_mode(sleep_mode);
    } else {
        PSRAM_LOGE("PSRAM Unified EnterSleepMode failed! Unsupported die type %d", __psram_die_type);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    return CSK_DRIVER_OK;
}

int32_t PSRAMUnified_ExitSleepMode(__psram_unified_init_t* init_params, uint32_t density){
    int32_t ret = CSK_DRIVER_OK;

    if (init_params == NULL){
        PSRAM_LOGE("PSRAM Unified Init failed! NULL pointer of init_params");
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.PSRAM_CTRL_PRESET = 1;
    IP_SYSCTRL->REG_SW_RESET_CFG2.bit.PSRAM_CTRL_RESET = 1; // Reset PSRAM controller

    __psram_die_type = init_params->die_type;
    __psram_die_density = density;

    uint32_t __psram_unified_clock_freq = 0;
    {
        // Enable PSRAM controller clock
        __HAL_CRM_PSRAM_CLK_ENABLE();
        __psram_unified_clock_freq = CRM_GetPsramFreq();
        PSRAM_LOG("PSRAM Unified Clock Freq: %d", __psram_unified_clock_freq);
    }

    /////////// Driver Strength ///////////
#if PSRAM_UNIFIED_CONTROLLER_DRV_STR
	// Driver strength
    IP_CMN_SYS->REG_PSRAMIO_CFG0.all = (PSRAM_UNIFIED_CFG_IO_DRV_DQ0 | 
                                        (PSRAM_UNIFIED_CFG_IO_DRV_DQ1 << 3) |
                                        (PSRAM_UNIFIED_CFG_IO_DRV_DQ2 << 6) |
                                        (PSRAM_UNIFIED_CFG_IO_DRV_DQ3 << 9) |
                                        (PSRAM_UNIFIED_CFG_IO_DRV_DQ4 << 12) |
                                        (PSRAM_UNIFIED_CFG_IO_DRV_DQ5 << 15) |
                                        (PSRAM_UNIFIED_CFG_IO_DRV_DQ6 << 18) |
                                        (PSRAM_UNIFIED_CFG_IO_DRV_DQ7 << 21));
    IP_CMN_SYS->REG_PSRAMIO_CFG1.all = (PSRAM_UNIFIED_CFG_IO_DRV_DQS |
                                        (PSRAM_UNIFIED_CFG_IO_DRV_CEN << 6) |
                                        (PSRAM_UNIFIED_CFG_IO_DRV_CLK << 9));
#endif

    // MASTER configure
    {
        IP_PSRAM_CTRL->REG_RXBUFMSTID.bit.RXBUF0_MSTRID = init_params->fifo0_master;
        IP_PSRAM_CTRL->REG_RXBUFMSTID.bit.RXBUF1_MSTRID = init_params->fifo1_master;
        IP_PSRAM_CTRL->REG_RXBUFMSTID.bit.RXBUF2_MSTRID = init_params->fifo2_master;
    }

    // Prefetch
    {
        IP_PSRAM_CTRL->REG_RXBUFPFCTRL.bit.RXBUF0_PREFETCH_EN = init_params->fifo0_enable;
        IP_PSRAM_CTRL->REG_RXBUFPFCTRL.bit.RXBUF1_PREFETCH_EN = init_params->fifo1_enable;
        IP_PSRAM_CTRL->REG_RXBUFPFCTRL.bit.RXBUF2_PREFETCH_EN = init_params->fifo2_enable;
    }

    {
        IP_PSRAM_CTRL->REG_BUSPRIORCTRL.all = (PSRAM_UNIFIED_BUS_PRIORITY_MASTER0 |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER1 << 2) |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER2 << 4) |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER3 << 6) |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER4 << 8) |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER5 << 10) |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER6 << 12) |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER7 << 14) |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER8 << 16) |
                                               (PSRAM_UNIFIED_BUS_PRIORITY_MASTER9 << 18));

        PSRAM_LOG("PSRAM unified priority parameter: 0x%x", IP_PSRAM_CTRL->REG_BUSPRIORCTRL.all);
    }

    {
        ret = __psram_unified_dll_lock();
        if (ret < 0){
            return ret;
        }
    }

    ///// Check PSRAM clock /////
    if (__psram_die_type == __psram_unified_die_xccela){
        if (__psram_unified_clock_freq > PSRAM_UNIFIED_XCCELA_MAX_FREQ){
#if PSRAM_UNIFIED_FREQ_CORRECTION
            CRM_InitSyspllPsram(PSRAM_UNIFIED_XCCELA_MAX_FREQ_PARA);

            __psram_unified_clock_freq = PSRAM_UNIFIED_XCCELA_MAX_FREQ;
#else
            PSRAM_LOGE("PSRAM Unified Init failed! Exceed max clock freq for Xccela die");
            return CSK_DRIVER_ERROR_UNSUPPORTED;
#endif
        }
    } else if (__psram_die_type == __psram_unified_die_apm){
        if (__psram_unified_clock_freq > PSRAM_UNIFIED_APM_MAX_FREQ){
#if PSRAM_UNIFIED_FREQ_CORRECTION
            CRM_InitSyspllPsram(PSRAM_UNIFIED_APM_MAX_FREQ_PARA);

            __psram_unified_clock_freq = PSRAM_UNIFIED_APM_MAX_FREQ;
#else
            PSRAM_LOGE("PSRAM Unified Init failed! Exceed max clock freq for APM die");
            return CSK_DRIVER_ERROR_UNSUPPORTED;
#endif
        }
    }

    if (__psram_die_type == __psram_unified_die_xccela){
        PSRAM_LOG("PSRAM Unified Exit Sleep Mode: Xccela die");
        __psram_xccela_exit_sleep_mode();

        __psram_xccela_controller_die_type();

        __psram_xccela_mr_seq_configure();

        __psram_xccela_controller_timing_configure(__psram_unified_clock_freq);

        __psram_xccela_ahb_seq_configure(__psram_unified_clock_freq, __psram_die_density);

        __psram_xccela_controller_die_page_size(__psram_die_density);

        __psram_xccela_refresh_rate_normal_set();

        __psram_xccela_mr_print();

    } else if (__psram_die_type == __psram_unified_die_apm){
        PSRAM_LOG("PSRAM Unified Exit Sleep Mode: APM die");
        __psram_apm_exit_sleep_mode();

        __psram_apm_controller_die_type();

        __psram_apm_mr_seq_configure();

        __psram_apm_ahb_seq_configure(__psram_unified_clock_freq, __psram_die_density);

        __psram_apm_controller_die_page_size(__psram_die_density);

        __psram_apm_controller_timing_configure(__psram_unified_clock_freq);

        __psram_apm_refresh_rate_normal_set();

        __psram_apm_mr_print();
    } else {
        PSRAM_LOGE("PSRAM Unified Init failed! Unsupported die type %d", __psram_die_type);
        return CSK_DRIVER_ERROR_UNSUPPORTED;
    }

    if (init_params->write_delay != NULL && init_params->read_delay != NULL){
        IP_PSRAM_CTRL->REG_DLLDELAY.bit.RDLVL_DELAY = *init_params->read_delay;
        IP_PSRAM_CTRL->REG_DLLDELAY.bit.WRLVL_DELAY = *init_params->write_delay;

        // resync DLL (0x28)
        IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 0x1;
    } else {
        return CSK_DRIVER_ERROR_PARAMETER;
    }

    return ret;
}
