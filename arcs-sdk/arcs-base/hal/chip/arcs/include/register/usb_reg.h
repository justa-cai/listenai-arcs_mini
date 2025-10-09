/*
 * usb.h
 *
 *  Created on: Sep. 7, 2020
 *  Ported on: Oct. 6, 2023
 *
 */

#ifndef __USB_CSK_H
#define __USB_CSK_H

// global USB Hardware configuration
#define USB_EP_NO_MAX_AVAIL                 7 // max. available EP No.
#define USB_EP_NO_DBG                       8 // EP No. for debug only
#define USB_DMA_CH_COUNT_AVAIL              6 // excluding 2 DMA channel for debug
#define USB_DMA_CH_COUNT_TOTAL              8 // including 2 DMA channel for debug

#define USB_ARCS_IN_EP_NUM                 (USB_EP_NO_MAX_AVAIL + 1) // available IN EP count
#define USB_ARCS_OUT_EP_NUM                (USB_EP_NO_MAX_AVAIL + 1) // available OUT EP count

/*****************************************************************************
 * USB Registers Map
 ****************************************************************************/
typedef struct{
	unsigned int CNTL;
	unsigned int ADDR;
	unsigned int COUNT;
	unsigned int RESERVED0;
}CSK_USB_DMA_RegDef;


typedef struct {
    volatile  unsigned char FADDR;              /* 0x00 7-bit address of the peripheral part of the transaction register */
    volatile  unsigned char POWER;              /* 0x01 control Suspend and Resume signaling */
    volatile const   unsigned short INTRTX;            /* 0x02 transmit interrupt status for Endpoint0-15 register, read clear */
    volatile const   unsigned short INTRRX;            /* 0x04 receive interrupt status for Endpoint1-15 register, read clear */
    volatile  unsigned short INTRTXE;			/* 0x06 transmit interrupt enable for Endpoint0-15 register */
    volatile  unsigned short INTRRXE;			/* 0x08 receive interrupt enable for Endpoint1-15 register */
    volatile const   unsigned char INTRUSB;			/* 0x0A active USB interrupt status register */
    volatile  unsigned char INTRUSBE;			/* 0x0B active USB interrupt enable register */
    volatile const   unsigned short FRAME;				/* 0x0C the last received frame number register */
    volatile  unsigned char INDEX;				/* 0x0E TX endpoint and RX endpoint index register */
    volatile  unsigned char TESTMODE;			/* 0x0F test mode setting register */

    //indexed register start
    volatile  unsigned short TXMAXP;			/* 0x10 Maximum packet size for peripheral TX endpoint 1-15 */
    	  union{
    		    volatile struct {				/* 0x12 According to INDEX register, Control status register for endpoint 0 */
    		    	unsigned char CSR0L;
    		    	unsigned char CSR0H;
    		    };
    		    volatile struct {				/* 0x12 According to INDEX register, Control status register for Tx status endpoint 1-15 */
    		    	unsigned char TXCSRL;
    		    	unsigned char TXCSRH;
    		    };
    	  };
    volatile  unsigned short RXMAXP;			/* 0x14 Maximum packet size for peripheral RX endpoint 1-15 */
    volatile  unsigned char RXCSRL;				/* 0x16 According to INDEX register, Control status register for Rx status endpoint X */
    volatile  unsigned char RXCSRH;				/* 0x17 According to INDEX register, Control status register for Rx status endpoint X */
	  	  union{
    			volatile const unsigned short	COUNT0;	/* 0x18 According to INDEX register, number of received bytes for endpoint 0 */
    			volatile const unsigned short	RXCOUNT;/* 0x18 According to INDEX register, number of received bytes for Rx count of endpoint 1-15 */
	  	  };
    volatile  unsigned char TXTYPE;				/* 0x1A host mode only */
    volatile  unsigned char TXINTERVAL;			/* 0x1B host mode only */
    volatile  unsigned char RXTYPE;				/* 0x1C host mode only */
    volatile  unsigned char RXINTERVAL;			/* 0x1D host mode only */
    volatile  unsigned char RESERVED0;			/* 0x1E */
	  	  union{
			volatile const unsigned char	CONFIGDATA;	/* 0x1F return details for core configuration */
			volatile const unsigned char	FIFOSIZE;	/* 0x1F return the configured size of the selected Rx FIFOs and Tx FIFOs, endpoint 1-15 */
	  	  };
	//indexed register end

	volatile  unsigned int FIFOX[16];		/* FIFOs for Endpoints 0-15 */

    volatile  unsigned char DEVCTL;			/* 0x60  */
    volatile const   unsigned char MISC;			/* 0x61  */
    volatile  unsigned char TXFIFOSZ;		/* 0x62  */
    volatile  unsigned char RXFIFOSZ;		/* 0x63  */
    volatile  unsigned short TXFIFOADD;		/* 0x64  */
    volatile  unsigned short RXFIFOADD;		/* 0x66  */
    volatile  unsigned int VCONTROL;		/* 0x68  */
    volatile const   unsigned short HWVERS;		/* 0x6C  */
    volatile const   unsigned short RESERVED1;		/* 0x70  */
    volatile const   unsigned char RESERVED2[7];	/* 0x78  */
    volatile  unsigned char SOFT_RST;		/* 0x7F  */
    volatile  unsigned int RESERVED3[98];
    volatile  unsigned int DMA_INTR;		/* 0x200 */
    //volatile  CSK_USB_DMA_RegDef USB_DMA[6];/* 0x204+n*0x10, total 6 dma channel*/
    volatile  CSK_USB_DMA_RegDef USB_DMA[8];/* 0x204+n*0x10, total 8 dma channel*/

    //volatile  unsigned int RESERVED4[47];
    //volatile  unsigned short RXDPKTBUF_DIS; /* 0x340  */
    //volatile  unsigned short TXDPKTBUF_DIS; /* 0x342  */
} CSK_USB_RegDef;



