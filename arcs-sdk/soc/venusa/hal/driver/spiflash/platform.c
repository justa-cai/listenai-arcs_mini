/*! \file platform.c
 *  \brief SPI Flash Driver Platform-Specific Implementation
 *
 *  This file contains hardware-specific implementations for the SPI flash driver,
 *  including register definitions, platform initialization routines, and SPI controller operations.
 */
#include "platform.h"

#include "venusa_ap.h"
#include "spiflash.h"
#include "clock_config.h"

#ifndef printf
#define printf(format, ...)    ((void)0)
#endif


#define inw(reg)                            (*((volatile unsigned int *) (reg))) /*!< Read word from memory-mapped register */
#define outw(reg, data)                     ((*((volatile unsigned int *)(reg)))=(unsigned int)(data)) /*!< Write word to memory-mapped register */

#define REG_SMU_BASE 0xF0100000          /*!< System Management Unit base address */
#define PWCTL_BASE 0xF1A00000             /*!< Power Control base address */
#define ANTOP_BASE 0xF1B00000             /*!< Analog Topology base address */

#define CPE_SPIB_BASE CMN_FLASHC_BASE    /*!< Common SPIB base address (0x46400000) */
#define CPE_SPI2_BASE SPI0_BASE          /*!< SPI2 base address (0x45400000) */
#define CPE_SPI3_BASE SPI1_BASE          /*!< SPI3 base address (0x45500000) */

#define REG_SMU_BASE_16 0x00F01000       /*!< SMU base address in 16MB segment */
#define CPE_SPIB_BASE_16 0x00F0B000      /*!< SPIB base address in 16MB segment */

#define SMU_SYSID_AE100 0x41451          /*!< AE100 system ID */
#define SMU_SYSID_AE210_16MB 0x41452     /*!< AE210 16MB system ID */
#define SMU_SYSID_AE210_4GB 0x41452      /*!< AE210 4GB system ID */
#define SMU_SYSID_AE300_4GB 0x41453      /*!< AE300 4GB system ID */

#define SPI_TX_FIFO 256                  /*!< Transmit FIFO size in bytes */
#define SPI_RX_FIFO 256                  /*!< Receive FIFO size in bytes */

#define MEMMAP_AE100 0                   /*!< AE100 memory mapping mode */
#define MEMMAP_AE210_16MB 1              /*!< AE210 16MB memory mapping mode */
#define MEMMAP_AE210_4GB 2               /*!< AE210 4GB memory mapping mode */
#define MEMMAP_AE300_4GB 3               /*!< AE300 4GB memory mapping mode */
#define MEMMAP_MAX 4                     /*!< Maximum number of memory map modes */


/*===========================================*/
/*  SPI Driver                            */
/*===========================================*/

/*======================================================*/
/* SPIB Register Definitions                */
/*======================================================*/
#define SPIB_REG_VER(base)			(base + 0x00)         /*!< Version Register */
#define SPIB_REG_IFSET(base)		(base + 0x10)          /*!< Interface Settings Register */
#define SPIB_REG_PIO(base)			(base + 0x14)          /*!< Programmable I/O Register */
#define SPIB_REG_DCTRL(base)		(base + 0x20)          /*!< Data Control Register */
#define SPIB_REG_CMD(base)			(base + 0x24)          /*!< Command Register */
#define SPIB_REG_ADDR(base)			(base + 0x28)          /*!< Address Register */
#define SPIB_REG_DATA(base)			(base + 0x2c)          /*!< Data Register */
#define SPIB_REG_CTRL(base)			(base + 0x30)          /*!< Control Register */
#define SPIB_REG_FIFOST(base)		(base + 0x34)          /*!< FIFO Status Register */
#define SPIB_REG_INTEN(base)		(base + 0x38)          /*!< Interrupt Enable Register */
#define SPIB_REG_INTST(base)		(base + 0x3c)          /*!< Interrupt Status Register */
#define SPIB_REG_REGTIMING(base)	(base + 0x40)          /*!< Register Timing Register */
#define SPIB_REG_MEMACCESS(base)	(base + 0x50)          /*!< Memory Access Register */


