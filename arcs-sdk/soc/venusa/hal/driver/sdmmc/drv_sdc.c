/**
 * @file   drv_sdc.c
 * @brief  SD Card Driver Core Functionality
 *         This file implements the core functionality for SD/SDIO card drivers,
 *         including initialization, data transfer, and device management.
 *         Supports both memory cards (SDSC/SDHC/SDXC) and SDIO devices.
 *         Provides APIs for reading/writing sectors, erasing blocks, and querying card status.
 * @author USER
 * @version V1.3.2
 * @date   2024-06-20
 *
 * @copyright (c) 2024 ListenAI. All rights reserved.
 *           Permission is granted to modify this code under the terms of the
 *           applicable open source license (see LICENSE file).
 *
 * @section LICENSE License Information
 * This software is licensed under the MIT License.
 * See the included LICENSE file for complete terms.
 *
 * @section DEPENDENCIES
 * Dependencies:
 *   - Hardware abstraction layer (HAL) for SDC controller
 *   - Real-time operating system (RTOS) components (if enabled)
 *   - Interrupt controller interface
 *   - DMA subsystem (for ADMA/SDMA modes)
 *
 * @section MODULE_DESCRIPTION
 * The driver supports multiple operational modes:
 *   - PIO mode for simple systems
 *   - Advanced DMA (ADMA) for high-throughput applications
 *   - SDMA mode for intermediate performance requirements
 *   - UHS-I protocol with automatic bus width negotiation
 *
 * @section MAIN_FUNCTIONS
 * Key functions implemented in this module:
 *   - lib_sdc_scan_card(): Card detection and initialization
 *   - lib_sdc_read_sector(): Sectors read operation
 *   - lib_sdc_write_sector(): Sectors write operation
 *   - lib_sdc_erase(): Block erasure functionality
 *   - SDC_SD_menu(): Display SD card information
 *   - SDC_SDIO_menu(): Display SDIO card registers
 *   - gm_sdc_api_action(): Unified API dispatcher
 */

#include <time.h>
#include "drv_sdc.h"
#include <ftsdc021.h>


/** @brief Get SDC register base address by index
 *  @param[in] ip_idx Index of SDC controller instance
 *  @return Base address of corresponding SDC register block
 */
extern volatile ftsdc021_reg*
gpRegSDC(u8 ip_idx);

/** @brief SD Card menu display function
 *  @param[in] ip_idx Index of SDC controller instance
 *  @return Operation status (0=success)
 */
u8
SDC_SD_menu(u8 ip_idx);

/** @brief SDIO Card menu display function
 *  @param[in] ip_idx Index of SDC controller instance
 *  @return Operation status (0=success)
 */
u8
SDC_SDIO_menu(u8 ip_idx);

/** @brief String representation table for card states
 *  Mapped to standard SD card state machine definitions
 */
static char* card_state_table[] = {
        "Card Idle State",          ///< RST released, cards powered up
        "Card Ready State",         ///< Initialization completed
        "Card Identify State",      ///< Sending CMD2/ACMD41 identification
        "Card Stanby State",        ///< Low power standby mode entered
        "Card Transfer State",      ///< Data transfer in progress
        "Card Send Data State",     ///< Host sending data to card
        "Card Receive Data State",  ///< Host receiving data from card
        "Card Programming State",   ///< Writing data blocks
        "Card Disconnect State",    ///< Power down sequence initiated
        "Undefined State"           ///< Reserved/invalid state
};

/** @brief String representation table for transfer types
 *  Classifies different data transfer mechanisms supported
 */
static char* transfer_type_table[] = {
        "ADMA",                     ///< Advanced DMA mode
        "SDMA",                     ///< Simple DMA mode
        "PIO",                      ///< Programmed I/O mode
        "External DMA",             ///< Third-party DMA controller
        "Undefined Transfer Type"   ///< Reserved/invalid type
};

/** @brief String representation table for bus speeds
 *  Standardized naming conventions for bus performance modes
 */
static char* transfer_speed_table[] = {
        "Normal Speed / SDR12",     ///< Up to 12.5MB/s single data rate
        "High Speed / SDR25",       ///< Up to 25MB/s single data rate
        "SDR50-100MHz",             ///< High-speed single data rate variants
        "SDR104-208MHz",            ///< Ultra high-speed single data rate
        "DDR50",                    ///< Double data rate at 50MHz
        "Undefined Speed"           ///< Reserved/invalid speed mode
};

/** @brief Human-readable storage capacity units
 *  Used for formatted capacity display conversions
 */
static char* card_capacities[] = {
        "Bytes",                    ///< Smallest unit (B)
        "KBytes",                   ///< Kilobytes (KB)
        "MiBytes",                  ///< Megabytes (MB)
        "GiBytes"                   ///< Gigabytes (GB)
};

/** @brief Get human-readable card state description
 *  @param[in] state Card state code from hardware register
 *  @return Pointer to descriptive string constant
 *  @note Truncates invalid states to last entry
 */
u8*
SDC_ShowCardState(Card_State state)
{
    if (state > CUR_STATE_RSV)
        state = CUR_STATE_RSV;

    return (u8*) (card_state_table[state]);
}

/** @brief Get human-readable transfer type description
 *  @param[in] tType Transfer type code from configuration
 *  @return Pointer to descriptive string constant
 *  @note Truncates invalid types to last entry
 */
u8*
SDC_ShowTransferType(Transfer_Type tType)
{
    if (tType > TRANS_UNKNOWN)
        tType = TRANS_UNKNOWN;

    return (u8*) (transfer_type_table[tType]);
}

/** @brief Get human-readable bus speed description
 *  @param[in] speed Bus speed code from capability register
 *  @return Pointer to descriptive string constant
 *  @note Truncates invalid speeds to last entry
 */
u8*
SDC_ShowTransferSpeed(Bus_Speed speed)
{
    if (speed > SPEED_RSRV)
        speed = SPEED_RSRV;

    return (u8*) (transfer_speed_table[speed]);
}

