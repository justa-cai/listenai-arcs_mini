#include "platform.h"

#include "arcs_ap.h"
#include "spiflash.h"
#include "types.h"

#pragma GCC optimize ("-fno-jump-tables")

#undef printf
#define printf(format, ...)    ((void)0)

#define inw(reg)                            (*((volatile unsigned int *) (reg)))
#define outw(reg, data)                     ((*((volatile unsigned int *)(reg)))=(unsigned int)(data))

#define REG_SMU_BASE 0xF0100000
#define PWCTL_BASE 0xF1A00000
#define ANTOP_BASE 0xF1B00000

#define CPE_SPIB_BASE CMN_FLASHC_BASE    /*0x46400000*/
#define CPE_SPI2_BASE SPI0_BASE          /*0x45400000*/
#define CPE_SPI3_BASE SPI1_BASE          /*0x45500000*/

#define REG_SMU_BASE_16 0x00F01000
#define CPE_SPIB_BASE_16 0x00F0B000

#define SMU_SYSID_AE100 0x41451
#define SMU_SYSID_AE210_16MB 0x41452
#define SMU_SYSID_AE210_4GB 0x41452
#define SMU_SYSID_AE300_4GB 0x41453

#define SPI_TX_FIFO 256 // bytes
#define SPI_RX_FIFO 256 // bytes

#define MEMMAP_AE100 0
#define MEMMAP_AE210_16MB 1
#define MEMMAP_AE210_4GB 2
#define MEMMAP_AE300_4GB 3
#define MEMMAP_MAX 4
/*===========================================*/
/*   SPI driver                              */
/*===========================================*/

/*======================================================*/
/* SPIB register definition  */
/*======================================================*/
#define SPIB_REG_VER(base)			(base + 0x00)
#define SPIB_REG_IFSET(base)		(base + 0x10)
#define SPIB_REG_PIO(base)			(base + 0x14)
#define SPIB_REG_DCTRL(base)		(base + 0x20)
#define SPIB_REG_CMD(base)			(base + 0x24)
#define SPIB_REG_ADDR(base)			(base + 0x28)
#define SPIB_REG_DATA(base)			(base + 0x2c)
#define SPIB_REG_CTRL(base)			(base + 0x30)
#define SPIB_REG_FIFOST(base)		(base + 0x34)
#define SPIB_REG_INTEN(base)		(base + 0x38)
#define SPIB_REG_INTST(base)		(base + 0x3c)
#define SPIB_REG_REGTIMING(base)	(base + 0x40)
#define SPIB_REG_MEMACCESS(base)	(base + 0x50)
/*--Interface Set Reg*/
#define SPIB_IF_ADDLEN_MASK 0x00030000
#define SPIB_IF_DATALEN_MASK 0x00001f00
#define SPIB_IF_DATAMERGE_MASK 0x00000080
#define SPIB_IF_DIR_MASK 0x00000010
#define SPIB_IF_LSB_MASK 0x00000008
#define SPIB_IF_SLV_MASK 0x00000004
#define SPIB_IF_CPOL_MASK 0x00000002
#define SPIB_IF_CPHA_MASK 0x00000001

#define SPIB_IF_ADDLEN_OFFSET 16
#define SPIB_IF_DATALEN_OFFSET 8
#define SPIB_IF_DATAMERGE_OFFSET 7
#define SPIB_IF_DIR_OFFSET 4
#define SPIB_IF_LSB_OFFSET 3
#define SPIB_IF_SLV_OFFSET 2
#define SPIB_IF_CPOL_OFFSET 1
#define SPIB_IF_CPHA_OFFSET 0

