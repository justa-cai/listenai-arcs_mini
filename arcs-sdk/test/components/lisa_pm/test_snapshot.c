/*
 * lisa_pm 测试 — snapshot case
 *
 * 把 canary 分别塞进 ILM (.itcm) 和 DLM (.dtcm)，开机一次性写入
 * magic 值。在 AUTO_LIGHT_SLEEP 主循环中跨多个深度睡眠/唤醒迭代
 * 检查 canary——任何一次 mismatch 即 TEST_ASSERT 失败。
 *
 * ILM/DLM 都不在 AON 域，深度睡眠掉电；如果 lisa_pm 的快照注册
 * 与 HAL save/restore 工作正常，唤醒后 canary 值应保持。损坏即
 * 说明快照路径有问题。
 *
 * 注：本测试无法直接把 canary 放进 .dtcm.bss——system.ld 的 .dtcm
 * 段 glob `(.dtcm .dtcm.*)` 比 .dtcm.bss 段早匹配，会把 .dtcm.bss.X
 * 这种命名抢走。`.bss` 部分能否被覆盖由 lisa_pm 的注册范围
 * `[_dtcm_data_start, _dtcm_bss_end)` 决定，属链接期可验证项；
 * 运行期只测到 DLM 即足以证明 save/restore 路径工作。
 */

#include "test_common.h"

#include <stdint.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "unity.h"

#define SNAP_ILM_MAGIC 0xCAFEBABEu
#define SNAP_DLM_MAGIC 0xDEADBEEFu

#define SNAP_ITER_TARGET 20
#define SNAP_ITER_DELAY_MS 300

__attribute__((used, section(".itcm.snap_canary")))
static volatile uint32_t s_ilm_canary;

__attribute__((used, section(".dtcm.snap_canary")))
static volatile uint32_t s_dlm_canary;

static void snap_canary_init(void)
{
    s_ilm_canary = SNAP_ILM_MAGIC;
    s_dlm_canary = SNAP_DLM_MAGIC;
    printf("[SNAP] canary placed: ilm=%p dlm=%p\n",
           (void *)&s_ilm_canary, (void *)&s_dlm_canary);
    printf("[SNAP] canary init values: ilm=0x%08x dlm=0x%08x\n",
           (unsigned)s_ilm_canary, (unsigned)s_dlm_canary);
}

/* 主测试：跨 SNAP_ITER_TARGET 个深度睡眠/唤醒循环检查 canary。
 * 系统在 vTaskDelay 期间会进入 light sleep（lisa_pm AUTO 策略 +
 * WiFi LISTEN），唤醒后继续迭代——若快照保留正常，canary 不变。 */
static void test_snapshot_canary_survives_sleep(void)
{
    snap_canary_init();

    for (uint32_t iter = 0; iter < SNAP_ITER_TARGET; ++iter) {
        printf("[SNAP] iter=%lu awake ilm=0x%08x dlm=0x%08x\n",
               (unsigned long)iter,
               (unsigned)s_ilm_canary,
               (unsigned)s_dlm_canary);

        TEST_ASSERT_EQUAL_HEX32(SNAP_ILM_MAGIC, s_ilm_canary);
        TEST_ASSERT_EQUAL_HEX32(SNAP_DLM_MAGIC, s_dlm_canary);

        vTaskDelay(pdMS_TO_TICKS(SNAP_ITER_DELAY_MS));
    }
}

