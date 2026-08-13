#include <time.h>
//#include "castor.h"
#include "ftsdc021.h"
#include "ftsdc021_sdio.h"

/**
 * SDIO card related operations
 */
extern volatile ftsdc021_reg*
gpRegSDC(u8 ip_idx);
extern clock_t
sdc_jiffies(void);



/* SDIO CMD5 */
u32
ftsdc021_ops_send_io_op_cond(u8 ip_idx, u32 ocr, u32* rocr)
{
    u32 i, err = 0;
    SDCardInfo* card = SDHost[ip_idx].Card;

    for (i = 100; i; i--) {
        err = ftsdc021_send_command(ip_idx, SDHCI_IO_SEND_OP_COND, SDHCI_CMD_TYPE_NORMAL, 0,
                                    SDHCI_CMD_RTYPE_R3R4, 00, 0x30ff0000);

        //err = ftsdc021_send_command(ip_idx, SDHCI_IO_SEND_OP_COND, SDHCI_CMD_TYPE_NORMAL, 0,
        //                            SDHCI_CMD_RTYPE_R3R4, 0, 0xff00ff00);
        if (err)
            break;

        /* if we're just probing, do a single pass */
        if (ocr == 0)
            break;

        if (card->respLo & MMC_CARD_BUSY)
            break;

        err = 1;

        ftsdc021_delay(100);
    }

    if (rocr)
        *rocr = card->respLo;


    return err;
}


/**
 * MMC card related operations
 */

/* MMC CMD1 */
u32
ftsdc021_ops_send_op_cond(u8 ip_idx, u32 ocr, u32* rocr)
{
    u32 i, err = 0;
    SDCardInfo* card = SDHost[ip_idx].Card;
    for (i = 1000; i; i--) {
        err = ftsdc021_send_command(ip_idx, MMC_SEND_OP_COND, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R3R4, 0, ocr);
        if (err)
            break;

        /* if we're just probing, do a single pass */
        if (ocr == 0)
            break;

        /* otherwise wait until reset completes */
        if (card->respLo & MMC_CARD_BUSY)
            break;

        err = 1;

        ftsdc021_delay(10);
    }

    if (rocr)
        *rocr = card->respLo;

    return err;
}

u32
ftsdc021_ops_mmc_switch(u8 ip_idx, u8 set, u8 index, u8 value)
{
    u32 err;
    u32 arg;
    SDCardInfo* card = SDHost[ip_idx].Card;
    if (card->CardType != MEMORY_CARD_TYPE_MMC) {
        sdc_dbg_print("Switch Function: This is not MMC Card !\n");
        return 1;
    }

    arg = (EXT_CSD_Write_byte << 24) | (index << 16) | (value << 8) | set;

    err = ftsdc021_send_command(ip_idx, SDHCI_MMC_SWITCH, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1BR5B, 1, arg);
    if (err)
        return err;

    return 0;
}

u32
ftsdc021_ops_send_ext_csd(u8 ip_idx)
{

    SDCardInfo* card = SDHost[ip_idx].Card;
    if ((SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC) &&
        SDHost[ip_idx].Card->CSD_MMC.SPEC_VERS < 4) {
        sdc_dbg_print(" Commmand 8 is not supported in this MMC system spec.\n");
        return 0;
    }

    memset(&card->EXT_CSD_MMC, 0, EXT_CSD_LENGTH);

    ftsdc021_set_transfer_mode(ip_idx, 0, 0, SDHCI_TXMODE_READ_DIRECTION, 0);

    ftsdc021_prepare_data(ip_idx, 1, EXT_CSD_LENGTH, (u32) & (card->EXT_CSD_MMC), READ);

    /* CMD 8 */
    if (ftsdc021_send_command(ip_idx, SDHCI_SEND_EXT_CSD, SDHCI_CMD_TYPE_NORMAL, 1, SDHCI_CMD_RTYPE_R1R5R6R7, 0, 0)) {
        sdc_dbg_print("Getting the Ext-CSD failed\n");
        return 1;
    }

    return ftsdc021_transfer_data(ip_idx, READ, (u32*) & (card->EXT_CSD_MMC), EXT_CSD_LENGTH);

}

/**
 * MMC4.4:
 * argument = 0x00000000, Resets the card to idle state.
 * argument = 0xF0F0F0F0, Resets the card to pre-idle state.
 * argument = 0xFFFFFFFA, Initiate alternative boot operation.
 *
 * SD Card: Argument always 0x00000000.
 */
