#include "Driver_GPDMA.h"
#include "chip.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Exported constants --------------------------------------------------------*/
#define CSK_GPDMA_VALIDATION_WORD_LENGTH 4

/* Exported types ------------------------------------------------------------*/
static void *gpdma_src_buffer = NULL;
static void *gpdma_dst_buffer = NULL;

/* Fucntion decalre ------------------------------------------------------------*/
static void GPDMA_NormalMode_M2M_Word_Channel0_Test(void);

/* Fucntion implement ------------------------------------------------------------*/
static volatile uint32_t GPDMA_Event = 0;

static void gpdma_normal_test_callback(uint32_t event, void *workspace)
{
    if (event & CSK_GPDMA_EVENT_TRANSFER_DONE) {
        printf("GPDMA Normal mode trigger\n");
    }

    GPDMA_Event = CSK_GPDMA_EVENT_TRANSFER_DONE;
}

#define CSK_GPDMA_VALIDATION_LENGTH 100

static void GPDMA_NormalMode_M2M_Word_Channel0_Test()
{

    // Malloc buffer
    gpdma_src_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);
    gpdma_dst_buffer = malloc(sizeof(uint32_t) * CSK_GPDMA_VALIDATION_LENGTH);

    // Source buffer value initialize
    uint32_t i = 0;
    for (i = 0; i < CSK_GPDMA_VALIDATION_LENGTH; i++) {
        *((uint32_t *)gpdma_src_buffer + i) = i * i;
    }

    csk_gpdma_init_t gpdma_para = {
        .dma_ch = gp_dma_ch0,
        .burst_len = gpdma_burst_len_1spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_m2m,
        .sample_unit = gpdma_sample_unit_word,
        .src_inc_mode = inc_mode_increase,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .handshake = hs_none,
    };

    GPDMA_Initialize();

    GPDMA_Config(&gpdma_para, gpdma_normal_test_callback, NULL);

    GPDMA_Start_Normal(gp_dma_ch0, (void *)gpdma_src_buffer, (void *)gpdma_dst_buffer, CSK_GPDMA_VALIDATION_LENGTH);

    while (!(GPDMA_Event & CSK_GPDMA_EVENT_TRANSFER_DONE));

    GPDMA_Event = 0;

    int32_t ret = 0;
    ret = memcmp(gpdma_dst_buffer, gpdma_src_buffer, CSK_GPDMA_VALIDATION_LENGTH * CSK_GPDMA_VALIDATION_WORD_LENGTH);
    if (ret != 0) {
        printf("[GPDMA][NORMAL][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer error", gpdma_src_buffer,
               gpdma_dst_buffer);
    } else {
        printf("[GPDMA][NORMAL][M2M][CH0][Word] source: 0x%x -> destination: 0x%x transfer success!!!\n",
               gpdma_src_buffer, gpdma_dst_buffer);
    }

    // Free
    free(gpdma_src_buffer);
    free(gpdma_dst_buffer);
    printf("gpdma end\n");
}
int main(int argc, char **argv)
{
    printf("Hello, world! gpdma\n");
    // Enable global interrupt
    enable_GINT();

    GPDMA_NormalMode_M2M_Word_Channel0_Test();
}
