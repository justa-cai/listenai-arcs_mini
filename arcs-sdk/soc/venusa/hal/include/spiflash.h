/*!
 * @file spiflash.h
 * @brief Flash memory controller header file containing device definitions, commands, and API functions
 *        This file provides low-level interfaces for SPI NOR flash operations including initialization,
 *        read/write/erase operations, security features, and power management.
 *        Contains hardware configuration parameters, command sets, and driver implementation details.
 */
#ifndef __FLASHROM__
#define __FLASHROM__

#ifdef __clang__
#include <stddef.h>
#else
#include <sys/types.h>
#endif
#include <stdbool.h>

/**
 * @brief Run Mode Configuration Options
 *        Determines whether interrupt handling is enabled during flash operations
 */
typedef enum {
    RUN_WITHOUT_INT = 0,  //!< Run without interrupt support
    RUN_WITH_INT          //!< Run with interrupt support
} RUN_MOD;

/**
 * @brief Flash Device Configuration Structure
 *        Central configuration container for flash controller settings
 */
typedef struct FLASH_DEV {
    unsigned long   base_addr;       //!< Base physical address of flash memory
    unsigned char   d_width;         //!< Data bus width (1, 2, or 4 bytes)
    unsigned char   sclk_div;        //!< SPI clock divider setting
    unsigned char   run_mod;         //!< Run mode selection (@ref RUN_MOD)
    unsigned char   w_protect;       //!< Write protection status flag
    unsigned long   timeout;         //!< Operation timeout in microseconds
    unsigned char   addr_bytes;      //!< Address length (3 or 4 bytes)
    unsigned char   addr_auto;       //!< Auto address incrementation enable
} FLASH_DEV;

/**
 * @brief Serial Flash Discoverable Parameters Table Format
 *        Standardized parameter table structure defined by JEDEC JESD216 spec
 */
typedef struct FLASH_SFDP_TAB {
    char           sig[4];           //!< Signature "SFDP"
    unsigned char  rev_minor;        //!< Minor revision number
    unsigned char  rev_major;        //!< Major revision number
    unsigned char  nph;              //!< Number of parameter headers following this table
    unsigned char  unused;           //!< Reserved field

    unsigned char  id_0;             //!< Manufacturer ID byte 0
    unsigned char  rev_minor_0;      //!< Minor revision byte 0
    unsigned char  rev_major_0;      //!< Major revision byte 0
    unsigned char  len_0;            //!< Length of first parameter header
    unsigned short ptp_0;            //!< Pointers to parameter tables
    unsigned short unused_0;         //!< Reserved field
} FLASH_SFDP_TAB;

/**
 * @brief JEDEC Standard Flash Parameters Register Bitfields
 *        Bitfield representation of SFDP JEDEC parameter register contents
 */
typedef struct FLASH_SFDP_JEDEC {
    unsigned int JEDEC_ERASE_SIZE             : 2; //!< Bits 0-1: Erase size capability
    unsigned int JEDEC_WRITE_GRANULARITY      : 1; //!< Bit 2: Write granularity support
    unsigned int JEDEC_WRITE_ENABLE_REQ       : 1; //!< Bit 3: Write enable required
    unsigned int JEDEC_WRITE_ENABLE_OP        : 1; //!< Bit 4: Write enable operation type
    unsigned int JEDEC_UNUSED_0               : 3; //!< Bits 5-7: Reserved
    unsigned int JEDEC_ERASE_OPCODE           : 8; //!< Bits 8-15: Erase opcode value
    unsigned int JEDEC_SUPPORT_FAST_READ      : 1; //!< Bit 16: Fast read support
    unsigned int JEDEC_ADDRESS_BYTES          : 2; //!< Bits 17-18: Address bytes supported
    unsigned int JEDEC_SUPPORT_DTR            : 1; //!< Bit 19: Dual transfer rate support
    unsigned int JEDEC_SUPPORT_FAST_READ122   : 1; //!< Bit 20: 122MHz fast read support
    unsigned int JEDEC_SUPPORT_FAST_READ144   : 1; //!< Bit 21: 144MHz fast read support
    unsigned int JEDEC_SUPPORT_FAST_READ114   : 1; //!< Bit 22: 114MHz fast read support
    unsigned int JEDEC_UNUSED_1               : 9; //!< Bits 23-31: Reserved
    unsigned int JEDEC_FLASH_MEM_DENS         : 32;//!< Bits 32-63: Flash memory density
} FLASH_SFDP_JEDEC;

