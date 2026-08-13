/*
 * gpt_timer.c
 *
 *  Created on: 2025年4月25日
 *      Author: USER
 */

#include "gpt.h"
#include "gpt_timer.h"

// Global variable for storing GPT timer information
GPT_TIMER_Info_t __timer_info = {
    .workspace = {NULL},
};

static inline void GPT_ConfigureChannelMode(volatile uint32_t* cnt_ctrl_reg,
											uint32_t cnt_mode_pos,
											uint32_t run_mode_pos,
											uint32_t cnt_mode,
											uint32_t run_mode) {
	// Clear existing count mode bits
	*cnt_ctrl_reg &= ~(GPT_CH_CNT_CTRL_CH0_TIMER0_CNT_MODE_Msk << cnt_mode_pos);
	// Set new count mode
	*cnt_ctrl_reg |= ((cnt_mode & 0x3U) << (cnt_mode_pos + GPT_CH_CNT_CTRL_CH0_TIMER0_CNT_MODE_Pos));

	// Clear existing run mode bits
	*cnt_ctrl_reg &= ~(GPT_CH_CNT_CTRL_CH0_TIMER0_RUN_MODE_Msk << run_mode_pos);
	// Set new run mode
	*cnt_ctrl_reg |= ((run_mode & 0x3U) << (run_mode_pos + GPT_CH_CNT_CTRL_CH0_TIMER0_RUN_MODE_Pos));
}

int32_t HAL_GPT_TimerControl(void* res, GPT_Channel_Num_t channel, GPT_TIMER_Config_Para_t* para) {
	CHECK_RESOURCES(res);

    GPT_Resources_t* gpt = (GPT_Resources_t *)res;
    uint8_t timer_index_flags = para->index;

    // Step 1: Disable the target GPT channel before configuration
    HAL_GPT_DisableChannel(res, channel);

    // Step 2: Register the channel usage as either 16-bit or 8-bit timer
    uint8_t is_16bit = ((timer_index_flags & HAL_GPT_TIMER_BITS_DIFF) == HAL_GPT_TIMER_16BITS_INDEX_0);
    HAL_GPT_RegisterChannel(res, channel,
    		is_16bit ? HARDWARE_CHANNEL_STAT_USED_BY_TIM16 : HARDWARE_CHANNEL_STAT_USED_BY_TIM8);

    // Step 3: Configure count mode and run mode
    volatile uint32_t* cnt_ctrl_reg = &gpt->reg->REG_CH_CNT_CTRL.all;
    volatile uint32_t base_bit_pos = channel * 4;

    if(is_16bit) {
    	GPT_ConfigureChannelMode(cnt_ctrl_reg, base_bit_pos, base_bit_pos, para->cnt_mode, para->run_mode);
    } else {
    	uint8_t index_mask = (timer_index_flags & HAL_GPT_TIMER_8BITS_MASK) >> HAL_GPT_TIMER_8BITS_POS;
		for (uint8_t idx = 0; index_mask; ++idx, index_mask >>= 1) {
			if (index_mask & 0x1U) {
				uint32_t offset_pos = base_bit_pos + 2 * idx;
				GPT_ConfigureChannelMode(cnt_ctrl_reg, offset_pos, offset_pos, para->cnt_mode, para->run_mode);
			}
		}
    }
    return CSK_DRIVER_OK;
}

int32_t HAL_GPT_ReadTimerCount(void *res, GPT_Channel_Num_t channel, uint8_t ch_mode, uint16_t *count) {
    CHECK_RESOURCES(res);

    GPT_Resources_t* gpt = (GPT_Resources_t*)res;

    // Pointer to the current timer count register for the specified channel
    volatile uint32_t* gpt_timer_value = &gpt->reg->REG_CH0_RELOAD.all + channel;

    if ((ch_mode & HAL_GPT_TIMER_BITS_DIFF) == HAL_GPT_TIMER_16BITS_INDEX_0){
        *count = (uint16_t)(*gpt_timer_value & 0xFFFF);
    } 
    
    else if ((ch_mode & HAL_GPT_TIMER_8BITS_MASK) == (HAL_GPT_TIMER_8BITS_INDEX_0 & HAL_GPT_TIMER_8BITS_MASK)){
        *count = (uint16_t)(*gpt_timer_value & 0xFF);
    } 
    
    else if ((ch_mode & HAL_GPT_TIMER_8BITS_MASK) == (HAL_GPT_TIMER_8BITS_INDEX_1 & HAL_GPT_TIMER_8BITS_MASK)){
        *count = (uint16_t)(((*gpt_timer_value) >> 8) & 0xFF);
    }
    
    return CSK_DRIVER_OK;
}

