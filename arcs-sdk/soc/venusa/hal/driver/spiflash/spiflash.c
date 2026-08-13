/**
 * @file spiflash.c
 * @brief Low-level control interface for MXIC SPI NOR Flash devices
 *
 * This file implements core power management, configuration locking, and identification
 * operations for MXIC series SPI NOR flash memory devices. The functions provide:
 *   - Deep power down/wakeup sequencing
 *   - Hardware lock bit manipulation
 *   - Device unique ID retrieval
 *   - Direct status register access patterns
 *
 * All operations follow JEDEC SPI NOR flash command protocols with proper
 * write enable sequencing and error checking mechanisms.
 *
 * @details
 *   Contains four primary operation categories:
 *     1. Power Management Functions:
 *        - @ref mxic_deep_power_down() puts device into low-power standby
 *        - @ref mxic_release_deep_power_down() resumes normal operation
 *     2. Security Features:
 *        - @ref mxic_write_lock_bit() programs hardware protection bits
 *     3. Device Information:
 *        - @ref mxic_read_uinque_id() retrieves factory-programmed UID
 *
 * @note
 *   Requires prior initialization of SPI bus parameters through external
 *   configuration interface. Error returns indicate command failure or
 *   timeout conditions during SPI transactions.
 *
 * @author USER
 * @version 1.0
 * @date 2025.06.06
 */

#include <string.h>
#include "platform.h"
#include "spiflash.h"
#include "venusa_ap.h"

#ifdef CFG_RTOS
extern void vPortEnterCritical(void);
extern void vPortExitCritical(void);
#endif   

//define it for not interrupt by isr or other task
#define SPIROM_NO_INTERRUPT

#ifndef printf
#define printf(format, ...)    ((void)0)
#endif

extern int mxic_check(FLASH_DEV *dev);
extern int mxic_program(FLASH_DEV *dev, unsigned int FlashAddr, unsigned char* start, unsigned int DataSize);
extern int mxic_erase(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int DataSize);
extern int mxic_erase_page(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int DataSize);
extern int mxic_read(FLASH_DEV *dev, unsigned int FlashAddr, unsigned char* start, unsigned int DataSize);
extern int mxic_lock(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int DataSize);
extern int mxic_unlock(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int DataSize);
extern int mxic_set_wrsr(FLASH_DEV *dev, unsigned int uiStat);
extern int mxic_enable_qd(FLASH_DEV *dev);
extern int mxic_security_program(FLASH_DEV *dev, unsigned int FlashAddr, unsigned char* start, unsigned int DataSize);
extern int mxic_security_erase(FLASH_DEV *dev, unsigned int FlashAddr);
extern int mxic_security_read(FLASH_DEV *dev, unsigned int FlashAddr, unsigned char* start, unsigned int DataSize);
extern int mxic_deep_power_down(FLASH_DEV *dev);
extern int mxic_release_deep_power_down(FLASH_DEV *dev);
extern int mxic_write_lock_bit(FLASH_DEV *dev, unsigned char value);

static int mxic_wr_en(FLASH_DEV *dev);
static int mxic_wr_rdy(FLASH_DEV *dev);

extern int flash_status_register_get(FLASH_DEV *dev, unsigned char reg_addr, uint32_t *data_out);
extern int flash_status_register_set(FLASH_DEV *dev, unsigned char reg_addr, const uint32_t data_in, uint32_t *data_out);
extern int flash_write_protection_set(FLASH_DEV *dev, bool enable);
extern int flash_dummy_cycles_set(FLASH_DEV *dev, bool dc_enable);

/**
 * @brief Initialize Flash Device with User-Defined Parameters
 *
 * This function performs initialization of the flash device using platform-specific
 * configuration and user-defined parameters. It sets up basic device properties
 * and handles interrupt masking based on run mode configuration.
 *
 * @param dev Pointer to FLASH_DEV structure representing the device
 * @param ud0 First user-defined parameter passed to platform init
 * @param ud1 Second user-defined parameter passed to platform init
 * @return Initialization result code (typically 0 for success)
 */
_EXT_RAM int flash_init(FLASH_DEV *dev, unsigned char ud0, unsigned char ud1)
{
	int ret;

	dev->w_protect = true;
	if(dev->timeout == 0) {
		dev->timeout = FLASH_RETRY_TIMES;
	}
	if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
		vPortEnterCritical();
#else
		disable_GINT();
#endif  
	}

	ret = platform_init(dev, ud0, ud1);
	if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
		vPortExitCritical();
#else
		enable_GINT();
#endif
	}

	return ret;
}

/**
 * @brief Read Data from Flash Memory
 *
 * Reads specified amount of data from flash memory at given offset. Uses critical
 * sections when running without interrupts to ensure thread safety.
 *
 * @param dev Pointer to FLASH_DEV structure
 * @param offset Starting address in flash memory
 * @param data Buffer to store read data
 * @param len Number of bytes to read
 * @return Negative error code on failure, 0 on success
 */
_EXT_RAM int flash_read(FLASH_DEV *dev, off_t offset, void *data, size_t len)
{
	int ret = -1;

	do {
		if(dev == NULL) break;

		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortEnterCritical();
#else
			disable_GINT();
#endif 
		}
		ret = mxic_read(dev, (unsigned int)offset, (unsigned char *)data, (unsigned int)len);
		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortExitCritical();
#else
			enable_GINT();
#endif
		}
	} while(0);

	return ret;
}

/**
 * @brief Write Data to Flash Memory
 *
 * Programs specified data to flash memory at given offset. Only performed if write
 * protection is disabled. Uses critical sections when running without interrupts.
 *
 * @param dev Pointer to FLASH_DEV structure
 * @param offset Starting address in flash memory
 * @param data Buffer containing data to program
 * @param len Number of bytes to program
 * @return Negative error code on failure, 0 on success
 */
_EXT_RAM int flash_write(FLASH_DEV *dev, off_t offset, const void *data, size_t len)
{
	int ret = -1;
	volatile int result;

	if(dev && dev->w_protect == false) {
		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortEnterCritical();
#else
			disable_GINT();
#endif
		}
		ret = mxic_program(dev, (unsigned int)offset, (unsigned char *)data, (unsigned int)len);

		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortExitCritical();
#else
			enable_GINT();
#endif
		}
	}

	return ret;
}

/**
 * @brief Erase Flash Memory Region
 *
 * Erases specified size of flash memory starting at given offset. Only performed if
 * write protection is disabled. Uses critical sections when running without interrupts.
 *
 * @param dev Pointer to FLASH_DEV structure
 * @param offset Starting address for erase operation
 * @param size Number of bytes to erase
 * @return Negative error code on failure, 0 on success
 */
_EXT_RAM int flash_erase(FLASH_DEV *dev, off_t offset, size_t size)
{
	int ret = -1;

	if(dev && dev->w_protect == false) {
		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortEnterCritical();
#else
			disable_GINT();
#endif
		}
		ret = mxic_erase(dev, (unsigned int)offset, (unsigned int)size);
		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortExitCritical();
#else
			enable_GINT();
#endif
		}
	}
	return ret;
}

/**
 * @brief Erase Single Flash Page
 *
 * Erases a single flash page at specified offset. Only performed if write protection
 * is disabled. Uses critical sections when running without interrupts.
 *
 * @param dev Pointer to FLASH_DEV structure
 * @param offset Page address to erase
 * @param size Page size specification
 * @return Negative error code on failure, 0 on success
 */
_EXT_RAM int flash_erase_page(FLASH_DEV *dev, off_t offset, size_t size)
{
	int ret = -1;

	if(dev && dev->w_protect == false) {
		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortEnterCritical();
#else
			disable_GINT();
#endif
		}
		ret = mxic_erase_page(dev, (unsigned int)offset, (unsigned int)size);
		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortExitCritical();
#else
			enable_GINT();
#endif
		}
	}
	return ret;
}

/**
 * @brief Secure Read from Flash Memory
 *
 * Performs authenticated read operation from secure flash regions. Uses critical
 * sections when running without interrupts to maintain security during access.
 *
 * @param dev Pointer to FLASH_DEV structure
 * @param offset Starting address for secure read
 * @param data Buffer to store read data
 * @param len Number of bytes to read
 * @return Negative error code on failure, 0 on success
 */
