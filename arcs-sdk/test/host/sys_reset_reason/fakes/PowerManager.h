#ifndef TEST_HOST_SYS_RESET_REASON_POWERMANAGER_H_
#define TEST_HOST_SYS_RESET_REASON_POWERMANAGER_H_

#include <stdint.h>

typedef enum _pmu_rstsrc {
    PMU_RST_POR        = 0U,
    PMU_RST_AON        = 1U,
    PMU_RST_CP_WDT     = 16U,
    PMU_RST_CMN        = 17U,
    PMU_RST_CP_SW      = 18U,
    PMU_RST_AP_SW_WDT  = 19U,
    PMU_RST_NONE       = 0xFFU,
} pmu_rstsrc_t;

uint32_t HAL_PMU_GetSysResetCauseRaw(void);
void HAL_PMU_SnapshotResetCause(void);

#endif