//#define CSK_USBC             ((CSK_USB_RegDef *)  USBC_BASE)

#define USB_ARCS_POWER_EN_SUSPENDM_POS     (0)
#define USB_ARCS_POWER_EN_SUSPENDM_MASK    (0x1 << USB_ARCS_POWER_EN_SUSPENDM_POS)
#define USB_ARCS_POWER_EN_SUSPENDM         (USB_ARCS_POWER_EN_SUSPENDM_MASK) //[RW]:
#define USB_ARCS_POWER_SUSPEND_MODE_POS    (1)
#define USB_ARCS_POWER_SUSPEND_MODE_MASK   (0x1 << USB_ARCS_POWER_SUSPEND_MODE_POS)
#define USB_ARCS_POWER_SUSPEND_MODE        (USB_ARCS_POWER_SUSPEND_MODE_MASK) //[RO]:
#define USB_ARCS_POWER_RESUME_POS          (2)
#define USB_ARCS_POWER_RESUME_MASK         (0x1 << USB_ARCS_POWER_RESUME_POS)
#define USB_ARCS_POWER_RESUME              (USB_ARCS_POWER_RESUME_MASK) //[RW]:

#define USB_ARCS_POWER_HSENABLE_POS		(5)
#define USB_ARCS_POWER_HSENABLE_MASK		(0x1 << USB_ARCS_POWER_HSENABLE_POS)
#define USB_ARCS_POWER_HSENABLE			(USB_ARCS_POWER_HSENABLE_MASK)
#define USB_ARCS_POWER_SOFTCONN_POS		(6)
#define USB_ARCS_POWER_SOFTCONN_MASK		(0x1 << USB_ARCS_POWER_SOFTCONN_POS)
#define USB_ARCS_POWER_SOFTCONN			(USB_ARCS_POWER_SOFTCONN_MASK)

#define USB_ARCS_INTRUSBE_SUSPEND_POS		(0)
#define USB_ARCS_INTRUSBE_SUSPEND_MASK		(0x1 << USB_ARCS_INTRUSBE_SUSPEND_POS)
#define USB_ARCS_INTRUSBE_SUSPEND			(USB_ARCS_INTRUSBE_SUSPEND_MASK)
#define USB_ARCS_INTRUSBE_RESUME_POS		(1)
#define USB_ARCS_INTRUSBE_RESUME_MASK		(0x1 << USB_ARCS_INTRUSBE_RESUME_POS)
#define USB_ARCS_INTRUSBE_RESUME			(USB_ARCS_INTRUSBE_RESUME_MASK)
#define USB_ARCS_INTRUSBE_RESET_POS		(2)
#define USB_ARCS_INTRUSBE_RESET_MASK		(0x1 << USB_ARCS_INTRUSBE_RESET_POS)
#define USB_ARCS_INTRUSBE_RESET			(USB_ARCS_INTRUSBE_RESET_MASK)
#define USB_ARCS_INTRUSBE_SOF_POS			(3)
#define USB_ARCS_INTRUSBE_SOF_MASK			(0x1 << USB_ARCS_INTRUSBE_SOF_POS)
#define USB_ARCS_INTRUSBE_SOF				(USB_ARCS_INTRUSBE_SOF_MASK)
#define USB_ARCS_INTRUSBE_CONN_POS			(4)
#define USB_ARCS_INTRUSBE_CONN_MASK		(0x1 << USB_ARCS_INTRUSBE_CONN_POS)
#define USB_ARCS_INTRUSBE_CONN				(USB_ARCS_INTRUSBE_CONN_MASK)
#define USB_ARCS_INTRUSBE_DISCON_POS		(5)
#define USB_ARCS_INTRUSBE_DISCON_MASK		(0x1 << USB_ARCS_INTRUSBE_DISCON_POS)
#define USB_ARCS_INTRUSBE_DISCON			(USB_ARCS_INTRUSBE_DISCON_MASK)
#define USB_ARCS_INTRUSBE_SESSREQ_POS		(6)
#define USB_ARCS_INTRUSBE_SESSREQ_MASK		(0x1 << USB_ARCS_INTRUSBE_SESSREQ_POS)
#define USB_ARCS_INTRUSBE_SESSREQ			(USB_ARCS_INTRUSBE_SESSREQ_MASK)
#define USB_ARCS_INTRUSBE_VBUSERROR_POS	(7)
#define USB_ARCS_INTRUSBE_VBUSERROR_MASK	(0x1 << USB_ARCS_INTRUSBE_VBUSERROR_POS)
#define USB_ARCS_INTRUSBE_VBUSERROR		(USB_ARCS_INTRUSBE_VBUSERROR_MASK)

