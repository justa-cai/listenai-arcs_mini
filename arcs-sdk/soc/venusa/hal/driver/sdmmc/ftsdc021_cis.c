#include <string.h>
//#include <lib_mm.h>
#include "ftsdc021.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)           (sizeof ((x)) / sizeof(*(x)))
#endif
static const u8 speed_val[16] =
        { 0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80 };
static const u32 speed_unit[8] =
        { 10000, 100000, 1000000, 10000000, 0, 0, 0, 0 };

typedef u32
(tpl_parse_t)(u8, SDIO_FUNC* func, const s8*, u32);

struct cis_tpl
{
    u8 code;
    u8 min_size;
    tpl_parse_t* parse;
};

static struct cis_tpl cis_tpl_list[4];
//= {
//{ 0x15,   3,  cistpl_vers_1       },
//{ 0x20,   4,  cistpl_manfid       },
//{ 0x21,   2,  /* cistpl_funcid */ },
//{ 0x22,   0,  cistpl_funce        },
//};

static struct cis_tpl cis_tpl_funce_list[3]; // = {
//{ 0x00,   4,  cistpl_funce_common     },
/// {   0x01,   0,  cistpl_funce_func       },
//  {   0x04,   1+1+6,  /* CISTPL_FUNCE_LAN_NODE_ID */  },

static u32
cistpl_vers_1(u8 ip_idx, SDIO_FUNC* func,
        const s8* buf, u32 size)
{
    u32 i, nr_strings;
    s8** buffer, *string;

    /* Find all null-terminated (including zero length) strings in
     the TPLLV1_INFO field. Trailing garbage is ignored. */
    buf += 2;
    size -= 2;

    nr_strings = 0;
    for (i = 0; i < size; i++) {
        if (buf[i] == 0xff)
            break;
        if (buf[i] == 0)
            nr_strings++;
    }
    if (nr_strings == 0)
        return 0;

    size = i;

#ifdef CFG_RTOS
    buffer = pvPortMalloc(sizeof(s8*) * nr_strings + size);
#else
    buffer = malloc(sizeof(s8*) * nr_strings + size);
#endif
    if (!buffer)
        return 1;

    string = (s8*) (buffer + nr_strings);

    for (i = 0; i < nr_strings; i++) {
        buffer[i] = string;
        strcpy(string, buf);
        string += strlen(string) + 1;
        buf += strlen(buf) + 1;
    }

    sdc_dbg_print("%s num_info = %d\n", __func__, nr_strings);
    if (func == NULL) {
        SDHost[ip_idx].Card->num_info = nr_strings;
        SDHost[ip_idx].Card->info = (s8**) buffer;
    } else {
        func->num_info = nr_strings;
        func->info = (const s8**)buffer;
    }

    return 0;
}

static u32
cistpl_manfid(u8 ip_idx, SDIO_FUNC* func, const s8* buf, u32 size)
{
    u32 vendor, device;

    /* TPLMID_MANF */
    vendor = buf[0] | (buf[1] << 8);

    /* TPLMID_CARD */
    device = buf[2] | (buf[3] << 8);

    if (func == NULL) {
        SDHost[ip_idx].Card->cis.vendor = vendor;
        SDHost[ip_idx].Card->cis.device = device;
    } else {
        func->vendor = vendor;
        func->device = device;
    }

    return 0;
}

static u32
cis_tpl_parse(u8 ip_idx, SDIO_FUNC* func,
        const s8* tpl_descr,
        const struct cis_tpl* tpl, s32 tpl_count,
        u8 code,
        const s8* buf, u32 size)
{
    u32 i, ret;

    /* look for a matching code in the table */
    for (i = 0; i < tpl_count; i++, tpl++) {
        if (tpl->code == code)
            break;
    }
    if (i < tpl_count) {
        if (size >= tpl->min_size) {
            if (tpl->parse)
                ret = tpl->parse(ip_idx, func, buf, size);
            else
                ret = 1; /* known tuple, not parsed */
        } else {
            /* invalid tuple */
            ret = 1;
        }
        if (ret) {
            sdc_dbg_print("SDIO: bad %s tuple 0x%02x (%u bytes)\n", tpl_descr, code, size);
        }
    } else {
        /* unknown tuple */
        ret = 1;
    }

    return ret;
}

