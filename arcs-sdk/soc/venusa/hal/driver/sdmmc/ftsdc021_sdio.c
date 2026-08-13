#include <time.h>
#include <sys/param.h>
#include "ftsdc021.h"
#include "ftsdc021_sdio.h"
//#include "castor.h"

/**
 * SDIO card related operations
 */
extern volatile ftsdc021_reg*
gpRegSDC(u8 ip_idx);
extern clock_t
sdc_jiffies(void);

static SDIO_Card_Feature SDIO_capability[8] = {
        { "SDC \0", 0 },
        { "SMB \0", 0 },
        { "SRW \0", 0 },
        { "SBS \0", 0 },
        { "S4MI\0", 0 },
        { "E4MI\0", 0 },
        { "LSC \0", 0 },
        { "4BLS\0", 0 }
};

static SDIO_Card_Feature SDIO_bus_interface_control[8] = {
        { "Bus width \0", 0 },
        { "Bus width \0", 0 },
        { "RFU 	    \0", 0 },
        { "RFU 	    \0", 0 },
        { "RFU 	    \0", 0 },
        { "ECSI      \0", 0 },
        { "SCSI      \0", 0 },
        { "CD Disable\0", 0 },
};

u32
ftsdc021_sdio_CMD52(u8 ip_idx, u32 write, u8 fn, u32 addr, u8 in, u8* out)
{
    u32 arg;
    u32 err;
    SDCardInfo* card = SDHost[ip_idx].Card;
    sd_mutex_lock(ip_idx);
    arg = write ? 0x80000000 : 0x00000000;
    arg |= fn << 28;
    arg |= (write && out) ? 0x08000000 : 0x00000000;
    arg |= addr << 9;
    arg |= in;
    sdc_dbg_print("CMD 52...write:%d; fn:%d; addr:%x; in:%d\n", write, fn, addr, in);
    /* CMD 52 Response type R5 */
    err = ftsdc021_send_command(ip_idx, SDHCI_IO_RW_DIRECT,
            SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 1, arg);
    if (err) {
        sd_mutex_unlock(ip_idx);
        CLOGE("%s send command fail\n", __func__);
        return err;
    }

    if (card->respLo & (R5_ERROR | R5_FUNCTION_NUMBER | R5_OUT_OF_RANGE)) {
        sdc_dbg_print("SDIO: IO_RW_DIRECT response indicate error 0x%llx.\n",
                card->respLo);
        sd_mutex_unlock(ip_idx);
        return 1;
    }

    if (out)
        *out = card->respLo & 0xFF;

    sd_mutex_unlock(ip_idx);
    return 0;
}

