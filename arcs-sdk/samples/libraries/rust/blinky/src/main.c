/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 *
 * Rust Blinky on ARCS SDK.
 *
 * C is the real entry; it delegates the demo body to a Rust extern "C"
 * function defined in src/lib.rs via the arcs::entry! macro.
 */
#define LOG_TAG "rust_blinky"
#include <lisa_log.h>

extern int rust_blinky_main(void);

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    LOGI("=== Rust Blinky Starting ===");
    int ret = rust_blinky_main();
    if (ret != 0) {
        LOGE("rust_blinky_main returned %d", ret);
    }
    return 0;
}
