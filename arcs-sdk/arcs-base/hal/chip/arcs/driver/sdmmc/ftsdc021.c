#include <time.h>
#include "stdbool.h"
#include "ftsdc021.h"
#include "cache.h"
#include "arcs_ap.h"
#include "log_print.h"

#define FTSDC021_BASE_CLOCK  100

volatile ftsdc021_reg* gpRegSD0 = (ftsdc021_reg*) SDC_FTSDC021_0_PA_BASE;
//volatile ftsdc021_reg* gpRegSD1 = (ftsdc021_reg*) SDC_FTSDC021_1_PA_BASE;

//extern void
//gm_cpu_dcache_invalidate_range(u32 addr, u32 len);

static const u32 tran_exp[] = {
        10000, 100000, 1000000, 10000000,
        0, 0, 0, 0
};

static const u8 tran_mant[] = {
        0, 10, 12, 13, 15, 20, 25, 30,
        35, 40, 45, 50, 55, 60, 70, 80,
};

SDHostInfo SDHost[1];
//SDHostInfo *SDHost = (SDHostInfo *)0x20;

u32 sdc_debug_level = 0;


/* Internal functions */
//static u32 ftsdc021_autocmd_error_recovery(u8 ip_idx);
static u32
ftsdc021_ErrorRecovery(u8 ip_idx);

#define jiffies sdc_jiffies

clock_t
sdc_jiffies(void)
{
#ifdef CFG_RTOS
    return (clock_t) (xTaskGetTickCount() * portTICK_PERIOD_MS);
    //return gm_timer_get_tick(0, 0);
#else
    extern uint32_t SysTick_Value(void);
    return SysTick_Value();
#endif       
}

volatile ftsdc021_reg*
gpRegSDC(u8 ip_idx)
{
    switch (ip_idx) {
    case 0:
        return gpRegSD0;
    default:
        break;
    }
//  sdc_dbg_print_level(0,"no valid SD ip_idx");
    return NULL;
}
s32
sd_mutex_create(u8 ip_idx)
{
#ifdef CFG_RTOS    
    SDHost[ip_idx].sd_mutex = xSemaphoreCreateMutex();
//    SDHost[ip_idx].sd_mutex = xSemaphoreCreateRecursiveMutex(); //BSD:
    if (SDHost[ip_idx].sd_mutex == NULL) {
        sdc_dbg_print("%s : create mutex fail\n", __func__);
        return 1;
    }
#else
    static uint32_t faked_mutex = 0;
    SDHost[ip_idx].sd_mutex = (void*)(&faked_mutex);
#endif
    return 0;
}
s32
sd_mutex_lock(u8 ip_idx)
{
#ifdef CFG_RTOS
    //if (pdPASS == xSemaphoreTake(SDHost[ip_idx].sd_mutex, 5000)) { //portMAX_DELAY )){
    if (pdPASS == xSemaphoreTake(SDHost[ip_idx].sd_mutex, portMAX_DELAY )){ //5*configTICK_RATE_HZ //BSD:
        return 0;
    } else {
        sdc_dbg_print("%s : mutex lock fail\n", __func__);
        return 1;
    }
#else
    return 0;
#endif    
}
s32
sd_mutex_unlock(u8 ip_idx)
{
#ifdef CFG_RTOS
    //if (pdPASS == xSemaphoreGive(SDHost[ip_idx].sd_mutex)) {
    if (pdPASS == xSemaphoreGive(SDHost[ip_idx].sd_mutex)) { //BSD:
        return 0;
    } else {
        sdc_dbg_print("%s : mutex unlock fail\n", __func__);
        return 1;
    }
#else
    return 0;
#endif      
}

bool
ftsdc021_fill_adma_desc_table(u8 ip_idx, u32 total_data, u32 data_addr)
{
    u32 byte_cnt, i, bytes_per_line;
    u8* buff;
    Adma2DescTable* tptr;
    Adma2DescTable* ptr;

    ptr = (Adma2DescTable*)((u32)SDHost[ip_idx].Card->FlowSet.adma_buffer); //FTSDC021_SD_0_ADMA_BUF;

    if (total_data < 4) {
        sdc_dbg_print("Data less than 4 bytes !!\n");
        return 1;
    }

    if (!SDHost[ip_idx].Card->FlowSet.lineBound)
        bytes_per_line = 65536;
    else
        bytes_per_line = SDHost[ip_idx].Card->FlowSet.lineBound;

    buff = (u8*) data_addr;
    i = 0;
    do {

        tptr = ptr + i;
        memset(tptr, 0, sizeof(Adma2DescTable));

        if ((total_data > 256) && (i < (ADMA2_NUM_OF_LINES - 2))) {

            /* Must be 4 bytes alignment */
            if (total_data < bytes_per_line)
                byte_cnt = total_data;
            else
                byte_cnt = bytes_per_line;
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

    } while (total_data > 0);

    if (SDHost[ip_idx].Card->FlowSet.adma2_insert_nop) {
        tptr = ptr + i;
        memset(tptr, 0, sizeof(Adma2DescTable));
    }
    tptr->attr |= (ADMA2_ENTRY_VALID | ADMA2_ENTRY_END | SDHost[ip_idx].Card->FlowSet.adma2_use_interrupt);

//    nds32_dma_clean_range((uint32_t)ptr, (uint32_t)ptr + ADMA2_NUM_OF_LINES * 8);
    dcache_clean_range((uint32_t)ptr, (uint32_t)ptr + ADMA2_NUM_OF_LINES * 8);

    return 0;
}

void
ftsdc021_print_err_msg(u8 ip_idx, u32 errSts)
{
    if (errSts & SDHCI_INTR_ERR_ADMA) {
        /* Must do Abort DMA Operation */
        sdc_dbg_print(" INTR: ERR## ... ADMA Error: 0x%x.\n", gpRegSDC(ip_idx)->ADMAErrSts);
    }

    if (errSts & SDHCI_INTR_ERR_AutoCMD) {
        sdc_dbg_print(" INTR: ERR## ... Auto CMD Error.\n");
    }

    if (errSts & SDHCI_INTR_ERR_CURR_LIMIT) {
        sdc_dbg_print(" INTR: ERR## ... Current Limit.\n");
    }

    if (errSts & SDHCI_INTR_ERR_DATA_ENDBIT) {
        sdc_dbg_print(" INTR: ERR## ... Data End Bit.\n");
    }

    if (errSts & SDHCI_INTR_ERR_DATA_CRC) {
        sdc_dbg_print(" INTR: ERR## ... Data CRC.\n");
    }

    if (errSts & SDHCI_INTR_ERR_DATA_TIMEOUT) {
        sdc_dbg_print(" INTR: ERR## ... Data Timeout.\n");
    }

    if (errSts & SDHCI_INTR_ERR_CMD_INDEX) {
        sdc_dbg_print(" INTR: ERR## ... Command Index\n");
    }

    if (errSts & SDHCI_INTR_ERR_CMD_ENDBIT) {
        sdc_dbg_print(" INTR: ERR## ... Command End Bit.\n");
    }

    if (errSts & SDHCI_INTR_ERR_CMD_CRC) {
        sdc_dbg_print(" INTR: ERR## ... Command CRC.\n");
    }

    if (errSts & SDHCI_INTR_ERR_CMD_TIMEOUT) {
        sdc_dbg_print(" INTR: ERR## ... Command Timeout.\n");
    }
}

static u32
ftsdc021_ErrorRecovery(u8 ip_idx)
{
    u8 delayCount = 10; // more than 40us, The max. freq. for SD is 50MHz.

    /* Step 8 and Step 9 to save previous error status and
     * clear error interrupt signal status. This are already
     * done at IRQ handler.
     */
    if (SDHost[ip_idx].ErrRecover) {
        /* CMD12 */
        /* Step 10: Issue Abort CMD */
        ftsdc021_send_abort_command(ip_idx);

        /* Step 11: Check Command Inhibit DAT and CMD */
        while (gpRegSDC(ip_idx)->PresentState & (SDHCI_STS_CMD_INHIBIT | SDHCI_STS_CMD_DAT_INHIBIT))
            ;

        /* Step 12 */
        if (SDHost[ip_idx].Card->ErrorSts & SDHCI_INTR_ERR_CMD_LINE) {
            sdc_dbg_print("Non-recoverable Error:CMD Line Error\n");
            return ERR_NON_RECOVERABLE;
        }
        /* Step 13 */
        if (SDHost[ip_idx].Card->ErrorSts & SDHCI_INTR_ERR_DATA_TIMEOUT) {
            sdc_dbg_print("Non-recoverable Error:Data Line Timeout\n");
            return ERR_NON_RECOVERABLE;
        }
        /* Step 14 */
        while (delayCount > 0) {
            delayCount--;
        }
        /* Step 15 */
        if (gpRegSDC(ip_idx)->PresentState & SDHCI_STS_DAT_LINE_LEVEL) {
            /* Step 17 */
            sdc_dbg_print("Recoverable Error\n");
            return ERR_RECOVERABLE;
        } else {
            /* Step 16 */
            sdc_dbg_print("Non-recoverable Error\n");
            return ERR_NON_RECOVERABLE;
        }

    }

    return ERR_RECOVERABLE;
}

static u32
ftsdc021_get_erase_address(u8 ip_idx, u32* start_addr, u32* end_addr, u32 start, u32 block_cnt)
{
    if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_SD) {
        if (((SDHost[ip_idx].Card->OCR >> 30) & 0x1) == 0) { /* Standard Capacity SD memory card */
            *start_addr = start * (1 << SDHost[ip_idx].Card->CSD_ver_1.WRITE_BL_LEN);
            *end_addr = (start + (block_cnt - 1)) * (1 << SDHost[ip_idx].Card->CSD_ver_1.WRITE_BL_LEN);
        } else if (((SDHost[ip_idx].Card->OCR >> 30) & 0x1) == 1) { /* High Capacity SD memory card */
            // Start address isn't modify because the the unit of argument fro cmd is block.
            *start_addr = start;
            *end_addr = start + (block_cnt - 1);
        } else {
            sdc_dbg_print("SD:Unknown OCR format\n");
            return 1;
        }
#if SDHCI_SUPPORT_eMMC
    } else if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC || SDHost[ip_idx].Card->CardType == MEMORY_eMMC) {
#else
    } else if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC) {
#endif
        if (((SDHost[ip_idx].Card->OCR >> 29) & 0x3) == 2) { /* Sector mode */
            *start_addr = start;
            *end_addr = (start + (block_cnt - 1));
        } else if (((SDHost[ip_idx].Card->OCR >> 29) & 0x3) == 0) { /* Byte mode */
            *start_addr = start * FTSDC021_DEF_BLK_LEN;
            *end_addr = *start_addr + ((block_cnt) * FTSDC021_DEF_BLK_LEN) - 1;
        } else {
            sdc_dbg_print("MMC:Unknown OCR format\n");
            return 1;
        }
    } else {
        sdc_dbg_print("Unknown Card type(Neither MMC nor SD)\n");
        return 1;
    }

    return 0;
}

void
ftsdc021_delay(u32 ms)
{
    clock_t t0;

    t0 = jiffies();

    do {
        ;
    } while (jiffies() - t0 < ms);

    return;
}

void
ftsdc021_dump_regs()
{

    s32 i, j;
    u32* pd;
    u32 data;

    pd = (u32*) SDC_FTSDC021_0_PA_BASE;
    for (i = 0, j = 0; i < 0x80; i++) {
        if (j == 0)
            sdc_dbg_print("0x%08x : ", (u32 )(pd + i));

        if (i == 0x8)   //Skip the data port
            data = 0;
        else
            data = pd[i];
        sdc_dbg_print("%08X ", data);

        if (++j == 4) {
            sdc_dbg_print("\n");
            j = 0;
        }
    }

}

u32
ftsdc021_wait_for_state(u8 ip_idx, u32 state, u32 ms)
{
    clock_t t0;

    t0 = jiffies();
    do {
        if (ftsdc021_ops_send_card_status(ip_idx)) {
            sdc_dbg_print(" ERR## ... Send Card Status Failed.\n");
            return 1;
        }

        if ((jiffies() - t0) > ms) {
            sdc_dbg_print(" ERR## ... Card Status not return to define(%d) state.\n", state);
            return 1;
        }

    } while (((SDHost[ip_idx].Card->respLo >> 9) & 0xF) != state);

    return 0;
}

/**
 * Return the "residual" number of bytes write.
 * Caller check for this for correctness.
 * Do not use interrupt signal for PIO mode.
 * The unit of length is bytes.
 */