int32_t HAL_GPT_RegisterTimerCallback(void* res, GPT_Channel_Num_t channel, CSK_GPT_SignalEvent_t cb_event, void* workspace) {
    CHECK_RESOURCES(res);

    GPT_Resources_t* gpt = (GPT_Resources_t*)res;

    //Assign the callback event function to the specified channel
    gpt->info->cb_event[channel] = cb_event;
    gpt->timer_info->workspace[channel] = workspace;

    return CSK_DRIVER_OK;
}

int32_t HAL_GPT_SetTimerPeriodByCount(void* res, GPT_Channel_Num_t channel, uint8_t ch_mode, uint16_t count) {
    CHECK_RESOURCES(res);

    GPT_Resources_t* gpt = (GPT_Resources_t*)res;

    // Set the reload value for the timer based on the count and index
    volatile uint32_t* gpt_timer_reload = &gpt->reg->REG_CH0_RELOAD.all + channel;

    if ((ch_mode & HAL_GPT_TIMER_BITS_DIFF) == HAL_GPT_TIMER_16BITS_INDEX_0){
        *gpt_timer_reload = count;
    } 
    
    else if ((ch_mode & HAL_GPT_TIMER_8BITS_MASK) == (HAL_GPT_TIMER_8BITS_INDEX_0 & HAL_GPT_TIMER_8BITS_MASK)){
        *gpt_timer_reload &= 0xFF00;

        *gpt_timer_reload |= (count & 0xFF);
    } 
    
    else if ((ch_mode & HAL_GPT_TIMER_8BITS_MASK) == (HAL_GPT_TIMER_8BITS_INDEX_1 & HAL_GPT_TIMER_8BITS_MASK)){
        *gpt_timer_reload &= 0x00FF;

        *gpt_timer_reload |= ((count << 8) & 0xFF00);
    }

    // Activate shadow load to apply the new reload value
    gpt->reg->REG_SHADOW_LOAD.bit.SHADOW_LOAD_ACTIVE |= 0x1 << (channel);

    while(gpt->reg->REG_SHADOW_LOAD.bit.SHADOW_LOAD_ACTIVE & (0x1 << channel));

    return CSK_DRIVER_OK;
}

int32_t HAL_GPT_StartTimer(void* res, GPT_Channel_Num_t channel, uint8_t ch_mode) {
    CHECK_RESOURCES(res);

    GPT_Resources_t* gpt = (GPT_Resources_t*)res;
    uint8_t index = 0;
    uint32_t cnt_ctrl_data = 0;

    // Determine the interrupt mask offset based on the timer index
    uint32_t channel_timer_int_imr_offset = GPT_IMR_ISR_GPT_TIMER0_INT_MASK_Pos;

    // Enable the specified GPT channel
    HAL_GPT_EnableChannel(res, channel);

    if ((ch_mode & HAL_GPT_TIMER_BITS_DIFF) == HAL_GPT_TIMER_16BITS_INDEX_0){
        // Clear the interrupt mask to enable interrupts for the specified channel
        gpt->reg->REG_IMR_ISR_GPT.all &= ~((0x1 << channel) << channel_timer_int_imr_offset);

        cnt_ctrl_data = (0x1 << channel) << GPT_CH_CNT_CTRL_TIMER0_EN_Pos;

    } else {
    	ch_mode = (ch_mode & HAL_GPT_TIMER_8BITS_MASK) >> HAL_GPT_TIMER_8BITS_POS;

        while(ch_mode){
            channel_timer_int_imr_offset = GPT_IMR_ISR_GPT_TIMER0_INT_MASK_Pos + index * 2;

            // Clear the interrupt mask to enable interrupts for the specified channel
            gpt->reg->REG_IMR_ISR_GPT.all &= ~((0x1 << channel) << channel_timer_int_imr_offset);

            cnt_ctrl_data |= (0x1 << channel) << (GPT_CH_CNT_CTRL_TIMER0_EN_Pos + index * 2);

            index++;
            ch_mode >>= 1;
        }
    }

    gpt->reg->REG_CH_CNT_CTRL.all |= cnt_ctrl_data;

    return CSK_DRIVER_OK;
}