_EXT_RAM int flash_security_read(FLASH_DEV *dev, off_t offset, void *data, size_t len)
{
	int ret = -1;

	do {
		if(dev == NULL) break;

		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortEnterCritical();
#else
			disable_GINT();
#endif
		}
		ret = mxic_security_read(dev, (unsigned int)offset, (unsigned char *)data, (unsigned int)len);
		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortExitCritical();
#else
			enable_GINT();
#endif
		}
	} while(0);

	return ret;
}

/**
 * @brief Secure Program Flash Memory
 *
 * Programs data to secure flash regions with authentication. Only performed if write
 * protection is disabled. Uses critical sections when running without interrupts.
 *
 * @param dev Pointer to FLASH_DEV structure
 * @param offset Starting address for secure programming
 * @param data Buffer containing data to program
 * @param len Number of bytes to program
 * @return Negative error code on failure, 0 on success
 */
_EXT_RAM int flash_security_write(FLASH_DEV *dev, off_t offset, const void *data, size_t len)
{
	int ret = -1;

	if(dev && dev->w_protect == false) {
		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortEnterCritical();
#else
			disable_GINT();
#endif
		}
		ret = mxic_security_program(dev, (unsigned int)offset, (unsigned char *)data, (unsigned int)len);
		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortExitCritical();
#else
			enable_GINT();
#endif
		}
	}

	return ret;
}

/**
 * @brief Secure Erase Flash Region
 *
 * Performs authenticated erase operation on secure flash regions. Only performed if
 * write protection is disabled. Uses critical sections when running without interrupts.
 *
 * @param dev Pointer to FLASH_DEV structure
 * @param offset Starting address for secure erase
 * @return Negative error code on failure, 0 on success
 */
_EXT_RAM int flash_security_erase(FLASH_DEV *dev, off_t offset)
{
	int ret = -1;

	if(dev && dev->w_protect == false) {
		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortEnterCritical();
#else
			disable_GINT();
#endif
		}
		ret = mxic_security_erase(dev, (unsigned int)offset);
		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortExitCritical();
#else
			enable_GINT();
#endif
		}
	}
	return ret;
}

/**
 * @brief Get Status Register Value
 *
 * Retrieves current value of specified status register from flash controller.
 * Supports multiple status registers through reg_addr parameter.
 *
 * @param dev Pointer to FLASH_DEV structure
 * @param reg_addr Status register address (1-3)
 * @param data_out Pointer to store retrieved status value
 * @return Negative error code on failure, 0 on success
 */
_EXT_RAM int flash_status_register_get(FLASH_DEV *dev, unsigned char reg_addr, uint32_t *data_out)
{
	int ret = -1;

	unsigned int cmd_rd, cmd_wr;
	int result = 0;

	if(reg_addr == 1) {
		cmd_rd = SPIROM_CMD_RDST;
		cmd_wr = SPIROM_CMD_WRSR;
	} else if (reg_addr == 2) {
		cmd_rd = SPIROM_CMD_RDST2;
		cmd_wr = SPIROM_CMD_WRSR2;
	} else if (reg_addr == 3) {
		cmd_rd = SPIROM_CMD_RDST3;
		cmd_wr = SPIROM_CMD_WRSR3;
	} else {
		return ret;
	}

	if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
		vPortEnterCritical();
#else
		disable_GINT();
#endif
	}

	do {
		result = spirom_cmd_send(dev, cmd_rd, 0x0, 0, NULL, (unsigned int*)data_out);
		if(result) break;

	} while(0);

	ret = result;

	if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
		vPortExitCritical();
#else
		enable_GINT();
#endif
	}

	return ret;
}

/**
 * @brief Set Status Register Value
 *
 * Updates specified status register with new value and optionally returns updated
 * value. Only writable if write protection is disabled.
 *
 * @param dev Pointer to FLASH_DEV structure
 * @param reg_addr Status register address (1-3)
 * @param data_in New status value to write
 * @param data_out Pointer to store updated status value (optional)
 * @return Negative error code on failure, 0 on success
 */
_EXT_RAM int flash_status_register_set(FLASH_DEV *dev, unsigned char reg_addr, const uint32_t data_in, uint32_t *data_out)
{
	int ret = -1;

	if(dev && dev->w_protect == false) {

		unsigned int cmd_rd, cmd_wr;
		unsigned int buf_in, buf_out;
		int result = 0;

		if(reg_addr == 1) {
			cmd_rd = SPIROM_CMD_RDST;
			cmd_wr = SPIROM_CMD_WRSR;
	} else if (reg_addr == 2) {
			cmd_rd = SPIROM_CMD_RDST2;
			cmd_wr = SPIROM_CMD_WRSR2;
		} else if (reg_addr == 3) {
			cmd_rd = SPIROM_CMD_RDST3;
			cmd_wr = SPIROM_CMD_WRSR3;
		} else {
			return ret;
		}

		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortEnterCritical();
#else
			disable_GINT();
#endif
		}

		do {
			result = mxic_wr_en(dev);
			if(result) break;
			result = spirom_cmd_send(dev, cmd_wr, data_in, 0, NULL, &buf_out);
			if(result) break;
			/*-- get enable status --*/
			result = mxic_wr_rdy(dev);
			if(result) break;

			result = spirom_cmd_send(dev, cmd_rd, 0x0, 0, NULL, &buf_out);
			if(result) break;

			*data_out = buf_out;
		} while(0);

		ret = result;

		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortExitCritical();
#else
			enable_GINT();
#endif
		}
	}

	return ret;
}

/**
 * @brief Set Write Protection State
 *
 * Enables or disables global write protection for the flash device. When enabling,
 * also attempts to invalidate any flash cache mechanisms present in hardware.
 *
 * @param dev Pointer to FLASH_DEV structure
 * @param enable True to enable write protection, False to disable
 * @return 0 on success, negative error code on failure
 */
_EXT_RAM int flash_write_protection_set(FLASH_DEV *dev, bool enable)
{
	int ret = -1;

	if(dev) {
		((FLASH_DEV *)dev)->w_protect = enable;
		/// invalidate flash cache
#ifdef FLASHCACHE_BASE
		if(enable && (inw(FLASHCACHE_BASE)&1)){
		    outw(FLASHCACHE_BASE, 0x40);
            outw(FLASHCACHE_BASE, 0x41);
            while(!(inw(FLASHCACHE_BASE + 0x4)&0x2));
		}
#endif
		ret = 0;
	}
	return ret;
}

/**
 * @brief Configure Flash dummy cycle count for high-speed operation
 *
 * This function programs the dummy cycle settings of the internal Flash
 * device to ensure correct read timing at different high SPI/QSPI clock
 * frequencies (e.g. 120 MHz and 133 MHz).
 *
 * Configures the number of dummy cycles used during read and write operations
 * to accommodate timing requirements of specific flash devices.
 *
 * @param dev Pointer to FLASH_DEV structure
 * @param dc_enable True to enable additional dummy cycles, False to disable
 * @return 0 on success, negative error code on failure
 */
_EXT_RAM int flash_dummy_cycles_set(FLASH_DEV *dev, bool enable)
{
	int ret = -1;

	if(dev) {
		unsigned long base = dev->base_addr;

		if(enable) {
			uint32_t dummy_cfg = 0x707; // dummy count = 10( 7+1+2[token] )
			outw(base + 0x90, dummy_cfg);

			uint32_t usr_cfg = 0xf5000030;
			outw(base + 0x80, usr_cfg);
		}

		ret = flash_write_protection_set(dev, false);
		uint32_t status3_rd = 0;
		uint32_t status3_wr = 0;
		ret = flash_status_register_get(dev, 3, &status3_rd);

		status3_wr = (uint8_t)(status3_rd & 0xFF);
		enable = enable ? 1 : 0;
		status3_wr = (status3_wr & ~(1U << 1)) | (enable << 1);

		ret = flash_status_register_set(dev, 3, status3_wr, &status3_rd);
		ret = flash_write_protection_set(dev, true);
	}

	return ret;
}

