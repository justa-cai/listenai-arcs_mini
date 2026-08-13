#include <stdio.h>
#include <stdlib.h>

#include "venusa_ap.h"
#include "log_print.h"
#include "systick.h"

#include "Driver_GPT_TIMER.h"
#include "Driver_GPT_Common.h"

#define GPT_TIMER_CHANNEL_0				0
#define GPT_TIMER_CHANNEL_1				1


#define __GPT_TIMER_CHANNEL       		0

static GPT_Config_Para_t gpt_config_para = {
	.clk_src = GPT_CLK_SRC_T0,
	.prediv = 90,
	.clk_div = GPT_CLK_DIV_32,
};

static void GPT_TIMER_16Bits_Event_Callback(uint32_t event, void *param) {
	uint16_t count;
	HAL_GPT_ReadTimerCount(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_16BITS_INDEX_0, &count);

	CLOGD("Complete repeat and current cycle: %d", count);
}

static void GPT_TIMER_16Bits_Event_Callback_Channel_0(uint32_t event, void *param) {
	uint16_t count;
	HAL_GPT_ReadTimerCount(GPT0(), GPT_TIMER_CHANNEL_0, HAL_GPT_TIMER_16BITS_INDEX_0, &count);

	CLOGD("Complete repeat channel %d and current cycle: %d", GPT_TIMER_CHANNEL_0, count);
}

static void GPT_TIMER_16Bits_Event_Callback_Channel_1(uint32_t event, void *param) {
	uint16_t count;
	HAL_GPT_ReadTimerCount(GPT0(), GPT_TIMER_CHANNEL_1, HAL_GPT_TIMER_16BITS_INDEX_0, &count);

	CLOGD("Complete repeat channel %d and current cycle: %d", GPT_TIMER_CHANNEL_1, count);
}

/*
 * 16_bit定时器周期计算示例：
 *
 *   clk_src = 24_000_000 Hz
 *   pre_div = 90
 *   div     = 32
 *   CNT     = 0xFFFF = 65535
 *
 *   F_timer = clk_src / ((pre_div + 1) * div)
 *           = 24_000_000 / (91 * 32)
 *           = 24_000_000 / 2912
 *           ≈ 8240.66 Hz
 *
 *   Timer period (T) = CNT / F_timer
 *                    = 65535 / 8240.66
 *                    ≈ 7.95
 */

static void gpt_test_timer_16bits_upmode_single(void) {
	GPT_TIMER_Config_Para_t gpt_timer_config = {
		.index = HAL_GPT_TIMER_16BITS_INDEX_0,
		.run_mode = GPT_TIMER_RUNMODE_SINGLE,	
		.cnt_mode = GPT_TIMER_COUNTMODE_UP,
	};

	HAL_GPT_Initialize(GPT0());

	HAL_GPT_PowerControl(GPT0(), CSK_POWER_FULL);

	HAL_GPT_Control(GPT0(), __GPT_TIMER_CHANNEL, &gpt_config_para);

	HAL_GPT_TimerControl(GPT0(), __GPT_TIMER_CHANNEL, &gpt_timer_config);

	HAL_GPT_RegisterTimerCallback(GPT0(), __GPT_TIMER_CHANNEL, GPT_TIMER_16Bits_Event_Callback, NULL);

	HAL_GPT_SetTimerPeriodByCount(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_16BITS_INDEX_0, 0xFFFF);

	HAL_GPT_StartTimer(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_16BITS_INDEX_0);
}

static void gpt_test_timer_16bits_upmode_repeat(void) {
	GPT_TIMER_Config_Para_t gpt_timer_config = {
		.index = HAL_GPT_TIMER_16BITS_INDEX_0,
		.run_mode = GPT_TIMER_RUNMODE_REPEAT,
		.cnt_mode = GPT_TIMER_COUNTMODE_UP,
	};

    HAL_GPT_Initialize(GPT0());

    HAL_GPT_PowerControl(GPT0(), CSK_POWER_FULL);

    HAL_GPT_Control(GPT0(), __GPT_TIMER_CHANNEL, &gpt_config_para);

    HAL_GPT_TimerControl(GPT0(), __GPT_TIMER_CHANNEL, &gpt_timer_config);

    HAL_GPT_RegisterTimerCallback(GPT0(), __GPT_TIMER_CHANNEL, GPT_TIMER_16Bits_Event_Callback, NULL);

    HAL_GPT_SetTimerPeriodByCount(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_16BITS_INDEX_0, 0xFFFF);

    HAL_GPT_StartTimer(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_16BITS_INDEX_0);
}

