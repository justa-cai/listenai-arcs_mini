// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
//
// Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
// Free software under GPL-3.0-or-later. Reusing any part of this file, in any
// work, must keep this notice, credit iamankushpandit as the author,
// and stay under the same licence with corresponding source offered.
// See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.
//
// ARCS-MINI CP 侧集成层。分层放置（wte→flash XIP，其余→PSRAM）：
//   http:// 下载两遍法：第一遍计数+抓头部 64B 算 wte 区间，
//   第二遍路由分流——wte 段写 flash（OTA 槽 0xA00000 起），其余进 PSRAM。
//   /SD: 路径维持整块 PSRAM 装载（需 ~6.4MB 空闲，内存紧张时用 http）。

#define LOG_TAG "llm.app"
#include "lisa_log.h"

#include <string.h>
#include <stdio.h>

#include "sysutils.h"   /* psram_malloc_align / psram_free */
#include "lsfs.h"
#include "lisa_http.h"
#include "lisa_device.h"
#include "lisa_flash.h"

#include "llm_app.h"
#include "story_arena.h"
#include "story_llm.h"
#include "story_log.h"
#include "story_mem.h"
#include "neo.h"

#define LLM_DEFAULT_MODEL_PATH "/SD:/llm/model.bin"
#define LLM_DEFAULT_TOK_PATH   "/SD:/llm/tok.bin"
/* 下载含 flash 擦写（~960 个 4KB 块），超时给足 */
#define LLM_HTTP_TIMEOUT_MS    300000u

/* 预算（8M kid_v8, kv=128 设备形状：fast ~30KB / bulk ~340KB）。
 * 各留 1.5 倍余量；fast/bulk 都放 PSRAM（CP 内部 SRAM 紧张）。 */
#define LLM_FAST_BYTES (64 * 1024)
#define LLM_BULK_BYTES (448 * 1024)

/* ── flash 驻留模型布局（LLM 分支，OTA 功能被模型借用）────────────
 * app 镜像结束(~0x918000)之后到 KV(0xF00000) 之前：
 *   model.bin @ 0x920000  (8M kid_v8: 5,854,896B → 结束 0xEB4210)
 *   tok.bin   @ 0xEB8000  (214,689B → 结束 0xEEC671)
 * 烧写：recovery 模式 adb push <file> /RAW/NAND/<hex偏移>（同固件烧录通道）。
 * 运行时 wte/tok 直接 flash XIP，head/body 拷入 PSRAM。 */
#define LLM_FLASH_CPU_BASE     0x30000000u
#define LLM_MODEL_FLASH_OFF    0x920000u
#define LLM_TOK_FLASH_OFF      0xEC0000u   /* 64KB 对齐（boot RAW 烧写要求） */
#define LLM_TOK_DEFAULT_LEN    214689u
#define LLM_FLASH_BLOCK        4096u

static struct {
    /* 分层布局 */
    uint8_t *head;           /* [0, wte_off)：header/norms/biases/wpe */
    size_t head_len;
    uint8_t *body;           /* [wte_end, total)：wq..w2 压缩副本 */
    size_t body_len;
    size_t wte_off, wte_len; /* wte 在完整 blob 中的区间 */
    size_t total;            /* 虚拟完整 blob 长度 */
    bool split;              /* true = wte 在 flash */
    /* 通用 */
    uint8_t *model;          /* split 时 = head；整块时 = 完整 blob */
    size_t model_len;
    uint8_t *tok;
    size_t tok_len;
    bool tok_in_flash;      /* tok 直接 XIP flash，unload 不释放 */
    uint8_t *fast_buf;
    uint8_t *bulk_buf;
    story_arena_t fast;
    story_arena_t bulk;
    llm_t llm;
    bool loaded;
} S;

/* -------------------------------------------------- HTTP 分层下载 */
static size_t rt_off;                 /* 当前流位置（两遍共用起点复位） */
static uint8_t hdr_cap[64];
static size_t hdr_cap_len;

