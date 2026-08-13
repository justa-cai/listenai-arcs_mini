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
#include "lisa_mem.h"
#include "acomp_xtts.h"
#include "resmgr.h"

#define TAG "xtts_res"
#include "lisa_log.h"

#define RES_STORAGE_FLASH  0
#define RES_STORAGE_SD     1

#define XTTS_RES_COUNT           8
#define XTTS_RES_ROLE_DATA       0
#define XTTS_RES_FRONT_MAIN      1
#define XTTS_RES_FRONT_CNCN_DICT 2
#define XTTS_RES_FRONT_ENUS_DICT 3
#define XTTS_RES_FRONT_CRF       4
#define XTTS_RES_FRONT_USER      5
#define XTTS_RES_REAR            6
#define XTTS_RES_REAR_NHV        7

#define XTTS_ROLE_DEFAULT        0
#define XTTS_ROLE_LINGXIAOQI     1
#define XTTS_ROLE_LUCY           2

#define XTTS_LUCY_FRONT_DICT_EMMC_ADDR 0x4e5f000
#define XTTS_LUCY_FRONT_DICT_SIZE      4695232
#define XTTS_LUCY_REAR_EMMC_ADDR       0x5320000
#define XTTS_LUCY_REAR_SIZE            4154272
#define XTTS_LUCY_REAR_NHV_EMMC_ADDR   0x52da000
#define XTTS_LUCY_REAR_NHV_SIZE        284992

static int g_xtts_prepared = 0;
static int g_xtts_inited = 0;
static int g_xtts_prepared_role = XTTS_ROLE_DEFAULT;

static int xtts_normalize_role(int role)
{
    if (role == XTTS_ROLE_LUCY) {
        return XTTS_ROLE_LUCY;
    }

    return XTTS_ROLE_LINGXIAOQI;
}

static void xtts_prepare_item_clear(acomp_res_item_t *item, uint32_t index)
{
    memset(item, 0, sizeof(*item));
    item->index = index;
}

static int xtts_prepare_flash_item(acomp_res_item_t *item, uint32_t index,
                                   uint32_t resid, const char *name)
{
    uint32_t res_size = 0;
    void *addr = resmgr_get_item(resid, &res_size);

    xtts_prepare_item_clear(item, index);
    if (!addr || res_size == 0) {
        LISA_LOGE(TAG, "%s missing: resid=%u addr=%p size=%u", name, resid, addr, res_size);
        return -1;
    }

    item->attr.hdr.storage = RES_STORAGE_FLASH;
    item->addr = (uint32_t)(uintptr_t)addr;
    item->size = res_size;
    LISA_LOGI(TAG, "%s: flash addr=%p size=%u", name, addr, res_size);
    return 0;
}

static void xtts_prepare_emmc_item(acomp_res_item_t *item, uint32_t index,
                                   uint32_t addr, uint32_t size, const char *name)
{
    xtts_prepare_item_clear(item, index);
    if (addr == 0 || size == 0) {
        LISA_LOGI(TAG, "%s: skipped", name);
        return;
    }

    item->attr.hdr.storage = RES_STORAGE_SD;
    item->addr = addr;
    item->size = size;
    LISA_LOGI(TAG, "%s: eMMC addr=0x%x size=%u", name, addr, size);
}