u32
ftsdc021_ops_go_idle_state(u8 ip_idx, u32 arg)
{
    u8 data;

    data = (arg == 0xFFFFFFFA) ? 1 : 0;

    return ftsdc021_send_command(ip_idx, SDHCI_GO_IDLE_STATE, SDHCI_CMD_TYPE_NORMAL, data, SDHCI_CMD_NO_RESPONSE, 0, arg);
}

/**
 * SD card related operations
 */

/* CMD8 SD card */
u32
ftsdc021_ops_send_if_cond(u8 ip_idx, u32 arg)
{
    u32 err;
    u8 test_pattern = (arg & 0xFF);
    u8 result_pattern;
    SDCardInfo* card = SDHost[ip_idx].Card;

    err = ftsdc021_send_command(ip_idx, SDHCI_SEND_IF_COND, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, arg);
    if (err)
        return err;

    result_pattern = card->respLo & 0xFF;

    if (result_pattern != test_pattern)
        return 1;

    return 0;
}

/* ACMD41 */
u32
ftsdc021_ops_send_app_op_cond(u8 ip_idx, u32 ocr, u32* rocr)
{
    u32 i, err = 0;
    SDCardInfo* card = SDHost[ip_idx].Card;
    for (i = 1000; i; i--) {
        /* CMD 55: Indicate to the card the next cmd is app-specific command */
        err = ftsdc021_send_command(ip_idx, SDHCI_APP_CMD, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, 0);
        if (err)
            break;

        err = ftsdc021_send_command(ip_idx, SDHCI_SD_SEND_OP_COND, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R3R4,
                                    0, ocr);
        if (err)
            break;

        /* if we're just probing, do a single pass */
        if (ocr == 0)
            break;

        if (card->respLo & MMC_CARD_BUSY)
            break;

        err = 1;

        ftsdc021_delay(1);
    }

    if (rocr)
        *rocr = card->respLo;

    return err;
}

/* CMD11 R1 */
u32
ftsdc021_ops_send_voltage_switch(u8 ip_idx)
{
    u32 err;

    err = ftsdc021_send_command(ip_idx, SDHCI_VOLTAGE_SWITCH, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, 0);
    if (err)
        return err;

    return 0;
}

u32

ftsdc021_ops_all_send_cid(u8 ip_idx)
{
    SDCardInfo* card = SDHost[ip_idx].Card;
    if (ftsdc021_send_command(ip_idx, SDHCI_SEND_ALL_CID, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R2, 1, 0)) {
        sdc_dbg_print("ALL SEND CID failed !\n");
        return 1;
    }
    card->CID_LO = card->respLo;
    card->CID_HI = card->respHi;

    sdc_dbg_print("**************** CID register ****************\n");
    if (card->CardType == MEMORY_CARD_TYPE_SD) {
        sdc_dbg_print("Manufacturer ID: 0x%02x\n", (u32)(card->CID_HI >> 48));
        sdc_dbg_print("OEM / App ID: 0x%04x\n", (u32)((card->CID_HI >> 32) & 0xFFFF));
//      sdc_dbg_print("Product Name: 0x%02x\n",
//              (u32)((((card->CID_HI) & 0xFFFFFFFF) << 8) | (card->CID_LO >> 56) & 0xFF));
        sdc_dbg_print("Product Revision: %d.%d\n", (u8)((card->CID_LO >> 52)) & 0xF, (u8)((card->CID_LO >> 48) & 0xF));
        sdc_dbg_print("Product Serial No.: %u\n", (u32)((card->CID_LO >> 16) & 0xFFFFFFFF));
        sdc_dbg_print("Reserved:0x%x\n", (u32)((card->CID_LO & 0xF000) >> 12));
        //sdc_dbg_print("Manufacturer Date: %lld - %0.2d\n",
        // ((card->CID_LO & 0xFF0) >> 4) + 2000, (u8)((card->CID_LO & 0xF)));
    } else {
        sdc_dbg_print("Manufacturer ID: 0x%02x\n", (u32)(card->CID_HI >> 48));
        sdc_dbg_print("OEM / App ID: 0x%04x\n", (u32)((card->CID_HI >> 32) & 0xFFFF));
        //sdc_dbg_print("Product Name: 0x%02x\n",
        //    (u32)((((card->CID_HI) & 0xFFFFFFFF) << 16) | (card->CID_LO >> 48) & 0xFFFF));
        sdc_dbg_print("Product Revision: %d.%d\n", (u32)((card->CID_LO >> 44) & 0xF), (u32)((card->CID_LO >> 40) & 0xF));
        sdc_dbg_print("Product Serial No.: %u\n", (u32)((card->CID_LO >> 8) & 0xFFFFFFFF));
        //sdc_dbg_print("Manufacturer Date: %d - %0.2d\n",
        //    (u32)(((card->CID_LO & 0xF) + 1997)), (u32)((card->CID_LO & 0xF0) >> 4));
    }

    return 0;
}

