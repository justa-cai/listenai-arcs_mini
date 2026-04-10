#ifndef __RESMGR_H__
#define __RESMGR_H__

#include <stdint.h>

// RESOUCE PROTOCOL VER1.1 FORMAT:
// |===============================================================|
// |  00 01 02 03 |  04 05 06 07  |   08 09 0A 0B  |   0C 0D 0E 0F |
// |---------------------------------------------------------------|
// |   RES_TAG    |  HDR_CRC32    |    PROT_VER    |   DATE_TIME   |
// |   ITEM_CNT   |   FW_ADDR     |     FW_LEN     |   USER_ID     |
// |   ITEM1.ID   |  ITEM1.ADDR   |    ITEM1.LEN   |   ITEM1.CHK   |
// |   ITEM2.ID   |  ITEM2.ADDR   |    ITEM2.LEN   |   ITEM2.CHK   |
// |   .........  |  .........    |    .........   |   .........   |
// |   ITEMx.ID   |  ITEMx.ADDR   |    ITEMx.LEN   |   ITEMx.CHK   |
// |...............................................................|
// |......................... ITEMs DATA ..........................|
// |...............................................................|
// |===============================================================|

typedef struct {
    uint32_t tagid;
    uint32_t addr;
    uint32_t size;
    uint32_t crc32;
} resmgr_item_desc_t;

typedef struct {
    uint32_t hdrtag;
    uint32_t hdrcrc;
    struct { uint16_t minor, major; } prot_ver;
    struct {
        uint32_t sec   : 6;
        uint32_t min   : 6;
        uint32_t hour  : 5;
        uint32_t day   : 5;
        uint32_t month : 4;
        uint32_t year  : 6;
    } date_time;
    uint32_t itemcnt;
    uint32_t fwaddr;
    uint32_t fwlen;
    uint32_t userid;
    resmgr_item_desc_t items[0];
} const *resmgr_hdr_t;

/* Resource IDs (must match respack tool output order and toy-scanpen-ap resmgr.h) */
typedef enum {
    RES_CV_CONFIG = 0,
    RES_CV_CUTLINE,

    RES_TRANS_TRANDB,
    RES_TRANS_DICT_TRAD,

    RES_XTTS_CNCN_YILIN_ROLE,
    RES_XTTS_CNCN_LINGXIAOQI_ROLE,
    RES_XTTS_CNCN_FRONT_LISTENAI_MAIN_ARM,
    RES_XTTS_ENGAM_LUCY_LOW_ARM,
    RES_XTTS_ENGAM_FRONT_COMMON_MAIN,
    RES_XTTS_ENUS_LIZZY_LOW_ARM,
    RES_XTTS_ENUS_FRONT_MAIN,

    RES_MAX,
} resmgr_resid_e;

/**
 * @brief Initialize resource manager from a fixed flash XIP address.
 *
 * @param flash_addr  Flash XIP base address of the resource header
 * @return 0 on success, -1 on failure (tag/crc mismatch)
 */
int resmgr_init(uint32_t flash_addr);

/**
 * @brief Get resource address and size by resource ID.
 *
 * @param resid  Resource ID (index into the item table)
 * @param psize  [out] Resource size in bytes (can be NULL)
 * @return Pointer to resource data in flash (XIP), or NULL on error
 */
void *resmgr_get_item(uint32_t resid, uint32_t *psize);

#endif /* __RESMGR_H__ */