static u32
cistpl_funce_common(u8 ip_idx, SDIO_FUNC* func, const s8* buf, u32 size)
{
    /* Only valid for the common CIS (function 0) */
    if (func != NULL)
        return 1;

    /* TPLFE_FN0_BLK_SIZE */
    SDHost[ip_idx].Card->cis.blksize = buf[1] | (buf[2] << 8);

    /* TPLFE_MAX_TRAN_SPEED */
    SDHost[ip_idx].Card->cis.max_dtr = speed_val[(buf[3] >> 3) & 15] *
            speed_unit[buf[3] & 7];

    return 0;
}

static u32
cistpl_funce_func(u8 ip_idx, SDIO_FUNC* func, const s8* buf, u32 size)
{
    u32 vsn = 0;
    u32 min_size;

    /* Only valid for the individual function's CIS (1-7) */
    if (func == NULL)
        return 1;

    sdc_dbg_print("%s\n", __func__);
    /*
     * This tuple has a different length depending on the SDIO spec
     * version.
     */
    vsn = SDHost[ip_idx].Card->cccr.sdio_vsn;
    min_size = (vsn == SDIO_SDIO_REV_1_00) ? 28 : 42;

    if (size < min_size)
        return 1;

    /* TPLFE_MAX_BLK_SIZE */
    func->max_blksize = buf[12] | (buf[13] << 8);
    func->cur_blksize = func->max_blksize;

    sdc_dbg_print("max blk size= %d\n", func->max_blksize);
    /* TPLFE_ENABLE_TIMEOUT_VAL, present in ver 1.1 and above */
    if (vsn > SDIO_SDIO_REV_1_00)
        func->enable_timeout = (buf[28] | (buf[29] << 8)) * 10;
    else
        func->enable_timeout = 10; //jiffies_to_msecs(HZ);

    return 0;
}

static u32
cistpl_funce(u8 ip_idx, SDIO_FUNC* func, const s8* buf, u32 size)
{
    if (size < 1)
        return 1;
    sdc_dbg_print("%s\n", __func__);
    return cis_tpl_parse(ip_idx, func, "CISTPL_FUNCE",
            cis_tpl_funce_list,
            ARRAY_SIZE(cis_tpl_funce_list),
            buf[0], buf, size);
}

u32
ftsdc021_sdio_cis_init(void)
{
    cis_tpl_list[0].code = 0x15;
    cis_tpl_list[0].min_size = 3;
    cis_tpl_list[0].parse = cistpl_vers_1;
    cis_tpl_list[1].code = 0x20;
    cis_tpl_list[1].min_size = 4;
    cis_tpl_list[1].parse = cistpl_manfid;
    cis_tpl_list[2].code = 0x21;
    cis_tpl_list[2].min_size = 2;
    cis_tpl_list[2].parse = NULL;
    cis_tpl_list[3].code = 0x22;
    cis_tpl_list[3].min_size = 0;
    cis_tpl_list[3].parse = cistpl_funce;

    cis_tpl_funce_list[0].code = 0x00;
    cis_tpl_funce_list[0].min_size = 4;
    cis_tpl_funce_list[0].parse = cistpl_funce_common;
    cis_tpl_funce_list[1].code = 0x01;
    cis_tpl_funce_list[1].min_size = 0;
    cis_tpl_funce_list[1].parse = cistpl_funce_func;
    return 0;
}

