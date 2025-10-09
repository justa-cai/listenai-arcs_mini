#ifndef __FLASHROM__
#define __FLASHROM__
#ifdef __clang__
#include <stddef.h>
#else
#include <sys/types.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include "types.h"

typedef enum {
	RUN_WITHOUT_INT = 0,  //run without interrupt
	RUN_WITH_INT          //run with interrupt
}RUN_MOD;

typedef struct FLASH_DEV {
	unsigned long   base_addr;
	unsigned char   d_width;   //1, 2, 4
	unsigned char   sclk_div;
	unsigned char   run_mod;
	unsigned char   w_protect;
	unsigned long   timeout;
	unsigned char   addr_bytes;// 3,4
	unsigned char   addr_auto;
	void (*interrupt_enable)(void);
	void (*interrupt_disable)(void);
	unsigned char   dualflash_mode;  //0: disable, 1: enable, 0xAD: auto detect
}FLASH_DEV;

typedef struct FLASH_SFDP_TAB {
	char           sig[4];
	unsigned char  rev_minor;
	unsigned char  rev_major;
	unsigned char  nph;        // number of parameter header
	unsigned char  unused;

	unsigned char  id_0;
	unsigned char  rev_minor_0;
	unsigned char  rev_major_0;
	unsigned char  len_0;
	unsigned short ptp_0;
	unsigned short unused_0;
}FLASH_SFDP_TAB;

typedef struct FLASH_SFDP_JEDEC {
	unsigned int JEDEC_ERASE_SIZE             : 2; // bit 0~1
	unsigned int JEDEC_WRITE_GRANULARITY      : 1; // bit 2
	unsigned int JEDEC_WRITE_ENABLE_REQ       : 1; // bit 3
	unsigned int JEDEC_WRITE_ENABLE_OP        : 1; // bit 4
	unsigned int JEDEC_UNUSED_0               : 3; // bit 5~7
	unsigned int JEDEC_ERASE_OPCODE           : 8; // bit 8~15
	unsigned int JEDEC_SUPPORT_FAST_READ      : 1; // bit 16
	unsigned int JEDEC_ADDRESS_BYTES          : 2; // bit 17~18
	unsigned int JEDEC_SUPPORT_DTR            : 1; // bit 19
	unsigned int JEDEC_SUPPORT_FAST_READ122   : 1; // bit 20
	unsigned int JEDEC_SUPPORT_FAST_READ144   : 1; // bit 21
	unsigned int JEDEC_SUPPORT_FAST_READ114   : 1; // bit 22
	unsigned int JEDEC_UNUSED_1               : 9; // bit 23~31
	unsigned int JEDEC_FLASH_MEM_DENS         : 32;// bit 32~63
}FLASH_SFDP_JEDEC;

typedef enum {
	FLASH_SPI_1_INN = 0,
	FLASH_SPI_1_EXT,
	FLASH_SPI_2_PA04_PA07 = 10,
	FLASH_SPI_2_PA08_PA11,
	FLASH_SPI_3_PA12_PA15 = 20,
	FLASH_SPI_3_PA22_PA25,
	FLASH_SPI_3_PA31_PB02,
	FLASH_SPI_RELEASE_DPD = 0x40,
	FLASH_SPI_IGNORE_QE = 0x80,
}FLASH_ENUM;

