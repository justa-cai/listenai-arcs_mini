/****************************************************************************************
 *
 * @file vrtc.c
 *
 *
 * Copyright (C) ListenAI 2023
 *
 * Created on: Jan 11, 2024
 *
 *
 ****************************************************************************************
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "Driver_AON_TIMER.h"
#include "ClockManager.h"
#include "arcs_ap.h"
#include "rtos_al.h"
#include "vrtc.h"
#include "amp_shared.h"
#ifdef CFG_AMP_IPC
#include "ipc.h"
#include "ipc_utils.h"
#include "ic_spinlock.h"
#endif
#include "PowerManager.h"
#include "pm.h"
#include "log_print.h"

#define VRTC_TIMER_VALUE_MASK          (0xFFFFFF)
#define VRTC_TIMEOUT                   (20) /*s*/
#define VRTC_CALI_PERIOD               2//(VRTC_TIMEOUT - 2) /*s*/
#define VRTC_DEFAULT_VAL               (VRTC_TIMEOUT * 32768)
#define VRTC_FREQ_SHIFT                10
#define VRTC_CLOCK_DRIFT               0 /*drfit(us) per second*/

#define VRTC_MIN_SLEEP_TICK           (2000/32)

#ifndef CFG_AMP_IPC

#define VRTC_SPIN_LOCK(lock)
#define VRTC_SPIN_UNLOCK(lock)
#define VRTC_SPIN_LOCK_IRQSAVE(lock)
#define VRTC_SPIN_UNLOCK_IRQSAVE(lock)

#else

#define VRTC_SPIN_LOCK(lock)                     ic_spin_lock(lock)
#define VRTC_SPIN_UNLOCK(lock)                   ic_spin_unlock(lock)
#define VRTC_SPIN_LOCK_IRQSAVE(lock)             ic_spin_lock_irqsave(lock)
#define VRTC_SPIN_UNLOCK_IRQSAVE(lock)           ic_spin_unlock_irqsave(lock)

#endif

#define VRTC_DEBUG  0

#if VRTC_DEBUG
#define vrtc_dbg(fmt, ...)    logDbg(fmt, ##__VA_ARGS__)
#define vrtc_err(fmt, ...)    logDbg(fmt, ##__VA_ARGS__)
#else
#define vrtc_dbg(fmt, ...)
#define vrtc_err(fmt, ...)    logDbg(fmt, ##__VA_ARGS__)
#endif

#define VRTC_START_CALI() do {\
                            IP_AON_CTRL->REG_BT_RC_CALI.bit.RCCAL_START = 0x1;\
                          } while(0)

/* Builds that never touch lisa_pm (no CFG_VRTC/CFG_VRTC_PROXY defined at
 * all) previously fell back to owning the hardware directly; preserve
 * that default now that CFG_VRTC is an explicit role macro. */
#ifndef CFG_VRTC
#if defined(CFG_VRTC_PROXY) && CFG_VRTC_PROXY
#define CFG_VRTC 0
#else
#define CFG_VRTC 1
#endif
#endif

#if CFG_VRTC

struct vrtc_timer
{
    int16_t id;
    int16_t next;
    uint32_t sec;
    uint32_t usec;
    void (*handler) (void);
};

struct vrtc_reg_info reg_info;

static void *time_handle = NULL;
static struct vrtc_timer vrtc_timer_pool[VRTC_TIMER_IDX_MAX];
static struct vrtc_timer *vrtc_current_timer = NULL;
static _PM_STARTUP_BSS volatile uint32_t vrtc_flags;
#endif
volatile struct vrtc_reg_info *vrtc_reg;
static bool s_vrtc_initialized;


static void vrtc_update_time(uint32_t value, int32_t compensation)
{
    vrtc_reg->sec  += value / vrtc_reg->freq;
    vrtc_reg->usec += (((value % vrtc_reg->freq) * vrtc_reg->freq_fact) >> VRTC_FREQ_SHIFT) + compensation;

    if (vrtc_reg->usec >= 1000000)
    {
        vrtc_reg->sec  += vrtc_reg->usec / 1000000;
        vrtc_reg->usec %= 1000000;
    }
}