/*
 * ILM 代码段保存测试: ILM snapshot 的本意 (符号名 _itcm_code_start/_end
 * 也直说) 就是保住放在 ILM 的可执行代码。数据 canary 测的是字节恒等;
 * 这里测的是"代码字节恢复后还能正确执行" — 真正的 use case 验证。
 *
 * 设计:
 *   - 两个函数分别 section(".itcm.snap_func_a") 和 ".itcm.snap_func_z",
 *     利用 linker 字典序倾向把它们摆在 ILM 不同字典序位置 (即便最终落点
 *     由 ld 决定, 两个函数都必须在 lisa_pm 注册的 ILM 范围内 — 用启动期
 *     地址 assert 防止"放别处了导致测试是 no-op"的 silent skip)
 *   - noinline + used: 防 inline 把函数体内联到 caller 让 .itcm.snap_func_*
 *     段为空; 防 GC 把无静态引用的函数干掉
 *   - 调用通过 volatile 函数指针: 防止 caller 优化为直接调用 (volatile
 *     load 必须发生, 然后通过指针 call, 真的去读 ILM 里的指令)
 *
 * 失败形态预期:
 *   - 如果 ILM snapshot 完全失效 → 函数代码烂掉 → 调用大概率 illegal
 *     instruction / hard fault 直接挂, 不是返回错值
 *   - 如果只是部分位错乱 (checksum 错位 / partial restore) → 可能返回
 *     错值, 这条情况下我们的 expected-vs-actual 断言会精确指出错位
 */
static int __attribute__((noinline, used, section(".itcm.snap_func_a")))
snap_ilm_func_a(int x) { return x * 7 + 3; }

static int __attribute__((noinline, used, section(".itcm.snap_func_z")))
snap_ilm_func_z(int x) { return (x ^ 0xDEAD) | 0x100; }

static int (* volatile s_fn_a)(int) = snap_ilm_func_a;
static int (* volatile s_fn_z)(int) = snap_ilm_func_z;

#define SNAP_FN_ITER 10

static void test_snapshot_ilm_function_executes_after_sleep(void)
{
    extern char _itcm_code_start[];
    extern char _itcm_code_end[];
    uintptr_t addr_a = (uintptr_t)snap_ilm_func_a;
    uintptr_t addr_z = (uintptr_t)snap_ilm_func_z;
    uintptr_t ilm_lo = (uintptr_t)_itcm_code_start;
    uintptr_t ilm_hi = (uintptr_t)_itcm_code_end;

    printf("[SNAP_FN] ilm=[0x%08x, 0x%08x) func_a=0x%08x func_z=0x%08x\n",
           (unsigned)ilm_lo, (unsigned)ilm_hi,
           (unsigned)addr_a, (unsigned)addr_z);

    /* 启动期 sanity: 函数必须落在 lisa_pm 注册的 ILM 范围内, 否则
     * snapshot 不覆盖它, 测试就成 no-op */
    TEST_ASSERT_TRUE(addr_a >= ilm_lo && addr_a < ilm_hi);
    TEST_ASSERT_TRUE(addr_z >= ilm_lo && addr_z < ilm_hi);

    for (int iter = 0; iter < SNAP_FN_ITER; ++iter) {
        int ra = s_fn_a(iter);
        int rz = s_fn_z(iter);
        printf("[SNAP_FN] iter=%d a(%d)=%d z(%d)=%d\n",
               iter, iter, ra, iter, rz);
        TEST_ASSERT_EQUAL_INT(iter * 7 + 3,             ra);
        TEST_ASSERT_EQUAL_INT((iter ^ 0xDEAD) | 0x100,  rz);

        vTaskDelay(pdMS_TO_TICKS(SNAP_ITER_DELAY_MS));
    }

    /* 最后一次 sleep 后再调一次, 确认最末 sleep/wake 周期后函数仍 OK */
    TEST_ASSERT_EQUAL_INT(SNAP_FN_ITER * 7 + 3,            s_fn_a(SNAP_FN_ITER));
    TEST_ASSERT_EQUAL_INT((SNAP_FN_ITER ^ 0xDEAD) | 0x100, s_fn_z(SNAP_FN_ITER));
}

/*
 * Mutation cycles 测试: snapshot 必须捕获"当前态", 不是某次性的"初始
 * 态"。前两个测试只写一次然后反复读, 严格说只证明"某个值被保下来过";
 * 万一 save 路径有 bug, 只保了一次然后后续 restore 全用同一份, 也照
 * 样过。这里每轮把 canary 写成 iter-specific 的唯一值, sleep, 读回,
 * 断言读到的就是这一轮写进去的值, 而不是上一轮的 / 初始的。
 *
 * 复用 .itcm.snap_canary / .dtcm.snap_canary (前置 case 已用过 + 通过),
 * 不另开 section, 测试间状态独立 (每轮自己写自己读)。
 */
