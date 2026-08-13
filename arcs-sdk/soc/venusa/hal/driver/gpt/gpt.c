/**
 * @file gpt.c
 * @brief General Purpose Timer (GPT) Driver Implementation
 *        This file contains the core implementation of GPT peripheral driver including
 *        initialization, interrupt handling, power management and channel control functions.
 *
 * @details
 *   Implements critical GPT operations:
 *     - Interrupt service routine for timer events
 *     - Power state transitions (off/low/full)
 *     - Clock configuration per channel
 *     - Channel registration and enablement
 *     - PWM/Timer mode selection
 *
 * @note
 *   Requires exclusive access to GPT registers during configuration sequences.
 *   Uses shadow load mechanism for atomic register updates.
 *   Supports multiple operating modes (idle/timer8/timer16/pwm).
 *
 * @author USER
 * @date Created on April 24, 2025
 */

#include "gpt.h"
#include "log_print.h"

static void gpt0_irq_handler(void);

static GPT_Info_t gpt0_info = {
	.cb_event = {0},
	.channel_type = {0},
};

GPT_Resources_t gpt0_resources = {
    .reg = IP_GPT,
    .irq_num = IRQ_GPT_VECTOR,
    .irq_handler = gpt0_irq_handler,
    .info = &gpt0_info,
	.pwm_info = &__pwm_info,
    .timer_info = &__timer_info,
};

/**
 * @brief IRQ handler for GPT module
 * @param[in] gpt Pointer to GPT resource structure
 * @details
 *   Handles both TIMER0 and TIMER1 interrupts:
 *     - Processes all active channels in sequence
 *     - Calls registered event callbacks based on channel type
 *     - Clears interrupt status flags after processing
 *   Sequence:
 *     1. Read interrupt status register
 *     2. Process TIMER0 interrupts (channels 0-n)
 *     3. Process TIMER1 interrupts (additional phases)
 */
static void gpt_irq_handler(GPT_Resources_t* gpt){
	uint32_t isr_status = gpt->reg->REG_IMR_ISR_GPT.all & 0xffff0000;

	gpt->reg->REG_IRSR_ICR_GPT.all = isr_status;

	uint8_t channel = 0;
	uint32_t irq_flag = 0;

	// Process TIMER0 interrupts
	channel = 0;
	irq_flag = isr_status & GPT_IMR_ISR_GPT_TIMER0_INT_ISR_Msk;
	irq_flag >>= GPT_IMR_ISR_GPT_TIMER0_INT_ISR_Pos;

	while(irq_flag) {
		if(irq_flag & 0x1) {
			if (gpt->info->channel_type[channel] == HARDWARE_CHANNEL_STAT_USED_BY_TIM16) {
				// 16bits-Timer trigger complete
				if (gpt->info->cb_event[channel]) {
					gpt->info->cb_event[channel](CSK_GPT_EVENT_16BITS_TIMER_COMPLETE, gpt->timer_info->workspace[channel]);
				}
			}
			else if (gpt->info->channel_type[channel] == HARDWARE_CHANNEL_STAT_USED_BY_TIM8) {
				// 8bits-Timer trigger complete
				if (gpt->info->cb_event[channel]) {
					gpt->info->cb_event[channel](CSK_GPT_EVENT_8BITS_TIMER0_COMPLETE, gpt->timer_info->workspace[channel]);
				}
			}
			else {
				// pass
				// TODO
			}
		}
		// Channel increase
		channel++;

		irq_flag = irq_flag >> 1;
	}

	// Process TIMER1 interrupts
	channel = 0;
	irq_flag = isr_status & GPT_IMR_ISR_GPT_TIMER1_INT_ISR_Msk;
	irq_flag >>= GPT_IMR_ISR_GPT_TIMER1_INT_ISR_Pos;

	while (irq_flag){
		if (irq_flag & 0x1){
			// Only 8bits index1 timer complete
			if (gpt->info->cb_event[channel]){
				gpt->info->cb_event[channel](CSK_GPT_EVENT_8BITS_TIMER1_COMPLETE, gpt->timer_info->workspace[channel]);
			}
		}
		// Channel increase
		channel++;

		irq_flag = irq_flag >> 1;
	}
}

/**
 * @brief Get GPT0 resource instance
 * @return Pointer to GPT0 resources structure
 * @note Singleton pattern implementation - always returns same instance
 */
void* GPT0(void) {
    return &gpt0_resources;
}

/**
 * @brief GPT0 specific IRQ handler wrapper
 * @details Forwards to generic handler with GPT0 resources
 */
static void gpt0_irq_handler(void) {
	gpt_irq_handler(GPT0());
}

/**
 * @brief Initialize GPT resources
 * @param[in] res Pointer to GPT resource structure
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details
 *   Resets all channels to idle state:
 *     - Clears all event callbacks
 *     - Sets channel types to IDLE
 *     - Nullifies PWM workspace pointer
 *   Performed during driver initialization phase
 */
