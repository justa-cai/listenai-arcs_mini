/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * C shim: exposes `dev->api` to Rust without requiring Rust to bind the full
 * lisa_device_t struct layout. Used by sys::gpio / sys::uart vtable dispatch.
 */
#ifndef ARCS_RUST_DEV_H_
#define ARCS_RUST_DEV_H_

#include "lisa_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return `dev->api` (or NULL if dev or its api is NULL).
 *
 * Used by Rust FFI bindings to avoid binding the full lisa_device_t struct.
 */
const void *arcs_rust_dev_get_api(lisa_device_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* ARCS_RUST_DEV_H_ */