/*-- Interface Set Register Bitfields --*/
#define SPIB_IF_ADDLEN_MASK 0x00030000   /*!< Address Length Mask */
#define SPIB_IF_DATALEN_MASK 0x00001f00  /*!< Data Length Mask */
#define SPIB_IF_DATAMERGE_MASK 0x00000080 /*!< Data Merge Mask */
#define SPIB_IF_DIR_MASK 0x00000010      /*!< Direction Mask */
#define SPIB_IF_LSB_MASK 0x00000008      /*!< LSB First Mask */
#define SPIB_IF_SLV_MASK 0x00000004      /*!< Slave Select Polarity Mask */
#define SPIB_IF_CPOL_MASK 0x00000002     /*!< Clock Polarity Mask */
#define SPIB_IF_CPHA_MASK 0x00000001     /*!< Clock Phase Mask */

#define SPIB_IF_ADDLEN_OFFSET 16         /*!< Address Length Bit Offset */
#define SPIB_IF_DATALEN_OFFSET 8         /*!< Data Length Bit Offset */
#define SPIB_IF_DATAMERGE_OFFSET 7       /*!< Data Merge Bit Offset */
#define SPIB_IF_DIR_OFFSET 4             /*!< Direction Bit Offset */
#define SPIB_IF_LSB_OFFSET 3             /*!< LSB First Bit Offset */
#define SPIB_IF_SLV_OFFSET 2             /*!< Slave Select Polarity Bit Offset */
#define SPIB_IF_CPOL_OFFSET 1            /*!< Clock Polarity Bit Offset */
#define SPIB_IF_CPHA_OFFSET 0            /*!< Clock Phase Bit Offset */

/*-- Data Control Register Bitfields --*/
#define SPIB_DCTRL_CMDEN_MASK 0x40000000 /*!< Command Enable Mask */
#define SPIB_DCTRL_ADDREN_MASK 0x20000000 /*!< Address Enable Mask */
#define SPIB_DCTRL_TRAMODE_MASK 0x0f000000 /*!< Transfer Mode Mask */
#define SPIB_DCTRL_WCNT_MASK 0x001ff000  /*!< Write Count Mask */
#define SPIB_DCTRL_DYCNT_MASK 0x00000600 /*!< Dummy Cycle Count Mask */
#define SPIB_DCTRL_RCNT_MASK 0x000001ff  /*!< Read Count Mask */
#define SPIB_DCTRL_ADDRFMT_MASK 0x10000000 /*!< Address Format Mask */
#define SPIB_DCTRL_DATAFMT_MASK 0xc00000  /*!< Data Format Mask */
#define SPIB_DCTRL_TOKENEN_MASK 0x200000  /*!< Token Enable Mask */

#define SPIB_DCTRL_CMDEN_OFFSET 30        /*!< Command Enable Bit Offset */
#define SPIB_DCTRL_ADDREN_OFFSET 29      /*!< Address Enable Bit Offset */
#define SPIB_DCTRL_TRAMODE_OFFSET 24     /*!< Transfer Mode Bit Offset */
#define SPIB_DCTRL_WCNT_OFFSET 12        /*!< Write Count Bit Offset */
#define SPIB_DCTRL_DYCNT_OFFSET 9        /*!< Dummy Cycle Count Bit Offset */
#define SPIB_DCTRL_RCNT_OFFSET 0         /*!< Read Count Bit Offset */
#define SPIB_DCTRL_ADDRFMT_OFFSET 28     /*!< Address Format Bit Offset */
#define SPIB_DCTRL_DATAFMT_OFFSET 22     /*!< Data Format Bit Offset */
#define SPIB_DCTRL_TOKENEN_OFFSET 21     /*!< Token Enable Bit Offset */

/*-- Control Register Bitfields --*/
#define SPIB_CTRL_TXFRST_MASK 0x00000004 /*!< Transmit FIFO Reset Mask */
#define SPIB_CTRL_RXFRST_MASK 0x00000002 /*!< Receive FIFO Reset Mask */
#define SPIB_CTRL_SPIRST_MASK 0x00000001 /*!< SPI Core Reset Mask */

