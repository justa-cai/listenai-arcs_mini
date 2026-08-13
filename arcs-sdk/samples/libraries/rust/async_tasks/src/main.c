/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */
#define LOG_TAG "rust_async_tasks"
#include <lisa_log.h>

extern int rust_main(void);

/*
 * Thin shim into the Rust entry point. rust_main() brings up the embassy
 * executor on this (the main) FreeRTOS task and never returns.
 */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    LOGI("=== Rust async/embassy demo ===");
    int ret = rust_main();
    if (ret != 0) {
        LOGE("rust_main returned %d", ret);
    }
    return 0;
}