/**
 * @brief Flash Device Type Enumeration
 *        Differentiates between various SPI flash configurations and modes
 */
typedef enum {
    FLASH_SPI_1_INN = 0,                //!< Single SPI internal mode
    FLASH_SPI_1_EXT,                    //!< Single SPI external mode
    FLASH_SPI_2_PA04_PA07 = 10,         //!< Dual SPI PA04-PA07
    FLASH_SPI_2_PA08_PA11,               //!< Dual SPI PA08-PA11
    FLASH_SPI_3_PA12_PA15 = 20,         //!< Quad SPI PA12-PA15
    FLASH_SPI_3_PA22_PA25,               //!< Quad SPI PA22-PA25
    FLASH_SPI_3_PA31_PB02,               //!< Quad SPI PA31-PB02
	FLASH_SPI_DUMMY_SET = 0x20,	     	 //!< Set dummy count
    FLASH_SPI_RELEASE_DPD = 0x40,        //!< Release from deep power down
    FLASH_SPI_IGNORE_QE = 0x80,          //!< Ignore quality enable bit

} FLASH_ENUM;

#define INDIVIDUAL_BLK_PROTECT 0         //!< Individual block protection enable

/*------------------------------------------------------------------------------
 * SPI Flash ROM Command Definitions
 * Following constants define operational codes for SPI flash commands
 *----------------------------------------------------------------------------*/
#define SPIROM_ID_VERSION      0x1420c2   //!< Device ID version (MXIC 25L8006)
#define SPIROM_ID_MASK         0x00ffffff//!< Bitmask for device ID matching
#define SPIROM_OP_READ         0x03      //!< Standard read operation
#define SPIROM_OP_QFAST_READ   0xeb     //!< Quad fast read high speed
#define SPIROM_OP_QFAST_READA4 0xec     //!< Quad fast read with 4-byte address
#define SPIROM_OP_FAST_READ    0x0b     //!< Fast read operation
#define SPIROM_OP_FAST_READA4  0x0c     //!< Fast read with 4-byte address
#define SPIROM_OP_DFAST_READ   0xbb     //!< Dual fast read
#define SPIROM_OP_DFAST_READA4 0xbc     //!< Dual fast read with 4-byte address
#define SPIROM_OP_RDID         0x9f     //!< Read manufacturer/device ID
#define SPIROM_OP_RUID         0x4b     //!< Read unique ID
#define SPIROM_OP_READ_ID      0x90    //!< Alternate read ID command
#define SPIROM_OP_WREN         0x06     //!< Write enable
#define SPIROM_OP_WRDI         0x04     //!< Write disable
#define SPIROM_OP_PE           0x81     //!< Page erase
#define SPIROM_OP_SE           0x20     //!< Sector erase
#define SPIROM_OP_SEA4         0x21    //!< Sector erase with 4-byte address
#define SPIROM_OP_BE           0x52     //!< Block erase (32KB)
#define SPIROM_OP_BEA4         0x5c    //!< Block erase with 4-byte address
#define SPIROM_OP_BE2          0xd8    //!< Block erase (64KB)
#define SPIROM_OP_BE2A4        0xdc    //!< Block erase with 4-byte address
#define SPIROM_OP_PP           0x02    //!< Page program
#define SPIROM_OP_PPA4         0x12    //!< Page program with 4-byte address
#define SPIROM_OP_QPP          0x32    //!< Quad page program
#define SPIROM_OP_QPPA4        0x34    //!< Quad page program with 4-byte address
#define SPIROM_OP_RDSR         0x05    //!< Read status register
#define SPIROM_OP_RDSR2        0x35    //!< Read status register 2
#define SPIROM_OP_RDSR3        0x15    //!< Read status register 3
#define SPIROM_OP_WRSR         0x01    //!< Write status register
#define SPIROM_OP_WRSR2        0x31    //!< Write status register 2
#define SPIROM_OP_WRSR3        0x11    //!< Write status register 3
#define SPIROM_OP_SBLK         0x36    //!< Single block lock
#define SPIROM_OP_SBULK        0x39    //!< Single block unlock
#define SPIROM_OP_RDBLOCK      0x3C    //!< Read block lock status
#define SPIROM_OP_RDSCUR       0x2B    //!< Read current sector
#define SPIROM_OP_WPSEL        0x68    //!< Write protect sector select
#define SPIROM_OP_GBLK         0x7E    //!< Global block lock
#define SPIROM_OP_GBULK        0x98    //!< Global block unlock
#define SPIROM_OP_EN_RESET     0x66    //!< Enable reset
#define SPIROM_OP_RESET        0x99    //!< Device reset
#define SPIROM_OP_RDSFDP       0x5A    //!< Read SFDP table
#define SPIROM_OP_SEC_PRGM     0x42    //!< Program security register
#define SPIROM_OP_SEC_ERASE    0x44    //!< Erase security register
#define SPIROM_OP_SEC_READ     0x48    //!< Read security register
#define SPIROM_OP_POW_DOWN     0xb9    //!< Deep power down
#define SPIROM_OP_REL_POW_DOWN 0xab   //!< Release from power down
#define SPIROM_OP_CHIP_ERASE   0x60    //!< Chip erase