_PM_TEXT_TEXT int32_t vrtc_get_time(int32_t origin, uint32_t *sec, uint32_t *usec)
{
    uint32_t val;
#if VRTC_DEBUG
    uint32_t raw, wrap = 0;
#endif
    (void)origin;
    VRTC_SPIN_LOCK_IRQSAVE(IC_SPIN_LOCK_TYPE_VRTC);
    val = IP_AON_TIMER->REG_OSTIMER_CURVAL.all;
#if VRTC_DEBUG
    raw = val;
#endif

    if (!(IP_AON_TIMER->REG_OS_TIMER_IRQ_CAUSE.bit.OSTIMER_STATUS))
    {
        val = vrtc_reg->period - val;
    }
    else /*wrap*/
    {
        val = vrtc_reg->period + (vrtc_reg->period - IP_AON_TIMER->REG_OSTIMER_CURVAL.all);
        #if VRTC_DEBUG
        wrap = 1;
        #endif
    }

    *sec  = vrtc_reg->sec + val / vrtc_reg->freq;
    *usec = vrtc_reg->usec + (((val % vrtc_reg->freq) * vrtc_reg->freq_fact) >> VRTC_FREQ_SHIFT) + (VRTC_CLOCK_DRIFT * val / vrtc_reg->freq);
    VRTC_SPIN_UNLOCK_IRQSAVE(IC_SPIN_LOCK_TYPE_VRTC);

    if (*usec >= 1000000)
    {
        *sec  += *usec / 1000000;
        *usec %= 1000000;
    }
#if VRTC_DEBUG
    if (wrap)
        vrtc_dbg("Get: wrap ");
    else
        vrtc_dbg("Get: ok ");
    vrtc_dbg("val %d(%d) time %d.%06d period %d f %d\n", val, raw, *sec, *usec, vrtc_reg->period, vrtc_reg->freq);
#endif
    return 0;
}

_PM_TEXT_TEXT uint64_t vrtc_get_time_us(void)
{
    uint32_t sec, usec;
    uint64_t time;

    vrtc_get_time(0, &sec, &usec);
    time = ((uint64_t)sec) * 1000000 + usec;

    return time;
}

uint32_t vrtc_get_freq(void)
{
    return vrtc_reg->freq;
}

#if VRTC_DEBUG
static volatile uint32_t start_time, raw_time, timer_begin;
#endif

#if CFG_VRTC
int32_t vrtc_is_allow_sleep(uint16_t auto_mode)
{
    if (!IRQ_enabled(IRQ_RCCAL_DONE_VECTOR))
        return 1;
    /*A short wake-up period may prevent RC calibration from completing.
    * If the system is interrupted while transitioning to sleep, it can cause the hardware state machine
    * to enter an inconsistent state.
    */
    if (vrtc_flags & VRTC_FLAGS_WAITING_RCCALI_DONE)
        return 0;

    if (auto_mode)
    {
        uint32_t val = IP_AON_TIMER->REG_OSTIMER_CURVAL.all;

        if ((val < VRTC_MIN_SLEEP_TICK) || (IP_AON_TIMER->REG_OS_TIMER_IRQ_CAUSE.bit.OSTIMER_STATUS))
            return 0;
    }

    return 1;
}

void vrtc_set_flag(uint32_t flag_bit)
{
    vrtc_flags |= flag_bit;
}

static void vrtc_rccali_irq_handle(void)
{
    uint32_t freq;

    IP_AON_CTRL->REG_BT_RC_CALI_IRQ.bit.RCCAL_DONE_CLR = 1;
    freq = (IC_BOARD_XTAL_FREQ << IP_AON_CTRL->REG_BT_RC_CALI.bit.RCCAL_LENGTH) / IP_AON_CTRL->REG_BT_RC_CALI.bit.RCCAL_RESULT;
    vrtc_reg->freq = (vrtc_reg->freq + freq) >> 1;
    vrtc_reg->freq_fact = (1000000 << VRTC_FREQ_SHIFT) / vrtc_reg->freq;
    freq = (IC_BOARD_XTAL_FREQ / 1000000);
    IP_AON_CTRL->REG_AON_WF_CTRL2.bit.CFG_SW_RCCAL_VALUE_US = (IP_AON_CTRL->REG_BT_RC_CALI.bit.RCCAL_RESULT << (20 - IP_AON_CTRL->REG_BT_RC_CALI.bit.RCCAL_LENGTH)) / freq;
    vrtc_flags &= ~VRTC_FLAGS_WAITING_RCCALI_DONE;
}

