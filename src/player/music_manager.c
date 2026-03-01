#define TAG "music_mgr"

#include "music_manager.h"
#include "audio_player.h"
#include "audio_url.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "player/audio_out.h"
#include "player/tts_player.h"
#include "lisa_http.h"
#include "lisa_time.h"
#include "play_mode.h"
#include "listen_audiomgr.h"
#include "assistant_controller.h"
#include "audio/app_player.h"
#include "display/lv_img_net_loader.h"

// 从 Kconfig 配置获取服务器地址
#ifndef CONFIG_MY_CLOUD_HOST
#define CONFIG_MY_CLOUD_HOST "192.168.1.100"
#endif
#define MUSIC_SERVER_BASE_URL "http://" CONFIG_MY_CLOUD_HOST ":9100"
#define MUSIC_API_SEARCH "/api/search"
#define MUSIC_API_RANDOM "/api/random"
#define MUSIC_API_RANDOM "/api/random"
#define MUSIC_API_DOWNLOAD_PATTERN "%s/api/download/%d"
#define MUSIC_API_TIMEOUT_MS (10000)
#define HTTP_RESPONSE_BUFFER_SIZE (4096)

typedef struct {
    char *buffer;
    size_t buffer_size;
    size_t total_len;
} http_response_context_t;

static char to_hex(char c) {
    return "0123456789ABCDEF"[c & 0x0F];
}

static void url_encode(const char *src, char *dst, size_t dst_size) {
    size_t i = 0;
    size_t j = 0;
    
    while (src[i] != '\0' && j < dst_size - 1) {
        unsigned char c = (unsigned char)src[i];
        
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            dst[j++] = c;
        }
        else if (c == ' ') {
            dst[j++] = '+';
        }
        else {
            if (j + 3 >= dst_size) {
                break;
            }
            dst[j++] = '%';
            dst[j++] = to_hex(c >> 4);
            dst[j++] = to_hex(c);
        }
        i++;
    }
    dst[j] = '\0';
}

static music_manager_state_t g_music_mgr = {
    .is_active = false,
    .source = MUSIC_SOURCE_NONE,
    .auto_next = true,
    .last_keyword = {0}
};

extern audioplayer_t *get_audio_player(void);
extern tts_player_t *get_tts_player(void);

static void http_data_callback(lisa_http_data_t *data)
{
    http_response_context_t *ctx = (http_response_context_t *)data->user;

    if (!ctx || !data->buf || !ctx->buffer) {
        return;
    }

    size_t copy_len = data->len;
    size_t new_total = ctx->total_len + copy_len;

    if (new_total + 1 >= ctx->buffer_size) {
        size_t new_size = ctx->buffer_size * 2;
        while (new_size < new_total + 1) {
            new_size *= 2;
        }

        char *new_buffer = (char *)lisa_mem_realloc(ctx->buffer, new_size);
        if (!new_buffer) {
            return;
        }

        ctx->buffer = new_buffer;
        ctx->buffer_size = new_size;
    }

    memcpy(ctx->buffer + ctx->total_len, data->buf, copy_len);
    ctx->total_len = new_total;
    ctx->buffer[ctx->total_len] = '\0';
}