/*--------------------------------------
 * Status Register Bit Masking
 *----------------------------------------*/
#define SPIROM_SR_WIP_MASK     0x01   //!< Write in progress flag
#define SPIROM_SR_WEL_MASK     0x02   //!< Write enable latch
#define SPIROM_SR_BP_MASK      0x3C   //!< Block protection mask

/*--------------------------------------
 * Flash Memory Geometric Definitions
 *----------------------------------------*/
#define SPIROM_PAGE_SIZE       0x100    //!< Page size in bytes
#define SPIROM_SECTOR_SIZE     0x1000   //!< Sector size in bytes
#define SPIROM_BLK32_SIZE      0x8000   //!< 32KB block size
#define SPIROM_BLK64_SIZE      0x10000  //!< 64KB block size
#define SPIROM_SECTOR_MASK     0xfffUL  //!< Sector alignment mask
#define SPIROM_BLK32_MASK      0x7fffUL//!< 32KB block mask
#define SPIROM_BLK64_MASK      0xffffUL//!< 64KB block mask
#define SPIROM_PAGE_SIZE       0x100    /*-- page size --*/
#define SPIROM_PAGE_MASK       0xFF
/*--------------------------------------
 * SPI Flash Command Set
 *----------------------------------------*/
#define SPIROM_CMD_READ        0x0     //!< Read command
#define SPIROM_CMD_RDID        0x1     //!< Read ID command
#define SPIROM_CMD_RDST        0x2     //!< Read status command
#define SPIROM_CMD_WREN        0x3     //!< Write enable command
#define SPIROM_CMD_WRDI        0x4     //!< Write disable command
#define SPIROM_CMD_ERASE       0x5     //!< Erase command
#define SPIROM_CMD_PROGRAM     0x6     //!< Program command
#define SPIROM_CMD_LOCK        0x7     //!< Lock command
#define SPIROM_CMD_UNLOCK      0x8     //!< Unlock command
#define SPIROM_CMD_RDBLOCK     0x9     //!< Read block lock status
#define SPIROM_CMD_RDSCUR      0xA     //!< Read current sector
#define SPIROM_CMD_WPSEL       0xB     //!< Write protect sector select
#define SPIROM_CMD_CHIP_UNLOCK 0xC     //!< Chip unlock command
#define SPIROM_CMD_WRSR        0xD     //!< Write status register
#define SPIROM_CMD_RDST2       0xE     //!< Read status register 2
#define SPIROM_CMD_WRSR2       0xF     //!< Write status register 2
#define SPIROM_CMD_ERASE_B32   0x10    //!< Erase 32KB block
#define SPIROM_CMD_ERASE_B64   0x11    //!< Erase 64KB block
#define SPIROM_CMD_RDST3       0x12    //!< Read status register 3
#define SPIROM_CMD_WRSR3       0x13    //!< Write status register 3
#define SPIROM_CMD_ERASE_PG    0x14    //!< Erase page
#define SPIROM_CMD_RESET_DEV   0x15    //!< Reset device
#define SPIROM_CMD_READ_SFDP   0x16    //!< Read SFDP table
#define SPIROM_CMD_RUID        0x17    //!< Read unique ID
#define SPIROM_CMD_SEC_PRGM    0x18    //!< Program security register
#define SPIROM_CMD_SEC_ERASE   0x19    //!< Erase security register
#define SPIROM_CMD_SEC_READ    0x1A    //!< Read security register
#define SPIROM_CMD_PD         0x1B    //!< Power down
#define SPIROM_CMD_REL_PD     0x1C    //!< Release from power down
#define SPIROM_CMD_ERASE_CHIP  0x1D   //!< Chip erase command