/*-- FIFO Status Register Bitfields --*/
#define SPIB_FIFOST_TXFFL_MASK 0x00800000 /*!< Transmit FIFO Full Mask */
#define SPIB_FIFOST_TXFEM_MASK 0x00400000 /*!< Transmit FIFO Almost Full Mask */
#define SPIB_FIFOST_TXFVE_MASK 0x001f0000 /*!< Transmit FIFO Entries Valid Mask */
#define SPIB_FIFOST_RXFFL_MASK 0x00008000 /*!< Receive FIFO Full Mask */
#define SPIB_FIFOST_RXFEM_MASK 0x00004000 /*!< Receive FIFO Almost Full Mask */
#define SPIB_FIFOST_RXFVE_MASK 0x00001f00 /*!< Receive FIFO Entries Valid Mask */
#define SPIB_FIFOST_SPIBSY_MASK 0x00000001 /*!< SPI Busy Mask */

#define SPIB_FIFOST_TXFFL_OFFSET 23       /*!< Transmit FIFO Full Bit Offset */
#define SPIB_FIFOST_TXFEM_OFFSET 22       /*!< Transmit FIFO Almost Full Bit Offset */
#define SPIB_FIFOST_TXFVE_OFFSET 16       /*!< Transmit FIFO Entries Valid Bit Offset */
#define SPIB_FIFOST_RXFFL_OFFSET 15       /*!< Receive FIFO Full Bit Offset */
#define SPIB_FIFOST_RXFEM_OFFSET 14       /*!< Receive FIFO Almost Full Bit Offset */
#define SPIB_FIFOST_RXFVE_OFFSET 8        /*!< Receive FIFO Entries Valid Bit Offset */
#define SPIB_FIFOST_SPIBSY_OFFSET 0       /*!< SPI Busy Bit Offset */
#define SPIB_FIFOST_SPIBSYnRXFEM (SPIB_FIFOST_RXFEM_MASK | SPIB_FIFOST_SPIBSY_MASK) /*!< Combined status mask */


#define SMU_HPCLKSEL_1_4 (0x2 << 1)      /*!< 1:4 clock divider selection */
#define SMU_HPCLKSEL_1_1 (0x0 << 1)      /*!< 1:1 clock divider selection */
#define SMU_REG_CTRL (REG_SMU_BASE + 0x24) /*!< SMU Control Register */
#define SMU_REG_CLK (REG_SMU_BASE + 0x20)  /*!< SMU Clock Register */


#define PW_REG_CTRL  (ANTOP_BASE + 0x2C) /*!< Power Control Register */
#define PW_REG_CTRL2 (ANTOP_BASE + 0x34) /*!< Secondary Power Control Register */


extern int mxic_qd_bit(FLASH_DEV *dev);
extern int mxic_enable_qd(FLASH_DEV *dev);
extern int mxic_disable_qd(FLASH_DEV *dev);
extern int mxic_enable_dc(FLASH_DEV *dev);
extern int mxic_disable_dc(FLASH_DEV *dev);
extern int mxic_get_dc(FLASH_DEV *dev, unsigned char *dc);

/**
 * @brief Initialize SPI flash device and configure hardware interface
 *
 * This function performs platform-specific initialization of the SPI flash device,
 * including reset sequences, timing configuration, address mode detection, and data width setup.
 *
 * @param[in] dev Pointer to FLASH_DEV structure containing device information
 * @param[in] udc0 User configuration byte 0
 * @param[in] udc1 User configuration byte 1
 * @return 0 on success, negative error code on failure
 *
 * The function executes the following steps:
 * 1. Reset user configuration to default values
 * 2. Configure SPI clock timing parameters
 * 3. Auto-detect address mode if enabled
 * 4. Set interface parameters based on detected/configured settings
 * 5. Attempt to enable Quad Data Mode if supported
 * 6. Configure memory access mode based on final data width
 */