/**
 * @brief Get Write Block Size Capability
 *
 * Returns the minimum programmable block size supported by the flash device.
 * Currently always returns -1 indicating no special blocking requirements.
 *
 * @param dev Pointer to FLASH_DEV structure
 * @return Block size in bytes (currently always -1)
 */
_EXT_RAM size_t flash_get_write_block_size(FLASH_DEV *dev)
{
	return -1;
}

/**
 * @brief Get Page Information by Offset
 *
 * Maps physical offset to logical page information including index, size and aligned
 * start address based on device sector size configuration.
 *
 * @param dev Pointer to FLASH_DEV structure
 * @param offset Target offset in flash memory
 * @param info Structure to receive page information
 * @return 0 on success, negative error code on failure
 */
_EXT_RAM int flash_get_page_info_by_offs(FLASH_DEV *dev, off_t offset, struct flash_pages_info *info)
{
	info->index = offset / SPIROM_SECTOR_SIZE;
	info->size = SPIROM_SECTOR_SIZE;
	info->start_offset = offset & (~(SPIROM_SECTOR_SIZE - 1));
	return 0;
}

_EXT_RAM int flash_get_jedec_id(FLASH_DEV *dev, uint32_t *flash_id)
{
	int ret = -1;
	unsigned char temp[4] = {0};

	if((dev != NULL) && (flash_id != NULL)) {
		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortEnterCritical();
#else
			disable_GINT();
#endif
		}

		ret = spirom_cmd_send(dev, SPIROM_CMD_RDID, 0x0, 0, NULL, (unsigned int *)temp);
		if(ret == 0) {
			*flash_id = ((uint32_t)temp[0] << 16) | ((uint32_t)temp[1] << 8) | (uint32_t)temp[2];
		}

		if(RUN_WITHOUT_INT == dev->run_mod) {
#ifdef CFG_RTOS
			vPortExitCritical();
#else
			enable_GINT();
#endif
		}
	}

	return ret;
}

/**
 * @brief Prepare SPI Command Word
 *
 * Formats command and address into single SPI instruction word according to protocol specifications.
 *
 * @param cmd Base command opcode
 * @param addr Flash memory address
 * @return Formatted SPI instruction word
 */
_EXT_RAM unsigned int spirom_prepare_cmd(unsigned int cmd, unsigned int addr)
{
    unsigned int b0 = (cmd & 0xff);
    unsigned int b1 = (((addr >> 16) & 0xff) << 8);
    unsigned int b2 = (((addr >> 8) & 0xff) << 16);
    unsigned int b3 = ((addr & 0xff) << 24);
    unsigned int word = (b0 | b1 | b2 | b3);
    return word;
}

/**
 * @brief Send SPI Command to Flash Device
 *
 * Transmits formatted SPI command to flash device and handles data transfer based on
 * command type. Supports various flash operations including read, write, erase and status checks.
 *
 * @param dev Pointer to FLASH_DEV structure
 * @param cmd Command opcode
 * @param addr Flash memory address
 * @param bytes Data length/count
 * @param pdata Data buffer (input/output depending on command)
 * @param Retdata Pointer to store return data
 * @return Negative error code on failure, 0 on success
 */

_EXT_RAM int
spirom_cmd_send(FLASH_DEV *dev, unsigned int cmd, unsigned int addr, unsigned int bytes, unsigned int* pdata, unsigned int* Retdata)
{
    unsigned int spib_dctrl;
    unsigned int op_addr, op;
    unsigned int data = 0;
    unsigned int spib_busy = 0;
    unsigned long spib_rx_empty = 0, base = dev->base_addr;
    /*-- wait if there is active transaction --*/
    spib_busy = spib_wait_spi(base);
    if(spib_busy != 0) {
        return -1;
    }
    /*-- clear tx/rx fifo --*/
    spib_clr_fifo(base);

    /*-- prepare opcode and address --*/
    switch(cmd) {
    case SPIROM_CMD_READ:
    	if(dev->d_width == 4) {
			spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_DY_RD, 0, 1, bytes - 1, 1, 2, 1);
			op = dev->addr_bytes == 4 ? SPIROM_OP_QFAST_READA4 : SPIROM_OP_QFAST_READ;
    	} else if(dev->d_width == 2) {
    		spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_DY_RD, 0, 0, bytes - 1, 1, 1, 0);
    		op = dev->addr_bytes == 4 ? SPIROM_OP_DFAST_READA4 : SPIROM_OP_DFAST_READ;
    	} else {
    		spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_DY_RD, 0, 0, bytes - 1, 0, 0, 0);
    		op = dev->addr_bytes == 4 ? SPIROM_OP_FAST_READA4 : SPIROM_OP_FAST_READ;
    	}
    	spib_exe_cmmd2(base, op, addr, spib_dctrl);
		spib_rx_data(base, pdata, bytes);
    	break;
    case SPIROM_CMD_WREN:
		op_addr = SPIROM_OP_WREN;
		spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
		spib_exe_cmmd(base, op_addr, spib_dctrl);
    	break;
    case SPIROM_CMD_WRDI:
		op_addr = SPIROM_OP_WRDI;
		spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
		spib_exe_cmmd(base, op_addr, spib_dctrl);
    	break;
    case SPIROM_CMD_ERASE:
    	op = dev->addr_bytes == 4 ? SPIROM_OP_SEA4 : SPIROM_OP_SE;
		spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_NONE, 0, 0, 0, 0, 0, 0);
		spib_exe_cmmd2(base, op, addr, spib_dctrl);
    	break;
    case SPIROM_CMD_ERASE_B32:
    	op = dev->addr_bytes == 4 ? SPIROM_OP_BEA4 : SPIROM_OP_BE;
		spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_NONE, 0, 0, 0, 0, 0, 0);
		spib_exe_cmmd2(base, op, addr, spib_dctrl);
		break;
    case SPIROM_CMD_ERASE_B64:
    	op = dev->addr_bytes == 4 ? SPIROM_OP_BE2A4 : SPIROM_OP_BE2;
		spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_NONE, 0, 0, 0, 0, 0, 0);
		spib_exe_cmmd2(base, op, addr, spib_dctrl);
		break;
    case SPIROM_CMD_ERASE_PG:
    	op_addr = spirom_prepare_cmd(SPIROM_OP_PE, addr);
		spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 3, 0, 0);
		spib_exe_cmmd(base, op_addr, spib_dctrl);
    	break;
    case SPIROM_CMD_PROGRAM:
    	if(dev->d_width == 4) {
    		op = dev->addr_bytes == 4 ? SPIROM_OP_QPPA4 : SPIROM_OP_QPP;
    		spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_WRonly, bytes - 1, 0, 0, 0, 2, 0);
    	} else {
    		op = dev->addr_bytes == 4 ? SPIROM_OP_PPA4 : SPIROM_OP_PP;
    		spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_WRonly, bytes - 1, 0, 0, 0, 0, 0);
    	}
    	spib_exe_cmmd2(base, op, addr, spib_dctrl);
		spib_tx_data(base, pdata, bytes);
    	break;
    case SPIROM_CMD_RDID:
		op_addr = SPIROM_OP_RDID;
		spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WR_RD, 0, 0, 2);
		spib_exe_cmmd(base, op_addr, spib_dctrl);
		spib_rx_empty = spib_wait_rx_empty(base);
		if(spib_rx_empty == 0) {
			data = spib_get_data(base);
		}
    	break;
    case SPIROM_CMD_RDST:
	case SPIROM_CMD_RDST2:
	case SPIROM_CMD_RDST3:
		if(cmd == SPIROM_CMD_RDST) {
			op_addr = SPIROM_OP_RDSR;
		}
		else if(cmd == SPIROM_CMD_RDST2) {
			op_addr = SPIROM_OP_RDSR2;
		}
		else {
			op_addr = SPIROM_OP_RDSR3;
		}
		spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WR_RD, 0, 0, 0);
		spib_exe_cmmd(base, op_addr, spib_dctrl);
		spib_rx_empty = spib_wait_rx_empty(base);
		if(spib_rx_empty == 0) {
			data = spib_get_data(base);
		}
		break;
    case SPIROM_CMD_LOCK:
    	op_addr = spirom_prepare_cmd(SPIROM_OP_SBLK, addr);
		// spib_dctrl = spib_prepare_dctrl (0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
		spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 3, 0, 0);
		spib_exe_cmmd(base, op_addr, spib_dctrl);
		break;
    case SPIROM_CMD_UNLOCK:
    	op_addr = spirom_prepare_cmd(SPIROM_OP_SBULK, addr);
		// spib_dctrl = spib_prepare_dctrl (0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
		spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 3, 0, 0);
		spib_exe_cmmd(base, op_addr, spib_dctrl);
		break;
    case SPIROM_CMD_RDBLOCK:
    	op_addr = spirom_prepare_cmd(SPIROM_OP_RDBLOCK, addr);
		spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WR_RD, 3, 0, 0);
		spib_exe_cmmd(base, op_addr, spib_dctrl);
		spib_rx_empty = spib_wait_rx_empty(base);
		if(spib_rx_empty == 0) {
			data = spib_get_data(base);
		}
    	break;
    case SPIROM_CMD_RDSCUR:
		op_addr = SPIROM_OP_RDSCUR;
		spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WR_RD, 0, 0, 0);
		spib_exe_cmmd(base, op_addr, spib_dctrl);
		spib_rx_empty = spib_wait_rx_empty(base);
		if(spib_rx_empty == 0) {
			data = spib_get_data(base);
		}
    	break;
    case SPIROM_CMD_CHIP_UNLOCK:
    	op_addr = SPIROM_OP_GBULK;
		spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
		spib_exe_cmmd(base, op_addr, spib_dctrl);
    	break;
    case SPIROM_CMD_WRSR:
	case SPIROM_CMD_WRSR2:
	case SPIROM_CMD_WRSR3:
		if(cmd == SPIROM_CMD_WRSR) {
			op_addr = (SPIROM_OP_WRSR | (addr << 8));
		}
		else if(cmd == SPIROM_CMD_WRSR2) {
			op_addr = (SPIROM_OP_WRSR2 | (addr << 8));
		}
		else {
			op_addr = (SPIROM_OP_WRSR3 | (addr << 8));
		}