/*--------------------------------------
 * Flash Operation Macros
 *----------------------------------------*/
#define FLASH_BLKSIZE SPIROM_SECTOR_SIZE * 16  //!< Block size calculation
#define FLASH_RETRY_TIMES 1800000              //!< Maximum retry attempts

/**
 * @brief Initialize SPI flash device
 *        Performs hardware initialization and brings flash device to known state
 *        @param[in,out] dev Pointer to FLASH_DEV structure with configuration
 *        @param[in] ud0 User data byte 0 for platform-specific init
 *        @param[in] ud1 User data byte 1 for platform-specific init
 *        @return 0 on success, negative errno code on failure
 *        @note Must be called before any other flash operations
 */
int flash_init(FLASH_DEV *dev, unsigned char ud0, unsigned char ud1);

#if FLASH_MUTEX_LOCK == 1
#if BOOT_HARTID == 0
/**
 * @brief Initialize AP mutex system for flash access arbitration
 *        Sets up mutual exclusion mechanism for application processor domain
 */
void flash_mutex_ap_init();
#else
/**
 * @brief Initialize core peripheral mutex system for flash access
 *        Configures lock mechanism for core peripheral access coordination
 *        @param[in,out] dev Flash device configuration structure
 *        @param[in] ud0 User data byte 0 for initialization
 *        @param[in] ud1 User data byte 1 for initialization
 *        @return 0 on success, negative error code on failure
 */
int flash_mutex_cp_init(FLASH_DEV *dev, unsigned char ud0, unsigned char ud1);
#endif
#endif

/**
 * @brief Read data from flash memory
 *        Performs bulk read operation from specified offset
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @param[in] offset Byte-aligned starting offset
 *        @param[out] data Buffer to store read data
 *        @param[in] len Number of bytes to read
 *        @return 0 on success, negative errno code on failure
 *        @note Supports arbitrary length reads subject to device capabilities
 */
int flash_read(FLASH_DEV *dev, off_t offset, void *data, size_t len);

/**
 * @brief Write buffer to flash memory
 *        Writes data to flash after disabling write protection
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @param[in] offset Starting write offset (must be aligned per device requirements)
 *        @param[in] data Pointer to data buffer
 *        @param[in] len Number of bytes to write
 *        @return 0 on success, negative errno code on failure
 *        @note Requires prior call to flash_write_protection_set(false)
 */
int flash_write(FLASH_DEV *dev, off_t offset, const void *data, size_t len);

