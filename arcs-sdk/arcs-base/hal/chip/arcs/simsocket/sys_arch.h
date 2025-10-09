/**
 ****************************************************************************************
 *
 * @file sys_arch.h
 *
 * @brief Implementation of simulate socket
 *
 * Copyright (C) ListenAI 2020-2023
 *
 * Created on: Nov 11, 2023
 *
 *
 ****************************************************************************************
 */
#ifndef _SYS_ARCH_H_
#define _SYS_ARCH_H_

#include "log_print.h"

#define SYS_ARCH_TIMEOUT 0xffffffffUL

#define set_errno(err)

#define SIMS_DEBUG(...)  CLOGD(__VA_ARGS__)
#define SIMS_ERR(...)    CLOGE(__VA_ARGS__)

#define SIMS_ASSERT(msg, assertion) do {\
                    if (!(assertion)) { \
                        SIMS_DEBUG(msg);} \
                    } while(0)

#define sim_sock_protect(x)       sys_arch_protect()
#define sim_sock_unprotect(x)     sys_arch_unprotect()

int32_t sys_sem_new(rtos_semaphore *pxSemaphore, uint8_t ucCount);
uint32_t sys_arch_sem_wait(rtos_semaphore *pxSemaphore, uint32_t ulTimeOut);
void sys_sem_free(rtos_semaphore *pxSemaphore);
void sys_sem_signal(rtos_semaphore *pxSemaphore);
void sys_arch_unprotect(void);
uint32_t sys_arch_protect(void);
void sys_init(void);
#endif