u32
ftsdc021_ops_send_rca(u8 ip_idx)
{
    u32 i, err = 0;
    SDCardInfo* card = SDHost[ip_idx].Card;
    for (i = 100; i; i--) {
        err = ftsdc021_send_command(ip_idx, SDHCI_SEND_RELATIVE_ADDR, SDHCI_CMD_TYPE_NORMAL, 0,
                                    SDHCI_CMD_RTYPE_R1R5R6R7, 0, (card->RCA << 16));
        if (err)
            break;

        /* MMC card, Host assign RCA to card.
         * SD card, Host ask for RCA.
         */
        if (card->RCA != 0)
            break;

        if ((card->respLo >> 16) & 0xffff)
            break;

        err = 1;

        ftsdc021_delay(10);
    }

    if (!err)
        if (!card->RCA)
            card->RCA = (u16)((card->respLo >> 16) & 0xffff);

    return 0;
}

/* CMD9: Send CSD */
u32
ftsdc021_ops_send_csd(u8 ip_idx)
{
    u32 err;
    SDCardInfo* card = SDHost[ip_idx].Card;
    /* CMD 9: Getting the CSD register from SD memory card */
    err = ftsdc021_send_command(ip_idx, SDHCI_SEND_CSD, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R2, 0, card->RCA << 16);
    if (err)
        return err;

    card->CSD_LO = card->respLo;
    card->CSD_HI = card->respHi;

    if (card->CardType == MEMORY_CARD_TYPE_SD) {
        if ((card->CSD_HI >> 54) == 0) {
            card->CSD_ver_1.CSD_STRUCTURE = (card->CSD_HI >> 54) & 0x3;
            card->CSD_ver_1.Reserved1 = (card->CSD_HI >> 48) & 0x3F;
            card->CSD_ver_1.TAAC = (card->CSD_HI >> 40) & 0xFF;
            card->CSD_ver_1.NSAC = (card->CSD_HI >> 32) & 0xFF;
            card->CSD_ver_1.TRAN_SPEED = (card->CSD_HI >> 24) & 0xFF;
            card->CSD_ver_1.CCC = (card->CSD_HI >> 12) & 0xFFF;
            card->CSD_ver_1.READ_BL_LEN = (card->CSD_HI >> 8) & 0xF;
            card->CSD_ver_1.READ_BL_PARTIAL = (card->CSD_HI >> 7) & 0x1;
            card->CSD_ver_1.WRITE_BLK_MISALIGN = (card->CSD_HI >> 6) & 0x1;
            card->CSD_ver_1.READ_BLK_MISALIGN = (card->CSD_HI >> 5) & 0x1;
            card->CSD_ver_1.DSR_IMP = (card->CSD_HI >> 4) & 0x1;
            card->CSD_ver_1.Reserved2 = (card->CSD_HI >> 2) & 0x3;
            card->CSD_ver_1.C_SIZE = (((card->CSD_HI & 0x3) << 10) | ((card->CSD_LO >> 54) & 0x3FF));
            card->CSD_ver_1.VDD_R_CURR_MIN = (card->CSD_LO >> 51) & 0x7;
            card->CSD_ver_1.VDD_R_CURR_MAX = (card->CSD_LO >> 48) & 0x7;
            card->CSD_ver_1.VDD_W_CURR_MIN = (card->CSD_LO >> 45) & 0x7;
            card->CSD_ver_1.VDD_W_CURR_MAX = (card->CSD_LO >> 42) & 0x7;
            card->CSD_ver_1.C_SIZE_MULT = (card->CSD_LO >> 39) & 0x7;
            card->CSD_ver_1.ERASE_BLK_EN = (card->CSD_LO >> 38) & 0x1;
            card->CSD_ver_1.SECTOR_SIZE = (card->CSD_LO >> 31) & 0x7F;
            card->CSD_ver_1.WP_GRP_SIZE = (card->CSD_LO >> 24) & 0x7F;
            card->CSD_ver_1.WP_GRP_ENABLE = (card->CSD_LO >> 23) & 0x1;
            card->CSD_ver_1.Reserved3 = (card->CSD_LO >> 21) & 0x3;
            card->CSD_ver_1.R2W_FACTOR = (card->CSD_LO >> 18) & 0x7;
            card->CSD_ver_1.WRITE_BL_LEN = (card->CSD_LO >> 14) & 0xF;
            card->CSD_ver_1.WRITE_BL_PARTIAL = (card->CSD_LO >> 13) & 0x1;
            card->CSD_ver_1.Reserved4 = (card->CSD_LO >> 8) & 0x1F;
            card->CSD_ver_1.FILE_FORMAT_GRP = (card->CSD_LO >> 7) & 0x1;
            card->CSD_ver_1.COPY = (card->CSD_LO >> 6) & 0x1;
            card->CSD_ver_1.PERM_WRITE_PROTECT = (card->CSD_LO >> 5) & 0x1;
            card->CSD_ver_1.TMP_WRITE_PROTECT = (card->CSD_LO >> 4) & 0x1;
            card->CSD_ver_1.FILE_FORMAT = (card->CSD_LO >> 2) & 0x3;
            card->CSD_ver_1.Reserver5 = (card->CSD_LO) & 0x3;
        } else if ((card->CSD_HI >> 54) == 1) {
            card->CSD_ver_2.CSD_STRUCTURE = (card->CSD_HI >> 54) & 0x3;
            card->CSD_ver_2.Reserved1 = (card->CSD_HI >> 48) & 0x3F;
            card->CSD_ver_2.TAAC = (card->CSD_HI >> 40) & 0xFF;
            card->CSD_ver_2.NSAC = (card->CSD_HI >> 32) & 0xFF;
            card->CSD_ver_2.TRAN_SPEED = (card->CSD_HI >> 24) & 0xFF;
            card->CSD_ver_2.CCC = (card->CSD_HI >> 12) & 0xFFF;
            card->CSD_ver_2.READ_BL_LEN = (card->CSD_HI >> 8) & 0xF;
            card->CSD_ver_2.READ_BL_PARTIAL = (card->CSD_HI >> 7) & 0x1;
            card->CSD_ver_2.WRITE_BLK_MISALIGN = (card->CSD_HI >> 6) & 0x1;
            card->CSD_ver_2.READ_BLK_MISALIGN = (card->CSD_HI >> 5) & 0x1;
            card->CSD_ver_2.DSR_IMP = (card->CSD_HI >> 4) & 0x1;
            card->CSD_ver_2.Reserved2 = (((card->CSD_HI >> 2) & 0x3) << 2) | ((card->CSD_LO >> 62) & 0x3);
            card->CSD_ver_2.C_SIZE = ((card->CSD_LO >> 40) & 0x3FFFFF);
            card->CSD_ver_2.Reserved3 = (card->CSD_LO >> 39) & 0x1;
            card->CSD_ver_2.ERASE_BLK_EN = (card->CSD_LO >> 38) & 0x1;
            card->CSD_ver_2.SECTOR_SIZE = (card->CSD_LO >> 31) & 0x7F;
            card->CSD_ver_2.WP_GRP_SIZE = (card->CSD_LO >> 24) & 0x7F;
            card->CSD_ver_2.WP_GRP_ENABLE = (card->CSD_LO >> 23) & 0x1;
            card->CSD_ver_2.Reserved4 = (card->CSD_LO >> 21) & 0x3;
            card->CSD_ver_2.R2W_FACTOR = (card->CSD_LO >> 18) & 0x7;
            card->CSD_ver_2.WRITE_BL_LEN = (card->CSD_LO >> 14) & 0xF;
            card->CSD_ver_2.WRITE_BL_PARTIAL = (card->CSD_LO >> 13) & 0x1;
            card->CSD_ver_2.Reserved5 = (card->CSD_LO >> 8) & 0x1F;
            card->CSD_ver_2.FILE_FORMAT_GRP = (card->CSD_LO >> 7) & 0x1;
            card->CSD_ver_2.COPY = (card->CSD_LO >> 6) & 0x1;
            card->CSD_ver_2.PERM_WRITE_PROTECT = (card->CSD_LO >> 5) & 0x1;
            card->CSD_ver_2.TMP_WRITE_PROTECT = (card->CSD_LO >> 4) & 0x1;
            card->CSD_ver_2.FILE_FORMAT = (card->CSD_LO >> 2) & 0x3;
            card->CSD_ver_2.Reserver6 = (card->CSD_LO) & 0x3;
        }
#if SDHCI_SUPPORT_eMMC
    } else if (card->CardType == MEMORY_CARD_TYPE_MMC || card->CardType == MEMORY_eMMC) {
#else
    } else if (card->CardType == MEMORY_CARD_TYPE_MMC) {
#endif
        card->CSD_MMC.CSD_STRUCTURE = (card->CSD_HI >> 54) & 0x3;
        card->CSD_MMC.SPEC_VERS = (card->CSD_HI >> 50) & 0xF;
        card->CSD_MMC.Reserved1 = (card->CSD_HI >> 48) & 0x3;
        card->CSD_MMC.TAAC = (card->CSD_HI >> 40) & 0xFF;
        card->CSD_MMC.NSAC = (card->CSD_HI >> 32) & 0xFF;
        card->CSD_MMC.TRAN_SPEED = (card->CSD_HI >> 24) & 0xFF;
        card->CSD_MMC.CCC = (card->CSD_HI >> 12) & 0xFFF;
        card->CSD_MMC.READ_BL_LEN = (card->CSD_HI >> 8) & 0xF;
        card->CSD_MMC.READ_BL_PARTIAL = (card->CSD_HI >> 7) & 0x1;
        card->CSD_MMC.WRITE_BLK_MISALIGN = (card->CSD_HI >> 6) & 0x1;
        card->CSD_MMC.READ_BLK_MISALIGN = (card->CSD_HI >> 5) & 0x1;
        card->CSD_MMC.DSR_IMP = (card->CSD_HI >> 4) & 0x1;
        card->CSD_MMC.Reserved2 = (card->CSD_HI >> 2) & 0x3;
        card->CSD_MMC.C_SIZE = ((card->CSD_HI & 0x3) << 10);
        card->CSD_MMC.C_SIZE |= ((card->CSD_LO >> 54) & 0x3FF);
        card->CSD_MMC.VDD_R_CURR_MIN = (card->CSD_LO >> 51) & 0x7;
        card->CSD_MMC.VDD_R_CURR_MAX = (card->CSD_LO >> 48) & 0x7;
        card->CSD_MMC.VDD_W_CURR_MIN = (card->CSD_LO >> 45) & 0x7;
        card->CSD_MMC.VDD_W_CURR_MAX = (card->CSD_LO >> 42) & 0x7;
        card->CSD_MMC.C_SIZE_MULT = (card->CSD_LO >> 39) & 0x7;
        card->CSD_MMC.ERASE_GRP_SIZE = (card->CSD_LO >> 34) & 0x1F;
        card->CSD_MMC.ERASE_GRP_MULT = (card->CSD_LO >> 29) & 0x1F;
        card->CSD_MMC.WP_GRP_SIZE = (card->CSD_LO >> 24) & 0x1F;
        card->CSD_MMC.WP_GRP_ENABLE = (card->CSD_LO >> 23) & 0x1;
        card->CSD_MMC.DEFAULT_ECC = (card->CSD_LO >> 21) & 0x3;
        card->CSD_MMC.R2W_FACTOR = (card->CSD_LO >> 18) & 0x7;
        card->CSD_MMC.WRITE_BL_LEN = (card->CSD_LO >> 14) & 0xF;
        card->CSD_MMC.WRITE_BL_PARTIAL = (card->CSD_LO >> 13) & 0x1;
        card->CSD_MMC.Reserved3 = (card->CSD_LO >> 9) & 0xF;
        card->CSD_MMC.CONTENT_PROT_APP = (card->CSD_LO >> 8) & 0x1;
        card->CSD_MMC.FILE_FORMAT_GRP = (card->CSD_LO >> 7) & 0x1;
        card->CSD_MMC.COPY = (card->CSD_LO >> 6) & 0x1;
        card->CSD_MMC.PERM_WRITE_PROTECT = (card->CSD_LO >> 5) & 0x1;
        card->CSD_MMC.TMP_WRITE_PROTECT = (card->CSD_LO >> 4) & 0x1;
        card->CSD_MMC.FILE_FORMAT = (card->CSD_LO >> 2) & 0x3;
        card->CSD_MMC.ECC = (card->CSD_LO) & 0x3;
    }

    return err;
}

