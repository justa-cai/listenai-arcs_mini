#include "arcs_ap.h"

#include "log_print.h"
#include "../../apc_i2s_tst_drv.h"
#include "Driver_GPDMA.h"

volatile int intr_apc_done;
volatile int dma_txch_done_0;
volatile int dma_txch_done_1;
volatile int dma_rxch_done_0;
volatile int dma_rxch_done_1;
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

void eclic_dma_gp_int_handler (void) {
	int rdata;
	rdata = IP_GPDMA->REG_DMA_INT_STATUS.all;
	//CLOGD("GPDMA DMA INT rdata = 0x%0x.\n",rdata);
	if((rdata & 0x1) == 0x1) {
		IP_GPDMA->REG_DMA_INT_CLR.all = 0x1; //Clear Channel0 INT
		dma_txch_done_0 = 1;
	}
	if((rdata & 0x2) == 0x2) {
		IP_GPDMA->REG_DMA_INT_CLR.all = 0x2; //Clear Channel1 INT
		dma_rxch_done_0 = 1;
	}
	if((rdata & 0x4) == 0x4) {
		IP_GPDMA->REG_DMA_INT_CLR.all = 0x4; //Clear Channel2 INT
		dma_txch_done_1 = 1;
	}
	if((rdata & 0x8) == 0x8) {
		IP_GPDMA->REG_DMA_INT_CLR.all = 0x8; //Clear Channel3 INT
		dma_rxch_done_1 = 1;
	}
	return;
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
	#ifdef GPIO_CFG
	int iocfg = GPIO_CFG;
	#else
	int iocfg = 1;
	#endif
	int rdata;
	unsigned int ref_data_l[40];
	unsigned int ref_data_r[40];
	unsigned int wdata_l,wdata_r;
	unsigned int temp[80];
	j=0;
	int i,k;
	intr_apc_done = 0;
	dma_txch_done_0 = 0;
	dma_txch_done_1 = 0;
	dma_rxch_done_0 = 0;
	dma_rxch_done_1 = 0;

	CLOGD("APC I2S0 MST TXRX 16bit mono test --IO:%d\n",iocfg);
	srand(100);
	for(i=0; i<40; i++){
	wdata_l  = 0xffffffff-i;
		write_mem(I2S0_SRC_ADDR + 4*i, wdata_l);
		wdata_r  = 0xaaaaaaaa+i;
		write_mem(I2S0_SRC_ADDR + 0x100 + 4*i, wdata_r);
	}
	for(i=0; i<40; i++){
		ref_data_r[i] = ((0x1+(i<<1))<<16) + (0x0+(i<<1));
		ref_data_l[i] = 0xfffeffff - (0x10001 * (i<<1));
	}

	CLOGD("Data Ready\n");

	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_APC_CLK = 1;
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_I2S_CLK = 1;

	//IO Config
	CLOGD("xx%d\n",iocfg);
	apc_i2s_gpio_cfg(0,iocfg);

	int_init();

	//APC Config
	//TX Channel_L Mode 24bit
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_L_MODE = 0x0;
	//TX Channel R Mode 24bit
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_R_MODE = 0x0;
	//RX Channel L Mode 24bit
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_L_MODE = 0x0;
	//RX Channel R Mode 24bit
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_R_MODE = 0x0;
	//Switch TX Channel DST from DAC to I2S0
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_DST_SEL = 0x1;
	//Switch RX Channel SRC from ADC to I2S0
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_SRC_SEL = 0x1;

	//I2S0 Config
	//Set I2S0 DATA Length
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_WLEN = 0x0;
	//Set BCK Polar
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_SWAP_CHLR = 3;
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCK_POL = 0x1;
	//Tx_MonoR_Stereo_DuplR_Mode
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_TX_MODE = 0x2;
	//MSB of Data in is 1 cycle delay to LRCK edge
	//AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_RX_DLY = 0x1;
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_TX_DLY = 0x1;
	//Set Master Mode
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_MASTER_MODE = 0x1;

	//DMA Config
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_DMA_THD_SEL = 0x2;
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_DMA_THD_SEL = 0x2;
	//Enable GP_DMAC_CLK
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK = 1;
	//config dma ch0 for tx ch0_l
	IP_GPDMA->REG_DMA_SRC_ADDR0_CH0.all = I2S0_SRC_ADDR; //Source addr
	IP_GPDMA->REG_DMA_DST_ADDR0_CH0.all = (uint32_t) (&IP_AUDIO_APC->REG_APC_TX_CH0_L_DATA.all); //Destination addr
	//PER2MEM, WIDTH=32bit, MSIZE=4
	IP_GPDMA->REG_DMA_CH0_CTRL.all = (8<<HS_SEL) | (3<<SRC_BURST_LEN) | (1<<M2P_PRE_FETCH) | (1<<AUTO_TFR) | (1<<DST_INC) | (1<<TFR_MODE) | (1<<CH_ENABLE);
	IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all = 40; //BLOCK_SIZE=5

	//config dma ch1 for rx ch1_l
	IP_GPDMA->REG_DMA_SRC_ADDR0_CH1.all = (uint32_t) (&IP_AUDIO_APC->REG_APC_RX_CH0_L_DATA.all); //Source addr
	IP_GPDMA->REG_DMA_DST_ADDR0_CH1.all = I2S0_DST_ADDR; //Destination addr
	//PER2MEM, WIDTH=32bit, MSIZE=4
	IP_GPDMA->REG_DMA_CH1_CTRL.all = (11<<HS_SEL) | (3<<DST_BURST_LEN) | (1<<AUTO_TFR) | (1<<SRC_INC) | (1<<TFR_MODE) | (1<<CH_ENABLE);
	IP_GPDMA->REG_DMA_BLOCK_LEN_CH1.all = 10; //BLOCK_SIZE=5

	//config dma ch2 for tx ch1_r
	IP_GPDMA->REG_DMA_SRC_ADDR0_CH2.all = (I2S0_SRC_ADDR+0x100); //Source addr
	IP_GPDMA->REG_DMA_DST_ADDR0_CH2.all = (uint32_t) (&IP_AUDIO_APC->REG_APC_TX_CH0_R_DATA.all); //Destination addr
	//PER2MEM, WIDTH=32bit, MSIZE=4
	IP_GPDMA->REG_DMA_CH2_CTRL.all = (9<<HS_SEL) | (3<<SRC_BURST_LEN) | (1<<M2P_PRE_FETCH) | (1<<AUTO_TFR) | (1<<DST_INC) | (1<<TFR_MODE) | (1<<CH_ENABLE);
	IP_GPDMA->REG_DMA_BLOCK_LEN_CH2.all = 40; //BLOCK_SIZE=5

	//config dma ch2 for rx ch1_r
	IP_GPDMA->REG_DMA_SRC_ADDR0_CH3.all = (uint32_t) (&IP_AUDIO_APC->REG_APC_RX_CH0_R_DATA.all); //Source addr
	IP_GPDMA->REG_DMA_DST_ADDR0_CH3.all = (I2S0_DST_ADDR+0x100); //Destination addr
	//PER2MEM, 3IDTH=32bit, MSIZE=4
	IP_GPDMA->REG_DMA_CH3_CTRL.all = (12<<HS_SEL) | (3<<DST_BURST_LEN) | (1<<AUTO_TFR) | (1<<SRC_INC) | (1<<TFR_MODE) | (1<<CH_ENABLE);
	IP_GPDMA->REG_DMA_BLOCK_LEN_CH3.all = 10; //BLOCK_SIZE=5

	IP_GPDMA->REG_DMA_INT_EN.bit.CFG_BLOCK_FINISH_INT_EN = 1;

	CLOGD("Waitting interrupt0...\n");

	IP_GPDMA->REG_DMA_CH0_CTRL.bit.CFG_CH_START_CH0 = 1;
	IP_GPDMA->REG_DMA_CH1_CTRL.bit.CFG_CH_START_CH1 = 1;
	IP_GPDMA->REG_DMA_CH2_CTRL.bit.CFG_CH_START_CH2 = 1;
	IP_GPDMA->REG_DMA_CH3_CTRL.bit.CFG_CH_START_CH3 = 1;


	APC_Tx_Ch_L_Enable(0,1);
	APC_Tx_Ch_R_Enable(0,1);
	APC_Rx_Ch_L_Enable(0,1);
	APC_Rx_Ch_R_Enable(0,1);

	APC_Enable(1);
	APC_I2S0_Enable(1);


	while(dma_txch_done_0==0||dma_rxch_done_0==0 ||dma_txch_done_1 == 0 || dma_rxch_done_1 == 0);

	//  APC_I2S0_Enable(0);        //Disable
	//  APC_Enable(0);            //Disable

	int rdata_r;
	int rdata_l;
	for(int i=0; i<10; i++) {
		rdata_l = read_mem(I2S0_DST_ADDR + 4*i);
		rdata_r = read_mem(I2S0_DST_ADDR + 0x100 + 4*i);
		//CLOGD("left channel rdata[%0d]:0x%x\n", i, rdata);
		if( (rdata_l != ref_data_l[i]) | (rdata_r != ref_data_r[i])){
			CLOGD("ERROR: left channel rdata[%0d]:0x%x\n", i, rdata_l);
			CLOGD("ERROR: right channel rdata[%0d]:0x%x\n", i, rdata_r);
		    //c_fail();
		}
	}
	while(1);
}