//		spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 1, 0, 0);
		spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, bytes + 1, 0, 0);
		spib_exe_cmmd(base, op_addr, spib_dctrl);
		break;
	case SPIROM_CMD_RESET_DEV:
		spib_dctrl = spib_prepare_dctrl(0x1, 0x0, SPIB_TM_NONE, 0, 0, 0);
		op_addr = SPIROM_OP_EN_RESET;
		spib_exe_cmmd(base, op_addr, spib_dctrl);
		op_addr = SPIROM_OP_RESET;
		spib_exe_cmmd(base, op_addr, spib_dctrl);
		break;
	case SPIROM_CMD_READ_SFDP:
		op_addr = SPIROM_OP_RDSFDP;

		spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_DY_RD, 0, 0, bytes - 1, 0, 0, 0);
		spib_exe_cmmd2(base, op_addr, addr, spib_dctrl);
		spib_rx_data(base, pdata, bytes);
		break;
	case SPIROM_CMD_RUID:
		op_addr = SPIROM_OP_RUID;
		spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_DY_RD, 0, 0, bytes - 1, 0 ,0, 0); //3byte address + 1cycle dummy
		spib_exe_cmmd2(base, op_addr, addr, spib_dctrl);
		spib_rx_data(base, pdata, bytes);
		break;
    case SPIROM_CMD_SEC_PRGM:
    	op = SPIROM_OP_SEC_PRGM;
    	spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_WRonly, bytes - 1, 0, 0, 0, 0, 0);
    	spib_exe_cmmd2(base, op, addr, spib_dctrl);
		spib_tx_data(base, pdata, bytes);
    	break;
    case SPIROM_CMD_SEC_ERASE:
    	op = SPIROM_OP_SEC_ERASE;
		spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_NONE, 0, 0, 0, 0, 0, 0);
		spib_exe_cmmd2(base, op, addr, spib_dctrl);
    	break;
    case SPIROM_CMD_SEC_READ:
    	op = SPIROM_OP_SEC_READ;
    	spib_dctrl = spib_prepare_dctrl2(0x1, 0x1, SPIB_TM_DY_RD, 0, 0, bytes - 1, 0, 0, 0);
    	spib_exe_cmmd2(base, op, addr, spib_dctrl);
		spib_rx_data(base, pdata, bytes);
    	break;
    case SPIROM_CMD_PD:
    	op = SPIROM_OP_POW_DOWN;
    	spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
    	spib_exe_cmmd(base, op, spib_dctrl);
    	break;
    case SPIROM_CMD_REL_PD:
    	op = SPIROM_OP_REL_POW_DOWN;
    	spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
    	spib_exe_cmmd(base, op, spib_dctrl);
    	break;
    case SPIROM_CMD_ERASE_CHIP:
    	op = SPIROM_OP_CHIP_ERASE;
    	spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
    	spib_exe_cmmd(base, op, spib_dctrl);
    	break;
	default:
    	printf("spirom_cmd_send: wrong cmd\n");
		return -1;
    	break;
    }
	spib_busy = spib_wait_spi(base);
    //errors
    if(spib_busy || spib_rx_empty) {
    	return -1;
    }
    *Retdata = data;
    return 0;
}

/**
 * @brief Enable write operations on the flash device
 *
 * Sends Write Enable (WREN) command and polls status register until write enable latch is set.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @return 0 on success, -1 on failure
 * @note Must be called before any program or erase operations
 */
_EXT_RAM static int mxic_wr_en(FLASH_DEV *dev)
{
	unsigned int result, RetData, j;

	result = spirom_cmd_send(dev, SPIROM_CMD_WREN, 0x0, 0, NULL, &RetData);
	if(result != 0) {
		printf("mxic_program: (program) page %d enable write fail\n", i);
		return -1;
	}
	for(j = 1; j < dev->timeout; j++) {
		/*-- get enable status --*/
		result = spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &RetData);
		if(result != 0) {
			printf("mxic_program: get (program) page %d write enable status fail\n", i);
			return -1;
		}
		if(RetData & SPIROM_SR_WEL_MASK)
			break;
	}
	if((RetData & SPIROM_SR_WEL_MASK) == 0) {
		printf("mxic_program: (program) page %d, write enable is not set (status %x)\n", i, RetData);
		return -1;
	}
	return 0;
}

/**
 * @brief Check if device is ready for next operation
 *
 * Polls status register until both Write In Progress (WIP) and Write Enable Latch (WEL) bits are clear.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @return 0 when device is ready, -1 on failure
 * @note Call after program/erase operations to check completion status
 */
_EXT_RAM static int mxic_wr_rdy(FLASH_DEV *dev)
{
	unsigned int result, RetData = 0, j;

	for(j = 1; j < dev->timeout; j++) {
		result = spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &RetData);
		if(result != 0) {
			printf("mxic_program: get (program) page %d write enable status fail\n", i);
			return -1;
		}
		if((RetData & (SPIROM_SR_WIP_MASK | SPIROM_SR_WEL_MASK)) == 0)
			break;
	}
	if((RetData & (SPIROM_SR_WIP_MASK | SPIROM_SR_WEL_MASK)) != 0) {
		printf("mxic_program: (program) page %d, write enable is not clear (status %x)\n", i, RetData);
		return -1;
	}
	return 0;
}

/**
 * @brief Set extended address register value
 *
 * Reads current extended address, compares with target address, and updates if MSB differs.
 * Uses special command sequence required by MXIC flash protocol.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @param[in] addr New extended address value to set
 * @return Result code from last SPI transaction (0 = success)
 * @note Address bit24 comparison triggers update mechanism
 */
_EXT_RAM int spirom_set_extaddr(FLASH_DEV *dev, unsigned int addr)
{
#define EXT_ADDR_W     0xc5
#define EXT_ADDR_R     0xc8
	int result = 0;
	unsigned int op_addr, spib_dctrl, data;
	unsigned long base = dev->base_addr;

	data = 0;
	// read extend addr
	spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WR_RD, 0, 0, 0);
	spib_exe_cmmd(base, EXT_ADDR_R, spib_dctrl);
	result = spib_wait_rx_empty(base);
	if(result == 0) {
		data = spib_get_data(base);
	}

	// bit24 of address is different, write it
	if((addr >> 24) != data) {
		do {
			result = mxic_wr_en(dev);
			if(result) break;

			addr = (addr >> 16) & 0xff00;
			op_addr = EXT_ADDR_W | addr;
			spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 1, 0, 0);
			spib_exe_cmmd(base, op_addr, spib_dctrl);

			result = spib_wait_spi(base);
		} while(0);
	}

	return result;
}