u32
ftsdc021_ops_select_card(u8 ip_idx)
{
    /* send CMD7 to enter transfer mode */
    return ftsdc021_send_command(ip_idx, SDHCI_SELECT_CARD, SDHCI_CMD_TYPE_NORMAL, 0,
                                 SDHCI_CMD_RTYPE_R1R5R6R7, 0, SDHost[ip_idx].Card->RCA << 16);

}

u32
ftsdc021_ops_app_repo_wr_num(u8 ip_idx, u32* num)
{

    u32 err;
    SDCardInfo* card = SDHost[ip_idx].Card;
    err =
        ftsdc021_send_command(ip_idx, SDHCI_APP_CMD, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 1, (card->RCA << 16));

    if (err)
        return err;

    ftsdc021_set_transfer_mode(ip_idx, 0, 0, SDHCI_TXMODE_READ_DIRECTION, 0);
    ftsdc021_prepare_data(ip_idx, 1, SD_WRITTEN_NUM_LENGTH, (u32) & (card->SCR), READ);

    err =
        ftsdc021_send_command(ip_idx, SDHCI_SEND_NUM_WR_BLKS, SDHCI_CMD_TYPE_NORMAL, 1, SDHCI_CMD_RTYPE_R1R5R6R7, 1, 0);

    if (err)
        return err;

    ftsdc021_transfer_data(ip_idx, READ, num, SD_WRITTEN_NUM_LENGTH);

    return 0;
}


