/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BIT
#if defined(_ASMLANGUAGE)
#define BIT(n) (1 << (n))
#else
/**
 * @brief Unsigned integer with bit position @p n set (signed in
 * assembly language).
 */
#define BIT(n) (1UL << (n))
#endif
#endif

#ifdef __cplusplus
}
#endif