_EXT_RAM int platform_init(FLASH_DEV *dev, unsigned char udc0, unsigned char udc1)
{
	int ret = 0;
	unsigned long base = dev->base_addr, usr_cfg;
	unsigned int RetData, SCLK_DIV = dev->sclk_div;  //0xff;	//SCLK is the same as the SPI clock source
	unsigned char val;

	// reset the user config to the default one
	usr_cfg = 0xf5000000;
	outw(base + 0x80, usr_cfg);

	RetData = (spib_get_regtiming(base) & (~0xFF));
	spib_set_regtiming(base, RetData | SCLK_DIV);

	if(dev->addr_auto) {
		RetData = (2 << SPIB_IF_ADDLEN_OFFSET) & SPIB_IF_ADDLEN_MASK;
		RetData |= ((7 << SPIB_IF_DATALEN_OFFSET) & SPIB_IF_DATALEN_MASK) |
				((1 << SPIB_IF_DATAMERGE_OFFSET) & SPIB_IF_DATAMERGE_MASK) | ((0 << SPIB_IF_DIR_OFFSET) & SPIB_IF_DIR_MASK) |
				((0 << SPIB_IF_LSB_OFFSET) & SPIB_IF_LSB_MASK) | ((0 << SPIB_IF_SLV_OFFSET) & SPIB_IF_SLV_MASK) |
				((0 << SPIB_IF_CPOL_OFFSET) & SPIB_IF_CPOL_MASK) | ((0 << SPIB_IF_CPHA_OFFSET) & SPIB_IF_CPHA_MASK);

		spib_set_ifset(base, RetData);

        if((FLASH_SPI_RELEASE_DPD & udc0) == FLASH_SPI_RELEASE_DPD) {
            udc0 &= (~FLASH_SPI_RELEASE_DPD);
            mxic_release_deep_power_down(dev);
        }

		FLASH_SFDP_TAB sfdp;
		ret = spirom_cmd_send(dev, SPIROM_CMD_READ_SFDP, 0x0, sizeof(sfdp), (unsigned int*)&sfdp, NULL);
		if(ret)
			return ret;

		// if JEDEC compliant, read out the address mode
		if(sfdp.sig[0] == 'S' && sfdp.sig[1] == 'F' && sfdp.sig[2] == 'D' && sfdp.sig[3] == 'P') {
			FLASH_SFDP_JEDEC jedec;
			ret = spirom_cmd_send(dev, SPIROM_CMD_READ_SFDP, sfdp.ptp_0, sizeof(jedec), (unsigned int*)&jedec, NULL);
			if(ret)
				return ret;

			if(jedec.JEDEC_ADDRESS_BYTES == 0b10 || jedec.JEDEC_ADDRESS_BYTES == 0b01) {
				dev->addr_bytes = 4;
			} else {
				dev->addr_bytes = 3;
			}
		}
	}

	dev->addr_bytes = (dev->addr_bytes == 4 ? 4 : 3);

	RetData = ((dev->addr_bytes - 1) << SPIB_IF_ADDLEN_OFFSET) & SPIB_IF_ADDLEN_MASK;
	RetData |= ((7 << SPIB_IF_DATALEN_OFFSET) & SPIB_IF_DATALEN_MASK) |
	        ((1 << SPIB_IF_DATAMERGE_OFFSET) & SPIB_IF_DATAMERGE_MASK) | ((0 << SPIB_IF_DIR_OFFSET) & SPIB_IF_DIR_MASK) |
	        ((0 << SPIB_IF_LSB_OFFSET) & SPIB_IF_LSB_MASK) | ((0 << SPIB_IF_SLV_OFFSET) & SPIB_IF_SLV_MASK) |
	        ((0 << SPIB_IF_CPOL_OFFSET) & SPIB_IF_CPOL_MASK) | ((0 << SPIB_IF_CPHA_OFFSET) & SPIB_IF_CPHA_MASK);
    spib_set_ifset(base, RetData);

    if((FLASH_SPI_RELEASE_DPD & udc0) == FLASH_SPI_RELEASE_DPD) {
        mxic_release_deep_power_down(dev);
    }

    extern int spirom_set_addr4(FLASH_DEV *dev, int en);
    spirom_set_addr4(dev, dev->addr_bytes == 4);

    if((FLASH_SPI_DUMMY_SET & udc0) == FLASH_SPI_DUMMY_SET) {
        flash_dummy_cycles_set(dev, udc1);
    }

	do {
		if(dev->d_width == 4) {
			if((FLASH_SPI_IGNORE_QE & udc0) == FLASH_SPI_IGNORE_QE) {
                // check if quad mode enabled
				ret = (mxic_qd_bit(dev) == 1 ? 0 : 1);
			} else {
				ret = mxic_enable_qd(dev);  //let flash support quad mode
			}
            if(ret) {
                dev->d_width = 1;  //roll back to spi mode if quad mode not enabled
                outw(SPIB_REG_MEMACCESS(base), dev->addr_bytes == 4 ? 9 : 1);

                if(FLASH_SPI_IGNORE_QE & udc0)
                	ret = 0;
				break;
			}
			outw(SPIB_REG_MEMACCESS(base), dev->addr_bytes == 4 ? 13 : 5);  //set spi read mode as Quad mode
		} else if(dev->d_width == 2) {
			outw(SPIB_REG_MEMACCESS(base), dev->addr_bytes == 4 ? 12 : 4);  //set spi read mode as Dual mode
		} else {
			outw(SPIB_REG_MEMACCESS(base), dev->addr_bytes == 4 ? 9 : 1);
		}

	} while(0);
    return ret;
}