u32
ftsdc021_sdio_CMD53(u8 ip_idx, u32 write, u8 fn, u32 addr, u32 incr_addr,
        u32* buf,
        u32 blocks, u32 blksz)
{
    u32 arg, multi_blk = 0, blk_cnt_en = 0;
    SDCardInfo* card = SDHost[ip_idx].Card;
    sd_mutex_lock(ip_idx);
    arg = write ? 0x80000000 : 0x00000000;
    arg |= fn << 28;
    arg |= incr_addr ? 0x04000000 : 0x00000000;
    arg |= addr << 9;
    //if (blocks == 1 && blksz < 64)
    if (blocks == 1 && blksz <= 512) {
        arg |= (blksz == 512) ? 0 : blksz; /* byte mode */
        sdc_dbg_print_level(2, "%s [%d / %d] byte mode\n", __func__, blocks,
                blksz);
        multi_blk = 0;
        blk_cnt_en = 0;
    } else {
        arg |= (0x08000000 | blocks); /* block mode */
        sdc_dbg_print_level(2, "%s [%d / %d] block mode\n", __func__, blocks,
                blksz);
        blk_cnt_en = 1;
        multi_blk = 1;
    }

    ftsdc021_set_transfer_mode(ip_idx, blk_cnt_en, 0,
            write ? SDHCI_TXMODE_WRITE_DIRECTION : SDHCI_TXMODE_READ_DIRECTION,
            multi_blk);
    ftsdc021_prepare_data(ip_idx, blocks, blksz, (u32) buf, write ? WRITE : READ);

    /* CMD 53 */
    if (ftsdc021_send_command(ip_idx, SDHCI_IO_RW_EXTENDED,
            SDHCI_CMD_TYPE_NORMAL, 1, SDHCI_CMD_RTYPE_R1R5R6R7, 0, arg)) {
        sdc_dbg_print("SDIO: IO_RW_EXTENDED failed\n");
        sd_mutex_unlock(ip_idx);
        return 1;
    }

    if (ftsdc021_transfer_data(ip_idx, write ? WRITE : READ, buf,
            (blocks * blksz))) {
        sdc_dbg_print("%s transfer data fail\n", __func__);
        sd_mutex_unlock(ip_idx);
        return 1;
    }

    if (card->respLo & (R5_ERROR | R5_FUNCTION_NUMBER | R5_OUT_OF_RANGE)) {
        sdc_dbg_print("SDIO: IO_RW_EXTENDED response indicate error 0x%llx.\n",
                card->respLo);
        sd_mutex_unlock(ip_idx);
        return 1;
    }

    sd_mutex_unlock(ip_idx);
    return 0;
}
#ifdef SUPPORT_BUFFER_CHAINING
bool
ftsdc021_fill_adma_desc_table_chain_buffer(u8 ip_idx, struct pbuf* p)
{
    u8 act, tmp, adma2_random;
    u32 byte_cnt, i, ran, bytes_per_line;
    u8* buff;
    Adma2DescTable* ptr = (Adma2DescTable*) FTSDC021_SD_ADMA_BUF;
    Adma2DescTable* tptr;

    if (p->tot_len < 4) {
        sdc_dbg_print("Data less than 4 bytes !!\n");
        return 1;
    }

    adma2_random = SDHost[ip_idx].Card->FlowSet.adma2Rand;
    if (!SDHost[ip_idx].Card->FlowSet.lineBound)
    bytes_per_line = 65536;
    else
    bytes_per_line = SDHost[ip_idx].Card->FlowSet.lineBound;

    if (!adma2_random) {
        act = ADMA2_TRAN;
    }

    buff = (u8*) data_addr;
    i = 0;
    do {
        /* Random Mode = 1, we only random the length inside the descriptor */
        /* Random Mode = 2, we random the action and fix length inside the descriptor */
        /* Random Mode = 3, we random the action and length inside the descriptor */
        if ((adma2_random > 1) && (i < (ADMA2_NUM_OF_LINES - 2))) {
            ran = rand();
            /* Occupy percentage to prevent too many Noop and Reserved */
            tmp = ran & 0xF;
            if (tmp < 8)
            act = ADMA2_TRAN;
            else if (tmp < 13)
            act = ADMA2_LINK;
            else
            act = ADMA2_NOP;
        } else {
            act = ADMA2_TRAN;
        }

        tptr = ptr + i;
        memset(tptr, 0, sizeof(Adma2DescTable));

        switch (act) {
            case ADMA2_TRAN:
            if ((total_data > 256) && (i < (ADMA2_NUM_OF_LINES - 2))) {
                if (!adma2_random || (adma2_random == 2))
                /* Must be 4 bytes alignment */
                if (total_data < bytes_per_line)
                byte_cnt = total_data;
                else
                byte_cnt = bytes_per_line;
                else
                byte_cnt = (ran % total_data) & 0xfffc;
            } else {
                if (total_data > 0xFFFF) {
                    sdc_dbg_print(" ERR## ... Not enough descriptor to fill.\n");
                    tptr->attr |= ADMA2_ENTRY_END;
                    return 1;
                }
                byte_cnt = total_data;
            }

            if (byte_cnt < 4)   //bad result from randGen()
            byte_cnt = 4;

            tptr->addr = (u32) buff;
            tptr->attr = ADMA2_TRAN | ADMA2_ENTRY_VALID;
            tptr->lgth = byte_cnt;

            buff += byte_cnt;
            total_data -= byte_cnt;
            i++;

            break;
            case ADMA2_LINK:
            tmp = ran & 0x7;
            i += tmp;

            if (i > (ADMA2_NUM_OF_LINES - 2))
            i = ADMA2_NUM_OF_LINES - 2;

            tptr->addr = (u32)(ptr + i);
            tptr->attr = ADMA2_LINK | ADMA2_ENTRY_VALID;

            break;
            /* Do not execute this line, go to next line */
            case ADMA2_NOP:
            case ADMA2_SET:
            default:
            tptr->attr = ADMA2_NOP | ADMA2_ENTRY_VALID;
            i++;

            break;
        }
    }while (total_data > 0);

    if (SDHost[ip_idx].Card->FlowSet.adma2_insert_nop) {
        tptr = ptr + i;
        memset(tptr, 0, sizeof(Adma2DescTable));
    }
    tptr->attr |= (ADMA2_ENTRY_VALID | ADMA2_ENTRY_END | SDHost[ip_idx].Card->FlowSet.adma2_use_interrupt);

    return 0;
}
u32
ftsdc021_prepare_data_chain_buffer(u8 ip_idx, u32 blk_cnt, u16 blk_sz, struct pbuf* p, Transfer_Act act)
{
    u32 bound, length;
    struct pbuf* q;

    gpRegSDC(ip_idx)->BlkSize = blk_sz;

    gpRegSDC(ip_idx)->BlkCnt = blk_cnt;

    if (gpRegSDC(ip_idx)->TxMode & SDHCI_TXMODE_AUTOCMD23_EN)
    gpRegSDC(ip_idx)->SdmaAddr = blk_cnt;

    q = p;
    while (q != NULL) {
        gm_cpu_clean_dcache_range(q->payload, q->len);
        gm_cpu_dcache_invalidate_range(q->payload, q->len);
        q = p->next;
    }
    SDHost[ip_idx].Card->cmplMask = WAIT_TRANS_COMPLETE;
    if (SDHost[ip_idx].Card->FlowSet.UseDMA == ADMA) {
        if (ftsdc021_fill_adma_desc_table(ip_idx, (blk_cnt * blk_sz), buff_addr))
        return 1;
        gpRegSDC(ip_idx)->ADMAAddr = (u32) FTSDC021_SD_ADMA_BUF;

        if (SDHost[ip_idx].Card->FlowSet.adma2_use_interrupt)
        SDHost[ip_idx].Card->cmplMask |= WAIT_DMA_INTR;

    } else
    sdc_dbg_print("%s cannot set other transfer mode\n", __func__);

    return 0;
}