u32
ftsdc021_ops_app_send_scr(u8 ip_idx)
{
    // Reading the SCR through PIO
    SDCardInfo* card = SDHost[ip_idx].Card;
    /* CMD 55 */
    ftsdc021_send_command(ip_idx, SDHCI_APP_CMD, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 1, (card->RCA << 16));

    ftsdc021_set_transfer_mode(ip_idx, 0, 0, SDHCI_TXMODE_READ_DIRECTION, 0);

    ftsdc021_prepare_data(ip_idx, 1, SCR_LENGTH, (u32) & (card->SCR), READ);

    /* ACMD 51 */
    ftsdc021_send_command(ip_idx, SDHCI_SEND_SCR, SDHCI_CMD_TYPE_NORMAL, 1, SDHCI_CMD_RTYPE_R1R5R6R7, 1, 0);

    return ftsdc021_transfer_data(ip_idx, READ, (u32*) & (card->SCR), SCR_LENGTH);
}

u32
ftsdc021_ops_app_set_wr_blk_cnt(u8 ip_idx, u32 n_blocks)
{
    u32 err;
    SDCardInfo* card = SDHost[ip_idx].Card;
    /* CMD 55: Indicate to the card the next cmd is app-specific command */
    err =
        ftsdc021_send_command(ip_idx, SDHCI_APP_CMD, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0,
                              (card->RCA << 16));
    if (err)
        return err;

    err =
        ftsdc021_send_command(ip_idx, SDHCI_SET_WR_BLOCK_CNT, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0,
                              n_blocks);
    if (err)
        return err;

    return 0;
}