u32
ftsdc021_transfer_data(u8 ip_idx, Transfer_Act act, u32* buffer, u32 length)
{
    u32 trans_sz = 0, len, wait_t, bf = (u32)buffer, size = length;
    u16 mask;
    clock_t t0;
//sdc_dbg_print("i=%d,a=%s,d=%s,l=%04X\n", ip_idx, ((act == WRITE)?"w":"r"), ((SDHost[ip_idx].Card->FlowSet.UseDMA == SDMA)?"S":"N"), length);
//sdc_dbg_print("i=%d,a=%s,l=%X\n", ip_idx, ((act == WRITE)?"w":"r"), length);
//sdc_dbg_print("t=%d-", gm_timer_get_tick(0,0));
    if (SDHost[ip_idx].Card->FlowSet.UseDMA == PIO) {
//if (SDHost[ip_idx].Card->FlowSet.UseDMA == PIO || length==4) {
        u32 dsize;
        u8* u8_buf = NULL;
        u16* u16_buf = NULL;

        wait_t = SDHost[ip_idx].Card->FlowSet.timeout_ms;
        dsize = SDHost[ip_idx].Card->FlowSet.lineBound;
        switch (dsize) {
        case 1:
            u8_buf = (u8*) buffer;
            break;
        case 2:
            u16_buf = (u16*) buffer;
            break;
        default:
            break;
        }

        if (act == WRITE)
            mask = 0x1<<10;
        else
            mask = 0x1<<11;

        trans_sz = SDHost[ip_idx].Card->fifo_depth;

        while (length) {
            t0 = jiffies();
            //while (!(gpRegSDC(ip_idx)->IntrSts & mask)) {
            while (!(gpRegSDC(ip_idx)->	PresentState & mask)){
                if (jiffies() - t0 > SDHost[ip_idx].Card->FlowSet.timeout_ms) {
                    sdc_dbg_print(" ERR## ... Wait Buffer %s Ready timeout (%d).\n",
                            (act == WRITE) ? "Write" : "Read", length);
                    goto out;
                }
            }

            /* Clear Interrupt status */
            //gpRegSDC(ip_idx)->IntrSts = mask;

            if (!length)
                break;

            len = (length < trans_sz) ? length : trans_sz;
            length -= len;

            while (len) {
                switch (dsize) {
                case 1:
                    if (act == WRITE) {
                        *((volatile u8*) (SDC_FTSDC021_0_PA_BASE + SDHCI_DATA_PORT)) = *u8_buf;
                    } else {
                        *u8_buf = *((volatile u8*) (SDC_FTSDC021_0_PA_BASE + SDHCI_DATA_PORT));
                    }
                    len -= 1;
                    u8_buf++;
                    break;
                case 2:
                    if (act == WRITE) {
                        *((volatile u16*) (SDC_FTSDC021_0_PA_BASE + SDHCI_DATA_PORT)) = *u16_buf;
                    } else {
                        *u16_buf = *((volatile u16*) (SDC_FTSDC021_0_PA_BASE + SDHCI_DATA_PORT));
                    }
                    len -= 2;
                    u16_buf++;
                    break;
                default:
                    if (act == WRITE) {
                        *((volatile u32*) (SDC_FTSDC021_0_PA_BASE + SDHCI_DATA_PORT)) = *buffer;
                    } else {
                        *buffer = *((volatile u32*) (SDC_FTSDC021_0_PA_BASE + SDHCI_DATA_PORT));
                    }
                    len -= 4;
                    buffer++;
                    break;
                }
            }
        }

        /* Wait for last block to be completed */

    } else if (SDHost[ip_idx].Card->FlowSet.UseDMA == SDMA) {
        u32 next_addr;

        trans_sz = SDHost[ip_idx].Card->FlowSet.sdma_bound_mask + 1;
        wait_t = SDHost[ip_idx].Card->FlowSet.timeout_ms;   // * (trans_sz >> 9);
        //fLib_DisableDCache();
        do {
            t0 = jiffies();
            /* Make sure SDMA finish before we lacth the next address */

#if CFG_RTOS
            sdc_dbg_print("DMA_DEBUG: SDMA take xSemaphoreTake");
            xSemaphoreTake(SDHost[ip_idx].sdio_dma_semaphore, pdMS_TO_TICKS(wait_t));
#endif
            while (SDHost[ip_idx].Card->cmplMask) {
                sdc_dbg_print("DMA_DEBUG: waiting for SDMA interrupt");
                if (SDHost[ip_idx].Card->ErrorSts) {
                    sdc_dbg_print("error sts\n");
                    goto out;
                }
                if (jiffies() - t0 > wait_t) {
                    sdc_dbg_print(" ERR## ... Wait SDMA interrupt timeout (%d).\n", length);
                    goto out;
                }
            }
            next_addr = gpRegSDC(ip_idx)->SdmaAddr;
            /* Transfered bytes count */
            len = next_addr - (u32) buffer;
            /* Minus the total desired bytes count. SDMA stops at boundary
             * but it might already exceed our intended bytes
             */
            if ((s32) (length - len) < 0)
                length = 0;
            else
                length -= len;

            if (!length)
                break;

            /* Boundary Checking */
            if (next_addr & SDHost[ip_idx].Card->FlowSet.sdma_bound_mask) {
                sdc_dbg_print(" ERR## ... SDMA interrupt not at %d boundary, addr=0x%08x.\n",
                        (SDHost[ip_idx].Card->FlowSet.sdma_bound_mask + 1), next_addr);
                return 1;
            } else {
                sdc_dbg_print_level(1, " SDMA interrupt at addr=0x%08x.\n", next_addr);
            }
            /* Remaining bytes less than SDMA boundary.
             * For finite transfer, Wait for transfer complete interrupt.
             * Infinite transfer, wait for DMA interrupt.
             */
            if (length > trans_sz)
                SDHost[ip_idx].Card->cmplMask = WAIT_DMA_INTR;
            else
                SDHost[ip_idx].Card->cmplMask = WAIT_TRANS_COMPLETE;
            buffer = (u32*) next_addr;
#if CFG_RTOS
            SDHost[ip_idx].dma_transfer_data = 1;
#endif
            gpRegSDC(ip_idx)->SdmaAddr = next_addr;


        } while (1);


        //fLib_DisableDCache();
        //fLib_EnableDCache();
    } else if (SDHost[ip_idx].Card->FlowSet.UseDMA == ADMA) {
#if CFG_RTOS
        xSemaphoreTake(SDHost[ip_idx].sdio_dma_semaphore, portMAX_DELAY);
        sdc_dbg_print("DMA_DEBUG: ADMA take xSemaphoreTake");
#endif
        wait_t = SDHost[ip_idx].Card->FlowSet.timeout_ms * 10;
        length = 0;
    } else {
#if 0
        if ((SDHost[ip_idx].Card->FlowSet.UseDMA == EDMA) && (act == READ)) {
            t0 = 0;
            do {
                wait_t = fLib_GetDMAChannelIntStatus(0);
                if (wait_t & 0x2) {
                    sdc_dbg_print(" ERR ## ... DMA error.\n");
                    goto out;
                }

                if (wait_t & 0x1)
                break;
            }while (t0++ < 0x10000);

        }
#endif
        wait_t = SDHost[ip_idx].Card->FlowSet.timeout_ms * 10;
        length = 0;

    }

    /* Only need to wait transfer complete for DMA */
    t0 = jiffies();

    while (SDHost[ip_idx].Card->cmplMask && !SDHost[ip_idx].Card->ErrorSts) {
        sdc_dbg_print("DMA_DEBUG: waiting for DMA finish");
        if (jiffies() - t0 > wait_t) {
            sdc_dbg_print(" Wait Transfer Complete Interrupt timeout (0x%x, 0x%x, 0x%x).\n",
                    length, SDHost[ip_idx].Card->cmplMask, SDHost[ip_idx].Card->ErrorSts);
            if (SDHost[ip_idx].Card->cmplMask)
                length = SDHost[ip_idx].Card->cmplMask;
            else if (SDHost[ip_idx].Card->ErrorSts)
                length = SDHost[ip_idx].Card->ErrorSts;
            goto out;
        }
    }

    out:
    if (SDHost[ip_idx].Card->ErrorSts) {
        sdc_dbg_print("transfer_data error %x \n", SDHost[ip_idx].Card->ErrorSts);
        ftsdc021_ErrorRecovery(ip_idx);
        return 1;
    }
//sdc_dbg_print("%d\n", gm_timer_get_tick(0,0));
    if (act == READ && SDHost[ip_idx].Card->FlowSet.UseDMA != PIO) {	//invalidate again
//        nds32_dma_fast_inv_stage2(bf, bf + size);
    	cache_dma_fast_inv_stage2(bf, bf + size);
    }
    return length;
}

u32
ftsdc021_prepare_data(u8 ip_idx, u32 blk_cnt, u16 blk_sz, u32 buff_addr, Transfer_Act act)
{
    u32 bound, length;

    gpRegSDC(ip_idx)->BlkSize = blk_sz;

    gpRegSDC(ip_idx)->BlkCnt = blk_cnt;

    if (gpRegSDC(ip_idx)->TxMode & SDHCI_TXMODE_AUTOCMD23_EN)
        gpRegSDC(ip_idx)->SdmaAddr = blk_cnt;

    if (buff_addr == 0) {
        sdc_dbg_print("%s invalid buffer address[%x]\n", __func__, buff_addr);
    } else {
        if (SDHost[ip_idx].Card->FlowSet.UseDMA != PIO) {
            if (act == READ) {
//                nds32_dma_fast_inv_stage1(buff_addr, buff_addr + blk_sz * blk_cnt);
            	cache_dma_fast_inv_stage1(buff_addr, buff_addr + blk_sz * blk_cnt);
            } else {
//                nds32_dma_clean_range(buff_addr, buff_addr + blk_sz * blk_cnt);
            	dcache_clean_range(buff_addr, buff_addr + blk_sz * blk_cnt);
            }
        }
        SDHost[ip_idx].Card->cmplMask = WAIT_TRANS_COMPLETE;
        if (SDHost[ip_idx].Card->FlowSet.UseDMA == SDMA) {
#if CFG_RTOS
            SDHost[ip_idx].dma_transfer_data = 1;
#endif
            gpRegSDC(ip_idx)->SdmaAddr = buff_addr;
            gpRegSDC(ip_idx)->BlkSize |= (SDHost[ip_idx].Card->FlowSet.lineBound << 12);

            length = blk_cnt * blk_sz;
            bound = SDHost[ip_idx].Card->FlowSet.sdma_bound_mask + 1;
            /* "Infinite transfer" Or "buff_addr + length cross the SDMA boundary",
             Wait for DMA interrupt, no Transfer Complete interrupt  */
            if (((buff_addr + length) > ((buff_addr & ~SDHost[ip_idx].Card->FlowSet.sdma_bound_mask) + bound))) {
                SDHost[ip_idx].Card->cmplMask &= ~WAIT_TRANS_COMPLETE;
                SDHost[ip_idx].Card->cmplMask |= WAIT_DMA_INTR;
            }
        }

        else if (SDHost[ip_idx].Card->FlowSet.UseDMA == ADMA) {
#if CFG_RTOS
            SDHost[ip_idx].dma_transfer_data = 1;
#endif
            if (ftsdc021_fill_adma_desc_table(ip_idx, (blk_cnt * blk_sz), buff_addr))
                return 1;
            //if(ip_idx == 0)
            gpRegSDC(ip_idx)->ADMAAddr = SDHost[ip_idx].Card->FlowSet.adma_buffer;	//(u32) FTSDC021_SD_0_ADMA_BUF;
            //else
            //gpRegSDC(ip_idx)->ADMAAddr = (u32) FTSDC021_SD_1_ADMA_BUF;

            //if (SDHost[ip_idx].Card->FlowSet.adma2_use_interrupt)
            //    SDHost[ip_idx].Card->cmplMask |= WAIT_DMA_INTR;

        }
    }
    return 0;
}

u32
ftsdc021_set_transfer_mode(u8 ip_idx, u8 blk_cnt_en, u8 auto_cmd, u8 dir, u8 multi_blk)
{
    u16 mode;

    if ((dir != SDHCI_TXMODE_READ_DIRECTION) && (dir != 0)) {
        sdc_dbg_print(" ERR## ... Transder Mode, direction value not correct.\n");
        return 1;
    }

    /*SDMA can not use ACMD23 */
    if ((SDHost[ip_idx].Card->FlowSet.UseDMA == SDMA) && (auto_cmd == 2))
        auto_cmd = 1;

    auto_cmd <<= 2;
    if ((auto_cmd != SDHCI_TXMODE_AUTOCMD12_EN) && (auto_cmd != SDHCI_TXMODE_AUTOCMD23_EN) && (auto_cmd != 0)) {
        sdc_dbg_print(" ERR## ... Transder Mode, auto cmd value not correct.\n");
        return 1;
    }

    mode = (auto_cmd | dir);

    if ((SDHost[ip_idx].Card->FlowSet.UseDMA == ADMA) || (SDHost[ip_idx].Card->FlowSet.UseDMA == SDMA))
        mode |= SDHCI_TXMODE_DMA_EN;

    if (blk_cnt_en)
        mode |= SDHCI_TXMODE_BLKCNT_EN;
    if (multi_blk)
        mode |= SDHCI_TXMODE_MULTI_SEL;
    gpRegSDC(ip_idx)->TxMode = mode;

    return 0;
}

u32
ftsdc021_send_abort_command(u8 ip_idx)
{
    u16 val;
    clock_t t0;

    sdc_dbg_print_level(1, " Send Abort command ... ");
    SDHost[ip_idx].Card->ErrorSts = 0;
    SDHost[ip_idx].Card->cmplMask = (WAIT_CMD_COMPLETE | WAIT_TRANS_COMPLETE);

    gpRegSDC(ip_idx)->CmdArgu = 0;

    val =
            ((SDHCI_STOP_TRANS & 0x3f) << 8) | ((SDHCI_CMD_TYPE_ABORT & 0x3) << 6) | ((SDHCI_CMD_RTYPE_R1BR5B & 0x1F));
    gpRegSDC(ip_idx)->CmdReg = val;

    t0 = jiffies();
    while (SDHost[ip_idx].Card->cmplMask) {
        /* 1 secs */
        if ((jiffies() - t0) > (SDHost[ip_idx].Card->FlowSet.timeout_ms << 2)) {
            sdc_dbg_print(" ERR## ... Abort Command: Wait for INTR timeout(0x%x).\n",
                    SDHost[ip_idx].Card->cmplMask);
            return ERR_CMD_TIMEOUT;
        }
    }

    // Reset the Data and Cmd line due to the None-auto CMD12.
    gpRegSDC(ip_idx)->SoftRst |= (SDHCI_SOFTRST_CMD | SDHCI_SOFTRST_DAT);
    while (gpRegSDC(ip_idx)->SoftRst & (SDHCI_SOFTRST_CMD | SDHCI_SOFTRST_DAT))
        ;

    sdc_dbg_print_level(1, "Done.\n");

    return SDHost[ip_idx].Card->ErrorSts;
}

u32
ftsdc021_send_stop_command(u8 ip_idx)
{
    u16 val;
    clock_t t0;

    sdc_dbg_print_level(1, " Send Stop command ... ");
    SDHost[ip_idx].Card->ErrorSts = 0;
    SDHost[ip_idx].Card->cmplMask = (WAIT_CMD_COMPLETE | WAIT_TRANS_COMPLETE);

    gpRegSDC(ip_idx)->CmdArgu = 0;

    val =
            ((SDHCI_STOP_TRANS & 0x3f) << 8) | ((SDHCI_CMD_TYPE_NORMAL & 0x3) << 6) | ((SDHCI_CMD_RTYPE_R1BR5B & 0x1F));
    gpRegSDC(ip_idx)->CmdReg = val;

    t0 = jiffies();
    while (SDHost[ip_idx].Card->cmplMask) {
        /* 1 secs */
        if ((jiffies() - t0) > (SDHost[ip_idx].Card->FlowSet.timeout_ms << 1)) {
            sdc_dbg_print(" ERR## ... Stop Command: Wait for INTR timeout(0x%x).\n",
                    SDHost[ip_idx].Card->cmplMask);
            return ERR_CMD_TIMEOUT;
        }
    }

    sdc_dbg_print_level(1, "Done.\n");

    return SDHost[ip_idx].Card->ErrorSts;
}
/**
 * Set block count, block size, DMA table address.
 */