u32
ftsdc021_sdio_CMD53_with_chain_buffer(u8 ip_idx, u32 write, u8 fn, u32 addr, struct pbuf* p, u32 blocks, u32 blksz)
{
    u32 arg, multi_blk, blk_cnt_en;
    sd_mutex_lock(ip_idx);
    arg = write ? 0x80000000 : 0x00000000;
    arg |= fn << 28;
    arg |= 0x04000000;
    arg |= addr << 9;

    //if (blocks == 1 && blksz < 64)
    if (blocks == 1 && blksz <= 512) {
        //arg |= (blksz == 64) ? 0 : blksz; /* byte mode */
        //arg |= blksz;
        arg |= (blksz == 512) ? 0 : blksz; /* byte mode */
        sdc_dbg_print_level(2, "%s [%d / %d] byte mode\n", __func__, blocks, blksz);
    } else {
        arg |= 0x08000000 | blocks; /* block mode */
        sdc_dbg_print_level(2, "%s [%d / %d] block mode\n", __func__, blocks, blksz);
        blk_cnt_en = 1;
        multi_blk = 1;
    }

    ftsdc021_set_transfer_mode(ip_idx, blk_cnt_en, 0, write ? SDHCI_TXMODE_WRITE_DIRECTION : SDHCI_TXMODE_READ_DIRECTION, multi_blk);
    ftsdc021_prepare_data(ip_idx, blocks, blksz, (u32) buf, write ? WRITE : READ);

    /* CMD 53 */
    if (ftsdc021_send_command(ip_idx, SDHCI_IO_RW_EXTENDED, SDHCI_CMD_TYPE_NORMAL, 1, SDHCI_CMD_RTYPE_R1R5R6R7, 0, arg)) {
        sdc_dbg_print("SDIO: IO_RW_EXTENDED failed\n");
        sd_mutex_unlock(ip_idx);
        return 1;
    }

    if (ftsdc021_transfer_data(ip_idx, write ? WRITE : READ, buf, (blocks * blksz))) {
        sdc_dbg_print("%s transfer data fail\n", __func__);
        sd_mutex_unlock(ip_idx);
        return 1;
    }

    if (card->respLo & (R5_ERROR | R5_FUNCTION_NUMBER | R5_OUT_OF_RANGE)) {
        sdc_dbg_print("SDIO: IO_RW_EXTENDED response indicate error 0x%llx.\n", card->respLo);
        sd_mutex_unlock(ip_idx);
        return 1;
    }

    sd_mutex_unlock(ip_idx);
    return 0;
}
#endif

u32
ftsdc021_sdio_enable_wide(u8 ip_idx)
{
    u32 ret;
    u8 ctrl;

    if (!(gpRegSDC(ip_idx)->HCReg & SDHCI_HC_BUS_WIDTH_4BIT))
        return 0;

    if (SDHost[ip_idx].Card->cccr.low_speed
            && !SDHost[ip_idx].Card->cccr.wide_bus)
        return 0;

    ret = ftsdc021_sdio_CMD52(ip_idx, 0, 0, SDIO_CCCR_IF, 0, &ctrl);
    if (ret)
        return ret;

    ctrl |= SDIO_BUS_WIDTH_4BIT;

    ret = ftsdc021_sdio_CMD52(ip_idx, 1, 0, SDIO_CCCR_IF, ctrl, NULL);
    if (ret)
        return ret;

    return 1;
}

u32
ftsdc021_sdio_set_bus_width(u8 ip_idx, u32 width)
{
    u32 err;
    u8 ctrl = 0;

    err = ftsdc021_sdio_CMD52(ip_idx, 0, 0, SDIO_CCCR_IF, 0, &ctrl);
    if (err)
        return err;

    /* Set 4 bit */
    if (width == 4) {
        ctrl |= 0x2;
    }else if( width == 1)
    {
    	ctrl &= ~0x3;
    }


    err = ftsdc021_sdio_CMD52(ip_idx, 1, 0, SDIO_CCCR_IF, ctrl, NULL);
    if (err)
        return err;

    return 0;
}