/*--------------------------------------------*/
/* SPIB Functions                            */
/*--------------------------------------------*/
/**
 * @brief Read current value of Interface Settings Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @return Current value of the Interface Settings Register
 */
_EXT_RAM unsigned int spib_get_ifset(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_IFSET(base));
    return reg;
}

/**
 * @brief Write new value to Interface Settings Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @param[in] reg New value to write to the register
 */
_EXT_RAM void spib_set_ifset(unsigned long base, unsigned int reg)
{
    outw(SPIB_REG_IFSET(base), reg);
}

/**
 * @brief Read current value of Programmable I/O Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @return Current value of the PIO Register
 */
_EXT_RAM unsigned int spib_get_pio(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_PIO(base));
    return reg;
}

/**
 * @brief Write new value to Programmable I/O Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @param[in] reg New value to write to the register
 */
_EXT_RAM void spib_set_pio(unsigned long base, unsigned int reg)
{
    outw(SPIB_REG_PIO(base), reg);
}

/**
 * @brief Read current value of Control Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @return Current value of the Control Register
 */
_EXT_RAM unsigned int spib_get_ctrl(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_CTRL(base));
    return reg;
}

/**
 * @brief Write new value to Control Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @param[in] reg New value to write to the register
 */
_EXT_RAM void spib_set_ctrl(unsigned long base, unsigned int reg)
{
    outw(SPIB_REG_CTRL(base), reg);
}

/**
 * @brief Read current value of FIFO Status Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @return Current value of the FIFO Status Register
 */
_EXT_RAM unsigned int spib_get_fifost(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_FIFOST(base));
    return reg;
}

/**
 * @brief Read current value of Interrupt Enable Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @return Current value of the Interrupt Enable Register
 */
_EXT_RAM unsigned int spib_get_inten(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_INTEN(base));
    return reg;
}

/**
 * @brief Write new value to Interrupt Enable Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @param[in] reg New value to write to the register
 */
_EXT_RAM void spib_set_inten(unsigned long base, unsigned int reg)
{
    outw(SPIB_REG_INTEN(base), reg);
}

/**
 * @brief Read current value of Interrupt Status Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @return Current value of the Interrupt Status Register
 */
_EXT_RAM unsigned int spib_get_intst(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_INTST(base));
    return reg;
}

/**
 * @brief Write new value to Interrupt Status Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @param[in] reg New value to write to the register
 */
_EXT_RAM void spib_set_intst(unsigned long base, unsigned int reg)
{
    outw(SPIB_REG_INTST(base), reg);
}

/**
 * @brief Read current value of Data Control Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @return Current value of the Data Control Register
 */
_EXT_RAM unsigned int spib_get_dctrl(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_DCTRL(base));
    /*check_timeout("read SPIB_REG_DCTRL failed");*/
    return reg;
}

/**
 * @brief Write new value to Data Control Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @param[in] reg New value to write to the register
 */
_EXT_RAM void spib_set_dctrl(unsigned long base, unsigned int reg)
{
    outw(SPIB_REG_DCTRL(base), reg);
}

/**
 * @brief Read current value of Command Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @return Current value of the Command Register
 */
