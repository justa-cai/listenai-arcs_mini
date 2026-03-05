
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#define TAG "app.datas"

#include "sys_init.h"
#include "app_datas.h"
#include "lisa_log.h"
#include "lisa_mem.h"

#include "kv.h"
#include "lisa_kv.h"

#include "FreeRTOS.h"
#include "task.h"
#include "romfs.h"
#include "ini.h"

#define DEFAULT_PRODUCT_ID             CONFIG_CLOUD_PRODUCT_ID_DEFAULT
#define DEFAULT_SECRET_ID              CONFIG_CLOUD_SECRET_ID_DEFAULT

#if CONFIG_CLOUD_FULL_DUPLEX_ENABLE
#define DEFAULT_FULL_DUPLEX            (1)
#define DEFAULT_FULL_DUPLEX_TIMEOUT_MS (CONFIG_CLOUD_FULL_DUPLEX_TIMEOUT_MS)
#else
#define DEFAULT_FULL_DUPLEX            (0)
#define DEFAULT_FULL_DUPLEX_TIMEOUT_MS (0)
#endif

#if CONFIG_CLOUD_STAGING_MODE_ENABLE
#define DEFAULT_DEVICE_MODE           (DEVICE_MODE_STAGING)
#else
#define DEFAULT_DEVICE_MODE           (DEVICE_MODE_PROD)
#endif

static struct app_datas *g_app_datas = NULL;
static TaskHandle_t g_app_datas_init_task = NULL;
static struct romfs *romfs = NULL;

#define STR_EMPTY(s) (s == NULL || strlen(s) == 0)

typedef struct {
    struct app_datas *app_data;
} ini_parse_context_t;

#define MAX_WAKEUP_KEYWORDS_NUM 10

static char *wakeup_keywords[MAX_WAKEUP_KEYWORDS_NUM] = {
    "xiao ao tong xue",
    "ni hao xiao ao",
    "xiao ling xiao ling",
};

static int ini_config_handler(void* user, const char* section, const char* name, const char* value)
{
    ini_parse_context_t *ctx = (ini_parse_context_t *)user;
    struct app_datas *app_data = ctx->app_data;

    if (strcmp(section, "cloud") == 0) {
        if (strcmp(name, "ws-host") == 0) {
            strncpy(app_data->host, value, sizeof(app_data->host) - 1);
            app_data->host[sizeof(app_data->host) - 1] = '\0';
        } else if (strcmp(name, "ws-host-staging") == 0) {
            strncpy(app_data->host_staging, value, sizeof(app_data->host_staging) - 1);
            app_data->host_staging[sizeof(app_data->host_staging) - 1] = '\0';
        } else if (strcmp(name, "auth-url") == 0) {
            strncpy(app_data->token_url, value, sizeof(app_data->token_url) - 1);
            app_data->token_url[sizeof(app_data->token_url) - 1] = '\0';
        } else if (strcmp(name, "auth-url-staging") == 0) {
            strncpy(app_data->token_url_staging, value, sizeof(app_data->token_url_staging) - 1);
            app_data->token_url_staging[sizeof(app_data->token_url_staging) - 1] = '\0';
        } else if (strcmp(name, "ws-host-integration") == 0) {
            strncpy(app_data->host_integration, value, sizeof(app_data->host_integration) - 1);
            app_data->host_integration[sizeof(app_data->host_integration) - 1] = '\0';
        } else if (strcmp(name, "auth-url-integration") == 0) {
            strncpy(app_data->token_url_integration, value, sizeof(app_data->token_url_integration) - 1);
            app_data->token_url_integration[sizeof(app_data->token_url_integration) - 1] = '\0';
        } else if (strcmp(name, "pid") == 0) {
            strncpy(app_data->pid, value, sizeof(app_data->pid) - 1);
            app_data->pid[sizeof(app_data->pid) - 1] = '\0';
        } else if (strcmp(name, "sid") == 0) {
            strncpy(app_data->sid, value, sizeof(app_data->sid) - 1);
            app_data->sid[sizeof(app_data->sid) - 1] = '\0';
        } else if (strcmp(name, "one-shot") == 0) {
            if (strcmp(value, "false") == 0) {
                app_data->oneshot = 0;
            }
        }
    } else if (strcmp(section, "wakeup") == 0) {
        if (strcmp(name, "keyword") == 0) {
            static uint8_t keyword_num = 0;
            if (keyword_num < MAX_WAKEUP_KEYWORDS_NUM) {
                wakeup_keywords[keyword_num] = psram_malloc(strlen(value) + 1);
                strcpy(wakeup_keywords[keyword_num], value);
                keyword_num++;
            }
        } else if (strcmp(name, "prompt") == 0) {
            strncpy(app_data->wakeup_prompt, value, sizeof(app_data->wakeup_prompt) - 1);
            app_data->wakeup_prompt[sizeof(app_data->wakeup_prompt) - 1] = '\0';
        }
    }

    return 1;
}