u32
ftsdc021_sdio_enable_4bit_bus(u8 ip_idx)
{
    u32 err;

    if (SDHost[ip_idx].Card->CardType == SDIO_TYPE_CARD)
        return ftsdc021_sdio_enable_wide(ip_idx);

    if ((gpRegSDC(ip_idx)->HCReg & SDHCI_HC_BUS_WIDTH_4BIT) &&
            (SDHost[ip_idx].Card->SCR.SD_BUS_WIDTHS)) {
        err = ftsdc021_ops_app_set_bus_width(ip_idx, 4);
        if (err)
            return err;
    } else
        return 0;

    err = ftsdc021_sdio_enable_wide(ip_idx);
    if (err <= 0)
        ftsdc021_ops_app_set_bus_width(ip_idx, 1);

    return err;
}

static u32
ftsdc021_sdio_switch_hs(u8 ip_idx, u32 enable)
{
    int ret;
    u8 speed;

    if (!(gpRegSDC(ip_idx)->HCReg & SDHCI_HC_HI_SPEED))
        return 0;

    if (!SDHost[ip_idx].Card->cccr.high_speed)
        return 0;

    ret = ftsdc021_sdio_CMD52(ip_idx, 0, 0, SDIO_CCCR_SPEED, 0, &speed);
    if (ret)
        return ret;

    if (enable)
        speed |= SDIO_SPEED_EHS;
    else
        speed &= ~SDIO_SPEED_EHS;

    ret = ftsdc021_sdio_CMD52(ip_idx, 1, 0, SDIO_CCCR_SPEED, speed, NULL);
    if (ret)
        return ret;

    return 1;
}

u32
ftsdc021_sdio_enable_hs(u8 ip_idx)
{
    u32 ret;

    ret = ftsdc021_sdio_switch_hs(ip_idx, 1);
    if (ret <= 0 || SDHost[ip_idx].Card->CardType == SDIO_TYPE_CARD)
        return ret;

    return ret;
}

u32
ftsdc021_sdio_set_bus_speed(u8 ip_idx, u8 speed)
{
    u32 err;
    u8 ctrl = 0;

    err = ftsdc021_sdio_CMD52(ip_idx, 0, 0, SDIO_CCCR_SPEED, 0, &ctrl);
    //sdc_dbg_print("%s sdio support speed =%d\n",__func__,ctrl);
    if (err)
        return err;

    /* Bit 0 Support High Speed, Bit 1: Enable High Speed */
    if (speed == 1) {
        if (!(ctrl & 0x1)) {
            sdc_dbg_print("ERR:## ... Card does not supprot High speed.\n");
            return 1;
        }
//sdc_dbg_print("\n\n\nERIC add\n\n\n");
        ctrl |= 0x2;
    } else
        ctrl &= ~0x2;

    err = ftsdc021_sdio_CMD52(ip_idx, 1, 0, SDIO_CCCR_SPEED, ctrl, NULL);
    if (err)
        return err;

    return 0;
}

u32
ftsdc021_sdio_set_func_block_size(u8 ip_idx, SDIO_FUNC* func, u32 blksz)
{
    u32 err;

    if (blksz == 0) {
        blksz = MIN(func->max_blksize, 512);
    }
    sdc_dbg_print("<%d> blks = %d\n", ip_idx, blksz);
    //sdc_dbg_print("sdio num=%d\n",func->num);
    err = ftsdc021_sdio_CMD52(ip_idx, 1, 0,
            SDIO_FBR_BASE(func->num) + SDIO_FBR_BLKSIZE, blksz & 0xff, NULL);

    if (err)
        return err;
    err = ftsdc021_sdio_CMD52(ip_idx, 1, 0,
            SDIO_FBR_BASE(func->num) + SDIO_FBR_BLKSIZE + 1,
            (blksz >> 8) & 0xff, NULL);
    if (err)
        return err;
    //sdc_dbg_print("%s\n",__func__);
    func->cur_blksize = blksz;
    return 0;
}

static u32
ftsdc021_sdio_read_fbr(u8 ip_idx, SDIO_FUNC* func)
{
    u32 ret;
    u8 data;

    ret = ftsdc021_sdio_CMD52(ip_idx, 0, 0,
    SDIO_FBR_BASE(func->num) + SDIO_FBR_STD_IF, 0, &data);
    if (ret)
        return 1;

    data &= 0x0f;

    if (data == 0x0f) {
        ret = ftsdc021_sdio_CMD52(ip_idx, 0, 0,
        SDIO_FBR_BASE(func->num) + SDIO_FBR_STD_IF_EXT, 0, &data);
        if (ret)
            return 1;
    }

    func->class = data;
    return 0;
}