static int get_random_music_from_server(audio_out_t *audio_out)
{
    lisa_http_t *http = NULL;
    lisa_http_request_t req = {0};
    http_response_context_t response_ctx = {0};
    int ret = -1;
    cJSON *root = NULL;

    response_ctx.buffer_size = HTTP_RESPONSE_BUFFER_SIZE;
    response_ctx.buffer = (char *)lisa_mem_alloc(response_ctx.buffer_size);
    if (!response_ctx.buffer) {
        return -1;
    }
    response_ctx.total_len = 0;

    char url_buf[256];
    int url_len = snprintf(url_buf, sizeof(url_buf), "%s%s",
                                  MUSIC_SERVER_BASE_URL, MUSIC_API_RANDOM);

    if (url_len <= 0 || url_len >= sizeof(url_buf)) {
        lisa_mem_free(response_ctx.buffer);
        return -1;
    }

    url_buf[sizeof(url_buf) - 1] = '\0';

    memset(&req, 0, sizeof(req));
    req.method = LISA_HTTP_GET;
    req.url = (uint8_t *)url_buf;
    req.timeout = MUSIC_API_TIMEOUT_MS;
    req.user = &response_ctx;
    req.on_data = http_data_callback;
    req.headers = NULL;
    req.body = NULL;
    req.body_len = 0;

    http = lisa_http_init(&req);
    if (!http) {
        goto cleanup;
    }

    lisa_http_err_e http_err = lisa_http_perform(http);
    if (http_err != LISA_HTTP_OK) {
        goto cleanup;
    }

    if (response_ctx.total_len == 0 || response_ctx.total_len >= response_ctx.buffer_size) {
        goto cleanup;
    }

    response_ctx.buffer[response_ctx.total_len] = '\0';

    root = cJSON_Parse(response_ctx.buffer);
    if (!root) {
        goto cleanup;
    }

    cJSON *id_item = cJSON_GetObjectItem(root, "id");
    if (!id_item || !cJSON_IsNumber(id_item)) {
        goto cleanup;
    }

    int song_id = cJSON_GetNumberValue(id_item);
    if (song_id <= 0) {
        goto cleanup;
    }

    snprintf(audio_out->m_url, AUIDO_OUT_URL_LEN, MUSIC_API_DOWNLOAD_PATTERN,
             MUSIC_SERVER_BASE_URL, song_id);
    audio_out->m_url[AUIDO_OUT_URL_LEN - 1] = '\0';

    cJSON *name_item = cJSON_GetObjectItem(root, "name");
    if (name_item && cJSON_IsString(name_item)) {
        const char *name_str = cJSON_GetStringValue(name_item);
        if (name_str) {
            int src_idx = 0, dst_idx = 0;
            while (name_str[src_idx] && dst_idx < AUIDO_OUT_NAME_LEN - 1) {
                char c = name_str[src_idx];
                if (c != '[' && c != ']' && c != '"') {
                    audio_out->m_name[dst_idx++] = c;
                }
                src_idx++;
            }
            audio_out->m_name[dst_idx] = '\0';
        }
    } else {
        strncpy(audio_out->m_name, "Random Music", AUIDO_OUT_NAME_LEN - 1);
        audio_out->m_name[AUIDO_OUT_NAME_LEN - 1] = '\0';
    }

    cJSON *image_item = cJSON_GetObjectItem(root, "image");
    if (image_item && cJSON_IsString(image_item)) {
        const char *image_str = cJSON_GetStringValue(image_item);
        if (image_str) {
            strncpy(audio_out->m_image_url, image_str, AUIDO_OUT_IMAGE_URL_LEN - 1);
            audio_out->m_image_url[AUIDO_OUT_IMAGE_URL_LEN - 1] = '\0';

            LISA_LOGI(TAG, "Loading music cover image synchronously: %s", image_str);
            audio_out->m_image_dsc = lv_img_net_load(image_str);
            if (audio_out->m_image_dsc) {
                LISA_LOGI(TAG, "Music cover image loaded successfully");
            } else {
                LISA_LOGE(TAG, "Failed to load music cover image: %s", image_str);
            }
        } else {
            audio_out->m_image_url[0] = '\0';
            audio_out->m_image_dsc = NULL;
        }
    } else {
        audio_out->m_image_url[0] = '\0';
        audio_out->m_image_dsc = NULL;
    }

    audio_out->mid[0] = '\0';
    audio_out->m_artist[0] = '\0';
    audio_out->m_all_rate[0] = '\0';
    audio_out->throw_time = 0;

    ret = 0;

cleanup:
    if (root) {
        cJSON_Delete(root);
    }
    if (http) {
        lisa_http_cleanup(http);
    }
    if (response_ctx.buffer) {
        lisa_mem_free(response_ctx.buffer);
    }

    return ret;
}