u32
ftsdc021_send_command(u8 ip_idx, u8 CmdIndex, u8 CmdType, u8 DataPresent,
        u8 RespType, u8 InhibitDATChk, u32 Argu)
{

    u16 val;
    clock_t t0;
    u32 err;
    u8 wait;

    sdc_dbg_print("CMD ID: %d, arg:%x, response: %x\n", CmdIndex, Argu);

    // Checking the MMC system spec. version more than 4.0 or newer.
#if SDHCI_SUPPORT_eMMC
    if ((SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC || SDHost[ip_idx].Card->CardType == MEMORY_eMMC) &&
#else
    if ((SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC) &&
#endif
            SDHost[ip_idx].Card->CSD_MMC.SPEC_VERS < 4) {

        if (CmdIndex == 6 || CmdIndex == 8) {
            sdc_dbg_print_level(0, " ERR## ... Commmand 6 or 8 is not supported in this MMC system spec.\n")
            return 1;
        }
    }

    if (gpRegSDC(ip_idx)->PresentState & SDHCI_STS_CMD_INHIBIT) {
        sdc_dbg_print(" ERR## CMD INHIBIT is one(Index = %d).\n", CmdIndex);
        return 1;
    }

    err = SDHost[ip_idx].Card->ErrorSts = 0;

    wait = WAIT_CMD_COMPLETE;
    SDHost[ip_idx].Card->cmplMask |= WAIT_CMD_COMPLETE;
    if (InhibitDATChk || (RespType == SDHCI_CMD_RTYPE_R1BR5B)) {
        if (gpRegSDC(ip_idx)->PresentState & SDHCI_STS_CMD_DAT_INHIBIT) {
            sdc_dbg_print(" cmd id :%d, ERR## CMD DATA INHIBIT is one.\n", CmdIndex);
            return 1;
        }

        /* If this is R1B, we need to wait transfer complete here.
         * Otherwise, wait transfer complete after read/write data.
         */
        if (RespType == SDHCI_CMD_RTYPE_R1BR5B) {
            SDHost[ip_idx].Card->cmplMask |= WAIT_TRANS_COMPLETE;
            wait |= WAIT_TRANS_COMPLETE;
        }

    }

    gpRegSDC(ip_idx)->CmdArgu = Argu;

    val = ((CmdIndex & 0x3f) << 8) | ((CmdType & 0x3) << 6) | ((DataPresent & 0x1) << 5) | (RespType & 0x1F);
    gpRegSDC(ip_idx)->CmdReg = val;

    if (CmdIndex != 13) {
        SDHost[ip_idx]
                .cmd_index = CmdIndex;

        if (DataPresent) {
            sdc_dbg_print_level(2, "Command Idx:%d, Cmd: 0x%x Argu:0x%x, Trans:0x%x, Sz:0x%x, cnt:%d.\n",
                    CmdIndex, gpRegSDC(ip_idx)->CmdReg, gpRegSDC(ip_idx)->CmdArgu, gpRegSDC(ip_idx)->TxMode,
                    gpRegSDC(ip_idx)->BlkSize,
                    gpRegSDC(ip_idx)->BlkCnt);
        } else {
            sdc_dbg_print_level(2, "Command Idx:%d, Cmd: 0x%x Argu:0x%x, Trans:0x%x.\n",
                    CmdIndex, gpRegSDC(ip_idx)->CmdReg, gpRegSDC(ip_idx)->CmdArgu, gpRegSDC(ip_idx)->TxMode)
        }
    }

    if (CmdIndex == SDHCI_SEND_TUNE_BLOCK)
        return 0;

    t0 = jiffies();
    while (!SDHost[ip_idx].Card->ErrorSts && (SDHost[ip_idx].Card->cmplMask & wait)) {
        /* 1 secs */
        if ((jiffies() - t0) > 1000) {
            sdc_dbg_print(" Wait for Interrupt timeout.\n");
            return ERR_CMD_TIMEOUT;
        }
    }

    /* Read Response Data */
    SDHost[ip_idx].Card->respLo = gpRegSDC(ip_idx)->CmdRespLo;
    SDHost[ip_idx].Card->respHi = gpRegSDC(ip_idx)->CmdRespHi;

    sdc_dbg_print("CMD ID: %d, response: %x\n", CmdIndex, SDHost[ip_idx].Card->respLo);

    if (SDHost[ip_idx].Card->ErrorSts) {
        err = SDHost[ip_idx].Card->ErrorSts;

        //if (SDHost[ip_idx].Card->ErrorSts & SDHCI_INTR_ERR_AutoCMD)
        //ftsdc021_autocmd_error_recovery(ip_idx);
        //else
        {
            //sdc_dbg_print("send command error\n",SDHost[ip_idx].Card->ErrorSts);
            //sdc_dbg_print("%s error bit = %x \n",__func__,SDHost[ip_idx].Card->ErrorSts);
            //  ftsdc021_ErrorRecovery(ip_idx);
        }
    }
    if (err != 0)
    	sdc_dbg_print("<ipx=%d> command:%d error %x\n", ip_idx, CmdIndex, err);
    return err;
}

u32
ftsdc021_card_read(u8 ip_idx, u32 startAddr, u32 blkcnt, u8* read_buf)
{
    u32 err;
    //sdc_dbg_print("addr:0x%06X, cnt=%02d, rlen=%04d, ", startAddr, blkcnt, SDHost[ip_idx].Card->read_block_len);
    err = ftsdc021_send_data_command(ip_idx, READ, startAddr, blkcnt, SDHost[ip_idx].Card->read_block_len,
            (u32*) read_buf);
    if (err)
        return err;

    if (ftsdc021_transfer_data(ip_idx, READ, (u32*) read_buf, (blkcnt << SDHost[ip_idx].Card->rblk_sz_2exp))) {
        return 1;
    }

    if (((blkcnt > 1) && !SDHost[ip_idx].Card->FlowSet.autoCmd)) {
        /* Stop */
        if (ftsdc021_send_stop_command(ip_idx))
            return 1;
    }

    return err;
}

u32
ftsdc021_card_write(u8 ip_idx, u32 startAddr, u32 blkcnt, u8* write_buf)
{
    u32 err;
//sdc_dbg_print("addr:0x%06X, cnt=%02d, wlen=%04d, ", startAddr, blkcnt, SDHost[ip_idx].Card->write_block_len);
    err = ftsdc021_send_data_command(ip_idx, WRITE, startAddr, blkcnt, SDHost[ip_idx].Card->write_block_len,
            (u32*) write_buf);
    if (err)
        return err;

    if (ftsdc021_transfer_data(ip_idx, WRITE, (u32*) write_buf, (blkcnt << SDHost[ip_idx].Card->wblk_sz_2exp))) {
        return 1;
    }

    if (((blkcnt > 1) && !SDHost[ip_idx].Card->FlowSet.autoCmd)) {
        /* Stop */
        if (ftsdc021_send_stop_command(ip_idx))
            return 1;
    }

    return 0;
}

u32
get_erase_group_size(u8 ip_idx, u32* erase_group_size)
{
    u32 grp_size;
    u32 grp_mult;

    if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_SD) {
        /* CSD version 1.0 */
        if (((SDHost[ip_idx].Card->CSD_HI >> 54) & 0x3) == 0) {
            if (SDHost[ip_idx].Card->CSD_ver_1.ERASE_BLK_EN == 1)
                *erase_group_size = 1;
            else
                *erase_group_size = SDHost[ip_idx].Card->CSD_ver_1.SECTOR_SIZE + 1;
        } else if (((SDHost[ip_idx].Card->CSD_HI >> 54) & 0x3) == 1) {
            /* CSD version 2.0, ERASE_BLK_EN is fixed to 1. Can erase in units of 512 bytes */
            *((u32*) (erase_group_size)) = 1;
        } else {
            sdc_dbg_print("SD:Unknown CSD format\n");
            return 1;
        }
#if SDHCI_SUPPORT_eMMC
    } else if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC ||
            SDHost[ip_idx].Card->CardType == MEMORY_eMMC) {
#else
    } else if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC) {
#endif
        grp_size = SDHost[ip_idx].Card->CSD_MMC.ERASE_GRP_SIZE;
        grp_mult = SDHost[ip_idx].Card->CSD_MMC.ERASE_GRP_MULT;
        *erase_group_size = (grp_size + 1) * (grp_mult + 1);
    } else {
        sdc_dbg_print("Unknown Card type(Neither MMC nor SD)\n");
        return 1;
    }

    return 0;
}

u32
ftsdc021_send_data_command(u8 ip_idx, Transfer_Act act, u32 startAddr, u32 blkCnt, u32 blkSz, void* bufAddr)
{
    u8 blk_cnt_en, auto_cmd, multi_blk, cmd_idx;
    u32 err;

    blk_cnt_en = auto_cmd = multi_blk = cmd_idx = 0;

    /* set Tx mode */
    if (blkCnt > 1) {
        /* Infinite Mode can not use Auto CMD12/CMD23 */
        auto_cmd = SDHost[ip_idx].Card->FlowSet.autoCmd;

        multi_blk = 1;

        if (blkCnt > 65535) {
            /* Block Count register limits the maximum of 65535 block transfer. If ADMA2
             * operation is less or equal to 65535, Block Count register can be used.
             * If ADMA2 larger than 65535 blocks, use descriptor table as transfer length.
             */
            if (SDHost[ip_idx].Card->FlowSet.UseDMA == ADMA)
                blk_cnt_en = 0;
            else {
                sdc_dbg_print(" Non-ADMA transfer can not larger than 65535 blocks.\n");
                return 1;
            }
        } else {
            blk_cnt_en = 1;
        }
    } else
        multi_blk = auto_cmd = blk_cnt_en = 0;

    if (ftsdc021_set_transfer_mode(ip_idx, blk_cnt_en, auto_cmd, (act == READ) ?
    SDHCI_TXMODE_READ_DIRECTION :
                                                                                 SDHCI_TXMODE_WRITE_DIRECTION,
            multi_blk))
        return 1;

    if (ftsdc021_prepare_data(ip_idx, blkCnt, (blkSz), (u32) bufAddr, act))
        return 1;

    if (!SDHost[ip_idx].Card->block_addr)
        startAddr = startAddr * (blkSz);

    /* Read :  Single block use CMD17 and multi block uses CMD18 */
    /* Write : Single block uses CMD24 and multi block uses CMD25 */
    if (blkCnt == 1)
        cmd_idx = (act == READ) ? SDHCI_READ_SINGLE_BLOCK : SDHCI_WRITE_BLOCK;
    else
        cmd_idx = (act == READ) ? SDHCI_READ_MULTI_BLOCK : SDHCI_WRITE_MULTI_BLOCK;

    err = ftsdc021_send_command(ip_idx, cmd_idx, SDHCI_CMD_TYPE_NORMAL, 1, SDHCI_CMD_RTYPE_R1R5R6R7, 1, startAddr);

    if (err)
        return err;

    if (SDHost[ip_idx].Card->respLo & SD_STATUS_ERROR_BITS) {
        sdc_dbg_print(" Card Status indicate error 0x%llx.\n", SDHost[ip_idx].Card->respLo);
        return 1;
    }

    return 0;
}

u32
ftsdc021_CheckCardInsert(u8 ip_idx)
{
    if ((SDHost[ip_idx].module_sts & SDHCI_MODULE_STS_FIXED) > 0) {
    	SDHost[ip_idx].Card->CardInsert = 1;
    } else {
    	if (gpRegSDC(ip_idx)->PresentState & SDHCI_STS_CARD_STABLE){
    		if(gpRegSDC(ip_idx)->PresentState & SDHCI_STS_CARD_INSERT)
    			SDHost[ip_idx].Card->CardInsert = 1;
    		else
    			SDHost[ip_idx].Card->CardInsert = 0;

    		if((SDHost[ip_idx].module_sts & SDHCI_MODULE_STS_INVERT) > 0)
    			SDHost[ip_idx].Card->CardInsert = !SDHost[ip_idx].Card->CardInsert;

       } else {
    	   SDHost[ip_idx].Card->CardInsert = 0;
       }
    }
    return SDHost[ip_idx].Card->CardInsert;
}

void
ftsdc021_SetPower(u8 ip_idx, s16 power)
{
    u8 pwr;
#if 1
    if (power == (s16) -1) {
        gpRegSDC(ip_idx)->PowerCtl = 0;
        goto out;
    }

    /*
     * Spec says that we should clear the power reg before setting
     * a new value.
     */
    gpRegSDC(ip_idx)->PowerCtl = 0;

    pwr = SDHCI_POWER_ON;

    switch (power) {
    case 17:
        case 18:
        case 19:
        pwr |= SDHCI_POWER_300;
        break;
    case 20:
        case 21:
        case 22:
        case 23:
        pwr |= SDHCI_POWER_330;
        break;
    default:
        sdc_dbg_print(" ERR## ... Voltage value not correct.\n");
    }
#endif
	if(pwr & SDHCI_POWER_330)
		sdc_dbg_print("vdd=%d, pwr = %s\n", power, "3.3v");
	else if(pwr & SDHCI_POWER_300)
		sdc_dbg_print("vdd=%d, pwr = %s\n", power, "3.0v");

    gpRegSDC(ip_idx)->PowerCtl = pwr;

    out:
    SDHost[ip_idx].power = power;

    sdc_dbg_print(" Power Control Register: 0x%01x.\n", gpRegSDC(ip_idx)->PowerCtl);
}