static bool routing;                   /* 第二遍分流进行中 */
static size_t head_got, body_got;
static uint8_t flash_stage[LLM_FLASH_BLOCK];
static size_t stage_len;
static bool flash_write_active;       /* flash 中无有效副本时才写 */
static uint32_t flash_next_off;
static lisa_device_t *flash_dev;
/* http 通道写 wte 的目标（OTA 槽头，与 flash 驻留模型区无关） */
#define LLM_WTE_HTTP_OFF      0xA00000u
#define LLM_WTE_HTTP_DATA_OFF (LLM_WTE_HTTP_OFF + 0x1000u)
#define LLM_WTE_HTTP_CPU_BASE (LLM_FLASH_CPU_BASE + LLM_WTE_HTTP_DATA_OFF)
#define LLM_WTE_MAGIC         0x5745544Cu

static uint64_t hdr_hash(const uint8_t *h, size_t total)
{
    uint64_t v = 1469598103934665603ull;
    for (size_t i = 0; i < 16; i++) {
        v ^= h[i];
        v *= 1099511628211ull;
    }
    for (int i = 0; i < 8; i++) {
        v ^= (total >> (i * 8)) & 0xff;
        v *= 1099511628211ull;
    }
    return v;
}

static int flash_stage_flush_pad(void)
{
    if (stage_len == 0) return 0;
    while (stage_len < LLM_FLASH_BLOCK) flash_stage[stage_len++] = 0xFF;
    if (lisa_flash_erase(flash_dev, flash_next_off, LLM_FLASH_BLOCK) < 0 ||
        lisa_flash_write(flash_dev, flash_next_off, flash_stage, LLM_FLASH_BLOCK) < 0) {
        LISA_LOGE(LOG_TAG, "flash write failed @0x%x", (unsigned)flash_next_off);
        return -1;
    }
    flash_next_off += LLM_FLASH_BLOCK;
    stage_len = 0;
    return 0;
}

static int flash_stage_feed(const uint8_t *p, size_t n)
{
    if (!flash_write_active) return 0;   /* flash 已有副本，只越流 */
    while (n > 0) {
        size_t take = LLM_FLASH_BLOCK - stage_len;
        if (take > n) take = n;
        memcpy(flash_stage + stage_len, p, take);
        stage_len += take;
        p += take;
        n -= take;
        if (stage_len == LLM_FLASH_BLOCK) {
            if (flash_stage_flush_pad() != 0) return -1;
        }
    }
    return 0;
}

/* 第二遍路由：head 段→PSRAM head；wte 段→flash；尾部→PSRAM body */
static int route_pass2(const uint8_t *p, size_t n)
{
    while (n > 0) {
        size_t take = n;
        if (rt_off < S.wte_off) {
            if (rt_off + take > S.wte_off) take = S.wte_off - rt_off;
            memcpy(S.head + rt_off, p, take);
            head_got += take;
        } else if (rt_off < S.wte_off + S.wte_len) {
            if (rt_off + take > S.wte_off + S.wte_len) take = S.wte_off + S.wte_len - rt_off;
            if (flash_stage_feed(p, take) != 0) return -1;
        } else {
            size_t bo = rt_off - (S.wte_off + S.wte_len);
            if (bo + take > S.body_len) take = S.body_len - bo;
            memcpy(S.body + bo, p, take);
            body_got += take;
        }
        rt_off += take;
        p += take;
        n -= take;
    }
    return 0;
}

static void http_on_data(lisa_http_data_t *d)
{
    if (!d || !d->buf || d->len <= 0) return;
    size_t len = (size_t)d->len;

    /* 头部捕获（第一遍） */
    if (hdr_cap_len < sizeof hdr_cap) {
        size_t take = sizeof hdr_cap - hdr_cap_len;
        if (take > len) take = len;
        memcpy(hdr_cap + hdr_cap_len, d->buf, take);
        hdr_cap_len += take;
    }

    if (routing) {
        /* route_pass2 内部按段推进 rt_off（流位置的唯一记账点） */
        if (route_pass2((const uint8_t *)d->buf, len) != 0) {
            routing = false;   /* flash 写失败：停流，交上层报错 */
        }
    } else {
        rt_off += len;
    }
    if ((rt_off % (512 * 1024)) < len) {
        LISA_LOGI(LOG_TAG, "streaming... %u KB", (unsigned)(rt_off / 1024));
    }
}