u32
ftsdc021_sdio_init_func(u8 ip_idx, SDIO_FUNC* func)
{
    int ret;

    sdc_dbg_print("%s init func [%d]\n", __func__, func->num);

#ifndef FTSDC021_NON_STD_SDIO
    {
        ret = ftsdc021_sdio_read_fbr(ip_idx, func);
        if (ret)
            goto fail;

        ret = ftsdc021_sdio_read_func_cis(ip_idx, func);
        if (ret)
            goto fail;

        func->enable = 1;
        sdc_dbg_print("init func[%d]\n", func->num);
        sdc_dbg_print("vendor = %x,  device = %x\n", func->vendor,
                func->device);
        sdc_dbg_print("max_blksize = %d, cur_blksize=%d\n", func->max_blksize,
                func->cur_blksize);
        sdc_dbg_print("enable_timeout = %d, state = %d\n", func->enable_timeout,
                func->state);
        sdc_dbg_print("num_info = %d, class = %d \n", func->num_info,
                func->class);
    }
#else
    {
        func->vendor = SDHost[ip_idx].Card->cis.vendor;
        func->device = SDHost[ip_idx].Card->cis.device;
        func->max_blksize = SDHost[ip_idx].Card->card->cis.blksize;
        func->enable = 1;
    }
#endif
    return 0;

    fail:
    /*
     * It is okay to remove the function here even though we hold
     * the host lock as we haven't registered the device yet.
     */
    func->enable = 0;
    return ret;
}

u32
ftsdc021_sdio_enable_func(u8 ip_idx, u32 func_num)
{
    u32 err;
    u8 reg;
    u32 t0;

    sdc_dbg_print("SDIO: Enabling device %d ...\n", func_num);

    err = ftsdc021_sdio_CMD52(ip_idx, 0, 0, SDIO_CCCR_IOEx, 0, &reg);
    if (err) {
        sdc_dbg_print("SDIO: Failed to enable device \n");
        return err;
    }

    reg |= 1 << func_num;

    err = ftsdc021_sdio_CMD52(ip_idx, 1, 0, SDIO_CCCR_IOEx, reg, NULL);
    if (err) {
        sdc_dbg_print("SDIO: Failed to enable device \n");
        return err;
    }

    t0 = sdc_jiffies();

    while (1) {
        err = ftsdc021_sdio_CMD52(ip_idx, 0, 0, SDIO_CCCR_IORx, 0, &reg);
        if (err) {
            sdc_dbg_print("SDIO: Failed to enable device \n");
            return err;
        }
        if (reg & 1 << func_num)
            break;
        if (sdc_jiffies() - t0 > SDHost[ip_idx].Card->FlowSet.timeout_ms) {
            sdc_dbg_print("SDIO: Failed to enable device \n");
            return 1;
        }
    }

    sdc_dbg_print("SDIO: Enabled device \n");

    return 0;
}

u32
ftsdc021_sdio_disable_func(u8 ip_idx, u32 func_num)
{
    s32 err;
    u8 reg;

    sdc_dbg_print("SDIO: Disabling device %d...\n", func_num);

    err = ftsdc021_sdio_CMD52(ip_idx, 0, 0, SDIO_CCCR_IOEx, 0, &reg);
    if (err) {
        sdc_dbg_print("SDIO: Failed to disable device %d\n", func_num);
        return err;
    }

    reg &= ~(1 << func_num);

    err = ftsdc021_sdio_CMD52(ip_idx, 1, 0, SDIO_CCCR_IOEx, reg, NULL);
    if (err) {
        sdc_dbg_print("SDIO: Failed to disable device %d\n", func_num);
        return err;
    }

    sdc_dbg_print("SDIO: Disabled device %d\n", func_num);

    return 0;
}

u32
ftsdc021_sdio_claim_irq_func(u8 ip_idx, u32 func_num)
{
    u32 err;
    u8 reg;
    err = ftsdc021_sdio_CMD52(ip_idx, 0, 0, SDIO_CCCR_IENx, 0, &reg);
    if (err)
        return err;

    reg |= 1 << func_num;

    reg |= 1; /* Master interrupt enable */

    err = ftsdc021_sdio_CMD52(ip_idx, 1, 0, SDIO_CCCR_IENx, reg, NULL);
    if (err)
        return err;
    return 0;
}

u32
ftsdc021_sdio_release_irq_func(u8 ip_idx, u32 func_num)
{
    u32 err;
    u8 reg;
    err = ftsdc021_sdio_CMD52(ip_idx, 0, 0, SDIO_CCCR_IENx, 0, &reg);
    if (err)
        return err;

    reg &= ~(1 << func_num);

    /* Disable master interrupt with the last function interrupt */
    if (!(reg & 0xFE))
        reg = 0;

    err = ftsdc021_sdio_CMD52(ip_idx, 1, 0, SDIO_CCCR_IENx, reg, NULL);
    if (err)
        return err;
    return 0;
}