/*-- Data Control Reg --*/
#define SPIB_DCTRL_CMDEN_MASK 0x40000000
#define SPIB_DCTRL_ADDREN_MASK 0x20000000
#define SPIB_DCTRL_TRAMODE_MASK 0x0f000000
#define SPIB_DCTRL_WCNT_MASK 0x001ff000
#define SPIB_DCTRL_DYCNT_MASK 0x00000600
#define SPIB_DCTRL_RCNT_MASK 0x000001ff
#define SPIB_DCTRL_ADDRFMT_MASK 0x10000000
#define SPIB_DCTRL_DATAFMT_MASK 0xc00000
#define SPIB_DCTRL_TOKENEN_MASK 0x200000
#define SPIB_DCTRL_CMDEN_OFFSET 30
#define SPIB_DCTRL_ADDREN_OFFSET 29
#define SPIB_DCTRL_TRAMODE_OFFSET 24
#define SPIB_DCTRL_WCNT_OFFSET 12
#define SPIB_DCTRL_DYCNT_OFFSET 9
#define SPIB_DCTRL_RCNT_OFFSET 0
#define SPIB_DCTRL_ADDRFMT_OFFSET 28
#define SPIB_DCTRL_DATAFMT_OFFSET 22
#define SPIB_DCTRL_TOKENEN_OFFSET 21
/*-- Control Reg --*/
#define SPIB_CTRL_TXFRST_MASK 0x00000004
#define SPIB_CTRL_RXFRST_MASK 0x00000002
#define SPIB_CTRL_SPIRST_MASK 0x00000001
/*-- FIFO Status Reg --*/
#define SPIB_FIFOST_TXFFL_MASK 0x00800000
#define SPIB_FIFOST_TXFEM_MASK 0x00400000
#define SPIB_FIFOST_TXFVE_MASK 0x001f0000
#define SPIB_FIFOST_RXFFL_MASK 0x00008000
#define SPIB_FIFOST_RXFEM_MASK 0x00004000
#define SPIB_FIFOST_RXFVE_MASK 0x00001f00
#define SPIB_FIFOST_SPIBSY_MASK 0x00000001
#define SPIB_FIFOST_TXFFL_OFFSET 23
#define SPIB_FIFOST_TXFEM_OFFSET 22
#define SPIB_FIFOST_TXFVE_OFFSET 16
#define SPIB_FIFOST_RXFFL_OFFSET 15
#define SPIB_FIFOST_RXFEM_OFFSET 14
#define SPIB_FIFOST_RXFVE_OFFSET 8
#define SPIB_FIFOST_SPIBSY_OFFSET 0
#define SPIB_FIFOST_SPIBSYnRXFEM (SPIB_FIFOST_RXFEM_MASK | SPIB_FIFOST_SPIBSY_MASK)


#define SMU_HPCLKSEL_1_4 (0x2 << 1)
#define SMU_HPCLKSEL_1_1 (0x0 << 1)
#define SMU_REG_CTRL (REG_SMU_BASE + 0x24)
#define SMU_REG_CLK (REG_SMU_BASE + 0x20)

#define PW_REG_CTRL  (ANTOP_BASE + 0x2C)
#define PW_REG_CTRL2 (ANTOP_BASE + 0x34)

extern int mxic_qd_bit(FLASH_DEV *dev);
extern int mxic_enable_qd(FLASH_DEV *dev);
extern int mxic_disable_qd(FLASH_DEV *dev);
extern int mxic_enable_dc(FLASH_DEV *dev);
extern int mxic_disable_dc(FLASH_DEV *dev);
extern int mxic_get_dc(FLASH_DEV *dev, unsigned char *dc);

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

#ifndef CONFIG_RAM_CONSTRAINED
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
#endif

	dev->addr_bytes = (dev->addr_bytes == 4 ? 4 : 3);

	RetData = ((dev->addr_bytes - 1) << SPIB_IF_ADDLEN_OFFSET) & SPIB_IF_ADDLEN_MASK;
	RetData |= ((7 << SPIB_IF_DATALEN_OFFSET) & SPIB_IF_DATALEN_MASK) |
	        ((1 << SPIB_IF_DATAMERGE_OFFSET) & SPIB_IF_DATAMERGE_MASK) | ((0 << SPIB_IF_DIR_OFFSET) & SPIB_IF_DIR_MASK) |
	        ((0 << SPIB_IF_LSB_OFFSET) & SPIB_IF_LSB_MASK) | ((0 << SPIB_IF_SLV_OFFSET) & SPIB_IF_SLV_MASK) |
	        ((0 << SPIB_IF_CPOL_OFFSET) & SPIB_IF_CPOL_MASK) | ((0 << SPIB_IF_CPHA_OFFSET) & SPIB_IF_CPHA_MASK);
    spib_set_ifset(base, RetData);

#ifndef CONFIG_RAM_CONSTRAINED
    if((FLASH_SPI_RELEASE_DPD & udc0) == FLASH_SPI_RELEASE_DPD) {
        mxic_release_deep_power_down(dev);
    }

	extern int spirom_set_addr4(FLASH_DEV *dev, int en);
    spirom_set_addr4(dev, dev->addr_bytes == 4);
#endif

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
/* SPIB function                              */
/*--------------------------------------------*/
_EXT_RAM unsigned int spib_get_ifset(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_IFSET(base));
    return reg;
}

_EXT_RAM void spib_set_ifset(unsigned long base, unsigned int reg)
{
    outw(SPIB_REG_IFSET(base), reg);
}

_EXT_RAM unsigned int spib_get_pio(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_PIO(base));
    return reg;
}

