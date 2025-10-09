#ifndef __PLAT_AE210P__
#define __PLAT_AE210P__

#define	FLASH_QUAD_MODE_EN

#ifdef CONFIG_EXT_RAM
#define _EXT_RAM __attribute__((section (CONFIG_ARCS_HAL_EXT_RAM_SECTION)))
#else
#define _EXT_RAM
#endif


#define SPIB_TM_WRsim               0x0
#define SPIB_TM_WRonly              0x1
#define SPIB_TM_RDonly              0x2
#define SPIB_TM_WR_RD               0x3
#define SPIB_TM_RD_WR               0x4
#define SPIB_TM_WR_DY_RD            0x5
#define SPIB_TM_RD_DY_WR            0x6
#define SPIB_TM_NONE				0x7
#define SPIB_TM_DY_WR				0x8
#define SPIB_TM_DY_RD				0x9

#define SPIB_VERSION                0x02002000


extern unsigned int spib_get_ifset (unsigned long base);
extern void spib_set_ifset (unsigned long base, unsigned int reg);
extern unsigned int spib_get_pio (unsigned long base);
extern void spib_set_pio (unsigned long base, unsigned int reg);
extern unsigned int spib_get_ctrl (unsigned long base);
extern void spib_set_ctrl (unsigned long base, unsigned int reg);
/*extern unsigned int spib_get_fifost (void);*/
extern unsigned int spib_get_inten (unsigned long base);
extern void spib_set_inten (unsigned long base, unsigned int reg);
extern unsigned int spib_get_intst (unsigned long base);
extern void spib_set_intst (unsigned long base, unsigned int reg);
extern unsigned int spib_get_dctrl (unsigned long base);
extern void spib_set_dctrl (unsigned long base, unsigned int reg);
extern unsigned int spib_get_cmd(unsigned long base);
extern void spib_set_cmd(unsigned long base, unsigned int cmd);
extern unsigned int spib_get_addr(unsigned long base);
extern void spib_set_addr(unsigned long base, unsigned int addr);
extern unsigned int spib_get_data(unsigned long base);
extern void spib_set_data(unsigned long base, unsigned int data);
extern unsigned int spib_get_regtiming(unsigned long base);
extern void spib_set_regtiming(unsigned long base, unsigned int data);
//extern void spib_get_fifo(unsigned int *buffer, unsigned int start, unsigned int length);
//extern unsigned int spib_set_fifo(unsigned int *buffer, unsigned int start, unsigned int length);
extern unsigned int spib_prepare_dctrl(unsigned int,unsigned int,unsigned int,unsigned int,unsigned int,unsigned int);
extern int spib_wait_spi (unsigned long base);
extern unsigned int spib_get_version (unsigned long base);
extern unsigned int spib_get_busy (unsigned long base);
extern unsigned int spib_get_rx_empty (unsigned long base);
extern int spib_wait_rx_empty(unsigned long base);
extern unsigned int spib_get_rx_entries (unsigned long base);
extern void spib_clr_fifo (unsigned long base);
extern void spib_exe_cmmd (unsigned long base, unsigned int op_addr, unsigned int spib_dctrl);
extern void spib_rx_data (unsigned long base, unsigned int *pRxdata, int RxBytes);
extern void spib_tx_data (unsigned long base, void *pTxdata, int TxBytes);

extern unsigned int spib_prepare_dctrl2(unsigned int cmden,
	    unsigned int addren,
	    unsigned int tm,
	    unsigned int wcnt,
	    unsigned int dycnt,
	    unsigned int rcnt,
		unsigned int addrfmt,
		unsigned int datafmt,
		unsigned int tokenen);
extern void spib_exe_cmmd2(unsigned long base, unsigned int op, unsigned int addr, unsigned int spib_dctrl);

#endif

