/* Zig on ARCS SDK - Audio Play Demo
 *
 * 验证 Zig 代码可以通过音频设备框架播放 PCM 音频。
 * Zig 端在编译时生成正弦波 PCM 数据，运行时通过 Audio HAL 播放。
 *
 * 播放内容:
 *   - 0.8s 1kHz 正弦波 (高音嘟)
 *   - 0.2s 静音
 *   - 0.8s 500Hz 正弦波 (低音嘟)
 *   - 0.2s 静音
 */

#define LOG_TAG "zig_audio"

#include <lisa_log.h>

/* Zig 导出的函数 */
extern int zig_audio_play_main(void);

int main(int argc, char **argv)
{
    LOGI("=== Zig Audio Play Demo ===");
    LOGI("Calling zig_audio_play_main()...");

    int ret = zig_audio_play_main();

    if (ret == 0) {
        LOGI("Zig audio play returned successfully (ret=%d)", ret);
        LOGI("=== Zig Audio Play PASSED ===");
    } else {
        LOGE("Zig audio play failed (ret=%d)", ret);
        LOGE("=== Zig Audio Play FAILED ===");
    }

    return 0;
}
