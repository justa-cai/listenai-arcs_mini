#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "PowerManager.h"
#include "ClockManager.h"

#include "Driver_GPADC.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"

#include "lisa_log.h"
#include "lisa_mutex.h"
#include "lisa_typedef.h"

#if CONFIG_ARCS_HAL_IC_MUTEX
#include "ic_mutex.h"
#endif

#define TAG "adc_sample"

#ifdef CONFIG_ARCS_HAL_IC_MUTEX
static IC_Mutex gpadc_ic_mutex;
#endif

lisa_mutex_t *gpadc_mutex = NULL;

static inline uint64_t get_time_us(void)
{ 
    return (uint64_t)__RV_CSR_READ(CSR_MCYCLE);
}

static inline uint64_t calc_time_elapsed_us(uint64_t start_time)
{
	return get_time_us() - start_time;
}

static void adc_sample_io_init(void)
{
    // BAT_ID and voice key (BAT_ID和语音键复用同一个引脚)
    // AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, VBAT_ID_PIN_NUM, CSK_AON_IOMUX_FUNC_ALTER3);
    // IP_AON_IOMUX->REG_PAD_AON_GPIOB_04.bit.PAD_AON_GPIOB_04_ANA_SEL = CSK_ANA_IOMUX_FUNC_DEFAULT;

    // Vbat voltage adc sample
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 5, CSK_AON_IOMUX_FUNC_ALTER3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_05.bit.PAD_AON_GPIOB_05_ANA_SEL = CSK_ANA_IOMUX_FUNC_DEFAULT;

    // // charge status
    // AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, VBAT_CHARGE_STATUS_PIN_NUM, CSK_AON_IOMUX_FUNC_ALTER3);
    // IP_AON_IOMUX->REG_PAD_AON_GPIOB_07.bit.PAD_AON_GPIOB_07_ANA_SEL = CSK_ANA_IOMUX_FUNC_DEFAULT;
}

void adc_sample_init(uint32_t ref, bool enable_scale, uint32_t channel)
{
    static bool init_flag = false;

    if (init_flag) {
        return;
    }

    adc_sample_io_init();

    if ((0 == ref) || (1 == ref) || (2 == ref)) {
        LISA_LOGI(TAG, "gpadc ref: %d, enable_scale: %d, channel: 0x%x", ref, enable_scale, channel);
    } else {
        LISA_LOGE(TAG, "error ref value: %d", ref);
        return;
    }

#ifdef CONFIG_ARCS_HAL_IC_MUTEX
    IC_Mutex_init(&gpadc_ic_mutex, IC_MUTEX_SLEEP_WAIT, IC_MUTEX_ID_GPADC);
    IC_Mutex_acquire(&gpadc_ic_mutex);
#endif

    __HAL_PMU_GPADC_RST_ENABLE();   /* RESET GPADC */
    HAL_CRM_SetGpadcClkDiv(12);     /* Set GPADC clock divider */
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 5, CSK_AON_IOMUX_FUNC_ALTER3);
    IP_AON_IOMUX->REG_PAD_AON_GPIOB_05.bit.PAD_AON_GPIOB_05_ANA_SEL = CSK_ANA_IOMUX_FUNC_DEFAULT;

    HAL_GPADC_Initialize(GPADC());
    // HAL_GPADC_Control(GPADC(), channel | CSK_GPADC_DMA_ENABLE(0));

    /*
     * ref=0，表示参考电压为Vbg, 1.2V
     * ref = 1，也表示参考电压为VDD_VA, 1.2V
     * ref = 2, 表示参考电压为VDD_IO / 2 或者 VDD_IO / 3?
     * ref = 3，表示参考电压为Vref_ext, 即外部参考电压？
    */
    // HAL_GPADC_SetVrefSel(GPADC(), ref);

    /* 这个是使能vin通道buf的开关 默认打开； 如果打开，获取采集的值要*3 */
    // HAL_GPADC_SetVinBuf_Enable(GPADC(), enable_scale);

    /* 这个参数表示一次性采集2个数据 */
    // HAL_GPADC_SetTriggerNum(GPADC(), 2);

    #ifdef CONFIG_ARCS_HAL_IC_MUTEX
    disable_IRQ(IRQ_GPADC_VECTOR);
    IC_Mutex_release(&gpadc_ic_mutex);
    #endif

    gpadc_mutex = lisa_mutex_create();
    LISA_ASSERT(gpadc_mutex != NULL, "gpadc_mutex_create failed");

    init_flag = true;

    return;
}

static uint32_t adc_sample_value_to_voltage(uint32_t adc_value)
{
    return (uint16_t)(adc_value*1000.0/1024*1.2*3);
}

uint16_t adc_sample_get_channel_value(uint32_t channel)
{
    uint32_t adc_value;
    uint16_t adc_real_voltage;

    lisa_mutex_lock(gpadc_mutex, LISA_OS_WAIT_FOREVER);

    #ifdef CONFIG_ARCS_HAL_IC_MUTEX
    IC_Mutex_acquire(&gpadc_ic_mutex);
    enable_IRQ(IRQ_GPADC_VECTOR);
    #endif

    // __HAL_PMU_GPADC_RST_ENABLE();   /* RESET GPADC */
    // HAL_CRM_SetGpadcClkDiv(12);     /* Set GPADC clock divider */

    // HAL_GPADC_Initialize(GPADC());
    HAL_GPADC_Control(GPADC(), channel | CSK_GPADC_DMA_ENABLE(0));
    HAL_GPADC_SetVrefSel(GPADC(), 0);
    HAL_GPADC_SetVinBuf_Enable(GPADC(), 1);
    HAL_GPADC_SetTriggerNum(GPADC(), 1);

    HAL_GPADC_Start(GPADC());
    HAL_GPADC_PollForConversion(GPADC(), 0);
    adc_value = HAL_GPADC_GetValue(GPADC(), channel);

    #ifdef CONFIG_ARCS_HAL_IC_MUTEX
    disable_IRQ(IRQ_GPADC_VECTOR);
    IC_Mutex_release(&gpadc_ic_mutex);
    #endif
    lisa_mutex_unlock(gpadc_mutex);

    adc_real_voltage = adc_sample_value_to_voltage(adc_value);

    return adc_real_voltage;
}
