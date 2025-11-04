/*
 * Inter-cores synchronization primitives for multiprocessor system.
 * Copyright 2024 ListenAI
 */
#ifndef __IC_H__
#define __IC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ic_common.h"

/*!
 * Top level multi-core system initialization routine.
 *
 * This function needs to be called prior to creating all the inter-cores
 * primitives. It initializes the \a inter-cores interrupt handlers.
 *
 * \return     IC_OK if successful, else returns error code
 */
extern int IC_System_initialize(void);

/*!
 * Top level multi-core system syncing routine.
 *
 * This is a barrier call that allows for all cores in the system to
 * synchronize.
 * All cores are synchronized after this call. This function needs to be called
 * prior to using all the inter-cores primitives.
 *
 * \return     IC_OK if successful, else returns error code
 */
extern int IC_System_sync(void);

#ifdef __cplusplus
}
#endif

#endif /* __IC_H__ */