static int xtts_build_prepare_items(acomp_ipc_prepare_t *prepare, int role)
{
    int plan_role = xtts_normalize_role(role);

    if (plan_role == XTTS_ROLE_LUCY) {
        LISA_LOGI(TAG, "prepare xtts plan: lucy");
        if (xtts_prepare_flash_item(&prepare->item[XTTS_RES_ROLE_DATA],
                                    XTTS_RES_ROLE_DATA,
                                    RES_XTTS_ENGAM_LUCY_LOW_ARM,
                                    "role") != 0) {
            return -1;
        }
        if (xtts_prepare_flash_item(&prepare->item[XTTS_RES_FRONT_MAIN],
                                    XTTS_RES_FRONT_MAIN,
                                    RES_XTTS_ENGAM_FRONT_COMMON_MAIN,
                                    "front_main") != 0) {
            return -1;
        }
        xtts_prepare_item_clear(&prepare->item[XTTS_RES_FRONT_CNCN_DICT], XTTS_RES_FRONT_CNCN_DICT);
        xtts_prepare_emmc_item(&prepare->item[XTTS_RES_FRONT_ENUS_DICT],
                               XTTS_RES_FRONT_ENUS_DICT,
                               XTTS_LUCY_FRONT_DICT_EMMC_ADDR,
                               XTTS_LUCY_FRONT_DICT_SIZE,
                               "front_engam_dict");
        xtts_prepare_item_clear(&prepare->item[XTTS_RES_FRONT_CRF], XTTS_RES_FRONT_CRF);
        xtts_prepare_item_clear(&prepare->item[XTTS_RES_FRONT_USER], XTTS_RES_FRONT_USER);
        xtts_prepare_emmc_item(&prepare->item[XTTS_RES_REAR],
                               XTTS_RES_REAR,
                               XTTS_LUCY_REAR_EMMC_ADDR,
                               XTTS_LUCY_REAR_SIZE,
                               "rear");
        xtts_prepare_emmc_item(&prepare->item[XTTS_RES_REAR_NHV],
                               XTTS_RES_REAR_NHV,
                               XTTS_LUCY_REAR_NHV_EMMC_ADDR,
                               XTTS_LUCY_REAR_NHV_SIZE,
                               "rear_nhv");
        return 0;
    }

    LISA_LOGI(TAG, "prepare xtts plan: cn");
    if (xtts_prepare_flash_item(&prepare->item[XTTS_RES_ROLE_DATA],
                                XTTS_RES_ROLE_DATA,
                                CONFIG_ACOMP_XTTS_RES_ROLE_RESID,
                                "role") != 0) {
        return -1;
    }
    if (xtts_prepare_flash_item(&prepare->item[XTTS_RES_FRONT_MAIN],
                                XTTS_RES_FRONT_MAIN,
                                CONFIG_ACOMP_XTTS_RES_FRONT_RESID,
                                "front_main") != 0) {
        return -1;
    }
    xtts_prepare_emmc_item(&prepare->item[XTTS_RES_FRONT_CNCN_DICT],
                           XTTS_RES_FRONT_CNCN_DICT,
                           CONFIG_ACOMP_XTTS_RES_CNCN_DICT_EMMC_ADDR,
                           CONFIG_ACOMP_XTTS_RES_CNCN_DICT_SIZE,
                           "front_cncn_dict");
    xtts_prepare_emmc_item(&prepare->item[XTTS_RES_FRONT_ENUS_DICT],
                           XTTS_RES_FRONT_ENUS_DICT,
                           CONFIG_ACOMP_XTTS_RES_ENUS_DICT_EMMC_ADDR,
                           CONFIG_ACOMP_XTTS_RES_ENUS_DICT_SIZE,
                           "front_enus_dict");
    xtts_prepare_emmc_item(&prepare->item[XTTS_RES_FRONT_CRF],
                           XTTS_RES_FRONT_CRF,
                           CONFIG_ACOMP_XTTS_RES_CRF_EMMC_ADDR,
                           CONFIG_ACOMP_XTTS_RES_CRF_SIZE,
                           "front_crf");
    xtts_prepare_emmc_item(&prepare->item[XTTS_RES_FRONT_USER],
                           XTTS_RES_FRONT_USER,
                           CONFIG_ACOMP_XTTS_RES_FRONT_USER_EMMC_ADDR,
                           CONFIG_ACOMP_XTTS_RES_FRONT_USER_SIZE,
                           "front_user");
    xtts_prepare_emmc_item(&prepare->item[XTTS_RES_REAR],
                           XTTS_RES_REAR,
                           CONFIG_ACOMP_XTTS_RES_REAR_EMMC_ADDR,
                           CONFIG_ACOMP_XTTS_RES_REAR_SIZE,
                           "rear");
    xtts_prepare_emmc_item(&prepare->item[XTTS_RES_REAR_NHV],
                           XTTS_RES_REAR_NHV,
                           CONFIG_ACOMP_XTTS_RES_REAR_NHV_EMMC_ADDR,
                           CONFIG_ACOMP_XTTS_RES_REAR_NHV_SIZE,
                           "rear_nhv");
    return 0;
}

int acomp_xtts_do_prepare_with_role(int role, xtts_event_cb_t event_cb, void *cb_priv)
{
    int plan_role = xtts_normalize_role(role);

    if (g_xtts_prepared && g_xtts_prepared_role == plan_role) {
        return 0;
    }

    if (g_xtts_prepared || g_xtts_inited) {
        LISA_LOGI(TAG, "role changed or stale state, reprepare: old=%d new=%d",
                  g_xtts_prepared_role, plan_role);
        acomp_xtts_do_cleanup();
    }

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
    acomp_ipc_prepare_t *prepare = lisa_mem_alloc(size);
    if (!prepare) return -1;

    memset(prepare, 0, size);
    prepare->number = XTTS_RES_COUNT;
    ret = xtts_build_prepare_items(prepare, plan_role);
    if (ret != 0) {
        lisa_mem_free(prepare);
        return ret;
    }

    ret = acomp_xtts_prepare(prepare);
    LISA_LOGI(TAG, "prepare ret:%d", ret);
    lisa_mem_free(prepare);

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
            .kick_policy = 0,
        };
        ret = acomp_xtts_stream_ch_enable(0, &desc);
        LISA_LOGI(TAG, "stream ch enable ret:%d", ret);
        if (ret != 0) {
            acomp_xtts_do_cleanup();
            return ret;
        }
    }

    ret = acomp_xtts_start();
    LISA_LOGI(TAG, "start ret:%d", ret);

    if (ret != 0) {
        acomp_xtts_do_cleanup();
        return ret;
    }

    g_xtts_prepared = 1;
    g_xtts_prepared_role = plan_role;
    return 0;
}

int acomp_xtts_do_prepare(xtts_event_cb_t event_cb, void *cb_priv)
{
    return acomp_xtts_do_prepare_with_role(XTTS_ROLE_LINGXIAOQI, event_cb, cb_priv);
}

int acomp_xtts_do_cleanup(void)
{
    if (!g_xtts_prepared && !g_xtts_inited) return 0;
    acomp_xtts_stop();
    /* Disable the IPC stream channel created in do_prepare; otherwise
     * its ~525KB PSRAM buffer leaks across prepare/cleanup cycles.
     * Must run after stop (so the AP-side producer halts) and before
     * acomp_xtts_cleanup (which clears xtts_handle and would make the
     * disable a no-op). */
    acomp_xtts_stream_ch_disable(0);
    acomp_xtts_cleanup();
    g_xtts_prepared = 0;
    g_xtts_inited = 0;
    g_xtts_prepared_role = XTTS_ROLE_DEFAULT;
    LISA_LOGI(TAG, "cleanup done");
    return 0;
}
