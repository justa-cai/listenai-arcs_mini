/*
 * miniapp_builtin.c - 开机自动启动内置 Lua 应用（luaapp flash 分区）
 *
 * 脚本随固件烧录：tools/romfs.py 把 <脚本>.lua 打成 ROMFS 镜像，经
 * partition_table.json 的 "luaapp" 分区（0x580000，respak 与 app 之间
 * 的空隙）随整包下发。上电延迟 MINIAPP_BUILTIN_DELAY_MS 后挂载，把
 * /radio.lua 以 id "local:radio" 安装启动 —— 与 adb push 完全同一
 * 路径：运行期 adb push 同名脚本即可热替换，重启后回到内置版本。
 *
 * 语音会话进行中不抢屏（等下一轮重试）；miniapp 已在运行（例如本开机
 * 周期内被 adb 推过别的应用）则放弃。
 */
#define TAG "miniapp"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lisa_log.h"
#include "mbedtls/md5.h"

#include "romfs.h"
#include "miniapp.h"

#define BUILTIN_LUAAPP_ADDR ((const void *)0x35800000u)  /* flash 0x580000 */
#define BUILTIN_LUAAPP_SIZE (0x80000u)                   /* 512 KB */
#define BUILTIN_ENTRY       "/radio.lua"
#define BUILTIN_ID          "local:radio"
#define BUILTIN_STACK       4096

extern bool voice_cloud_is_session_active(void);
extern bool voice_cloud_is_uploading_audio(void);

static void builtin_task(void *arg)
{
    (void)arg;
    const uint32_t delay_ms = CONFIG_MINIAPP_BUILTIN_DELAY_MS;
    uint32_t waited = 0;

    /* 等系统就绪：先睡满基础延迟，之后若语音在忙则按秒续等（上限 60s） */
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    while (waited < 60000u &&
           (voice_cloud_is_session_active() || voice_cloud_is_uploading_audio())) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        waited += 1000u;
    }
    if (miniapp_is_active()) {
        LISA_LOGI(TAG, "builtin app skipped: a miniapp is already running");
        vTaskDelete(NULL);
        return;
    }

    struct romfs *fs = NULL;
    if (romfs_init(&fs, BUILTIN_LUAAPP_ADDR, BUILTIN_LUAAPP_SIZE) != 0) {
        LISA_LOGW(TAG, "builtin app: luaapp partition not a romfs (not flashed?)");
        vTaskDelete(NULL);
        return;
    }
    uint8_t *data = NULL;
    uint32_t size = 0;
    if (romfs_info_get(fs, BUILTIN_ENTRY, &data, &size) != 0 || !data || size == 0) {
        LISA_LOGW(TAG, "builtin app: %s missing", BUILTIN_ENTRY);
        romfs_deinit(&fs);
        vTaskDelete(NULL);
        return;
    }
    if (size > CONFIG_MINIAPP_SOURCE_MAX_BYTES) {
        LISA_LOGW(TAG, "builtin app: source %u over limit %u",
                  (unsigned)size, (unsigned)CONFIG_MINIAPP_SOURCE_MAX_BYTES);
        romfs_deinit(&fs);
        vTaskDelete(NULL);
        return;
    }

    miniapp_package_t pkg = {0};
    snprintf(pkg.id, sizeof(pkg.id), "%s", BUILTIN_ID);
    pkg.size = size;
    uint8_t digest[16];
    const char hex[] = "0123456789abcdef";
    mbedtls_md5((const uint8_t *)data, size, digest);
    for (unsigned i = 0; i < sizeof(digest); ++i) {
        pkg.hash[2 * i] = hex[digest[i] >> 4];
        pkg.hash[2 * i + 1] = hex[digest[i] & 15];
    }
    pkg.hash[32] = '\0';
    snprintf(pkg.version, sizeof(pkg.version), "%s", pkg.hash);

    char error[128] = {0};
    int rc = miniapp_install(&pkg, (const char *)data, error, sizeof(error));
    LISA_LOGI(TAG, "builtin app %s (%u bytes)",
              rc == 0 ? "started" : "failed", (unsigned)size);
    romfs_deinit(&fs);
    vTaskDelete(NULL);
}

void miniapp_builtin_init(void)
{
    if (xTaskCreate(builtin_task, "miniapp.boot", BUILTIN_STACK / 4u, NULL,
                    4, NULL) != pdPASS) {
        LISA_LOGE(TAG, "builtin app task create failed");
    }
}