static void vrtc_isr(uint32_t event, void* workspace)
{
#if VRTC_DEBUG
    uint64_t time1;
    uint32_t time_h, time_l, data;
    uint32_t val = IP_AON_TIMER->REG_OSTIMER_CURVAL.all;
    volatile uint32_t time_now = rtos_get_sys_us();
    uint32_t n, t;
#endif

    //VRTC_START_CALI();
    VRTC_SPIN_LOCK(IC_SPIN_LOCK_TYPE_VRTC);
    vrtc_update_time(vrtc_reg->period, (VRTC_CLOCK_DRIFT * vrtc_reg->period / vrtc_reg->freq));
#if VRTC_DEBUG
#if 0
    if (start_time == 0)
    {
        start_time = time_now;
        vrtc_reg->sec = 0;
        vrtc_reg->usec = 0;
    }
    else
    {
        raw_time = vrtc_reg->sec*1000000 + vrtc_reg->usec - (VRTC_CLOCK_DRIFT * vrtc_reg->period / vrtc_reg->freq);
    }

    time_now -= start_time;
#else
    raw_time = vrtc_reg->sec*1000000 + vrtc_reg->usec - (VRTC_CLOCK_DRIFT * vrtc_reg->period / vrtc_reg->freq);
#endif
#endif

    if (vrtc_current_timer != NULL)
    {
        if (vrtc_current_timer->next != -1)
        {
            uint32_t period;
            uint64_t usec1, usec2;
            struct vrtc_timer *timer;

            timer = &vrtc_timer_pool[vrtc_current_timer->next];
            usec1 = (uint64_t)vrtc_reg->sec * 1000000 + vrtc_reg->usec;
            usec2 = (uint64_t)timer->sec * 1000000 + timer->usec;
            if (usec2 > (usec1 + 100))
            {
                period = (uint32_t)(usec2 - usec1);
                period = ((uint64_t)(vrtc_reg->freq) * period) / 1000000;
                AON_TIMER_SetTimerPeriodByCount(time_handle, period - 1);
                vrtc_reg->period = period;

                if (vrtc_current_timer->handler)
                    vrtc_current_timer->handler();

                vrtc_dbg("%s cb next perid %d\n", __func__, period);
            }
            else
            {
                AON_TIMER_SetTimerPeriodByCount(time_handle, VRTC_DEFAULT_VAL);

                if (vrtc_current_timer->handler)
                    vrtc_current_timer->handler();
                if (timer->handler)
                    timer->handler();
                vrtc_dbg("%s overflow %d %d.%06d\n", __func__, vrtc_current_timer->next, timer->sec, timer->usec);

                goto PERM_TIMER;
            }
            vrtc_current_timer = timer;
            VRTC_SPIN_UNLOCK(IC_SPIN_LOCK_TYPE_VRTC);
            return;
        }
        else
        {
            AON_TIMER_SetTimerPeriodByCount(time_handle, VRTC_DEFAULT_VAL);

            if (vrtc_current_timer && vrtc_current_timer->handler)
                vrtc_current_timer->handler();
            vrtc_dbg("%s last timer\n", __func__);
        }
    }

PERM_TIMER:
#if VRTC_DEBUG
    data = vrtc_reg->period;
#endif
    vrtc_reg->period   = VRTC_DEFAULT_VAL;
    vrtc_current_timer = NULL;
#if VRTC_DEBUG
    n = IP_AON_TIMER->REG_OSTIMER_CURVAL.all;
    t = IP_AON_TIMER->REG_OSTIMER_CTRL.all;
#endif
    VRTC_SPIN_UNLOCK(IC_SPIN_LOCK_TYPE_VRTC);

#if VRTC_DEBUG
    vrtc_dbg("\nISR: val %x period %d def %d reg %d 0x%x\n", val, data, VRTC_DEFAULT_VAL, n, t);

    time1 = (uint64_t)vrtc_reg->sec * 1000000 + vrtc_reg->usec;

    time_l = time1;
    time_h = 0;
    vrtc_dbg("     vrtc: %d-%d ", time_h, time_l);
    time_l = time_now;
    time_h = 0;
    vrtc_dbg("mtime: %d-%d ", time_h, time_l);
    vrtc_dbg("gap: %d ", time_now - timer_begin);

    time_l = time_now - time1;
    vrtc_dbg("delta: %d %d\n\n", time_l, (time_now - raw_time));
#endif
}
#if defined(CFG_AMP_IPC)
static void vrtc_isr_ipc_cb(void)
{
    ipc_send_signal(IPC_SIG_VRTC_ALERT);
    vrtc_dbg("ISR: ipc\n");
}
#endif