static int _search_by_keyword(const char *keyword, audio_out_t *audio_out)
{
    lisa_http_t *http = NULL;
    lisa_http_request_t req = {0};
    http_response_context_t response_ctx = {0};
    cJSON *root = NULL;

    LISA_LOGI(TAG, "_search_by_keyword ENTRY: keyword='%s'", keyword);

    response_ctx.buffer_size = HTTP_RESPONSE_BUFFER_SIZE;
    response_ctx.buffer = (char *)lisa_mem_alloc(response_ctx.buffer_size);
    if (!response_ctx.buffer) {
        LISA_LOGE(TAG, "_search_by_keyword: Failed to allocate buffer");
        return -1;
    }
    response_ctx.total_len = 0;

    char url_buf[512];
    char encoded_keyword[256];
    url_encode(keyword, encoded_keyword, sizeof(encoded_keyword));
    
    int url_len = snprintf(url_buf, sizeof(url_buf), "%s%s?q=%s",
                                  MUSIC_SERVER_BASE_URL, MUSIC_API_SEARCH, encoded_keyword);

    LISA_LOGI(TAG, "Searching by keyword: %s (url: %s)", keyword, url_buf);

    if (url_len <= 0 || url_len >= sizeof(url_buf)) {
        LISA_LOGE(TAG, "_search_by_keyword: URL length invalid");
        lisa_mem_free(response_ctx.buffer);
        return -1;
    }

    url_buf[sizeof(url_buf) - 1] = '\0';

    memset(&req, 0, sizeof(req));
    req.method = LISA_HTTP_GET;
    req.url = (uint8_t *)url_buf;
    req.timeout = MUSIC_API_TIMEOUT_MS;
    req.user = &response_ctx;
    req.on_data = http_data_callback;
    req.headers = NULL;
    req.body = NULL;
    req.body_len = 0;

    http = lisa_http_init(&req);
    if (!http) {
        LISA_LOGE(TAG, "_search_by_keyword: HTTP init failed");
        lisa_mem_free(response_ctx.buffer);
        return -1;
    }

    lisa_http_err_e http_err = lisa_http_perform(http);
    if (http_err != LISA_HTTP_OK) {
        LISA_LOGE(TAG, "_search_by_keyword: HTTP request failed, err=%d", http_err);
        lisa_mem_free(response_ctx.buffer);
        lisa_http_cleanup(http);
        return -1;
    }

    LISA_LOGI(TAG, "_search_by_keyword: HTTP success, received %zu bytes", response_ctx.total_len);

    if (response_ctx.total_len == 0 || response_ctx.total_len >= response_ctx.buffer_size) {
        LISA_LOGE(TAG, "_search_by_keyword: Response length invalid");
        lisa_mem_free(response_ctx.buffer);
        lisa_http_cleanup(http);
        return -1;
    }

    response_ctx.buffer[response_ctx.total_len] = '\0';

    root = cJSON_Parse(response_ctx.buffer);
    if (!root) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) {
            LISA_LOGE(TAG, "_search_by_keyword: JSON parse error at: %s", error_ptr);
        } else {
            LISA_LOGE(TAG, "_search_by_keyword: JSON parse error: unknown");
        }
        lisa_mem_free(response_ctx.buffer);
        lisa_http_cleanup(http);
        return -1;
    }

    LISA_LOGI(TAG, "_search_by_keyword: JSON parsed successfully");

    cJSON *files_item = cJSON_GetObjectItem(root, "files");
    if (!files_item || !cJSON_IsArray(files_item)) {
        LISA_LOGE(TAG, "_search_by_keyword: No 'files' array in search response");
        lisa_mem_free(response_ctx.buffer);
        lisa_http_cleanup(http);
        cJSON_Delete(root);
        return -1;
    }

    int file_count = cJSON_GetArraySize(files_item);
    LISA_LOGI(TAG, "_search_by_keyword: Found %d files", file_count);
    
    if (file_count <= 0) {
        LISA_LOGI(TAG, "_search_by_keyword: Search returned no results for keyword: %s", keyword);
        lisa_mem_free(response_ctx.buffer);
        lisa_http_cleanup(http);
        cJSON_Delete(root);
        return -1;
    }

    static unsigned int random_seed = 0;
    random_seed++;
    int random_index = random_seed % file_count;
    LISA_LOGI(TAG, "_search_by_keyword: Picking random index %d of %d", random_index, file_count);
    
    cJSON *file = cJSON_GetArrayItem(files_item, random_index);
    if (!file) {
        LISA_LOGE(TAG, "_search_by_keyword: Failed to get file at index %d", random_index);
        lisa_mem_free(response_ctx.buffer);
        lisa_http_cleanup(http);
        cJSON_Delete(root);
        return -1;
    }

    cJSON *id_item = cJSON_GetObjectItem(file, "id");
    cJSON *name_item = cJSON_GetObjectItem(file, "name");
    
    if (!id_item || !cJSON_IsNumber(id_item)) {
        LISA_LOGE(TAG, "_search_by_keyword: No valid 'id' in file");
        lisa_mem_free(response_ctx.buffer);
        lisa_http_cleanup(http);
        cJSON_Delete(root);
        return -1;
    }

    int song_id = cJSON_GetNumberValue(id_item);
    if (song_id <= 0) {
        LISA_LOGE(TAG, "_search_by_keyword: Invalid song_id: %d", song_id);
        lisa_mem_free(response_ctx.buffer);
        lisa_http_cleanup(http);
        cJSON_Delete(root);
        return -1;
    }

    snprintf(audio_out->m_url, AUIDO_OUT_URL_LEN, MUSIC_API_DOWNLOAD_PATTERN,
             MUSIC_SERVER_BASE_URL, song_id);
    audio_out->m_url[AUIDO_OUT_URL_LEN - 1] = '\0';

    LISA_LOGI(TAG, "_search_by_keyword: Download URL: %s (song_id=%d)", audio_out->m_url, song_id);

    if (name_item && cJSON_IsString(name_item)) {
        const char *name_str = cJSON_GetStringValue(name_item);
        if (name_str) {
            int src_idx = 0, dst_idx = 0;
            while (name_str[src_idx] && dst_idx < AUIDO_OUT_NAME_LEN - 1) {
                char c = name_str[src_idx];
                if (c != '[' && c != ']' && c != '"') {
                    audio_out->m_name[dst_idx++] = c;
                }
                src_idx++;
            }
            audio_out->m_name[dst_idx] = '\0';
        }
    } else {
        strncpy(audio_out->m_name, "Random Music", AUIDO_OUT_NAME_LEN - 1);
        audio_out->m_name[AUIDO_OUT_NAME_LEN - 1] = '\0';
    }

    cJSON *image_item = cJSON_GetObjectItem(file, "image");
    if (image_item && cJSON_IsString(image_item)) {
        const char *image_str = cJSON_GetStringValue(image_item);
        if (image_str) {
            strncpy(audio_out->m_image_url, image_str, AUIDO_OUT_IMAGE_URL_LEN - 1);
            audio_out->m_image_url[AUIDO_OUT_IMAGE_URL_LEN - 1] = '\0';

            LISA_LOGI(TAG, "Loading music cover image synchronously: %s", image_str);
            audio_out->m_image_dsc = lv_img_net_load(image_str);
            if (audio_out->m_image_dsc) {
                LISA_LOGI(TAG, "Music cover image loaded successfully");
            } else {
                LISA_LOGE(TAG, "Failed to load music cover image: %s", image_str);
            }
        } else {
            audio_out->m_image_url[0] = '\0';
            audio_out->m_image_dsc = NULL;
        }
    } else {
        audio_out->m_image_url[0] = '\0';
        audio_out->m_image_dsc = NULL;
    }

    audio_out->mid[0] = '\0';
    audio_out->m_artist[0] = '\0';
    audio_out->m_all_rate[0] = '\0';
    audio_out->throw_time = 0;

    lisa_mem_free(response_ctx.buffer);
    lisa_http_cleanup(http);
    cJSON_Delete(root);
    
    LISA_LOGI(TAG, "_search_by_keyword SUCCESS: Found song by keyword: %s (index %d of %d)", 
              audio_out->m_name, random_index, file_count);
    return 0;
}

