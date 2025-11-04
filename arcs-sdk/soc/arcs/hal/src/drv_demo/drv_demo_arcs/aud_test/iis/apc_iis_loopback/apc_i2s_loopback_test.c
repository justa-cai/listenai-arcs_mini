#include "arcs_ap.h"

#include "log_print.h"
#include "../../apc_i2s_tst_drv.h"
#include "Driver_GPDMA.h"


volatile int intr_apc_flag;
volatile int dma_txch_done;
volatile int dma_rxch_done;
#define I2S0_DST_ADDR (CMN_PSRAM_REGION+0x100)
#define I2S0_SRC_ADDR (CMN_PSRAM_REGION+0x300)

uint32_t iis0_dst[4096];
uint32_t iis0_src[4096];

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
	uint32_t ref_data_l[40];
	dma_txch_done=0;
	dma_rxch_done=0;
	intr_apc_flag = 0;

	logInit(0, 115200);
	CLOGD("APC I2S0 LOOPBACK test\n");
	for(int i=0;i<80;i++){
		write_mem(iis0_src + i, 0xaaaaaaaa+i);
	}

	for(int i=0; i<20; i++) {
		ref_data_l[i] = 0xaaaaaaaa+i;
	}

	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_APC_CLK = 1;
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_I2S_CLK = 1;
	//I2S0 As Slave
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_MASTER_MODE = 1;

	//IO Config
	//apc_i2s_gpio_cfg(0,iocfg);

	int_init(); //int init

	//APC Config
	//TX Channel_L Mode 16bit
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_L_MODE = 0x0;
	//TX Channel R Mode 16bit
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_R_MODE = 0x0;
	//RX Channel L Mode 16bit
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_L_MODE = 0x0;
	//RX Channel R Mode 16bit
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_R_MODE = 0x0;
	//Switch TX Channel DST from DAC to I2S0
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_DST_SEL = 0x1;
	//Switch RX Channel SRC from ADC to I2S0
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_SRC_SEL = 0x1;
	//Set Tx Channel Stereo Mode
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_STEREO_MODE = 0x1;
	//Set Rx Channel Stereo Mode
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_STEREO_MODE = 0x1;

	//I2S0 Config
	//I2S0 LOOP BACK MODE
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_LOOP_BACK = 1;
	//Voice Mode
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_SERIAL_MODE = 1;
	//Set I2S0 DATA Length
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_WLEN = 0x0;
	//Set BCK Polar
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCK_POL = 0x1;
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCK_LRCK = 0;
	//MSB of Data out is 1 cycle delay to LRCK edge
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_TX_DLY = 0x1;
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_RX_DLY = 0x1;
	//BCK Force on for master mode
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCK_FORCE_ON = 0x1;
	//Set I2S0 LRCK Polarity  LEFT_L_RIGHT_H
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_LRCK_POL = 1;
	//half cycle delay
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_TX_HALF_CYCLE_DLY = 1;

	APC_Tx_Ch_R_FIFO_Overflow_Unmask();
	APC_Tx_Ch_L_FIFO_Overflow_Unmask();
	//DMA Config
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_DMA_THD_SEL = 0x0;
	IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_DMA_THD_SEL = 0x0;
	//Enable DMA Clock
	IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK = 1;
	//config dma ch0 for tx ch0_l
	IP_GPDMA->REG_DMA_SRC_ADDR0_CH0.all = iis0_src; //Source addr
	IP_GPDMA->REG_DMA_DST_ADDR0_CH0.all = (uint32_t) (&IP_AUDIO_APC->REG_APC_TX_CH0_L_DATA.all); //Destination addr
	//PER2MEM, WIDTH=32bit, MSIZE=4
	IP_GPDMA->REG_DMA_CH0_CTRL.all = (8<<HS_SEL) | (3<<SRC_BURST_LEN) | (1<<M2P_PRE_FETCH) | (0<<AUTO_TFR) | (1<<DST_INC) | (1<<TFR_MODE) | (1<<CH_ENABLE) | (2<<6);
	IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all = 40; //BLOCK_SIZE=5

	//config dma ch1 for rx ch1_l
	IP_GPDMA->REG_DMA_SRC_ADDR0_CH1.all = (uint32_t) (&IP_AUDIO_APC->REG_APC_RX_CH0_L_DATA.all); //Source addr
	IP_GPDMA->REG_DMA_DST_ADDR0_CH1.all = iis0_dst; //Destination addr
	//PER2MEM, WIDTH=32bit, MSIZE=4
	IP_GPDMA->REG_DMA_CH1_CTRL.all = (11<<HS_SEL) | (3<<DST_BURST_LEN) | (0<<AUTO_TFR) | (1<<SRC_INC) | (0<<TFR_MODE) | (1<<CH_ENABLE) | (2<<6);
	IP_GPDMA->REG_DMA_BLOCK_LEN_CH1.all = 40; //BLOCK_SIZE=5

	IP_GPDMA->REG_DMA_INT_EN.bit.CFG_BLOCK_FINISH_INT_EN = 1;

	IP_GPDMA->REG_DMA_CH0_CTRL.bit.CFG_CH_START_CH0 = 1;
	IP_GPDMA->REG_DMA_CH1_CTRL.bit.CFG_CH_START_CH1 = 1;

	APC_Tx_Ch_L_Enable(0,1);
	APC_Tx_Ch_R_Enable(0,1);
	APC_Rx_Ch_L_Enable(0,1);
	APC_Enable(1);

	APC_I2S0_Enable(1);

	CLOGD("Waitting interrupt0...\n");

	while(dma_txch_done==0||dma_rxch_done==0);

	APC_I2S0_Enable(0);        //Disable

	APC_Enable(0);            //Disable

	for(int i=0; i<20; i++) {
	rdata = read_mem(iis0_dst + i);
	CLOGD("left channel rdata[%0d]:0x%x\n", i, rdata);
	if( (rdata != ref_data_l[i]) | (APC_Rx_Ch_R_Data( ) != 0x0)) {
		CLOGD("ERROR: left channel rdata[%0d]:0x%x\n", i, rdata);
		CLOGD("ERROR: right channel rdata[%0d]:0x%x\n", i, APC_Rx_Ch_R_Data( ));
		//c_fail();
	}
	}

	//generate tx overflow int
	IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_ENABLE = 0x0;
	IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_STEREO_MODE = 0x0;

	for(int i=0; i<20; i++) {
		APC_Tx_Ch_L_Data( 0xFFFFFFFF );
	}

	while(intr_apc_flag == 0x0);
	CLOGD("TX CH L overflow\n");

	for(int i=0; i<20; i++) {
		APC_Tx_Ch_R_Data( 0xFFFFFFFF );
	}
	while(intr_apc_flag < 0x2);
	CLOGD("TX CH R overflow\n");
	APC_Tx_Ch_FIFO_Flush( );
	if((IP_AUDIO_APC->REG_APC_TX_CH0_L_DATA.all != 0x0) || (IP_AUDIO_APC->REG_APC_TX_CH0_R_DATA.all != 0x0)) {
		CLOGD("TX CH FIFO FLUSH FAILED!\n");
		//c_fail();
	}

	while(1);
}