#define USB_ARCS_INTRUSB_SUSPEND_POS		(0)
#define USB_ARCS_INTRUSB_SUSPEND_MASK		(0x1 << USB_ARCS_INTRUSB_SUSPEND_POS)
#define USB_ARCS_INTRUSB_SUSPEND			(USB_ARCS_INTRUSB_SUSPEND_MASK)
#define USB_ARCS_INTRUSB_RESUME_POS		(1)
#define USB_ARCS_INTRUSB_RESUME_MASK		(0x1 << USB_ARCS_INTRUSB_RESUME_POS)
#define USB_ARCS_INTRUSB_RESUME			(USB_ARCS_INTRUSB_RESUME_MASK)
#define USB_ARCS_INTRUSB_RESET_POS			(2)
#define USB_ARCS_INTRUSB_RESET_MASK		(0x1 << USB_ARCS_INTRUSB_RESET_POS)
#define USB_ARCS_INTRUSB_RESET				(USB_ARCS_INTRUSB_RESET_MASK)
#define USB_ARCS_INTRUSB_SOF_POS			(3)
#define USB_ARCS_INTRUSB_SOF_MASK			(0x1 << USB_ARCS_INTRUSB_SOF_POS)
#define USB_ARCS_INTRUSB_SOF				(USB_ARCS_INTRUSB_SOF_MASK)
#define USB_ARCS_INTRUSB_CONN_POS          (4) // Only valid in Host mode
#define USB_ARCS_INTRUSB_CONN_MASK        (0x1 << USB_ARCS_INTRUSB_CONN_POS)
#define USB_ARCS_INTRUSB_CONN              (USB_ARCS_INTRUSB_CONN_MASK)
#define USB_ARCS_INTRUSB_DISCON_POS        (5)
#define USB_ARCS_INTRUSB_DISCON_MASK       (0x1 << USB_ARCS_INTRUSB_DISCON_POS)
#define USB_ARCS_INTRUSB_DISCON            (USB_ARCS_INTRUSB_DISCON_MASK)

#define USB_ARCS_CSR0L_RXPKTRDY_POS		(0)
#define USB_ARCS_CSR0L_RXPKTRDY_MASK		(0x1 << USB_ARCS_CSR0L_RXPKTRDY_POS)
#define USB_ARCS_CSR0L_RXPKTRDY			(USB_ARCS_CSR0L_RXPKTRDY_MASK)
#define USB_ARCS_CSR0L_TXPKTRDY_POS		(1)
#define USB_ARCS_CSR0L_TXPKTRDY_MASK		(0x1 << USB_ARCS_CSR0L_TXPKTRDY_POS)
#define USB_ARCS_CSR0L_TXPKTRDY			(USB_ARCS_CSR0L_TXPKTRDY_MASK)
#define USB_ARCS_CSR0L_SENTSTALL_POS		(2)
#define USB_ARCS_CSR0L_SENTSTALL_MASK		(0x1 << USB_ARCS_CSR0L_SENTSTALL_POS)
#define USB_ARCS_CSR0L_SENTSTALL			(USB_ARCS_CSR0L_SENTSTALL_MASK)
#define USB_ARCS_CSR0L_DATAEND_POS			(3)
#define USB_ARCS_CSR0L_DATAEND_MASK		(0x1 << USB_ARCS_CSR0L_DATAEND_POS)
#define USB_ARCS_CSR0L_DATAEND				(USB_ARCS_CSR0L_DATAEND_MASK)
#define USB_ARCS_CSR0L_SETUPEND_POS		(4)
#define USB_ARCS_CSR0L_SETUPEND_MASK		(0x1 << USB_ARCS_CSR0L_SETUPEND_POS)
#define USB_ARCS_CSR0L_SETUPEND			(USB_ARCS_CSR0L_SETUPEND_MASK)
#define USB_ARCS_CSR0L_SENDSTALL_POS		(5)
#define USB_ARCS_CSR0L_SENDSTALL_MASK		(0x1 << USB_ARCS_CSR0L_SENDSTALL_POS)
#define USB_ARCS_CSR0L_SENDSTALL			(USB_ARCS_CSR0L_SENDSTALL_MASK)
#define USB_ARCS_CSR0L_SERVICEDRXPKTRDY_POS		(6)
#define USB_ARCS_CSR0L_SERVICEDRXPKTRDY_MASK		(0x1 << USB_ARCS_CSR0L_SERVICEDRXPKTRDY_POS)
#define USB_ARCS_CSR0L_SERVICEDRXPKTRDY			(USB_ARCS_CSR0L_SERVICEDRXPKTRDY_MASK)
#define USB_ARCS_CSR0L_SERVICEDSETUPEND_POS		(7)
#define USB_ARCS_CSR0L_SERVICEDSETUPEND_MASK		(0x1 << USB_ARCS_CSR0L_SERVICEDSETUPEND_POS)
#define USB_ARCS_CSR0L_SERVICEDSETUPEND			(USB_ARCS_CSR0L_SERVICEDSETUPEND_MASK)