_EXT_RAM void spib_set_pio(unsigned long base, unsigned int reg)
{
    outw(SPIB_REG_PIO(base), reg);
}

_EXT_RAM unsigned int spib_get_ctrl(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_CTRL(base));
    return reg;
}

_EXT_RAM void spib_set_ctrl(unsigned long base, unsigned int reg)
{
    outw(SPIB_REG_CTRL(base), reg);
}

_EXT_RAM unsigned int spib_get_fifost(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_FIFOST(base));
    return reg;
}

_EXT_RAM unsigned int spib_get_inten(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_INTEN(base));
    return reg;
}

_EXT_RAM void spib_set_inten(unsigned long base, unsigned int reg)
{
    outw(SPIB_REG_INTEN(base), reg);
}

_EXT_RAM unsigned int spib_get_intst(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_INTST(base));
    return reg;
}

_EXT_RAM void spib_set_intst(unsigned long base, unsigned int reg)
{
    outw(SPIB_REG_INTST(base), reg);
}

_EXT_RAM unsigned int spib_get_dctrl(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_DCTRL(base));
    /*check_timeout("read SPIB_REG_DCTRL failed");*/
    return reg;
}

_EXT_RAM void spib_set_dctrl(unsigned long base, unsigned int reg)
{
    outw(SPIB_REG_DCTRL(base), reg);
}

_EXT_RAM unsigned int spib_get_cmd(unsigned long base)
{
    return inw(SPIB_REG_CMD(base));
}

_EXT_RAM void spib_set_cmd(unsigned long base, unsigned int cmd)
{
    outw(SPIB_REG_CMD(base), cmd);
}

_EXT_RAM unsigned int spib_get_addr(unsigned long base)
{
    return inw(SPIB_REG_ADDR(base));
}

_EXT_RAM void spib_set_addr(unsigned long base, unsigned int addr)
{
    outw(SPIB_REG_ADDR(base), addr);
}

_EXT_RAM unsigned int spib_get_data(unsigned long base)
{
    return inw(SPIB_REG_DATA(base));
}

_EXT_RAM void spib_set_data(unsigned long base, unsigned int data)
{
    outw(SPIB_REG_DATA(base), data);
}

_EXT_RAM unsigned int spib_get_regtiming(unsigned long base)
{
    return inw(SPIB_REG_REGTIMING(base));
}

_EXT_RAM void spib_set_regtiming(unsigned long base, unsigned int data)
{
    outw(SPIB_REG_REGTIMING(base), data);
}

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

_EXT_RAM unsigned int spib_get_version(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_VER(base));
    return reg;
}

_EXT_RAM unsigned int spib_get_busy(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_FIFOST(base));
    // printf("spib_get_busy ===0x%08x\n", reg);
    return (reg & SPIB_FIFOST_SPIBSY_MASK);
}

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

_EXT_RAM unsigned int spib_get_rx_empty(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_FIFOST(base));
    return (reg & SPIB_FIFOST_RXFEM_MASK);
}

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

_EXT_RAM unsigned int spib_get_rx_entries(unsigned long base)
{
    unsigned int reg = inw(SPIB_REG_FIFOST(base));
    unsigned int RetData;

    RetData = ((reg & SPIB_FIFOST_RXFVE_MASK) >> SPIB_FIFOST_RXFVE_OFFSET);
    return (RetData);
}

_EXT_RAM void spib_clr_fifo(unsigned long base)
{
    unsigned int spib_ctrl = inw(SPIB_REG_CTRL(base));
    spib_ctrl |= (SPIB_CTRL_TXFRST_MASK | SPIB_CTRL_RXFRST_MASK);
    spib_set_ctrl(base, spib_ctrl);
}
#define  guiWithMultiout  0

_EXT_RAM void spib_exe_cmmd(unsigned long base, unsigned int op_addr, unsigned int spib_dctrl)
{
    /*-- execute command --*/
    if(guiWithMultiout == 0) {
        spib_set_data(base, op_addr);     /*-- push flash command into tx fifo --*/
        spib_set_dctrl(base, spib_dctrl); /*-- set dctrl --*/
        spib_set_cmd(base, 0x0);          /*-- set dummy command to trigger transation start --*/
    }
}

_EXT_RAM void spib_exe_cmmd2(unsigned long base, unsigned int op, unsigned int addr, unsigned int spib_dctrl)
{
    /*-- execute command --*/
	spib_set_dctrl(base, spib_dctrl);
	spib_set_addr(base, addr);
	spib_set_cmd(base, op);
}

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
                //printf("spib_set_fifo: write fifo timeout\n");
                return;
            }

            spib_set_data(base, *p_src_buffer);

            p_src_buffer++;
        }
    }
}
