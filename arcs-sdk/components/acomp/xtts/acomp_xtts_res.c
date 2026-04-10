/*
 * XTTS resource prepare/cleanup — SDK layer
 *
 * Handles XTTS init, callback registration, resource prepare,
 * stream channel setup, start/stop/cleanup.
 * Resource addresses are configured via Kconfig.
 * Does NOT call cv_cleanup or translation_cleanup —
 * the application layer is responsible for coordinating algorithm lifecycle.
 */

#include <string.h>
#include "plat_os.h"
#include "acomp_xtts.h"
#include "resmgr.h"

#define TAG "xtts_res"
#include "lisa_log.h"

#define RES_STORAGE_FLASH  0
#define RES_STORAGE_SD     1

#define XTTS_RES_COUNT           7
#define XTTS_RES_ROLE_DATA       0
#define XTTS_RES_FRONT_MAIN      1
#define XTTS_RES_FRONT_CNCN_DICT 2
#define XTTS_RES_FRONT_ENUS_DICT 3
#define XTTS_RES_FRONT_CRF       4
#define XTTS_RES_REAR            5
#define XTTS_RES_REAR_NHV        6

static int g_xtts_prepared = 0;
static int g_xtts_inited = 0;

int acomp_xtts_do_prepare(xtts_event_cb_t event_cb, void *cb_priv)
{
    if (g_xtts_prepared) return 0;

    int ret;

    /* First-time init + callback registration */
    if (!g_xtts_inited) {
        ret = acomp_xtts_init();
        LISA_LOGI(TAG, "init ret:%d", ret);
        if (ret != 0) return ret;

        if (event_cb) {
            acomp_xtts_add_callback(XTTS_CB_EVENT_STATUS | XTTS_CB_EVENT_STREAM_UPDATE,
                                    event_cb, cb_priv);
        }
        g_xtts_inited = 1;
    }

    uint32_t size = sizeof(acomp_ipc_prepare_t) + sizeof(acomp_res_item_t) * XTTS_RES_COUNT;
    acomp_ipc_prepare_t *prepare = os_mem_alloc(size);
    if (!prepare) return -1;

    memset(prepare, 0, size);
    prepare->number = XTTS_RES_COUNT;

    /* [0] ROLE_DATA from flash (via resmgr) */
    prepare->item[XTTS_RES_ROLE_DATA].index = XTTS_RES_ROLE_DATA;
    {
        uint32_t res_size = 0;
        void *addr = resmgr_get_item(CONFIG_ACOMP_XTTS_RES_ROLE_RESID, &res_size);
        if (addr && res_size > 0) {
            prepare->item[XTTS_RES_ROLE_DATA].attr.hdr.storage = RES_STORAGE_FLASH;
            prepare->item[XTTS_RES_ROLE_DATA].addr = (uint32_t)(uintptr_t)addr;
            prepare->item[XTTS_RES_ROLE_DATA].size = res_size;
            LISA_LOGI(TAG, "role: flash addr=%p size=%u", addr, res_size);
        }
    }

    /* [1] FRONT_MAIN from flash (via resmgr) */
    prepare->item[XTTS_RES_FRONT_MAIN].index = XTTS_RES_FRONT_MAIN;
    {
        uint32_t res_size = 0;
        void *addr = resmgr_get_item(CONFIG_ACOMP_XTTS_RES_FRONT_RESID, &res_size);
        if (addr && res_size > 0) {
            prepare->item[XTTS_RES_FRONT_MAIN].attr.hdr.storage = RES_STORAGE_FLASH;
            prepare->item[XTTS_RES_FRONT_MAIN].addr = (uint32_t)(uintptr_t)addr;
            prepare->item[XTTS_RES_FRONT_MAIN].size = res_size;
            LISA_LOGI(TAG, "front: flash addr=%p size=%u", addr, res_size);
        }
    }

    /* [2] FRONT_CNCN_DICT from eMMC */
    prepare->item[XTTS_RES_FRONT_CNCN_DICT].index = XTTS_RES_FRONT_CNCN_DICT;
    prepare->item[XTTS_RES_FRONT_CNCN_DICT].attr.hdr.storage = RES_STORAGE_SD;
    prepare->item[XTTS_RES_FRONT_CNCN_DICT].addr = CONFIG_ACOMP_XTTS_RES_CNCN_DICT_EMMC_ADDR;
    prepare->item[XTTS_RES_FRONT_CNCN_DICT].size = CONFIG_ACOMP_XTTS_RES_CNCN_DICT_SIZE;

    /* [3] FRONT_ENUS_DICT from eMMC */
    prepare->item[XTTS_RES_FRONT_ENUS_DICT].index = XTTS_RES_FRONT_ENUS_DICT;
    prepare->item[XTTS_RES_FRONT_ENUS_DICT].attr.hdr.storage = RES_STORAGE_SD;
    prepare->item[XTTS_RES_FRONT_ENUS_DICT].addr = CONFIG_ACOMP_XTTS_RES_ENUS_DICT_EMMC_ADDR;
    prepare->item[XTTS_RES_FRONT_ENUS_DICT].size = CONFIG_ACOMP_XTTS_RES_ENUS_DICT_SIZE;

    /* [4] FRONT_CRF from eMMC */
    prepare->item[XTTS_RES_FRONT_CRF].index = XTTS_RES_FRONT_CRF;
    prepare->item[XTTS_RES_FRONT_CRF].attr.hdr.storage = RES_STORAGE_SD;
    prepare->item[XTTS_RES_FRONT_CRF].addr = CONFIG_ACOMP_XTTS_RES_CRF_EMMC_ADDR;
    prepare->item[XTTS_RES_FRONT_CRF].size = CONFIG_ACOMP_XTTS_RES_CRF_SIZE;

    /* [5] REAR from eMMC */
    prepare->item[XTTS_RES_REAR].index = XTTS_RES_REAR;
    prepare->item[XTTS_RES_REAR].attr.hdr.storage = RES_STORAGE_SD;
    prepare->item[XTTS_RES_REAR].addr = CONFIG_ACOMP_XTTS_RES_REAR_EMMC_ADDR;
    prepare->item[XTTS_RES_REAR].size = CONFIG_ACOMP_XTTS_RES_REAR_SIZE;

    /* [6] REAR_NHV from eMMC */
    prepare->item[XTTS_RES_REAR_NHV].index = XTTS_RES_REAR_NHV;
    prepare->item[XTTS_RES_REAR_NHV].attr.hdr.storage = RES_STORAGE_SD;
    prepare->item[XTTS_RES_REAR_NHV].addr = CONFIG_ACOMP_XTTS_RES_REAR_NHV_EMMC_ADDR;
    prepare->item[XTTS_RES_REAR_NHV].size = CONFIG_ACOMP_XTTS_RES_REAR_NHV_SIZE;

    ret = acomp_xtts_prepare(prepare);
    LISA_LOGI(TAG, "prepare ret:%d", ret);
    os_mem_free(prepare);

    if (ret != 0) return ret;

    /* Stream channel must be created before start!
     * AP's xtts_start() looks for R2M channel and assigns pcm_out_ch.
     * If channel is not yet created, pcm_out_ch=NULL and all PCM data is discarded. */
    {
        acomp_stream_chn_create_desc_t desc = {
            .cname = "stream.xtts_pcm",
            .direction = 1, /* R2M */
            .index = 0,
            .buffer_size = CONFIG_ACOMP_XTTS_STREAM_BUFFER_SIZE,
            .num_descs = CONFIG_ACOMP_XTTS_STREAM_NUM_DESCS,
            .kick_policy = 1,
        };
        ret = acomp_xtts_stream_ch_enable(0, &desc);
        LISA_LOGI(TAG, "stream ch enable ret:%d", ret);
        if (ret != 0) {
            acomp_xtts_cleanup();
            return ret;
        }
    }

    ret = acomp_xtts_start();
    LISA_LOGI(TAG, "start ret:%d", ret);

    if (ret != 0) {
        acomp_xtts_cleanup();
        return ret;
    }

    g_xtts_prepared = 1;
    return 0;
}

int acomp_xtts_do_cleanup(void)
{
    if (!g_xtts_prepared) return 0;
    acomp_xtts_stop();
    acomp_xtts_cleanup();
    g_xtts_prepared = 0;
    LISA_LOGI(TAG, "cleanup done");
    return 0;
}
