/**
 * @file arcs_zig.h
 * @brief ARCS SDK Zig Adapter - C 接口声明
 *
 * 包含 Zig 库导出的所有 C-callable 函数声明。
 */

#ifndef ARCS_ZIG_H
#define ARCS_ZIG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Zig Hello World 入口函数
 *
 * 初始化 Zig 日志系统并输出测试信息。
 *
 * @return 0 成功, -1 失败
 */
int zig_hello_main(void);

#ifdef __cplusplus
}
#endif

#endif /* ARCS_ZIG_H */
