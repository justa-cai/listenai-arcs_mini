/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief LISA KV EasyFlash 后端基础示例
 *
 * 本示例演示 lisa_kv 在 EasyFlash 后端下的基础用法：
 * 1. 初始化 KV 存储
 * 2. 写入和读取 int/string/bool/blob
 * 3. 释放 get 接口返回的动态内存
 * 4. 删除示例 key 并 dump 当前 KV 内容
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define LOG_TAG "lisa_kv_sample"
#include <lisa_log.h>

#include "FreeRTOS.h"
#include "task.h"
#include "lisa_kv.h"

#define KV_KEY_INT       "kv.demo.int"
#define KV_KEY_STRING    "kv.demo.str"
#define KV_KEY_BOOL      "kv.demo.bool"
#define KV_KEY_BLOB      "kv.demo.blob"

static int check_result(const char *step, int ret)
{
    if (ret != 0) {
        LISA_LOGE(LOG_TAG, "%s failed, ret=%d", step, ret);
        return -1;
    }

    LISA_LOGI(LOG_TAG, "%s ok", step);
    return 0;
}

static int demo_int(void)
{
    int value = 0;

    if (check_result("set int", lisa_kv_set_int(KV_KEY_INT, 1234)) != 0) {
        return -1;
    }

    if (check_result("get int", lisa_kv_get_int(KV_KEY_INT, &value)) != 0) {
        return -1;
    }

    if (value != 1234) {
        LISA_LOGE(LOG_TAG, "int value mismatch: %d", value);
        return -1;
    }

    LISA_LOGI(LOG_TAG, "int value: %d", value);
    return 0;
}

static int demo_string(void)
{
    char *value = NULL;
    const char *expected = "hello easyflash kv";

    if (check_result("set string", lisa_kv_set_string(KV_KEY_STRING, expected)) != 0) {
        return -1;
    }

    if (check_result("get string", lisa_kv_get_string(KV_KEY_STRING, &value)) != 0) {
        return -1;
    }

    if (strcmp(value, expected) != 0) {
        LISA_LOGE(LOG_TAG, "string mismatch: %s", value);
        lisa_kv_free(value);
        return -1;
    }

    LISA_LOGI(LOG_TAG, "string value: %s", value);
    lisa_kv_free(value);
    return 0;
}

static int demo_bool(void)
{
    bool value = false;

    if (check_result("set bool", lisa_kv_set_bool(KV_KEY_BOOL, true)) != 0) {
        return -1;
    }

    if (check_result("get bool", lisa_kv_get_bool(KV_KEY_BOOL, &value)) != 0) {
        return -1;
    }

    if (!value) {
        LISA_LOGE(LOG_TAG, "bool value mismatch");
        return -1;
    }

    LISA_LOGI(LOG_TAG, "bool value: %s", value ? "true" : "false");
    return 0;
}

static int demo_blob(void)
{
    const uint8_t expected[] = { 0x11, 0x22, 0x33, 0x44, 0x55 };
    uint8_t *value = NULL;
    int len = 0;

    if (check_result("set blob", lisa_kv_set_blob(KV_KEY_BLOB, (uint8_t *)expected, sizeof(expected))) != 0) {
        return -1;
    }

    if (check_result("get blob", lisa_kv_get_blob(KV_KEY_BLOB, &value, &len)) != 0) {
        return -1;
    }

    if ((len != (int)sizeof(expected)) || (memcmp(value, expected, sizeof(expected)) != 0)) {
        LISA_LOGE(LOG_TAG, "blob mismatch, len=%d", len);
        lisa_kv_free(value);
        return -1;
    }

    LISA_LOGI(LOG_TAG, "blob value len: %d", len);
    lisa_kv_free(value);
    return 0;
}

static void cleanup_demo_keys(void)
{
    (void)lisa_kv_del(KV_KEY_INT);
    (void)lisa_kv_del(KV_KEY_STRING);
    (void)lisa_kv_del(KV_KEY_BOOL);
    (void)lisa_kv_del(KV_KEY_BLOB);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    LISA_LOGI(LOG_TAG, "=== LISA KV EasyFlash Example ===");

    if (check_result("lisa_kv_init", lisa_kv_init()) != 0) {
        goto failed;
    }

    cleanup_demo_keys();

    if ((demo_int() != 0) ||
        (demo_string() != 0) ||
        (demo_bool() != 0) ||
        (demo_blob() != 0)) {
        goto failed;
    }

    LISA_LOGI(LOG_TAG, "dump EasyFlash env after write:");
    lisa_kv_dump();

    cleanup_demo_keys();
    LISA_LOGI(LOG_TAG, "demo keys deleted");
    LISA_LOGI(LOG_TAG, "=== LISA KV EasyFlash Example completed ===");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

failed:
    LISA_LOGE(LOG_TAG, "=== LISA KV EasyFlash Example failed ===");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