struct app_datas *get_app_datas(void)
{
    assert(g_app_datas_init_task);

    if (g_app_datas_init_task != xTaskGetCurrentTaskHandle()) {
        LOGW("Attempting to access app data in a dangerous thread. Caller: %p, current thread:%s, safety thread: %s",
             __builtin_return_address(0), pcTaskGetName(xTaskGetCurrentTaskHandle()),
             pcTaskGetName(g_app_datas_init_task));
    }

    return g_app_datas;
}

static const char *device_id_str_get(void)
{
    static char id_buffer_str[17] = {0};
    uint8_t id_buffer[8] = {0};

    uint32_t *id_1 = (uint32_t *)0x48600208;
    uint32_t *id_2 = (uint32_t *)0x4860020c;
    char *device_id = NULL;
    int r;

    if (id_buffer_str[0] != '\0') {
        return id_buffer_str;
    }

    if (*id_1 == 0 && *id_2 == 0) {
        id_buffer_str[0] = '\0';
    } else {
        memcpy(id_buffer, id_1, sizeof(uint32_t));
        memcpy(id_buffer + 4, id_2, sizeof(uint32_t));
        sprintf(id_buffer_str, "%02x%02x%02x%02x%02x%02x%02x%02x", id_buffer[0], id_buffer[1], id_buffer[2],
                id_buffer[3], id_buffer[4], id_buffer[5], id_buffer[6], id_buffer[7]);
    }

    return id_buffer_str;
}

static void app_datas_load_from_romfs(void)
{
    int r;

    r = romfs_init(&romfs, CONFIG_ROMFS_IMAGE_ADDR, CONFIG_ROMFS_IMAGE_SIZE);
    if (r) {
        LOGW("romfs init failed");
        return;
    }
    const char *locale = "zh-CN";
    char *temp_buf = NULL;
    char config_path[64] = {0};

    r = lisa_kv_get_string("user.locale", &temp_buf);
    if (r == 0 && temp_buf) {
        if (strcmp(temp_buf, "en-GB") == 0) {
            locale = "en-GB";
        }
        lisa_kv_free(temp_buf);
    }

    snprintf(config_path, sizeof(config_path), "%s/config.ini", locale);

    uint32_t size;
    uint8_t *data;
    r = romfs_info_get(romfs, config_path, &data, &size);
    if (r) {
        LOGW("failed to load config from romfs: %s", config_path);
        return;
    }

    ini_parse_context_t ctx = {
        .app_data = g_app_datas,
    };
    r = ini_parse_string_length((const char *)data, size, ini_config_handler, &ctx);
    if (r != 0) {
        LOGW("ini parse failed: %d", r);
        return;
    }

    LOGI("loaded default config from romfs: %s", config_path);
}

static void app_datas_load_from_lisa_kv(void)
{
    int r;
    char *temp_buf = NULL;

    r = lisa_kv_get_string(KV_KEY_USER_PID, &temp_buf);
    if (r == 0) {
        strcpy(g_app_datas->pid, temp_buf);
        lisa_kv_free(temp_buf);
        LOGI("use pid from kv system");
    }

    r = lisa_kv_get_string(KV_KEY_USER_SID, &temp_buf);
    if (r == 0) {
        strcpy(g_app_datas->sid, temp_buf);
        lisa_kv_free(temp_buf);
        LOGI("use sid from kv system");
    }

    r = lisa_kv_get_string(KV_KEY_USER_DEVICE_ID, &temp_buf);
    if (r == 0) {
        strcpy(g_app_datas->did, temp_buf);
        lisa_kv_free(temp_buf);
    } else {
        strcpy(g_app_datas->did, device_id_str_get());
    }

    r = lisa_kv_get_bool(KV_KEY_FULL_DUPLEX, (bool *)&g_app_datas->full_duplex);
    if (r != 0) {
        g_app_datas->full_duplex = DEFAULT_FULL_DUPLEX;
    }

    int timeout_ms = 0;
    r = lisa_kv_get_int(KV_KEY_FULL_DUPLEX_TIMEOUT_MS, &timeout_ms);
    if (r != 0) {
        timeout_ms = DEFAULT_FULL_DUPLEX_TIMEOUT_MS;
    }
    g_app_datas->full_duplex_timeout_ms = timeout_ms;

    int device_mode = DEFAULT_DEVICE_MODE;
    r = lisa_kv_get_int(KV_KEY_DEVICE_MODE, &device_mode);
    if (r != 0) {
        device_mode = DEFAULT_DEVICE_MODE;
    }
    if (device_mode < DEVICE_MODE_PROD || device_mode > DEVICE_MODE_INTEGRATION) {
        device_mode = DEFAULT_DEVICE_MODE;
    }
    g_app_datas->device_mode = (uint8_t)device_mode;

    int work_mode = -1;

    r = lisa_kv_get_int(KV_KEY_WAKEUP_MODE, &work_mode);
    if (r) {
        work_mode = VOICE_WORK_MODE_VOICE_WAKEUP;
    }
    g_app_datas->voice_work_mode = work_mode;
}