u32
ftsdc021_ops_app_set_bus_width(u8 ip_idx, u32 width)
{
    u32 err;
    SDCardInfo* card = SDHost[ip_idx].Card;
    /* CMD 55: Indicate to the card the next cmd is app-specific command */
    err =
        ftsdc021_send_command(ip_idx, SDHCI_APP_CMD, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0,
                              (card->RCA << 16));
    if (err)
        return err;

    err = ftsdc021_send_command(ip_idx, SDHCI_SET_BUS_WIDTH, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0, width);
    if (err)
        return err;

    return 0;
}

/**
 * Switch Function returns 64 bytes status data. Caller must make sure
 * "resp" pointer has allocated enough space.
 */
u32
ftsdc021_ops_sd_switch(u8 ip_idx, u32 mode, u32 group, u8 value, u8* resp)
{
    u32 arg;
    SDCardInfo* card = SDHost[ip_idx].Card;
    if (card->CardType != MEMORY_CARD_TYPE_SD) {
        sdc_dbg_print("Switch Function: This is not SD Card !\n");
        return 1;
    }

    ftsdc021_set_transfer_mode(ip_idx, 0, 0, SDHCI_TXMODE_READ_DIRECTION, 0);
    ftsdc021_prepare_data(ip_idx, 1, 64, (u32) resp, READ);

    //Check Function
    arg = mode << 31 | 0x00FFFFFF;
    arg &= ~(0xF << (group * 4));
    arg |= value << (group * 4);

    // Commented by MikeYeh 081201: The 31st bit of argument for CMD6 is zero to indicate "Check Function".
    // Check function used to query if the card supported a specific function.
    /* CMD 6 */
    if (ftsdc021_send_command(ip_idx, SDHCI_SWITCH_FUNC, SDHCI_CMD_TYPE_NORMAL, 1, SDHCI_CMD_RTYPE_R1R5R6R7, 1, arg)) {
        sdc_dbg_print("CMD6 Failed.\n");
        return 1;
    }

    return ftsdc021_transfer_data(ip_idx, READ, (u32*) resp, 64);
}