/**
 * @brief Erase flash memory region
 *        Erases specified memory area after disabling write protection
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @param[in] offset Starting offset of erase region
 *        @param[in] size Size of region to erase (must conform to device erasure unit sizes)
 *        @return 0 on success, negative errno code on failure
 *        @note Use flash_get_page_info_by_offs() to verify valid erase boundaries
 *        @note Requires prior call to flash_write_protection_set(false)
 */
int flash_erase(FLASH_DEV *dev, off_t offset, size_t size);

/**
 * @brief Erase individual flash page
 *        Page-level erase operation for fine-grained erasure control
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @param[in] offset Starting offset of page to erase
 *        @param[in] size Size of page to erase (typically matches SPIROM_PAGE_SIZE)
 *        @return 0 on success, negative errno code on failure
 *        @note More efficient than full sector/block erase when only one page needs modification
 */
int flash_erase_page(FLASH_DEV *dev, off_t offset, size_t size);

/**
 * @brief Control write protection state
 *        Manages hardware write protection mechanism
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @param[in] enable True to enable write protection, False to disable
 *        @return 0 on success, negative errno code on failure
 *        @warning Some devices auto-reenable write protection after each write/erase operation
 */
int flash_write_protection_set(FLASH_DEV *dev, bool enable);

/**
 * @brief Set dummy cycles for flash operations
 *        Configures number of dummy cycles for read/write commands
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @param[in] enable True to enable additional dummy cycles, False to disable
 *        @return 0 on success, negative errno code on failure
 *        @note Adjusting dummy cycles may be necessary for high-speed operation or specific device requirements
 */
int flash_dummy_cycles_set(FLASH_DEV *dev, bool enable);

/**
 * @brief Get minimum write block size
 *        Retrieves smallest writable unit supported by the driver
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @return Write block size in bytes (may differ from physical page size due to caching)
 */
size_t flash_get_write_block_size(FLASH_DEV *dev);

/**
 * @struct flash_pages_info
 * @brief Information about flash pages at specific offsets
 *        Contains location and characteristics of individual flash pages
 */
struct flash_pages_info {
    off_t start_offset; ///< Starting offset from flash base address
    size_t size;        ///< Size of the page in bytes
    int index;          ///< Index number of the page
};

/**
 * @brief Get page information by offset
 *        Populates flash_pages_info structure with details about the page containing the offset
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @param[in] offset Offset within the desired page
 *        @param[out] info Pointer to flash_pages_info structure to fill
 *        @return 0 on success, -EINVAL if invalid page offset
 *        @note Used to determine proper erase/program units for a given offset
 */
int flash_get_page_info_by_offs(FLASH_DEV *dev, off_t offset, struct flash_pages_info *info);

/**
 * @brief Read flash JEDEC ID (24-bit).
 *
 * @param dev      Pointer to flash device.
 * @param flash_id Output JEDEC ID: [23:16]=MID, [15:8]=DID1, [7:0]=DID2.
 * @return 0 on success, negative errno code on fail.
 */
int flash_get_jedec_id(FLASH_DEV *dev, uint32_t *flash_id);

/**
 * @brief Send command to SPI flash device
 *        Low-level command interface for direct SPI transaction control
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @param[in] cmd Command opcode
 *        @param[in] addr Command address parameter
 *        @param[in] bytes Number of data bytes involved
 *        @param[in,out] pdata Pointer to input/output data buffer
 *        @param[out] Retdata Pointer to store return data
 *        @return 0 on success, negative error code on failure
 *        @note Primitive interface used by higher-level operations
 */
int spirom_cmd_send(FLASH_DEV *dev, unsigned int cmd, unsigned int addr,
                   unsigned int bytes, unsigned int* pdata, unsigned int* Retdata);

/**
 * @brief Initialize platform and flash device
 *        Performs system-level initialization required for flash operation
 *        @param[in,out] dev Pointer to FLASH_DEV structure
 *        @param[in] udc0 Platform-specific init parameter 0
 *        @param[in] udc1 Platform-specific init parameter 1
 *        @return 0 on success, negative error code on failure
 *        @note Typically called early in boot sequence before other flash operations
 */