int32_t vrtc_set_timer(int32_t timer_idx, uint32_t duration_us, void (*handler) (void))
{
    uint32_t tick = 0, sec, usec;
    struct vrtc_timer *next_timer;

    if ((timer_idx < 0) || (timer_idx >= VRTC_TIMER_IDX_MAX))
        return -1;
#if VRTC_DEBUG
    timer_begin = rtos_get_sys_us() - start_time;
#endif

    if ((duration_us > 100) && (timer_idx < VRTC_TIMER_IDX_MAX))
    {
        VRTC_SPIN_LOCK_IRQSAVE(IC_SPIN_LOCK_TYPE_VRTC);
        tick = IP_AON_TIMER->REG_OSTIMER_CURVAL.all;
        if (!(IP_AON_TIMER->REG_OS_TIMER_IRQ_CAUSE.bit.OSTIMER_STATUS))
        {
            tick = vrtc_reg->period - tick;
        }
        else
        {
            tick = vrtc_reg->period + (vrtc_reg->period - IP_AON_TIMER->REG_OSTIMER_CURVAL.all);
        }
        sec  = vrtc_reg->sec + tick / vrtc_reg->freq + duration_us / 1000000;
        usec = vrtc_reg->usec + (((tick % vrtc_reg->freq) * vrtc_reg->freq_fact) >> VRTC_FREQ_SHIFT) + (VRTC_CLOCK_DRIFT * tick / vrtc_reg->freq) + duration_us % 1000000;

        if (usec >= 1000000)
        {
            sec  += usec / 1000000;
            usec %= 1000000;
        }
        next_timer = &vrtc_timer_pool[timer_idx];
        next_timer->sec     = sec;
        next_timer->usec    = usec;
        next_timer->handler = handler;

        if ((vrtc_current_timer) && ((vrtc_current_timer->id != timer_idx) || (vrtc_current_timer->next != -1)))
        {
            struct vrtc_timer *timer;

            timer = &vrtc_timer_pool[VRTC_TIMER_IDX_LOCAL + VRTC_TIMER_IDX_IPC - timer_idx];
            if ((timer->sec < sec) || ((timer->sec == sec) && (timer->usec < usec)))
                next_timer = timer;
        }

        if ((vrtc_current_timer != next_timer) || (vrtc_current_timer->id == timer_idx))
        {
            uint64_t usec1, usec2;

            tick = IP_AON_TIMER->REG_OSTIMER_CURVAL.all;
            if (!(IP_AON_TIMER->REG_OS_TIMER_IRQ_CAUSE.bit.OSTIMER_STATUS))
            {
                vrtc_update_time(vrtc_reg->period - tick, (VRTC_CLOCK_DRIFT * vrtc_reg->period / vrtc_reg->freq));
            }
            else
            {
                IP_AON_TIMER->REG_OS_TIMER_IRQ_CLR.all = 0x1;
                vrtc_update_time(vrtc_reg->period + (vrtc_reg->period - IP_AON_TIMER->REG_OSTIMER_CURVAL.all), (VRTC_CLOCK_DRIFT * (vrtc_reg->period - tick) / vrtc_reg->freq));
            }
            usec1 = (uint64_t)vrtc_reg->sec * 1000000 + vrtc_reg->usec;
            usec2 = (uint64_t)next_timer->sec * 1000000 + next_timer->usec;
            if (usec2 >= usec1)
            {
                tick = (uint32_t)(usec2 - usec1);
                tick = ((uint64_t)(vrtc_reg->freq) * tick) / 1000000;
                AON_TIMER_SetTimerPeriodByCount(time_handle, (uint32_t)tick - 1);
                vrtc_reg->period = (uint32_t)tick;
                if (vrtc_current_timer && vrtc_current_timer != next_timer)
                {
                    next_timer->next = vrtc_current_timer->id;
                    vrtc_current_timer->next = -1;
                    vrtc_current_timer = next_timer;
                }
                else if (vrtc_current_timer == NULL)
                {
                    next_timer->next   = -1;
                    vrtc_current_timer = next_timer;
                }
                vrtc_dbg("Set:Active\n");
            }
            else
            {
                vrtc_err("Set VRTC: invalid timer %d.%06d cur:%d.%06d\n", next_timer->sec, next_timer->usec, vrtc_reg->sec, vrtc_reg->usec);
            }
        }
        else
        {
            vrtc_current_timer->next = timer_idx;
            vrtc_timer_pool[timer_idx].next = -1;
        }
        VRTC_SPIN_UNLOCK_IRQSAVE(IC_SPIN_LOCK_TYPE_VRTC);
#if VRTC_DEBUG
        if (next_timer == &vrtc_timer_pool[timer_idx])
            vrtc_err("Set: head\n");
        else
            vrtc_err("Set: tail\n");
#endif
    }
    else
    {
        if (handler)
            handler();
    }
    vrtc_dbg("Set: id %d tick %d dur %d mtime %d rtc %d.%06d\n", timer_idx, (uint32_t)tick, duration_us, timer_begin, vrtc_reg->sec, vrtc_reg->usec);

    return 0;
}
#if defined(CFG_AMP_IPC)
int32_t vrtc_set_timer_from_ipc(void)
{
    vrtc_set_timer(VRTC_TIMER_IDX_IPC, vrtc_reg->timeout_req, vrtc_isr_ipc_cb);

    return 0;
}
#endif
#else
int32_t vrtc_set_timer(int32_t timer_idx, uint32_t duration_us, void (*handler) (void))
{
    vrtc_reg->timeout_req = duration_us - 32;
    ipc_send_signal(IPC_SIG_VRTC_SET);

    return 0;
}
#endif