#define USB_ARCS_CSR0H_FLUSHFIFO_POS		(0)
#define USB_ARCS_CSR0H_FLUSHFIFO_MASK		(0x1 << USB_ARCS_CSR0H_FLUSHFIFO_POS)
#define USB_ARCS_CSR0H_FLUSHFIFO			(USB_ARCS_CSR0H_FLUSHFIFO_MASK)

#define USB_ARCS_TXCSRL_TXPKTRDY_POS		(0)
#define USB_ARCS_TXCSRL_TXPKTRDY_MASK		(0x1 << USB_ARCS_TXCSRL_TXPKTRDY_POS)
#define USB_ARCS_TXCSRL_TXPKTRDY			(USB_ARCS_TXCSRL_TXPKTRDY_MASK)
#define USB_ARCS_TXCSRL_FIFONOTEMPTY_POS	(1)
#define USB_ARCS_TXCSRL_FIFONOTEMPTY_MASK	(0x1 << USB_ARCS_TXCSRL_FIFONOTEMPTY_POS)
#define USB_ARCS_TXCSRL_FIFONOTEMPTY		(USB_ARCS_TXCSRL_FIFONOTEMPTY_MASK)
#define USB_ARCS_TXCSRL_UNDERRUN_POS       (2)
#define USB_ARCS_TXCSRL_UNDERRUN_MASK      (0x1 << USB_ARCS_TXCSRL_UNDERRUN_POS)
#define USB_ARCS_TXCSRL_UNDERRUN           (USB_ARCS_TXCSRL_UNDERRUN_MASK)
#define USB_ARCS_TXCSRL_FLUSHFIFO_POS		(3)
#define USB_ARCS_TXCSRL_FLUSHFIFO_MASK		(0x1 << USB_ARCS_TXCSRL_FLUSHFIFO_POS)
#define USB_ARCS_TXCSRL_FLUSHFIFO			(USB_ARCS_TXCSRL_FLUSHFIFO_MASK)
#define USB_ARCS_TXCSRL_SENDSTALL_POS		(4)
#define USB_ARCS_TXCSRL_SENDSTALL_MASK		(0x1 << USB_ARCS_TXCSRL_SENDSTALL_POS)
#define USB_ARCS_TXCSRL_SENDSTALL			(USB_ARCS_TXCSRL_SENDSTALL_MASK)
#define USB_ARCS_TXCSRL_SENTSTALL_POS		(5)
#define USB_ARCS_TXCSRL_SENTSTALL_MASK		(0x1 << USB_ARCS_TXCSRL_SENTSTALL_POS)
#define USB_ARCS_TXCSRL_SENTSTALL			(USB_ARCS_TXCSRL_SENTSTALL_MASK)
#define USB_ARCS_TXCSRL_CLRDATATOG_POS     (6)
#define USB_ARCS_TXCSRL_CLRDATATOG_MASK    (0x1 << USB_ARCS_TXCSRL_CLRDATATOG_POS)
#define USB_ARCS_TXCSRL_CLRDATATOG         (USB_ARCS_TXCSRL_CLRDATATOG_MASK)