static void gpt_test_timer_16bits_downmode_repeat(void) {
	GPT_TIMER_Config_Para_t gpt_timer_config = {
		.index = HAL_GPT_TIMER_16BITS_INDEX_0,
		.run_mode = GPT_TIMER_RUNMODE_REPEAT,
		.cnt_mode = GPT_TIMER_COUNTMODE_DOWN,
	};

    HAL_GPT_Initialize(GPT0());

    HAL_GPT_PowerControl(GPT0(), CSK_POWER_FULL);

    HAL_GPT_Control(GPT0(), __GPT_TIMER_CHANNEL, &gpt_config_para);

    HAL_GPT_TimerControl(GPT0(), __GPT_TIMER_CHANNEL, &gpt_timer_config);

    HAL_GPT_RegisterTimerCallback(GPT0(), __GPT_TIMER_CHANNEL, GPT_TIMER_16Bits_Event_Callback, NULL);

    HAL_GPT_SetTimerPeriodByCount(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_16BITS_INDEX_0, 0xFFFF);

    HAL_GPT_StartTimer(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_16BITS_INDEX_0);
}

static void gpt_test_timer_16bits_downmode_freerun(void) {
	GPT_TIMER_Config_Para_t gpt_timer_config = {
		.index = HAL_GPT_TIMER_16BITS_INDEX_0,
		.run_mode = GPT_TIMER_RUNMODE_FREE_RUN,
		.cnt_mode = GPT_TIMER_COUNTMODE_DOWN,
	};

    HAL_GPT_Initialize(GPT0());

    HAL_GPT_PowerControl(GPT0(), CSK_POWER_FULL);

    HAL_GPT_Control(GPT0(), __GPT_TIMER_CHANNEL, &gpt_config_para);

    HAL_GPT_TimerControl(GPT0(), __GPT_TIMER_CHANNEL, &gpt_timer_config);

    HAL_GPT_RegisterTimerCallback(GPT0(), __GPT_TIMER_CHANNEL, GPT_TIMER_16Bits_Event_Callback, NULL);

    //not reload, never stop
    HAL_GPT_StartTimer(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_16BITS_INDEX_0);
}

static void gpt_test_timer_16bits_updownmode_single(void) {
	GPT_TIMER_Config_Para_t gpt_timer_config = {
		.index = HAL_GPT_TIMER_16BITS_INDEX_0,
		.run_mode = GPT_TIMER_RUNMODE_SINGLE,
		.cnt_mode = GPT_TIMER_COUNTMODE_UPDOWM,
	};

    HAL_GPT_Initialize(GPT0());

    HAL_GPT_PowerControl(GPT0(), CSK_POWER_FULL);

    HAL_GPT_Control(GPT0(), __GPT_TIMER_CHANNEL, &gpt_config_para);

    HAL_GPT_TimerControl(GPT0(), __GPT_TIMER_CHANNEL, &gpt_timer_config);

    HAL_GPT_RegisterTimerCallback(GPT0(), __GPT_TIMER_CHANNEL, GPT_TIMER_16Bits_Event_Callback, NULL);

    HAL_GPT_SetTimerPeriodByCount(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_16BITS_INDEX_0, 0xFFFF);

    HAL_GPT_StartTimer(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_16BITS_INDEX_0);
}