u32
ftsdc021_sdio_read_cccr(u8 ip_idx, u32 ocr)
{
    u32 ret;
    u32 cccr_vsn;
    u32 uhs = ocr & R4_18V_PRESENT;
    u8 data;
    u8 speed;

    memset(&SDHost[ip_idx].Card->cccr, 0, sizeof(SDIO_CCCR));

    ret = ftsdc021_sdio_CMD52(ip_idx, 0, 0, SDIO_CCCR_CCCR, 0, &data);
    if (ret)
        goto out;

    cccr_vsn = data & 0x0f;

    if (cccr_vsn > SDIO_CCCR_REV_3_00) {
        sdc_dbg_print("SDIO: unrecognised CCCR structure version %d\n",
                cccr_vsn);
        return 1;
    }

    SDHost[ip_idx].Card->cccr.sdio_vsn = (data & 0xf0) >> 4;

    ret = ftsdc021_sdio_CMD52(ip_idx, 0, 0, SDIO_CCCR_CAPS, 0, &data);
    if (ret)
        goto out;

    if (data & SDIO_CCCR_CAP_SMB)
        SDHost[ip_idx].Card->cccr.multi_block = 1;
    if (data & SDIO_CCCR_CAP_LSC)
        SDHost[ip_idx].Card->cccr.low_speed = 1;
    if (data & SDIO_CCCR_CAP_4BLS)
        SDHost[ip_idx].Card->cccr.wide_bus = 1;

    if (cccr_vsn >= SDIO_CCCR_REV_1_10) {
        ret = ftsdc021_sdio_CMD52(ip_idx, 0, 0, SDIO_CCCR_POWER, 0, &data);
        if (ret)
            goto out;

        if (data & SDIO_POWER_SMPC)
            SDHost[ip_idx].Card->cccr.high_power = 1;
    }

    if (cccr_vsn >= SDIO_CCCR_REV_1_20) {
        ret = ftsdc021_sdio_CMD52(ip_idx, 0, 0, SDIO_CCCR_SPEED, 0, &speed);
        if (ret)
            goto out;

        SDHost[ip_idx].Card->SCR.SD_SPEC3 = 0;
        SDHost[ip_idx].Card->sw_caps.sd3_bus_mode = 0;
        SDHost[ip_idx].Card->sw_caps.sd3_drv_type = 0;
        if (cccr_vsn >= SDIO_CCCR_REV_3_00 && uhs) {
            SDHost[ip_idx].Card->SCR.SD_SPEC3 = 1;
            ret = ftsdc021_sdio_CMD52(ip_idx, 0, 0,
            SDIO_CCCR_UHS, 0, &data);
            if (ret)
                goto out;

			//lyt, important but maybe not error;
            if (gpRegSDC(ip_idx)->HostCtrl2 &
                    (SDHCI_SDR12 | SDHCI_SDR25 |
                    SDHCI_SDR50 | SDHCI_SDR104 |
                    SDHCI_DDR50)) {
                if (data & SDIO_UHS_DDR50)
                    SDHost[ip_idx].Card->sw_caps.sd3_bus_mode
                    |= SD_MODE_UHS_DDR50;

                if (data & SDIO_UHS_SDR50)
                    SDHost[ip_idx].Card->sw_caps.sd3_bus_mode
                    |= SD_MODE_UHS_SDR50;

                if (data & SDIO_UHS_SDR104)
                    SDHost[ip_idx].Card->sw_caps.sd3_bus_mode
                    |= SD_MODE_UHS_SDR104;
            }

            ret = ftsdc021_sdio_CMD52(ip_idx, 0, 0,
            SDIO_CCCR_DRIVE_STRENGTH, 0, &data);
            if (ret)
                goto out;

            if (data & SDIO_DRIVE_SDTA)
                SDHost[ip_idx].Card->sw_caps.sd3_drv_type |= SD_DRIVER_TYPE_A;
            if (data & SDIO_DRIVE_SDTC)
                SDHost[ip_idx].Card->sw_caps.sd3_drv_type |= SD_DRIVER_TYPE_C;
            if (data & SDIO_DRIVE_SDTD)
                SDHost[ip_idx].Card->sw_caps.sd3_drv_type |= SD_DRIVER_TYPE_D;
        }

        /* if no uhs mode ensure we check for high speed */
        if (!SDHost[ip_idx].Card->sw_caps.sd3_bus_mode) {
            if (speed & SDIO_SPEED_SHS) {
                SDHost[ip_idx].Card->cccr.high_speed = 1;
                SDHost[ip_idx].Card->sw_caps.hs_max_dtr = 50000000;
            } else {
                SDHost[ip_idx].Card->cccr.high_speed = 0;
                SDHost[ip_idx].Card->sw_caps.hs_max_dtr = 25000000;
            }
        }
    }
    sdc_dbg_print("SDIO CCCR sdio_vsn = %d, sd_vsn = %d, multi_block = %d\n",
            SDHost[ip_idx].Card->cccr.sdio_vsn,
            SDHost[ip_idx].Card->cccr.sd_vsn,
            SDHost[ip_idx].Card->cccr.multi_block);
    sdc_dbg_print("SDIO CCCR low_speed = %d, wide_bus = %d, high_power = %d\n",
            SDHost[ip_idx].Card->cccr.low_speed,
            SDHost[ip_idx].Card->cccr.wide_bus,
            SDHost[ip_idx].Card->cccr.high_power);
    sdc_dbg_print("SDIO CCCR high_speed = %d, disable_cd = %d\n",
            SDHost[ip_idx].Card->cccr.high_speed,
            SDHost[ip_idx].Card->cccr.disable_cd);

    out:
    return ret;
}

