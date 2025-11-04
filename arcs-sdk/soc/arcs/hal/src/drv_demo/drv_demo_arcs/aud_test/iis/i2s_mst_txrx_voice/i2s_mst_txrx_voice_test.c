#include "arcs_ap.h"

#include "log_print.h"
#include "../../apc_i2s_tst_drv.h"
#include "Driver_GPDMA.h"


volatile int intr_apc_done;
volatile int dma_txch_done;
volatile int dma_rxch_done;
#define I2S0_DST_ADDR (CMN_PSRAM_REGION+0x100)
#define I2S0_SRC_ADDR (CMN_PSRAM_REGION+0x300)
volatile int j;
int exp_data[40];

/* 32 bit word AHB read or write functions:default */
int read_mem(int addr) {
    int rdval;
    rdval = *(int *)addr;   // read value from data memory
    return rdval;
}

void write_mem(int addr, int val) {
    *(int *)addr = val;   // store value in data memory
}

void eclic_dma_gp_int_handler (void) {
	int rdata;
	rdata = IP_GPDMA->REG_DMA_INT_STATUS.all;
	CLOGD("GPDMA DMA INT rdata = 0x%0x.\n",rdata);
	if((rdata & 0x1) == 0x1) {
		IP_GPDMA->REG_DMA_INT_CLR.all = 0x1; //Clear Channel0 INT
		dma_txch_done = 1;
	}
	if((rdata & 0x2) == 0x2) {
		IP_GPDMA->REG_DMA_INT_CLR.all = 0x2; //Clear Channel1 INT
		dma_rxch_done = 1;
	}
	return;
}

void eclic_apc_int_handler (void){
	int rdata;
	rdata = IP_AUDIO_APC->REG_APC_INTR_TX_ISR.all;
	if(rdata & 0x1){
		intr_apc_done = 1;
		IP_AUDIO_APC->REG_APC_INTR_TX_CLR.bit.TX_CH0_L_FIFO_EMP_CLR = 0x1;
		APC_Tx_Ch_L_Enable(0,0);
	}else if(rdata & 0x10){
		if(j>=40){
			IP_AUDIO_APC->REG_APC_INTR_TX_MSK.bit.TX_CH0_L_DMA_REQ_MSK = 0x1;
		}
		for(int i=0;i<8;i++){
			IP_AUDIO_APC->REG_APC_TX_CH0_L_DATA.all = exp_data[j++];
		}
		IP_AUDIO_APC->REG_APC_INTR_TX_CLR.bit.TX_CH0_L_DMA_REQ_CLR = 0x1;
	}else{
		CLOGD("Unexpect APC Interrupt\n");
		//c_fail();
	}
}

void int_init()
{
	register_ISR(IRQ_DMAC_GP_VECTOR, eclic_dma_gp_int_handler, NULL); //gpdma
	ECLIC_EnableIRQ(IRQ_DMAC_GP_VECTOR);

	register_ISR(IRQ_APC_VECTOR, eclic_apc_int_handler, NULL); //apc
	ECLIC_EnableIRQ(IRQ_APC_VECTOR);
}

