#include "arcs_ap.h"
#include "log_print.h"
#include "../../apc_i2s_tst_drv.h"
#include "Driver_GPDMA.h"
#include "ClockManager.h"

#define CODEC_DST_ADDR_L  (CMN_PSRAM_REGION+0x200)
#define CODEC_DST_ADDR_R  (CMN_PSRAM_REGION+0x1000)
#define DATA_LENGTH      100  // Number of samples to capture

volatile int dma_ch0_done;
volatile int dma_ch1_done;

// Interrupt Handler
void eclic_dma_gp_int_handler(void) {
    int rdata = IP_GPDMA->REG_DMA_INT_STATUS.all;
    CLOGD("GPDMA INT status: 0x%08X\n", rdata);

    if(rdata & 0x1) {
        IP_GPDMA->REG_DMA_INT_CLR.all = 0x1;
        dma_ch0_done = 1;
    }
    if(rdata & 0x2) {
        IP_GPDMA->REG_DMA_INT_CLR.all = 0x2;
        dma_ch1_done = 1;
    }
}

/* 32 bit word AHB read or write functions */
int read_mem(int addr) {
    return *(int *)addr;
}

void write_mem(int addr, int val) {
    *(int *)addr = val;
}

typedef struct {
    uint8_t adcosr;     // Over-sampling ratio (1:250x, 2:125x)
    uint8_t adcsr;      // Sample rate (3:16KHz)
    const char* desc;   // Description of the test case
} test_config_t;

const test_config_t test_configs[] = {
    {1, 3, "16KHz 250x"},   // 250x oversampling, 16KHz
//    {2, 3, "16KHz 125x"},   // 125x oversampling, 16KHz,analog N/A
//    {4, 3, "16KHz 50x"},    // 50x oversampling,  16KHz,analog N/A
    {1, 8, "48KHz 250x"},   // 250x oversampling, 48KHz
//    {3, 0, "8KHz  100x"},   // 100x oversampling, 8KHz, analog N/A
//    {1, 0, "8KHz  250x"},   // 250x oversampling, 8KHz.analog N/A
    {0, 0, "8KHz  500x"}    // 500x oversampling, 8KHz
};

void dump_data(const char* channel, uint32_t base_addr) {
    CLOGD("%s Channel Data Dump:\n", channel);
    for(int i = 0; i < DATA_LENGTH; i++) {
        if(i % 8 == 0) {
            CLOGD("\n0x%08X: ", base_addr + 4*i);
        }
        CLOGD("%08X ", read_mem(base_addr + 4*i));
    }
    CLOGD("\n");
}

void setup_dma() {
    // Configure DMA.CH0 APC_rx_l to mem
    IP_GPDMA->REG_DMA_SRC_ADDR0_CH0.all = (uint32_t)(&IP_AUDIO_APC->REG_APC_RX_CH0_L_DATA.all);
    IP_GPDMA->REG_DMA_DST_ADDR0_CH0.all = CODEC_DST_ADDR_L;
    IP_GPDMA->REG_DMA_CH0_CTRL.all = (11<<HS_SEL) | (3<<DST_BURST_LEN) | (1<<AUTO_TFR) | (1<<SRC_INC) | (1<<TFR_MODE) | (1<<CH_ENABLE);
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all = DATA_LENGTH;

    // Configure DMA.CH1 APC rx r to mem
    IP_GPDMA->REG_DMA_SRC_ADDR0_CH1.all = (uint32_t)(&IP_AUDIO_APC->REG_APC_RX_CH0_R_DATA.all);
    IP_GPDMA->REG_DMA_DST_ADDR0_CH1.all = CODEC_DST_ADDR_R;
    IP_GPDMA->REG_DMA_CH1_CTRL.all = (12<<HS_SEL) | (3<<DST_BURST_LEN) | (1<<AUTO_TFR) | (1<<SRC_INC) | (1<<TFR_MODE) | (1<<CH_ENABLE);
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH1.all = DATA_LENGTH;

    IP_GPDMA->REG_DMA_INT_EN.bit.CFG_BLOCK_FINISH_INT_EN = 1;
}

void setup_audio(const test_config_t* config) {
    // Enable Audio Clock
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_APC_CLK = 1;
    __HAL_CRM_ADC_CLK_ENABLE();
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK = 1;

    // Rx CH Select ADC01
    IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_SRC_SEL = 0;

    // RX Channel L/R Mode 24bit
    IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_L_MODE = 0x1;
    IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_R_MODE = 0x1;

    // Set RX CH DMA Threshold
    IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_DMA_THD_SEL = 0x2;

    // Configure ADC
    ADC_ENABLE_SETTING();
    IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCOSR = config->adcosr;
    IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCSR = config->adcsr;
    IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.ADCCLK_EN = 1;
    IP_AUDIO_CODEC->REG_AUD_R5_ADC_CTRL0.bit.REG_ADC_RSTN = 1;

    // Enable APC and channels
    APC_Enable(1);
    APC_Rx_Ch_L_Enable(0,1);
    APC_Rx_Ch_R_Enable(0,1);
}

void run_test_case(const test_config_t* config) {
    dma_ch0_done = 0;
    dma_ch1_done = 0;

    logInit(0, 115200);
    CLOGD("\nStarting test case: %s\n", config->desc);

    // Enable DMA Interrupt
    register_ISR(IRQ_DMAC_GP_VECTOR, eclic_dma_gp_int_handler, NULL);
    ECLIC_EnableIRQ(IRQ_DMAC_GP_VECTOR);

    setup_dma();
    setup_audio(config);

    // Start DMA transfers
    IP_GPDMA->REG_DMA_CH0_CTRL.bit.CFG_CH_START_CH0 = 1;
    IP_GPDMA->REG_DMA_CH1_CTRL.bit.CFG_CH_START_CH1 = 1;

    CLOGD("Recording audio data...\n");
    while(!dma_ch1_done);  // Wait for DMA to complete

    // Dump captured data for inspection
    dump_data("Left", CODEC_DST_ADDR_L);
    dump_data("Right", CODEC_DST_ADDR_R);

    CLOGD("Test case %s completed\n", config->desc);
}

int main(void) {
    // Run all test cases
    for(size_t i = 0; i < sizeof(test_configs)/sizeof(test_configs[0]); i++) {
        run_test_case(&test_configs[i]);

        // Add delay between test cases if needed
        for(int j = 0; j < 1000000; j++) { asm("nop"); }
    }

    CLOGD("All test cases completed\n");
    while(1);
}
