/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */
#define LOG_TAG "rust_i2c_scan"
#include <lisa_log.h>

extern int rust_main(void);

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    LOGI("=== Rust I2C bus-scan demo ===");
    int ret = rust_main();
    if (ret != 0) LOGE("rust_main returned %d", ret);
    return 0;
}