void
ftsdc021_SetSDClock(u8 ip_idx, u32 clock)
{
    int div, timeout, s_clk;
    u16 clk;

    div = timeout = s_clk = clk = 0;

    gpRegSDC(ip_idx)->ClkCtl = 0;
    //sdc_dbg_print("%s max clock=%d  %d \n",__func__,SDHost[ip_idx].max_clk,clock);
    //if (SDHost[ip_idx].Card->max_dtr < clock)
    //  sdc_dbg_print(" WARN## ... Set clock(%d Hz) larger than Max Rate(%d Hz).\n", clock, SDHost[ip_idx].Card->max_dtr);

    if (clock == 0)
        goto out;

    sdc_dbg_print("Host max clock :%d\n", SDHost[ip_idx].max_clk);
    if (SDHost[ip_idx].max_clk <= clock){
        div = 0;
        sdc_dbg_print("clock is overflow host max clock");
        sdc_dbg_print("clock will set as the host max clock");
    } else {
        for (div = 1; div < 0x3FF; div++) {
            if ((SDHost[ip_idx].max_clk / (2 * div)) <= clock)
                break;
        }
    }
    sdc_dbg_print("SDLK Freq select, Div: %d, Clock: (%d/%d) Hz, ", div, (SDHost[ip_idx].max_clk / div), clock);

    clk = div << 8;
    clk |= SDHCI_CLKCNTL_INTERNALCLK_EN;
    gpRegSDC(ip_idx)->ClkCtl = clk;

    /* Wait max 10 ms */
    timeout = 10;
    while (!(gpRegSDC(ip_idx)->ClkCtl & SDHCI_CLKCNTL_INTERNALCLK_STABLE)) {
        if (timeout == 0) {
            sdc_dbg_print(" ERR## ... Internal clock never estabilised.\n");
            return;
        }
        timeout--;
        ftsdc021_delay(1);
    }

    clk |= SDHCI_CLKCNTL_SDCLK_EN;
    gpRegSDC(ip_idx)->ClkCtl = clk;

    /* Make sure to use pulse latching when run below 50 MHz */
    /* SDR104 use multiphase DLL latching, others use pulse latching */
    if (!div)
        div = 1;
    else
        div *= 2;

    s_clk = SDHost[ip_idx].max_clk / div;

    if (s_clk < 100000000) {
        /* Clear the setting before */
        gpRegSDC(ip_idx)->VendorReg0 &= ~((0x3F << 8) | 0x1);
        gpRegSDC(ip_idx)->VendorReg0 |= (1); // | (0x1 << 8));

        /* Adjust the Pulse Latch Offset if div > 0 */
#if SDHCI_SUPPORT_eMMC
        if (SDHost[ip_idx].Card->CardType != SDIO_TYPE_CARD && SDHost[ip_idx].Card->CardType != MEMORY_eMMC) {
#else
        if (SDHost[ip_idx].Card->CardType != SDIO_TYPE_CARD) {
#endif
            if (s_clk > 1000000) {
                ftsdc021_pulselatch_tuning(ip_idx, div);
            }
        }

    } else { /* Sampling tuning for SDR104 and SDR50 if required */
        u32 try;

        gpRegSDC(ip_idx)->VendorReg0 &= ~1;

        if (s_clk == 100000000)
            try
            = 16;
        else {
            s_clk /= 1000000;
            try
            = ((8 * s_clk) / 25) - 16;
        }

        ftsdc021_execute_tuning(ip_idx, try);
    }

    out:
    SDHost[ip_idx].clock = clock;

    //sdc_dbg_print(" Clock Control Register: 0x%04x VendorReg0 = 0x%04x\n", gpRegSDC(ip_idx)->ClkCtl,gpRegSDC(ip_idx)->VendorReg0);
}
#if SDHCI_SUPPORT_eMMC
u32
ftsdc021_set_partition_access(u8 ip_idx, u8 partition)
{
    u8 val;

    if (SDHost[ip_idx].Card->CardType != MEMORY_eMMC) {
        sdc_dbg_print(" ERR## ... The inserted card isn't embedded MMC.\n");
        return 1;
    }

    SDHost[ip_idx].Card->EXT_CSD_MMC.PARTITION_CONF &= ~0x7;
    val = SDHost[ip_idx].Card->EXT_CSD_MMC.PARTITION_CONF | partition;

    if (ftsdc021_ops_mmc_switch(ip_idx, SDHost[ip_idx].Card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_PARTITION_CONF, val)) {
        sdc_dbg_print(" ERR## ... MMC: Set Partition Config failed.\n");
        return 1;
    }

    if (ftsdc021_ops_send_ext_csd(ip_idx, SDHost[ip_idx].Card)) {
        sdc_dbg_print(" ERR## ... MMC: Get EXT-CSD from card is failed\n");
        return 1;
    }

    if ((SDHost[ip_idx].Card->EXT_CSD_MMC.PARTITION_CONF & 7) != partition) {
        sdc_dbg_print(" ERR## ... Value not match (%d, %d).\n", SDHost[ip_idx].Card->EXT_CSD_MMC.PARTITION_CONF, val);
        return 1;
    }

    return 0;
}

u32
ftsdc021_set_bootmode(u8 ip_idx, u8 partition)
{
    u8 val;

    if (SDHost[ip_idx].Card->CardType != MEMORY_eMMC) {
        sdc_dbg_print(" ERR## ... The inserted card isn't embedded MMC.\n");
        return 1;
    }

    SDHost[ip_idx].Card->EXT_CSD_MMC.PARTITION_CONF &= ~(0x7 << 3);
    val = SDHost[ip_idx].Card->EXT_CSD_MMC.PARTITION_CONF | (partition << 3);

    if (ftsdc021_ops_mmc_switch(ip_idx, SDHost[ip_idx].Card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_PARTITION_CONF, val)) {
        sdc_dbg_print(" ERR## ... MMC: Set Partition Config failed.\n");
        return 1;
    }

    if (ftsdc021_ops_send_ext_csd(ip_idx, SDHost[ip_idx].Card)) {
        sdc_dbg_print(" ERR## ... MMC: Get EXT-CSD from card is failed\n");
        return 1;
    }

    if (((SDHost[ip_idx].Card->EXT_CSD_MMC.PARTITION_CONF >> 3) & 7) != partition) {
        sdc_dbg_print(" ERR## ... Value not match (%d, %d).\n", SDHost[ip_idx].Card->EXT_CSD_MMC.PARTITION_CONF, val);
        return 1;
    }

    return 0;
}

u32
ftsdc021_set_boot_size(u8 ip_idx, u8 value)
{
    if (SDHost[ip_idx].Card->CardType == MEMORY_eMMC) {
        if (ftsdc021_ops_mmc_switch(ip_idx, SDHost[ip_idx].Card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BOOT_SIZE_MULT, value)) {
            sdc_dbg_print(" ERR## ... MMC: Set Bus width failed.\n");
            return 1;
        }
    } else {
        sdc_dbg_print(" ERR## ... The inserted card isn't embedded MMC.\n");
    }
    return 0;
}
#endif
u32
ftsdc021_set_bus_width(u8 ip_idx, u8 width)
{
    u8 wdth = 0;

    // Commented by MikeYeh 081127: the BW means Bus width
    /* Setting the Bus width */
#if SDHCI_SUPPORT_eMMC
    if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC || SDHost[ip_idx].Card->CardType == MEMORY_eMMC) {
#else
    if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC) {
#endif
        if (SDHost[ip_idx].Card->CSD_MMC.SPEC_VERS < 4 && width != 1) {
            sdc_dbg_print(" MMC: Change width isn't allowed for version less than MMC4.1.\n");
            sdc_dbg_print(" MMC: Force the bus width to be 1.\n");
            width = 1;
        }

        /**
         * MMC BUS_WIDTH[183] of Extended CSD.
         * 0=1 bit, 1=4bits, 2=8bits.
         */
        switch (width) {
        case 1:
            wdth = 0;
            break;
        case 4:
            wdth = 1;
            break;
        case 8:
            wdth = 2;
            break;
        default:
            sdc_dbg_print(" ERR## ... MMC: Not supported bus witdth %d.\n", width);
            return 1;
        }

        if (SDHost[ip_idx].Card->CSD_MMC.SPEC_VERS >= 4) {
            if (ftsdc021_ops_mmc_switch(ip_idx, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH, wdth)) {
                sdc_dbg_print(" ERR## ... MMC: Set Bus width failed.\n");
                return 1;
            }
        }
    } else if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_SD) {
        /**
         * The Bit2 in SCR shows whether the "4bit bus width(DAT0-3)" is supported.
         * ACMD6 command, '00'=1bit or '10'=4bit bus.
         */
        switch (width) {
        case 1:
            if (!(SDHost[ip_idx].Card->SCR.SD_BUS_WIDTHS & SDHCI_SCR_SUPPORT_1BIT_BUS)) {
                sdc_dbg_print(" ERR## ... SD: 1 bit width not support by this card.\n");
                return 1;
            }
            wdth = SDHCI_1BIT_BUS_WIDTH;
            break;
        case 4:
            if (!(SDHost[ip_idx].Card->SCR.SD_BUS_WIDTHS & SDHCI_SCR_SUPPORT_4BIT_BUS)) {
                sdc_dbg_print(" ERR## ... SD: 4 bit width not support by this card.\n");
                return 1;
            }
            wdth = SDHCI_4BIT_BUS_WIDTH;
            break;
        default:
            sdc_dbg_print(" ERR## ... SD: Not supported bus witdth %d.\n", width);
            return 1;
        }

        if (ftsdc021_ops_app_set_bus_width(ip_idx, wdth)) {
            sdc_dbg_print(" ERR## ... SD: Set Bus Width %d failed !\n", width);
            return 1;
        }

    } else if (SDHost[ip_idx].Card->CardType == SDIO_TYPE_CARD) {
        ftsdc021_sdio_set_bus_width(ip_idx, width);
    }

    gpRegSDC(ip_idx)->HCReg &= ~(SDHCI_HC_BUS_WIDTH_8BIT | SDHCI_HC_BUS_WIDTH_4BIT);
    switch (width) {
    case 1:
        SDHost[ip_idx].Card->bus_width = 1;
        break;
    case 4:
        gpRegSDC(ip_idx)->HCReg |= SDHCI_HC_BUS_WIDTH_4BIT;
        SDHost[ip_idx].Card->bus_width = 4;
        break;
    case 8:
        gpRegSDC(ip_idx)->HCReg |= SDHCI_HC_BUS_WIDTH_8BIT;
        SDHost[ip_idx].Card->bus_width = 8;
        break;
    default:
        sdc_dbg_print(" Unsupport bus width %d for HW register setting.\n", width);
        break;
    }

    sdc_dbg_print(" Set Bus width %d bit(s), Host Control Register: 0x%x.\n", width, gpRegSDC(ip_idx)->HCReg);

    return 0;
}

/**
 * 0x0 : Default/SDR12/Normal Speed
 * 0x1 : SDR25/High Speed
 * 0x2 : SDR50
 * 0x3 : SDR104
 * 0x4 : DDR50
 */
u32
ftsdc021_set_bus_speed_mode(u8 ip_idx, u8 speed)
{
    u32 err, i;

    sdc_dbg_print("%s set speed = %d\n", __func__, speed);

    if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_SD) {
        /* Version 1.01 Card does not support CMD6 */
        if (SDHost[ip_idx].Card->SCR.SD_SPEC == 0) {
            speed = 0;
            goto out;
        }

        /* Check Group 1 function support */
        //sdc_dbg_print(" Checking function Group 1 (Bus Speed Mode) ...");
        for (i = 0; i < 5; i++) {
            if (ftsdc021_ops_sd_switch(ip_idx, 0, 0, i, &SDHost[ip_idx].Card->SwitchSts[0])) {
                sdc_dbg_print(" ERR## ... Problem reading Switch function(Bus Speed Mode).\n");
                return 1;
            }

            if (SDHost[ip_idx].Card->SwitchSts[13] & (1 << i)) {
                SDHost[ip_idx].Card->bs_mode |= (1 << i);
                //sdc_dbg_print(" %d ", i);
            }
        }
        //sdc_dbg_print("\n");

        if (speed > 4) {
            sdc_dbg_print(" ERR## ... Bus Speed Mode value %d error.\n", speed);
        }
#if 0
        if (!(gpRegSDC(ip_idx)->HostCtrl2 & SDHCI_18V_SIGNAL) && (speed > 1)) {
            sdc_dbg_print(" No 1.8V Signaling Can not set speed more than 1");
            return 1;
        }
#endif
        /* Check function support */
        if (!(SDHost[ip_idx].Card->bs_mode & (1 << speed))) {
            //sdc_dbg_print(" ERR## ... %s not support by Card.\n", SDC_ShowTransferSpeed(speed));
        	speed = UHS_SDR12_BUS_SPEED;
        }

        /* Change the clock to 400K Hz to prevent 64Bytes status data from DataCRC on some cards.*/
        ftsdc021_SetSDClock(ip_idx, 400000);

        err = ftsdc021_ops_sd_switch(ip_idx, 1, 0, speed, &SDHost[ip_idx].Card->SwitchSts[0]);
        /* Read it back for confirmation */
        if (err || ((SDHost[ip_idx].Card->SwitchSts[16] & 0xF) != speed)) {
            sdc_dbg_print(" ERR## ... Problem switching(Group 1) card into %d mode!\n", speed);
            return 1;
        }
#if SDHCI_SUPPORT_eMMC
    } else if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC || SDHost[ip_idx].Card->CardType == MEMORY_eMMC) {
#else
    } else if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC) {
#endif
        if (SDHost[ip_idx].Card->CSD_MMC.SPEC_VERS < 4 && speed != 0) {
            sdc_dbg_print(" MMC: Change speed isn't allowed for version less than MMC4.1.\n");
            sdc_dbg_print(" MMC: Force the bus speed to be default speed.\n");
            speed = 0;
            goto out;
        }
        /*
         * MMC Card only has Default and Hight speed.
         */
        if (speed > 1) {
            sdc_dbg_print(" ERR## ... Bus Speed Mode value %d error.\n", speed);
        }

        if (SDHost[ip_idx].Card->CSD_MMC.SPEC_VERS >= 4) {
                sdc_dbg_print(" MMC: switching card into %d mode!\n", speed);
            if (ftsdc021_ops_mmc_switch(ip_idx, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING, speed)) {
                sdc_dbg_print(" ERR## ... MMC: Problem switching card into %d mode!\n", speed);
                return 1;
            }
        }

    } else {
        //speed = 0;
        if (ftsdc021_sdio_set_bus_speed(ip_idx, speed)) {
            sdc_dbg_print("sdio support 25MHz\n");
            speed = 0;
        }
    }

    out:
    /* SDR mode only valid for 1.8v IO signal */
    if (gpRegSDC(ip_idx)->HostCtrl2 & SDHCI_18V_SIGNAL) {
        gpRegSDC(ip_idx)->HostCtrl2 &= ~0x7;
        gpRegSDC(ip_idx)->HostCtrl2 |= speed;

    } else {
    	sdc_dbg_print(" Current siggnal voltage is Not 1.8V\n");

        if (speed == 0)
            gpRegSDC(ip_idx)->HCReg &= ~(u8) 0x4;
        else if (speed == 1)
            gpRegSDC(ip_idx)->HCReg |= 0x4;
        else {
            sdc_dbg_print(" No 1.8V Signaling Can not set speed more than 1.\n");
            sdc_dbg_print(" Set as default speed \n");
            speed = 0;
        }
    }
    SDHost[ip_idx].Card->speed = (Bus_Speed) speed;

    if (speed < 4) {
        if (speed < 1) {
            SDHost[ip_idx].Card->max_dtr = 25000000; // << speed; tiger debug
        } else if (speed < 2){ /* current controller stable @ max 130 MHz */
            SDHost[ip_idx].Card->max_dtr = 100000000;
        } else if (speed < 3){
            SDHost[ip_idx].Card->max_dtr = 100000000;
        } else if (speed < 4){
        	SDHost[ip_idx].Card->max_dtr = 200000000;
        } else {
        	SDHost[ip_idx].Card->max_dtr = 50000000;
        }
    } else
        SDHost[ip_idx].Card->max_dtr = 25000000;

    ftsdc021_SetSDClock(ip_idx, SDHost[ip_idx].Card->max_dtr);

    // Make sure the state had been returned to "Transfer State"
#if SDHCI_SUPPORT_eMMC
    if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC ||
            SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_SD ||
            SDHost[ip_idx].Card->CardType == MEMORY_eMMC) {
#else
    if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC ||
            SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_SD) {
#endif
        if (ftsdc021_wait_for_state(ip_idx, CUR_STATE_TRAN, 2000)) {
            sdc_dbg_print(" ERR## ... Card can't return to transfer state\n");
            return 1;
        }
    }

    //sdc_dbg_print("Set Switch function(Bus Speed Mode = %d), Host Control Register: 0x%x, 0x%04x.\n",
    //speed, gpRegSDC(ip_idx)->HCReg, gpRegSDC(ip_idx)->HostCtrl2);

    return 0;
}

u32
ftsdc021_uhs1_mode(u8 ip_idx)
{
    return 0;    //(gpRegSDC(ip_idx)->HostCtrl2 & SDHCI_18V_SIGNAL);
}

u32
ftsdc021_pulselatch_tuning(u8 ip_idx, u32 div)
{

    u8 val, *pass_array, *buf;
    u32 i, j, up_bound = div;

#ifdef CFG_RTOS
    pass_array = pvPortMalloc(up_bound);
    buf = pvPortMalloc(512);
#else
    pass_array = malloc(up_bound);
    buf = malloc(512);
#endif  

    memset(pass_array, 1, up_bound);

    // Record
    for (i = 0; i < up_bound; i++) {
        gpRegSDC(ip_idx)->VendorReg0 &= ~(0x3f << 8);
        gpRegSDC(ip_idx)->VendorReg0 |= (i << 8);

        //sdc_dbg_print("Try Latch offset:%d\n", i);
        if (ftsdc021_card_read(ip_idx, 0, 1, buf)) {
            *(pass_array + i) = 0;
        }
    }

    // Find the most proper value to set
    if (up_bound == 1) {
        if (*(pass_array) == 0) {
            sdc_dbg_print("WARN## ... Can't find the proper value for latching.\n");
        }
        val = 0;
    } else {
        for (i = 0; i < up_bound; i++) {
            if (*(pass_array + i) == 0 &&
                    *(pass_array + ((i + 1) % up_bound)) == 1) {
                i = i + 1;
                break;
            }
            if (i == up_bound - 1) {
                if (*(pass_array) == 0) {
                    sdc_dbg_print("WARN## ... Can't find the proper Latch offset\n");
                    goto err;
                } else {
                    val = (up_bound >> 1);
                    goto out;
                }
            }
        }

        for (j = (i + 1); j < ((i + 1) + up_bound) - 1; j++) {
            if (*(pass_array + (j % up_bound)) == 0) {
                j = j - 1;
                break;
            }
        }

        val = (i + j) >> 1;
        if (val >= up_bound)
            val = val % up_bound;
    }
    out:
    sdc_dbg_print("The most proper Latch offset is %d\n", val);
    gpRegSDC(ip_idx)->VendorReg0 &= ~(0x3f << 8);
    gpRegSDC(ip_idx)->VendorReg0 |= (val << 8);
    err:
#ifdef CFG_RTOS
    vPortFree(pass_array);
    vPortFree(buf);
#else
    free(pass_array);
    free(buf);
#endif  

    return 0;
}

