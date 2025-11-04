#include "arcs_ap.h"

#include "log_print.h"
#include "../../apc_i2s_tst_drv.h"
#include "Driver_GPDMA.h"


volatile int intr_apc_flag;
volatile int dma_txch_done;
volatile int dma_rxch_done;
#define I2S0_DST_ADDR (CMN_PSRAM_REGION+0x100)
#define I2S0_SRC_ADDR (CMN_PSRAM_REGION+0x300)

uint32_t iis0_dst[16];
uint32_t iis0_src[16];

/* 32 bit word AHB read or write functions:default */
int read_mem(int addr) {
    int rdval;
    rdval = *(int *)addr;   // read value from data memory
    return rdval;
}

void write_mem(int addr, int val) {
    *(int *)addr = val;   // store value in data memory
}

void eclic_apc_int_handler(void){
	CLOGD("APC_IRQ\n");

	if(APC_Rx_Ch_R_FIFO_Overflow_Status() == 0x1){
		APC_Rx_Ch_R_FIFO_Overflow_Clear();
		APC_Rx_Ch_R_FIFO_Overflow_Mask();
	}
	if(APC_Rx_Ch_R_FIFO_Underflow_Status() == 0x1){
		APC_Rx_Ch_R_FIFO_Underflow_Clear();
		APC_Rx_Ch_R_FIFO_Underflow_Mask();
	}
	if(APC_Rx_Ch_R_FIFO_Full_Status() == 0x1){
		APC_Rx_Ch_R_FIFO_Full_Clear();
		APC_Rx_Ch_R_FIFO_Full_Mask();
	}
	if(APC_Rx_Ch_R_FIFO_Empty_Status() == 0x1){
		APC_Rx_Ch_R_FIFO_Empty_Clear();
		APC_Rx_Ch_R_FIFO_Empty_Mask();
	}
	if(APC_Rx_Ch_L_DMA_Req_Status() == 0x1){
		APC_Rx_Ch_L_DMA_Req_Clear();
		APC_Rx_Ch_L_DMA_Req_Mask();
	}
	if(APC_Rx_Ch_L_FIFO_Overflow_Status() == 0x1){
		APC_Rx_Ch_L_FIFO_Overflow_Clear();
		APC_Rx_Ch_L_FIFO_Overflow_Mask();
	}
	if(APC_Rx_Ch_L_FIFO_Underflow_Status() == 0x1){
		APC_Rx_Ch_L_FIFO_Underflow_Clear();
		APC_Rx_Ch_L_FIFO_Underflow_Mask();
	}
	if(APC_Rx_Ch_L_FIFO_Full_Status() == 0x1){
		APC_Rx_Ch_L_FIFO_Full_Clear();
		APC_Rx_Ch_L_FIFO_Full_Mask();
	}
	if(APC_Rx_Ch_L_FIFO_Empty_Status() == 0x1){
		APC_Rx_Ch_L_FIFO_Empty_Clear();
		APC_Rx_Ch_L_FIFO_Empty_Mask();
	}
	if(APC_Tx_Ch_R_FIFO_Overflow_Status() == 0x1){
		APC_Tx_Ch_R_FIFO_Overflow_Clear();
		APC_Tx_Ch_R_FIFO_Overflow_Mask();
	}
	if(APC_Tx_Ch_R_FIFO_Underflow_Status() == 0x1){
		APC_Tx_Ch_R_FIFO_Underflow_Clear();
		APC_Tx_Ch_R_FIFO_Underflow_Mask();
	}
	if(APC_Tx_Ch_R_FIFO_Full_Status() == 0x1){
		APC_Tx_Ch_R_FIFO_Full_Clear();
		APC_Tx_Ch_R_FIFO_Full_Mask();
	}
	if(APC_Tx_Ch_R_FIFO_Empty_Status() == 0x1){
		APC_Tx_Ch_R_FIFO_Empty_Clear();
		APC_Tx_Ch_R_FIFO_Empty_Mask();
	}
	if(APC_Tx_Ch_L_FIFO_Overflow_Status() == 0x1){
		APC_Tx_Ch_L_FIFO_Overflow_Clear();
		APC_Tx_Ch_L_FIFO_Overflow_Mask();
	}
	if(APC_Tx_Ch_L_FIFO_Underflow_Status() == 0x1){
		APC_Tx_Ch_L_FIFO_Underflow_Clear();
		APC_Tx_Ch_L_FIFO_Underflow_Mask();
	}
	if(APC_Tx_Ch_L_FIFO_Full_Status() == 0x1){
		APC_Tx_Ch_L_FIFO_Full_Clear();
		APC_Tx_Ch_L_FIFO_Full_Mask();
	}
	if(APC_Tx_Ch_L_FIFO_Empty_Status() == 0x1){
		APC_Tx_Ch_L_FIFO_Empty_Clear();
		APC_Tx_Ch_L_FIFO_Empty_Mask();
	}

	intr_apc_flag += 1;
}