#define SNAP_MUT_ITER 10

static void test_snapshot_mutation_cycles(void)
{
    printf("[SNAP_MUT] entry: ilm=0x%08x dlm=0x%08x\n",
           (unsigned)s_ilm_canary, (unsigned)s_dlm_canary);

    for (uint32_t iter = 0; iter < SNAP_MUT_ITER; ++iter) {
        uint32_t expected_ilm = 0xAA000000U | iter;
        uint32_t expected_dlm = 0xBB000000U | iter;

        /* 写新值, 然后睡 — sleep 路径必须把当前的 expected_* 写进 PSRAM
         * snapshot, 唤醒后再读回; 如果 save 用的是上一轮的值就会失败 */
        s_ilm_canary = expected_ilm;
        s_dlm_canary = expected_dlm;

        vTaskDelay(pdMS_TO_TICKS(SNAP_ITER_DELAY_MS));

        printf("[SNAP_MUT] iter=%lu wrote ilm=0x%08x dlm=0x%08x "
               "read ilm=0x%08x dlm=0x%08x\n",
               (unsigned long)iter,
               (unsigned)expected_ilm, (unsigned)expected_dlm,
               (unsigned)s_ilm_canary, (unsigned)s_dlm_canary);

        TEST_ASSERT_EQUAL_HEX32(expected_ilm, s_ilm_canary);
        TEST_ASSERT_EQUAL_HEX32(expected_dlm, s_dlm_canary);
    }
}

/*
 * Bulk buffer 测试: 单点 canary 只采到一个地址, 整段范围内只要保住
 * 那一个点测试就过。这里在 ILM 和 DLM 各放 128 字节 buffer 用确定性
 * pattern 填充, sleep, 逐字节 verify, 证明 lisa_pm 注册的整段范围
 * (不只是几个采样点) 都进了 save/restore。
 *
 * 同时跑 3 个不同 seed, 每轮重新填充 pattern, 隐式覆盖 bulk + mutation
 * 联合行为 (整段会随着 seed 变化, 每次 save 都得用当前的)。
 *
 * pattern 函数用 (idx * 0x37) ^ seed: 跨 idx 各字节值都不同 + 跨 seed
 * 也都不同, 避免任何字节意外恒等导致的假阳。
 */
#define SNAP_BULK_SIZE 128U
#define SNAP_BULK_SEEDS_N 3

__attribute__((used, section(".itcm.bulk_buf")))
static volatile uint8_t s_ilm_bulk[SNAP_BULK_SIZE] = {0};

__attribute__((used, section(".dtcm.bulk_buf")))
static volatile uint8_t s_dlm_bulk[SNAP_BULK_SIZE] = {0};

static uint8_t snap_bulk_pattern(uint32_t idx, uint8_t seed)
{
    return (uint8_t)((idx * 0x37U) ^ seed);
}