/**
 * @brief Control 4th address byte feature
 *
 * Sets or clears the 4th address byte mode using manufacturer-specific commands.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @param[in] en Non-zero to enable, zero to disable
 * @return Result code from SPI transaction (0 = success)
 * @note Uses dedicated enable/disable commands defined in hardware spec
 */
_EXT_RAM int spirom_set_addr4(FLASH_DEV *dev, int en)
{
#define EXT_SET_ADDR4     0xb7
#define EXT_CLR_ADDR4     0xe9
	unsigned int op_addr, spib_dctrl;
	unsigned long base = dev->base_addr;

	if(en) op_addr = EXT_SET_ADDR4;
	else   op_addr = EXT_CLR_ADDR4;
	spib_dctrl = spib_prepare_dctrl(0x0, 0x0, SPIB_TM_WRonly, 0, 0, 0);
	spib_exe_cmmd(base, op_addr, spib_dctrl);

	return spib_wait_spi(base);
}

/**
 * @brief Unlock chip protection mechanisms
 *
 * Performs complete unlock sequence including write enable, chip unlock command,
 * and verification of unlock status.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @return 0 on successful unlock, -1 on failure
 * @note Required before modifying protected sectors or security registers
 */
_EXT_RAM int mxic_unlock_chip(FLASH_DEV *dev)
{
    unsigned int RetData;
    int result = 0;
    do {
		/*-- write enable --*/
		result = mxic_wr_en(dev);
		if(result) break;
		/*-- Chip-UnLock --*/
		result = spirom_cmd_send(dev, SPIROM_CMD_CHIP_UNLOCK, 0x0, 0, NULL, &RetData);
		if(result) break;
		/*-- get ULock status --*/
		result = mxic_wr_rdy(dev);
		if(result) break;
		result = spirom_cmd_send(dev, SPIROM_CMD_RDBLOCK, 0, 0, NULL, &RetData);
		if(result) break;

		if((RetData & 0xFF) == 0xFF) {
			printf("mxic_unlock_chip: ULock-chip fail\n");
			result = -1;
		}
    } while(0);

    return result;
}

/**
 * @brief Identify connected flash chip type
 *
 * Reads JEDEC ID bytes via RDID command and constructs manufacturer ID code.
 * Also configures SPI clock divisor for proper operation.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @return Result code from last SPI transaction (0 = success)
 * @note Prints identified Flash type to console
 */
// TODO: modify this function to target-specific function - check flash
_EXT_RAM int mxic_check(FLASH_DEV *dev)
{
    unsigned long result, RetData, FlashId, i, base = dev->base_addr;
    unsigned char temp[4];
    unsigned int SCLK_DIV = 0xff;	//SCLK is the same as the SPI clock source

    RetData = (spib_get_regtiming(base) & (~0xFF));
    spib_set_regtiming(base, RetData | SCLK_DIV);
    SCLK_DIV = spib_get_regtiming(base);
    printf("mxic_check: SCLK_DIV=%x\n", SCLK_DIV);

    do {
		result = spirom_cmd_send(dev, SPIROM_CMD_RDID, 0x0, 0, NULL, (unsigned int*)temp);
		if(result != 0) {
			printf("ERROR: read spi rom id fail\n");
			break;
		}

		FlashId = temp[0] << 16 | temp[1] << 8 | temp[2];

		printf("Flash type is 0x%08x\n", FlashId);
    } while(0);

    return result;
}

/**
 * @brief Erase one or more sectors/blocks
 *
 * Implements intelligent block/sector erasure based on alignment and size requirements.
 * Supports 64KB blocks, 32KB blocks, and standard sectors.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @param[in] FlashAddr Starting address for erase operation
 * @param[in] DataSize Number of bytes to erase
 * @return 0 on success, -1 on failure
 * @note Auto-selects most efficient erase unit size
 */
// TODO: modify this function to target-specific function - erase flash
_EXT_RAM int mxic_erase(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int DataSize)
{
    unsigned int EraseAddrStart, EraseSize, EraseSectorCnt, /*EraseSectorIndex,*/ i, j;
    unsigned int result, RetData, timeout = dev->timeout;

    EraseAddrStart = (FlashAddr / SPIROM_SECTOR_SIZE) * SPIROM_SECTOR_SIZE;
    EraseSize = (FlashAddr - EraseAddrStart) + DataSize;
    EraseSectorCnt = (EraseSize + (SPIROM_SECTOR_SIZE - 1)) / SPIROM_SECTOR_SIZE;
//    EraseSectorIndex = (EraseAddrStart / SPIROM_SECTOR_SIZE);

    for(i = 0; i < EraseSectorCnt; ) {
        /*---------------------*/
        /*-- ERASE procedure   */
        /*---------------------*/
    	/*-- write enable --*/
    	result = mxic_wr_en(dev);
    	if(result) break;
        /*-- erase --*/
        if(!(EraseAddrStart & SPIROM_BLK64_MASK) && (EraseSectorCnt >= (SPIROM_BLK64_SIZE / SPIROM_SECTOR_SIZE + i))) {
			result = spirom_cmd_send(dev, SPIROM_CMD_ERASE_B64, EraseAddrStart, 0, NULL, &RetData);
			EraseAddrStart += SPIROM_BLK64_SIZE;
			i += SPIROM_BLK64_SIZE / SPIROM_SECTOR_SIZE;
		} else if(!(EraseAddrStart & SPIROM_BLK32_MASK) && (EraseSectorCnt >= (SPIROM_BLK32_SIZE / SPIROM_SECTOR_SIZE + i))) {
			result = spirom_cmd_send(dev, SPIROM_CMD_ERASE_B32, EraseAddrStart, 0, NULL, &RetData);
			EraseAddrStart += SPIROM_BLK32_SIZE;
			i += SPIROM_BLK32_SIZE / SPIROM_SECTOR_SIZE;
		} else {
			result = spirom_cmd_send(dev, SPIROM_CMD_ERASE, EraseAddrStart, 0, NULL, &RetData);
			EraseAddrStart += SPIROM_SECTOR_SIZE;
			i += 1;
		}
        if(result != 0) {
            printf("mxic_erase: rom erase fail\n");
            break;
        }

        /*-- get erase status --*/
        result = mxic_wr_rdy(dev);
        if(result) break;
//        EraseAddrStart += SPIROM_SECTOR_SIZE;
//        EraseSectorIndex++;
    }

    return result;
}

/**
 * @brief Erase a single page
 *
 * Standard page erase operation with write enable and busy waiting.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @param[in] FlashAddr Page address to erase
 * @param[in] DataSize Number of bytes to erase (must match page size)
 * @return 0 on success, -1 on failure
 * @note Simpler version of mxic_erase() optimized for single page operations
 */
_EXT_RAM int mxic_erase_page(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int DataSize)
{
    unsigned int EraseAddrStart, EraseSize, EraseCnt, /*EraseSectorIndex,*/ i, j;
    unsigned int result, RetData, timeout = dev->timeout;

    EraseAddrStart = (FlashAddr / SPIROM_PAGE_SIZE) * SPIROM_PAGE_SIZE;
    EraseSize = (FlashAddr - EraseAddrStart) + DataSize;
    EraseCnt = (EraseSize + (SPIROM_PAGE_SIZE - 1)) / SPIROM_PAGE_SIZE;
//    EraseSectorIndex = (EraseAddrStart / SPIROM_PAGE_SIZE);

    for(i = 0; i < EraseCnt; ) {
    	/*-- write enable --*/
    	result = mxic_wr_en(dev);
    	if(result) break;
        /*-- erase --*/
		result = spirom_cmd_send(dev, SPIROM_CMD_ERASE_PG, EraseAddrStart, 0, NULL, &RetData);
		EraseAddrStart += SPIROM_PAGE_SIZE;
		i += 1;
        if(result != 0) {
            printf("mxic_erase: rom erase fail\n");
            break;
        }

        /*-- get erase status --*/
        result = mxic_wr_rdy(dev);
        if(result) break;
//        EraseAddrStart += SPIROM_PAGE_SIZE;
//        EraseSectorIndex++;
    }

    return result;
}

