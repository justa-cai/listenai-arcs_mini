/*
 * Translation resource prepare/cleanup — SDK layer
 *
 * Handles Translation init, callback registration, resource prepare,
 * start/stop/cleanup. All 8 resources come from eMMC.
 * Resource addresses are configured via Kconfig.
 * Does NOT call cv_cleanup or xtts_cleanup —
 * the application layer is responsible for coordinating algorithm lifecycle.
 */

#include <string.h>
#include "lisa_mem.h"
#include "acomp_translation.h"
#include "resmgr.h"

#define TAG "trans_res"
#include "lisa_log.h"

#define RES_STORAGE_FLASH  0
#define RES_STORAGE_SD     1
#define TRANS_RES_COUNT    20  /* ENCN 9 + CNEN 9 + trandb + dict */

static int g_trans_prepared = 0;
static int g_trans_inited = 0;

int acomp_translation_do_prepare(trans_event_cb_t event_cb, void *cb_priv)
{
    if (g_trans_prepared) return 0;

    int ret;

    /* First-time init + callback registration */
    if (!g_trans_inited) {
        ret = acomp_translation_init();
        LISA_LOGI(TAG, "init ret:%d", ret);
        if (ret != 0) return ret;
        if (event_cb) {
            acomp_translation_add_callback(TRANS_CB_EVENT_RESULT | TRANS_CB_EVENT_STATUS,
                                           event_cb, cb_priv);
        }
        g_trans_inited = 1;
    }

    uint32_t size = sizeof(acomp_ipc_prepare_t) + sizeof(acomp_res_item_t) * TRANS_RES_COUNT;
    acomp_ipc_prepare_t *prepare = lisa_mem_alloc(size);
    if (!prepare) return -1;

    memset(prepare, 0, size);
    prepare->number = TRANS_RES_COUNT;

    /* ENCN encoder A-F from eMMC */
    struct { uint32_t addr; uint32_t sz; } encn[] = {
        { CONFIG_ACOMP_TRANS_RES_ENCN_ENC_A_EMMC_ADDR, CONFIG_ACOMP_TRANS_RES_ENCN_ENC_A_SIZE },
        { CONFIG_ACOMP_TRANS_RES_ENCN_ENC_B_EMMC_ADDR, CONFIG_ACOMP_TRANS_RES_ENCN_ENC_B_SIZE },
        { CONFIG_ACOMP_TRANS_RES_ENCN_ENC_C_EMMC_ADDR, CONFIG_ACOMP_TRANS_RES_ENCN_ENC_C_SIZE },
        { CONFIG_ACOMP_TRANS_RES_ENCN_ENC_D_EMMC_ADDR, CONFIG_ACOMP_TRANS_RES_ENCN_ENC_D_SIZE },
        { CONFIG_ACOMP_TRANS_RES_ENCN_ENC_E_EMMC_ADDR, CONFIG_ACOMP_TRANS_RES_ENCN_ENC_E_SIZE },
        { CONFIG_ACOMP_TRANS_RES_ENCN_ENC_F_EMMC_ADDR, CONFIG_ACOMP_TRANS_RES_ENCN_ENC_F_SIZE },
    };
    for (int i = 0; i < 6; i++) {
        prepare->item[i].index = i;
        prepare->item[i].attr.hdr.storage = RES_STORAGE_SD;
        prepare->item[i].addr = encn[i].addr;
        prepare->item[i].size = encn[i].sz;
    }

    /* ENCN decoder from eMMC */
    prepare->item[6].index = 6;
    prepare->item[6].attr.hdr.storage = RES_STORAGE_SD;
    prepare->item[6].addr = CONFIG_ACOMP_TRANS_RES_ENCN_DEC_EMMC_ADDR;
    prepare->item[6].size = CONFIG_ACOMP_TRANS_RES_ENCN_DEC_SIZE;

    /* ENCN decoder embedding from eMMC */
    prepare->item[7].index = 7;
    prepare->item[7].attr.hdr.storage = RES_STORAGE_SD;
    prepare->item[7].addr = CONFIG_ACOMP_TRANS_RES_ENCN_DEC_EMB_EMMC_ADDR;
    prepare->item[7].size = CONFIG_ACOMP_TRANS_RES_ENCN_DEC_EMB_SIZE;

    /* ENCN encoder embedding from eMMC */
    prepare->item[8].index = 8;
    prepare->item[8].attr.hdr.storage = RES_STORAGE_SD;
    prepare->item[8].addr = CONFIG_ACOMP_TRANS_RES_ENCN_ENC_EMB_EMMC_ADDR;
    prepare->item[8].size = CONFIG_ACOMP_TRANS_RES_ENCN_ENC_EMB_SIZE;

    /* CNEN encoder A-F from eMMC */
    struct { uint32_t addr; uint32_t sz; } cnen[] = {
        { CONFIG_ACOMP_TRANS_RES_CNEN_ENC_A_EMMC_ADDR, CONFIG_ACOMP_TRANS_RES_CNEN_ENC_A_SIZE },
        { CONFIG_ACOMP_TRANS_RES_CNEN_ENC_B_EMMC_ADDR, CONFIG_ACOMP_TRANS_RES_CNEN_ENC_B_SIZE },
        { CONFIG_ACOMP_TRANS_RES_CNEN_ENC_C_EMMC_ADDR, CONFIG_ACOMP_TRANS_RES_CNEN_ENC_C_SIZE },
        { CONFIG_ACOMP_TRANS_RES_CNEN_ENC_D_EMMC_ADDR, CONFIG_ACOMP_TRANS_RES_CNEN_ENC_D_SIZE },
        { CONFIG_ACOMP_TRANS_RES_CNEN_ENC_E_EMMC_ADDR, CONFIG_ACOMP_TRANS_RES_CNEN_ENC_E_SIZE },
        { CONFIG_ACOMP_TRANS_RES_CNEN_ENC_F_EMMC_ADDR, CONFIG_ACOMP_TRANS_RES_CNEN_ENC_F_SIZE },
    };
    for (int i = 0; i < 6; i++) {
        prepare->item[9 + i].index = 9 + i;
        prepare->item[9 + i].attr.hdr.storage = RES_STORAGE_SD;
        prepare->item[9 + i].addr = cnen[i].addr;
        prepare->item[9 + i].size = cnen[i].sz;
    }

    /* CNEN decoder from eMMC */
    prepare->item[15].index = 15;
    prepare->item[15].attr.hdr.storage = RES_STORAGE_SD;
    prepare->item[15].addr = CONFIG_ACOMP_TRANS_RES_CNEN_DEC_EMMC_ADDR;
    prepare->item[15].size = CONFIG_ACOMP_TRANS_RES_CNEN_DEC_SIZE;

    /* CNEN decoder embedding from eMMC */
    prepare->item[16].index = 16;
    prepare->item[16].attr.hdr.storage = RES_STORAGE_SD;
    prepare->item[16].addr = CONFIG_ACOMP_TRANS_RES_CNEN_DEC_EMB_EMMC_ADDR;
    prepare->item[16].size = CONFIG_ACOMP_TRANS_RES_CNEN_DEC_EMB_SIZE;

    /* CNEN encoder embedding from eMMC */
    prepare->item[17].index = 17;
    prepare->item[17].attr.hdr.storage = RES_STORAGE_SD;
    prepare->item[17].addr = CONFIG_ACOMP_TRANS_RES_CNEN_ENC_EMB_EMMC_ADDR;
    prepare->item[17].size = CONFIG_ACOMP_TRANS_RES_CNEN_ENC_EMB_SIZE;

    /* Trandb from flash (via resmgr) */
    prepare->item[18].index = 18;
    {
        uint32_t res_size = 0;
        void *addr = resmgr_get_item(RES_TRANS_TRANDB, &res_size);
        if (addr && res_size > 0) {
            prepare->item[18].attr.hdr.storage = RES_STORAGE_FLASH;
            prepare->item[18].addr = (uint32_t)(uintptr_t)addr;
            prepare->item[18].size = res_size;
            LISA_LOGI(TAG, "trandb: flash addr=%p size=%u", addr, res_size);
        }
    }

    /* Dict from flash (via resmgr) */
    prepare->item[19].index = 19;
    {
        uint32_t res_size = 0;
        void *addr = resmgr_get_item(RES_TRANS_DICT_TRAD, &res_size);
        if (addr && res_size > 0) {
            prepare->item[19].attr.hdr.storage = RES_STORAGE_FLASH;
            prepare->item[19].addr = (uint32_t)(uintptr_t)addr;
            prepare->item[19].size = res_size;
            LISA_LOGI(TAG, "dict: flash addr=%p size=%u", addr, res_size);
        }
    }

    ret = acomp_translation_prepare(prepare);
    LISA_LOGI(TAG, "prepare ret:%d", ret);
    lisa_mem_free(prepare);

    if (ret != 0) return ret;

    ret = acomp_translation_start();
    LISA_LOGI(TAG, "start ret:%d", ret);

    if (ret != 0) {
        acomp_translation_cleanup();
        return ret;
    }

    g_trans_prepared = 1;
    return 0;
}

int acomp_translation_do_cleanup(void)
{
    if (!g_trans_prepared && !g_trans_inited) return 0;
    if (g_trans_prepared) {
        acomp_translation_stop();
    }
    acomp_translation_cleanup();
    g_trans_prepared = 0;
    g_trans_inited = 0;
    LISA_LOGI(TAG, "cleanup done");
    return 0;
}
