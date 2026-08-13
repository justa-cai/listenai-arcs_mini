/* Zig on ARCS SDK - Blinky Demo
 *
 * 验证 Zig 代码可以通过设备框架操控 GPIO 硬件。
 * C 端作为入口, 调用 Zig 导出的 blinky 函数。
 */

#define LOG_TAG "zig_blinky"

#include <lisa_log.h>

/* Zig 导出的函数 */
extern int zig_blinky_main(void);

int main(int argc, char **argv)
{
    LOGI("=== Zig Blinky Demo ===");
    LOGI("Calling zig_blinky_main()...");

    int ret = zig_blinky_main();

    if (ret == 0) {
        LOGI("Zig blinky returned successfully (ret=%d)", ret);
        LOGI("=== Zig Blinky PASSED ===");
    } else {
        LOGE("Zig blinky failed (ret=%d)", ret);
        LOGE("=== Zig Blinky FAILED ===");
    }

    return 0;
}
