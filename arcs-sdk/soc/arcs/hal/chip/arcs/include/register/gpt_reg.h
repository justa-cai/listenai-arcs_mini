/*
 * Copyright (c) 2012-2020 ListenAI Corporation
 * All rights reserved.
 *
 */

#ifndef __GPT_REG_H
#define __GPT_REG_H
 
#define GPT_NUMBER_OF_CHANNELS	8

/*---------------------------- register definition ---------------------------------*/
typedef struct {
    volatile    const		unsigned int IDREV;                		            /* 0x00 ID and Revision Register */
    volatile 	unsigned int CHx_CLK_CTRL[GPT_NUMBER_OF_CHANNELS];      		/* 0x04-0x18 channelx clock control */
    volatile 	unsigned int CHx_CTRL[GPT_NUMBER_OF_CHANNELS];      			/* 0x1C-0x30 channelx mode control */
    volatile 	unsigned int CHx_RELOAD[GPT_NUMBER_OF_CHANNELS];      		    /* 0x34-0x48 channelx reload value */
	volatile 	unsigned int CNT_MODE_CTRL_1;		                            /* 0x4C channel5-channel4 counter direction control */
	volatile 	unsigned int CNT_MODE_CTRL_0;		                            /* 0x50 channel3-channel0 counter direction control */
	volatile 	unsigned int RUN_MODE_CTRL_1;		                            /* 0x54 channel5-channel4 run mode control */
	volatile 	unsigned int RUN_MODE_CTRL_0;		                            /* 0x58 channel3-channel0 run mode control */
	volatile    const 	unsigned int CHx_CNT[GPT_NUMBER_OF_CHANNELS];		  	/* 0x5C-0x70 current counter value for channelx */
	volatile 	unsigned int CH_CNT_EN;				                            /* 0x74 counter control for channelx */
	volatile    const 	unsigned int CH_NUM;				                    /* 0x78 number of channels */
	volatile 	unsigned int CHx_MATCH_0[GPT_NUMBER_OF_CHANNELS];		        /* 0x7C-0x90 compare match data */
	volatile 	unsigned int DMA_CTRL;				                            /* 0x94 dma status */
	volatile 	unsigned int IMR_CH;				                            /* 0x98 rx fifo overflow and input capture interrupt mask */
	volatile 	unsigned int IMR_TIMER;				                            /* 0x9C timer interrupt mask */
	volatile    const 	unsigned int ISR_CH;				                    /* 0xA0 rx fifo overflow and input capture interrupt status */
	volatile    const 	unsigned int ISR_TIMER;				                    /* 0xA4 timer interrupt status */
	volatile 	unsigned int ICR_CH;				                            /* 0xA8 rx dma done and input capture interrupt clear */
	volatile 	unsigned int ICR_TIMER;				                            /* 0xAC timer1-timer4 interrupt clear */
	volatile 	unsigned int CH_TIMER_ENABLE;		                            /* 0xB0 timer enable */
	volatile 	unsigned int CHxLEDC_TX_FIFO[8];								/* 0xDC-0xF8 ledc channelx transmit fifo for channelx */
	volatile    const 	unsigned int CHx_RX_FIFO[8];							/* 0x100-0x11C channelx rx fifo */
	volatile    const 	unsigned int FIFO_STATUS_0;			                    /* 0x120 channel4-channel5 used data for tx&rx fifo */
	volatile    const 	unsigned int FIFO_STATUS_1;			                    /* 0x124 channel0-channel3 used data for tx&rx fifo */
	volatile    const 	unsigned int FIFO_STATUS_2;			                    /* 0x128 channelx fifo status */
	volatile 	unsigned int FIFO_CONFIG;			                            /* 0x12C clear tx&rx fifo write/read pointer */
	volatile	unsigned int SHADOW_SYNC;			                            /* 0x130 channel0-channel7 shadow register update mode*/
	volatile 	unsigned int SHADOW_LOAD;			                            /* 0x134 channel0-channel7 shadow register load active*/
	volatile 	unsigned int IRSR_CH;				                            /* 0x138 input capture interrupt raw status */
	volatile 	unsigned int IRSR_TIMER;			                            /* 0x13C timer0 - timer3 interrupt raw status*/
	volatile 	unsigned int ICR_FIFO;				                            /* 0x140 clear fifo overflow&underflow interrupt status*/
    volatile    unsigned int SYNC_MODE;                                         /* 0x144 channel sync mode enable*/
} GPT_RegDef;

/*---------------------------------CHx_CLK_CTRL-------------------------------------*/
#define GPT_CHx_CLK_CTRL_CLK_DIV_Pos            (16U)                                         
#define GPT_CHx_CLK_CTRL_CLK_DIV_Msk            (0x7UL << GPT_CHx_CLK_CTRL_CLK_DIV_Pos)                     
#define GPT_CHx_CLK_CTRL_CLK_DIV                GPT_CHx_CLK_CTRL_CLK_DIV_Msk  

#define GPT_CHx_CLK_CTRL_CLK_SEL_Pos            (19U)                                         
#define GPT_CHx_CLK_CTRL_CLK_SEL_Msk            (0x3UL << GPT_CHx_CLK_CTRL_CLK_SEL_Pos)
#define GPT_CHx_CLK_CTRL_CLK_SEL                GPT_CHx_CLK_CTRL_CLK_SEL_Msk    

#define GPT_CHx_CLK_CTRL_CLK_GATE_Pos           (21U)
#define GPT_CHx_CLK_CTRL_CLK_GATE_Msk           (0x1UL << GPT_CHx_CLK_CTRL_CLK_GATE_Pos)
#define GPT_CHx_CLK_CTRL_CLK_GATE               GPT_CHx_CLK_CTRL_CLK_GATE_Msk

