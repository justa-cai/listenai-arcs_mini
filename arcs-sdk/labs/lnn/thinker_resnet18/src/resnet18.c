/*
 * Copyright (c) 2026 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "resnet18"

#include "resnet18.h"

#include <ClockManager.h>
#include <arcs_ap.h>
#include <cache.h>
#include <dma_cpy.h>
#include <lisa_log.h>
#include <luna/luna.h>
#include <math.h>
#include <stddef.h>
#include <string.h>
#include <sysutils.h>
#include <thinker/thinker.h>
#include <thinker/thinker_status.h>

#define INPUT_CHANNELS (3)
#define INPUT_WIDTH    (32)
#define INPUT_HEIGHT   (32)
#define INPUT_POINTS   (INPUT_WIDTH * INPUT_HEIGHT)
#define MEMORY_ALIGN   (64U)
#define DTYPE_SIZE(dtype) ((uint32_t)((dtype) & 0xFU))

#define RETURN_IF_THINKER_FAILED(expr)          \
    do {                                        \
        if (!thinker_status_ok((expr), #expr)) { \
            return -1;                          \
        }                                       \
    } while (0)

#define GOTO_IF_THINKER_FAILED(expr, label)     \
    do {                                        \
        if (!thinker_status_ok((expr), #expr)) { \
            goto label;                         \
        }                                       \
    } while (0)

static uint8_t rgb8[INPUT_CHANNELS][INPUT_POINTS] __attribute__((aligned(MEMORY_ALIGN)));
static int8_t feats[INPUT_CHANNELS][INPUT_POINTS] __psram_noinit__ __attribute__((aligned(MEMORY_ALIGN)));
static uint8_t thinker_psram_pool[CONFIG_THINKER_RESNET18_PSRAM_POOL_SIZE] __psram_noinit__
    __attribute__((aligned(MEMORY_ALIGN)));
static uint8_t thinker_share_pool[CONFIG_THINKER_RESNET18_SHARE_POOL_SIZE]
    __attribute__((section(".thinker.apram.noinit"), aligned(MEMORY_ALIGN)));
static tMemory memory_list[7];
static int32_t num_memory;
static tModelHandle model;
static tExecHandle executor;
static bool initialized;
static bool dma_ready;

static double cifar100_train_mean[] = {0.5070751592371323, 0.48654887331495095, 0.4409178433670343};
static double cifar100_train_std[] = {0.2673342858792401, 0.2564384629170883, 0.27615047132568404};

static const char *results[] = {
    "apple",      "aquarium_fish", "baby",         "bear",       "beaver",      "bed",         "bee",
    "beetle",     "bicycle",       "bottle",       "bowl",       "boy",         "bridge",      "bus",
    "butterfly",  "camel",         "can",          "castle",     "caterpillar", "cattle",      "chair",
    "chimpanzee", "clock",         "cloud",        "cockroach",  "couch",       "crab",        "crocodile",
    "cup",        "dinosaur",      "dolphin",      "elephant",   "flatfish",    "forest",      "fox",
    "girl",       "hamster",       "house",        "kangaroo",   "keyboard",    "lamp",        "lawn_mower",
    "leopard",    "lion",          "lizard",       "lobster",    "man",         "maple_tree",  "motorcycle",
    "mountain",   "mouse",         "mushroom",     "oak_tree",   "orange",      "orchid",      "otter",
    "palm_tree",  "pear",          "pickup_truck", "pine_tree",  "plain",       "plate",       "poppy",
    "porcupine",  "possum",        "rabbit",       "raccoon",    "ray",         "road",        "rocket",
    "rose",       "sea",           "seal",         "shark",      "shrew",       "skunk",       "skyscraper",
    "snail",      "snake",         "spider",       "squirrel",   "streetcar",   "sunflower",   "sweet_pepper",
    "table",      "tank",          "telephone",    "television", "tiger",       "tractor",     "train",
    "trout",      "tulip",         "turtle",       "wardrobe",   "whale",       "willow_tree", "wolf",
    "woman",      "worm"};

static uint32_t align_up(uint32_t value, uint32_t align)
{
    return (value + align - 1U) & ~(align - 1U);
}

static bool thinker_status_ok(tStatus status, const char *op)
{
    if (status != T_SUCCESS) {
        LOGE("%s failed: %d", op, status);
        return false;
    }

    return true;
}

static int prepare_memory_plan(const int8_t *model_data, uint64_t model_size)
{
    uint32_t psram_used = 0;
    uint32_t share_used = 0;

    memset(memory_list, 0, sizeof(memory_list));
    num_memory = 0;

    RETURN_IF_THINKER_FAILED(tGetMemoryPlan(memory_list, &num_memory, model_data, model_size));

    for (int32_t i = 0; i < num_memory; i++) {
        uint32_t size = align_up(memory_list[i].size_, MEMORY_ALIGN);

        if (memory_list[i].dptr_ != 0U) {
            continue;
        }

        if (memory_list[i].dev_type_ == PSRAM || memory_list[i].dev_type_ == UNCERTAIN) {
            if (psram_used + size > sizeof(thinker_psram_pool)) {
                LOGE("PSRAM pool too small: need=%u configured=%u", psram_used + size,
                     (uint32_t)sizeof(thinker_psram_pool));
                return -1;
            }

            memory_list[i].dptr_ = (addr_type)(uintptr_t)&thinker_psram_pool[psram_used];
            psram_used += size;
        } else if (memory_list[i].dev_type_ == SHARE_MEM) {
            if (share_used + size > sizeof(thinker_share_pool)) {
                LOGE("share pool too small: need=%u configured=%u", share_used + size,
                     (uint32_t)sizeof(thinker_share_pool));
                return -1;
            }

            memory_list[i].dptr_ = (addr_type)(uintptr_t)&thinker_share_pool[share_used];
            share_used += size;
        } else {
            LOGE("unsupported memory plan entry: idx=%d dev_type=%u size=%u", i, memory_list[i].dev_type_,
                 memory_list[i].size_);
            return -1;
        }

        LOGI("memory[%d]: dev=%u mem=%u size=%u ptr=0x%08x", i, memory_list[i].dev_type_,
             memory_list[i].mem_type_, memory_list[i].size_, (uint32_t)memory_list[i].dptr_);
    }

    memset(thinker_psram_pool, 0, psram_used);
    if (share_used > 0U) {
        memset(thinker_share_pool, 0, share_used);
    }

    LOGI("memory plan: entries=%d psram=%u share=%u", num_memory, psram_used, share_used);

    return 0;
}

static void release_dma(void)
{
    if (dma_ready) {
        dma_uninit(ALG_DMA_CH);
        dma_ready = false;
    }
}

int resnet18_init(void)
{
    const int8_t *model_data = (const int8_t *)(uintptr_t)CONFIG_THINKER_RESNET18_MODEL_ADDR;
    uint64_t model_size = CONFIG_THINKER_RESNET18_MODEL_SIZE;

    if (initialized) {
        return 0;
    }

    LOGI("Thinker version: %s", tGetVersion(0));
    __HAL_CRM_LUNA_CLK_ENABLE();
    /* Luna validation apps run with DCache disabled; flush first because SDK startup enables it. */
    HAL_FlushDCache();
    HAL_DisableDCache();
    enable_GINT();
    luna_init();
    LOGI("Luna version: 0x%08x freq=%u", luna_version(), CRM_GetLunaFreq());

    dma_ready = dma_init(ALG_DMA_CH);
    if (!dma_ready) {
        LOGE("dma_init failed");
        return -1;
    }

    LOGI("model addr=0x%08x size=%llu", (uint32_t)CONFIG_THINKER_RESNET18_MODEL_ADDR, model_size);

    if (model_size == 0U) {
        LOGE("CONFIG_THINKER_RESNET18_MODEL_SIZE is 0");
        release_dma();
        return -1;
    }

    GOTO_IF_THINKER_FAILED(tInitialize(), fail_dma);

    if (prepare_memory_plan(model_data, model_size) != 0) {
        goto fail_runtime;
    }

    GOTO_IF_THINKER_FAILED(tModelInit(&model, model_data, model_size, memory_list, num_memory), fail_runtime);

    GOTO_IF_THINKER_FAILED(tCreateExecutor(model, &executor, memory_list, num_memory), fail_model);

    initialized = true;
    return 0;