void eclic_dma_gp_int_handler(void) {
	int rdata;
	rdata = IP_GPDMA->REG_DMA_INT_STATUS.all;
	CLOGD("GPDMA DMA INT rdata = 0x%0x.\n",rdata);
	if((rdata & 0x1) == 0x1) {
		IP_GPDMA->REG_DMA_INT_CLR.all = 0x1; //Clear Channel2 INT
		dma_txch_done = 1;
	}
	if((rdata & 0x2) == 0x2) {
		IP_GPDMA->REG_DMA_INT_CLR.all = 0x2; //Clear Channel2 INT
		dma_rxch_done = 1;
	}
	return;
}

void int_init()
{
	enable_GINT();
	register_ISR(IRQ_DMAC_GP_VECTOR, eclic_dma_gp_int_handler, NULL); //gpdma
	ECLIC_EnableIRQ(IRQ_DMAC_GP_VECTOR);

	register_ISR(IRQ_APC_VECTOR, eclic_apc_int_handler, NULL); //apc
	ECLIC_EnableIRQ(IRQ_APC_VECTOR);
}

int main()
{
	int iocfg = 1;
	int rdata;
	int i;

	uint32_t ref_data_l[16] = {0xfff894, 0xffd2a8, 0xff554c, 0xfe1a4c, 0xfb8f94, 0xf72314, 0xf08d18, 0xe83900, 0xdf92a8, 0xd9037c, 0xd771e8, 0xdd5344, 0xeb93c8, 0x00cfc8, 0x194cf4, 0x2fd628};
	uint32_t ref_data_r[16] = {0xfff894, 0xffd2a8, 0xff554c, 0xfe1a4c, 0xfb8f94, 0xf72314, 0xf08d18, 0xe83900, 0xdf92a8, 0xd9037c, 0xd771e8, 0xdd5344, 0xeb93c8, 0x00cfc8, 0x194cf4, 0x2fd628};
	uint32_t wdata[16] = {0xe2589000, 0xe51ce600, 0xeff50b00, 0xfda76500, 0x087e4800, 0x0dbde700, 0x0e98a600, 0x0e454000, 0x0f025a00, 0x10674e00, 0x10154400, 0x0be30d00, 0x0398bb00, 0xf8eeae00, 0xee149800, 0xe4273b00};

	dma_txch_done=0;
	dma_rxch_done=0;
	intr_apc_flag = 0;

	logInit(0, 115200);
	CLOGD("APC EQ test\n");

    for(i=0; i<16; i++) {
        //write_mem(iis0_src + i, wdata[i]);
        write_mem((int)(iis0_src + i), wdata[i]);
        //write_mem(iis0_src + 0x100 + i, wdata[i]);
    }

	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_APC_CLK = 1;
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_I2S_CLK = 1;

	int_init(); //int init

	//APC Config
	//TX Channel_L Mode 16bit
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_L_MODE = 0x3;
	//TX Channel R Mode
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_R_MODE = 0x3;
	//RX Channel L Mode
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_L_MODE = 0x1;
	//RX Channel R Mode
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_R_MODE = 0x1;
	//Switch TX Channel DST from DAC to I2S0
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_DST_SEL = 0x1;
	//Switch RX Channel SRC from ADC to I2S0
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_SRC_SEL = 0x1;
	//Set Tx Channel Stereo Mode
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_STEREO_MODE = 0x0;
	//Set Rx Channel Stereo Mode
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_STEREO_MODE = 0x0;

	//I2S0 Config
	//I2S0 LOOP BACK MODE
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_MASTER_MODE = 1;
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_LOOP_BACK = 1;
	//Voice Mode
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_SERIAL_MODE = 0;//iis0
	//Set I2S0 DATA Length
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_WLEN = 0x3;
	//Set BCK Polar
	//IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCK_POL = 0x1;
	//IP_AUDIO_APC->REG_APC_I2S0_CFG1.bit.I2S0_BCK_DIV = 0xa;
	//IP_AUDIO_APC->REG_APC_I2S0_CFG1.bit.I2S0_BCK_DIV_LD = 1;
	//IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCK_LRCK = 1;
	//IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BYPASS_FIFOVLD = 1;

	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCK_POL = 0x1;
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCK_LRCK = 18; //18
	IP_AUDIO_APC->REG_APC_I2S0_CFG1.bit.I2S0_BCK_DIV = 14;
	IP_AUDIO_APC->REG_APC_I2S0_CFG1.bit.I2S0_BCK_DIV_LD = 1;

	//MSB of Data out is 1 cycle delay to LRCK edge
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_TX_DLY = 0x1;
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_RX_DLY = 0x1;
	//BCK Force on for master mode
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCK_FORCE_ON = 0x1;
	//Set I2S0 LRCK Polarity  LEFT_L_RIGHT_H
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_LRCK_POL = 1;
	//half cycle delay
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_TX_HALF_CYCLE_DLY = 1;

	APC_EQ_Stage( 0xA );
	APC_EQ_Not_Bypass( );

	APC_EQCoef_0( 0x100000 );   //B0
	APC_EQCoef_1( 0xF35400 );   //B1
	APC_EQCoef_3( 0x100000 );   //B2
	APC_EQCoef_2( 0x125400 );   //A1
	APC_EQCoef_4( 0xF0A800 );   //A2

	APC_EQCoef_5( 0x100000 );   //B0
	APC_EQCoef_6( 0xF66800 );   //B1
	APC_EQCoef_8( 0x100000 );   //B2
	APC_EQCoef_7( 0x132800 );   //A1
	APC_EQCoef_9( 0xF24400 );   //A2

	APC_EQCoef_10( 0x100000 );   //B0
	APC_EQCoef_11( 0x4400   );   //B1
	APC_EQCoef_13( 0x100000 );   //B2
	APC_EQCoef_12( 0x154C00 );   //A1
	APC_EQCoef_14( 0xF48C00 );   //A2

	APC_EQCoef_15( 0x100000 );   //B0
	APC_EQCoef_16( 0x183800 );   //B1
	APC_EQCoef_18( 0x100000 );   //B2
	APC_EQCoef_17( 0x178C00 );   //A1
	APC_EQCoef_19( 0xF6B400 );   //A2

	APC_EQCoef_20( 0x100000 );   //B0
	APC_EQCoef_21( 0xF35400 );   //B1
	APC_EQCoef_23( 0x100000 );   //B2
	APC_EQCoef_22( 0x125400 );   //A1
	APC_EQCoef_24( 0xF0A800 );   //A2

	APC_EQCoef_25( 0x100000 );   //B0
	APC_EQCoef_26( 0xF66800 );   //B1
	APC_EQCoef_28( 0x100000 );   //B2
	APC_EQCoef_27( 0x132800 );   //A1
	APC_EQCoef_29( 0xF24400 );   //A2

	APC_EQCoef_30( 0x100000 );   //B0
	APC_EQCoef_31( 0x4400   );   //B1
	APC_EQCoef_33( 0x100000 );   //B2
	APC_EQCoef_32( 0x154C00 );   //A1
	APC_EQCoef_34( 0xF48C00 );   //A2

	APC_EQCoef_35( 0x100000 );   //B0
	APC_EQCoef_36( 0x183800 );   //B1
	APC_EQCoef_38( 0x100000 );   //B2
	APC_EQCoef_37( 0x178C00 );   //A1
	APC_EQCoef_39( 0xF6B400 );   //A2

	APC_EQCoef_40( 0x100000 );   //B0
	APC_EQCoef_41( 0xF35400 );   //B1
	APC_EQCoef_43( 0x100000 );   //B2
	APC_EQCoef_42( 0x125400 );   //A1
	APC_EQCoef_44( 0xF0A800 );   //A2

	APC_EQCoef_45( 0x100000 );   //B0
	APC_EQCoef_46( 0xF66800 );   //B1
	APC_EQCoef_48( 0x100000 );   //B2
	APC_EQCoef_47( 0x132800 );   //A1
	APC_EQCoef_49( 0xF24400 );   //A2

	//DMA Config
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_DMA_THD_SEL = 0x2; //8 words
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_DMA_THD_SEL = 0x2;
	//Enable DMA Clock
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK = 1;
	//config dma ch0 for tx ch0_l
	IP_GPDMA->REG_DMA_SRC_ADDR0_CH0.all = (uint32_t)iis0_src; //Source addr
	IP_GPDMA->REG_DMA_DST_ADDR0_CH0.all = (uint32_t) (&IP_AUDIO_APC->REG_APC_TX_CH0_L_DATA.all); //Destination addr
	//PER2MEM, WIDTH=32bit, MSIZE=4
	IP_GPDMA->REG_DMA_CH0_CTRL.all = (8<<HS_SEL) | (3<<SRC_BURST_LEN) | (1<<M2P_PRE_FETCH) | (0<<AUTO_TFR) | (1<<DST_INC) | (1<<TFR_MODE) | (1<<CH_ENABLE) | (2<<6);
	IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all = 16; //BLOCK_SIZE=5

	//config dma ch1 for rx ch0_l
	IP_GPDMA->REG_DMA_SRC_ADDR0_CH1.all = (uint32_t) (&IP_AUDIO_APC->REG_APC_RX_CH0_L_DATA.all); //Source addr
	IP_GPDMA->REG_DMA_DST_ADDR0_CH1.all = (uint32_t)iis0_dst; //Destination addr
	//PER2MEM, WIDTH=32bit, MSIZE=4
	IP_GPDMA->REG_DMA_CH1_CTRL.all = (11<<HS_SEL) | (3<<DST_BURST_LEN) | (0<<AUTO_TFR) | (1<<SRC_INC) | (0<<TFR_MODE) | (1<<CH_ENABLE) | (2<<6);
	IP_GPDMA->REG_DMA_BLOCK_LEN_CH1.all = 16; //BLOCK_SIZE=5

	IP_GPDMA->REG_DMA_INT_EN.bit.CFG_BLOCK_FINISH_INT_EN = 1;

	IP_GPDMA->REG_DMA_CH0_CTRL.bit.CFG_CH_START_CH0 = 1;
	IP_GPDMA->REG_DMA_CH1_CTRL.bit.CFG_CH_START_CH1 = 1;

	APC_Enable(1);
	APC_EQ_Clear( );
	APC_Tx_Ch_L_Enable(0,1);
	//APC_Tx_Ch_R_Enable(0,1);
	APC_Rx_Ch_L_Enable(0,1);
	while(!APC_EQ_Clear_Done());
	APC_EQ_Clear_Disable( );
	APC_EQ_Ch_LR_Enable( );
	APC_I2S0_Enable(1);

	CLOGD("Waitting interrupt0...\n");

	while(dma_txch_done==0||dma_rxch_done==0);

	APC_I2S0_Enable(0);        //Disable

	APC_Enable(0);            //Disable

    for(i=0; i<16; i++) {
        rdata = read_mem((int)iis0_dst + i);
        CLOGD("left channel rdata[%0d]:0x%x\n", i, rdata);
        if( rdata != ref_data_l[i] ) {
        	CLOGD("ERROR: left channel rdata[%0d]:0x%x\n", i, rdata);
        }
    }

	while(1);
}