/**
 * @brief Program flash memory with word-aligned data
 *
 * Core programming routine that handles page boundary crossing and writes in chunks.
 * Designed for internal use by mxic_program().
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @param[in] FlashAddr Destination address in flash
 * @param[in] start Pointer to source data buffer
 * @param[in] DataSize Number of bytes to program
 * @return 0 on success, -1 on failure
 * @note Operates on 32-bit aligned data boundaries
 */
_EXT_RAM int _mxic_program(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int* start, unsigned int DataSize)
{
    unsigned int result, RetData, *pdata = (unsigned int *)start; // k;
    unsigned int j, timeout = dev->timeout;

    unsigned int step_size, remain_size = DataSize;

    do {
		step_size = SPIROM_PAGE_SIZE - (FlashAddr & SPIROM_PAGE_MASK);
		step_size = (step_size > remain_size) ? remain_size : step_size;
		/*---------------------------*/
		/*-- PAGE PROGRAM procedure  */
		/*---------------------------*/
		/*-- write enable --*/
		result = mxic_wr_en(dev);
		if(result) break;

		// printf("send program cmd......\n");
		result = spirom_cmd_send(dev, SPIROM_CMD_PROGRAM, FlashAddr, step_size, pdata, &RetData);
		if(result != 0) {
			printf("mxic_program: (program) page %d fail\n", i);
			break;
		}
		/*-- ckeck completion --*/
		result = mxic_wr_rdy(dev);
		if(result) break;

		FlashAddr += step_size;
		pdata = (unsigned int *)((unsigned int)pdata + step_size);
		remain_size -= step_size;
    }while(remain_size);

    return result;
}

/**
 * @brief Copy memory with byte-level precision
 *
 * Simple memcpy replacement ensuring exact byte replication.
 *
 * @param[out] dst Destination buffer
 * @param[in] src Source buffer
 * @param[in] size Number of bytes to copy
 */
_EXT_RAM void mem_cpy(void *dst, void *src, int size)
{
	for(int i = 0; i < size; i++) {
		((unsigned char *)dst)[i] = ((unsigned char *)src)[i];
	}
}

/**
 * @brief Program flash memory with arbitrary data alignment
 *
 * Handles unaligned data by splitting operation into aligned segments.
 * First copies data to aligned buffer, programs in chunks, then handles remainder.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @param[in] FlashAddr Destination address in flash
 * @param[in] start Pointer to source data buffer
 * @param[in] DataSize Number of bytes to program
 * @return 0 on success, -1 on failure
 * @note Public interface for complex programming scenarios
 */
_EXT_RAM int mxic_program(FLASH_DEV *dev, unsigned int FlashAddr, unsigned char* start, unsigned int DataSize)
{
	unsigned int result, RetData, FirstWrite, MidWrite, LastWrite, addr;
	unsigned char data[4] __attribute__((aligned(4)));
	unsigned char *pdata = start;

	do {
		//handle non 4 bytes aligned buffer and size
		FirstWrite = 4 - ((unsigned int)start & 0x3);
		if(FirstWrite == 4) {
			FirstWrite = 0;
		}
		if(DataSize > FirstWrite) {
			LastWrite = (DataSize - FirstWrite) & 0x3;
			MidWrite = DataSize - FirstWrite - LastWrite;
		} else {
			FirstWrite = DataSize;
			MidWrite = 0;
			LastWrite = 0;
		}
		addr = FlashAddr;
		if(FirstWrite) {
			//non 4 bytes aligned write
			mem_cpy(data, pdata, FirstWrite);
			result = _mxic_program(dev, addr, (unsigned int*)data, FirstWrite);
			if(result) break;
			pdata += FirstWrite;
			addr += FirstWrite;
		}
		if(MidWrite) {
			//4 bytes aligned write
			result = _mxic_program(dev, addr, (unsigned int*)pdata, MidWrite);
			if(result) break;
			pdata += MidWrite;
			addr += MidWrite;
		}
		if(LastWrite) {
			//non 4 bytes aligned write
			mem_cpy(data, pdata, LastWrite);
			result = _mxic_program(dev, addr, (unsigned int*)data, LastWrite);
		}
	} while(0);

	return result;
}

/**
 * @brief Read flash memory with word-aligned access
 *
 * Bulk reading function that uses maximum efficient transfer size (up to 512 bytes).
 * Designed for internal use by mxic_read().
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @param[in] FlashAddr Source address in flash
 * @param[out] start Destination buffer pointer
 * @param[in] DataSize Number of bytes to read
 * @return 0 on success, -1 on failure
 * @note Operates on 32-bit aligned data boundaries
 */
_EXT_RAM int _mxic_read(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int start, unsigned int DataSize)
{
    unsigned int result, RetData, CurrSize;
    /*-- SPIB_DCTRL_RCNT_MASK(0x1ff) --*/
    while(DataSize) {
        if(DataSize >= 0x200)
            CurrSize = 0x200;
        else
            CurrSize = DataSize;
        result = spirom_cmd_send(dev, SPIROM_CMD_READ, FlashAddr, CurrSize, (unsigned int*)start, &RetData);
        if(result != 0) {
            printf("Flash_Read: fail\n");
            break;
        }
        FlashAddr += CurrSize;
        start += CurrSize;
        DataSize -= CurrSize;
    }
    return result;
}

/**
 * @brief Read flash memory with arbitrary data alignment
 *
 * Handles unaligned reads by splitting operation into aligned segments.
 * Copies misaligned portions through temporary buffer.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @param[in] FlashAddr Source address in flash
 * @param[out] start Destination buffer pointer
 * @param[in] DataSize Number of bytes to read
 * @return 0 on success, -1 on failure
 * @note Public interface matching mxic_program() behavior
 */
_EXT_RAM int mxic_read(FLASH_DEV *dev, unsigned int FlashAddr, unsigned char* start, unsigned int DataSize)
{
    unsigned int result, FirstRead, MidRead, LastRead, addr;
    unsigned char data[4] __attribute__((aligned(4)));
    unsigned char *pdata = start;

    do {
		//handle non 4 bytes aligned buffer and size
		FirstRead = 4 - ((unsigned int)start & 0x3);
		if(FirstRead == 4) {
			FirstRead = 0;
		}
		if(DataSize > FirstRead) {
			LastRead = (DataSize - FirstRead) & 0x3;
			MidRead = DataSize - FirstRead - LastRead;
		} else {
			FirstRead = DataSize;
			MidRead = 0;
			LastRead = 0;
		}
		addr = FlashAddr;
		if(FirstRead) {
			//non 4 bytes aligned read
			result = _mxic_read(dev, addr, (unsigned int)data, FirstRead);
			if(result) break;
			mem_cpy(pdata, data, FirstRead);
			pdata += FirstRead;
			addr += FirstRead;
		}
		if(MidRead) {
			//4 bytes aligned read
			result = _mxic_read(dev, addr, (unsigned int)pdata, MidRead);
			if(result) break;
			pdata += MidRead;
			addr += MidRead;
		}
		if(LastRead) {
			//non 4 bytes aligned read
			result = _mxic_read(dev, addr, (unsigned int)data, LastRead);
			if(result) break;
			mem_cpy(pdata, data, LastRead);
		}
    } while(0);

    return result;
}

/**
 * @brief Lock flash regions (global protection)
 *
 * Sets status register bits to lock entire chip against further writes.
 * Current implementation provides basic lockdown capability.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @param[in] FlashAddr Dummy parameter (not used)
 * @param[in] DataSize Dummy parameter (not used)
 * @return Always returns 0 (implementation placeholder)
 * @note Real hardware may require different lock granularities
 */
// TODO: modify this function to target-specific function - lock flash
_EXT_RAM int mxic_lock(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int DataSize)
{
#if INDIVIDUAL_BLK_PROTECT
#else
    unsigned int RetData = 0;

    spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &RetData);
    mxic_set_wrsr(dev, RetData | 0x3C);
    spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &RetData);
    printf("mxic_lock: status = %08x \n", RetData);