#define INDIVIDUAL_BLK_PROTECT 0
/*--------------------------------------------*/
/* SPI Flash ROM definition                   */
/*--------------------------------------------*/
#define SPIROM_ID_VERSION      0x1420c2 /* MXIC 25L8006  */
/* #define SPIROM_ID_VERSION   0x1520c2   MXIC 25L1606  */
#define SPIROM_ID_MASK         0x00ffffff
#define SPIROM_OP_READ         0x03      /*-- read --*/
#define SPIROM_OP_QFAST_READ   0xeb /*-- quad read in higher speed --*/
#define SPIROM_OP_QFAST_READA4 0xec /*-- quad read in higher speed --*/
#define SPIROM_OP_FAST_READ    0x0b /*-- read in higher speed --*/
#define SPIROM_OP_FAST_READA4  0x0c /*-- read in higher speed --*/
#define SPIROM_OP_DFAST_READ   0xbb /*-- dual read in higher speed --*/
#define SPIROM_OP_DFAST_READA4 0xbc /*-- dual read in higher speed --*/
#define SPIROM_OP_RDID         0x9f      /*-- manufacturer/device ID read --*/
#define SPIROM_OP_RUID	 	   0x4b /* -- read unique id --*/
#define SPIROM_OP_READ_ID      0x90   /*-- manufacturer/device ID read --*/
#define SPIROM_OP_WREN         0x06      /*-- write enable --*/
#define SPIROM_OP_WRDI         0x04      /*-- write disable --*/
#define SPIROM_OP_PE           0x81       /*-- page erase --*/
#define SPIROM_OP_SE           0x20       /*-- sector erase --*/
#define SPIROM_OP_SEA4         0x21       /*-- sector erase --*/
#define SPIROM_OP_BE           0x52       /*-- block erase 32K --*/
#define SPIROM_OP_BEA4         0x5c       /*-- block erase 32K --*/
#define SPIROM_OP_BE2          0xd8       /*-- block erase 64K --*/
#define SPIROM_OP_BE2A4        0xdc       /*-- block erase 64K --*/
#define SPIROM_OP_PP           0x02        /*-- page program --*/
#define SPIROM_OP_PPA4         0x12        /*-- page program --*/
#define SPIROM_OP_QPP          0x32       /*-- quad page program --*/
#define SPIROM_OP_QPPA4        0x34       /*-- quad page program --*/
#define SPIROM_OP_RDSR         0x05      /*-- read status register --*/
#define SPIROM_OP_RDSR2        0x35     /*-- read status register2 --*/
#define SPIROM_OP_RDSR3        0x15     /*-- read status register3 --*/
#define SPIROM_OP_WRSR         0x01      /*-- write status register --*/
#define SPIROM_OP_WRSR2        0x31     /*-- write status register2 --*/
#define SPIROM_OP_WRSR3        0x11     /*-- write status register3 --*/
#define SPIROM_OP_SBLK         0x36      /*-- single block lock --*/
#define SPIROM_OP_SBULK        0x39     /*-- single block unlock --*/
#define SPIROM_OP_RDBLOCK      0x3C
#define SPIROM_OP_RDSCUR       0x2B
#define SPIROM_OP_WPSEL        0x68
#define SPIROM_OP_GBLK         0x7E
#define SPIROM_OP_GBULK        0x98
#define SPIROM_OP_EN_RESET     0x66 /*-- enable reset --*/
#define SPIROM_OP_RESET        0x99 /*-- reset --*/
#define SPIROM_OP_RDSFDP       0x5A /*-- read SFDP --*/
#define SPIROM_OP_SEC_PRGM     0x42 /*-- program security register --*/
#define SPIROM_OP_SEC_ERASE    0x44 /*-- erase security register --*/
#define SPIROM_OP_SEC_READ     0x48 /*-- read security register --*/
#define SPIROM_OP_POW_DOWN     0xb9 /*-- deep power-down --*/
#define SPIROM_OP_REL_POW_DOWN 0xab /*-- release from deep power-down and read device id --*/
#define SPIROM_OP_CHIP_ERASE   0x60 /*-- chip erase --*/

/*-- bit mask for status register --*/
#define SPIROM_SR_WIP_MASK     0x01   /*-- write in progress --*/
#define SPIROM_SR_WEL_MASK     0x02   /*-- write enable --*/
#define SPIROM_SR_BP_MASK      0x3C    /*-- BP protection mode --*/
#define SPIROM_PAGE_SIZE       0x100    /*-- page size in bytes --*/
#define SPIROM_SECTOR_SIZE     0x1000   /*-- sector size in bytes --*/
#define SPIROM_BLK32_SIZE      0x8000    /*-- block 32K in bytes --*/
#define SPIROM_BLK64_SIZE      0x10000   /*-- block 64K in bytes --*/
#define SPIROM_SECTOR_MASK     0xfffUL  /*-- sector 4K mask --*/
#define SPIROM_BLK32_MASK      0x7fffUL /*-- block 32K mask --*/
#define SPIROM_BLK64_MASK      0xffffUL /*-- block 64K mask --*/
#define SPIROM_PAGE_SIZE       0x100    /*-- page size --*/
#define SPIROM_PAGE_MASK       0xFF

