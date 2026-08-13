/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */
#define LOG_TAG "rust_bind"
#include <lisa_log.h>

extern int rust_binding_test_main(void);

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    LOGI("=== Rust Binding Test STARTING ===");
    int ret = rust_binding_test_main();
    if (ret == 0) {
        LOGI("=== Rust Binding Test PASS-GATE OK ===");
    } else {
        LOGE("=== Rust Binding Test FAILED (ret=%d) ===", ret);
    }
    return 0;
}
