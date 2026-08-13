/**
 ****************************************************************************************
 *
 * @file amp_shared.h
 *
 * @brief Shared memory in an AMP system
 *
 * Copyright (C) ListenAI 2025
 *
 ****************************************************************************************
 */
#ifndef _AMP_SHARED_H_
#define _AMP_SHARED_H_

#include <stdint.h>

#if defined(CONFIG_PM) && (CONFIG_PM == 1)
#include "pm.h"
#endif

/*
 * Halt the other core
 */
#define AMP_APP_STATUS_HALT_PEER_ACK             0x00000001
#define AMP_APP_STATUS_HALT_PEER_RESUME          0x00000002
#define AMP_APP_STATUS_HALT_PEER_RESUME_ACK      0x00000004
#define AMP_APP_STATUS_HALT_PEER_ALL             0x00000007

#define AMP_APP_STATUS_VRTC_ALERT                0x00000008

#if defined(CFG_AMP_IPC)
#define CONFIG_CORE_NUM      2
#else
#define CONFIG_CORE_NUM      1
#endif

struct vrtc_reg_info {
    uint32_t sec;
    uint32_t usec;
    uint32_t freq;
    uint32_t freq_fact;
    uint32_t period;
    uint32_t timeout_req;
};

#if defined(CONFIG_PM) && (CONFIG_PM == 1)
struct pm_core_context {
    volatile int32_t state;
    volatile uint32_t cross_core_lock;
};
struct pm_shared_data {
    struct pm_core_context core_ctx[CONFIG_CORE_NUM];

    volatile uint32_t last_wakeup_cause;
    pm_config_t config;
    pm_sleep_config_t sleep_cfg;
};
#endif
struct amp_shared_info {
    volatile uint32_t app_status;
    volatile struct vrtc_reg_info vrtc_reg;
#if defined(CONFIG_PM) && (CONFIG_PM == 1)
    struct pm_shared_data pm_data;
#endif
};

void amp_shared_bind(volatile struct amp_shared_info *shared);
volatile struct amp_shared_info *amp_shared_get(void);
void amp_app_status_set(uint32_t bit_mask);
uint32_t amp_app_status_get(uint32_t bit_mask);
void amp_app_status_clear(uint32_t bit_mask);
uint32_t amp_app_status_read(void);
void amp_app_status_write(uint32_t status);

#endif