#define USB_ARCS_RXCSRL_RXPKTRDY_POS		(0)
#define USB_ARCS_RXCSRL_RXPKTRDY_MASK		(0x1 << USB_ARCS_RXCSRL_RXPKTRDY_POS)
#define USB_ARCS_RXCSRL_RXPKTRDY			(USB_ARCS_RXCSRL_RXPKTRDY_MASK)
#define USB_ARCS_RXCSRL_OVERRUN_POS        (2)
#define USB_ARCS_RXCSRL_OVERRUN_MASK      (0x1 << USB_ARCS_RXCSRL_OVERRUN_POS)
#define USB_ARCS_RXCSRL_OVERRUN            (USB_ARCS_RXCSRL_OVERRUN_MASK)
#define USB_ARCS_RXCSRL_FLUSHFIFO_POS		(4)
#define USB_ARCS_RXCSRL_FLUSHFIFO_MASK		(0x1 << USB_ARCS_RXCSRL_FLUSHFIFO_POS)
#define USB_ARCS_RXCSRL_FLUSHFIFO			(USB_ARCS_RXCSRL_FLUSHFIFO_MASK)
#define USB_ARCS_RXCSRL_SENDSTALL_POS		(5)
#define USB_ARCS_RXCSRL_SENDSTALL_MASK		(0x1 << USB_ARCS_RXCSRL_SENDSTALL_POS)
#define USB_ARCS_RXCSRL_SENDSTALL			(USB_ARCS_RXCSRL_SENDSTALL_MASK)
#define USB_ARCS_RXCSRL_SENTSTALL_POS		(6)
#define USB_ARCS_RXCSRL_SENTSTALL_MASK		(0x1 << USB_ARCS_RXCSRL_SENTSTALL_POS)
#define USB_ARCS_RXCSRL_SENTSTALL			(USB_ARCS_RXCSRL_SENTSTALL_MASK)
#define USB_ARCS_RXCSRL_CLRDATATOG_POS		(7)
#define USB_ARCS_RXCSRL_CLRDATATOG_MASK	(0x1 << USB_ARCS_RXCSRL_CLRDATATOG_POS)
#define USB_ARCS_RXCSRL_CLRDATATOG			(USB_ARCS_RXCSRL_CLRDATATOG_MASK)

#define USB_ARCS_FADDR_ADDR_MASK 			(0x7F)

#define USB_ARCS_COUNT0_RXCOUNT_POS		(0)
#define USB_ARCS_COUNT0_RXCOUNT_MASK		(0x7F)
#define USB_ARCS_COUNT0_MASK				(0x7F)	// 7bits
#define USB_ARCS_RXCOUNT_MASK				(0x3FF)	// 14bits

#define USB_ARCS_TXCSRH_DMAREQMODE_POS  	(2)
#define USB_ARCS_TXCSRH_DMAREQMODE_MASK 	(1<<USB_ARCS_TXCSRH_DMAREQMODE_POS)
#define USB_ARCS_TXCSRH_DMAREQMODE			USB_ARCS_TXCSRH_DMAREQMODE_MASK
#define USB_ARCS_TXCSRH_DMAREQMODE_0		0
#define USB_ARCS_TXCSRH_DMAREQMODE_1		USB_ARCS_TXCSRH_DMAREQMODE_MASK
#define USB_ARCS_TXCSRH_FRCDATATOG_POS  	(3)
#define USB_ARCS_TXCSRH_FRCDATATOG_MASK 	(1<<USB_ARCS_TXCSRH_FRCDATATOG_POS)
#define USB_ARCS_TXCSRH_FRCDATATOG			USB_ARCS_TXCSRH_FRCDATATOG_MASK
#define USB_ARCS_TXCSRH_DMAREQENAB_POS  	(4)
#define USB_ARCS_TXCSRH_DMAREQENAB_MASK 	(1<<USB_ARCS_TXCSRH_DMAREQENAB_POS)
#define USB_ARCS_TXCSRH_DMAREQENAB			USB_ARCS_TXCSRH_DMAREQENAB_MASK
#define USB_ARCS_TXCSRH_MODE_POS  			(5)
#define USB_ARCS_TXCSRH_MODE_MASK 			(1<<USB_ARCS_TXCSRH_MODE_POS)
#define USB_ARCS_TXCSRH_MODE				USB_ARCS_TXCSRH_MODE_MASK
#define USB_ARCS_TXCSRH_ISO_POS  			(6)
#define USB_ARCS_TXCSRH_ISO_MASK 			(1<<USB_ARCS_TXCSRH_ISO_POS)
#define USB_ARCS_TXCSRH_ISO				(USB_ARCS_TXCSRH_ISO_MASK)
#define USB_ARCS_TXCSRH_AUTOSET_POS  		(7)
#define USB_ARCS_TXCSRH_AUTOSET_MASK 		(1<<USB_ARCS_TXCSRH_AUTOSET_POS)
#define USB_ARCS_TXCSRH_AUTOSET			(USB_ARCS_TXCSRH_AUTOSET_MASK)