/** @brief Format storage capacity with appropriate unit
 *  @param[in] cap Total capacity in bytes
 *  @param[out] buf Preallocated buffer for formatted string
 *  @note Uses floating point approximation for fractional parts
 *  @note Selects largest applicable unit (GB -> MB -> KB -> B)
 */
void
SDC_ShowCapacity(u64 cap, s8* buf)
{
    u8 i;
    u32 integer = 0;
    u32 fraction = 0;
    u64 tmp;
    for (i = 3; i > 0; i--) {
        if ((cap >> (i * 10)) > 0) {
            tmp = cap >> ((i - 1) * 10);
            fraction = (u32) (tmp & 0x3ff);
            integer = (u32) (tmp >> 10);
            break;
        }
    }
    sprintf((char *)buf, "%d.%d %s", integer, fraction, card_capacities[i]);
}

/** @brief Test SDIO card functionality
 *  @param[in] ip_idx Index of SDC controller instance
 *  @return Error code (0=success)
 *  @note Verifies card type before performing operations
 *  @note Updates card information through control operations
 */
u16
SDC_sdio_test(u8 ip_idx)
{
    if (SDHost[ip_idx].Card->CardType != SDIO_TYPE_CARD) {
        sdc_dbg_print(" No SDIO Card insert.\n");
        return 0;
    }

    ftsdc021_ops_Card_Info(ip_idx);

    return 0;
}

/** @brief Read and display card status register
 *  @param[in] ip_idx Index of SDC controller instance
 *  @return Error code (0=success)
 *  @note Decodes bitfields from response register
 *  @note Prints detailed error diagnostics for each flag
 */