u32
ftsdc021_execute_tuning(u8 ip_idx, u32 try)
{
    u8 delay, cnt, hndshk;
    Transfer_Type dma;
    clock_t t0;

    delay = cnt = hndshk = 0;

    if (!((gpRegSDC(ip_idx)->HostCtrl2 & SDHCI_SDR104) && (gpRegSDC(ip_idx)->HostCtrl2 & SDHCI_18V_SIGNAL))) {
        sdc_dbg_print(" ERR## ... Tuning only require for SDR104 mode.\n");
        return 1;
    }

    /* Must tune 4 data lines */
    if (SDHost[ip_idx].Card->bus_width != 4)
        ftsdc021_set_bus_width(ip_idx, 4);

    /* Prevent using DMA when do tuning */
    dma = SDHost[ip_idx].Card->FlowSet.UseDMA;
    SDHost[ip_idx].Card->FlowSet.UseDMA = PIO;
    hndshk = gpRegSDC(ip_idx)->DmaHndshk & 1;
    gpRegSDC(ip_idx)->DmaHndshk = 0;

    cnt = 0;
    retune:
    delay = 0;
    gpRegSDC(ip_idx)->VendorReg3 &= ~((u32) 0xff << 24);
    gpRegSDC(ip_idx)->VendorReg3 = 0x00000804 | (try
            << 24);

    gpRegSDC(ip_idx)->SoftRst |= SDHCI_SOFTRST_DAT;
    /* Set Execute Tuning at Host Control2 Register */
    gpRegSDC(ip_idx)->HostCtrl2 |= SDHCI_EXECUTE_TUNING;

    /* Issue CMD19 repeatedly until Execute Tuning is zero.
     * Abort the loop if reached 40 times or 150 ms
     */
    do {
        if (ftsdc021_ops_send_tune_block(ip_idx))
            break;
        /* Wait until Buffer Read Ready */
        t0 = jiffies();
        do {
            if ((jiffies() - t0 > 100)) {
                sdc_dbg_print(" ERR##: Tuning wait for BUFFER READ READY timeout.\n");
                //gpRegSDC(ip_idx)->SoftRst |= (SDHCI_SOFTRST_CMD | SDHCI_SOFTRST_DAT);
                //while (gpRegSDC(ip_idx)->SoftRst & (SDHCI_SOFTRST_CMD | SDHCI_SOFTRST_DAT));
                break;
            }
        } while (!(gpRegSDC(ip_idx)->IntrSts & SDHCI_INTR_STS_BUFF_READ_READY));

        gpRegSDC(ip_idx)->IntrSts &= SDHCI_INTR_STS_BUFF_READ_READY;

    } while (gpRegSDC(ip_idx)->HostCtrl2 & SDHCI_EXECUTE_TUNING);

    /* Check Sampling Clock Select */
    if (gpRegSDC(ip_idx)->HostCtrl2 & SDHCI_SMPL_CLCK_SELECT) {
        /* FPGA only */
        delay = (gpRegSDC(ip_idx)->VendorReg3 >> 16) & 0x1F;
        sdc_dbg_print("Tuning Complete(0x%08x), SD Delay : 0x%x.\n", gpRegSDC(ip_idx)->VendorReg4, delay);

        if (!(gpRegSDC(ip_idx)->VendorReg4 & (1 << delay))) {
            sdc_dbg_print("SD Delay does not match tuning record(0x%08x).\n", gpRegSDC(ip_idx)->VendorReg4);

            if (cnt++ < 3) {
                goto retune;
            } else {
                sdc_dbg_print("Re-Tuning Failed.\n");
                SDHost[ip_idx].Card->FlowSet.UseDMA = dma;
                return 1;
            }
        }
    } else {
        sdc_dbg_print("Tuning Failed.\n");
        SDHost[ip_idx].Card->FlowSet.UseDMA = dma;
        return 1;
    }

    SDHost[ip_idx].Card->FlowSet.UseDMA = dma;
    gpRegSDC(ip_idx)->DmaHndshk |= hndshk;

    return 0;
}

/**
 * SDMA: line_bound is Buffer Boundary value.
 * ADMA: line_boud is bytes per descriptor line.
 */
void
ftsdc021_set_transfer_type(u8 ip_idx, Transfer_Type type, u32 line_bound)
{
    gpRegSDC(ip_idx)->HCReg &= ~(u8) (0x3 << 3);
    gpRegSDC(ip_idx)->DmaHndshk &= ~1;

    SDHost[ip_idx].Card->FlowSet.UseDMA = type;
    SDHost[ip_idx].Card->FlowSet.lineBound = line_bound;

    if (type == PIO) {
        gpRegSDC(ip_idx)->IntrSigEn = SDHCI_INTR_SIGN_EN_PIO;
        gpRegSDC(ip_idx)->IntrSigEn = SDHCI_INTR_EN_INIT;

    } else if (type == SDMA) {
        SDHost[ip_idx].Card->FlowSet.sdma_bound_mask = (1 << (SDHost[ip_idx].Card->FlowSet.lineBound + 12)) - 1;
        gpRegSDC(ip_idx)->IntrSigEn = SDHCI_INTR_SIGN_EN_SDMA;
        //gpRegSDC(ip_idx)->IntrSigEn = SDHCI_INTR_EN_INIT;
    }
    #if 1
    else if (type == ADMA) {
        SDHost[ip_idx].Card->FlowSet.adma2_insert_nop = 0;
        SDHost[ip_idx].Card->FlowSet.adma2_use_interrupt = 0;
        //  if(ip_idx == 0)
        //  gpRegSDC(ip_idx)->ADMAAddr = FTSDC021_SD_0_ADMA_BUF;
        //else
        gpRegSDC(ip_idx)->ADMAAddr = SDHost[ip_idx].Card->FlowSet.adma_buffer;
        gpRegSDC(ip_idx)->HCReg |= (u8) SDHCI_HC_USE_ADMA2;
        gpRegSDC(ip_idx)->IntrSigEn = SDHCI_INTR_EN_INIT;        //SDHCI_INTR_SIGN_EN_ADMA;
    }
    #endif
    else {
        sdc_dbg_print("%s unsupported DMA mode\n", __func__);
        gpRegSDC(ip_idx)->DmaHndshk |= 1;
    }

    gpRegSDC(ip_idx)->ErrSigEn = SDHCI_ERR_EN_ALL;
}

void
ftsdc021_HCReset(u8 ip_idx, u8 type)
{
    u32 i;

    gpRegSDC(ip_idx)->SoftRst = type;
    for (i = 0; i < 100; i++) {
        if ((gpRegSDC(ip_idx)->SoftRst & type) == 0)
            break;
        ftsdc021_delay(1);
    }

#if 0
   if (type & SDHCI_SOFTRST_ALL){
       ftsdc021_init(ip_idx);
   } else {
        gpRegSDC(ip_idx)->SdmaAddr = 0;
        gpRegSDC(ip_idx)->BlkSize = 0;
        gpRegSDC(ip_idx)->BlkCnt = 0;
        gpRegSDC(ip_idx)->CmdArgu = 0;
        gpRegSDC(ip_idx)->CmdReg = 0;

        gpRegSDC(ip_idx)->TxMode = 0;
        gpRegSDC(ip_idx)->ADMAAddr = 0;
        gpRegSDC(ip_idx)->VendorReg0 = 0;
    }
#endif
}

u32
ftsdc021_card_erase(u8 ip_idx, u32 StartBlk, u32 BlkCount)
{
    u32 erase_start_addr = 0;
    u32 erase_end_addr = 0;
    u32 err;

    if (ftsdc021_get_erase_address(ip_idx, &erase_start_addr, &erase_end_addr, StartBlk, BlkCount)) {
        sdc_dbg_print("Getting the erase start and end address are failed\n");
        return 1;
    }

#if SDHCI_SUPPORT_eMMC
    if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC || SDHost[ip_idx].Card->CardType == MEMORY_eMMC) {
#else
    if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC) {
#endif
        /* CMD 35 */
        err =
                ftsdc021_send_command(ip_idx, SDHCI_ERASE_GROUP_START, SDHCI_CMD_TYPE_NORMAL, 0,
                SDHCI_CMD_RTYPE_R1R5R6R7, 0, erase_start_addr);
        sdc_dbg_print_level(1, "resp:%llx\n", SDHost[ip_idx].Card->respLo);

        if (err || (SDHost[ip_idx].Card->respLo & SD_STATUS_ERROR_BITS)) {
            sdc_dbg_print("Erase command: %d, arg:0x%x failed.\n", SDHCI_ERASE_GROUP_START, erase_start_addr);
            return 1;
        }
        /* CMD 36 */
        err =
                ftsdc021_send_command(ip_idx, SDHCI_ERASE_GROUP_END, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7,
                        0, erase_end_addr);
        sdc_dbg_print_level(1, "resp:%llx\n", SDHost[ip_idx].Card->respLo);

        if (err || (SDHost[ip_idx].Card->respLo & SD_STATUS_ERROR_BITS)) {
            sdc_dbg_print("Erase command: %d, arg:0x%x failed.\n", SDHCI_ERASE_GROUP_END, erase_end_addr);
            return 1;
        }

        /* CMD 38 */
        err = ftsdc021_send_command(ip_idx, SDHCI_ERASE, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1BR5B, 1, 0);
        sdc_dbg_print_level(1, "resp:%llx\n", SDHost[ip_idx].Card->respLo);

        if (err || (SDHost[ip_idx].Card->respLo & SD_STATUS_ERROR_BITS)) {
            sdc_dbg_print("Erase command: %d failed.\n", SDHCI_ERASE);
            return 1;
        }

        if (ftsdc021_wait_for_state(ip_idx, CUR_STATE_TRAN, 5000))
            return 1;

    } else if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_SD) {
        /* CMD 32 */
        err =
                ftsdc021_send_command(ip_idx, SDHCI_ERASE_WR_BLK_START, SDHCI_CMD_TYPE_NORMAL, 0,
                SDHCI_CMD_RTYPE_R1R5R6R7, 0, erase_start_addr);
        if (err || (SDHost[ip_idx].Card->respLo & SD_STATUS_ERROR_BITS)) {
            sdc_dbg_print(" ERR ... Erase command: %d, arg:0x%x failed.\n",
                    SDHCI_ERASE_WR_BLK_START, erase_start_addr);
            return 1;
        }
        /* CMD 33 */
        err =
                ftsdc021_send_command(ip_idx, SDHCI_ERASE_WR_BLK_END, SDHCI_CMD_TYPE_NORMAL, 0,
                SDHCI_CMD_RTYPE_R1R5R6R7, 0, erase_end_addr);
        if (err || (SDHost[ip_idx].Card->respLo & SD_STATUS_ERROR_BITS)) {
            sdc_dbg_print(" ERR## ... Erase command: %d, arg:0x%x failed.\n",
                    SDHCI_ERASE_WR_BLK_END, erase_end_addr);
            return 1;
        }

        /* CMD 38 */
        err = ftsdc021_send_command(ip_idx, SDHCI_ERASE, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1BR5B, 1, 0);
        if (err || (SDHost[ip_idx].Card->respLo & SD_STATUS_ERROR_BITS)) {
            sdc_dbg_print(" ERR## ... Erase command: %d failed.\n", SDHCI_ERASE);
            return 1;
        }

        if (ftsdc021_wait_for_state(ip_idx, CUR_STATE_TRAN, 5000))
            return 1;
    } else {
        sdc_dbg_print(" ERR## ... This card type doesn't support erasing\n");
        return 1;
    }

    return 0;
}

u32
ftsdc021_enable_irq(u8 ip_idx, u16 sts)
{
    gpRegSDC(ip_idx)->IntrEn |= sts;
    gpRegSDC(ip_idx)->IntrSigEn |= sts;
    return (u32) ERR_SD_NO_ERROR;
}

u32
ftsdc021_disable_irq(u8 ip_idx, u16 sts)
{
    gpRegSDC(ip_idx)->IntrEn &= ~sts;
    gpRegSDC(ip_idx)->IntrSigEn &= ~sts;
    return (u32) ERR_SD_NO_ERROR;
}

void
ftsdc021_disable_card_detect(u8 ip_idx)
{
    ftsdc021_disable_irq(ip_idx, (SDHCI_INTR_STS_CARD_INSERT | SDHCI_INTR_STS_CARD_REMOVE));
}

void
ftsdc021_enable_card_detect(u8 ip_idx)
{
    ftsdc021_enable_irq(ip_idx, (SDHCI_INTR_STS_CARD_INSERT | SDHCI_INTR_STS_CARD_REMOVE));
}

void
ftsdc021_card_detection_init(u8 ip_idx)
{
    if ((SDHost[ip_idx].module_sts & SDHCI_MODULE_STS_INVERT) == 0)
        return;
    ftsdc021_disable_card_detect(ip_idx);
    gpRegSDC(ip_idx)->HCReg |= SDHCI_HC_CARD_DETECT_SIGNAL;
    ftsdc021_enable_card_detect(ip_idx);
}