u32
ftsdc021_ops_Card_Info(u8 ip_idx)
{
    u8 resp = 0;
    u8 i = 0;
    //u32 *data_from_cmd53;
    u32 err;
#if 1
    /* CMD 52 */
    err = ftsdc021_sdio_CMD52(ip_idx, 0, 0, SDIO_FBR_BASE(0), 0, &resp);
    if (err) {
        sdc_dbg_print("SDIO RW DIRECT: address 0x100 failed.\n");
        return err;
    }

    switch (resp & 0xf) {
    case 0x0:
        sdc_dbg_print(
                "No SDIO standard interface supported by this function\n");
        break;

    case 0x1:
        sdc_dbg_print("SDIO Standard UART\n");
        break;

    case 0x2:
        sdc_dbg_print("SDIO Type-A for BT Standard\n");
        break;

    case 0x3:
        sdc_dbg_print("SDIO Type-B for BT Standard\n");
        break;

    case 0x4:
        sdc_dbg_print("SDIO GPS Standard\n");
        break;

    case 0x5:
        sdc_dbg_print("SDIO Camera Standard\n");
        break;

    case 0x6:
        sdc_dbg_print("SDIO PHS Standard\n");
        break;

    case 0x7:
        sdc_dbg_print("SDIO WLAN Standard\n");
        break;

    case 0x8:
        sdc_dbg_print("Embedded SDIO-ATA Standard\n");
        break;

    case 0xF:
        sdc_dbg_print("Externed SDIO Standard\n");
        break;
    default:
        break;
    }
#endif
    // Show the SDIO Card Capability from the address 0x08 in function 0.
    err = ftsdc021_sdio_CMD52(ip_idx, 0, 0, SDIO_CCCR_CAPS, 0, &resp);
    if (err) {
        sdc_dbg_print("SDIO RW DIRECT: address 0x8 failed.\n");
        return err;
    }

    sdc_dbg_print("sdio CCCR 08 = %02x\n", resp);
    i = 0;
    sdc_dbg_print("Card Capability:\n");
    do {
        sdc_dbg_print("%s:", SDIO_capability[i].name);
        if (resp & (1 << i)) {
            SDIO_capability[i].support = 1;
        } else {
            SDIO_capability[i].support = 0;
        }
        sdc_dbg_print("%d, ", SDIO_capability[i].support);
        i++;
    } while (i < 8);

    sdc_dbg_print("\n\n");

    // Show the SDIO Card Bus Interface Control from the address 0x07 in function 0.
    err = ftsdc021_sdio_CMD52(ip_idx, 0, 0, SDIO_CCCR_IF, 0, &resp);
    if (err) {
        sdc_dbg_print("SDIO RW DIRECT: address 0x7 failed.\n");
        return err;
    }

    i = 0;
    sdc_dbg_print("Bus Interface Control:\n");
    do {
        sdc_dbg_print("%s:", SDIO_bus_interface_control[i].name);

        /* Bus width is located at [1:0] 2 bits */
        if (i == 0) {
            SDIO_bus_interface_control[0].support = 1 << (resp & 0x3);
            i += 5;
        } else {
            if (resp & (1 << i))
                SDIO_bus_interface_control[i].support = 1;
            else
                SDIO_bus_interface_control[i].support = 0;

            i++;
        }

        sdc_dbg_print("%d, ", SDIO_bus_interface_control[0].support);
    } while (i < 8);

    sdc_dbg_print("\n");

    // Fetching the CIS pointer in CCCR
    /*  SDC_send_command(SDC_IO_RW_DIRECT, SDC_CMD_TYPE_NORMAL, 0,
     SDHCI_CMD_RTYPE_R1R5R6R7, 1,
     ((SD_CMD52_RW_in_R)|(SD_CMD52_no_RAW)|
     SD_CMD52_FUNC(0)|SD_CMD52_Reg_Addr(0x09)));

     temp_resp = (u32)FTSDC020Reg->CmdRespLo;
     temp_resp = temp_resp & 0xFF;
     sdc_dbg_print("%x ",temp_resp);


     SDC_send_command(SDC_IO_RW_DIRECT, SDC_CMD_TYPE_NORMAL, 0,
     SDHCI_CMD_RTYPE_R1R5R6R7, 1,
     ((SD_CMD52_RW_in_R)|(SD_CMD52_no_RAW)|
     SD_CMD52_FUNC(0)|SD_CMD52_Reg_Addr(0x0A)));

     temp_resp = (u32)FTSDC020Reg->CmdRespLo;
     temp_resp = temp_resp & 0xFF;
     sdc_dbg_print("%x ",temp_resp);

     SDC_send_command(SDC_IO_RW_DIRECT, SDC_CMD_TYPE_NORMAL, 0,
     SDHCI_CMD_RTYPE_R1R5R6R7, 1,
     ((SD_CMD52_RW_in_R)|(SD_CMD52_no_RAW)|
     SD_CMD52_FUNC(0)|SD_CMD52_Reg_Addr(0x0B)));

     temp_resp = (u32)FTSDC020Reg->CmdRespLo;
     temp_resp = temp_resp & 0xFF;
     sdc_dbg_print("%x ",temp_resp);

     sdc_dbg_print("\n");*/

    // Setting the Function Enable bit in CCCR for Function 1.
    /*  SDC_send_command(SDC_IO_RW_DIRECT, SDC_CMD_TYPE_NORMAL, 0,
     SDHCI_CMD_RTYPE_R1R5R6R7, 1,
     ((SD_CMD52_RW_in_W)|(SD_CMD52_no_RAW)|
     SD_CMD52_FUNC(0)|SD_CMD52_Reg_Addr(0x02)
     |0x02));
     do{
     SDC_send_command(SDC_IO_RW_DIRECT, SDC_CMD_TYPE_NORMAL, 0,
     SDHCI_CMD_RTYPE_R1R5R6R7, 1,
     ((SD_CMD52_RW_in_R)|(SD_CMD52_no_RAW)|
     SD_CMD52_FUNC(0)|SD_CMD52_Reg_Addr(0x03)));

     temp_resp = (u32)FTSDC020Reg->CmdRespLo;
     }while(temp_resp & 0x02 != 0x02);

     */
    // Show the SDIO Card CCCR/SDIO and Specification Revision from 0x00-0x19 in function 0.
    /*  SDC_send_command(SDC_IO_RW_DIRECT, SDC_CMD_TYPE_NORMAL, 0,
     SDHCI_CMD_RTYPE_R1R5R6R7, 1,
     ((SD_CMD52_RW_in_W)|(SD_CMD52_no_RAW)|
     SD_CMD52_FUNC(0)|SD_CMD52_Reg_Addr(0x10) | 0x40));
     SDC_send_command(SDC_IO_RW_DIRECT, SDC_CMD_TYPE_NORMAL, 0,
     SDHCI_CMD_RTYPE_R1R5R6R7, 1,
     ((SD_CMD52_RW_in_W)|(SD_CMD52_no_RAW)|
     SD_CMD52_FUNC(0)|SD_CMD52_Reg_Addr(0x11) | 0x00));
     */
    // Fetching the multiple bytes from beginning 64bytes of CCCR through CMD52 for comparing.
    sdc_dbg_print("Data from CMD52:\n");
    sdc_dbg_print("0x00 0x01 0x02 0x03\n");
    sdc_dbg_print("====================\n");
    for (i = 0; i < 64; i++) {
        err = ftsdc021_sdio_CMD52(ip_idx, 0, 0, (0x00 + i), 0, &resp);
        if (err) {
            sdc_dbg_print("SDIO RW DIRECT: address 0x%x failed.\n", i);
            return err;
        }

        sdc_dbg_print("0x%02x ", resp);

        if (i % 4 == 3) {
            sdc_dbg_print("\n");
        }
    }
    sdc_dbg_print("\n");

    /* CMD52 : Setting the Blocksize(64bytes) for card if SMB feature in card capability is support. */
    /*  SDC_send_command(SDC_IO_RW_DIRECT, SDC_CMD_TYPE_NORMAL, 0,
     SDHCI_CMD_RTYPE_R1R5R6R7, 1,
     ((SD_CMD52_RW_in_W)|(SD_CMD52_RAW)|
     SD_CMD52_FUNC(0)|SD_CMD52_Reg_Addr(0x10) | 0x40));*/

    // Fetching the multiple bytes from beginning 64bytes of CCCR through CMD53.
    return 1;
}