u16
SDC_read_card_status(u8 ip_idx)
{
    if (SDHost[ip_idx].Card->CardType != SDIO_TYPE_CARD) {

        if (ftsdc021_ops_send_card_status(ip_idx)) {
            sdc_dbg_print("ERR## Get Card Status Failed !\n");
            return 0;
        }

        sdc_dbg_print(" Card Status = 0x%08x\n", (u32 ) SDHost[ip_idx].Card->respLo);

        //sdc_dbg_print("  in %s.\n", SDC_ShowCardState((SDHost[ip_idx].Card->respLo >> 9) & 0xF));

        if (SDHost[ip_idx].Card->respLo & (0x1 << 3))
            sdc_dbg_print("  AKE_Seq_error\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 5))
            sdc_dbg_print("  The following cmd will be ACMD\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 8))
            sdc_dbg_print("  Ready for Data\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 13))
            sdc_dbg_print("  Erase Reset: set\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 14))
            sdc_dbg_print("  Card ECC: Disabled\n");
        else
            sdc_dbg_print("  Card ECC: Enabled\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 15))
            sdc_dbg_print("  WP Erase Skip: protected\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 16))
            sdc_dbg_print("  CSD Overwrite: err\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 19))
            sdc_dbg_print("  General or Unknown Error\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 20))
            sdc_dbg_print("  Internal Card Controller err\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 21))
            sdc_dbg_print("  Card ECC Failure\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 22))
            sdc_dbg_print("  Illegal Command err\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 23))
            sdc_dbg_print("  Prev. Command CRC err\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 24))
            sdc_dbg_print("  Lock/Unlock err\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 25))
            sdc_dbg_print("  Card locked\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 26))
            sdc_dbg_print("  WP Violation\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 27))
            sdc_dbg_print("  Erase Param. err\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 28))
            sdc_dbg_print("  Erase Seq. err\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 29))
            sdc_dbg_print("  Block Len err\n");

        if (SDHost[ip_idx].Card->respLo & (0x1 << 30))
            sdc_dbg_print("  Addr. err\n");

        if (SDHost[ip_idx].Card->respLo & ((u32) 0x1 << 31))
            sdc_dbg_print("  Out of Range\n");
    }

    return 0;
}

/** @brief Read data from SD card sectors
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[in] addr Starting sector address
 *  @param[in] cnt Number of sectors to read
 *  @param[out] buf Destination buffer for read data
 *  @return SD operation result code
 *  @note Validates address range against card capacity
 *  @note Sets fixed block length via CMD16 before transfer
 *  @note Uses hardware-accelerated XFER mode when available
 */
SD_RESULT
lib_sdc_read(u8 ip_idx, u32 addr, u32 cnt, void* buf)
{

    //clock_t t0, t1;

    if (addr + cnt > (SDHost[ip_idx].Card->numOfBlocks + 1)) {
        /* For FPGA verification purpose, just give warning and allow the operation. */
        sdc_dbg_print(" WARN## ...  Out of Range (%d + %d) > %d.\n", addr, cnt, SDHost[ip_idx].Card->numOfBlocks);
        return ERR_SD_DATA_LENGTH_TOO_LONG;
    }

    /* Check if we have enough buffer */
    //if ((cnt * (1 << rd_bl_len)) > FTSDC021_BUFFER_LENGTH)
    //cnt = FTSDC021_BUFFER_LENGTH / (1 << rd_bl_len);
    /* CMD 16 */
    ftsdc021_send_command(ip_idx, SDHCI_SET_BLOCKLEN, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0,
            (SDHost[ip_idx].Card->read_block_len));

    sdc_dbg_print(" Read %d blocks from address %d ", cnt, addr);

//  t0 = jiffies();
    if (!ftsdc021_card_read(ip_idx, addr, cnt, (u8*) buf)) {
        //  t1 = jiffies();
        //  sdc_dbg_print(" success. (%d ms)\n", (t1 - t0));
    } else
    {
        sdc_dbg_print("read addr %d ,cnt %d FAILED.\n", addr, cnt);
        return ERR_SD_CARD_ERROR;
    }

    return ERR_SD_NO_ERROR;
}

/** @brief Write data to SD card sectors
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[in] addr Starting sector address
 *  @param[in] cnt Number of sectors to write
 *  @param[in] compare Auto-compare enable flag
 *  @param[in] buf Source buffer containing data to write
 *  @return SD operation result code
 *  @note Validates address range against card capacity
 *  @note Sets fixed block length via CMD16 before transfer
 *  @note Supports UHS-I automatic preset with block counting
 */
SD_RESULT
lib_sdc_write(u8 ip_idx, u32 addr, u32 cnt, u8 compare, void* buf)
{
    //u32 i;
    //clock_t t0, t1;

    if (addr + cnt > (SDHost[ip_idx].Card->numOfBlocks + 1)) {
        /* For FPGA verification purpose, just give warning and allow the operation. */
        sdc_dbg_print(" WARN## ...  Out of Range (%d + %d) > %d.\n", addr, cnt, SDHost[ip_idx].Card->numOfBlocks);
        return ERR_SD_DATA_LENGTH_TOO_LONG;
    }

    /* send CMD16 for (1 << wr_bl_len) Bytes fixed block length */
    ftsdc021_send_command(ip_idx, SDHCI_SET_BLOCKLEN, SDHCI_CMD_TYPE_NORMAL, 0, SDHCI_CMD_RTYPE_R1R5R6R7, 0,
            (SDHost[ip_idx].Card->write_block_len));

    //sdc_dbg_print(" Write %d blocks to address %d, ", cnt, addr);

    if (ftsdc021_wait_for_state(ip_idx, CUR_STATE_TRAN, 5000))
        return ERR_SD_WAIT_TRANSFER_STATE_TIMEOUT;

    if (ftsdc021_uhs1_mode(ip_idx)) {
        if (ftsdc021_ops_app_set_wr_blk_cnt(ip_idx, cnt)) {
            sdc_dbg_print(" ERR## ... Set Write Block Count %d.\n", cnt);
            return ERR_SD_OTHER_ERROR;
        }
    }

    //t0 = jiffies();
    if (ftsdc021_card_write(ip_idx, addr, cnt, (u8*) buf)) {
        sdc_dbg_print(" lib_sdc_write FAILED.\n");
        //if (SDHost[ip_idx].Card->FlowSet.UseDMA == ADMA)
        //SDC_ShowDscpTbl(0);
        return ERR_SD_CARD_ERROR;
    }

    return ERR_SD_NO_ERROR;
}

/** @brief Erase SD card sectors
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[in] addr Starting sector address
 *  @param[in] cnt Number of sectors to erase
 *  @return SD operation result code
 *  @note Special handling for full card erase (cnt=0)
 *  @note Uses erasure groups defined by card capabilities
 *  @note Supports multi-pass erasure for large regions
 */
SD_RESULT
lib_sdc_erase(u8 ip_idx, u32 addr, u32 cnt)
{
    u32 size;

    if (addr + cnt > (SDHost[ip_idx].Card->numOfBlocks + 1)) {
        sdc_dbg_print("Selecting area exceedes the Max. block number: %d!!\n", SDHost[ip_idx].Card->numOfBlocks);
        return ERR_SD_OUT_OF_ADDRESS_RANGE;
    }

    if (get_erase_group_size(ip_idx, &size))
        return ERR_SD_OTHER_ERROR;

    /* Means erase all*/
    if (cnt == 0) {
        while (addr < SDHost[ip_idx].Card->numOfBlocks) {
            cnt = SDHost[ip_idx].Card->numOfBlocks - addr;

            if (cnt > 32768)
                cnt = 32768;

            //sdc_dbg_print("Erasing from address %d total %d blocks", addr, cnt);
            if (ftsdc021_card_erase(ip_idx, addr, cnt)) {
                sdc_dbg_print("Erasing from address %d total %d blocks  FAILED.\n", addr, cnt);
                return ERR_SD_OTHER_ERROR;
            }        // else
                     //sdc_dbg_print(" success.\n");

            addr += cnt;
        }

    } else {
        //sdc_dbg_print("Erasing from block %d total %d, size %d blocks", addr, cnt, size);
        if (ftsdc021_card_erase(ip_idx, addr, cnt)) {
            sdc_dbg_print("Erasing from address %d total %d blocks  FAILED.\n", addr, cnt);
            return ERR_SD_OTHER_ERROR;
        } //else
          //sdc_dbg_print(" success.\n");

    }

    return ERR_SD_OTHER_ERROR;
}

/** @brief Display SD card information menu
 *  @param[in] ip_idx Index of SDC controller instance
 *  @return Operation status (0=success)
 *  @note Shows comprehensive card characteristics
 *  @note Includes bus configuration and performance parameters
 */
u8
SDC_SD_menu(u8 ip_idx)
{
    u64 capacity;
    s8 buf[64];
    capacity = (u64) SDHost[ip_idx].Card->numOfBlocks;
    capacity <<= 9;
    SDC_ShowCapacity(capacity, buf);
    sdc_dbg_print("****************  Information  ****************\n");
    sdc_dbg_print("* Bus width: %d.\n", SDHost[ip_idx].Card->bus_width);
    sdc_dbg_print("* Transfer speed: %s.\n", SDC_ShowTransferSpeed(SDHost[ip_idx].Card->speed));
    sdc_dbg_print("* Transfer type: %s.\n", SDC_ShowTransferType(SDHost[ip_idx].Card->FlowSet.UseDMA));
    sdc_dbg_print("* Card Capacity = %s\n", buf);
    if (SDHost[ip_idx].Card->FlowSet.UseDMA == ADMA) {
        sdc_dbg_print("* ADMA2 Bytes per Lines: %d.\n", SDHost[ip_idx].Card->FlowSet.lineBound);
        sdc_dbg_print("* ADMA2 inset Nop: %d.\n", SDHost[ip_idx].Card->FlowSet.adma2_insert_nop);
        sdc_dbg_print("* ADMA2 interrupt at last line: %d.\n", SDHost[ip_idx].Card->FlowSet.adma2_use_interrupt);
    } else if (SDHost[ip_idx].Card->FlowSet.UseDMA == SDMA) {
        sdc_dbg_print("* SDMA Boundary: %d.\n", 1 << (SDHost[ip_idx].Card->FlowSet.lineBound + 12));
    } else {
        sdc_dbg_print("* Data Port Width: %d.\n", SDHost[ip_idx].Card->FlowSet.lineBound);
    }
    //sdc_dbg_print("* Auto Command: %d.\n", SDHost[ip_idx].Card->FlowSet.autoCmd);
    //sdc_dbg_print("* Erase within burnin: %s.\n", SDHost[ip_idx].Card->FlowSet.Erasing ? "Yes" : "No");
    //sdc_dbg_print("* Abort: %s.\n", SDC_ShowAbortType(SDHost[ip_idx].Card->FlowSet.SyncAbt));

    return 0;
}

/** @brief Display SDIO card information menu
 *  @param[in] ip_idx Index of SDC controller instance
 *  @return Operation status (0=success)
 *  @note Shows SDIO-specific register contents
 *  @note Displays vendor register banks and attributes
 */
u8
SDC_SDIO_menu(u8 ip_idx)
{
    sdc_dbg_print("***********  Information  ***********\n");
    sdc_dbg_print("*************************************\n");
    sdc_dbg_print("*VendorReg0: %x    VendorReg6: %x    *\n", gpRegSDC(ip_idx)->VendorReg0,
            gpRegSDC(ip_idx)->VendorReg6);
    sdc_dbg_print("*VendorReg3: %x    VendorReg4: %x    *\n", gpRegSDC(ip_idx)->VendorReg3,
            gpRegSDC(ip_idx)->VendorReg4);
    sdc_dbg_print("*HW attrib : %x                      *\n", gpRegSDC(ip_idx)->VendorReg7);
    return 0;
}

/** @brief Scan for and initialize SD/SDIO card
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[in] speed Target bus speed mode
 *  @return SD operation result code
 *  @note Complete initialization sequence including voltage ramp-up
 *  @note Supports both memory cards and SDIO devices
 *  @note Calls appropriate application initialization callbacks
 */
SD_RESULT
lib_sdc_scan_card(u8 ip_idx, u8 speed)
{
    u8 i;
    if (!ftsdc021_CheckCardInsert(ip_idx)) {
        sdc_dbg_print(" No card inserted !\n");
        return ERR_SD_OTHER_ERROR;
    }

    ftsdc021_set_bus_width(ip_idx, 1);
    ftsdc021_SetSDClock(ip_idx, SDHost[ip_idx].min_clk);    //SDHost[ip_idx].min_clk);
    ftsdc021_SetPower(ip_idx, 21);      //3.3v
    ftsdc021_intr_en(ip_idx, 0);

    if (ftsdc021_ops_go_idle_state(ip_idx, 0) != 0)
        sdc_dbg_print("idle state fail\n");
    //ftsdc021_HCReset(ip_idx,SDHCI_SOFTRST_CMD|SDHCI_SOFTRST_DAT);
    ftsdc021_delay(1);

    if (ftsdc021_scan_cards(ip_idx)) {
        sdc_dbg_print(" Scan card FAILED !\n");
        return ERR_SD_OTHER_ERROR;
    }

    if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_SD)
        ftsdc021_read_scr(ip_idx);
#if SDHCI_SUPPORT_eMMC
    else if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC || SDHost[ip_idx].Card->CardType == MEMORY_eMMC)
#else
    else if (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC)
#endif
        ftsdc021_read_ext_csd(ip_idx);

    ftsdc021_set_bus_speed_mode(ip_idx, speed);  //tiger debug
    ftsdc021_set_bus_width(ip_idx, 4);	//user data bus width 1;lyt: if enable uhs, bus width must be 4

#if SDHCI_SUPPORT_eMMC
    if ((SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_SD) ||
            (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC) ||
            (SDHost[ip_idx].Card->CardType == MEMORY_eMMC)) {
#else
    if ((SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_SD) ||
            (SDHost[ip_idx].Card->CardType == MEMORY_CARD_TYPE_MMC)) {
#endif
        SDC_SD_menu(ip_idx);
    } else if (SDHost[ip_idx].Card->CardType == SDIO_TYPE_CARD) {
        if ((SDHost[ip_idx].module_sts & SDHCI_MODULE_STS_SDIO_STD_FUNC) > 0) {
            ftsdc021_sdio_read_cccr(ip_idx, SDHost[ip_idx].Card->OCR);
            ftsdc021_sdio_cis_init();
            ftsdc021_sdio_read_common_cis(ip_idx);
            //ftsdc021_sdio_enable_4bit_bus(ip_idx);
            ftsdc021_sdio_enable_hs(ip_idx);
            for (i = 0; i < SDHost[ip_idx].Card->sdio_funcs; i++) {
                SDHost[ip_idx].Card->func[i].num = i + 1;
                ftsdc021_sdio_init_func(ip_idx, &SDHost[ip_idx].Card->func[i]);
            }
        } else {
            ftsdc021_sdio_enable_4bit_bus(ip_idx);
            ftsdc021_sdio_enable_hs(ip_idx);
        }
        //ftsdc021_set_bus_width(ip_idx,4);
        SDC_SDIO_menu(ip_idx);
    } else {
        sdc_dbg_print("Unknown Card Type !");
    }

    return ERR_SD_NO_ERROR;
}

/** @brief Check if card is physically present
 *  @param[in] ip_idx Index of SDC controller instance
 *  @return SD operation result code
 *  @note Directly queries hardware presence detection
 */
SD_RESULT
lib_sdc_card_exist(u8 ip_idx)
{
    if (ftsdc021_CheckCardInsert(ip_idx))
        return ERR_SD_NO_ERROR;
    else
        return ERR_SD_CARD_NOT_EXIST;
}

/** @brief Check if card is writable
 *  @param[in] ip_idx Index of SDC controller instance
 *  @return SD operation result code
 *  @note Checks write protection status bits
 *  @note Returns lock error if card is write-protected
 */
SD_RESULT
lib_sdc_card_writable(u8 ip_idx)
{
    if (ftsdc021_card_writable(ip_idx))
        return ERR_SD_NO_ERROR;
    else
        return ERR_SD_CARD_LOCK;
}

/** @brief Check if card is inserted (logical presence)
 *  @param[in] ip_idx Index of SDC controller instance
 *  @return SD operation result code
 *  @note Differentiates from physical presence check
 *  @note Considers card initialization status
 */
SD_RESULT
lib_sdc_card_insert(u8 ip_idx)
{
    if (SDHost[ip_idx].Card->CardInsert)
        return ERR_SD_NO_ERROR;
    else
        return ERR_SD_CARD_NOT_EXIST;
}

/** @brief Get card information parameters
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[out] p_blk_len Block length in bytes
 *  @param[out] p_blk_num Total number of blocks
 *  @param[out] p_erase_size Erase sector size
 *  @return SD operation result code
 *  @note Retrieves key geometric parameters from card registration data
 */
SD_RESULT
lib_sdc_get_card_info(u8 ip_idx, u32* p_blk_len, u32* p_blk_num, u32* p_erase_size)
{
    SD_RESULT ret;
    sd_mutex_lock(ip_idx);
    if (ftsdc021_CheckCardInsert(ip_idx) != 1)
        ret = ERR_SD_CARD_NOT_EXIST;
    else {
        if (p_blk_len != NULL)
            *p_blk_len = SDHost[ip_idx].Card->read_block_len;

        if (p_blk_num != NULL)
            *p_blk_num = SDHost[ip_idx].Card->numOfBlocks;

        if (p_erase_size != NULL)
            *p_erase_size = SDHost[ip_idx].Card->erase_sector_size;
        ret = ERR_SD_NO_ERROR;
    }
    sd_mutex_unlock(ip_idx);
    return ret;
}

/** @brief Read multiple sectors from SD card
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[in] sector Starting sector number
 *  @param[in] cnt Number of sectors to read
 *  @param[out] buff Destination buffer for read data
 *  @return SD operation result code
 *  @note Mutex-protected thread-safe operation
 *  @note Validates card existence before attempting read
 */
SD_RESULT
lib_sdc_read_sector(u8 ip_idx, u32 sector, u32 cnt, void* buff)
{
    SD_RESULT ret;
    sd_mutex_lock(ip_idx);
    ret = lib_sdc_card_exist(ip_idx);
    if (ret != ERR_SD_NO_ERROR) {
        sd_mutex_unlock(ip_idx);
        return ret;
    }
    sdc_dbg_print("r: sector=%d cnt=%d\n", sector, cnt);
    ret = lib_sdc_read(ip_idx, sector, cnt, buff);
    sd_mutex_unlock(ip_idx);
    return ret;
}

/** @brief Write multiple sectors to SD card
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[in] sector Starting sector number
 *  @param[in] cnt Number of sectors to write
 *  @param[in] buff Source buffer containing data to write
 *  @return SD operation result code
 *  @note Mutex-protected thread-safe operation
 *  @note Validates card existence before attempting write
 */
SD_RESULT
lib_sdc_write_sector(u8 ip_idx, u32 sector, u32 cnt, void* buff)
{
    SD_RESULT ret;
    sd_mutex_lock(ip_idx);
    ret = lib_sdc_card_exist(ip_idx);
    if (ret != ERR_SD_NO_ERROR) {
        sd_mutex_unlock(ip_idx);
        return ret;
    }
    sdc_dbg_print("w: sector=%d cnt=%d\n", sector, cnt);
    ret = lib_sdc_write(ip_idx, sector, cnt, 0, buff);
    sd_mutex_unlock(ip_idx);
    return ERR_SD_NO_ERROR;
}

/** @brief Get current card type classification
 *  @param[in] ip_idx Index of SDC controller instance
 *  @return Card type enumeration value
 *  @note Returns UNKNOWN if card not present
 */
u32
lib_sdc_card_type(u8 ip_idx)
{
    if (lib_sdc_card_exist(ip_idx) == ERR_SD_NO_ERROR)
        return SDHost[ip_idx].Card->CardType;
    else
        return CARD_TYPE_UNKNOWN;
}

/** @brief Set SD bus width configuration
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[in] width Desired bus width (1=1bit, 4=4bit)
 *  @return SD operation result code
 *  @note Only changes width if different from current setting
 *  @note Must call after changing clock frequency
 */
SD_RESULT
lib_sdc_set_bus_width(u8 ip_idx, u8 width)
{
    SD_RESULT ret = ERR_SD_NO_ERROR;
    //sd_mutex_lock(ip_idx);
    if (SDHost[ip_idx].Card->bus_width != width)
        ret = (SD_RESULT) ftsdc021_set_bus_width(ip_idx, width);
    //sd_mutex_unlock(ip_idx);
    return ret;
}

/** @brief Set ADMA buffer address
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[in] buffer Physical address of DMA buffer
 *  @return Always successful (SD_NO_ERROR)
 *  @note Stores buffer address in card context structure
 */
SD_RESULT
lib_sdc_set_amd_buffer(u8 ip_idx, u32 buffer)
{
    SDHost[ip_idx].Card->FlowSet.adma_buffer = (u64) buffer;
    return ERR_SD_NO_ERROR;
}

/** @brief Platform initialization for SDC controllers
 *  @param[in] sdc0_option Configuration flags for SDC0
 *  @param[in] sdc1_option Configuration flags for SDC1
 *  @param[in] hw_setting Function pointer for hardware setup
 *  @param[in] card_buffer Base address for card context storage
 *  @return Initialization status code
 *  @note Creates mutexes and initializes controller structures
 *  @note Supports separate configuration for dual controller systems
 */
u32
gm_api_sdc_platform_init(u32 sdc0_option, u32 sdc1_option, sdc_platform_setting_t hw_setting, u32 card_buffer)
{
//    u8 i;
    if (hw_setting == NULL) {
        sdc_dbg_print("%s : no hardware setting\n", __func__);
        return (u32) ERR_SD_INIT_ERROR;
    }
    hw_setting();
    sdc_dbg_print("%s sdc0 op=%d sdc1 op=%d\n", __func__, sdc0_option, sdc1_option);

    SDHost[SD_0].Card = (SDCardInfo*) (card_buffer);
    memset(SDHost[SD_0].Card, 0, 512);
//    SDHost[SD_1].Card = (SDCardInfo*)(card_buffer + 512);
//    for (i = 0; i < 2; i++) {
    SDHost[SD_0].sd_init[0] = NULL;
    SDHost[SD_0].sd_init[1] = NULL;
    SDHost[SD_0].sd_detect[0] = NULL;
    SDHost[SD_0].sd_detect[1] = NULL;
    SDHost[SD_0].reset_flag = 1;
#if CFG_RTOS
    SDHost[SD_0].sdio_dma_semaphore = xSemaphoreCreateBinary( );
    SDHost[SD_0].dma_transfer_data = 0;
#endif
//    }

    if ((sdc0_option & SDC_OPTION_ENABLE) > 0) {
        SDHost[SD_0].module_sts = 0;
        if ((sdc0_option & SDC_OPTION_CD_INVERT) > 0)
            SDHost[SD_0].module_sts |= SDHCI_MODULE_STS_INVERT;
        if ((sdc0_option & SDC_OPTION_FIXED) > 0)
            SDHost[SD_0].module_sts |= SDHCI_MODULE_STS_FIXED;
        if ((sdc0_option & SDC_OPTION_SDIO_STD_FUNC) > 0)
            SDHost[SD_0].module_sts |= SDHCI_MODULE_STS_SDIO_STD_FUNC;
        if ((sdc0_option & SDC_OPTION_SDIO_FORCE_3_3_V) > 0 )
            SDHost[SD_0].module_sts |= SDHCI_MODULE_STS_SDIO_FORCE_3_3_V;
        SDHost[SD_0].sdio_app_init = NULL;
        SDHost[SD_0].sdcard_app_init = NULL;
        sd_mutex_create(SD_0);
//        sd_mutex_lock(SD_0);
//        ftsdc021_init(SD_0);
//        sd_mutex_unlock(SD_0);
    } else {
        SDHost[SD_0].sd_mutex = NULL;
        SDHost[SD_0].module_sts = 0;
        SDHost[SD_0].sdio_app_init = NULL;
        SDHost[SD_0].sdcard_app_init = NULL;
    }

//    if ((sdc1_option & SDC_OPTION_ENABLE) > 0) {
//        SDHost[SD_1].module_sts = 0;
//        if ((sdc1_option & SDC_OPTION_CD_INVERT) > 0)
//            SDHost[SD_1].module_sts |= SDHCI_MODULE_STS_INVERT;
//        if ((sdc1_option & SDC_OPTION_FIXED) > 0)
//            SDHost[SD_1].module_sts |= SDHCI_MODULE_STS_FIXED;
//        if ((sdc1_option & SDC_OPTION_SDIO_STD_FUNC) > 0)
//            SDHost[SD_1].module_sts |= SDHCI_MODULE_STS_SDIO_STD_FUNC;
//
//        SDHost[SD_1].sdio_app_init = NULL;
//        SDHost[SD_1].sdcard_app_init = NULL;
//
//        sd_mutex_create(SD_1);
//        sd_mutex_lock(SD_1);
//        ftsdc021_init(SD_1);
//        sd_mutex_unlock(SD_1);
//
//    } else {
//        SDHost[SD_1].sd_mutex = NULL;
//        SDHost[SD_1].module_sts = 0;
//        SDHost[SD_1].sdio_app_init = NULL;
//        SDHost[SD_1].sdcard_app_init = NULL;
//    }
    return ERR_SD_NO_ERROR;
}

/** @brief Register SD card application init handler
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[in] app_init Function pointer to init handler
 *  @note Called after successful card scanning
 */
void
lib_sdc_register_sdcard_app_init(u8 ip_idx, SDC_device_handler app_init)
{
    SDHost[ip_idx].sdcard_app_init = app_init;
}

/** @brief Register SDIO application init handler
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[in] app_init Function pointer to init handler
 *  @note Called after successful SDIO card detection
 */
void
lib_sdc_register_sdio_app_init(u8 ip_idx, SDC_device_handler app_init)
{
    SDHost[ip_idx].sdio_app_init = app_init;
}

/** @brief Main card detection and initialization task
 *  @param[in] ip_idx Index of SDC controller instance
 *  @return Operation status code
 *  @note Central state machine for card hotplugging
 *  @note Coordinates between hardware events and software init sequences
 */
u32
lib_sdc_detect(u8 ip_idx)
{
	//int speed = UHS_SDR50_BUS_SPEED;
	int speed = UHS_SDR25_BUS_SPEED;
	//speed = UHS_SDR104_BUS_SPEED;

    u32 card_type;
    {
        if (SDHost[ip_idx].sd_mutex != NULL) {
            if ((SDHost[ip_idx].module_sts & SDHCI_MODULE_STS_FIXED) == 0)
                ftsdc021_card_detection_handle(ip_idx);
            if (lib_sdc_card_exist(ip_idx) == ERR_SD_NO_ERROR) {
                if (SDHost[ip_idx].sdcard_init_complete == 0) {
                    if ((SDHost[ip_idx].reset_flag == 1) && (SDHost[ip_idx].Card->already_init == 0)) {

                        //lyt: can't finish
                        ftsdc021_HCReset(ip_idx, SD_SOFTRST_ALL);
                        //lyt: maybe no use
                        //ftsdc021_intr_en(ip_idx, 1);
                    }
                    if (SDHost[SD_0].Card->already_init == 0) {
						//host init
                        ftsdc021_init(ip_idx);
                    }

                    sdc_dbg_print("================Start Card Scan %d ===============\n", ip_idx);

					//device init
                    if (lib_sdc_scan_card(ip_idx, speed) != ERR_SD_NO_ERROR)
                        return ERR_SD_OTHER_ERROR;

                    if (lib_sdc_card_exist(ip_idx) != ERR_SD_NO_ERROR)
                        return ERR_SD_CARD_NOT_EXIST;

                    card_type = lib_sdc_card_type(ip_idx);

                    switch (card_type) {
                    case MEMORY_CARD_TYPE_SD:
                        if (SDHost[ip_idx].sdcard_app_init != NULL)
                            SDHost[ip_idx].sdcard_app_init(ip_idx);
                        break;
                    case SDIO_TYPE_CARD:
                        if (SDHost[ip_idx].sdio_app_init != NULL)
                            SDHost[ip_idx].sdio_app_init(ip_idx);
                        break;
                    case MEMORY_SDIO_COMBO:
                    case MEMORY_CARD_TYPE_MMC:
                    case CARD_TYPE_UNKNOWN:
                    default:
                        sdc_dbg_print("un supported card [%d]\n", card_type);
                        break;
                    }
                    SDHost[ip_idx].reset_flag = 1;

                    if (lib_sdc_card_exist(ip_idx) != ERR_SD_NO_ERROR)
                        return ERR_SD_CARD_NOT_EXIST;
                    SDHost[ip_idx].sdcard_init_complete = 1;
                }
            } else {

                SDHost[ip_idx].sdcard_init_complete = 0;
                if (SDHost[ip_idx].reset_flag == 1) {
                    sdc_dbg_print("%d reset sd\n", ip_idx);
                    ftsdc021_HCReset(ip_idx, SD_SOFTRST_ALL);

                    ftsdc021_intr_en(ip_idx, 1);
                    SDHost[ip_idx].reset_flag = 0;
                    if ((SDHost[ip_idx].module_sts & SDHCI_MODULE_STS_FIXED) == 0)
                        ftsdc021_card_detection_init(ip_idx);
                }
            }
        }
    }
    return ERR_SD_NO_ERROR;
}

/** @brief Unified API dispatcher for SDC operations
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[in] type Operation type code
 *  @param[in] in Input parameters specific to operation type
 *  @param[out] out Output parameters specific to operation type
 *  @return Operation status code
 *  @note Central hub for all SDC subsystem operations
 *  @note Translates generic requests into specific driver calls
 */
u32
gm_sdc_api_action(u8 ip_idx, u32 type, void* in, void* out)
{
    u32 ret;
    u32 in_val = 0;
    u32* out_val = NULL;
    SDIO_FUNC* func;
    SDIO_FUNC* curr_func;
    ret = 0;

    if (out != NULL) {
        out_val = (u32*) out;
        *out_val = 0;
    }

    if (in != NULL) {
        in_val = *((u32*) in);
        //  sdc_dbg_print("%s in = %x\n",__func__,in_val);
    }
    switch (type) {
    case GM_SDC_ACTION_INIT:
        in = NULL;
        out = NULL;
        ret = ftsdc021_init(ip_idx);
        break;
    case GM_SDC_ACTION_CARD_SCAN:
        out = NULL;
        ret = lib_sdc_scan_card(ip_idx, (u8) in_val);
        break;
    case GM_SDC_ACTION_GET_CARD_TYPE:
        in = NULL;
        *out_val = lib_sdc_card_type(ip_idx);
        break;
    case GM_SDC_ACTION_SET_BUS_WIDTH:
        out = NULL;
        ret = lib_sdc_set_bus_width(ip_idx, (u8) in_val);
        break;
    case GM_SDC_ACTION_DET_INIT:
        out = NULL;
        in = NULL;
        ftsdc021_card_detection_init(ip_idx);
        break;
    case GM_SDC_ACTION_DET_HANDLE:
        out = NULL;
        in = NULL;
        ftsdc021_card_detection_handle(ip_idx);
        break;
    case GM_SDC_ACTION_SOFT_RESET:
        out = NULL;
        in = NULL;
        ftsdc021_HCReset(ip_idx, (u8) in_val);
        break;
    case GM_SDC_ACTION_IS_CARD_EXIST:
        in = NULL;
        out = NULL;
        ret = lib_sdc_card_exist(ip_idx);
        break;
    case GM_SDC_ACTION_IS_CARD_WRITABLE:
        in = NULL;
        out = NULL;
        ret = lib_sdc_card_writable(ip_idx);
        break;
    case GM_SDC_ACTION_IS_CARD_INSERT:
        in = NULL;
        out = NULL;
        ret = lib_sdc_card_insert(ip_idx);
        break;
    case GM_SDC_ACTION_GET_BLK_LEN:
        in = NULL;
        *out_val = SDHost[ip_idx].Card->read_block_len;
        break;
    case GM_SDC_ACTION_GET_BLK_NUM:
        in = NULL;
        *out_val = SDHost[ip_idx].Card->numOfBlocks;
        break;
    case GM_SDC_ACTION_GET_ERASE_SIZE:
        in = NULL;
        *out_val = SDHost[ip_idx].Card->erase_sector_size;
        break;
    case GM_SDC_ACTION_ENABLE_IRQ:
        out = NULL;
        ret = ftsdc021_enable_irq(ip_idx, (unsigned short) in_val);
        break;
    case GM_SDC_ACTION_DISABLE_IRQ:
        out = NULL;
        ret = ftsdc021_disable_irq(ip_idx, (unsigned short) in_val);
        break;
    case GM_SDC_ACTION_SDIO_REG_IRQ:
        out = NULL;
        ret = ftsdc021_ops_register_int_func(ip_idx, (SDIO_ISR_handler) in);
        break;
    case GM_SDC_ACTION_SDIO_REMOVE_IRQ:
        in = NULL;
        out = NULL;
        ret = ftsdc021_ops_remove_int_func(ip_idx);
        break;
    case GM_SDC_ACTION_IRQ_SET_INIT:
        in = NULL;
        out = NULL;
        ftsdc021_intr_en(ip_idx, 1);
        break;
    case GM_SDC_ACTION_IRQ_SET_NORMAL:
        in = NULL;
        out = NULL;
        ftsdc021_intr_en(ip_idx, 0);
        break;
    case GM_SDC_ACTION_SET_APP_INIT_DONE:
        out = NULL;
        SDHost[ip_idx].sdcard_init_complete = (u8) in_val;
        break;
    case GM_SDC_ACTION_IS_APP_INIT_DONE:
        in = NULL;
        out = NULL;
        ret = SDHost[ip_idx].sdcard_init_complete;
        break;
    case GM_SDC_ACTION_SET_ADMA_BUFER:
        ret = lib_sdc_set_amd_buffer(ip_idx, (u32) in);
        break;
    case GM_SDC_ACTION_IS_HOST_INIT_DONE:
        in = NULL;
        out = NULL;
        ret = SDHost[ip_idx].Card->already_init;
        break;
    case GM_SDC_ACTION_ENTER_IDLE_STATE:
        in = NULL;
        out = NULL;
        ret = ftsdc021_ops_go_idle_state(ip_idx, 0);
        break;
    case GM_SDC_ACTION_CARD_DETECTION:
        ret = lib_sdc_detect(ip_idx);
        break;
    case GM_SDC_ACTION_REG_SDCARD_APP_INIT:
        out = NULL;
        lib_sdc_register_sdcard_app_init(ip_idx, (SDC_device_handler) in);
        break;
    case GM_SDC_ACTION_REG_SDIO_APP_INIT:
        out = NULL;
        lib_sdc_register_sdio_app_init(ip_idx, (SDC_device_handler) in);
        break;
    case GM_SDC_ACTION_SDIO_FUNC_SET_BLOCK_SIZE:
        out = NULL;
        func = (SDIO_FUNC*) in;
        curr_func = &SDHost[ip_idx].Card->func[func->num - 1];
        ftsdc021_sdio_set_func_block_size(ip_idx, func, func->max_blksize);
        curr_func->cur_blksize = func->cur_blksize;
        break;
    case GM_SDC_ACTION_SDIO_FUNC_ENABLE:
        out = NULL;
        ftsdc021_sdio_enable_func(ip_idx, in_val);
        break;
    case GM_SDC_ACTION_SDIO_FUNC_DISABLE:
        out = NULL;
        ftsdc021_sdio_disable_func(ip_idx, in_val);
        break;
    case GM_SDC_ACTION_SDIO_FUNC_CLAIM_IRQ:
        out = NULL;
        ftsdc021_sdio_claim_irq_func(ip_idx, in_val);
        break;
    case GM_SDC_ACTION_SDIO_FUNC_RELEASE_IRQ:
        out = NULL;
        ftsdc021_sdio_release_irq_func(ip_idx, in_val);
        break;
    case GM_SDC_ACTION_SDIO_GET_FUNC:
        func = (SDIO_FUNC*) out;
        memcpy(func, &SDHost[ip_idx].Card->func[in_val - 1], sizeof(SDIO_FUNC));
        break;
    case GM_SDC_ACTION_SDIO_GET_FUNC_NUM:
        *out_val = SDHost[ip_idx].Card->sdio_funcs;
        break;
    case GM_SDC_ACTION_PS_SET_INIT_FUNC:
        SDHost[ip_idx].sd_init[SDC_PS_FUNC_RESTORE] = (SDC_driver_func) in;
        break;
    case GM_SDC_ACTION_PS_SET_DETECTION_FUNC:
        SDHost[ip_idx].sd_detect[SDC_PS_FUNC_RESTORE] = (SDC_driver_func) in;
        break;
    default:
        sdc_dbg_print("%s un support type %d\n", __func__, type);
        break;
    }
    //if(out != NULL)
    //  sdc_dbg_print("% out=%x\n",__func__,*out_val);

    return (u32) ret;
}

/** @brief Erase sectors via unified API
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[in] addr Starting sector address
 *  @param[in] cnt Number of sectors to erase
 *  @return Operation status code
 *  @note Thin wrapper around core erase functionality
 */
u32
gm_sdc_api_erase(u8 ip_idx, u32 addr, u32 cnt)
{
    SD_RESULT ret;
    ret = lib_sdc_erase(ip_idx, addr, cnt);
    return (u32) ret;
}

/** @brief Read sectors via unified API
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[in] sector Starting sector number
 *  @param[in] cnt Number of sectors to read
 *  @param[out] buff Destination buffer for read data
 *  @return Operation status code
 *  @note Thin wrapper around core read functionality
 */
u32
gm_sdc_api_sdcard_sector_read(u8 ip_idx, u32 sector, u32 cnt, void* buff)
{
    SD_RESULT ret;
    ret = lib_sdc_read_sector(ip_idx, sector, cnt, buff);
    return (u32) ret;
}

/** @brief Write sectors via unified API
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[in] sector Starting sector number
 *  @param[in] cnt Number of sectors to write
 *  @param[in] buff Source buffer containing data to write
 *  @return Operation status code
 *  @note Thin wrapper around core write functionality
 */
u32
gm_sdc_api_sdcard_sector_write(u8 ip_idx, u32 sector, u32 cnt, void* buff)
{
    SD_RESULT ret;
    ret = lib_sdc_write_sector(ip_idx, sector, cnt, buff);
    return (u32) ret;
}

/** @brief Execute SDIO CMD53 command
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[in] write Write direction flag
 *  @param[in] fn Function number (1-based)
 *  @param[in] addr Register address
 *  @param[in] incr_addr Auto-increment address flag
 *  @param[in] buf Data buffer pointer
 *  @param[in] blocks Number of blocks to transfer
 *  @param[in] blksz Block size in bytes
 *  @return Operation status code
 *  @note Advanced SDIO command for direct register access
 */
u32
gm_sdc_api_sdio_cmd53(u8 ip_idx, u32 write, u8 fn, u32 addr, u32 incr_addr, u32* buf, u32 blocks, u32 blksz)
{
    return ftsdc021_sdio_CMD53(ip_idx, write, fn, addr, incr_addr, buf, blocks, blksz);
}

/** @brief Execute SDIO CMD52 command
 *  @param[in] ip_idx Index of SDC controller instance
 *  @param[in] write Write direction flag
 *  @param[in] fn Function number (1-based)
 *  @param[in] addr Register address
 *  @param[in] in Input byte count
 *  @param[out] out Output byte count
 *  @return Operation status code
 *  @note Basic SDIO command for simple register access
 */
u32
gm_sdc_api_sdio_cmd52(u8 ip_idx, u32 write, u8 fn, u32 addr, u8 in, u8* out)
{
    return ftsdc021_sdio_CMD52(ip_idx, write, fn, addr, in, out);
}
