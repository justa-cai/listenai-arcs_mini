/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */
#define LOG_TAG "rust_fpu_softfloat"
#include <lisa_log.h>

extern int rust_main(void);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    LOGI("=== Rust FPU demo: soft-float ABI + target-feature=+f (ilp32) ===");
    int ret = rust_main();
    if (ret != 0) {
        LOGE("rust_main returned %d", ret);
    }
    return 0;
}