/*-- SPI ROM cmd --*/
#define SPIROM_CMD_READ        0x0
#define SPIROM_CMD_RDID        0x1
#define SPIROM_CMD_RDST        0x2
#define SPIROM_CMD_WREN        0x3
#define SPIROM_CMD_WRDI        0x4
#define SPIROM_CMD_ERASE       0x5
#define SPIROM_CMD_PROGRAM     0x6
#define SPIROM_CMD_LOCK        0x7
#define SPIROM_CMD_UNLOCK      0x8
#define SPIROM_CMD_RDBLOCK     0x9
#define SPIROM_CMD_RDSCUR      0xA
#define SPIROM_CMD_WPSEL       0xB
#define SPIROM_CMD_CHIP_UNLOCK 0xC
#define SPIROM_CMD_WRSR        0xD
#define SPIROM_CMD_RDST2       0xE
#define SPIROM_CMD_WRSR2       0xF
#define SPIROM_CMD_ERASE_B32   0x10
#define SPIROM_CMD_ERASE_B64   0x11
#define SPIROM_CMD_RDST3       0x12
#define SPIROM_CMD_WRSR3       0x13
#define SPIROM_CMD_ERASE_PG    0x14
#define SPIROM_CMD_RESET_DEV   0x15
#define SPIROM_CMD_READ_SFDP   0x16
#define SPIROM_CMD_RUID        0X17
#define SPIROM_CMD_SEC_PRGM    0x18
#define SPIROM_CMD_SEC_ERASE   0x19
#define SPIROM_CMD_SEC_READ    0x1A
#define SPIROM_CMD_PD		   0x1B
#define SPIROM_CMD_REL_PD	   0x1C
#define SPIROM_CMD_ERASE_CHIP  0x1D
#define SPIROM_CMD_REMSID      0X1E



#define FLASH_BLKSIZE SPIROM_SECTOR_SIZE * 16
#define FLASH_RETRY_TIMES 1800000

/**
 *  @brief  initial spi flash device
 *
 *  @return  0 on success, negative errno code on fail.
 */
int flash_init(FLASH_DEV *dev, unsigned char ud0, unsigned char ud1);

/**
 *  @brief  Read data from flash
 *
 *  @param  dev             : flash dev
 *  @param  offset          : Offset (byte aligned) to read
 *  @param  data            : Buffer to store read data
 *  @param  len             : Number of bytes to read.
 *
 *  @return  0 on success, negative errno code on fail.
 */
int flash_read(FLASH_DEV *dev, off_t offset, void *data, size_t len);

/**
 *  @brief  Write buffer into flash memory.
 *
 *  Prior to the invocation of this API, the flash_write_protection_set needs
 *  to be called first to disable the write protection.
 *
 *  @param  dev             : flash device
 *  @param  offset          : starting offset for the write
 *  @param  data            : data to write
 *  @param  len             : Number of bytes to write
 *
 *  @return  0 on success, negative errno code on fail.
 */
int flash_write(FLASH_DEV *dev, off_t offset, const void *data, size_t len);

/**
 *  @brief  Erase part or all of a flash memory
 *
 *  Acceptable values of erase size and offset are subject to
 *  hardware-specific multiples of page size and offset. Please check
 *  the API implemented by the underlying sub driver, for example by
 *  using flash_get_page_info_by_offs() if that is supported by your
 *  flash driver.
 *
 *  Prior to the invocation of this API, the flash_write_protection_set needs
 *  to be called first to disable the write protection.
 *
 *  @param  dev             : flash device
 *  @param  offset          : erase area starting offset
 *  @param  size            : size of area to be erased
 *
 *  @return  0 on success, negative errno code on fail.
 *
 *  @see flash_get_page_info_by_offs()
 *  @see flash_get_page_info_by_idx()
 */
int flash_erase(FLASH_DEV *dev, off_t offset, size_t size);

/**
 *  @brief  Erase part or all of a flash memory
 *
 *  Acceptable values of erase size and offset are subject to
 *  hardware-specific multiples of page size and offset.
 *  Prior to the invocation of this API, the flash_write_protection_set needs
 *  to be called first to disable the write protection.
 *
 *  @param  dev             : flash device
 *  @param  offset          : erase area starting offset
 *  @param  size            : size of area to be erased
 *
 *  @return  0 on success, negative errno code on fail.
 */