static void gpt_test_timer_16bits_multiple_channels_repeat(void) {
	GPT_TIMER_Config_Para_t gpt_timer_config_channel_0 = {
		.index = HAL_GPT_TIMER_16BITS_INDEX_0,
		.run_mode = GPT_TIMER_RUNMODE_REPEAT,
		.cnt_mode = GPT_TIMER_COUNTMODE_UP,
	};

	GPT_TIMER_Config_Para_t gpt_timer_config_channel_1 = {
		.index = HAL_GPT_TIMER_16BITS_INDEX_0,
		.run_mode = GPT_TIMER_RUNMODE_REPEAT,
		.cnt_mode = GPT_TIMER_COUNTMODE_DOWN,
	};

	HAL_GPT_Initialize(GPT0());

	HAL_GPT_PowerControl(GPT0(), CSK_POWER_FULL);

	HAL_GPT_Control(GPT0(), GPT_TIMER_CHANNEL_0, &gpt_config_para);
	HAL_GPT_Control(GPT0(), GPT_TIMER_CHANNEL_1, &gpt_config_para);

	HAL_GPT_TimerControl(GPT0(), GPT_TIMER_CHANNEL_0, &gpt_timer_config_channel_0);
	HAL_GPT_TimerControl(GPT0(), GPT_TIMER_CHANNEL_1, &gpt_timer_config_channel_1);

	HAL_GPT_RegisterTimerCallback(GPT0(), GPT_TIMER_CHANNEL_0, GPT_TIMER_16Bits_Event_Callback_Channel_0, NULL);
	HAL_GPT_RegisterTimerCallback(GPT0(), GPT_TIMER_CHANNEL_1, GPT_TIMER_16Bits_Event_Callback_Channel_1, NULL);

	HAL_GPT_SetTimerPeriodByCount(GPT0(), GPT_TIMER_CHANNEL_0, HAL_GPT_TIMER_16BITS_INDEX_0, 0xFFFF);
	HAL_GPT_SetTimerPeriodByCount(GPT0(), GPT_TIMER_CHANNEL_1, HAL_GPT_TIMER_16BITS_INDEX_0, 0x7FFF);

	HAL_GPT_StartTimer(GPT0(), GPT_TIMER_CHANNEL_0, HAL_GPT_TIMER_16BITS_INDEX_0);
	HAL_GPT_StartTimer(GPT0(), GPT_TIMER_CHANNEL_1, HAL_GPT_TIMER_16BITS_INDEX_0);
}

static void GPT_TIMER_8Bits_Mixed_Callback(uint32_t event, void *param) {
    if (event == CSK_GPT_EVENT_8BITS_TIMER0_COMPLETE) {
        CLOGD("Complete 8bits_index0_event: %d", event);
    } else if (event == CSK_GPT_EVENT_8BITS_TIMER1_COMPLETE) {
        CLOGD("Complete 8bits_index1_event: %d", event);
    } else {
        CLOGD("Unknown GPT event: %d", event);
    }
}

static void GPT_TIMER_8Bits_Event_Callback_Channel_0(uint32_t event, void *param) {
	CLOGD("Complete Channel 0 8bits_index0_event: %d", event);
}

static void GPT_TIMER_8Bits_Event_Callback_Channel_1(uint32_t event, void *param) {
	CLOGD("Complete Channel 1 8bits_index1_event: %d", event);
}

/*
 * 8_bit定时器周期计算示例：
 *
 *   clk_src = 24_000_000 Hz
 *   pre_div = 90
 *   div     = 32
 *   CNT     = 0xFF = 255
 *
 *   F_timer = clk_src / ((pre_div + 1) * div)
 *           = 24_000_000 / (91 * 32)
 *           = 24_000_000 / 2912
 *           ≈ 8240.66 Hz
 *
 *   Timer period (T) = CNT / F_timer
 *                    = 255 / 8240.66
 *                    ≈ 123.8ms
 */