static void snap_bulk_fill_sleep_verify(uint8_t seed_ilm, uint8_t seed_dlm)
{
    uint32_t miss_ilm = 0;
    uint32_t miss_dlm = 0;
    int first_miss_idx_ilm = -1;
    int first_miss_idx_dlm = -1;
    uint8_t first_miss_got_ilm = 0;
    uint8_t first_miss_got_dlm = 0;

    for (uint32_t i = 0; i < SNAP_BULK_SIZE; ++i) {
        s_ilm_bulk[i] = snap_bulk_pattern(i, seed_ilm);
        s_dlm_bulk[i] = snap_bulk_pattern(i, seed_dlm);
    }

    vTaskDelay(pdMS_TO_TICKS(SNAP_ITER_DELAY_MS));

    for (uint32_t i = 0; i < SNAP_BULK_SIZE; ++i) {
        uint8_t exp_ilm = snap_bulk_pattern(i, seed_ilm);
        uint8_t exp_dlm = snap_bulk_pattern(i, seed_dlm);
        uint8_t got_ilm = s_ilm_bulk[i];
        uint8_t got_dlm = s_dlm_bulk[i];

        if (got_ilm != exp_ilm) {
            if (first_miss_idx_ilm < 0) {
                first_miss_idx_ilm = (int)i;
                first_miss_got_ilm = got_ilm;
            }
            miss_ilm++;
        }
        if (got_dlm != exp_dlm) {
            if (first_miss_idx_dlm < 0) {
                first_miss_idx_dlm = (int)i;
                first_miss_got_dlm = got_dlm;
            }
            miss_dlm++;
        }
    }

    printf("[SNAP_BULK] seed_ilm=0x%02x seed_dlm=0x%02x "
           "miss ilm=%u/%u dlm=%u/%u",
           seed_ilm, seed_dlm,
           (unsigned)miss_ilm, (unsigned)SNAP_BULK_SIZE,
           (unsigned)miss_dlm, (unsigned)SNAP_BULK_SIZE);
    if (first_miss_idx_ilm >= 0) {
        printf(" first_ilm[%d]: got 0x%02x expect 0x%02x",
               first_miss_idx_ilm,
               (unsigned)first_miss_got_ilm,
               (unsigned)snap_bulk_pattern((uint32_t)first_miss_idx_ilm, seed_ilm));
    }
    if (first_miss_idx_dlm >= 0) {
        printf(" first_dlm[%d]: got 0x%02x expect 0x%02x",
               first_miss_idx_dlm,
               (unsigned)first_miss_got_dlm,
               (unsigned)snap_bulk_pattern((uint32_t)first_miss_idx_dlm, seed_dlm));
    }
    printf("\n");

    TEST_ASSERT_EQUAL_UINT32(0, miss_ilm);
    TEST_ASSERT_EQUAL_UINT32(0, miss_dlm);
}

static void test_snapshot_bulk_buffer_survives_sleep(void)
{
    extern char _itcm_code_start[];
    extern char _itcm_code_end[];
    extern char _dtcm_data_start[];
    extern char _dtcm_bss_end[];

    uintptr_t ilm_buf = (uintptr_t)s_ilm_bulk;
    uintptr_t dlm_buf = (uintptr_t)s_dlm_bulk;
    uintptr_t ilm_lo = (uintptr_t)_itcm_code_start;
    uintptr_t ilm_hi = (uintptr_t)_itcm_code_end;
    uintptr_t dlm_lo = (uintptr_t)_dtcm_data_start;
    uintptr_t dlm_hi = (uintptr_t)_dtcm_bss_end;

    printf("[SNAP_BULK] ilm range=[0x%08x, 0x%08x) buf=[0x%08x, 0x%08x)\n",
           (unsigned)ilm_lo, (unsigned)ilm_hi,
           (unsigned)ilm_buf, (unsigned)(ilm_buf + SNAP_BULK_SIZE));
    printf("[SNAP_BULK] dlm range=[0x%08x, 0x%08x) buf=[0x%08x, 0x%08x)\n",
           (unsigned)dlm_lo, (unsigned)dlm_hi,
           (unsigned)dlm_buf, (unsigned)(dlm_buf + SNAP_BULK_SIZE));

    /* sanity: 两个 buffer 完全落在 lisa_pm 注册的范围内, 否则 snapshot
     * 不覆盖, 测试 silent no-op */
    TEST_ASSERT_TRUE(ilm_buf >= ilm_lo);
    TEST_ASSERT_TRUE(ilm_buf + SNAP_BULK_SIZE <= ilm_hi);
    TEST_ASSERT_TRUE(dlm_buf >= dlm_lo);
    TEST_ASSERT_TRUE(dlm_buf + SNAP_BULK_SIZE <= dlm_hi);

    /* 3 个 seed pair, 各自 fill→sleep→verify; ilm 和 dlm 用不同 seed
     * 避免任何"ilm 串到 dlm"的错位被两边 pattern 巧合覆盖 */
    snap_bulk_fill_sleep_verify(0x11, 0x66);
    snap_bulk_fill_sleep_verify(0x22, 0x77);
    snap_bulk_fill_sleep_verify(0x33, 0x88);
}