_EXT_RAM unsigned int spib_get_cmd(unsigned long base)
{
    return inw(SPIB_REG_CMD(base));
}

/**
 * @brief Write new value to Command Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @param[in] cmd New command value to write
 */
_EXT_RAM void spib_set_cmd(unsigned long base, unsigned int cmd)
{
    outw(SPIB_REG_CMD(base), cmd);
}

/**
 * @brief Read current value of Address Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @return Current value of the Address Register
 */
_EXT_RAM unsigned int spib_get_addr(unsigned long base)
{
    return inw(SPIB_REG_ADDR(base));
}

/**
 * @brief Write new value to Address Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @param[in] addr New address value to write
 */
_EXT_RAM void spib_set_addr(unsigned long base, unsigned int addr)
{
    outw(SPIB_REG_ADDR(base), addr);
}

/**
 * @brief Read current value of Data Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @return Current value of the Data Register
 */
_EXT_RAM unsigned int spib_get_data(unsigned long base)
{
    return inw(SPIB_REG_DATA(base));
}

/**
 * @brief Write new value to Data Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @param[in] data New data value to write
 */
_EXT_RAM void spib_set_data(unsigned long base, unsigned int data)
{
    outw(SPIB_REG_DATA(base), data);
}

/**
 * @brief Read current value of Register Timing Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @return Current value of the Register Timing Register
 */
_EXT_RAM unsigned int spib_get_regtiming(unsigned long base)
{
    return inw(SPIB_REG_REGTIMING(base));
}

/**
 * @brief Write new value to Register Timing Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @param[in] data New timing value to write
 */
_EXT_RAM void spib_set_regtiming(unsigned long base, unsigned int data)
{
    outw(SPIB_REG_REGTIMING(base), data);
}

/**
 * @brief Prepare Data Control Register value with basic parameters
 *
 * @param[in] cmden Command enable flag
 * @param[in] addren Address enable flag
 * @param[in] tm Transfer mode
 * @param[in] wcnt Write count
 * @param[in] dycnt Dummy cycle count
 * @param[in] rcnt Read count
 * @return Composed Data Control Register value
 */
_EXT_RAM unsigned int spib_prepare_dctrl(unsigned int cmden,
    unsigned int addren,
    unsigned int tm,
    unsigned int wcnt,
    unsigned int dycnt,
    unsigned int rcnt)
{
    unsigned int v[8];
    unsigned int i;
    unsigned int dctrl = 0x0;

    v[0] = ((cmden << SPIB_DCTRL_CMDEN_OFFSET) & SPIB_DCTRL_CMDEN_MASK);
    v[1] = ((addren << SPIB_DCTRL_ADDREN_OFFSET) & SPIB_DCTRL_ADDREN_MASK);
    v[2] = ((tm << SPIB_DCTRL_TRAMODE_OFFSET) & SPIB_DCTRL_TRAMODE_MASK);
    v[3] = ((wcnt << SPIB_DCTRL_WCNT_OFFSET) & SPIB_DCTRL_WCNT_MASK);
    v[4] = ((dycnt << SPIB_DCTRL_DYCNT_OFFSET) & SPIB_DCTRL_DYCNT_MASK);
    v[5] = ((rcnt << SPIB_DCTRL_RCNT_OFFSET) & SPIB_DCTRL_RCNT_MASK);

    for(i = 0; i < 6; i++)
        dctrl |= v[i];
    // printf("dctrl = %x\n", dctrl);
    return dctrl;
}

/**
 * @brief Prepare Data Control Register value with extended parameters
 *
 * @param[in] cmden Command enable flag
 * @param[in] addren Address enable flag
 * @param[in] tm Transfer mode
 * @param[in] wcnt Write count
 * @param[in] dycnt Dummy cycle count
 * @param[in] rcnt Read count
 * @param[in] addrfmt Address format
 * @param[in] datafmt Data format
 * @param[in] tokenen Token enable flag
 * @return Composed Data Control Register value
 */