static u32
ftsdc021_sdio_read_cis(u8 ip_idx, SDIO_FUNC* func)
{
    u32 ret;
    struct sdio_func_tuple* this, **prev;
    u32 i, ptr = 0;

    /*
     * Note that this works for the common CIS (function number 0) as
     * well as a function's CIS * since SDIO_CCCR_CIS and SDIO_FBR_CIS
     * have the same offset.
     */
    for (i = 0; i < 3; i++) {
        u8 x, fn;

        if (func != NULL)
            fn = func->num;
        else
            fn = 0;

        ret = ftsdc021_sdio_CMD52(ip_idx, 0, 0,
        SDIO_FBR_BASE(fn) + SDIO_FBR_CIS + i, 0, &x);
        if (ret)
            return ret;
        ptr |= x << (i * 8);
    }
    if (func)
        prev = &func->tuples;
    else
        prev = &SDHost[ip_idx].Card->tuples;

    do {
        u8 tpl_code, tpl_link;

        ret = ftsdc021_sdio_CMD52(ip_idx, 0, 0, ptr++, 0, &tpl_code);
        if (ret)
            break;

        /* 0xff means we're done */
        if (tpl_code == 0xff)
            break;

        /* null entries have no link field or data */
        if (tpl_code == 0x00)
            continue;

        ret = ftsdc021_sdio_CMD52(ip_idx, 0, 0, ptr++, 0, &tpl_link);
        if (ret)
            break;

        /* a size of 0xff also means we're done */
        if (tpl_link == 0xff)
            break;

#ifdef CFG_RTOS
        this = pvPortMalloc(sizeof(*this) + tpl_link);
#else
        this = malloc(sizeof(*this) + tpl_link);
#endif
        if (!this)
            return 1;

        for (i = 0; i < tpl_link; i++) {
            ret = ftsdc021_sdio_CMD52(ip_idx, 0, 0,
                    ptr + i, 0, &this->data[i]);
            if (ret)
                break;
        }
        if (ret) {
#ifdef CFG_RTOS
            vPortFree(this);
#else
            free(this);
#endif
            break;
        }

        /* Try to parse the CIS tuple */
        ret = cis_tpl_parse(ip_idx, func, "CIS",
                cis_tpl_list, ARRAY_SIZE(cis_tpl_list),
                tpl_code, (s8*) this->data, tpl_link);
        if (ret) {
            /*
             * The tuple is unknown or known but not parsed.
             * Queue the tuple for the function driver.
             */
            this->next = NULL;
            this->code = tpl_code;
            this->size = tpl_link;
            *prev = this;
            prev = &this->next;
#if 0
            if (ret == -ENOENT) {
                /* warn about unknown tuples */
                pr_warning("%s: queuing unknown"
                        " CIS tuple 0x%02x (%u bytes)\n",
                        mmc_hostname(card->host),
                        tpl_code, tpl_link);
            }

            /* keep on analyzing tuples */
#endif
            ret = 0;
        } else {
            /*
             * We don't need the tuple anymore if it was
             * successfully parsed by the SDIO core or if it is
             * not going to be queued for a driver.
             */
#ifdef CFG_RTOS
            vPortFree(this);
#else
            free(this);
#endif
        }

        ptr += tpl_link;
    } while (!ret);

    /*
     * Link in all unknown tuples found in the common CIS so that
     * drivers don't have to go digging in two places.
     */
    if (func)
        *prev = SDHost[ip_idx].Card->tuples;

    sdc_dbg_print("SDIO CIS vendor = %d, device=%d, blksize = %d, max_dtr=%d\n", SDHost[ip_idx].Card->cis.vendor,
            SDHost[ip_idx].Card->cis.device, SDHost[ip_idx].Card->cis.blksize, SDHost[ip_idx].Card->cis.max_dtr);
    return ret;
}

u32
ftsdc021_sdio_read_common_cis(u8 ip_idx)
{
    return ftsdc021_sdio_read_cis(ip_idx, NULL);
}

void
ftsdc021_sdio_free_common_cis(u8 ip_idx)
{
    struct sdio_func_tuple* tuple, *victim;

    tuple = SDHost[ip_idx].Card->tuples;

    while (tuple) {
        victim = tuple;
        tuple = tuple->next;
#ifdef CFG_RTOS
        vPortFree(victim);
#else
        free(victim);
#endif
    }

    SDHost[ip_idx].Card->tuples = NULL;
}

u32
ftsdc021_sdio_read_func_cis(u8 ip_idx, SDIO_FUNC* func)
{
    u32 ret;
    ret = ftsdc021_sdio_read_cis(ip_idx, func);
    if (ret)
        return ret;

    /* * Vendor/device id is optional for function CIS, so
     * copy it from the card structure as needed.
     */

    if (func->vendor == 0) {
        func->vendor = SDHost[ip_idx].Card->cis.vendor;
        func->device = SDHost[ip_idx].Card->cis.device;
    }

    return 0;
}

void
ftsdc021_sdio_free_func_cis(u8 ip_idx, SDIO_FUNC* func)
{
    struct sdio_func_tuple* tuple, *victim;

    tuple = func->tuples;

    while (tuple && tuple != SDHost[ip_idx].Card->tuples) {
        victim = tuple;
        tuple = tuple->next;
#ifdef CFG_RTOS
        vPortFree(victim);
#else
        free(victim);
#endif
    }

    func->tuples = NULL;

}

