#include "set_trng.h"

void *trng_handle = NULL;
static volatile uint32_t testCnt = 0;

#define TRNG_COUNT_NUM   50

//32*32 = 1024bits
#define TRNG_WORD_NUM   32

#define TRNG_GROUP_NUM  10
#define TRNG_GROUP_SIZE 20

static uint32_t trng_interrupt_buffer[TRNG_COUNT_NUM] = {0};
static volatile bool irq_triggered = false;

static bool trng_output_unique(uint32_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len - 1; i++) {
        for (uint32_t j = i + 1; j < len; j++) {
            if (buf[i] == buf[j]) {
                return false;
            }
        }
    }
    return true;
}

static void trng_callbackevent(void *param)
{
    irq_triggered = true;

    if (HAL_TRNG_GetDataReady(trng_handle)) {
        trng_interrupt_buffer[testCnt++] = HAL_TRNG_GetData(trng_handle);
        // CLOG("trng data is 0x%x", trng_interrupt_buffer[testCnt++]);
    }

    HAL_TRNG_Enable(trng_handle);

    if (testCnt > TRNG_COUNT_NUM)
        HAL_TRNG_Disable(trng_handle);
}

bool set_trng_multigroup_value()
{
    uint32_t all_data[TRNG_GROUP_NUM * TRNG_GROUP_SIZE] = {0};
    uint32_t idx = 0;

    HAL_TRNG_Initialize(trng_handle);
    HAL_TRNG_PowerControl(trng_handle, CSK_POWER_FULL);
    HAL_TRNG_Control(trng_handle, CSK_TRNG_COLDTIME_2_23 | CSK_TRNG_HOTTIME_2_17 | CSK_TRNG_DELAYTIME_2_11);
    HAL_TRNG_InterruptDisable(trng_handle);

    for (uint32_t g = 0; g < TRNG_GROUP_NUM; g++) {
        for (uint32_t i = 0; i < TRNG_GROUP_SIZE; i++) {
            HAL_TRNG_Enable(trng_handle);
            while (HAL_TRNG_GetDataReady(trng_handle) == 0);
            all_data[idx++] = HAL_TRNG_GetData(trng_handle);
        }
    }

    bool status = trng_output_unique(all_data, TRNG_GROUP_NUM * TRNG_GROUP_SIZE);

    HAL_TRNG_Uninitialize(trng_handle);
    HAL_TRNG_PowerControl(trng_handle, CSK_POWER_OFF);

    return status;
}

bool set_trng_polling_data_uniqueness()
{
    uint32_t trng_general_buffer[TRNG_COUNT_NUM] = {0};

    HAL_TRNG_Initialize(trng_handle);
    HAL_TRNG_PowerControl(trng_handle, CSK_POWER_FULL);
    HAL_TRNG_Control(trng_handle, CSK_TRNG_COLDTIME_2_23 | CSK_TRNG_HOTTIME_2_17 | CSK_TRNG_DELAYTIME_2_11);
    HAL_TRNG_InterruptDisable(trng_handle);

    for (uint32_t i = 0; i < TRNG_COUNT_NUM; i++) {
        HAL_TRNG_Enable(trng_handle);
        while (HAL_TRNG_GetDataReady(trng_handle) == 0);
        trng_general_buffer[i] = HAL_TRNG_GetData(trng_handle);
    }

    bool status = trng_output_unique(trng_general_buffer, TRNG_COUNT_NUM);

    HAL_TRNG_Disable(trng_handle);
    HAL_TRNG_Uninitialize(trng_handle);
    HAL_TRNG_PowerControl(trng_handle, CSK_POWER_OFF);

    return status;
}

bool set_trng_interrupt_value()
{
    testCnt = 0;
    memset(trng_interrupt_buffer, 0, sizeof(trng_interrupt_buffer));

    HAL_TRNG_Initialize(trng_handle);
    HAL_TRNG_PowerControl(trng_handle, CSK_POWER_FULL);
    HAL_TRNG_Control(trng_handle, CSK_TRNG_COLDTIME_2_23 | CSK_TRNG_HOTTIME_2_17 | CSK_TRNG_DELAYTIME_2_11);

    HAL_TRNG_RegisterCallback(trng_handle, trng_callbackevent);
    HAL_TRNG_InterruptEnable(trng_handle);

    HAL_TRNG_Enable(trng_handle);

    while (testCnt < TRNG_COUNT_NUM);

    HAL_TRNG_Disable(trng_handle);
    HAL_TRNG_Uninitialize(trng_handle);
    HAL_TRNG_PowerControl(trng_handle, CSK_POWER_OFF);

    return irq_triggered;
}
