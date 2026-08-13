/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * C shim: forwards a length-prefixed, non-NUL-terminated Rust log message to
 * easylogger's elog_output, avoiding Rust-side variadic FFI and NUL terminators.
 */
#ifndef ARCS_RUST_LOG_H_
#define ARCS_RUST_LOG_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Forward a Rust log message to easylogger.
 *
 * @param level     ELOG_LVL_* (0=ASSERT, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=VERBOSE)
 * @param tag       NUL-terminated tag string (e.g. "rust")
 * @param msg       Pointer to message bytes (need not be NUL-terminated)
 * @param msg_len   Length of msg in bytes
 */
void arcs_rust_log(uint8_t level, const char *tag, const char *msg, size_t msg_len);

#ifdef __cplusplus
}
#endif

#endif /* ARCS_RUST_LOG_H_ */
