#include <string.h>
#include "arcs_ap.h"
#include "log_print.h"
#include "../../apc_i2s_tst_drv.h"
#include "Driver_GPDMA.h"

#define I2S0_DST_ADDR (CMN_PSRAM_REGION+0x100)
#define I2S0_SRC_ADDR (CMN_PSRAM_REGION+0x300)
#define DATA_LENGTH   80     // Number of samples to transfer
#define DEBUG_CLK_SEL 31     // Debug clock selection value

// DMA status flags
volatile struct {
    int tx_done[2];  // TX channels done flags
    int rx_done[2];  // RX channels done flags
    int apc_done;    // APC interrupt done flag
} dma_status;

// Test configuration structure
typedef struct {
    uint8_t bit_depth;   // 0:16bit, 1:24bit, 2:32bit
    uint8_t sample_rate; // 0:8KHz, 3:16KHz
    uint8_t mode;        // 0:mono, 1:stereo, 2:voice
    uint8_t lsb;         // 0:MSB first, 1:LSB first
    const char* desc;
} i2s_test_config_t;

// Available test configurations
const i2s_test_config_t test_configs[] = {
    {0, 3, 0, 0, "16bit mono 16KHz MSB"},      // 16bit mono
    {1, 3, 0, 0, "24bit mono 16KHz MSB"},      // 24bit mono MSB
    {1, 3, 0, 1, "24bit mono 16KHz LSB"},      // 24bit mono LSB
    {2, 0, 1, 0, "32bit stereo 8KHz MSB"},     // 32bit stereo
    {0, 3, 2, 0, "16bit voice mode 16KHz"}     // Voice mode
};

void write_mem(int addr, int val) {
    *(int *)addr = val;
}

int read_mem(int addr) {
    return *(int *)addr;
}

void dump_data(const char* channel, uint32_t* data, uint32_t count) {
    CLOGD("%s Channel Data Dump (%d samples):\n", channel, count);
    for(uint32_t i = 0; i < count; i++) {
        if(i % 8 == 0) CLOGD("\n[%04d]: ", i);
        CLOGD("%08X ", data[i]);
    }
    CLOGD("\n");
}

void eclic_apc_int_handler(void) {
    uint32_t rdata = IP_AUDIO_APC->REG_APC_INTR_TX_ISR.all;

    if(rdata & 0x1) {
        dma_status.apc_done = 1;
        IP_AUDIO_APC->REG_APC_INTR_TX_CLR.bit.TX_CH0_L_FIFO_EMP_CLR = 0x1;
        APC_Tx_Ch_L_Enable(0, 0);
    } else if(rdata & 0x10) {
        // Handle DMA request if needed
        IP_AUDIO_APC->REG_APC_INTR_TX_CLR.bit.TX_CH0_L_DMA_REQ_CLR = 0x1;
    } else {
        CLOGD("Unexpected APC Interrupt: 0x%08X\n", rdata);
    }
}

void eclic_dma_gp_int_handler(void) {
    uint32_t rdata = IP_GPDMA->REG_DMA_INT_STATUS.all;

    if(rdata & 0x1) {
        IP_GPDMA->REG_DMA_INT_CLR.all = 0x1;
        dma_status.tx_done[0] = 1;
    }
    if(rdata & 0x2) {
        IP_GPDMA->REG_DMA_INT_CLR.all = 0x2;
        dma_status.rx_done[0] = 1;
    }
    if(rdata & 0x4) {
        IP_GPDMA->REG_DMA_INT_CLR.all = 0x4;
        dma_status.tx_done[1] = 1;
    }
    if(rdata & 0x8) {
        IP_GPDMA->REG_DMA_INT_CLR.all = 0x8;
        dma_status.rx_done[1] = 1;
    }
}

void init_interrupts(void) {
    register_ISR(IRQ_DMAC_GP_VECTOR, eclic_dma_gp_int_handler, NULL);
    ECLIC_EnableIRQ(IRQ_DMAC_GP_VECTOR);

    register_ISR(IRQ_APC_VECTOR, eclic_apc_int_handler, NULL);
    ECLIC_EnableIRQ(IRQ_APC_VECTOR);
}