static void gpt_test_timer_8bits_index0_upmode_single(void) {
	GPT_TIMER_Config_Para_t gpt_timer_config = {
		.index = HAL_GPT_TIMER_8BITS_INDEX_0,
		.run_mode = GPT_TIMER_RUNMODE_SINGLE,
		.cnt_mode = GPT_TIMER_COUNTMODE_UP,
	};

	HAL_GPT_Initialize(GPT0());

	HAL_GPT_PowerControl(GPT0(), CSK_POWER_FULL);

	HAL_GPT_Control(GPT0(), __GPT_TIMER_CHANNEL, &gpt_config_para);

	HAL_GPT_TimerControl(GPT0(), __GPT_TIMER_CHANNEL, &gpt_timer_config);

	HAL_GPT_RegisterTimerCallback(GPT0(), __GPT_TIMER_CHANNEL, GPT_TIMER_8Bits_Mixed_Callback, NULL);

    HAL_GPT_SetTimerPeriodByCount(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_8BITS_INDEX_0, 0xFF);

    HAL_GPT_StartTimer(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_8BITS_INDEX_0);
}

static void gpt_test_timer_8bits_index1_upmode_single(void) {
	GPT_TIMER_Config_Para_t gpt_timer_config = {
		.index = HAL_GPT_TIMER_8BITS_INDEX_1,
		.run_mode = GPT_TIMER_RUNMODE_SINGLE,
		.cnt_mode = GPT_TIMER_COUNTMODE_UP,
	};

	HAL_GPT_Initialize(GPT0());

	HAL_GPT_PowerControl(GPT0(), CSK_POWER_FULL);

	HAL_GPT_Control(GPT0(), __GPT_TIMER_CHANNEL, &gpt_config_para);

	HAL_GPT_TimerControl(GPT0(), __GPT_TIMER_CHANNEL, &gpt_timer_config);

	HAL_GPT_RegisterTimerCallback(GPT0(), __GPT_TIMER_CHANNEL, GPT_TIMER_8Bits_Mixed_Callback, NULL);

	HAL_GPT_SetTimerPeriodByCount(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_8BITS_INDEX_1, 0xFF);

	HAL_GPT_StartTimer(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_8BITS_INDEX_1);
}

static void gpt_test_timer_8bits_index0_upmode_repeat(void) {
	GPT_TIMER_Config_Para_t gpt_timer_config = {
		.index = HAL_GPT_TIMER_8BITS_INDEX_0,
		.run_mode = GPT_TIMER_RUNMODE_REPEAT,
		.cnt_mode = GPT_TIMER_COUNTMODE_UP,
	};

	HAL_GPT_Initialize(GPT0());

	HAL_GPT_PowerControl(GPT0(), CSK_POWER_FULL);

	HAL_GPT_Control(GPT0(), __GPT_TIMER_CHANNEL, &gpt_config_para);

	HAL_GPT_TimerControl(GPT0(), __GPT_TIMER_CHANNEL, &gpt_timer_config);

	HAL_GPT_RegisterTimerCallback(GPT0(), __GPT_TIMER_CHANNEL, GPT_TIMER_8Bits_Mixed_Callback, NULL);

	HAL_GPT_SetTimerPeriodByCount(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_8BITS_INDEX_0, 0xFF);

	HAL_GPT_StartTimer(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_8BITS_INDEX_0);
}

static void gpt_test_timer_8bits_index1_upmode_repeat(void) {
	GPT_TIMER_Config_Para_t gpt_timer_config = {
		.index = HAL_GPT_TIMER_8BITS_INDEX_1,
		.run_mode = GPT_TIMER_RUNMODE_REPEAT,
		.cnt_mode = GPT_TIMER_COUNTMODE_UP,
	};

	HAL_GPT_Initialize(GPT0());

	HAL_GPT_PowerControl(GPT0(), CSK_POWER_FULL);

	HAL_GPT_Control(GPT0(), __GPT_TIMER_CHANNEL, &gpt_config_para);

	HAL_GPT_TimerControl(GPT0(), __GPT_TIMER_CHANNEL, &gpt_timer_config);

	HAL_GPT_RegisterTimerCallback(GPT0(), __GPT_TIMER_CHANNEL, GPT_TIMER_8Bits_Mixed_Callback, NULL);

	HAL_GPT_SetTimerPeriodByCount(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_8BITS_INDEX_1, 0xFF);

	HAL_GPT_StartTimer(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_8BITS_INDEX_1);
}