int main()
{
	int iocfg = 1;
	int rdata;
	uint32_t ref_data_l[20] = {0xffffff, 0xffffff, 0xfffffe, 0xfffffe, 0xfffffd, 0xfffffd, 0xfffffc, 0xfffffc, 0xfffffb, 0xfffffb,
	0xfffffa, 0xfffffa, 0xfffff9, 0xfffff9, 0xfffff8, 0xfffff8, 0xfffff7, 0xfffff7, 0xfffff6, 0xfffff6};
	uint32_t ref_data_r[20] = {0xffffff, 0xffffff, 0xfffffe, 0xfffffe, 0xfffffd, 0xfffffd, 0xfffffc, 0xfffffc, 0xfffffb, 0xfffffb,
	0xfffffa, 0xfffffa, 0xfffff9, 0xfffff9, 0xfffff8, 0xfffff8, 0xfffff7, 0xfffff7, 0xfffff6, 0xfffff6};
	uint32_t act_data_l[20];
	uint32_t act_data_r[20];
	dma_txch_done=0;
	dma_rxch_done=0;

	j=0;
	intr_apc_done = 0;
	CLOGD("APC I2S0 Master txrx voice test\n");
	srand(100);
	for(int i=0;i<160;i++){
		write_mem(I2S0_SRC_ADDR + 4*(2*i), 0xaaaaaaaa+i);
		write_mem(I2S0_SRC_ADDR + 4*((2*i)+1),0xffffffff-i);
	}


	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_APC_CLK = 1;
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_I2S_CLK = 1;
	//I2S0 As Master
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_MASTER_MODE = 1;
	//IO Config
	apc_i2s_gpio_cfg(0,iocfg);

	int_init();

	//APC Config/
	//TX Channel_L Mode 16bit
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_L_MODE = 0x1;
	//TX Channel R Mode 16bit
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_R_MODE = 0x1;
	//RX Channel L Mode 16bit
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_L_MODE = 0x1;
	//RX Channel R Mode 16bit
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_R_MODE = 0x1;
	//Switch TX Channel DST from DAC to I2S0
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_DST_SEL = 0x1;
	//Switch RX Channel SRC from ADC to I2S0
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_SRC_SEL = 0x1;
	//Set Tx Channel Stereo Mode
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_STEREO_MODE = 0x1;
	//Set Rx Channel Stereo Mode
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_STEREO_MODE = 0x1;

	//I2S0 Config
	//Enable bck gate
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCKOUT_GATE = 0x1;
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCK_LRCK = 0;
	//BCK Polarity
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCK_POL = 1;
	//Set I2S0 DATA Length
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_WLEN = 0x2;
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_RX_DLY = 1;
	//Long sync Enable
	IP_AUDIO_APC->REG_APC_I2S0_CFG1.bit.I2S0_LONGSYNC = 1;
	//Voice Mode
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_SERIAL_MODE = 1;
	//Slot Config to 4
	IP_AUDIO_APC->REG_APC_I2S0_CFG1.bit.I2S0_SLOTNUM = 4;
	//Slot lrck
	IP_AUDIO_APC->REG_APC_I2S0_CFG1.bit.I2S0_SLOT_LRCK = 0x1D;


	//DMA Config
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_DMA_THD_SEL = 0x3;
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_DMA_THD_SEL = 0x3;
	//Enable GP_DMAC_CLK
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK = 1;

	//config dma ch0 for tx ch0_l
	IP_GPDMA->REG_DMA_SRC_ADDR0_CH0.all = I2S0_SRC_ADDR; //Source addr
	IP_GPDMA->REG_DMA_DST_ADDR0_CH0.all = (uint32_t) (&IP_AUDIO_APC->REG_APC_TX_CH0_L_DATA.all); //Destination addr
	//PER2MEM, WIDTH=32bit, MSIZE=4
	IP_GPDMA->REG_DMA_CH0_CTRL.all = (8<<HS_SEL) | (3<<SRC_BURST_LEN) | (1<<M2P_PRE_FETCH) | (1<<AUTO_TFR) | (1<<DST_INC) | (1<<TFR_MODE) | (1<<CH_ENABLE);
	IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all = 80;

	//config dma ch1 for rx ch1_l
	IP_GPDMA->REG_DMA_SRC_ADDR0_CH1.all = (uint32_t) (&IP_AUDIO_APC->REG_APC_RX_CH0_L_DATA.all); //Source addr
	IP_GPDMA->REG_DMA_DST_ADDR0_CH1.all = I2S0_DST_ADDR; //Destination addr
	//PER2MEM, WIDTH=32bit, MSIZE=4
	IP_GPDMA->REG_DMA_CH1_CTRL.all = (11<<HS_SEL) | (3<<DST_BURST_LEN) | (1<<AUTO_TFR) | (1<<SRC_INC) | (1<<TFR_MODE) | (1<<CH_ENABLE);
	IP_GPDMA->REG_DMA_BLOCK_LEN_CH1.all = 20;

	IP_GPDMA->REG_DMA_INT_EN.bit.CFG_BLOCK_FINISH_INT_EN = 1;

	IP_GPDMA->REG_DMA_CH0_CTRL.bit.CFG_CH_START_CH0 = 1;
	IP_GPDMA->REG_DMA_CH1_CTRL.bit.CFG_CH_START_CH1 = 1;

	APC_Enable(1);

	APC_Tx_Ch_L_Enable(0, 1);
	APC_Rx_Ch_L_Enable(0, 1);

	APC_I2S0_Enable(1);

	CLOGD("Waitting interrupt0...\n");

	while(dma_txch_done==0||dma_rxch_done==0);

	//  APC_I2S0_Enable(0);        //Disable
	//  APC_Enable(0);            //Disable

	for(int i=0; i<40; i++) {
		rdata = read_mem(I2S0_DST_ADDR + 4*i);
		if(i%2)
			act_data_r[i/2] = rdata;
		else
			act_data_l[i/2] = rdata;
	}
	for(int i=0;i<20;i++){
		if( act_data_l[i] != ref_data_l[i] ) {
			CLOGD("ERROR: left channel rdata[%0d]:0x%x  exp_data:%x\n", i, act_data_l[i],ref_data_l[i]);
			//c_fail();
		}
	}
	for(int i=2;i<20;i++){
		if( act_data_r[i] != ref_data_r[i] ) {
			CLOGD("ERROR: right channel rdata[%0d]:0x%x  exp_data:%x\n", i, act_data_r[i],ref_data_r[i]);
			//c_fail();
		}
	}
	while(1);
}