static int http_get(const char *url)
{
    lisa_http_request_t req = {
        .method = LISA_HTTP_GET,
        .url = (uint8_t *)url,
        .timeout = LLM_HTTP_TIMEOUT_MS,
        .on_data = http_on_data,
    };
    lisa_http_t *ins = lisa_http_init(&req);
    if (!ins) {
        LISA_LOGE(LOG_TAG, "http init failed: %s", url);
        return -1;
    }
    /* 必须用 download 而非 perform：perform 会整包缓冲响应体（模型 5.85MB
     * 直接把 PSRAM 打爆），download 按 1KB 块流式回调。 */
    if (lisa_http_download(ins) != LISA_HTTP_OK) {
        LISA_LOGE(LOG_TAG, "http download failed: %s", url);
        lisa_http_cleanup(ins);
        return -1;
    }
    lisa_http_cleanup(ins);
    return 0;
}

static int flash_marker_valid(uint64_t want_hash, size_t wte_len)
{
    const uint32_t *m = (const uint32_t *)(LLM_FLASH_CPU_BASE + LLM_WTE_HTTP_OFF);
    if (m[0] != LLM_WTE_MAGIC) return 0;
    if (m[1] != (uint32_t)wte_len) return 0;
    const uint64_t *h = (const uint64_t *)&m[2];
    if (*h != want_hash) return 0;
    return 1;
}

static int flash_marker_write(uint64_t hash, size_t wte_len)
{
    static uint8_t blk[LLM_FLASH_BLOCK];
    memset(blk, 0xFF, sizeof blk);
    uint32_t *m = (uint32_t *)blk;
    m[0] = LLM_WTE_MAGIC;
    m[1] = (uint32_t)wte_len;
    memcpy(&m[2], &hash, 8);
    /* 标记最后写：wte 数据成功落地后才置位 */
    if (lisa_flash_erase(flash_dev, LLM_WTE_HTTP_OFF, LLM_FLASH_BLOCK) < 0 ||
        lisa_flash_write(flash_dev, LLM_WTE_HTTP_OFF, blk, LLM_FLASH_BLOCK) < 0) {
        return -1;
    }
    return 0;
}

static int load_model_split_http(const char *url)
{
    /* 第一遍：计数 + 抓头部 */
    hdr_cap_len = 0;
    rt_off = 0;
    if (http_get(url) != 0) return -1;
    size_t total = rt_off;
    if (total < 64) {
        LISA_LOGE(LOG_TAG, "model too small: %u", (unsigned)total);
        return -1;
    }
    if (!neo_wte_span(hdr_cap, &S.wte_off, &S.wte_len, NULL)) {
        LISA_LOGE(LOG_TAG, "bad model header");
        return -1;
    }
    S.total = total;
    if (S.wte_off + S.wte_len >= total) {
        LISA_LOGE(LOG_TAG, "wte span out of range: off=%u len=%u total=%u",
                  (unsigned)S.wte_off, (unsigned)S.wte_len, (unsigned)total);
        return -1;
    }
    S.head_len = S.wte_off;
    S.body_len = total - S.wte_off - S.wte_len;

    neo_config_t cfg;
    neo_parse_config(hdr_cap, 64, &cfg);
    LISA_LOGI(LOG_TAG, "model %u B: wte[%u..%u)=%u B flash, head=%u body=%u psram",
              (unsigned)total, (unsigned)S.wte_off,
              (unsigned)(S.wte_off + S.wte_len), (unsigned)S.wte_len,
              (unsigned)S.head_len, (unsigned)S.body_len);
    LISA_LOGI(LOG_TAG, "config: dim=%d hd=%d L=%d V=%d seq=%d",
              cfg.dim, cfg.hidden_dim, cfg.n_layers, cfg.vocab_size, cfg.seq_len);

    S.head = psram_malloc_align(16, S.head_len);
    S.body = psram_malloc_align(16, S.body_len);
    if (!S.head || !S.body) {
        LISA_LOGE(LOG_TAG, "psram alloc failed head=%p body=%p", (void *)S.head, (void *)S.body);
        return -1;
    }

    /* flash 副本检查 */
    flash_dev = lisa_device_get("flash0");
    if (!flash_dev || !lisa_device_ready(flash_dev)) {
        LISA_LOGE(LOG_TAG, "flash0 not ready");
        return -1;
    }
    uint64_t hash = hdr_hash(hdr_cap, total);
    if (flash_marker_valid(hash, S.wte_len)) {
        flash_write_active = false;
        LISA_LOGI(LOG_TAG, "wte flash copy valid, skip rewrite");
    } else {
        flash_write_active = true;
        stage_len = 0;
        flash_next_off = LLM_WTE_HTTP_DATA_OFF;
        LISA_LOGI(LOG_TAG, "writing wte to flash @0x%x (%u B)...",
                  (unsigned)LLM_WTE_HTTP_DATA_OFF, (unsigned)S.wte_len);
    }

    /* 第二遍：分流 */
    head_got = body_got = 0;
    rt_off = 0;
    routing = true;
    int rc = http_get(url);
    routing = false;
    if (rc == 0 && flash_write_active) {
        rc = flash_stage_flush_pad();
    }
    if (rc == 0 && flash_write_active) {
        rc = flash_marker_write(hash, S.wte_len);
    }
    if (rc != 0) return -1;
    if (head_got != S.head_len || body_got != S.body_len) {
        LISA_LOGE(LOG_TAG, "stream incomplete: head %u/%u body %u/%u",
                  (unsigned)head_got, (unsigned)S.head_len,
                  (unsigned)body_got, (unsigned)S.body_len);
        return -1;
    }

    S.split = true;
    S.model = S.head;
    S.model_len = S.total;
    return 0;
}

