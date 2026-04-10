/*
 * Resource manager — shared by cv, xtts, translation
 *
 * Parses a flash-resident resource header table and provides
 * res_item_by_id-style lookups for algorithm model resources.
 */

#include <stdint.h>
#include <stddef.h>

#include "resmgr.h"
#ifdef CONFIG_ACOMP_RESMGR_DATCHK
#include "crc32.h"
#endif

#define TAG "resmgr"
#include "lisa_log.h"

/* Expected tag in the resource header (must match respack tool config) */
#define RESMGR_TAGNAME  CONFIG_ACOMP_RESMGR_TAGNAME

static resmgr_hdr_t s_res_hdr = NULL;

int resmgr_init(uint32_t flash_addr)
{
    s_res_hdr = (resmgr_hdr_t)(uintptr_t)flash_addr;

    /* Validate header tag */
    if (s_res_hdr->hdrtag != RESMGR_TAGNAME) {
        LISA_LOGE(TAG, "tag mismatch: expect 0x%08x, got 0x%08x",
                  RESMGR_TAGNAME, s_res_hdr->hdrtag);
        s_res_hdr = NULL;
        return -1;
    }

    /* Sanity check itemcnt to avoid reading out of bounds on corrupted flash */
    if (s_res_hdr->itemcnt > 256) {
        LISA_LOGE(TAG, "itemcnt too large: %u", s_res_hdr->itemcnt);
        s_res_hdr = NULL;
        return -1;
    }

#ifdef CONFIG_ACOMP_RESMGR_DATCHK
    /* Validate header CRC (covers prot_ver through last item descriptor) */
    uint32_t hdr_payload_size = 24 + (s_res_hdr->itemcnt * sizeof(resmgr_item_desc_t));
    uint32_t chk = crc32_calc(&s_res_hdr->prot_ver, hdr_payload_size, 0);
    if (s_res_hdr->hdrcrc != chk) {
        LISA_LOGE(TAG, "hdr crc mismatch: expect 0x%08x, got 0x%08x",
                  s_res_hdr->hdrcrc, chk);
        s_res_hdr = NULL;
        return -1;
    }
#endif

    LISA_LOGI(TAG, "init ok @ %p, items=%u", s_res_hdr, s_res_hdr->itemcnt);
    return 0;
}

void *resmgr_get_item(uint32_t resid, uint32_t *psize)
{
    if (!s_res_hdr || resid >= s_res_hdr->itemcnt) {
        LISA_LOGE(TAG, "invalid resid(%u), hdr=%p, cnt=%u",
                  resid, s_res_hdr,
                  s_res_hdr ? s_res_hdr->itemcnt : 0);
        return NULL;
    }

    const resmgr_item_desc_t *item = &s_res_hdr->items[resid];
    if (psize) {
        *psize = item->size;
    }

    /* item->addr is offset from the resource header base */
    return (uint8_t *)s_res_hdr + item->addr;
}