_EXT_RAM unsigned int spib_prepare_dctrl2(unsigned int cmden,
    unsigned int addren,
    unsigned int tm,
    unsigned int wcnt,
    unsigned int dycnt,
    unsigned int rcnt,
	unsigned int addrfmt,
	unsigned int datafmt,
	unsigned int tokenen)
{
    unsigned int v[9];
    unsigned int i;
    unsigned int dctrl = 0x0;

    v[0] = ((cmden << SPIB_DCTRL_CMDEN_OFFSET) & SPIB_DCTRL_CMDEN_MASK);
    v[1] = ((addren << SPIB_DCTRL_ADDREN_OFFSET) & SPIB_DCTRL_ADDREN_MASK);
    v[2] = ((tm << SPIB_DCTRL_TRAMODE_OFFSET) & SPIB_DCTRL_TRAMODE_MASK);
    v[3] = ((wcnt << SPIB_DCTRL_WCNT_OFFSET) & SPIB_DCTRL_WCNT_MASK);
    v[4] = ((dycnt << SPIB_DCTRL_DYCNT_OFFSET) & SPIB_DCTRL_DYCNT_MASK);
    v[5] = ((rcnt << SPIB_DCTRL_RCNT_OFFSET) & SPIB_DCTRL_RCNT_MASK);
    v[6] = (addrfmt << SPIB_DCTRL_ADDRFMT_OFFSET) & SPIB_DCTRL_ADDRFMT_MASK;
    v[7] = (datafmt << SPIB_DCTRL_DATAFMT_OFFSET) & SPIB_DCTRL_DATAFMT_MASK;
    v[8] = (tokenen << SPIB_DCTRL_TOKENEN_OFFSET) & SPIB_DCTRL_TOKENEN_MASK;

    for(i = 0; i < 9; i++)
        dctrl |= v[i];
    // printf("dctrl = %x\n", dctrl);
    return dctrl;
}

/**
 * @brief Read current value of Version Register
 *
 * @param[in] base Base address of SPIB peripheral
 * @return Current value of the Version Register
 */
_EXT_RAM unsigned int spib_get_version(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_VER(base));
    return reg;
}

/**
 * @brief Check if SPI bus is currently busy
 *
 * @param[in] base Base address of SPIB peripheral
 * @return Non-zero if SPI bus is busy, zero if idle
 */
_EXT_RAM unsigned int spib_get_busy(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_FIFOST(base));
    // printf("spib_get_busy ===0x%08x\n", reg);
    return (reg & SPIB_FIFOST_SPIBSY_MASK);
}

/**
 * @brief Wait for SPI transaction completion
 *
 * @param[in] base Base address of SPIB peripheral
 * @return 0 on success, -1 if timeout occurs
 * @note Uses a fixed timeout value of 1000 iterations
 */
_EXT_RAM int spib_wait_spi(unsigned long base)
{
    unsigned int i;
    unsigned int timeout = 1000;

    for(i = 1; i < timeout; i++) {
        if(spib_get_busy(base) == 0)
            return 0;
    }
    return -1;
}

/**
 * @brief Check if receive FIFO is empty
 *
 * @param[in] base Base address of SPIB peripheral
 * @return Non-zero if receive FIFO is empty, zero otherwise
 */
_EXT_RAM unsigned int spib_get_rx_empty(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_FIFOST(base));
    return (reg & SPIB_FIFOST_RXFEM_MASK);
}

/**
 * @brief Wait for receive FIFO to become empty
 *
 * @param[in] base Base address of SPIB peripheral
 * @return 0 on success, -1 if timeout occurs
 * @note Uses a fixed timeout value of 1000 iterations
 */
_EXT_RAM int spib_wait_rx_empty(unsigned long base)
{
	unsigned int i;
	unsigned int timeout = 1000;

	for(i = 1; i < timeout; i++) {
		if(spib_get_rx_empty(base) == 0)
			return 0;
	}
	return -1;
}

/**
 * @brief Get number of valid entries in receive FIFO
 *
 * @param[in] base Base address of SPIB peripheral
 * @return Number of valid entries in receive FIFO
 */
_EXT_RAM unsigned int spib_get_rx_entries(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_FIFOST(base));
    unsigned int RetData;

    RetData = ((reg & SPIB_FIFOST_RXFVE_MASK) >> SPIB_FIFOST_RXFVE_OFFSET);
    return (RetData);
}