#endif
    return 0;
}

/**
 * @brief Unlock flash regions (global protection)
 *
 * Clears status register bits enabling writes to previously locked areas.
 * Complementary to mxic_lock().
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @param[in] FlashAddr Dummy parameter (not used)
 * @param[in] DataSize Dummy parameter (not used)
 * @return Always returns 0 (implementation placeholder)
 * @note Requires prior unlock sequence for full functionality
 */
// TODO: modify this function to target-specific function - unlock flash
_EXT_RAM int mxic_unlock(FLASH_DEV *dev, unsigned int FlashAddr, unsigned int DataSize)
{
#if INDIVIDUAL_BLK_PROTECT
#else
    unsigned int RetData = 0;

    spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &RetData);
    mxic_set_wrsr(dev, RetData & ~0x3C);
    spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &RetData);
    printf("mxic_unlock: status = %08x \n", RetData);
#endif
    return 0;
}

/**
 * @brief Write to Status Register 1 (WRSR)
 *
 * Updates device configuration bits after obtaining write enable.
 * Used internally by protection functions.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @param[in] uiStat Value to write to status register
 * @return 0 on success, -1 on failure
 * @note Follows standard write-enable -> write -> wait sequence
 */
_EXT_RAM int mxic_set_wrsr(FLASH_DEV *dev, unsigned int uiStat)
{
    unsigned int result, RetData;

    do {
		result = mxic_wr_en(dev);
		if(result) break;

		/*-- set WRSR --*/
		result = spirom_cmd_send(dev, SPIROM_CMD_WRSR, uiStat, 0, NULL, &RetData);
		if(result) break;

		/*-- get status --*/
		result = mxic_wr_rdy(dev);
    } while(0);

    // printf("mxic_set_wrsr: uiStat=%x RetData=%x\n", uiStat, RetData);
    return result;
}

/**
 * @brief Check Quad Enable (QE) bit status
 *
 * Reads secondary status register to determine if QE mode is active.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @return 1 if QE mode is enabled, 0 otherwise
 * @note Does not modify device state
 */
_EXT_RAM int mxic_qd_bit(FLASH_DEV *dev)
{
	unsigned int buf2;
	int result = 0;
	do {
		result = spirom_cmd_send(dev, SPIROM_CMD_RDST2, 0x0, 0, NULL, &buf2);
		if(result) break;

		if(buf2 & 0x2) {//if it is in QE mode, return 1, else return 0
			result = 1;
		} else {
			result = 0;
		}
	} while(0);

	return result;
}

/**
 * @brief Enable Quad Mode Operation
 *
 * Sets QE bit in status register after verifying current state.
 * Follows complete sequence: read-modify-write with write enable.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @return 0 on success, -1 on failure
 * @note Device must support Quad SPI mode
 */
_EXT_RAM int mxic_enable_qd(FLASH_DEV *dev)
{
	unsigned int buf1, buf2, buf_st12;
	int result = 0;
	do {
		result = spirom_cmd_send(dev, SPIROM_CMD_RDST2, 0x0, 0, NULL, &buf2);
		if(result) break;

		if(buf2 & 0x2) {//if it is in QE mode, return
			break;
		} else {
			buf2 |= 0x2;
			result = spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &buf_st12);
			if(result) break;

			buf_st12 |= (buf2 << 8);

			result = mxic_wr_en(dev);
			if(result) break;
			result = spirom_cmd_send(dev, SPIROM_CMD_WRSR, buf_st12, 1, NULL, &buf1);
			if(result) break;
			/*-- get enable status --*/
			result = mxic_wr_rdy(dev);
			if(result) break;

			result = spirom_cmd_send(dev, SPIROM_CMD_RDST2, 0x0, 0, NULL, &buf2);
			if(result) break;

			if(buf2 & 0x2) {//QE mode enabled
				break;
			} else {
				return -1;
			}
		}
	} while(0);

	return result;
}

/**
 * @brief Disable Quad Mode Operation
 *
 * Clears QE bit in status register following proper sequence.
 * Maintains other status register bits unchanged.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @return 0 on success, -1 on failure
 * @note Device will revert to standard SPI mode
 */
_EXT_RAM int mxic_disable_qd(FLASH_DEV *dev)
{
	unsigned int buf1, buf2, buf_st12;
	int result = 0;
	do {
		result = spirom_cmd_send(dev, SPIROM_CMD_RDST2, 0x0, 0, NULL, &buf2);
		if(result) break;

		if(buf2 & 0x2) {//if it is in QE mode
			buf2 &= ~0x2;
			result = spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &buf_st12);
			if(result) break;

			buf_st12 |= (buf2 << 8);

			result = mxic_wr_en(dev);
			if(result) break;
			result = spirom_cmd_send(dev, SPIROM_CMD_WRSR, buf_st12, 1, NULL, &buf1);
			if(result) break;
			/*-- get enable status --*/
			result = mxic_wr_rdy(dev);
		}
	} while(0);

	return result;
}

/**
 * @brief Enable Deep Power Down Mode
 *
 * Puts device into low power state while maintaining minimal functionality.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @return 0 on success, -1 on failure
 * @note Use mxic_release_deep_power_down() to restore normal operation
 */
_EXT_RAM int mxic_deep_power_down(FLASH_DEV *dev)
{
	unsigned int result, RetData;
	result = spirom_cmd_send(dev, SPIROM_CMD_PD, 0x0, 0, NULL, &RetData);
	if(result != 0) {
		printf("mxic_deep_power_down: fail\n");
	}
    return result;
}

/**
 * @brief Release from Deep Power Down Mode
 *
 * Restores normal device operation after deep power down.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @return 0 on success, -1 on failure
 * @note Must be called before performing any other operations
 */
_EXT_RAM int mxic_release_deep_power_down(FLASH_DEV *dev)
{
	unsigned int result, RetData;
	result = spirom_cmd_send(dev, SPIROM_CMD_REL_PD, 0x0, 0, NULL, &RetData);
	if(result != 0) {
		printf("mxic_release_deep_power_down: fail\n");
	}
    return result;
}

/**
 * @brief Program lock bits in status register
 *
 * Sets specified lock bits after verifying current state. Maintains other bits unchanged.
 * Follows complete read-modify-write sequence with write protection.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @param[in] value Bitmask of lock bits to set
 * @return 0 on success, -1 on failure
 * @note Each bit corresponds to specific protection feature
 */
_EXT_RAM int mxic_write_lock_bit(FLASH_DEV *dev, unsigned char value)
{
	unsigned int result;
	unsigned int buf1, buf2, buf_st12;

	do {
		result = spirom_cmd_send(dev, SPIROM_CMD_RDST2, 0x0, 0, NULL, &buf2);
		if(result) break;

		if(buf2 & value) {
			break;
		} else {
			buf2 |= value;
			result = spirom_cmd_send(dev, SPIROM_CMD_RDST, 0x0, 0, NULL, &buf_st12);
			if(result) break;

			buf_st12 |= (buf2 << 8);

			result = mxic_wr_en(dev);
			if(result) break;
			result = spirom_cmd_send(dev, SPIROM_CMD_WRSR, buf_st12, 0, NULL, &buf1);
			if(result) break;

			result = spirom_cmd_send(dev, SPIROM_CMD_RDST2, 0x0, 0, NULL, &buf2);
			if(result) break;

			if(buf2 & value) {
				break;
			} else {
				return -1;
			}
		}
	} while(0);

	return result;
}

/**
 * @brief Enable Dummy Clock (DC) bit in Status Register 3 of the flash device.
 *        Attempts to set the DC bit if it is not already set. This involves:
 *        1. Reading current Status Register 3 value.
 *        2. If DC bit (LSB) is clear, enable write operations.
 *        3. Write updated register value with DC bit set.
 *        4. Wait for write operation to complete.
 *
 * @param[in] dev Pointer to FLASH_DEV structure representing the target device.
 *            Must not be NULL.
 *
 * @return int Returns 0 on successful completion. Negative values indicate errors during:
 *             - Status register read (@ref spirom_cmd_send)
 *             - Write enable sequence (@ref mxic_wr_en)
 *             - Register write operation (@ref spirom_cmd_send)
 *             - Write ready check (@ref mxic_wr_rdy)
 *          The exact error code depends on which operation failed first.
 *          Positive values may also indicate failure conditions from underlying functions.
 *
 * @note This function modifies the device's Status Register 3 to enable the DC feature.
 *       Multiple attempts are not made - exits on first encountered error.
 */