/* -------------------------------------------------- 整块装载（/SD: 路径） */
static uint8_t *load_file(const char *path, size_t *out_len)
{
    struct lsfs_dirent st;
    memset(&st, 0, sizeof st);
    if (lsfs_stat(path, &st) != 0) {
        LISA_LOGE(LOG_TAG, "stat failed: %s", path);
        return NULL;
    }
    size_t size = st.size;
    if (size < 64) {
        LISA_LOGE(LOG_TAG, "file too small: %s (%u bytes)", path, (unsigned)size);
        return NULL;
    }
    uint8_t *buf = psram_malloc_align(16, size);
    if (!buf) {
        LISA_LOGE(LOG_TAG, "psram alloc %u bytes failed for %s", (unsigned)size, path);
        return NULL;
    }
    struct lsfs_file_t f;
    lsfs_file_t_init(&f);
    if (lsfs_open(&f, path, LSFS_O_READ) != 0) {
        LISA_LOGE(LOG_TAG, "open failed: %s", path);
        psram_free(buf);
        return NULL;
    }
    size_t got = 0;
    while (got < size) {
        size_t chunk = size - got;
        if (chunk > 32 * 1024) chunk = 32 * 1024;
        ssize_t n = lsfs_read(&f, buf + got, chunk);
        if (n <= 0) break;
        got += (size_t)n;
    }
    lsfs_close(&f);
    if (got != size) {
        LISA_LOGE(LOG_TAG, "read incomplete: %s %u/%u", path, (unsigned)got, (unsigned)size);
        psram_free(buf);
        return NULL;
    }
    *out_len = size;
    LISA_LOGI(LOG_TAG, "loaded %s: %u bytes @%p", path, (unsigned)size, (void *)buf);
    return buf;
}