static void gpt_test_timer_8bits_index0_1_downmode_repeat(void) {
	GPT_TIMER_Config_Para_t gpt_timer_config = {
		.index = HAL_GPT_TIMER_8BITS_INDEX_0 | HAL_GPT_TIMER_8BITS_INDEX_1,
		.run_mode = GPT_TIMER_RUNMODE_REPEAT,
		.cnt_mode = GPT_TIMER_COUNTMODE_DOWN,
	};

	HAL_GPT_Initialize(GPT0());

	HAL_GPT_PowerControl(GPT0(), CSK_POWER_FULL);

	HAL_GPT_Control(GPT0(), __GPT_TIMER_CHANNEL, &gpt_config_para);

	HAL_GPT_TimerControl(GPT0(), __GPT_TIMER_CHANNEL, &gpt_timer_config);

	HAL_GPT_RegisterTimerCallback(GPT0(), __GPT_TIMER_CHANNEL, GPT_TIMER_8Bits_Mixed_Callback, NULL);

	HAL_GPT_SetTimerPeriodByCount(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_8BITS_INDEX_0, 0xFF);
	HAL_GPT_SetTimerPeriodByCount(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_8BITS_INDEX_1, 0xFF);

	HAL_GPT_StartTimer(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_8BITS_INDEX_0);
	HAL_GPT_StartTimer(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_8BITS_INDEX_1);
}

static void gpt_test_timer_8bits_index0_upmode_repeat_vs_index1_downmode_repeat(void) {
	GPT_TIMER_Config_Para_t gpt_timer_config_index_0 = {
		.index = HAL_GPT_TIMER_8BITS_INDEX_0,
		.run_mode = GPT_TIMER_RUNMODE_REPEAT,
		.cnt_mode = GPT_TIMER_COUNTMODE_UP,
	};

	GPT_TIMER_Config_Para_t gpt_timer_config_index_1 = {
		.index = HAL_GPT_TIMER_8BITS_INDEX_1,
		.run_mode = GPT_TIMER_RUNMODE_REPEAT,
		.cnt_mode = GPT_TIMER_COUNTMODE_DOWN,
	};

	HAL_GPT_Initialize(GPT0());

	HAL_GPT_PowerControl(GPT0(), CSK_POWER_FULL);

	HAL_GPT_Control(GPT0(), __GPT_TIMER_CHANNEL, &gpt_config_para);

	HAL_GPT_TimerControl(GPT0(), __GPT_TIMER_CHANNEL, &gpt_timer_config_index_0);
	HAL_GPT_TimerControl(GPT0(), __GPT_TIMER_CHANNEL, &gpt_timer_config_index_1);

	HAL_GPT_RegisterTimerCallback(GPT0(), __GPT_TIMER_CHANNEL, GPT_TIMER_8Bits_Mixed_Callback, NULL);

	HAL_GPT_SetTimerPeriodByCount(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_8BITS_INDEX_0, 0xFF);
	HAL_GPT_SetTimerPeriodByCount(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_8BITS_INDEX_1, 0x80);

	HAL_GPT_StartTimer(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_8BITS_INDEX_0);
	HAL_GPT_StartTimer(GPT0(), __GPT_TIMER_CHANNEL, HAL_GPT_TIMER_8BITS_INDEX_1);
}