int platform_init(FLASH_DEV *dev, unsigned char udc0, unsigned char udc1);

/**
 * @brief Erase security information from flash
 *        Removes sensitive data stored in security registers
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @param[in] offset Offset of security information to erase
 *        @return 0 on success, negative error code on failure
 *        @warning Permanent operation - erased data cannot be recovered
 */
int flash_security_erase(FLASH_DEV *dev, off_t offset);

/**
 * @brief Read security information from flash
 *        Reads sensitive data stored in security registers
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @param[in] offset Offset of security information to read
 *        @param[out] data Buffer to store security data
 *        @param[in] len Length of data to read
 *        @return 0 on success, negative error code on failure
 *        @note Access restricted to authorized users only
 */
int flash_security_read(FLASH_DEV *dev, off_t offset, void *data, size_t len);

/**
 * @brief Write security information to flash
 *        Stores sensitive data in security registers
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @param[in] offset Offset where security data will be written
 *        @param[in] data Pointer to security data buffer
 *        @param[in] len Length of data to write
 *        @return 0 on success, negative error code on failure
 *        @note Requires special privileges to execute
 */
int flash_security_write(FLASH_DEV *dev, off_t offset, const void *data, size_t len);

/**
 * @brief Read flash status register
 *        Retrieves current state of flash status registers
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @param[in] reg_addr Status register address
 *        @param[out] data_out Pointer to store register value
 *        @return 0 on success, negative error code on failure
 *        @note Different registers provide various status information (WIP, WEL, etc.)
 */
int flash_status_register_get(FLASH_DEV *dev, unsigned char reg_addr, uint32_t *data_out);

/**
 * @brief Set flash status register
 *        Modifies values in flash status registers
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @param[in] reg_addr Status register address
 *        @param[in] data_in Value to write to register
 *        @param[out] data_out Pointer to store updated register value
 *        @return 0 on success, negative error code on failure
 *        @note Use with caution - incorrect settings may affect flash behavior
 */
int flash_status_register_set(FLASH_DEV *dev, unsigned char reg_addr, const uint32_t data_in, uint32_t *data_out);

/**
 * @brief Put MXIC flash into deep power down mode
 *        Reduces power consumption when flash is idle
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @return 0 on success, negative error code on failure
 *        @note Device must be revived with mxic_release_deep_power_down() before normal operation
 */
int mxic_deep_power_down(FLASH_DEV *dev);

/**
 * @brief Release MXIC flash from deep power down
 *        Restores normal operation after deep power down
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @return 0 on success, negative error code on failure
 *        @note Readies device ID as part of wakeup sequence
 */
int mxic_release_deep_power_down(FLASH_DEV *dev);

/**
 * @brief Set MXIC flash write lock bit
 *        Controls write protection at hardware level
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @param[in] value Non-zero to set lock, zero to clear lock
 *        @return 0 on success, negative error code on failure
 *        @note Effects persist until next power cycle unless configured otherwise
 */
int mxic_write_lock_bit(FLASH_DEV *dev, unsigned char value);

/**
 * @brief Read unique ID from flash device
 *        Retrieves manufacturer-programmed device identification
 *        @param[in] dev Pointer to FLASH_DEV structure
 *        @param[out] buff Buffer to store unique ID (typically 64 bits)
 *        @return 0 on success, non-zero error code on failure
 *        @note ID is permanent and unique per device
 */
int mxic_read_uinque_id(FLASH_DEV *dev, unsigned char *buff);

/**
 * @brief Erases the entire flash memory chip.
 *
 * This function sends a command to the flash memory device to perform a full
 * chip erase operation. All data stored in the flash will be cleared, and the
 * memory will be reset to its default erased state.
 *
 * @param dev Pointer to the FLASH_DEV structure representing the flash device.
 * @return 0 on success, non-zero error code on failure.
 */
int mxic_chip_erase(FLASH_DEV *dev);

#endif /* __FLASHROM__ */