int32_t vrtc_init(void)
{
    if (s_vrtc_initialized)
        return 0;

#if CFG_VRTC
    int32_t i;
    volatile struct vrtc_timer *timer;
#endif
#if !defined(CFG_AMP_IPC)
    memset(&reg_info, 0, sizeof(struct vrtc_reg_info));
    vrtc_reg = &reg_info;
#else
    vrtc_reg = &(amp_shared_get()->vrtc_reg);
#endif

#if CFG_VRTC
    for (i = 0; i < VRTC_TIMER_IDX_MAX; i++)
    {
        timer = &vrtc_timer_pool[i];
        timer->id    = i;
        timer->next  = -1;
        timer->sec   = 0;
        timer->usec  = 0;
        timer->handler = NULL;
    }

    HAL_CRM_SetRc32kCaliStart();
    vrtc_reg->freq = CRM_GetSrcFreq(CRM_IpSrcAon32kClk);
    vrtc_reg->freq_fact   = (1000000 << VRTC_FREQ_SHIFT) / vrtc_reg->freq;
    vrtc_reg->timeout_req = 0;
    vrtc_reg->sec    = 0;
    vrtc_reg->usec   = 0;
    vrtc_reg->period = VRTC_DEFAULT_VAL;
    vrtc_flags = 0;

    vrtc_dbg("vrtc_init: freq %d fact %d\n", vrtc_reg->freq, vrtc_reg->freq_fact);

    time_handle = AON_TIMER();
    AON_TIMER_Initialize(time_handle, vrtc_isr, NULL);
    AON_TIMER_PowerControl(time_handle, CSK_POWER_FULL);
    AON_TIMER_Control(time_handle, HAL_AON_TIMER_MODE_Repeat | HAL_AON_TIMER_INTERRUPT_Enabled | HAL_AON_TIMER_CLK_SEL_Rc32k);
    AON_TIMER_SetTimerPeriodByCount(time_handle, VRTC_DEFAULT_VAL);
    AON_TIMER_StartTimer(time_handle);

    IP_AON_CTRL->REG_AON_WF_CTRL2.bit.CFG_SW_RCCAL_VALUE_US = (IP_AON_CTRL->REG_BT_RC_CALI.bit.RCCAL_RESULT << (20 - IP_AON_CTRL->REG_BT_RC_CALI.bit.RCCAL_LENGTH)) / (IC_BOARD_XTAL_FREQ / 1000000);
    IP_AON_CTRL->REG_BT_RC_CALI_IRQ.bit.RCCAL_DONE_CLR = 1;
    register_ISR(IRQ_RCCAL_DONE_VECTOR, vrtc_rccali_irq_handle, NULL);
    enable_IRQ(IRQ_RCCAL_DONE_VECTOR);
    IP_AON_CTRL->REG_BT_RC_CALI_IRQ.bit.RCCAL_DONE_MASK = 0x1;
#endif

    s_vrtc_initialized = true;
    return 0;
}