int llm_app_load(const char *model_path, const char *tok_path)
{
    if (S.loaded) {
        LISA_LOGW(LOG_TAG, "already loaded, run 'llm free' first");
        return -1;
    }
    if (!model_path || !model_path[0]) model_path = LLM_DEFAULT_MODEL_PATH;
    if (!tok_path || !tok_path[0]) tok_path = LLM_DEFAULT_TOK_PATH;

    story_mem_log("before load");

    if (strncmp(model_path, "http://", 7) == 0) {
        if (load_model_split_http(model_path) != 0) goto err;
    } else {
        S.model = load_file(model_path, &S.model_len);
        S.split = false;
        if (!S.model) return -1;
    }
    S.tok = load_file(tok_path, &S.tok_len);
    if (!S.tok) goto err;

    S.fast_buf = psram_malloc_align(16, LLM_FAST_BYTES);
    S.bulk_buf = psram_malloc_align(16, LLM_BULK_BYTES);
    if (!S.fast_buf || !S.bulk_buf) {
        LISA_LOGE(LOG_TAG, "arena alloc failed fast=%p bulk=%p",
                  (void *)S.fast_buf, (void *)S.bulk_buf);
        goto err;
    }

    story_arena_init(&S.fast, S.fast_buf, LLM_FAST_BYTES);
    story_arena_init(&S.bulk, S.bulk_buf, LLM_BULK_BYTES);
    story_arena_begin(&S.fast, "llm-fast");
    story_arena_begin(&S.bulk, "llm-bulk");

    llm_blobs_t blobs = {.model = S.model, .model_len = S.model_len,
                         .tok = S.tok, .tok_len = S.tok_len,
                         .wte = S.split ? (const uint8_t *)LLM_WTE_HTTP_CPU_BASE : NULL,
                         .body = S.split ? S.body : NULL};
    llm_params_t params = llm_default_params();

    int64_t t0 = story_time_us();
    if (!llm_load(&S.llm, &blobs, &params, &S.fast, &S.bulk)) {
        LISA_LOGE(LOG_TAG, "llm_load failed");
        goto err;
    }
    S.loaded = true;
    LISA_LOGI(LOG_TAG, "llm_load ok in %lld ms", (long long)((story_time_us() - t0) / 1000));
    story_arena_log(&S.fast, "fast");
    story_arena_log(&S.bulk, "bulk");
    story_mem_log("after load");
    return 0;

err:
    llm_app_unload();
    return -1;
}

int llm_app_unload(void)
{
    if (S.loaded) {
        llm_unload(&S.llm);
        S.loaded = false;
    }
    story_arena_end(&S.fast);
    story_arena_end(&S.bulk);
    if (S.tok && !S.tok_in_flash) psram_free(S.tok);
    S.tok_in_flash = false;
    if (S.split) {
        if (S.head) psram_free(S.head);
        if (S.body) psram_free(S.body);
    } else if (S.model) {
        psram_free(S.model);
    }
    if (S.fast_buf) psram_free(S.fast_buf);
    if (S.bulk_buf) psram_free(S.bulk_buf);
    memset(&S, 0, sizeof S);
    LISA_LOGI(LOG_TAG, "unloaded");
    return 0;
}

bool llm_app_is_loaded(void)
{
    return S.loaded;
}

static bool piece_log(void *user, const char *piece, int len)
{
    (void)user;
    /* 每 token 一条日志：调试期观察流式输出与节奏 */
    LISA_LOGI(LOG_TAG, "> %.*s", len, piece);
    return true;
}

static int run_answer(bool story_mode, const char *input)
{
    if (!S.loaded) {
        LISA_LOGE(LOG_TAG, "not loaded, run 'llm load' first");
        return -1;
    }
    static char answer[LLM_ANSWER_CHARS + 1];
    llm_stats_t st;
    int64_t t0 = story_time_us();
    bool ok;
    if (story_mode) {
        ok = llm_story(&S.llm, input, answer, sizeof answer, piece_log, NULL, &st);
    } else {
        llm_history_t hist;
        llm_history_clear(&hist);
        ok = llm_answer(&S.llm, &hist, input, answer, sizeof answer, piece_log, NULL, &st);
    }
    int64_t ms = (story_time_us() - t0) / 1000;
    if (!ok) {
        LISA_LOGE(LOG_TAG, "generate failed");
        return -1;
    }
    LISA_LOGI(LOG_TAG, "ANSWER(%lldms): %s", (long long)ms, answer);
    if (st.gen_tokens > 0) {
        LISA_LOGI(LOG_TAG,
                  "stats: prompt=%d gen=%d stop=%s load=%lldms prefill=%lldms gen=%lldms (%lldms/tok)",
                  st.prompt_tokens, st.gen_tokens, st.stop_reason,
                  st.load_us / 1000, st.prefill_us / 1000, st.gen_us / 1000,
                  st.gen_us / 1000 / st.gen_tokens);
    }
    return 0;
}

int llm_app_ask(const char *question)
{
    return run_answer(false, question);
}

int llm_app_story(const char *topic)
{
    return run_answer(true, topic);
}

extern volatile uint32_t SystemCoreClock;

void llm_app_info(void)
{
    LISA_LOGI(LOG_TAG, "clock=%u Hz", (unsigned)SystemCoreClock);
    LISA_LOGI(LOG_TAG, "loaded=%d split=%d model=%u tok=%u (head=%u body=%u wte@flash=%u)",
              S.loaded, S.split, (unsigned)S.model_len, (unsigned)S.tok_len,
              (unsigned)S.head_len, (unsigned)S.body_len,
              (unsigned)S.wte_len);
    if (S.loaded) {
        story_arena_log(&S.fast, "fast");
        story_arena_log(&S.bulk, "bulk");
    }
    story_mem_log("info");
}