fail_model:
    (void)tModelFini(model);
    model = 0;
fail_runtime:
    (void)tUninitialize();
fail_dma:
    release_dma();
    return -1;
}

int resnet18_process(const void *data, uint32_t width, uint32_t height, bool rotated, const char **result_label,
                     int8_t *result_score)
{
    if (!initialized || result_label == NULL || result_score == NULL || data == NULL || width == 0U || height == 0U) {
        return -1;
    }

    /* Point resize and convert from RGB565 to RGB888 */
    for (uint32_t y = 0; y < INPUT_HEIGHT; y++) {
        for (uint32_t x = 0; x < INPUT_WIDTH; x++) {
            uint32_t src_x = x * width / INPUT_WIDTH;
            uint32_t src_y = y * height / INPUT_HEIGHT;
            if (rotated) {
                src_x = width - src_x - 1;
                src_y = height - src_y - 1;
            }
            uint16_t rgb565 = ((uint16_t *)data)[src_y * width + src_x];
            uint8_t r = (rgb565 >> 11) & 0x1F;
            uint8_t g = (rgb565 >> 5) & 0x3F;
            uint8_t b = rgb565 & 0x1F;

            rgb8[0][y * INPUT_WIDTH + x] = r << 3;
            rgb8[1][y * INPUT_WIDTH + x] = g << 2;
            rgb8[2][y * INPUT_WIDTH + x] = b << 3;
        }
    }

    /* Extract features and convert to int8 */
    for (uint32_t i = 0; i < INPUT_POINTS; i++) {
        for (uint32_t j = 0; j < INPUT_CHANNELS; j++) {
            double feat = (double)(rgb8[j][i] / 255.0 - cifar100_train_mean[j]) / cifar100_train_std[j];
            feats[j][i] = (int8_t)floor(feat * 64 + 0.5);
        }
    }

    tData input;
    RETURN_IF_THINKER_FAILED(tGetInputInfo(executor, 0, &input));

    uint32_t input_bytes = DTYPE_SIZE(input.dtype_);
    for (uint32_t i = 0; i < input.shape_.ndim_; i++) {
        input_bytes *= input.shape_.dims_[i];
    }

    LOGI("input: ptr=%p dev=%u dtype=0x%x ndim=%u bytes=%u", input.dptr_, input.dev_type_, input.dtype_,
         input.shape_.ndim_, input_bytes);

    if (input.dtype_ != Int8 || input_bytes != sizeof(feats)) {
        LOGE("unexpected input: dtype=0x%x bytes=%u expected=%u", input.dtype_, input_bytes, (uint32_t)sizeof(feats));
        return -1;
    }

    if (input.dptr_ != NULL) {
        (void)memcpy(input.dptr_, &feats[0][0], sizeof(feats));
    } else {
        input.dptr_ = &feats[0][0];
        input.dev_type_ = PSRAM;
    }

    RETURN_IF_THINKER_FAILED(tSetInput(executor, 0, &input));

    RETURN_IF_THINKER_FAILED(tForward(executor));

    tData output;
    RETURN_IF_THINKER_FAILED(tGetOutput(executor, 0, &output));

    if (output.dptr_ == NULL || output.dtype_ != Int8 || output.shape_.ndim_ < 2U) {
        LOGE("unexpected output: ptr=%p dtype=0x%x ndim=%u", output.dptr_, output.dtype_, output.shape_.ndim_);
        return -1;
    }

    uint32_t scores_cnt = 1U;
    for (uint32_t i = 0; i < output.shape_.ndim_; i++) {
        scores_cnt *= output.shape_.dims_[i];
    }

    if (scores_cnt == 0U || scores_cnt > (sizeof(results) / sizeof(results[0])) || DTYPE_SIZE(output.dtype_) != 1U) {
        LOGE("unexpected output size: %u", scores_cnt);
        return -1;
    }

    int8_t *scores = (int8_t *)output.dptr_;
    /* Output format: Int8 tensor [1, 100], one quantized score per CIFAR-100 label in results[]. */

    uint32_t best_index = 0;
    int8_t best_score = scores[0];
    for (uint32_t i = 1; i < scores_cnt; i++) {
        if (scores[i] > best_score) {
            best_index = i;
            best_score = scores[i];
        }
    }

    LOGI("best score: %d, index: %u, label: %s", best_score, best_index, results[best_index]);

    *result_label = results[best_index];
    *result_score = best_score;

    return 0;
}

int resnet18_deinit(void)
{
    if (!initialized) {
        return 0;
    }

    (void)tReleaseExecutor(executor);
    (void)tModelFini(model);
    (void)tUninitialize();
    release_dma();

    executor = 0;
    model = 0;
    initialized = false;

    return 0;
}
