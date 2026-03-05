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



/*
 * Halt the other core
 */
#define IPC_APP_STATUS_HALT_PEER_BITS_ACK        0x00000001
#define IPC_APP_STATUS_HALT_PEER_BITS_RESUME     0x00000002
#define IPC_APP_STATUS_HALT_PEER_BITS_ALL        0x00000003

#define IPC_APP_STATUS_VRTC_ALERT                0x00000004

enum {
    IPC_CORE_STATE_ACTIVE,
    IPC_CORE_STATE_SLEEP,
    IPC_CORE_STATE_WAKEUP,
    IPC_CORE_STATE_MAX
};


struct vrtc_reg_info
{
    uint32_t sec;
    uint32_t usec;
    uint32_t freq;
    uint32_t freq_fact;
    uint32_t period;
    uint32_t timeout_req;
};

struct pm_master_state
{
    uint32_t state;
    uint64_t sleep_time;
    uint64_t wakeup_time;
};

struct pm_slave_state
{
    uint32_t state;
};

struct amp_shared_info
{
    volatile uint32_t app_status;
    volatile struct vrtc_reg_info vtc_reg;
    volatile struct pm_master_state master_state;
    volatile struct pm_slave_state slave_state;
    volatile uint32_t wakeup_cause;
};





#endif
