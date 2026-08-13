/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * Rust Hello-World on ARCS SDK.
 *
 * C is the real entry; it delegates the demo body to a Rust extern "C"
 * function defined in src/lib.rs via the arcs::entry! macro.
 */
#define LOG_TAG "rust_test"
#include <lisa_log.h>

extern int rust_hello_main(void);

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    LOGI("=== Rust Language Support Test ===");
    int ret = rust_hello_main();
    if (ret == 0) {
        LOGI("=== Rust Test PASSED ===");
    } else {
        LOGE("=== Rust Test FAILED (ret=%d) ===", ret);
    }
    return 0;
}