void setup_gpio(int iocfg) {
    apc_i2s_gpio_cfg(0, iocfg);

    // Setup debug clock output if needed
    IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_SEL = DEBUG_CLK_SEL;
    IP_SYSCTRL->REG_TEST_CTRL.bit.DBG_CLK_EN = 1;
    IP_AUDIO_CODEC->REG_AUD_R1_GLOBAL0.bit.TEST_OUT_SELA = 1;
}

void setup_i2s_config(const i2s_test_config_t* config) {
    // Configure APC channels
    uint8_t mode = config->bit_depth;
    IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_L_MODE = mode;
    IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_R_MODE = mode;
    IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_L_MODE = mode;
    IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_R_MODE = mode;

    // Set destination/source
    IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_DST_SEL = 0x1; // I2S0
    IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_SRC_SEL = 0x1; // I2S0

    // Configure stereo/voice modes
    if(config->mode == 1) { // Stereo
        IP_AUDIO_APC->REG_APC_TX_CH0_CFG.bit.TX_CH0_STEREO_MODE = 0x1;
        IP_AUDIO_APC->REG_APC_RX_CH0_CFG.bit.RX_CH0_STEREO_MODE = 0x1;
    } else if(config->mode == 2) { // Voice
        IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_SERIAL_MODE = 1;
        IP_AUDIO_APC->REG_APC_I2S0_CFG1.bit.I2S0_SLOTNUM = 4;
        IP_AUDIO_APC->REG_APC_I2S0_CFG1.bit.I2S0_SLOT_LRCK = 0x1D;
    }

    // Configure I2S0
    IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_WLEN = config->bit_depth;
    IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_MASTER_MODE = 0x1;
    IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCK_POL = 0x1;
    IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_LSB = config->lsb;

    if(config->mode != 2) { // Not voice mode
        IP_AUDIO_APC->REG_APC_I2S0_CFG0.bit.I2S0_BCK_FORCE_ON = 0x1;
    }
}

void setup_dma_channels(const i2s_test_config_t* config) {
    // Clear DMA status
    memset((void*)&dma_status, 0, sizeof(dma_status));

    // Common DMA settings
    uint32_t common_ctrl = (1<<AUTO_TFR) | (1<<TFR_MODE) | (1<<CH_ENABLE);

    // TX Channel 0 (Left)
    IP_GPDMA->REG_DMA_SRC_ADDR0_CH0.all = I2S0_SRC_ADDR;
    IP_GPDMA->REG_DMA_DST_ADDR0_CH0.all = (uint32_t)(&IP_AUDIO_APC->REG_APC_TX_CH0_L_DATA.all);
    IP_GPDMA->REG_DMA_CH0_CTRL.all = (8<<HS_SEL) | (3<<SRC_BURST_LEN) | (1<<DST_INC) | common_ctrl;
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH0.all = DATA_LENGTH;

    // RX Channel 0 (Left)
    IP_GPDMA->REG_DMA_SRC_ADDR0_CH1.all = (uint32_t)(&IP_AUDIO_APC->REG_APC_RX_CH0_L_DATA.all);
    IP_GPDMA->REG_DMA_DST_ADDR0_CH1.all = I2S0_DST_ADDR;
    IP_GPDMA->REG_DMA_CH1_CTRL.all = (11<<HS_SEL) | (3<<DST_BURST_LEN) | (1<<SRC_INC) | common_ctrl;
    IP_GPDMA->REG_DMA_BLOCK_LEN_CH1.all = DATA_LENGTH/4;

    // For stereo/voice modes, setup additional channels
    if(config->mode >= 1) {
        // TX Channel 1 (Right)
        IP_GPDMA->REG_DMA_SRC_ADDR0_CH2.all = I2S0_SRC_ADDR + 0x100;
        IP_GPDMA->REG_DMA_DST_ADDR0_CH2.all = (uint32_t)(&IP_AUDIO_APC->REG_APC_TX_CH0_R_DATA.all);
        IP_GPDMA->REG_DMA_CH2_CTRL.all = (9<<HS_SEL) | (3<<SRC_BURST_LEN) | (1<<DST_INC) | common_ctrl;
        IP_GPDMA->REG_DMA_BLOCK_LEN_CH2.all = DATA_LENGTH;

        // RX Channel 1 (Right)
        IP_GPDMA->REG_DMA_SRC_ADDR0_CH3.all = (uint32_t)(&IP_AUDIO_APC->REG_APC_RX_CH0_R_DATA.all);
        IP_GPDMA->REG_DMA_DST_ADDR0_CH3.all = I2S0_DST_ADDR + 0x100;
        IP_GPDMA->REG_DMA_CH3_CTRL.all = (12<<HS_SEL) | (3<<DST_BURST_LEN) | (1<<SRC_INC) | common_ctrl;
        IP_GPDMA->REG_DMA_BLOCK_LEN_CH3.all = DATA_LENGTH/4;
    }

    IP_GPDMA->REG_DMA_INT_EN.bit.CFG_BLOCK_FINISH_INT_EN = 1;
}

