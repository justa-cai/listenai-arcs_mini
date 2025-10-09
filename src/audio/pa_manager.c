#define TAG "PA_MGR"

#include "systick.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "evs_utils.h"
#include "lisa_mutex.h"
#include "pa_manager.h"
#include "lisa_timer.h"
#include "Driver_GPIO.h"
#include "IOMuxManager.h"
#include "lisa_typedef.h"


typedef struct pa_manager_s {
	PA_MGR_STATE 	m_state;
	uint32_t 		m_timer_period;
	lisa_timer_t 	*m_change_timer;
	lisa_mutex_t 	*m_lock;
} pa_manager_t;

static pa_manager_t *s_pa_hdl = NULL;

#define PA_CONTROL_IO_PAD 	CSK_IOMUX_PAD_A
#define PA_CONTROL_IO_NUM  	1
#define PA_CONTROL_IO_POS  	CSK_GPIO_PIN1
#define PA_OUT_ON         	(1)
#define PA_OUT_OFF         	(0)

static void *PA_DRV_HANDLE = NULL;

void pa_manager_pre_init()
{
	LISA_LOGI(TAG, "pa manager pre init");
	PA_DRV_HANDLE = (PA_CONTROL_IO_PAD == CSK_IOMUX_PAD_A) ? GPIOA() : GPIOB();	

	IOMuxManager_PinConfigure(PA_CONTROL_IO_PAD, PA_CONTROL_IO_NUM, CSK_IOMUX_FUNC_ALTER1);

	GPIO_Control(PA_DRV_HANDLE, CSK_GPIO_DEBOUNCE_DISABLE, PA_CONTROL_IO_POS);
	GPIO_SetDir(PA_DRV_HANDLE, PA_CONTROL_IO_POS, CSK_GPIO_DIR_OUTPUT);

	GPIO_PinWrite(PA_DRV_HANDLE, PA_CONTROL_IO_POS, PA_OUT_OFF);
}

int pa_manager_onoff(int onoff)
{
	if (PA_DRV_HANDLE != NULL) {
		if (onoff) {
			LISA_LOGI(TAG, "PA ON");
			for(volatile int i = 0; i < 7; i++) {
				GPIO_PinWrite(PA_DRV_HANDLE, PA_CONTROL_IO_POS, PA_OUT_OFF);
				SysTick_Delay_Us(50);
				GPIO_PinWrite(PA_DRV_HANDLE, PA_CONTROL_IO_POS, PA_OUT_ON);
				SysTick_Delay_Us(50);
			}
		} else {
			LISA_LOGI(TAG, "PA OFF");
			GPIO_PinWrite(PA_DRV_HANDLE, PA_CONTROL_IO_POS, PA_OUT_OFF);
			SysTick_Delay_Us(50);
		}
		return 0;
	}
	return -1;
}

/**
 * @brief  Switch PA State
 * @param  next_state		Next PA State
 */
static void __pa_switch_state(PA_MGR_STATE next_state)
{
	if (s_pa_hdl) {
		lisa_mutex_lock(s_pa_hdl->m_lock, LISA_OS_WAIT_FOREVER);

		switch (next_state) {
			case PA_MGR_OFF: {
				if (s_pa_hdl->m_state != PA_MGR_OFF) {
					// pa off
					if (pa_manager_onoff(0) == 0) {
						// set state
						s_pa_hdl->m_state = PA_MGR_OFF;
					}
				}
			} break;
			case PA_MGR_ON: {
				if (s_pa_hdl->m_state != PA_MGR_ON) {
					// pa on
					if (pa_manager_onoff(1) == 0) {
						// set state
						s_pa_hdl->m_state = PA_MGR_ON;
					}
				}
			} break;
			case PA_MGR_NONE: {
				s_pa_hdl->m_state = PA_MGR_NONE;
			};
		}

		lisa_mutex_unlock(s_pa_hdl->m_lock);
	}
}

static int __handle_pa_off_runnable(void *arg)
{
	pa_manager_t *hdl = (pa_manager_t *)arg;

	LISA_LOGI(TAG, "PA off, by timeout");
	__pa_switch_state(PA_MGR_OFF);
	return 0;
}

/**
 * @brief 	PA OFF Timer Callback
 * @param  	arg		PA Manager Handle
 */
static void __pa_off_timeout(void *arg)
{
	evs_handler_post_runnable(__handle_pa_off_runnable, arg);
}

void pa_manager_init(PA_MGR_STATE init_state)
{
	s_pa_hdl = (pa_manager_t *)lisa_mem_alloc(sizeof(pa_manager_t));
	LISA_ASSERT(s_pa_hdl, "pa handle null");

	s_pa_hdl->m_change_timer = lisa_timer_create(LS_PA_BASE_TIME, __pa_off_timeout, s_pa_hdl);
	LISA_ASSERT(s_pa_hdl->m_change_timer, "pa change timer null");

	s_pa_hdl->m_lock = lisa_mutex_create();
	LISA_ASSERT(s_pa_hdl->m_lock, "pa lock null");

	s_pa_hdl->m_timer_period = LS_PA_BASE_TIME;

	s_pa_hdl->m_state = PA_MGR_NONE;
	if (init_state != PA_MGR_NONE)
	{
		pa_manager_refresh(init_state, LS_PA_BASE_TIME, "pa_mgr_init");
	}
}

void pa_manager_refresh(PA_MGR_STATE next_state, uint32_t duration, const char *const by_which)
{
	if (!s_pa_hdl) return;

	LISA_LOGD(TAG, "Refresh PA to %s, timeout %u, by \"%s\"",
						PA_PRINT_STATE(next_state), duration, by_which);
	if (next_state == PA_MGR_ON) {
		__pa_switch_state(next_state);
	} else if (next_state == PA_MGR_OFF) {
		// delay change PA state to off
		if (duration == 0) {
			__pa_switch_state(next_state);
			// stop change timer
			lisa_timer_stop(s_pa_hdl->m_change_timer);
		}
	}

	if (duration == LS_PA_FOREVER) {
		lisa_timer_stop(s_pa_hdl->m_change_timer);
	} else {
		// launch change to off timer
		if (duration != s_pa_hdl->m_timer_period) {
			lisa_timer_change_period(s_pa_hdl->m_change_timer, duration);
			s_pa_hdl->m_timer_period = duration;
			// Restart Timer
			lisa_timer_reset(s_pa_hdl->m_change_timer);
		} else {
			lisa_timer_stop(s_pa_hdl->m_change_timer);
			lisa_timer_start(s_pa_hdl->m_change_timer);
		}
	}
	LISA_LOGD(TAG, "Refresh PA end by \"%s\"", by_which);
}

void pa_manager_reset_state(const char *const by_which)
{
	LISA_LOGD(TAG, "Reset PA to %s, by \"%s\"", PA_PRINT_STATE(PA_MGR_NONE), by_which);
	__pa_switch_state(PA_MGR_NONE);
}

PA_MGR_STATE pa_manager_get_state()
{
	if (s_pa_hdl) {
		return s_pa_hdl->m_state;
	}
	return PA_MGR_NONE;
}