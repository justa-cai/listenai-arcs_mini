#include <stdint.h>

#include "cache.h"

#include "PSRAM_Unified_Common.h"

typedef struct {
    uint32_t delay_max;
    uint32_t delay_min;
    uint32_t delay_gap;
} DqsDelay_FmtDef;

#ifdef PSRAM_UNIFIED_FT_TEST
#define PSRAM_UNIFIED_FT_DQS_INVALID 0xFFFFFFFFU

uint32_t __g_write_dqs_delay_min = PSRAM_UNIFIED_FT_DQS_INVALID;
uint32_t __g_write_dqs_delay_max = PSRAM_UNIFIED_FT_DQS_INVALID;
uint32_t __g_write_dqs_delay = PSRAM_UNIFIED_FT_DQS_INVALID;

uint32_t __g_read_dqs_delay_min = PSRAM_UNIFIED_FT_DQS_INVALID;
uint32_t __g_read_dqs_delay_max = PSRAM_UNIFIED_FT_DQS_INVALID;
uint32_t __g_read_dqs_delay = PSRAM_UNIFIED_FT_DQS_INVALID;

static void psram_unified_ft_reset_write_dqs(void)
{
    __g_write_dqs_delay_min = PSRAM_UNIFIED_FT_DQS_INVALID;
    __g_write_dqs_delay_max = PSRAM_UNIFIED_FT_DQS_INVALID;
    __g_write_dqs_delay = PSRAM_UNIFIED_FT_DQS_INVALID;
}

static void psram_unified_ft_reset_read_dqs(void)
{
    __g_read_dqs_delay_min = PSRAM_UNIFIED_FT_DQS_INVALID;
    __g_read_dqs_delay_max = PSRAM_UNIFIED_FT_DQS_INVALID;
    __g_read_dqs_delay = PSRAM_UNIFIED_FT_DQS_INVALID;
}
#endif

static uint32_t psram_rand_seed = 0x12345678;

static uint32_t psram_rand(void)
{
    uint32_t x = psram_rand_seed;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    psram_rand_seed = x;
    return x;
}

int32_t __psram_unified_dll_lock(void)
{
#define PSRAM_DLL_LOCK_TIMEOUT 1000000
#define PSRAM_DETECT_PHASE_MAX 8
    uint8_t rdata = 0;
    uint32_t i = 0;
    uint8_t phase_detect_sel = 1;

    IP_PSRAM_CTRL->REG_DLLEN.bit.DLL_BYPASS = 0x0;
    IP_PSRAM_CTRL->REG_DLLEN.bit.PHASE_DETECT_SEL = phase_detect_sel;
    IP_PSRAM_CTRL->REG_DLLEN.bit.DLL_EN = 1;
    IP_PSRAM_CTRL->REG_DLLRST.all = 1;

    PSRAM_LOG("Wait PSRAM DLL done");

    do {
        rdata = IP_PSRAM_CTRL->REG_LOCKDONE.all;
        i++;

        if (i > PSRAM_DLL_LOCK_TIMEOUT) {
            IP_PSRAM_CTRL->REG_DLLEN.bit.DLL_EN = 0;
            phase_detect_sel++;

            if (phase_detect_sel >= PSRAM_DETECT_PHASE_MAX) {
                PSRAM_LOGE("PSRAM DLL lock failed");
                return CSK_DRIVER_ERROR_TIMEOUT;
            }

            i = 0;
            IP_PSRAM_CTRL->REG_DLLEN.bit.PHASE_DETECT_SEL = phase_detect_sel;
            IP_PSRAM_CTRL->REG_DLLEN.bit.DLL_EN = 1;
            IP_PSRAM_CTRL->REG_DLLRST.all = 1;
        }
    } while (rdata != 1);

    uint32_t div = IP_PSRAM_CTRL->REG_DLLOBSVR0.bit.HALF_CLOCK_MODE ? 2 : 4;
    PSRAM_LOG("DLL lock, phase=%d value=%d div=%d",
              phase_detect_sel,
              IP_PSRAM_CTRL->REG_DLLOBSVR0.bit.DLL_LOCK_VALUE,
              div);

    IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 0x1;

    return CSK_DRIVER_OK;

#undef PSRAM_DLL_LOCK_TIMEOUT
#undef PSRAM_DETECT_PHASE_MAX
}