void generate_test_data(void) {
    // Generate simple test pattern
    for(int i = 0; i < DATA_LENGTH; i++) {
        write_mem(I2S0_SRC_ADDR + 4*i, 0xAAAAAAAA + i);
        if(i % 2 == 0) {
            write_mem(I2S0_SRC_ADDR + 0x100 + 4*i, 0xFFFFFFF - i);
        }
    }
}

void run_test_case(const i2s_test_config_t* config) {
    // Initialize hardware
    logInit(0, 115200);
    CLOGD("\nStarting I2S Test: %s\n", config->desc);

    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_APC_CLK = 1;
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_I2S_CLK = 1;
    IP_AP_CFG->REG_CLK_CFG0.bit.ENA_DMAC_GP_CLK = 1;

    // Setup hardware
    setup_gpio(1); // Default GPIO config
    init_interrupts();
    setup_i2s_config(config);
    setup_dma_channels(config);
    generate_test_data();

    // Start DMA transfers
    IP_GPDMA->REG_DMA_CH0_CTRL.bit.CFG_CH_START_CH0 = 1;
    IP_GPDMA->REG_DMA_CH1_CTRL.bit.CFG_CH_START_CH1 = 1;
    if(config->mode >= 1) {
        IP_GPDMA->REG_DMA_CH2_CTRL.bit.CFG_CH_START_CH2 = 1;
        IP_GPDMA->REG_DMA_CH3_CTRL.bit.CFG_CH_START_CH3 = 1;
    }

    // Enable APC and channels
    APC_Tx_Ch_L_Enable(0, 1);
    APC_Rx_Ch_L_Enable(0, 1);
    if(config->mode >= 1) {
        APC_Tx_Ch_R_Enable(0, 1);
        APC_Rx_Ch_R_Enable(0, 1);
    }
    APC_Enable(1);
    APC_I2S0_Enable(1);

    // Wait for completion
    CLOGD("Waiting for DMA to complete...\n");
    while(!dma_status.rx_done[0] || (config->mode >= 1 && !dma_status.rx_done[1]));

    // Dump received data
    dump_data("Left", (uint32_t*)I2S0_DST_ADDR, DATA_LENGTH/4);
    if(config->mode >= 1) {
        dump_data("Right", (uint32_t*)(I2S0_DST_ADDR + 0x100), DATA_LENGTH/4);
    }

    CLOGD("Test case %s completed\n", config->desc);
}

int main(void) {
    // Run all test cases
    for(size_t i = 0; i < sizeof(test_configs)/sizeof(test_configs[0]); i++) {
        run_test_case(&test_configs[i]);

        // Add delay between test cases
        for(int j = 0; j < 1000000; j++) { asm("nop"); }
    }

    CLOGD("All I2S test cases completed\n");
    while(1);
}