/*
 * .dtcm.bss 覆盖测试: 关掉原 test_snapshot.c 头部注释里承认的覆盖缺口。
 *
 * 原问题: 通过 section attribute 把 canary 直接放进 .dtcm.bss 行不通
 *   — system.ld 的 .dtcm 段 glob `* (.dtcm .dtcm.*)` 抢先匹配, 任何
 *   .dtcm.bss.X 命名都会被吸到 .dtcm (数据段) 而非 .dtcm.bss (BSS 段);
 *   而 lisa_pm 注册的范围是 [_dtcm_data_start, _dtcm_bss_end), 后半段
 *   .dtcm.bss 没有数据可测, 是覆盖空洞。
 *
 * 此处解法: 在 CMakeLists.txt 显式用
 *   listenai_code_relocate(SECTIONS .snap_dtcm_bss LOCATION DTCM_BSS)
 * 把我们自定义的 .snap_dtcm_bss 段定向到 .dtcm.bss 输出段。然后启动期
 * assert canary 地址确实落在 [_dtcm_data_end, _dtcm_bss_end) 内,
 * 防止"relocate 没生效"导致 silent no-op。
 */
__attribute__((used, section(".snap_dtcm_bss")))
static volatile uint32_t s_dtcm_bss_canary;

static void test_snapshot_dtcm_bss_canary_survives_sleep(void)
{
    extern char _dtcm_data_end[];
    extern char _dtcm_bss_end[];

    uintptr_t addr = (uintptr_t)&s_dtcm_bss_canary;
    uintptr_t bss_lo = (uintptr_t)_dtcm_data_end;
    uintptr_t bss_hi = (uintptr_t)_dtcm_bss_end;

    printf("[SNAP_BSS] canary=0x%08x .dtcm.bss range=[0x%08x, 0x%08x)\n",
           (unsigned)addr, (unsigned)bss_lo, (unsigned)bss_hi);

    /* 关键 sanity: canary 必须在 .dtcm.bss 输出段范围内
     * ([_dtcm_data_end, _dtcm_bss_end)), 而不是被吸到 .dtcm 段。
     * 失败 = listenai_code_relocate 没生效 / section 名字写错 = 本测试
     * silent no-op */
    TEST_ASSERT_TRUE(addr >= bss_lo);
    TEST_ASSERT_TRUE(addr + sizeof(s_dtcm_bss_canary) <= bss_hi);

    /* mutation pattern: 每轮唯一值, 验证 .dtcm.bss 的 save 路径
     * 同样跟当前态 */
    for (uint32_t iter = 0; iter < SNAP_MUT_ITER; ++iter) {
        uint32_t expected = 0xCC000000U | iter;
        s_dtcm_bss_canary = expected;
        vTaskDelay(pdMS_TO_TICKS(SNAP_ITER_DELAY_MS));
        printf("[SNAP_BSS] iter=%lu wrote=0x%08x read=0x%08x\n",
               (unsigned long)iter,
               (unsigned)expected, (unsigned)s_dtcm_bss_canary);
        TEST_ASSERT_EQUAL_HEX32(expected, s_dtcm_bss_canary);
    }
}

void run_snapshot_tests(void)
{
    RUN_TEST(test_snapshot_canary_survives_sleep);
    RUN_TEST(test_snapshot_ilm_function_executes_after_sleep);
    RUN_TEST(test_snapshot_mutation_cycles);
    RUN_TEST(test_snapshot_bulk_buffer_survives_sleep);
    RUN_TEST(test_snapshot_dtcm_bss_canary_survives_sleep);
}