static void psram_data_init(uint32_t *psram_src_data)
{
    for (uint32_t i = 0; i < PSRAM_SEARCH_DQS_NUM; i++) {
        psram_src_data[i] = psram_rand();
    }
}

static uint32_t psram_random_write_data_check(uint32_t *psram_src_data,
                                              uint32_t *psram_dst_data,
                                              int32_t write_delay)
{
    uint32_t ret = 0;

    IP_PSRAM_CTRL->REG_DLLDELAY.bit.WRLVL_DELAY = write_delay;
    IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 1;

    for (uint32_t loop = 0; loop < PSRAM_INNER_SEARCH_LOOP_NUM; loop++) {
        psram_memcpy(psram_dst_data, psram_src_data,
                     (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));
        HAL_FlushInvalidateDCache_by_Addr(
            (uint32_t *)psram_dst_data,
            (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        ret |= psram_compare(psram_src_data, psram_dst_data,
                             (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        psram_dst_data += (PSRAM_SEARCH_DQS_NUM / 2);
    }

    return ret;
}

static uint32_t psram_random_read_data_check(uint32_t *psram_src_data,
                                             uint32_t *psram_dst_data,
                                             int32_t read_delay)
{
    uint32_t ret = 0;

    IP_PSRAM_CTRL->REG_DLLDELAY.bit.RDLVL_DELAY = read_delay;
    IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 1;

    for (uint32_t loop = 0; loop < PSRAM_INNER_SEARCH_LOOP_NUM; loop++) {
        HAL_InvalidateDCache_by_Addr(
            (uint32_t *)psram_dst_data,
            (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        ret |= psram_compare(psram_src_data, psram_dst_data,
                             (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));
        if (ret != 0) {
            break;
        }
    }

    return ret;
}

static uint32_t psram_random_read_data_check_1(uint32_t *psram_src_data,
                                               uint32_t *psram_dst_data,
                                               int32_t read_delay)
{
    uint32_t ret = 0;

    IP_PSRAM_CTRL->REG_DLLDELAY.bit.RDLVL_DELAY = read_delay;
    IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 1;

    for (uint32_t loop = 0; loop < PSRAM_INNER_SEARCH_LOOP_NUM; loop++) {
        psram_memcpy(psram_dst_data, psram_src_data,
                     (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));
        HAL_FlushInvalidateDCache_by_Addr(
            (uint32_t *)psram_dst_data,
            (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));

        ret |= psram_compare(psram_src_data, psram_dst_data,
                             (PSRAM_SEARCH_DQS_NUM / 2) * sizeof(uint32_t));
        if (ret != 0) {
            break;
        }
    }

    return ret;
}

static uint8_t quick_psram_search_dqs_delay(
    DqsDelay_FmtDef *dqs_delay,
    int32_t delay_reg,
    uint32_t *psram_src_data,
    uint32_t (*random_data_check_handler)(uint32_t *, uint32_t *, int32_t))
{
    uint32_t ret = 0;
    uint8_t flag = 0;
    int32_t delay;
    uint8_t finded = 0;
    int32_t delay_step = PSRAM_DELAY_STEP;
    int32_t delay_max = (random_data_check_handler == psram_random_write_data_check) ?
                        PSRAM_EYE_WRITE_DIAGRAM_TOP : PSRAM_EYE_READ_DIAGRAM_TOP;
    int32_t delay_min = (random_data_check_handler == psram_random_write_data_check) ?
                        PSRAM_EYE_WRITE_DIAGRAM_BOTTOM : PSRAM_EYE_READ_DIAGRAM_BOTTOM;
    uint32_t *psram_src_data_ptr;
    uint32_t local_src_data[PSRAM_SEARCH_DQS_NUM];
    uint8_t mod_i = 0;

    dqs_delay->delay_max = delay_reg;
    dqs_delay->delay_min = delay_reg;
    delay = dqs_delay->delay_max;

search_top:
    while (1) {
        if (psram_src_data == NULL) {
            psram_data_init(local_src_data);
            psram_src_data_ptr = local_src_data;
        } else {
            psram_src_data_ptr = psram_src_data;
        }

        if (mod_i == 0) {
            ret = random_data_check_handler(
                psram_src_data_ptr,
                (uint32_t *)PSRAM_UNIFIED_BASE_ADDRESS,
                delay);
            mod_i = 1;
        } else {
            ret = random_data_check_handler(
                psram_src_data_ptr + (PSRAM_SEARCH_DQS_NUM / 2),
                (uint32_t *)PSRAM_UNIFIED_BASE_ADDRESS + (PSRAM_SEARCH_DQS_NUM / 2),
                delay);
            mod_i = 0;
        }

        if (ret != 0) {
            PSRAM_LOG("[FAILED] DQS: %d", delay);

            if (finded == 1) {
                delay_max = delay - 1;
                delay = dqs_delay->delay_max + 1;
                if (delay > delay_max) {
                    break;
                }

                if (delay_step == 1) {
                    break;
                }

                delay_step = delay_step / 2;
                continue;
            }
        } else {
            PSRAM_LOG("[SUCCESS] DQS: %d", delay);

            if (finded == 0) {
                dqs_delay->delay_min = delay;
                dqs_delay->delay_max = delay;
                finded = 1;
            } else {
                dqs_delay->delay_max = delay;
            }
        }

        if (delay >= delay_max) {
            break;
        }

        delay += delay_step;
        if (delay > delay_max) {
            delay = delay_max;
        }
    }

    PSRAM_LOG("DQS delay search up: %d--%d",
              dqs_delay->delay_min,
              dqs_delay->delay_max);

    if (flag == 2) {
        return finded;
    }

    if (finded == 0) {
        delay_max = dqs_delay->delay_max - 1;
        flag = 1;
    }

    if ((dqs_delay->delay_min > (uint32_t)delay_reg) && (finded == 1)) {
        delay_min = delay_reg;
    }

    delay_step = PSRAM_DELAY_STEP;
    delay = dqs_delay->delay_min;

    while (1) {
        if (psram_src_data == NULL) {
            psram_data_init(local_src_data);
            psram_src_data_ptr = local_src_data;
        } else {
            psram_src_data_ptr = psram_src_data;
        }

        if (mod_i == 0) {
            ret = random_data_check_handler(
                psram_src_data_ptr,
                (uint32_t *)PSRAM_UNIFIED_BASE_ADDRESS,
                delay);
            mod_i = 1;
        } else {
            ret = random_data_check_handler(
                psram_src_data_ptr + (PSRAM_SEARCH_DQS_NUM / 2),
                (uint32_t *)PSRAM_UNIFIED_BASE_ADDRESS + (PSRAM_SEARCH_DQS_NUM / 2),
                delay);
            mod_i = 0;
        }

        if (ret != 0) {
            PSRAM_LOG("[FAILED] DQS: %d", delay);

            if (finded == 1) {
                delay_min = delay + 1;
                delay = dqs_delay->delay_min - 1;
                if (delay < delay_min) {
                    break;
                }

                if (delay_step == 1) {
                    break;
                }

                delay_step = delay_step / 2;
                continue;
            }
        } else {
            PSRAM_LOG("[SUCCESS] DQS: %d", delay);

            if (finded == 0) {
                dqs_delay->delay_min = delay;
                dqs_delay->delay_max = delay;
                finded = 1;
            } else {
                dqs_delay->delay_min = delay;
            }
        }

        if (delay <= delay_min) {
            break;
        }

        delay -= delay_step;
        if (delay < delay_min) {
            delay = delay_min;
        }
    }

    PSRAM_LOG("DQS delay search down: %d--%d",
              dqs_delay->delay_min,
              dqs_delay->delay_max);

    if (flag == 1) {
        delay = dqs_delay->delay_max + 1;
        flag = 2;
        goto search_top;
    }

    return finded;
}

int32_t __psram_unified_search_write_dqs_delay(uint32_t *g_wt_delay)
{
    uint32_t ret = CSK_DRIVER_OK;
    DqsDelay_FmtDef dqs_delay = {0};

    PSRAM_LOG("[INFO][START] search write DQS delay");

#ifdef PSRAM_UNIFIED_FT_TEST
    psram_unified_ft_reset_write_dqs();
#endif

    if (quick_psram_search_dqs_delay(&dqs_delay,
                                     IP_PSRAM_CTRL->REG_DLLDELAY.bit.WRLVL_DELAY,
                                     NULL,
                                     psram_random_write_data_check) == 0) {
        PSRAM_LOGE("[WRITE] DQS search failed");
        ret = CSK_DRIVER_ERROR;
    }

    dqs_delay.delay_gap =
        PSRAM_UNIFED_WT_DQS_POS_N * (dqs_delay.delay_max - dqs_delay.delay_min) /
        PSRAM_UNIFED_WT_DQS_POS_M + dqs_delay.delay_min;
    PSRAM_LOG("[WRITE] DQS range 0x%x-0x%x selected 0x%x",
              dqs_delay.delay_min,
              dqs_delay.delay_max,
              dqs_delay.delay_gap);

    IP_PSRAM_CTRL->REG_DLLDELAY.bit.WRLVL_DELAY = dqs_delay.delay_gap;
    IP_PSRAM_CTRL->REG_DLLRESYNC.bit.DLL_RESYNC = 1;

#ifdef PSRAM_UNIFIED_FT_TEST
    if (ret == CSK_DRIVER_OK) {
        __g_write_dqs_delay_min = dqs_delay.delay_min;
        __g_write_dqs_delay_max = dqs_delay.delay_max;
        __g_write_dqs_delay = dqs_delay.delay_gap;
    }
#endif

#ifdef PSRAM_UNIFIED_FT_TEST
    if ((g_wt_delay != NULL) && (ret == CSK_DRIVER_OK)) {
        *g_wt_delay = dqs_delay.delay_gap;
    }
#else
    if (g_wt_delay != NULL) {
        *g_wt_delay = dqs_delay.delay_gap;
    }
#endif

    return ret;
}

int32_t __psram_unified_search_read_dqs_delay(uint32_t *psram_src_data,
                                              uint32_t *g_rd_delay)
{
    uint32_t ret = CSK_DRIVER_OK;
    DqsDelay_FmtDef dqs_delay = {0};
    uint32_t (*checker)(uint32_t *, uint32_t *, int32_t) =
        (psram_src_data == NULL) ? psram_random_read_data_check_1 :
                                   psram_random_read_data_check;

    PSRAM_LOG("[INFO][START] search read DQS delay");

#ifdef PSRAM_UNIFIED_FT_TEST
    psram_unified_ft_reset_read_dqs();
#endif

    if (quick_psram_search_dqs_delay(&dqs_delay,
                                     IP_PSRAM_CTRL->REG_DLLDELAY.bit.RDLVL_DELAY,
                                     psram_src_data,
                                     checker) == 0) {
        PSRAM_LOGE("[READ] DQS search failed");
        ret = CSK_DRIVER_ERROR;
    }

    dqs_delay.delay_gap =
        PSRAM_UNIFED_RD_DQS_POS_N * (dqs_delay.delay_max - dqs_delay.delay_min) /
        PSRAM_UNIFED_RD_DQS_POS_M + dqs_delay.delay_min;
    PSRAM_LOG("[READ] DQS range 0x%x-0x%x selected 0x%x",
              dqs_delay.delay_min,
              dqs_delay.delay_max,
              dqs_delay.delay_gap);

    IP_PSRAM_CTRL->REG_DLLDELAY.bit.RDLVL_DELAY = dqs_delay.delay_gap;
    IP_PSRAM_CTRL->REG_DLLRESYNC.all = 1;

#ifdef PSRAM_UNIFIED_FT_TEST
    if (ret == CSK_DRIVER_OK) {
        __g_read_dqs_delay_min = dqs_delay.delay_min;
        __g_read_dqs_delay_max = dqs_delay.delay_max;
        __g_read_dqs_delay = dqs_delay.delay_gap;
    }
#endif

#ifdef PSRAM_UNIFIED_FT_TEST
    if ((g_rd_delay != NULL) && (ret == CSK_DRIVER_OK)) {
        *g_rd_delay = dqs_delay.delay_gap;
    }
#else
    if (g_rd_delay != NULL) {
        *g_rd_delay = dqs_delay.delay_gap;
    }
#endif

    return ret;
}