_EXT_RAM int mxic_enable_dc(FLASH_DEV *dev)
{
    int result = 0;
    unsigned int buf1, buf2;

    do {
        result = spirom_cmd_send(dev, SPIROM_CMD_RDST3, 0x0, 0, NULL, &buf2);
        if(result) break;
        if(buf2 & 0x1) { //if DC bit is set
            break;
        } else {
            buf2 |= 0x1;
            result = mxic_wr_en(dev);
            if(result) break;

            result = spirom_cmd_send(dev, SPIROM_CMD_WRSR3, buf2, 0, NULL, &buf1);
            if(result) break;

            /*-- get enable status --*/
            result = mxic_wr_rdy(dev);
        }
    } while(0);

    return result;
}

/**
 * @brief Disable Dummy Clock (DC) bit in Status Register 3 of the flash device.
 *        Clears the DC bit if it is currently set. Operation sequence includes:
 *        1. Read current Status Register 3 value.
 *        2. If DC bit (LSB) is set, disable write protection temporarily.
 *        3. Write updated register value with DC bit cleared.
 *        4. Wait for write operation to complete.
 *
 * @param[in] dev Pointer to FLASH_DEV structure representing the target device.
 *            Must not be NULL.
 *
 * @return int Returns 0 on successful completion. Negative values indicate errors during:
 *             - Status register read (@ref spirom_cmd_send)
 *             - Write enable sequence (@ref mxic_wr_en)
 *             - Register write operation (@ref spirom_cmd_send)
 *             - Write ready check (@ref mxic_wr_rdy)
 *          The exact error code depends on which operation failed first.
 *          Positive values may also indicate failure conditions from underlying functions.
 *
 * @note This function modifies the device's Status Register 3 to disable the DC feature.
 *       Exits immediately upon encountering any error condition.
 */
_EXT_RAM int mxic_disable_dc(FLASH_DEV *dev)
{
    int result = 0;
    unsigned int buf1, buf2;

    do {
        result = spirom_cmd_send(dev, SPIROM_CMD_RDST3, 0x0, 0, NULL, &buf2);
        if(result) break;
        if(buf2 & 0x1) { //if DC bit is set
            buf2 &= ~0x1;
            result = mxic_wr_en(dev);
            if(result) break;

            result = spirom_cmd_send(dev, SPIROM_CMD_WRSR3, buf2, 0, NULL, &buf1);
            if(result) break;

            /*-- get enable status --*/
            result = mxic_wr_rdy(dev);
        }
    } while(0);

    return result;
}

/**
 * @brief Retrieve current state of the Dummy Clock (DC) bit from Status Register 3.
 *        Reads the LSB of Status Register 3 and stores the result in the provided buffer.
 *
 * @param[in]  dev     Pointer to FLASH_DEV structure representing the target device.
 *                     Must not be NULL.
 * @param[out] dc      Pointer to uint8_t variable where the DC bit status will be stored.
 *                     Value written will be either 0 (disabled) or 1 (enabled).
 *
 * @return int Returns 0 on successful read operation. Non-zero indicates error during
 *             status register read (@ref spirom_cmd_send).
 *
 * @note The output parameter 'dc' must point to valid memory location before calling this function.
 *       No modification of device registers occurs during this operation.
 */
_EXT_RAM int mxic_get_dc(FLASH_DEV *dev, unsigned char *dc)
{
    int result;
    unsigned int buf2;

    do {
        result = spirom_cmd_send(dev, SPIROM_CMD_RDST3, 0x0, 0, NULL, &buf2);
        if(result) break;
        *dc = buf2 & 0x1;
    } while(0);

    return result;
}

/**
 * @brief Security program protected data area
 *
 * Writes data to security protected regions after obtaining proper credentials.
 * Follows standard write enable sequence with special security command.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @param[in] FlashAddr Destination address in secure area
 * @param[in] start Pointer to data buffer
 * @param[in] DataSize Number of bytes to program
 * @return 0 on success, -1 on failure
 * @note Requires prior authentication/unlock sequence
 */
_EXT_RAM int mxic_security_program(FLASH_DEV *dev, unsigned int FlashAddr, unsigned char* start, unsigned int DataSize)
{
	unsigned int result, RetData;

	/*-- write enable --*/
	result = mxic_wr_en(dev);
	if(result) return result;

	result = spirom_cmd_send(dev, SPIROM_CMD_SEC_PRGM, FlashAddr, DataSize, (unsigned int*)start, &RetData);
	if(result != 0) {
		printf("mxic_security_program: fail\n");
		return result;
	}
	/*-- ckeck completion --*/
	result = mxic_wr_rdy(dev);

	return result;
}

/**
 * @brief Erase security protected data area
 *
 * Removes data from security protected regions after authentication.
 * Uses specialized security erase command.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @param[in] FlashAddr Address within secure area to erase
 * @return 0 on success, -1 on failure
 * @note Affects only security protected storage areas
 */
_EXT_RAM int mxic_security_erase(FLASH_DEV *dev, unsigned int FlashAddr)
{
    unsigned int result, RetData;

	/*-- write enable --*/
	result = mxic_wr_en(dev);
	if(result) return result;
	/*-- erase --*/
	result = spirom_cmd_send(dev, SPIROM_CMD_SEC_ERASE, FlashAddr, 0, NULL, &RetData);

	if(result != 0) {
		printf("mxic_security_erase: security erase fail\n");
		return result;
	}

	/*-- get erase status --*/
	result = mxic_wr_rdy(dev);

    return result;
}

/**
 * @brief Read from security protected data area
 *
 * Retrieves data from protected memory regions after authentication.
 * Uses specialized security read command.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @param[in] FlashAddr Source address in secure area
 * @param[out] start Destination buffer pointer
 * @param[in] DataSize Number of bytes to read
 * @return 0 on success, -1 on failure
 * @note Requires proper security level access permissions
 */
_EXT_RAM int mxic_security_read(FLASH_DEV *dev, unsigned int FlashAddr, unsigned char* start, unsigned int DataSize)
{
    unsigned int result, RetData;

	result = spirom_cmd_send(dev, SPIROM_CMD_SEC_READ, FlashAddr, DataSize, (unsigned int*)start, &RetData);
	if(result != 0) {
		printf("mxic_security_read: fail\n");
	}
    return result;
}

/**
 * @brief Read unique identification information from the flash device.
 *
 * Retrieves the 16-byte unique ID stored in the flash chip and copies it to
 * the provided buffer. Useful for device authentication and inventory tracking.
 *
 * @param dev Pointer to the FLASH_DEV structure representing the target device.
 * @param buff Pointer to byte array where the unique ID will be stored.
 *            Must provide at least 16 bytes of storage space.
 *
 * @return 0 on success, non-zero error code on failure.
 *         Error details can be obtained from spirom_cmd_send implementation.
 *
 * @note The unique ID returned is factory programmed and guaranteed globally unique
 *       per JEDEC standards for compliant devices. Data transfer uses direct
 *       memory mapping with 16-byte transaction length.
 */
_EXT_RAM int mxic_read_uinque_id(FLASH_DEV *dev, unsigned char *buff)
{
	unsigned int result, RetData;
	result = spirom_cmd_send(dev, SPIROM_CMD_RUID, 0x0, 16, (unsigned int*)buff, &RetData);

	if(result != 0) {
		printf("mxic_read_uinque_id: fail\n");
	}
	return result;
}

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
_EXT_RAM int mxic_chip_erase(FLASH_DEV* dev)
{
	unsigned int result, retData;

	/*-- write enable --*/
	result = mxic_wr_en(dev);
	if(result) return result;

	/*-- erase chip --*/
	result = spirom_cmd_send(dev, SPIROM_CMD_ERASE_CHIP, 0x0, 0, NULL, &retData);
	if(result != 0) {
		printf("mxic_chip_erase: fail\n");
	}

    result = mxic_wr_rdy(dev);
    if(result) return result;

    return result;
}
