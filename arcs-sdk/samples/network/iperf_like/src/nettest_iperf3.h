/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * iperf3 协议客户端实现（支持上行 / 下行 / 双向）
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nettest_result.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 使用 iperf3 协议执行一轮 TCP 吞吐测试
 *
 * 协议流程：
 *   1. 建立控制连接到 server:port
 *   2. 发送 37 字节 cookie
 *   3. PARAM_EXCHANGE: 发送 JSON 测试参数
 *   4. CREATE_STREAMS: 建立数据流连接（发送相同 cookie）
 *   5. TEST_START → TEST_RUNNING: 按模式执行发送 / 接收
 *   6. TEST_END → EXCHANGE_RESULTS: JSON 结果交换
 *   7. IPERF_DONE: 清理关闭
 *
 * @param enabled  外部使能标志，设为 false 可中止测试
 * @param result   输出测试结果
 * @return 0 成功，-1 失败
 */
int nettest_runner_iperf3_run(nettest_mode_t mode,
                              volatile bool *enabled,
                              nettest_round_result_t *result);

/* 中止正在进行的 iperf3 测试 */
void nettest_runner_iperf3_abort(void);

#ifdef __cplusplus
}
#endif
