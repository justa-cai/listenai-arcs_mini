/* Zig on ARCS SDK - Hello World 测试
 *
 * 验证 Zig 语言代码可以编译、链接并在 ARCS SoC 上运行。
 * C 端作为入口, 调用 Zig 导出的函数。
 */

#define LOG_TAG "zig_test"

#include <lisa_log.h>
#include <stdio.h>

/* Zig 导出的函数 */
extern int zig_hello_main(void);

int main(int argc, char **argv)
{
    LOGI("=== Zig Language Support Test ===");
    LOGI("Calling zig_hello_main()...");

    int ret = zig_hello_main();

    if (ret == 0) {
        LOGI("Zig function returned successfully (ret=%d)", ret);
        LOGI("=== Zig Test PASSED ===");
    } else {
        LOGE("Zig function failed (ret=%d)", ret);
        LOGE("=== Zig Test FAILED ===");
    }

    return 0;
}