static void app_datas_load_from_default(void)
{
    if (STR_EMPTY(g_app_datas->pid)) {
        LOGI("use pid from hard code");
        strcpy(g_app_datas->pid, DEFAULT_PRODUCT_ID);
    }

    if (STR_EMPTY(g_app_datas->sid)) {
        LOGI("use sid from hard code");
        strcpy(g_app_datas->sid, DEFAULT_SECRET_ID);
    }
}

int app_datas_init(void)
{
    g_app_datas_init_task = xTaskGetCurrentTaskHandle();
    assert(g_app_datas_init_task != NULL);

    g_app_datas = (struct app_datas *)lisa_mem_alloc(sizeof(struct app_datas));
    assert(g_app_datas != NULL);
    memset(g_app_datas, 0, sizeof(struct app_datas));

    g_app_datas->oneshot = 1;
#ifdef CONFIG_OTA
    strcpy(g_app_datas->wakeup_prompt, "请通过\"#唤醒词#\"唤醒我");
#else
    strcpy(g_app_datas->wakeup_prompt, "请通过\"小聆小聆\"唤醒我");
#endif

    app_datas_load_from_romfs();
    app_datas_load_from_lisa_kv();
    app_datas_load_from_default();

    assert(strlen(g_app_datas->pid) > 0);
    assert(strlen(g_app_datas->sid) > 0);
    assert(strlen(g_app_datas->did) > 0);

    int pid_len = strlen(g_app_datas->pid);
    int sid_len = strlen(g_app_datas->sid);
    if (pid_len > 8) {
        LOGI("PID: %.4s****%.4s", g_app_datas->pid, g_app_datas->pid + pid_len - 4);
    } else {
        LOGI("PID: %s", g_app_datas->pid);
    }
    if (sid_len > 8) {
        LOGI("SID: %.4s****%.4s", g_app_datas->sid, g_app_datas->sid + sid_len - 4);
    } else {
        LOGI("SID: %s", g_app_datas->sid);
    }

    g_app_datas->can_wakeup = 1;

    LOGI("did: %s", g_app_datas->did);
    LOGI("full_duplex: %d", g_app_datas->full_duplex);
    LOGI("full_duplex_timeout_ms: %d", g_app_datas->full_duplex_timeout_ms);
    LOGI("device_mode: %d", g_app_datas->device_mode);
    LOGI("voice_work_mode: 0x%02x", g_app_datas->voice_work_mode);
    LOGI("oneshot: %d", g_app_datas->oneshot);
    LOGI("host: %s", g_app_datas->host);
    LOGI("host_staging: %s", g_app_datas->host_staging);
    LOGI("host_integration: %s", g_app_datas->host_integration);
    LOGI("token_url: %s", g_app_datas->token_url);
    LOGI("token_url_staging: %s", g_app_datas->token_url_staging);
    LOGI("token_url_integration: %s", g_app_datas->token_url_integration);
    LOGI("music_active_url: %s", g_app_datas->music_active_url);
    LOGI("music_tranlink_url: %s", g_app_datas->music_tranlink_url);
    LOGI("wakeup_prompt: %s", g_app_datas->wakeup_prompt);
    LOGI("port: %s", g_app_datas->port);
    LOGI("scheme: %s", g_app_datas->scheme);

    for (int i = 0; i < MAX_WAKEUP_KEYWORDS_NUM; i++) {
        LOGI("wakeup keyword[%d]: %s", i, wakeup_keywords[i] ? wakeup_keywords[i] : "null");
    }


    return 0;
}

bool is_wakeup_keyword(char *keyword)
{
#ifdef CONFIG_BOARD_ARCS_MINI
    return true;
#else
    for (int i = 0; i < MAX_WAKEUP_KEYWORDS_NUM; i++) {
        if (wakeup_keywords[i] == NULL) {
            break;
        }

        if (strcmp(wakeup_keywords[i], keyword) == 0) {
            return true;
        }
    }

    return false;
#endif
}