/* -------------------------------------------------- flash 驻留加载 */
int llm_app_load_flash(uint32_t model_off, uint32_t tok_off)
{
    if (S.loaded) {
        LISA_LOGW(LOG_TAG, "already loaded, run 'llm free' first");
        return -1;
    }
    const uint8_t *fb = (const uint8_t *)(LLM_FLASH_CPU_BASE + model_off);
    const uint8_t *tb = (const uint8_t *)(LLM_FLASH_CPU_BASE + tok_off);
    if ((uintptr_t)fb & 15) {
        LISA_LOGE(LOG_TAG, "model offset must be 16-byte aligned");
        return -1;
    }

    story_mem_log("before flash load");

    /* 模型头直接从 flash 解析校验 */
    if (!neo_wte_span(fb, &S.wte_off, &S.wte_len, &S.total)) {
        LISA_LOGE(LOG_TAG, "bad model @0x%x (not burned?)", (unsigned)model_off);
        return -1;
    }
    LISA_LOGI(LOG_TAG, "model @0x%x total=%u (wte span %u..%u), whole blob -> psram",
              (unsigned)model_off, (unsigned)S.total,
              (unsigned)S.wte_off, (unsigned)(S.wte_off + S.wte_len));

    /* AP PSRAM 已让渡（AP 1MB / CP 15MB），整块拷入 PSRAM——wte 不再
     * 走 flash XIP，消除输出头每 token 的 flash 带宽瓶颈 */
    S.model = psram_malloc_align(16, S.total);
    S.tok = psram_malloc_align(16, LLM_TOK_DEFAULT_LEN);
    if (!S.model || !S.tok) {
        LISA_LOGE(LOG_TAG, "psram alloc failed model=%p tok=%p", (void *)S.model, (void *)S.tok);
        goto err;
    }

    uint32_t magic;
    memcpy(&magic, tb, 4);
    if (magic != 0x324B5443u) {
        LISA_LOGE(LOG_TAG, "bad tok @0x%x (not burned?)", (unsigned)tok_off);
        goto err;
    }

    int64_t t0 = story_time_us();
    memcpy(S.model, fb, S.total);
    memcpy(S.tok, tb, LLM_TOK_DEFAULT_LEN);
    S.model_len = S.total;
    S.tok_len = LLM_TOK_DEFAULT_LEN;
    S.split = false;   /* 单块 blob，无需分层指针 */
    LISA_LOGI(LOG_TAG, "blob+tok copied flash->psram in %lld ms",
              (long long)((story_time_us() - t0) / 1000));

    S.fast_buf = psram_malloc_align(16, LLM_FAST_BYTES);
    S.bulk_buf = psram_malloc_align(16, LLM_BULK_BYTES);
    if (!S.fast_buf || !S.bulk_buf) {
        LISA_LOGE(LOG_TAG, "arena alloc failed");
        goto err;
    }
    story_arena_init(&S.fast, S.fast_buf, LLM_FAST_BYTES);
    story_arena_init(&S.bulk, S.bulk_buf, LLM_BULK_BYTES);
    story_arena_begin(&S.fast, "llm-fast");
    story_arena_begin(&S.bulk, "llm-bulk");

    llm_blobs_t blobs = {.model = S.model, .model_len = S.model_len,
                         .tok = S.tok, .tok_len = S.tok_len};
    llm_params_t params = llm_default_params();
    t0 = story_time_us();
    if (!llm_load(&S.llm, &blobs, &params, &S.fast, &S.bulk)) {
        LISA_LOGE(LOG_TAG, "llm_load failed");
        goto err;
    }
    S.loaded = true;
    LISA_LOGI(LOG_TAG, "llm_load ok in %lld ms", (long long)((story_time_us() - t0) / 1000));
    story_arena_log(&S.fast, "fast");
    story_arena_log(&S.bulk, "bulk");
    story_mem_log("after flash load");
    return 0;
err:
    llm_app_unload();
    return -1;
}