#define USB_ARCS_RXCSRH_DMAREQMODE_POS  	(3)
#define USB_ARCS_RXCSRH_DMAREQMODE_MASK 	(1<<USB_ARCS_RXCSRH_DMAREQMODE_POS)
#define USB_ARCS_RXCSRH_DMAREQMODE_0		0
#define USB_ARCS_RXCSRH_DMAREQMODE_1		USB_ARCS_RXCSRH_DMAREQMODE_MASK
#define USB_ARCS_RXCSRH_DISNYET_POS        (4)
#define USB_ARCS_RXCSRH_DISNYET_MASK       (1<<USB_ARCS_RXCSRH_DISNYET_POS)
#define USB_ARCS_RXCSRH_DISNYET            USB_ARCS_RXCSRH_DISNYET_MASK
#define USB_ARCS_RXCSRH_PIDERR_POS         (4)
#define USB_ARCS_RXCSRH_PIDERR_MASK        (1<<USB_ARCS_RXCSRH_PIDERR_POS)
#define USB_ARCS_RXCSRH_PIDERR             USB_ARCS_RXCSRH_PIDERR_MASK
#define USB_ARCS_RXCSRH_DMAREQENAB_POS  	(5)
#define USB_ARCS_RXCSRH_DMAREQENAB_MASK 	(1<<USB_ARCS_RXCSRH_DMAREQENAB_POS)
#define USB_ARCS_RXCSRH_DMAREQENAB			USB_ARCS_RXCSRH_DMAREQENAB_MASK
#define USB_ARCS_RXCSRH_ISO_POS  			(6)
#define USB_ARCS_RXCSRH_ISO_MASK 			(1<<USB_ARCS_RXCSRH_ISO_POS)
#define USB_ARCS_RXCSRH_ISO				USB_ARCS_RXCSRH_ISO_MASK
#define USB_ARCS_RXCSRH_AUTOCLEAR_POS  	(7)
#define USB_ARCS_RXCSRH_AUTOCLEAR_MASK 	(1<<USB_ARCS_RXCSRH_AUTOCLEAR_POS)
#define USB_ARCS_RXCSRH_AUTOCLEAR			USB_ARCS_RXCSRH_AUTOCLEAR_MASK

#define USB_ARCS_DMA_CNTL_DMA_ENAB_POS  	(0)
#define USB_ARCS_DMA_CNTL_DMA_ENAB_MASK 	(1<<USB_ARCS_DMA_CNTL_DMA_ENAB_POS)
#define USB_ARCS_DMA_CNTL_DMA_ENAB			USB_ARCS_DMA_CNTL_DMA_ENAB_MASK
#define USB_ARCS_DMA_CNTL_DMA_DIR_POS  	(1)
#define USB_ARCS_DMA_CNTL_DMA_DIR_MASK 	(1<<USB_ARCS_DMA_CNTL_DMA_DIR_POS)
#define USB_ARCS_DMA_CNTL_DMA_DIR(n)		(((n) & 0x1) << USB_ARCS_DMA_CNTL_DMA_DIR_POS)
#define USB_ARCS_DMA_CNTL_DMA_DIR_RXEP		(0<<USB_ARCS_DMA_CNTL_DMA_DIR_POS)
#define USB_ARCS_DMA_CNTL_DMA_DIR_TXEP		(1<<USB_ARCS_DMA_CNTL_DMA_DIR_POS)
#define USB_ARCS_DMA_CNTL_DMAMODE_POS  	(2)
#define USB_ARCS_DMA_CNTL_DMAMODE_MASK 	(1<<USB_ARCS_DMA_CNTL_DMAMODE_POS)
#define USB_ARCS_DMA_CNTL_DMAMODE			USB_ARCS_DMA_CNTL_DMAMODE_MASK
#define USB_ARCS_DMA_CNTL_DMAMODE_0		(0<<USB_ARCS_DMA_CNTL_DMAMODE_POS)
#define USB_ARCS_DMA_CNTL_DMAMODE_1		(1<<USB_ARCS_DMA_CNTL_DMAMODE_POS)
#define USB_ARCS_DMA_CNTL_DMAIE_POS  		(3)
#define USB_ARCS_DMA_CNTL_DMAIE_MASK 		(1<<USB_ARCS_DMA_CNTL_DMAIE_POS)
#define USB_ARCS_DMA_CNTL_DMAIE			USB_ARCS_DMA_CNTL_DMAIE_MASK
#define USB_ARCS_DMA_CNTL_DMAEP_POS  		(4)
#define USB_ARCS_DMA_CNTL_DMAEP_MASK 		(0xF<<USB_ARCS_DMA_CNTL_DMAEP_POS)
#define USB_ARCS_DMA_CNTL_DMAEP(n)			(((n) & 0xF) << USB_ARCS_DMA_CNTL_DMAEP_POS)
#define USB_ARCS_DMA_CNTL_DMAERR_POS  		(8)
#define USB_ARCS_DMA_CNTL_DMAERR_MASK 		(1<<USB_ARCS_DMA_CNTL_DMAERR_POS)
#define USB_ARCS_DMA_CNTL_DMAERR			USB_ARCS_DMA_CNTL_DMAERR_MASK
#define USB_ARCS_DMA_CNTL_DMA_BRSTM_POS  	(9)
#define USB_ARCS_DMA_CNTL_DMA_BRSTM_MASK 	(0x3<<USB_ARCS_DMA_CNTL_DMA_BRSTM_POS)
#define USB_ARCS_DMA_CNTL_DMA_BRSTM		USB_ARCS_DMA_CNTL_DMA_BRSTM_MASK
#define USB_ARCS_DMA_CNTL_DMA_BRSTM_0		(0<<USB_ARCS_DMA_CNTL_DMA_BRSTM_POS)
#define USB_ARCS_DMA_CNTL_DMA_BRSTM_1		(1<<USB_ARCS_DMA_CNTL_DMA_BRSTM_POS)
#define USB_ARCS_DMA_CNTL_DMA_BRSTM_2		(2<<USB_ARCS_DMA_CNTL_DMA_BRSTM_POS)
#define USB_ARCS_DMA_CNTL_DMA_BRSTM_3		(3<<USB_ARCS_DMA_CNTL_DMA_BRSTM_POS)