/**
 * @brief Clear both transmit and receive FIFOs
 *
 * @param[in] base Base address of SPIB peripheral
 */
_EXT_RAM void spib_clr_fifo(unsigned long base)
{
    unsigned int spib_ctrl = inw(SPIB_REG_CTRL(base));
    spib_ctrl |= (SPIB_CTRL_TXFRST_MASK | SPIB_CTRL_RXFRST_MASK);
    spib_set_ctrl(base, spib_ctrl);
}

#define  guiWithMultiout  0

/**
 * @brief Execute SPI command with direct data writing
 *
 * @param[in] base Base address of SPIB peripheral
 * @param[in] op_addr Operation address/command
 * @param[in] spib_dctrl Data control register value
 * @note Only used when guiWithMultiout is 0 (single output mode)
 */
_EXT_RAM void spib_exe_cmmd(unsigned long base, unsigned int op_addr, unsigned int spib_dctrl)
{
    /*-- execute command --*/
    if(guiWithMultiout == 0) {
        spib_set_data(base, op_addr);     /*-- push flash command into tx fifo --*/
        spib_set_dctrl(base, spib_dctrl); /*-- set dctrl --*/
        spib_set_cmd(base, 0x0);          /*-- set dummy command to trigger transation start --*/
    }
}

/**
 * @brief Execute SPI command with separate address phase
 *
 * @param[in] base Base address of SPIB peripheral
 * @param[in] op Command opcode
 * @param[in] addr Target address
 * @param[in] spib_dctrl Data control register value
 */
_EXT_RAM void spib_exe_cmmd2(unsigned long base, unsigned int op, unsigned int addr, unsigned int spib_dctrl)
{
    /*-- execute command --*/
	spib_set_dctrl(base, spib_dctrl);
	spib_set_addr(base, addr);
	spib_set_cmd(base, op);
}

/**
 * @brief Read data from receive FIFO into buffer
 *
 * @param[in] base Base address of SPIB peripheral
 * @param[out] pRxdata Pointer to destination buffer
 * @param[in] RxBytes Number of bytes to receive
 * @note Operates in single output mode (guiWithMultiout == 0)
 */
_EXT_RAM void spib_rx_data(unsigned long base, unsigned int* pRxdata, int RxBytes)
{
    unsigned int i, RxWords = 0;
    unsigned int* p_dst_buffer = (unsigned int*)pRxdata;

    if(guiWithMultiout == 0) {
        /*-- wait completion --*/
        while(spib_get_busy(base) != 0) {
            if(spib_get_rx_empty(base) == 0) {
                RxWords = spib_get_rx_entries(base);
                // printf("spib_get_rx_entries: %d\n", RxWords);
                for(i = 0; i < RxWords; i++) {
                    *p_dst_buffer++ = inw(SPIB_REG_DATA(base));
                }
            }
        }
        RxWords = spib_get_rx_entries(base);
        for(i = 0; i < RxWords; i++) {
            *p_dst_buffer++ = inw(SPIB_REG_DATA(base));
        }
    }
}

/**
 * @brief Write data to transmit FIFO from buffer
 *
 * @param[in] base Base address of SPIB peripheral
 * @param[in] pTxdata Pointer to source buffer
 * @param[in] TxBytes Number of bytes to transmit
 * @note Operates in single output mode (guiWithMultiout == 0)
 */
_EXT_RAM void spib_tx_data(unsigned long base, void* pTxdata, int TxBytes)
{
    unsigned int i, j, data;
    unsigned int TxWords = (TxBytes + 3) / 4;
    unsigned int* p_src_buffer = (unsigned int*)pTxdata;
    unsigned int timeout = 8000;
    unsigned int spib_tx_full;

    if(guiWithMultiout == 0) {
        for(i = 0; i < TxWords; i++) {
            for(j = 0; j < timeout; j++) {
                spib_tx_full = (spib_get_fifost(base) & SPIB_FIFOST_TXFFL_MASK);

                if(spib_tx_full == 0) {
                    break;
                }
            }
            if(spib_tx_full) {
                printf("spib_set_fifo: write fifo timeout\n");
                return;
            }

            spib_set_data(base, *p_src_buffer);

            p_src_buffer++;
        }
    }
}