void gpt_test_timer_8bits_multiple_channels_repeat(void) {
	GPT_TIMER_Config_Para_t gpt_timer_config_index_0 = {
		.index = HAL_GPT_TIMER_8BITS_INDEX_0,
		.run_mode = GPT_TIMER_RUNMODE_REPEAT,
		.cnt_mode = GPT_TIMER_COUNTMODE_UP,
	};

	GPT_TIMER_Config_Para_t gpt_timer_config_index_1 = {
		.index = HAL_GPT_TIMER_8BITS_INDEX_1,
		.run_mode = GPT_TIMER_RUNMODE_REPEAT,
		.cnt_mode = GPT_TIMER_COUNTMODE_DOWN,
	};

	HAL_GPT_Initialize(GPT0());

	HAL_GPT_PowerControl(GPT0(), CSK_POWER_FULL);

	HAL_GPT_Control(GPT0(), GPT_TIMER_CHANNEL_0, &gpt_config_para);
	HAL_GPT_Control(GPT0(), GPT_TIMER_CHANNEL_1, &gpt_config_para);

	HAL_GPT_TimerControl(GPT0(), GPT_TIMER_CHANNEL_0, &gpt_timer_config_index_0); // channel_0 index_0
	HAL_GPT_TimerControl(GPT0(), GPT_TIMER_CHANNEL_1, &gpt_timer_config_index_1); // channel_1 index_1

	HAL_GPT_RegisterTimerCallback(GPT0(), GPT_TIMER_CHANNEL_0, GPT_TIMER_8Bits_Event_Callback_Channel_0, NULL);
	HAL_GPT_RegisterTimerCallback(GPT0(), GPT_TIMER_CHANNEL_1, GPT_TIMER_8Bits_Event_Callback_Channel_1, NULL);

	HAL_GPT_SetTimerPeriodByCount(GPT0(), GPT_TIMER_CHANNEL_0, HAL_GPT_TIMER_8BITS_INDEX_0, 0xFF);
	HAL_GPT_SetTimerPeriodByCount(GPT0(), GPT_TIMER_CHANNEL_1, HAL_GPT_TIMER_8BITS_INDEX_1, 0x80);

	HAL_GPT_StartTimer(GPT0(), GPT_TIMER_CHANNEL_0, HAL_GPT_TIMER_8BITS_INDEX_0);
	HAL_GPT_StartTimer(GPT0(), GPT_TIMER_CHANNEL_1, HAL_GPT_TIMER_8BITS_INDEX_1);
}

typedef struct
{
	void (*function)(void);
	const char* name;
}test_case_t;

static test_case_t test_array[] = {
	// {gpt_test_timer_16bits_upmode_single, "gpt_test_timer_16bits_upmode_single"},
	// {gpt_test_timer_16bits_upmode_repeat, "gpt_test_timer_16bits_upmode_repeat"},
	// {gpt_test_timer_16bits_downmode_repeat, "gpt_test_timer_16bits_downmode_repeat"},
	// {gpt_test_timer_16bits_downmode_freerun, "gpt_test_timer_16bits_downmode_freerun"},
	// {gpt_test_timer_16bits_updownmode_single, "gpt_test_timer_16bits_updownmode_single"},
	// {gpt_test_timer_16bits_multiple_channels_repeat, "gpt_test_timer_16bits_multiple_channels_repeat"}

	// {gpt_test_timer_8bits_index0_upmode_single, "gpt_test_timer_8bits_index0_upmode_single"},
	// {gpt_test_timer_8bits_index1_upmode_single, "gpt_test_timer_8bits_index1_upmode_single"},
	// {gpt_test_timer_8bits_index0_upmode_repeat, "gpt_test_timer_8bits_index0_upmode_repeat"},
	// {gpt_test_timer_8bits_index1_upmode_repeat, "gpt_test_timer_8bits_index1_upmode_repeat"},
	// {gpt_test_timer_8bits_index0_1_downmode_repeat, "gpt_test_timer_8bits_index0_1_downmode_repeat"},
	// {gpt_test_timer_8bits_index0_upmode_repeat_vs_index1_downmode_repeat, "gpt_test_timer_8bits_index0_upmode_repeat_vs_index1_downmode_repeat"},
	// {gpt_test_timer_8bits_multiple_channels_repeat, "gpt_test_timer_8bits_multiple_channels_repeat"},
};

int main(void) {
    logInit(0, 115200);
    CLOGD("enter main: gpt timer test\r");
    uint32_t index;

    for (index = 0; index < sizeof(test_array) / sizeof(test_array[0]); index++)
    {
        test_case_t item = test_array[index];
		CLOGD("test case: %s", item.name);
    	item.function();
    }

    while (1);

    return 0;
}