#define USB_ARCS_FIFOSZ_DPB_MASK			(0x10)
#define USB_ARCS_FIFOSZ_DPB				(USB_ARCS_FIFOSZ_DPB_MASK)
#define USB_ARCS_FIFOSZ_SZ_MASK			(0x0F)
#define USB_ARCS_FIFOSZ_SZ_8				(0x00)
#define USB_ARCS_FIFOSZ_SZ_16				(0x01)
#define USB_ARCS_FIFOSZ_SZ_32				(0x02)
#define USB_ARCS_FIFOSZ_SZ_64				(0x03)
#define USB_ARCS_FIFOSZ_SZ_128				(0x04)
#define USB_ARCS_FIFOSZ_SZ_256				(0x05)
#define USB_ARCS_FIFOSZ_SZ_512				(0x06)
#define USB_ARCS_FIFOSZ_SZ_1024			(0x07)
#define USB_ARCS_FIFOSZ_SZ_2048			(0x08)
#define USB_ARCS_FIFOSZ_SZ_4096			(0x09)

#define USB_ARCS_MAXP_MASK 				(0x7FF)

#define USB_ARCS_INTRTX_EP0_POS			(0x0)
#define USB_ARCS_INTRTX_EP0_MASK			(0x01<<USB_ARCS_INTRTX_EP0_POS)
#define USB_ARCS_INTRTX_EP0				(USB_ARCS_INTRTX_EP0_MASK)
#define USB_ARCS_INTRTX_EP_POS			    (0x1) // excluding EP0
#define USB_ARCS_INTRTX_EP_MASK			(0x1F<<USB_ARCS_INTRTX_EP_POS)
#define USB_ARCS_INTRTX_EP				    (USB_ARCS_INTRTX_EP_MASK)

#define USB_ARCS_INTRRX_BIT0_POS			(0x0)
#define USB_ARCS_INTRRX_BIT0_MASK			(0x01<<USB_ARCS_INTRRX_BIT0_POS)
#define USB_ARCS_INTRRX_BIT0				(USB_ARCS_INTRRX_BIT0_MASK)
#define USB_ARCS_INTRRX_EP_POS			    (0x1) // excluding EP0
#define USB_ARCS_INTRRX_EP_MASK			(0x1F<<USB_ARCS_INTRRX_EP_POS)
#define USB_ARCS_INTRRX_EP				    (USB_ARCS_INTRRX_EP_MASK)

#define USB_ARCS_DMA_INTR_INTR0_POS		(0)
#define USB_ARCS_DMA_INTR_INTR0_MASK		(0x1 << USB_ARCS_DMA_INTR_INTR0_POS)
#define USB_ARCS_DMA_INTR_INTR0			(USB_ARCS_DMA_INTR_INTR0_MASK)
/*
#define USB_ARCS_DMA_INTR_INTR1_POS		(1)
#define USB_ARCS_DMA_INTR_INTR1_MASK		(0x1 << USB_ARCS_DMA_INTR_INTR1_POS)
#define USB_ARCS_DMA_INTR_INTR1			(USB_ARCS_DMA_INTR_INTR1_MASK)
#define USB_ARCS_DMA_INTR_INTR2_POS		(2)
#define USB_ARCS_DMA_INTR_INTR2_MASK		(0x1 << USB_ARCS_DMA_INTR_INTR2_POS)
#define USB_ARCS_DMA_INTR_INTR2			(USB_ARCS_DMA_INTR_INTR2_MASK)
#define USB_ARCS_DMA_INTR_INTR3_POS		(3)
#define USB_ARCS_DMA_INTR_INTR3_MASK		(0x1 << USB_ARCS_DMA_INTR_INTR3_POS)
#define USB_ARCS_DMA_INTR_INTR3			(USB_ARCS_DMA_INTR_INTR3_MASK)
#define USB_ARCS_DMA_INTR_INTR4_POS		(4)
#define USB_ARCS_DMA_INTR_INTR4_MASK		(0x1 << USB_ARCS_DMA_INTR_INTR4_POS)
#define USB_ARCS_DMA_INTR_INTR4			(USB_ARCS_DMA_INTR_INTR4_MASK)
#define USB_ARCS_DMA_INTR_INTR5_POS		(5)
#define USB_ARCS_DMA_INTR_INTR5_MASK		(0x1 << USB_ARCS_DMA_INTR_INTR5_POS)
#define USB_ARCS_DMA_INTR_INTR5			(USB_ARCS_DMA_INTR_INTR5_MASK)
#define USB_ARCS_DMA_INTR_INT0_5_MASK		(0x3F)
*/
#define USB_DMA_INTR_EP_POS(n)        (n)
#define USB_DMA_INTR_EP_MASK(n)       (0x1 << USB_DMA_INTR_EP_POS(n))
#define USB_DMA_INTR_EP(n)            (USB_DMA_INTR_EP_MASK(n))
#define USB_DMA_INTR_EP0_n_MASK(n)    ((0x1 << ((n)+1)) - 1)
#define USB_DMA_INTR_EP_ALL_MASK      USB_DMA_INTR_EP0_n_MASK(USB_EP_NO_MAX_AVAIL) // including EP0