int flash_erase_page(FLASH_DEV *dev, off_t offset, size_t size);

/**
 *  @brief  Enable or disable write protection for a flash memory
 *
 *  This API is required to be called before the invocation of write or erase
 *  API. Please note that on some flash components, the write protection is
 *  automatically turned on again by the device after the completion of each
 *  write or erase calls. Therefore, on those flash parts, write protection needs
 *  to be disabled before each invocation of the write or erase API. Please refer
 *  to the sub-driver API or the data sheet of the flash component to get details
 *  on the write protection behavior.
 *
 *  @param  dev             : flash device
 *  @param  enable          : enable or disable flash write protection
 *
 *  @return  0 on success, negative errno code on fail.
 */
int flash_write_protection_set(FLASH_DEV *dev, bool enable);

/**
 *  @brief  Get the minimum write block size supported by the driver
 *
 *  The write block size supported by the driver might differ from the write
 *  block size of memory used because the driver might implements write-modify
 *  algorithm.
 *
 *  @param  dev flash device
 *
 *  @return  write block size in bytes.
 */
size_t flash_get_write_block_size(FLASH_DEV *dev);

/**
 * @struct flash_pages_info
 * @brief Structure representing information about flash pages.
 *
 * This structure holds information about flash pages including the start offset
 * from the base of flash address, the size of the page, and the page index.
 */
struct flash_pages_info {
	off_t start_offset; /* offset from the base of flash address */
	size_t size;
	u32_t index;
};

/**
 *  @brief  Get the size and start offset of flash page at certain flash offset.
 *
 *  @param  dev flash device
 *  @param  offset Offset within the page
 *  @param  info Page Info structure to be filled
 *
 *  @return  0 on success, -EINVAL if page of the offset doesn't exist.
 */
int flash_get_page_info_by_offs(FLASH_DEV *dev, off_t offset,
					  struct flash_pages_info *info);

/**
 * @brief Send command to spirom and read/write data.
 *
 * This function sends a command to the spirom device, along with address and data
 * if required. It can be used to read data from the spirom, write data to it,
 * or perform other operations as specified by the command.
 *
 * @param dev Pointer to the FLASH_DEV structure representing the spirom device.
 * @param cmd Command code to send to the spirom.
 * @param addr Address parameter for the command (if required).
 * @param bytes Number of bytes to read/write (if applicable).
 * @param pdata Pointer to the data buffer for read/write operation (if applicable).
 * @param Retdata Pointer to store returned data (if applicable).
 * @return Returns 0 on success, negative error code on failure.
 */
int spirom_cmd_send(FLASH_DEV *dev, unsigned int cmd, unsigned int addr,
		unsigned int bytes, unsigned int* pdata, unsigned int* Retdata);

/**
 * @brief Initialize the platform and spirom device.
 *
 * This function initializes the platform and sets up the spirom device for
 * communication. It may perform platform-specific initialization tasks as well
 * as configuring the spirom device.
 *
 * @param dev Pointer to the FLASH_DEV structure representing the spirom device.
 * @param udc0 Value for platform-specific initialization parameter 0.
 * @param udc1 Value for platform-specific initialization parameter 1.
 * @return Returns 0 on success, negative error code on failure.
 */
int platform_init(FLASH_DEV *dev, unsigned char udc0, unsigned char udc1);

/**
 * @brief Erase security information in the flash memory.
 *
 * This function erases security information in the flash memory at the specified offset.
 *
 * @param dev Pointer to the FLASH_DEV structure representing the flash device.
 * @param offset Offset from the base of flash memory where security information is located.
 * @return Returns 0 on success, negative error code on failure.
 */
int flash_security_erase(FLASH_DEV *dev, off_t offset);

/**
 * @brief Read security information from the flash memory.
 *
 * This function reads security information from the flash memory at the specified offset
 * and stores it in the provided data buffer.
 *
 * @param dev Pointer to the FLASH_DEV structure representing the flash device.
 * @param offset Offset from the base of flash memory where security information is located.
 * @param data Pointer to the buffer to store the read security information.
 * @param len Length of the security information to read.
 * @return Returns 0 on success, negative error code on failure.
 */
