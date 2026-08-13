/**
 ****************************************************************************************
 *
 * @file amp_shared.c
 *
 * @brief AMP shared memory access helpers.
 *
 * Copyright (C) ListenAI 2026
 *
 ****************************************************************************************
 */

#include "amp_shared.h"

static volatile struct amp_shared_info *amp_shared_env;

void amp_shared_bind(volatile struct amp_shared_info *shared)
{
    amp_shared_env = shared;
}

volatile struct amp_shared_info *amp_shared_get(void)
{
    return amp_shared_env;
}

void amp_app_status_set(uint32_t bit_mask)
{
    if (amp_shared_env)
        amp_shared_env->app_status |= bit_mask;
}

uint32_t amp_app_status_get(uint32_t bit_mask)
{
    if (amp_shared_env == 0)
        return 0;

    return ((amp_shared_env->app_status & bit_mask) != 0 ? 1 : 0);
}

void amp_app_status_clear(uint32_t bit_mask)
{
    if (amp_shared_env)
        amp_shared_env->app_status &= ~bit_mask;
}

uint32_t amp_app_status_read(void)
{
    if (amp_shared_env == 0)
        return 0;

    return amp_shared_env->app_status;
}

void amp_app_status_write(uint32_t status)
{
    if (amp_shared_env)
        amp_shared_env->app_status = status;
}