//Enable SD card push-in and pull-out detect
void
ftsdc021_card_detection_handle(u8 ip_idx)
{
    u32 present_state;
    if ((SDHost[ip_idx].module_sts & SDHCI_MODULE_STS_INVERT) == 0)
        return;

    present_state = gpRegSDC(ip_idx)->PresentState;
    if ((present_state & SDHCI_STS_CARD_DETECT) == 0) {
        gpRegSDC(ip_idx)->HCReg |= (SDHCI_HC_CARD_DETECT_SIGNAL | SDHCI_HC_CARD_DETECT_TEST);
    } else {
        if ((gpRegSDC(ip_idx)->HCReg & SDHCI_HC_CARD_DETECT_TEST) > 0)
            gpRegSDC(ip_idx)->HCReg &= ~SDHCI_HC_CARD_DETECT_TEST;

        ftsdc021_disable_card_detect(ip_idx);
        gpRegSDC(ip_idx)->HCReg |= SDHCI_HC_CARD_DETECT_SIGNAL;
        ftsdc021_enable_card_detect(ip_idx);
    }
}
void
ftsdc021_0_IntrHandler()
{
    u16 sts = 0;
    u8 ip_idx = 0;
    u16 sdio_sts = 0;
#if CFG_RTOS
    BaseType_t xHPWoken = pdFALSE;
#endif
    //volatile u32 next_addr;

    /* Read Interrupt Status */
    sts = gpRegSDC(ip_idx)->IntrSts;
    sdio_sts = gpRegSDC(ip_idx)->IntrSts;

    /* As soon as the command complete, data ready to be read/write.
     * Buffer Read/Write Ready usually used when PIO mode.
     * We don't expect to use interrupt here, but polling.
     * Leave it to read/write data function.
     */
    sts &= ~(SDHCI_INTR_STS_BUFF_READ_READY | SDHCI_INTR_STS_BUFF_WRITE_READY);

    /* Clear Interrupt Status immediately */
    gpRegSDC(ip_idx)->IntrSts &= sts;

    if (sts & SDHCI_INTR_STS_CARD_INTR) {
    }

    if (sts & SDHCI_INTR_STS_CARD_INSERT) {
        sdc_dbg_print("INTR: Card Insert\n");
        SDHost[ip_idx].Card->CardInsert = 1;
        if (SDHost[ip_idx].Card->already_init == true) //{
        //ftsdc021_init(ip_idx);
        //} else
        {
            ftsdc021_SetSDClock(ip_idx, SDHost[ip_idx].min_clk);
            ftsdc021_SetPower(ip_idx, 17);
        }
        //send_control(EVENT_SD,SD_VALUE_INSERT,FROM_INTRQ);
    }
    if (sts & SDHCI_INTR_STS_CARD_REMOVE) {
        sdc_dbg_print("INTR: Card Remove\n");
        if (ftsdc021_CheckCardInsert(ip_idx) == 0) {
            sdc_dbg_print("Card remove ====\n");
            SDHost[ip_idx].Card->CardInsert = 0;
            SDHost[ip_idx].sdcard_init_complete = 0;

            SDHost[ip_idx].Card->already_init = false;
            // Stop to provide clock
            ftsdc021_SetSDClock(ip_idx, 0);
            // Stop to provide power
            ftsdc021_SetPower(ip_idx, -1);
            //send_control(EVENT_SD,SD_VALUE_REMOVE,FROM_INTRQ);
        }
    }

    if (sts & SDHCI_INTR_STS_CMD_COMPLETE) {
        SDHost[ip_idx].Card->cmplMask &= ~WAIT_CMD_COMPLETE;
    }

    if (sts & SDHCI_INTR_STS_TXR_COMPLETE) {
        if (SDHost[ip_idx].Card->cmplMask & WAIT_TRANS_COMPLETE) {
            SDHost[ip_idx].Card->cmplMask &= ~WAIT_TRANS_COMPLETE;
#if CFG_RTOS
            if(SDHost[ip_idx].dma_transfer_data ==1)
            {
                SDHost[ip_idx].dma_transfer_data = 0;
                xSemaphoreGiveFromISR(SDHost[ip_idx].sdio_dma_semaphore, &xHPWoken);
                sdc_dbg_print("DMA_DEBUG: release semp complete");
            }
#endif
        } else {
            sdc_dbg_print("!");
            while (1)
                ;
        }
    }

    if (sts & SDHCI_INTR_STS_BLKGAP) {
        SDHost[ip_idx].Card->cmplMask &= ~WAIT_BLOCK_GAP;
        sdc_dbg_print("$");
    }

    if (sts & SDHCI_INTR_STS_DMA) {

        //sdc_dbg_print("DMA Intr");
        SDHost[ip_idx].Card->cmplMask &= ~WAIT_DMA_INTR;
#if CFG_RTOS
        if(SDHost[ip_idx].dma_transfer_data ==1)
        {
            SDHost[ip_idx].dma_transfer_data = 0;
            xSemaphoreGiveFromISR(SDHost[ip_idx].sdio_dma_semaphore, &xHPWoken);
            sdc_dbg_print("DMA_DEBUG: release semp intr");
        }
#endif
    }

    if (sts & SDHCI_INTR_STS_ERR) {

        ftsdc021_print_err_msg(ip_idx, gpRegSDC(ip_idx)->ErrSts);
        SDHost[ip_idx].Card->ErrorSts = gpRegSDC(ip_idx)->ErrSts;
        /* Step 2: Check CMD Line Error */
        if (SDHost[ip_idx].Card->ErrorSts & SDHCI_INTR_ERR_CMD_LINE) {
            /* Step 3: Software Reset for CMD line */
            gpRegSDC(ip_idx)->SoftRst |= SDHCI_SOFTRST_CMD;
            /* Step 4 */
            while (gpRegSDC(ip_idx)->SoftRst & SDHCI_SOFTRST_CMD)
                ;
        }

        /* Step 5: Check DAT Line Error */
        if (SDHost[ip_idx].Card->ErrorSts & SDHCI_INTR_ERR_DAT_LINE) {
#if CFG_RTOS
            if((SDHost[ip_idx].dma_transfer_data ==1) && ((SDHost[ip_idx].Card->ErrorSts |  SDHCI_INTR_ERR_DAT_LINE) == SDHCI_INTR_ERR_DAT_LINE))
            {
                SDHost[ip_idx].dma_transfer_data = 0;
                xSemaphoreGiveFromISR(SDHost[ip_idx].sdio_dma_semaphore, &xHPWoken);
                sdc_dbg_print("DMA_DEBUG: release sempaphore, data error");
            }
#endif
            /* Step 6: Software Reset for DAT line */
            gpRegSDC(ip_idx)->SoftRst |= SDHCI_SOFTRST_DAT;
            /* Step 7 */
            while (gpRegSDC(ip_idx)->SoftRst & SDHCI_SOFTRST_DAT)
                ;
        }

        /* Auto CMD Error Status register is reset */
        if (SDHost[ip_idx].Card->ErrorSts & SDHCI_INTR_ERR_AutoCMD)
            SDHost[ip_idx].Card->autoErr = gpRegSDC(ip_idx)->AutoCMDErr;

        gpRegSDC(ip_idx)->ErrSts = SDHost[ip_idx].Card->ErrorSts;

        if (SDHost[ip_idx].Card->ErrorSts & SDHCI_INTR_ERR_TUNING) {
            sdc_dbg_print(" INTR: ERR## ... Tuning Error.\n");
            ftsdc021_execute_tuning(ip_idx, 16);
        }

    }

    if (SDHost[ip_idx].sdio_card_int != NULL)
        //SDHost[ip_idx].sdio_card_int((sdio_sts & SDHCI_INTR_STS_CMD_COMPLETE | sdio_sts & SDHCI_INTR_STS_CARD_INTR));
        SDHost[ip_idx].sdio_card_int(
                ((sdio_sts & SDHCI_INTR_STS_TXR_COMPLETE) | (sdio_sts & SDHCI_INTR_STS_CARD_INTR)
                        | (sdio_sts & SDHCI_INTR_STS_CMD_COMPLETE)));
#if CFG_RTOS
    portYIELD_FROM_ISR(xHPWoken);
#endif
}

//void
//SDHC_ISR()
//{
//    unsigned int msk = (1 << IRQ_SDHC_VECTOR);
//    /* Mask and clear HW interrupt vector */
//    __nds32__mtsr_dsb(__nds32__mfsr(NDS32_SR_INT_MASK2) & ~msk, NDS32_SR_INT_MASK2);
//    __nds32__mtsr(msk, NDS32_SR_INT_PEND2);
//    ftsdc021_0_IntrHandler();
//    /* Unmask HW interrupt vector */
//    __nds32__mtsr(__nds32__mfsr(NDS32_SR_INT_MASK2) | msk, NDS32_SR_INT_MASK2);
//}
#if 0
void
ftsdc021_1_IntrHandler(u32 num, void* priv)
{
    u16 sts = 0;
    u8 ip_idx = 1;
    u16 sdio_sts = 0;

    /* Read Interrupt Status */
    sts = gpRegSDC(ip_idx)->IntrSts;
    sdio_sts = sts;

    /* As soon as the command complete, data ready to be read/write.
     * Buffer Read/Write Ready usually used when PIO mode.
     * We don't expect to use interrupt here, but polling.
     * Leave it to read/write data function.
     */
    sts &= ~(SDHCI_INTR_STS_BUFF_READ_READY | SDHCI_INTR_STS_BUFF_WRITE_READY);

    /* Clear Interrupt Status immediately */
    gpRegSDC(ip_idx)->IntrSts &= sts;

    if (sts & SDHCI_INTR_STS_CARD_INTR) {
    }

    if (sts & SDHCI_INTR_STS_CARD_INSERT) {
        sdc_dbg_print("INTR: Card Insert\n");
        SDHost[ip_idx].Card->CardInsert = 1;
        if (SDHost[ip_idx].Card->already_init == true) {
            //  ftsdc021_init(ip_idx);
            //} else {
            ftsdc021_SetSDClock(ip_idx, SDHost[ip_idx].min_clk);
            ftsdc021_SetPower(ip_idx, 17);
        }
    }
    if (sts & SDHCI_INTR_STS_CARD_REMOVE) {
        sdc_dbg_print("INTR: Card Remove\n");
        if (ftsdc021_CheckCardInsert(ip_idx) == 0) {
            sdc_dbg_print("Card remove ====\n");
            SDHost[ip_idx].Card->CardInsert = 0;
            SDHost[ip_idx].sdcard_init_complete = 0;
            SDHost[ip_idx].Card->already_init = false;
            // Stop to provide clock
            ftsdc021_SetSDClock(ip_idx, 0);
            // Stop to provide power
            ftsdc021_SetPower(ip_idx, -1);
        }
    }

    if (sts & SDHCI_INTR_STS_CMD_COMPLETE) {
        SDHost[ip_idx].Card->cmplMask &= ~WAIT_CMD_COMPLETE;
    }

    if (sts & SDHCI_INTR_STS_TXR_COMPLETE) {
        if (SDHost[ip_idx].Card->cmplMask & WAIT_TRANS_COMPLETE) {
            SDHost[ip_idx].Card->cmplMask &= ~WAIT_TRANS_COMPLETE;
        } else {
            sdc_dbg_print("!");
            while (1)
                ;
        }
    }

    if (sts & SDHCI_INTR_STS_BLKGAP) {
        SDHost[ip_idx].Card->cmplMask &= ~WAIT_BLOCK_GAP;
        sdc_dbg_print("$");
    }

    if (sts & SDHCI_INTR_STS_DMA) {
        SDHost[ip_idx].Card->cmplMask &= ~WAIT_DMA_INTR;
    }

    if (sts & SDHCI_INTR_STS_ERR) {
        //ftsdc021_dump_regs();
        ftsdc021_print_err_msg(ip_idx, gpRegSDC(ip_idx)->ErrSts);
        SDHost[ip_idx].Card->ErrorSts = gpRegSDC(ip_idx)->ErrSts;
        /* Step 2: Check CMD Line Error */
        if (SDHost[ip_idx].Card->ErrorSts & SDHCI_INTR_ERR_CMD_LINE) {
            /* Step 3: Software Reset for CMD line */
            gpRegSDC(ip_idx)->SoftRst |= SDHCI_SOFTRST_CMD;
            /* Step 4 */
            while (gpRegSDC(ip_idx)->SoftRst & SDHCI_SOFTRST_CMD)
                ;
        }

        /* Step 5: Check DAT Line Error */
        if (SDHost[ip_idx].Card->ErrorSts & SDHCI_INTR_ERR_DAT_LINE) {
            /* Step 6: Software Reset for DAT line */
            gpRegSDC(ip_idx)->SoftRst |= SDHCI_SOFTRST_DAT;
            /* Step 7 */
            while (gpRegSDC(ip_idx)->SoftRst & SDHCI_SOFTRST_DAT)
                ;
        }

        /* Auto CMD Error Status register is reset */
        if (SDHost[ip_idx].Card->ErrorSts & SDHCI_INTR_ERR_AutoCMD)
            SDHost[ip_idx].Card->autoErr = gpRegSDC(ip_idx)->AutoCMDErr;

        gpRegSDC(ip_idx)->ErrSts = SDHost[ip_idx].Card->ErrorSts;

        if (SDHost[ip_idx].Card->ErrorSts & SDHCI_INTR_ERR_TUNING) {
            sdc_dbg_print(" INTR: ERR## ... Tuning Error.\n");
            ftsdc021_execute_tuning(ip_idx, 16);
        }

    }
    if (SDHost[ip_idx].sdio_card_int != NULL)
        //SDHost[ip_idx].sdio_card_int((sdio_sts & SDHCI_INTR_STS_CMD_COMPLETE | sdio_sts & SDHCI_INTR_STS_CARD_INTR));
        SDHost[ip_idx].sdio_card_int(
                ((sdio_sts & SDHCI_INTR_STS_TXR_COMPLETE) | (sdio_sts & SDHCI_INTR_STS_CARD_INTR)
                        | (sdio_sts & SDHCI_INTR_STS_CMD_COMPLETE)));
}
#endif
u32
ftsdc021_read_scr(u8 ip_idx)
{
    if (SDHost[ip_idx].Card->CardType != MEMORY_CARD_TYPE_SD)
        return 1;

    memset(&SDHost[ip_idx].Card->SCR, 0, sizeof(SD_SCR));

    if (ftsdc021_ops_app_send_scr(ip_idx)) {
        sdc_dbg_print(" ERR## ... SD: Get SCR from card is failed\n");
        return 1;
    }

    sdc_dbg_print("**************** SCR register ****************\n");
    sdc_dbg_print("SCR Structure: %d\n", SDHost[ip_idx].Card->SCR.SCR_STRUCTURE);
    sdc_dbg_print("SCR Memory Card - Spec. Version: %d\n", SDHost[ip_idx].Card->SCR.SD_SPEC);
    sdc_dbg_print("Data status after erase: %d\n", SDHost[ip_idx].Card->SCR.DATA_STAT_AFTER_ERASE);
    sdc_dbg_print("SD Security Support: %d\n", SDHost[ip_idx].Card->SCR.SD_SECURITY);
    sdc_dbg_print("DAT Bus widths supported: %d\n", SDHost[ip_idx].Card->SCR.SD_BUS_WIDTHS);
    sdc_dbg_print("CMD23 supported: %d\n", SDHost[ip_idx].Card->SCR.CMD23_SUPPORT);
    sdc_dbg_print("CMD20 supported: %d\n", SDHost[ip_idx].Card->SCR.CMD20_SUPPORT);

    sdc_dbg_print("**********************************************\n");

    return 0;

}

u32
ftsdc021_read_ext_csd(u8 ip_idx)
{
#if SDHCI_SUPPORT_eMMC
    if (SDHost[ip_idx].Card->CardType != MEMORY_CARD_TYPE_MMC && SDHost[ip_idx].Card->CardType != MEMORY_eMMC)
    return 1;
#else
    if (SDHost[ip_idx].Card->CardType != MEMORY_CARD_TYPE_MMC)
        return 1;
#endif
    if (SDHost[ip_idx].Card->CSD_MMC.SPEC_VERS < 4) {
        sdc_dbg_print(" Commmand 8 is not supported in this MMC system spec.\n");
        goto fail;
    }

    if (ftsdc021_ops_send_ext_csd(ip_idx)) {
        sdc_dbg_print(" ERR## ... MMC: Get EXT-CSD from card is failed\n");
        goto fail;
    }

    sdc_dbg_print("**************** Extended CSD register ****************\n");
    if (SDHost[ip_idx].Card->EXT_CSD_MMC.CARDTYPE & 0xE)
        SDHost[ip_idx].Card->max_dtr = 52000000;
    else if (SDHost[ip_idx].Card->EXT_CSD_MMC.CARDTYPE & 1)
        SDHost[ip_idx].Card->max_dtr = 26000000;
    else
        sdc_dbg_print(" WARN## ...  Unknown Max Speed at EXT-CSD.\n");

    sdc_dbg_print("Ext-CSD:  Max Speed %d Hz.\n", SDHost[ip_idx].Card->max_dtr);

#if SDHCI_SUPPORT_eMMC
    if (SDHost[ip_idx].Card->CardType == MEMORY_eMMC) {
        sdc_dbg_print(" BOOT_INFO[228]:0x%x\n", SDHost[ip_idx].Card->EXT_CSD_MMC.BOOT_INFO);
        /* Some eMMC occupy byte 227 as second byte for BOOT_SIZE_MULTI */
        sdc_dbg_print(" BOOT_SIZE_MULT[226]:0x%x.\n", (SDHost[ip_idx].Card->EXT_CSD_MMC.Reserved6 << 8 |
                        SDHost[ip_idx].Card->EXT_CSD_MMC.BOOT_SIZE_MULTI));
        sdc_dbg_print(" SEC_COUNT[215-212]: 0x%08x.\n", SDHost[ip_idx].Card->EXT_CSD_MMC.SEC_COUNT);
        sdc_dbg_print(" HS_TIMING[185]: 0x%x.\n", SDHost[ip_idx].Card->EXT_CSD_MMC.HS_TIMING);
        sdc_dbg_print(" BUS_WIDTH[183]: 0x%x.\n", SDHost[ip_idx].Card->EXT_CSD_MMC.BUS_WIDTH);
        sdc_dbg_print(" PARTITION_CONF[179]: 0x%x.\n", SDHost[ip_idx].Card->EXT_CSD_MMC.PARTITION_CONF);
        sdc_dbg_print(" BOOT_CONFIG_PROT[178]: 0x%x.\n", SDHost[ip_idx].Card->EXT_CSD_MMC.BOOT_CONFIG_PROT);
        sdc_dbg_print(" BOOT_BUS_WIDTH[177]: 0x%x.\n", SDHost[ip_idx].Card->EXT_CSD_MMC.BOOT_BUS_WIDTH);
        sdc_dbg_print(" BOOT_BUS_WP[173]: 0x%x.\n", SDHost[ip_idx].Card->EXT_CSD_MMC.BOOT_WP);
        sdc_dbg_print(" PARTITION_SETTING_COMPLETED[155]: 0x%x.\n", SDHost[ip_idx].Card->EXT_CSD_MMC.PARTITION_SETTING_COMPLETED);

        SDHost[ip_idx].Card->numOfBootBlocks = (SDHost[ip_idx].Card->EXT_CSD_MMC.Reserved6 << 8 | SDHost[ip_idx].Card->EXT_CSD_MMC.BOOT_SIZE_MULTI) * 128 * 1024;
        SDHost[ip_idx].Card->numOfBootBlocks >>= 9;
    }
#endif
    sdc_dbg_print("**********************************************\n");

    fail:
    /* The block number of MMC card, which capacity is more than 2GB, shall be fetched from Ext-CSD. */
    if (SDHost[ip_idx].Card->CSD_MMC.CSD_STRUCTURE == 3) {
        sdc_dbg_print(" MMC EXT CSD Version 1.%d\n", SDHost[ip_idx].Card->EXT_CSD_MMC.EXT_CSD_REV);
        sdc_dbg_print(" MMC CSD Version 1.%d\n", SDHost[ip_idx].Card->EXT_CSD_MMC.CSD_STRUCTURE);
    } else {
        sdc_dbg_print("MMC CSD Version 1.%d\n", SDHost[ip_idx].Card->CSD_MMC.CSD_STRUCTURE);
    }

    if (SDHost[ip_idx].Card->CSD_MMC.C_SIZE == 0xFFF) {
        SDHost[ip_idx].Card->numOfBlocks = SDHost[ip_idx].Card->EXT_CSD_MMC.SEC_COUNT;
        SDHost[ip_idx].Card->block_addr = 1;
    } else {
        SDHost[ip_idx].Card->numOfBlocks = (SDHost[ip_idx].Card->CSD_MMC.C_SIZE + 1)
                << (SDHost[ip_idx].Card->CSD_MMC.C_SIZE_MULT + 2);
        /* Change to 512 bytes unit */
        SDHost[ip_idx].Card->numOfBlocks = SDHost[ip_idx].Card->numOfBlocks
                << (SDHost[ip_idx].Card->CSD_MMC.READ_BL_LEN - 9);
        SDHost[ip_idx].Card->block_addr = 0;
    }

    /* Index start from zero */
    SDHost[ip_idx].Card->numOfBlocks -= 1;

    return 0;
}