int flash_security_read(FLASH_DEV *dev, off_t offset, void *data, size_t len);

/**
 * @brief Write security information to the flash memory.
 *
 * This function writes security information to the flash memory at the specified offset
 * using the provided data buffer.
 *
 * @param dev Pointer to the FLASH_DEV structure representing the flash device.
 * @param offset Offset from the base of flash memory where security information will be written.
 * @param data Pointer to the buffer containing the security information to be written.
 * @param len Length of the security information to write.
 * @return Returns 0 on success, negative error code on failure.
 */
int flash_security_write(FLASH_DEV *dev, off_t offset, const void *data, size_t len);

/**
 * @brief Get the value of a status register in the flash memory.
 *
 * This function gets the value of a status register in the flash memory at the specified register address.
 *
 * @param dev Pointer to the FLASH_DEV structure representing the flash device.
 * @param reg_addr Address of the status register to be gotten.
 * @param data_out Pointer to store the value read from the status register (if applicable).
 * @return Returns 0 on success, negative error code on failure.
 */
int flash_status_register_get(FLASH_DEV *dev, unsigned char reg_addr, uint32_t *data_out);

/**
 * @brief Set the value of a status register in the flash memory.
 *
 * This function sets the value of a status register in the flash memory at the specified register address.
 *
 * @param dev Pointer to the FLASH_DEV structure representing the flash device.
 * @param reg_addr Address of the status register to be set.
 * @param data_in The value to be written to the status register.
 * @param data_out Pointer to store the value read from the status register (if applicable).
 * @return Returns 0 on success, negative error code on failure.
 */
int flash_status_register_set(FLASH_DEV *dev, unsigned char reg_addr, const uint32_t data_in, uint32_t *data_out);

/**
 * @brief Enter deep power-down mode for MXIC flash memory.
 *
 * This function puts the MXIC flash memory into deep power-down mode, reducing power consumption.
 *
 * @param dev Pointer to the FLASH_DEV structure representing the MXIC flash device.
 * @return Returns 0 on success, negative error code on failure.
 */
int mxic_deep_power_down(FLASH_DEV *dev);

/**
 * @brief Release deep power-down mode for MXIC flash memory.
 *
 * This function releases the MXIC flash memory from deep power-down mode, allowing normal operation.
 *
 * @param dev Pointer to the FLASH_DEV structure representing the MXIC flash device.
 * @return Returns 0 on success, negative error code on failure.
 */
int mxic_release_deep_power_down(FLASH_DEV *dev);

/**
 * @brief Set the write lock bit for MXIC flash memory.
 *
 * This function sets or clears the write lock bit for MXIC flash memory, depending on the specified value.
 *
 * @param dev Pointer to the FLASH_DEV structure representing the MXIC flash device.
 * @param value Value to set the write lock bit (0 for clear, non-zero for set).
 * @return Returns 0 on success, negative error code on failure.
 */
int mxic_write_lock_bit(FLASH_DEV *dev, unsigned char value);

/**
 * @brief Reads the unique ID from the flash memory device.
 *
 * This function sends a command to the flash memory device to read its unique ID.
 * The ID is then stored in the provided buffer.
 *
 * @param dev Pointer to the FLASH_DEV structure representing the flash device.
 * @param buff Pointer to the buffer where the unique ID will be stored.
 * @return 0 on success, non-zero error code on failure.
 */
int mxic_read_uinque_id(FLASH_DEV *dev, unsigned char *buff);

/**
 * @brief Configures the dual flash settings for the system.
 *
 * This function sets up the dual flash configuration by specifying the low and high addresses of Flash0.
 * It is used to configure the memory mapping for dual flash operation.
 *
 * @param flash0_low  The low address of Flash0.
 * @param flash0_high The high address of Flash0.
 * @return None.
 * note: Range of Flash0: [flash0_low - flash0_high - 1], flash0_low and flash0_high should be aligned to 4K
 */
void flash_dualflash_config(uint32_t flash0_low, uint32_t flash0_high);

int flash_id(FLASH_DEV *dev, uint32_t *id_manufacturer, uint32_t *id_device);

#endif