#define GPT_CHx_CLK_CTRL_CLK_PREDIV_LD_Pos      (23U)
#define GPT_CHx_CLK_CTRL_CLK_RPEDIV_LD_Msk      (0x1UL << GPT_CHx_CLK_CTRL_CLK_PREDIV_LD_Pos)
#define GPT_CHx_CLK_CTRL_CLK_PREDIV_LD          GPT_CHx_CLK_CTRL_CLK_RPEDIV_LD_Msk

#define GPT_CHx_CLK_CTRL_CLK_DIV_LD_Pos         (24U)
#define GPT_CHx_CLK_CTRL_CLK_DIV_LD_Msk         (0x1UL << GPT_CHx_CLK_CTRL_CLK_DIV_LD_Pos)
#define GPT_CHx_CLK_CTRL_CLK_DIV_LD             GPT_CHx_CLK_CTRL_CLK_DIV_LD_Msk    

/*---------------------------------CHx_CTRL-------------------------------------*/
#define GPT_CHx_CTRL_CH_OPERATION_Pos           (0U)                                         
#define GPT_CHx_CTRL_CH_OPERATION_Msk           (0x7UL << GPT_CHx_CTRL_CH_OPERATION_Pos)                     
#define GPT_CHx_CTRL_CH_OPERATION               GPT_CHx_CTRL_CH_OPERATION_Msk   

#define GPT_CHx_CTRL_CH_MODE_Pos            	(3U)
#define GPT_CHx_CTRL_CH_MODE_Msk            	(0x7UL << GPT_CHx_CTRL_CH_MODE_Pos)
#define GPT_CHx_CTRL_CH_MODE               		GPT_CHx_CTRL_CH_MODE_Msk

#define GPT_CHx_CTRL_PWM_POLARITY_Pos           (6U)                                         
#define GPT_CHx_CTRL_PWM_POLARITY_Msk           (0x1UL << GPT_CHx_CTRL_PWM_POLARITY_Pos)                     
#define GPT_CHx_CTRL_PWM_POLARITY               GPT_CHx_CTRL_PWM_POLARITY_Msk  

#define GPT_CHx_CTRL_PWMOUT_MODE_Pos            (7U)                                         
#define GPT_CHx_CTRL_PWMOUT_MODE_Msk            (0x1UL << GPT_CHx_CTRL_PWMOUT_MODE_Pos)                     
#define GPT_CHx_CTRL_PWMOUT_MODE               	GPT_CHx_CTRL_PWMOUT_MODE_Msk  

#define GPT_CHx_CTRL_IC_SOURCE_Pos            	(8U)                                         
#define GPT_CHx_CTRL_IC_SOURCE_Msk            	(0x1UL << GPT_CHx_CTRL_IC_SOURCE_Pos)                     
#define GPT_CHx_CTRL_IC_SOURCE               	GPT_CHx_CTRL_IC_SOURCE_Msk

#define GPT_CHx_CTRL_IC_EDGE_Pos            	(9U)
#define GPT_CHx_CTRL_IC_EDGE_Msk            	(0x3UL << GPT_CHx_CTRL_IC_EDGE_Pos)
#define GPT_CHx_CTRL_IC_EDGE               		GPT_CHx_CTRL_IC_EDGE_Msk

#define GPT_CHx_CTRL_IC_FILTER_Pos            	(11U)                                         
#define GPT_CHx_CTRL_IC_FILTER_Msk            	(0x7UL << GPT_CHx_CTRL_IC_FILTER_Pos)                     
#define GPT_CHx_CTRL_IC_FILTER               	GPT_CHx_CTRL_IC_FILTER_Msk

#define GPT_CHx_CTRL_CLK_SAMPLE_GATE_Pos        (14U)
#define GPT_CHx_CTRL_CLK_SAMPLE_GATE_Msk        (0x1UL << GPT_CHx_CTRL_CLK_SAMPLE_GATE_Pos)
#define GPT_CHx_CTRL_CLK_SAMPLE_GATE            GPT_CHx_CTRL_CLK_SAMPLE_GATE_Msk

#define GPT_CHx_CTRL_CLK_CNT_GATE_Pos           (15U)
#define GPT_CHx_CTRL_CLK_CNT_GATE_Msk           (0x1UL << GPT_CHx_CTRL_CLK_CNT_GATE_Pos)
#define GPT_CHx_CTRL_CLK_CNT_GATE               GPT_CHx_CTRL_CLK_CNT_GATE_Msk

#define GPT_CHx_CHTRIG_RESET_CNT_EN_Pos         (17U)
#define GPT_CHx_CHTRIG_RESET_CNT_EN_Msk         (0x1UL << GPT_CHx_CHTRIG_RESET_CNT_EN_Pos)
#define GPT_CHx_CHTRIG_RESET_CNT_EN             GPT_CHx_CHTRIG_RESET_CNT_EN_Msk

#define GPT_CHx_CTRL_CH_STOP_Pos            	(18U)
#define GPT_CHx_CTRL_CH_STOP_Msk            	(0x1UL << GPT_CHx_CTRL_CH_STOP_Pos)
#define GPT_CHx_CTRL_CH_STOP               		GPT_CHx_CTRL_CH_STOP_Msk

#define GPT_CHx_CTRL_CH_START_Pos            	(19U)
#define GPT_CHx_CTRL_CH_START_Msk            	(0x1UL << GPT_CHx_CTRL_CH_START_Pos)
#define GPT_CHx_CTRL_CH_START               	GPT_CHx_CTRL_CH_START_Msk


#endif /* __GPT_REG_H */