u32
ftsdc021_scan_cards(u8 ip_idx)            //, u8 boot_mode)
{
    //u32 resp;
    u32 ocr = 0, rocr = 0, s18r = 0, err = 0;
    u8 F8 = 0;
    u16 bit_pos = 0;

    /* It's not necessary to save from error_recovery function until the card is in transfer state.
     Because some commands are not supported in MMC/SD/SDIO in the mix-initiation.
     Error_recover function may destoried the initial procedure. */
    SDHost[ip_idx].ErrRecover = 0;

    s18r = (((gpRegSDC(ip_idx)->CapReg2 & (SDHCI_SUPPORT_SDR50 | SDHCI_SUPPORT_SDR104 | SDHCI_SUPPORT_DDR50)) ) && (!(SDHost[ip_idx].module_sts & SDHCI_MODULE_STS_SDIO_FORCE_3_3_V ))) ? 1 : 0;
    //s18r = (gpRegSDC(ip_idx)->CapReg2 & (SDHCI_SUPPORT_SDR50 | SDHCI_SUPPORT_SDR104 | SDHCI_SUPPORT_DDR50)) ? 1 : 0;
#if 0
    if (SDHost[ip_idx].Card->already_init == TRUE && SDHost[ip_idx].Card->CardType == SDIO_TYPE_CARD) {
        /* CMD 52: Reset the SDIO only card.(Write the specified bit after fetching the content.) */
        ftsdc021_send_command(ip_idx, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0,
                SDHCI_CMD_RTYPE_R1R5R6R7, 1,
                ((SD_CMD52_RW_in_R) | (SD_CMD52_no_RAW) | SD_CMD52_FUNC(0) |
                        SD_CMD52_Reg_Addr(0x06)));

        resp = (u32) gpRegSDC(ip_idx)->CmdRespLo;
        //if (dbg_print > 1)
//          sdc_dbg_print("FTSDC021: (Cmd %d) Card Status --> %x\n", cmd_index, (resp & 0xF0));

        resp = resp & 0xF;
        resp = resp | 0x8;// Setting the reset bit in 6th byte of function 0

        ftsdc021_send_command(ip_idx, SDHCI_IO_RW_DIRECT, SDHCI_CMD_TYPE_NORMAL, 0,
                SDHCI_CMD_RTYPE_R1R5R6R7, 1,
                (SD_CMD52_RW_in_W | SD_CMD52_RAW |
                        SD_CMD52_FUNC(0) | SD_CMD52_Reg_Addr(0x06) | resp));

    } else {
        s18r = (gpRegSDC(ip_idx)->CapReg2 & (SDHCI_SUPPORT_SDR50 | SDHCI_SUPPORT_SDR104 | SDHCI_SUPPORT_DDR50)) ? 1 : 0;
        reinit:

        /* CMD0 */
        if (ftsdc021_ops_go_idle_state(ip_idx, 0)) {
            return 1;
        }
#endif
    {
        SDHost[ip_idx].Card->CardType = CARD_TYPE_UNKNOWN;
//loop:   // tiger debug
        /* CMD8 of SD card, F8 is only referenced by SD and SDIO flow */
        err = ftsdc021_ops_send_if_cond(ip_idx, ((SDHost[ip_idx].ocr_avail & 0xFF8000) != 0) << 8 | 0xAA);
        F8 = err ? 0 : 1;
//        goto loop;
        /* Query for OCR
         * First we search for SDIO... CMD5
         */
        err = ftsdc021_ops_send_io_op_cond(ip_idx, 0, &ocr);
        if (!err) {
            /* It could be combo card, change it later */
            SDHost[ip_idx].Card->CardType = SDIO_TYPE_CARD;
            goto set_voltage;
        }

        /*
         * ...then normal SD... ACMD41
         */
        err = ftsdc021_ops_send_app_op_cond(ip_idx, 0, &ocr);
        if (!err) {
            SDHost[ip_idx].Card->CardType = MEMORY_CARD_TYPE_SD;
            goto set_voltage;
        }

        /*
         * ...and finally MMC. CMD1
         */
        err = ftsdc021_ops_send_op_cond(ip_idx, 0, &ocr);
        if (!err) {
            SDHost[ip_idx].Card->CardType = MEMORY_CARD_TYPE_MMC;
            goto set_voltage;
        }
    }

    set_voltage:

    if (SDHost[ip_idx].Card->CardType == CARD_TYPE_UNKNOWN) {
        sdc_dbg_print("No Supported Card found !\n");
        return 1;
    }

	//eMMC will set this bit
    if (ocr & 0x7F) {
        sdc_dbg_print("Card claims to support voltages below the defined range. These will be ignored.\n");
        ocr &= ~0x7F;
    }

	//eMMC will set this bit
    if (ocr & (1 << 7)) {
        sdc_dbg_print(" WARN## ... SD card claims to support the incompletely defined 'low voltage range'."
                "This will be ignored.\n");
        ocr &= ~(1 << 7);
    }

    /* Mask off any voltage we do not support */
    ocr &= SDHost[ip_idx].ocr_avail;

    /* Select the lowest voltage */
    for (bit_pos = 0; bit_pos < 24; bit_pos++) {
		if (ocr & (1 << bit_pos))
			break;
        }
    bit_pos = 21;

    if (bit_pos == 24) {
        sdc_dbg_print(" ERR## ... Can not find correct voltage value.\n");
        return 1;
    }


    ftsdc021_SetPower(ip_idx, bit_pos);
    ocr |= (3 << bit_pos);

    rocr = 0;
#if SDHCI_SUPPORT_eMMC
    if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC || SDHost[ip_idx].Card->CardType == MEMORY_eMMC) {
#else
    if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC) {
#endif
        /* Bit 30 indicate Host support high capacity */
        err = ftsdc021_ops_send_op_cond(ip_idx, (ocr | (1 << 30)), &rocr);  //&SDHost[ip_idx].Card->OCR);
        if (err) {
            sdc_dbg_print("MMC card init failed ... CMD1 !\n'");
            return 1;
        }

        if (((SDHost[ip_idx].Card->OCR >> 29) & 3) == 2) {
            SDHost[ip_idx].Card->block_addr = 1;
            sdc_dbg_print(" Found MMC Card .... sector mode \n");
        } else {
            SDHost[ip_idx].Card->block_addr = 0;
            sdc_dbg_print(" Found MMC Card .... byte mode \n");
        }
    } else {
        if (SDHost[ip_idx].Card->CardType == SDIO_TYPE_CARD) {
			
			//cmd5, with working voltage range of ocr
            err = ftsdc021_ops_send_io_op_cond(ip_idx, ocr, &rocr);  //SDHost[ip_idx].Card->OCR);
            /* CMD5 error, is it SD card ? */
            if (err) {
                SDHost[ip_idx].Card->CardType = MEMORY_CARD_TYPE_SD;
            } else {
                SDHost[ip_idx].Card->sdio_funcs = (rocr >> 28) & 0x7;
                SDHost[ip_idx].Card->Memory_Present = (rocr >> 27) & 0x1;
                SDHost[ip_idx].Card->OCR = rocr & 0x00FFFFFF;

                if (SDHost[ip_idx].Card->Memory_Present == 1) {
                    SDHost[ip_idx].Card->CardType = SDIO_TYPE_CARD;
                    //SDHost[ip_idx].Card->CardType = MEMORY_SDIO_COMBO;
                    sdc_dbg_print(" ERR## Combo card is not support in SDhost driver ... %d\n", rocr);

                } else {
                    SDHost[ip_idx].Card->CardType = SDIO_TYPE_CARD;
                }

                sdc_dbg_print(" Found SDIO Card .... \n");
                //sdc_dbg_print("num of IO Func = %d , memory present = %d, OCR = %x\n",SDHost[ip_idx].Card->sdio_funcs,SDHost[ip_idx].Card->Memory_Present,SDHost[ip_idx].Card->OCR);
                //ftsdc021_set_bus_speed_mode(ip_idx, 4);
            }
        }

        if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_SD) {
            if (F8 == 1) {
                ocr |= ((s18r << 24) | (1 << 30));
            }

            if (ftsdc021_ops_send_app_op_cond(ip_idx, ocr, &SDHost[ip_idx].Card->OCR)) {
                sdc_dbg_print(" SD Card init failed ... ACMD41\n");
                return 1;
            }

            if (SDHost[ip_idx].Card->OCR & (1 << 30)) {
            	sdc_dbg_print(" Found SD Memory Card (SDHC or SDXC).\n");
            } else {
                sdc_dbg_print(" Found SD Memory Card Version 1.X.\n");
            }

		//lyt: change 3.3v voltage to 1.8v
	    if(SDHost[ip_idx].Card->OCR & (0x1 << 24)){
                sdc_dbg_print(" SD card is changging signal voltage to 1.8v\n");
			
                sdc_dbg_print(" PresentState0:%x\n", (gpRegSDC(ip_idx)->PresentState)>>20);
                //change signal voltae to 1.8v
                ftsdc021_ops_send_voltage_switch(ip_idx);

                sdc_dbg_print(" PresentState1:%x\n", (gpRegSDC(ip_idx)->PresentState)>>20);

                //disable clock
                gpRegSDC(ip_idx)->ClkCtl &= ~(0x1<<2);
	
                //enable host 1.8v
                gpRegSDC(ip_idx)->HostCtrl2 = gpRegSDC(ip_idx)->HostCtrl2 | SDHCI_18V_SIGNAL;

                ftsdc021_delay(6);

                //enable clock
                gpRegSDC(ip_idx)->ClkCtl |= (0x1<<2);

                //wait for clock stable
                ftsdc021_delay(1);

                for(int i=0; i<10; i++)
                	sdc_dbg_print(" PresentState2:%x\n", (gpRegSDC(ip_idx)->PresentState)>>20);
	    	}
        }
    }

    if (SDHost[ip_idx].Card->CardType != SDIO_TYPE_CARD) {
        // Commented by MikeYeh 081127
        /* Sending the CMD2 to all card to get each CID number */
        /* CMD 2 */
        err = ftsdc021_ops_all_send_cid(ip_idx);
        if (err){
        	sdc_dbg_print("CMD2 ERROR\n");
            return err;
        }
    }
#if SDHCI_SUPPORT_eMMC
    SDHost[ip_idx].Card->RCA =
    (SDHost[ip_idx].Card->CardType != MEMORY_CARD_TYPE_MMC && SDHost[ip_idx].Card->CardType != MEMORY_eMMC) ? 0 : 2;
#else
    SDHost[ip_idx].Card->RCA =
            (SDHost[ip_idx].Card->CardType != MEMORY_CARD_TYPE_MMC) ? 0 : 2;
#endif
    if (ftsdc021_ops_send_rca(ip_idx)) {
    	sdc_dbg_print("ERR## Card init failed ... CMD3 !\n");
        return 1;
    }
    //SDHost[ip_idx].Card->respLo = (u32) (gpRegSDC(ip_idx)->CmdRespLo);
    SDHost[ip_idx].Card->respLo = gpRegSDC(ip_idx)->CmdRespLo;