static int _get_random_by_keyword(const char *keyword, audio_out_t *audio_out)
{
    LISA_LOGI(TAG, "_get_random_by_keyword ENTRY: keyword='%s'", keyword);
    
    int ret = _search_by_keyword(keyword, audio_out);
    if (ret == 0) {
        LISA_LOGI(TAG, "_get_random_by_keyword: search SUCCESS");
        return 0;
    }
    
    LISA_LOGI(TAG, "_get_random_by_keyword: search failed (ret=%d), falling back to random", ret);
    ret = get_random_music_from_server(audio_out);
    LISA_LOGI(TAG, "_get_random_by_keyword: random result=%d", ret);
    return ret;
}

void music_manager_init(void)
{
    memset(&g_music_mgr, 0, sizeof(g_music_mgr));
    g_music_mgr.auto_next = true;
    g_music_mgr.last_keyword[0] = '\0';
    LISA_LOGI(TAG, "Music manager initialized");
}

void music_manager_set_active(bool active)
{
    g_music_mgr.is_active = active;
    LISA_LOGI(TAG, "Music manager active: %d", active);
}

void music_manager_set_source(music_source_t source)
{
    g_music_mgr.source = source;
    LISA_LOGI(TAG, "Music manager source: %d", source);
}

void music_manager_set_auto_next(bool enable)
{
    g_music_mgr.auto_next = enable;
    LISA_LOGI(TAG, "Music manager auto_next: %d", enable);
}

void music_manager_clear_keyword(void)
{
    g_music_mgr.last_keyword[0] = '\0';
    LISA_LOGI(TAG, "Music manager keyword cleared");
}

bool music_manager_is_active(void)
{
    return g_music_mgr.is_active;
}

music_source_t music_manager_get_source(void)
{
    return g_music_mgr.source;
}