#define USB_ARCS_SOFT_RST_NRST_POS			(0)
#define USB_ARCS_SOFT_RST_NRST_MASK		(0x1 << USB_ARCS_SOFT_RST_NRST_POS)
#define USB_ARCS_SOFT_RST_NRST				(USB_ARCS_SOFT_RST_NRST_MASK)
#define USB_ARCS_SOFT_RST_NRSTX_POS		(1)
#define USB_ARCS_SOFT_RST_NRSTX_MASK		(0x1 << USB_ARCS_SOFT_RST_NRSTX_POS)
#define USB_ARCS_SOFT_RST_NRSTX			(USB_ARCS_SOFT_RST_NRSTX_MASK)

//From the viewpoint of host, OUT: host->device, IN: device->host
#define DIR_IDX_OUT                 0
#define DIR_IDX_IN                  1

/*
// USB IN EP index
enum usb_arcs_in_ep_idx {
	USB_ARCS_IN_EP_0 = 0,
	USB_ARCS_IN_EP_1,
	USB_ARCS_IN_EP_2,
	USB_ARCS_IN_EP_3,
	USB_ARCS_IN_EP_4,
	USB_ARCS_IN_EP_5,
	USB_ARCS_IN_EP_NUM
};

// USB OUT EP index
enum usb_arcs_out_ep_idx {
	USB_ARCS_OUT_EP_0 = 0,
	USB_ARCS_OUT_EP_1,
	USB_ARCS_OUT_EP_2,
	USB_ARCS_OUT_EP_3,
	USB_ARCS_OUT_EP_4,
	USB_ARCS_OUT_EP_5,
	USB_ARCS_OUT_EP_NUM
};
*/



#define EDPxReg_SEL(num)				(CSK_USBC->INDEX = (num&0x7F))
#define USB_ARCS_DAINT_IN_EP_INT(ep) 	(1 << (ep))
#define USB_ARCS_DAINT_OUT_EP_INT(ep) 	(1 << (ep))

#define USB_ARCS_STS_SETUP				(1)
#define USB_ARCS_STS_IN				(2)
#define USB_ARCS_STS_OUT				(3)
#define USB_ARCS_STS_STATUS			(4)

#define USB_ARCS_DMA_DIR_RX_ENDPOINT	(0)
#define USB_ARCS_DMA_DIR_TX_ENDPOINT	(1)

//#define USB_ARCS_DMA_NUMBER_OF_CHANNELS (5)

#define USB_ARCS_DMA_CHANNEL_NOT_ASSIGNED (0xFF)

#define DMA_MULTIPLE (1)

#define GET_FIFOSZ_CFG(fifosize)    (((fifosize) <= 8) ? (USB_ARCS_FIFOSZ_SZ_8) :\
                                    (((fifosize) <= 16) ? (USB_ARCS_FIFOSZ_SZ_16) :\
                                    (((fifosize) <= 32) ? (USB_ARCS_FIFOSZ_SZ_32) :\
                                    (((fifosize) <= 64) ? (USB_ARCS_FIFOSZ_SZ_64) :\
                                    (((fifosize) <= 128) ? (USB_ARCS_FIFOSZ_SZ_128) :\
                                    (((fifosize) <= 256) ? (USB_ARCS_FIFOSZ_SZ_256) :\
                                    (((fifosize) <= 512) ? (USB_ARCS_FIFOSZ_SZ_512) :\
                                    (((fifosize) <= 1024) ? (USB_ARCS_FIFOSZ_SZ_1024) :(USB_ARCS_FIFOSZ_SZ_256)))))))))

#define FIFOSZ_CFG_TO_BYTES(sz_cfg)     (1 << (((sz_cfg) & 0xF) + 3))

#endif /* __USB_CSK_H */