int32_t HAL_GPT_Initialize(void *res) {
    CHECK_RESOURCES(res);

    GPT_Resources_t *gpt = (GPT_Resources_t *)res;

    {
    	uint8_t i = 0;
    	for(i = 0; i < GPT_NUMBER_OF_CHANNELS; i++) {
    		gpt->info->cb_event[i] = NULL;
    		gpt->info->channel_type[i] = HARDWARE_CHANNEL_STAT_IDLE;
    }
    }

    {
    	gpt->pwm_info->workspace = NULL;
    }

	return CSK_DRIVER_OK;
}

/**
 * @brief Control GPT power states
 * @param[in] res Pointer to GPT resource structure
 * @param[in] state Target power state (@ref CSK_POWER_STATE)
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details
 *   Power state transitions:
 *     - POWER_OFF: Disable interrupts and unregister handler
 *     - POWER_LOW: Maintain minimal power consumption
 *     - POWER_FULL: Full power operation with clock enabled
 *       Includes hardware reset sequence and interrupt reregistration
 */
int32_t HAL_GPT_PowerControl(void* res, CSK_POWER_STATE state) {
    CHECK_RESOURCES(res);

    GPT_Resources_t *gpt = (GPT_Resources_t *)res;

    switch (state)
    {
    case CSK_POWER_OFF:
        disable_IRQ(gpt->irq_num);
        register_ISR(gpt->irq_num, NULL, NULL);
        break;
    case CSK_POWER_LOW:
        // Minimal power retention
        break;
    case CSK_POWER_FULL:
        // Power up sequence
        IP_SYSCTRL->REG_PERI_CLK_CFG4.bit.ENA_GPT_CLK = 0x1;
        IP_SYSCTRL->REG_SW_RESET_CFG2.bit.GPT_RESET = 0x1;
        gpt->reg->REG_IRSR_ICR_GPT.all |= 0xFFFF0000;
        register_ISR(gpt->irq_num,gpt->irq_handler, NULL);
        enable_IRQ(gpt->irq_num);
        break;
    default:
        break;
    }
    return CSK_DRIVER_OK;
}

/**
 * @brief Configure GPT channel clock parameters
 * @param[in] res Pointer to GPT resource structure
 * @param[in] channel Target channel number
 * @param[in] para Pointer to configuration parameters
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details
 *   Five-step clock configuration sequence:
 *     1. Gate clock (stop updates)
 *     2. Set prescaler value
 *     3. Set clock divider
 *     4. Select clock source
 *     5. ungate clock (resume operation)
 *   Uses double-buffered writes for safe register modification
 */
int32_t HAL_GPT_Control(void* res, GPT_Channel_Num_t channel, GPT_Config_Para_t* para) {
    CHECK_RESOURCES(res);

    GPT_Resources_t* gpt = (GPT_Resources_t*)res;
    volatile uint32_t* ch_clk_ctrl = &gpt->reg->REG_CH0_CLK_CTRL.all + channel;

    uint32_t clk_ctrl = *ch_clk_ctrl;

    // Step 1: Gate the clock (write 1 close clock before changing settings)
    clk_ctrl |= GPT_CH0_CLK_CTRL_CH0_CLK_GATE_Msk;
    *ch_clk_ctrl = clk_ctrl;

    // Step 2: Set pre-divider value
    clk_ctrl &= ~GPT_CH0_CLK_CTRL_CH0_CLK_PREDIV_Msk;
    clk_ctrl |= ((para->prediv) << GPT_CH0_CLK_CTRL_CH0_CLK_PREDIV_Pos);
    *ch_clk_ctrl = clk_ctrl;
    *ch_clk_ctrl |= GPT_CH0_CLK_CTRL_CH0_CLK_PREDIV_LD_Msk;

    // Step 3: Set clock dividers
    clk_ctrl &= ~GPT_CH0_CLK_CTRL_CH0_CLK_DIV_Msk;
    clk_ctrl |= ((para->clk_div) << GPT_CH0_CLK_CTRL_CH0_CLK_DIV_Pos);
    *ch_clk_ctrl = clk_ctrl;
    *ch_clk_ctrl |= GPT_CH0_CLK_CTRL_CH0_CLK_DIV_LD_Msk;

    // Step 4: Set clock source
    clk_ctrl &= ~GPT_CH0_CLK_CTRL_CH0_CLK_SEL_Msk;
    clk_ctrl |= ((para->clk_src) << GPT_CH0_CLK_CTRL_CH0_CLK_SEL_Pos);
    *ch_clk_ctrl = clk_ctrl;

    // Step 5: Ungate the clock (write 0 enable clock to start working)
    clk_ctrl &= ~GPT_CH0_CLK_CTRL_CH0_CLK_GATE_Msk;
    *ch_clk_ctrl = clk_ctrl;

    return CSK_DRIVER_OK;
}