int music_manager_play_next_random(char *song_name, int name_len)
{
    LISA_LOGI(TAG, "music_manager_play_next_random ENTRY: active=%d, auto_next=%d", 
              g_music_mgr.is_active, g_music_mgr.auto_next);
    
    if (!g_music_mgr.is_active || !g_music_mgr.auto_next) {
        LISA_LOGI(TAG, "Music manager not active or auto_next disabled");
        return -1;
    }

    audioplayer_t *player = get_audio_player();
    if (!player) {
        LISA_LOGE(TAG, "Failed to get audio player");
        return -1;
    }

    audio_out_t *audios = (audio_out_t *)lisa_mem_calloc(1, sizeof(audio_out_t));
    if (!audios) {
        LISA_LOGE(TAG, "Failed to allocate audio_out");
        return -1;
    }

    int ret = get_random_music_from_server(audios);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to get random music, ret=%d", ret);
        lisa_mem_free(audios);
        return -1;
    }

    if (song_name && name_len > 0) {
        strncpy(song_name, audios->m_name, name_len - 1);
        song_name[name_len - 1] = '\0';
    }

    LISA_LOGI(TAG, "Auto-playing next: %s", audios->m_name);

    if (player->on_directive) {
        LISA_LOGI(TAG, "Calling on_directive for play_next_random");
        player->on_directive(player, AUDIO_PLAY, audios, 1);
    } else {
        LISA_LOGE(TAG, "on_directive not available");
        lisa_mem_free(audios);
        return -1;
    }

    return 0;
}

int music_manager_fetch_and_play_random(char *song_name, int name_len)
{
    LISA_LOGI(TAG, "music_manager_fetch_and_play_random ENTRY");
    
    music_manager_set_active(true);
    music_manager_set_source(MUSIC_SOURCE_ONLINE);
    music_manager_clear_keyword();

    audioplayer_t *player = get_audio_player();
    if (!player) {
        LISA_LOGE(TAG, "Failed to get audio player");
        return -1;
    }

    audio_out_t *audios = (audio_out_t *)lisa_mem_calloc(1, sizeof(audio_out_t));
    if (!audios) {
        LISA_LOGE(TAG, "Failed to allocate audio_out");
        return -1;
    }

    if (get_random_music_from_server(audios) != 0) {
        LISA_LOGE(TAG, "Failed to get random music");
        lisa_mem_free(audios);
        return -1;
    }

    if (song_name && name_len > 0) {
        strncpy(song_name, audios->m_name, name_len - 1);
        song_name[name_len - 1] = '\0';
    }

    LISA_LOGI(TAG, "Playing random music: %s", audios->m_name);

    if (player->on_directive) {
        LISA_LOGI(TAG, "Calling on_directive for fetch_and_play_random");
        player->on_directive(player, AUDIO_PLAY, audios, 1);
    } else {
        LISA_LOGE(TAG, "on_directive not available");
        lisa_mem_free(audios);
        return -1;
    }

    return 0;
}

int music_manager_search_and_play(const char *keyword, char *song_name, int name_len)
{
    LISA_LOGI(TAG, "music_manager_search_and_play ENTRY: keyword='%s'", keyword);
    
    music_manager_set_active(true);
    music_manager_set_source(MUSIC_SOURCE_ONLINE);

    if (!keyword || strlen(keyword) == 0) {
        LISA_LOGI(TAG, "No keyword provided for search");
        return -1;
    }

    LISA_LOGI(TAG, "Searching and playing: %s", keyword);
    
    strncpy(g_music_mgr.last_keyword, keyword, sizeof(g_music_mgr.last_keyword) - 1);
    
    audioplayer_t *player = get_audio_player();
    if (!player) {
        LISA_LOGE(TAG, "Failed to get audio player");
        return -1;
    }

    audio_out_t *audios = (audio_out_t *)lisa_mem_calloc(1, sizeof(audio_out_t));
    if (!audios) {
        LISA_LOGE(TAG, "Failed to allocate audio_out");
        return -1;
    }

    int ret = _get_random_by_keyword(keyword, audios);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to get random music by keyword, ret=%d", ret);
        lisa_mem_free(audios);
        return -1;
    }

    if (song_name && name_len > 0) {
        strncpy(song_name, audios->m_name, name_len - 1);
        song_name[name_len - 1] = '\0';
    }

    LISA_LOGI(TAG, "Playing song by keyword: %s", audios->m_name);

    LISA_LOGI(TAG, "Player state before on_directive: state=%d", player->m_player_state);
    if (player->on_directive) {
        LISA_LOGI(TAG, "Calling on_directive for search result");
        player->on_directive(player, AUDIO_PLAY, audios, 1);
    } else {
        LISA_LOGE(TAG, "on_directive not available");
        lisa_mem_free(audios);
        return -1;
    }

    return 0;
}