#if SDHCI_SUPPORT_eMMC
    if ((SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_SD) ||
            (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC) ||
            (SDHost[ip_idx].Card->CardType == MEMORY_eMMC)) {
#else
    if ((SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_SD) ||
            (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC)) {

#endif
        u32 e, m;

        if (ftsdc021_ops_send_csd(ip_idx)) {
        	sdc_dbg_print("ERR## Card init failed ... CMD9 !\n");
            return 1;
        }

        SDHost[ip_idx].Card->wblk_sz_2exp = 9;
        SDHost[ip_idx].Card->rblk_sz_2exp = 9;
        sdc_dbg_print("**************** CSD register ****************\n");
        if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_SD) {
            if ((SDHost[ip_idx].Card->CSD_HI >> 54) == 0) {
			//sdsc
                SDHost[ip_idx].Card->numOfBlocks =
                        (SDHost[ip_idx].Card->CSD_ver_1.C_SIZE + 1) << (SDHost[ip_idx].Card->CSD_ver_1.C_SIZE_MULT + 2);

                sdc_dbg_print(" CSD Ver 1.0 \n");

                e = SDHost[ip_idx].Card->CSD_ver_1.TRAN_SPEED & 7;
                m = (SDHost[ip_idx].Card->CSD_ver_1.TRAN_SPEED >> 3) & 0xF;

                SDHost[ip_idx].Card->block_addr = 0;
                SDHost[ip_idx].Card->erase_sector_size = SDHost[ip_idx].Card->CSD_ver_1.SECTOR_SIZE + 1;
                SDHost[ip_idx].Card->rblk_sz_2exp = SDHost[ip_idx].Card->CSD_ver_1.READ_BL_LEN;
                SDHost[ip_idx].Card->wblk_sz_2exp = SDHost[ip_idx].Card->CSD_ver_1.WRITE_BL_LEN;

            } else if ((SDHost[ip_idx].Card->CSD_HI >> 54) == 1) {
			//sdhc sdxc sduc

                SDHost[ip_idx].Card->numOfBlocks = (SDHost[ip_idx].Card->CSD_ver_2.C_SIZE + 1) << 10;
                sdc_dbg_print(" CSD Ver 2.0\n");

                e = SDHost[ip_idx].Card->CSD_ver_2.TRAN_SPEED & 7;
                m = (SDHost[ip_idx].Card->CSD_ver_2.TRAN_SPEED >> 3) & 0xF;
                SDHost[ip_idx].Card->erase_sector_size = SDHost[ip_idx].Card->CSD_ver_2.SECTOR_SIZE + 1;
                SDHost[ip_idx].Card->block_addr = 1;
                SDHost[ip_idx].Card->rblk_sz_2exp = SDHost[ip_idx].Card->CSD_ver_2.READ_BL_LEN;
                SDHost[ip_idx].Card->wblk_sz_2exp = SDHost[ip_idx].Card->CSD_ver_2.WRITE_BL_LEN;
            } else {
                SDHost[ip_idx].Card->numOfBlocks = 0;
                sdc_dbg_print("Reserve\n");
                return 1;
            }

            if(SDHost[ip_idx].Card->rblk_sz_2exp > 9)
            {
                SDHost[ip_idx].Card->numOfBlocks *= (1 << (SDHost[ip_idx].Card->rblk_sz_2exp - 9));
            }
            SDHost[ip_idx].Card->rblk_sz_2exp = (SDHost[ip_idx].Card->rblk_sz_2exp > 9) ? 9 : SDHost[ip_idx].Card->rblk_sz_2exp;
            SDHost[ip_idx].Card->wblk_sz_2exp = (SDHost[ip_idx].Card->wblk_sz_2exp > 9) ? 9 : SDHost[ip_idx].Card->wblk_sz_2exp;

            SDHost[ip_idx].Card->read_block_len = (1 << SDHost[ip_idx].Card->rblk_sz_2exp);
            SDHost[ip_idx].Card->write_block_len = (1 << SDHost[ip_idx].Card->wblk_sz_2exp);
            sdc_dbg_print("block num = %d, block length = %d / %d bytes, erase sector size =%d\n",
                    SDHost[ip_idx].Card->numOfBlocks,
                    SDHost[ip_idx].Card->write_block_len, SDHost[ip_idx].Card->read_block_len,
                    SDHost[ip_idx].Card->erase_sector_size);
            SDHost[ip_idx].Card->capacity = SDHost[ip_idx].Card->numOfBlocks;
            SDHost[ip_idx].Card->capacity *= 512;
            sdc_dbg_print("capacity = %lld , %llx \n", SDHost[ip_idx].Card->capacity, SDHost[ip_idx].Card->capacity);

            SDHost[ip_idx].Card->max_dtr = tran_exp[e] * tran_mant[m];
            sdc_dbg_print(" Current Max Transfer Speed %d Hz.\n", SDHost[ip_idx].Card->max_dtr);

            /* Index start from zero */
            SDHost[ip_idx].Card->numOfBlocks -= 1;
        } else {
		
			//all eMMC should support 512B as blocksize			
	    	SDHost[ip_idx].Card->wblk_sz_2exp = 9;
            SDHost[ip_idx].Card->rblk_sz_2exp = 9;
            SDHost[ip_idx].Card->read_block_len = (1 << SDHost[ip_idx].Card->rblk_sz_2exp);
            SDHost[ip_idx].Card->write_block_len = (1 << SDHost[ip_idx].Card->wblk_sz_2exp);
            sdc_dbg_print("C_SIZE %d, CSIZE_MULT %d, RD_BL_LEN %d.\n", SDHost[ip_idx].Card->CSD_MMC.C_SIZE,
                    SDHost[ip_idx].Card->CSD_MMC.C_SIZE_MULT, SDHost[ip_idx].Card->CSD_MMC.READ_BL_LEN);
        }
        sdc_dbg_print("**********************************************\n");

        if (ftsdc021_ops_send_card_status(ip_idx)) {
            sdc_dbg_print("ERR## Get Card Status Failed after CMD9 !\n");
            return 1;
        }
        sdc_dbg_print("FTSDC021: (Cmd %d) Card Status --> %s\n", SDHost[ip_idx].cmd_index,
                SDC_ShowCardState((Card_State )((SDHost[ip_idx].Card->respLo >> 9) & 0xF)));
    }

    if ((((SDHost[ip_idx].Card->respLo >> 9) & 0xF) == CUR_STATE_STBY)
            || SDHost[ip_idx].Card->CardType == SDIO_TYPE_CARD) {

        if (ftsdc021_ops_select_card(ip_idx)) {
            sdc_dbg_print("ERR## Card init failed ... CMD7 !\n");
            return 1;
        }
    } else {
        sdc_dbg_print("Initialization is failed due to state changing from stand-by to transfer\n");
        return 1;
    }

    /* enable error recovery */
    SDHost[ip_idx].ErrRecover = 0;

    if (SDHost[ip_idx].Card->CardType != SDIO_TYPE_CARD) {
        if (ftsdc021_ops_send_card_status(ip_idx)) {
            sdc_dbg_print("ERR## Get Card Status Failed (Init) !\n");
            return 1;
    	}
        sdc_dbg_print_level(1, "FTSDC021: (Cmd %d) Card Status --> %s\n", SDHost[ip_idx].cmd_index,
                SDC_ShowCardState((Card_State)((SDHost[ip_idx].Card->respLo >> 9) & 0xF)));
    }

    return 0;
}


void
ftsdc021_intr_en(u8 ip_idx, u8 is_init)
{
    if (is_init) {
        gpRegSDC(ip_idx)->IntrEn = SDHCI_INTR_EN_INIT;
        gpRegSDC(ip_idx)->ErrEn = SDHCI_ERR_EN_INIT;
        gpRegSDC(ip_idx)->IntrSigEn = SDHCI_INTR_EN_INIT;
        gpRegSDC(ip_idx)->ErrSigEn = SDHCI_ERR_EN_INIT;
    } else {
        gpRegSDC(ip_idx)->IntrEn = SDHCI_INTR_EN_ALL;
        gpRegSDC(ip_idx)->ErrEn = SDHCI_ERR_EN_ALL;
        gpRegSDC(ip_idx)->IntrSigEn = SDHCI_INTR_EN_ALL;
        gpRegSDC(ip_idx)->ErrSigEn = SDHCI_ERR_EN_ALL;
    }
}

#define SDIO_CLK_N     1     /* clk*n/m */
#define SDIO_CLK_M     1     /* clk*n/m */
#define SDIO_CLK_SEL   1 //1     /* 1:syspll, 0:xtal */
#if SDIO_CLK_SEL
#define SDIO_CLK_SRC   (100 * 1000 * 1000)
#else
#define SDIO_CLK_SRC   (12 * 1000 * 1000)
#endif

u32
ftsdc021_get_base_clk()
{
    u32 base;

    base = SDIO_CLK_SRC * SDIO_CLK_N / (SDIO_CLK_M);
    return base;
}

#include "sdioh_reg.h"
u32
ftsdc021_init_normal(u8 ip_idx)
{
    u32 clk, i;

    sdc_dbg_print(" - Host Controller Version: 0x%02x(0x%08x).\n", gpRegSDC(ip_idx)->HCVer,
            gpRegSDC(ip_idx)->IpVersion);
    SDHost[ip_idx].sdcard_init_complete = 0;
    //SDHost[ip_idx].Card = (SDCardInfo*)(c+ ip_idx *512);
    //memset(SDHost[ip_idx].Card, 0, 512);
    SDHost[ip_idx].Card->FlowSet.Erasing = 0;
    SDHost[ip_idx].Card->FlowSet.autoCmd = 0;
    SDHost[ip_idx].sdio_card_int = NULL;
    //SDHost[ip_idx].invert = 0;
    SDHost[ip_idx].ErrRecover = 0;
    SDHost[ip_idx].clock = 0;
    SDHost[ip_idx].ocr_avail = 0;

    //TODO: arcs_c0 IP_SYSCTRL->REG_SW_RESET0.bit.SDIOH_RESET = 0x1;
    __COMPILER_BARRIER();
	IP_SDIOH->REG_CCR_TCR_SRR.bit.SD_CLK_EN       = 0x1;
	IP_SDIOH->REG_HC1_PCR_BGCR.bit.SD_BUS_POW     = 0x1; // Set SD power enable
	IP_SDIOH->REG_HC1_PCR_BGCR.bit.SD_BUS_VOL     = 0x3; // Set SD power enable
	IP_SDIOH->REG_VR1.bit.LO_SD_RSTN     = 0x0; // Release Reset signal
	IP_SDIOH->REG_VR1.bit.LO_SD_RSTN     = 0x1; // Release Reset signal
    __COMPILER_BARRIER();

    //TODO: arcs_c0 IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.DIV_SDIOH_CLK_LD = 0;
    //TODO: arcs_c0 IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.SEL_SDIOH_CLK = SDIO_CLK_SEL;
    //TODO: arcs_c0 IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.DIV_SDIOH_CLK_M = SDIO_CLK_M;
    //TODO: arcs_c0 IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.DIV_SDIOH_CLK_N = SDIO_CLK_N;
    __COMPILER_BARRIER();
    //TODO: arcs_c0 IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.ENA_SDIOH_CLK = 1;
    __COMPILER_BARRIER();
    //TODO: arcs_c0 IP_SYSCTRL->REG_PERI_CLK_CFG3.bit.DIV_SDIOH_CLK_LD = 1;
    __COMPILER_BARRIER();




    /* Reset the controller */
    gpRegSDC(ip_idx)->SoftRst |= (SDHCI_SOFTRST_CMD | SDHCI_SOFTRST_DAT);
    for (i = 0; i < 100; i++) {
        if ((gpRegSDC(ip_idx)->SoftRst & (SDHCI_SOFTRST_CMD | SDHCI_SOFTRST_DAT)) == 0)
            break;
        ftsdc021_delay(1);
    }
    if(i == 100){
    	sdc_dbg_print("Host reset fail\n");
    }

    clk = ftsdc021_get_base_clk();
    SDHost[ip_idx].max_clk = clk;
    SDHost[ip_idx].min_clk = 360000;

	gpRegSDC(ip_idx)->CapReg2 |= 7;

    if (gpRegSDC(ip_idx)->CapReg & SDHCI_CAP_VOLTAGE_33V)
        SDHost[ip_idx].ocr_avail = (3 << 21);

    if (gpRegSDC(ip_idx)->CapReg & SDHCI_CAP_VOLTAGE_30V)
        SDHost[ip_idx].ocr_avail |= (3 << 17);

    sdc_dbg_print(" - Base Clock Frequency: Min %d Hz, Max %d Hz.\n", SDHost[ip_idx].min_clk, SDHost[ip_idx].max_clk);
    sdc_dbg_print(" - Max Block Length: 0x%x bytes.\n", 0x200 << ((gpRegSDC(ip_idx)->CapReg >> 16) & 0x3));
    sdc_dbg_print(" - 8-bit: %s, ADMA2: %s, HS: %s, SDMA: %s, S/R: %s, Voltage: 0x%x,SDR50/104,DDR50: 0x%x.\n\n",
            ((gpRegSDC(ip_idx)->CapReg >> 18) & 1) ? "Yes" : "No",
            ((gpRegSDC(ip_idx)->CapReg >> 19) & 1) ? "Yes" : "No",
            ((gpRegSDC(ip_idx)->CapReg >> 21) & 1) ? "Yes" : "No",
            ((gpRegSDC(ip_idx)->CapReg >> 22) & 1) ? "Yes" : "No",
            ((gpRegSDC(ip_idx)->CapReg >> 23) & 1) ? "Yes" : "No", ((gpRegSDC(ip_idx)->CapReg >> 24) & 7),
            (gpRegSDC(ip_idx)->CapReg2 & 7));
	//the host's configure is check done, begin the init of device

    /* Support SDR50, SDR104 and DDR50, we might request wot switch 1.8V IO signal */
	/*enable the 3.3v power */
	ftsdc021_SetPower(ip_idx, 21);

    /* Clock must be < 400KHz for initialization */
	ftsdc021_SetSDClock(ip_idx, SDHost[ip_idx].min_clk);

    if (gpRegSDC(ip_idx)->VendorReg7 & 0x1C) {
        /* FIFO use SRAM: 1K, 2K or 4K */
        SDHost[ip_idx].Card->fifo_depth = 512;
    } else if (gpRegSDC(ip_idx)->VendorReg7 & 0x2)
        SDHost[ip_idx].Card->fifo_depth = 16 << 2;
    else if (gpRegSDC(ip_idx)->VendorReg7 & 0x1)
        SDHost[ip_idx].Card->fifo_depth = 8 << 2;

    sdc_dbg_print(" - FIFO depth %d bytes.\n", SDHost[ip_idx].Card->fifo_depth);

    /* Initialize timer */

    /* set timeout ctl to the max*/
    gpRegSDC(ip_idx)->TimeOutCtl = 14;
    SDHost[ip_idx].Card->FlowSet.timeout_ms = (1 << (gpRegSDC(ip_idx)->TimeOutCtl + 13)) / clk;
    if (SDHost[ip_idx].Card->FlowSet.timeout_ms == 0)
        SDHost[ip_idx].Card->FlowSet.timeout_ms = 14;
    SDHost[ip_idx].Card->FlowSet.timeout_ms *= 1000;
    sdc_dbg_print("clk = %d , imeount ms = %d\n", clk,SDHost[ip_idx].Card->FlowSet.timeout_ms);


	//Enable the host's IRQ in cpu system 
//        OS_CPU_Vector_Table[IRQ_SDHC_VECTOR] = SDHC_ISR;
    register_ISR(SDC_FTSDC021_0_IRQ, ftsdc021_0_IntrHandler, NULL);
//    __nds32__set_int_priority(NDS32_HWINT(IRQ_SDHC_VECTOR), 0);
//    /* enable HW# (timer1, GPIO & SWI) */
//    __nds32__enable_int(NDS32_HWINT(IRQ_SDHC_VECTOR));
//    /* Interrupt pending register, write 1 to clear */
//    __nds32__mtsr(IC_SDHC, NDS32_SR_INT_PEND2);
    enable_IRQ(SDC_FTSDC021_0_IRQ);

    //ftsdc021_intr_en(ip_idx, 1);

    /* Optionally enable interrupt signal to CPU */
    //ftsdc021_set_transfer_type(ip_idx,PIO, 4);
    //if(ip_idx == 0)
    ftsdc021_set_transfer_type(ip_idx, SDMA, 4);  //tiger debug
    //else
    //ftsdc021_set_transfer_type(ip_idx, ADMA, 0);
    sdc_dbg_print(" Checking card ... ");
    if (ftsdc021_CheckCardInsert(ip_idx)) {
        sdc_dbg_print("Inserted.\n");
        ftsdc021_delay(1);
    } else {
        sdc_dbg_print("Not Inserted.\n");
    }
    /* Do not require to enable Error Interrupt Signal.
     * Bit 15 of Normal Interrupt Status will tell if
     * error happens.
     */

    sdc_dbg_print("Host Init OK\n");


    SDHost[ip_idx].Card->already_init = true;
    return (u32) ERR_SD_NO_ERROR;
}

u32
ftsdc021_init(u8 ip_idx)
{
    if((SDHost[ip_idx].sd_init[SDC_PS_FUNC_NORMAL] == NULL)&&(SDHost[ip_idx].sd_init[SDC_PS_FUNC_RESTORE] == NULL))
        return ftsdc021_init_normal(ip_idx);

//  if(SDHost[ip_idx].sd_init[SDC_PS_FUNC_RESTORE] == NULL)
    //  return SDHost[ip_idx].sd_init[SDC_PS_FUNC_NORMAL](ip_idx);

    return 0;
}
u32
ftsdc021_card_writable(u8 ip_idx)
{
    return (gpRegSDC(ip_idx)->PresentState & SDHCI_STS_CARD_WRITABLE);
}