u32
ftsdc021_ops_send_tune_block(u8 ip_idx)
{

    //hst = *((volatile u16 *)(FTSDC021_FPGA_BASE + 0x3E));
    if (!(gpRegSDC(ip_idx)->HostCtrl2 & SDHCI_EXECUTE_TUNING)) {
        sdc_dbg_print(" Execute tuning done..\n");
        return 0;
    }

    ftsdc021_set_transfer_mode(ip_idx, 0, 0, SDHCI_TXMODE_READ_DIRECTION, 0);
    ftsdc021_prepare_data(ip_idx, 1, 64, 0, READ);

    if (ftsdc021_send_command(ip_idx, SDHCI_SEND_TUNE_BLOCK, SDHCI_CMD_TYPE_NORMAL, 1, SDHCI_CMD_NO_RESPONSE, 1, 0))
        return 1;
    /* Do not require to read data */

    return 0;
}

/* CMD13: Return Card status inside R1 response */
u32
ftsdc021_ops_send_card_status(u8 ip_idx)
{
    u32 err;
    SDCardInfo* card = SDHost[ip_idx].Card;
    err = ftsdc021_send_command(ip_idx, SDHCI_SEND_STATUS, SDHCI_CMD_TYPE_NORMAL, 0,
                                SDHCI_CMD_RTYPE_R1R5R6R7, 0, (card->RCA << 16));
    if (err)
        return err;

    return 0;
}

/* ACMD13: Return 512 bits of SD Status */
u32
ftsdc021_ops_send_sd_status(u8 ip_idx)
{
    u32 err;
    SDCardInfo* card = SDHost[ip_idx].Card;
    // The following insure the state is in Secure Mode.
    /* CMD 16 */
    err = ftsdc021_send_command(ip_idx, SDHCI_SET_BLOCKLEN, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 1, 0x40);
    if (err || (card->respLo & SD_STATUS_ERROR_BITS)) {
        sdc_dbg_print("Any error was happened when user called command\n");
        return 1;
    }

    /* CMD 55 */
    err =
        ftsdc021_send_command(ip_idx, SDHCI_APP_CMD, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 1,
                              (card->RCA << 16));
    if (err || (card->respLo & SD_STATUS_ERROR_BITS)) {
        sdc_dbg_print("Any error was happened when user called command\n");
        return 1;
    }

    ftsdc021_set_transfer_mode(ip_idx, 0, 0, SDHCI_TXMODE_READ_DIRECTION, 0);
    ftsdc021_prepare_data(ip_idx, 1, sizeof(SDStatus), (u32) & (card->sd_status), READ);

    /* ACMD 13 */
    ftsdc021_send_command(ip_idx, SDHCI_SD_STATUS, SDHCI_CMD_TYPE_NORMAL, 1, SDHCI_CMD_RTYPE_R1R5R6R7, 1, 0);

    return ftsdc021_transfer_data(ip_idx, READ, (u32*) & (card->sd_status), sizeof(SDStatus));

}

u32
ftsdc021_ops_register_int_func(u8 ip_idx, SDIO_ISR_handler handler)
{
    //ftsdc021_enable_irq(ip_idx, 0x0100); //BSD: DON'T enable CARD INTR before set up its handler!!
    SDHost[ip_idx].sdio_card_int = handler;
    return (u32)ERR_SD_NO_ERROR;
}

u32
ftsdc021_ops_remove_int_func(u8 ip_idx)
{
    ftsdc021_disable_irq(ip_idx, 0x0100);
    SDHost[ip_idx].sdio_card_int = NULL;
    return (u32)ERR_SD_NO_ERROR;
}