/**
 * @brief Register channel usage type
 * @param[in] res Pointer to GPT resource structure
 * @param[in] channel Target channel number
 * @param[in] type Hardware channel type (@ref Hardware_Channel_Type_t)
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details
 *   Configures channel mode based on usage type:
 *     - IDLE: No specific mode set
 *     - TIM8: 8-bit timer mode (CH_MODE=0x2)
 *     - TIM16: 16-bit timer mode (CH_MODE=0x1)
 *     - PWM: PWM mode (OPERATION=0x3, CH_MODE=0x4)
 *   Also opens clock gate for active channels
 */
int32_t HAL_GPT_RegisterChannel(void* res, GPT_Channel_Num_t channel, Hardware_Channel_Type_t type) {
	CHECK_RESOURCES(res);

    GPT_Resources_t* gpt = (GPT_Resources_t*)res;

	gpt->info->channel_type[channel] = type;

	// Get the channel control register pointer
	volatile uint32_t* ch_clk_ctrl = &gpt->reg->REG_CH0_CTRL.all + channel;
	volatile uint32_t ch_clk_ctrl_data = *ch_clk_ctrl;

	// Clear CH_MODE and OPERATION bits
	ch_clk_ctrl_data &= ~(GPT_CH0_CTRL_CH0_CH_MODE_Msk | GPT_CH0_CTRL_CH0_OPERATION_Msk);

    // Clear CLK_CNT_GATE bit to open clock gate(write 0 enable)
    ch_clk_ctrl_data &= ~(GPT_CH0_CTRL_CH0_CLK_CNT_GATE_Msk);

    // Set CH_MODE and OPERATION according to the usage type
	switch (type){
		case HARDWARE_CHANNEL_STAT_IDLE:
			// No mode set when idle
			break;
		case HARDWARE_CHANNEL_STAT_USED_BY_TIM8:
			// Set CH_MODE to 0x2 for 8-bit timer
			ch_clk_ctrl_data |= (0x2 << GPT_CH0_CTRL_CH0_CH_MODE_Pos);
			break;
		case HARDWARE_CHANNEL_STAT_USED_BY_TIM16:
			// Set CH_MODE to 0x1 for 16-bit timer
			ch_clk_ctrl_data |= (0x1 << GPT_CH0_CTRL_CH0_CH_MODE_Pos);
			break;
		case HARDWARE_CHANNEL_STAT_USED_BY_PWM:
			// Set OPERATION to 0x3 and CH_MODE to 0x4 for PWM
			ch_clk_ctrl_data |= (0x3 << GPT_CH0_CTRL_CH0_OPERATION_Pos) |
								(0x4 << GPT_CH0_CTRL_CH0_CH_MODE_Pos);
			break;
	}

	// Write back the updated control value
	*ch_clk_ctrl = ch_clk_ctrl_data;

	return CSK_DRIVER_OK;
}

/**
 * @brief Disable specified GPT channel
 * @param[in] res Pointer to GPT resource structure
 * @param[in] channel Target channel number
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details
 *   Stops channel operation by setting STOP bit in control register.
 *   Does not modify other channel configurations.
 */
int32_t HAL_GPT_DisableChannel(void* res, GPT_Channel_Num_t channel) {
    CHECK_RESOURCES(res);

    GPT_Resources_t* gpt = (GPT_Resources_t *)res;

    volatile uint32_t* gpt_ch_ctrl = &gpt->reg->REG_CH0_CTRL.all + channel;

    *gpt_ch_ctrl |= 0x1 << GPT_CH0_CTRL_CH0_STOP_Pos;

    return CSK_DRIVER_OK;
}

/**
 * @brief Enable specified GPT channel
 * @param[in] res Pointer to GPT resource structure
 * @param[in] channel Target channel number
 * @return CSK_DRIVER_OK on success, error code otherwise
 * @details
 *   Starts channel operation by:
 *     - Setting START bit in control register
 *     - Asserting CNT_START bit in count control register
 *   Required after successful channel configuration
 */
int32_t HAL_GPT_EnableChannel(void* res, GPT_Channel_Num_t channel) {
	CHECK_RESOURCES(res);

    GPT_Resources_t* gpt = (GPT_Resources_t*)res;

    volatile uint32_t* gpt_ch_ctrl = &gpt->reg->REG_CH0_CTRL.all + channel;

    *gpt_ch_ctrl |= 0x1 << GPT_CH0_CTRL_CH0_START_Pos;

	// Count start
	gpt->reg->REG_CH_CNT_CTRL.bit.CNT_START = 0x1 << channel;

	return CSK_DRIVER_OK;
}
