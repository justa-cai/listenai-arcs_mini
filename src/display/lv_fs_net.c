#define TAG "LV_FS_NET"

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "HTTPCUsr_api.h"

#include "lv_fs_net.h"
#include "lisa_log.h"
#include "lvgl.h"

#define LV_FS_NET_ENABLE_CACHE_MODE (1)

#define NET_FS_HTTP_TIMEOUT_SEC (10)

#ifdef LV_FS_NET_ENABLE_CACHE_MODE
#define NET_FS_CACHE_CHUNK_SIZE   (4096)
#define NET_FS_GLOBAL_CACHE_COUNT (2)
#endif

typedef struct {
    char url[HTTP_CLIENT_MAX_URL_LENGTH];
    HTTPParameters *http_params;
    HTTP_CLIENT http_info;
    uint32_t position;
    uint32_t bytes_read;
    bool connection_active;
    bool eof_reached;

#ifdef LV_FS_NET_ENABLE_CACHE_MODE
    uint8_t *cache_buffer;
    uint32_t cache_size;
    uint32_t cache_position;
    bool owns_cache;
#endif
} net_file_t;

#ifdef LV_FS_NET_ENABLE_CACHE_MODE
typedef struct {
    char url[HTTP_CLIENT_MAX_URL_LENGTH];
    uint8_t *cache_buffer;
    uint32_t cache_size;
    uint32_t last_access_time;
    bool in_use;
} global_cache_entry_t;

static global_cache_entry_t g_cache_pool[NET_FS_GLOBAL_CACHE_COUNT];
static uint32_t g_cache_access_counter = 0;
#endif

static bool is_valid_url(const char *url);
static bool fs_ready_cb(lv_fs_drv_t *drv);
static void *fs_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode);
static lv_fs_res_t fs_close_cb(lv_fs_drv_t *drv, void *file_p);
static lv_fs_res_t fs_read_cb(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br);
static lv_fs_res_t fs_seek_cb(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence);
static lv_fs_res_t fs_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p);

static int http_connect(net_file_t *net_file, uint32_t start_position);
static void http_disconnect(net_file_t *net_file);
static int http_reconnect_at_position(net_file_t *net_file, uint32_t position);

#ifdef LV_FS_NET_ENABLE_CACHE_MODE
static int download_entire_file(net_file_t *net_file);
static global_cache_entry_t *find_cache_entry(const char *url);
static global_cache_entry_t *find_lru_cache_entry(void);
static void store_to_global_cache(const char *url, uint8_t *buffer, uint32_t size);
static void init_global_cache(void);
#endif

static lv_fs_drv_t fs_drv;

void lv_fs_net_init(void)
{
    lv_fs_drv_init(&fs_drv);

    fs_drv.letter = 'N';
    fs_drv.cache_size = 0;
    fs_drv.ready_cb = fs_ready_cb;
    fs_drv.open_cb = fs_open_cb;
    fs_drv.close_cb = fs_close_cb;
    fs_drv.read_cb = fs_read_cb;
    fs_drv.seek_cb = fs_seek_cb;
    fs_drv.tell_cb = fs_tell_cb;
    fs_drv.dir_open_cb = NULL;
    fs_drv.dir_read_cb = NULL;
    fs_drv.dir_close_cb = NULL;

    lv_fs_drv_register(&fs_drv);

#ifdef LV_FS_NET_ENABLE_CACHE_MODE
    init_global_cache();
#endif

    LOGI("Network file system driver registered with letter 'N'");
}

static bool is_valid_url(const char *url)
{
    if (url == NULL || strlen(url) == 0) {
        return false;
    }

    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
        LOGE("URL must start with http:// or https://");
        return false;
    }

    size_t url_len = strlen(url);
    if (url_len < 10 || url_len > 2048) {
        LOGE("URL length invalid: %zu", url_len);
        return false;
    }

    return true;
}

static bool fs_ready_cb(lv_fs_drv_t *drv)
{
    return true;
}

static int http_connect(net_file_t *net_file, uint32_t start_position)
{
    int ret;

    if (net_file->connection_active) {
        LOGW("Connection already active");
        return 0;
    }

    if (net_file->http_params == NULL) {
        net_file->http_params = (HTTPParameters *)lv_mem_alloc(sizeof(HTTPParameters));
        if (net_file->http_params == NULL) {
            LOGE("Failed to allocate HTTP parameters");
            return -1;
        }
        memset(net_file->http_params, 0, sizeof(HTTPParameters));
    }

    strncpy(net_file->http_params->Uri, net_file->url, HTTP_CLIENT_MAX_URL_LENGTH - 1);
    net_file->http_params->HttpVerb = VerbGet;
    net_file->http_params->nTimeout = NET_FS_HTTP_TIMEOUT_SEC;

    ret = HTTPC_open(net_file->http_params);
    if (ret != 0) {
        LOGE("HTTPC_open failed: %d", ret);
        return -1;
    }

    char *range_header = NULL;
    if (start_position > 0) {
        range_header = (char *)lv_mem_alloc(128);
        if (range_header != NULL) {
            snprintf(range_header, 128, "Range: bytes=%u-\r\n", start_position);
            LOGD("Using Range header: %s", range_header);
        }
    }

    ret = HTTPC_request(net_file->http_params, (HTTP_CLIENT_GET_HEADER)range_header);
    if (range_header != NULL) {
        lv_mem_free(range_header);
    }

    if (ret != 0) {
        LOGE("HTTPC_request failed: %d", ret);
        HTTPC_close(net_file->http_params);
        return -1;
    }

    ret = HTTPC_get_request_info(net_file->http_params, &net_file->http_info);
    if (ret != 0) {
        LOGE("HTTPC_get_request_info failed: %d", ret);
        HTTPC_close(net_file->http_params);
        return -1;
    }

    net_file->connection_active = true;
    net_file->position = start_position;
    net_file->bytes_read = 0;

    LOGI("HTTP connected: file_size=%u, status=%u, start_pos=%u", net_file->http_info.TotalResponseBodyLength,
         net_file->http_info.HTTPStatusCode, start_position);

    return 0;
}

static void http_disconnect(net_file_t *net_file)
{
    if (net_file->connection_active && net_file->http_params != NULL) {
        HTTPC_close(net_file->http_params);
        net_file->connection_active = false;
        LOGD("HTTP disconnected");
    }
}

static int http_reconnect_at_position(net_file_t *net_file, uint32_t position)
{
    LOGD("Reconnecting at position %u", position);
    http_disconnect(net_file);
    return http_connect(net_file, position);
}

#ifdef LV_FS_NET_ENABLE_CACHE_MODE

static void init_global_cache(void)
{
    memset(g_cache_pool, 0, sizeof(g_cache_pool));
    g_cache_access_counter = 0;
    LOGI("Global cache initialized: %d slots", NET_FS_GLOBAL_CACHE_COUNT);
}

static global_cache_entry_t *find_cache_entry(const char *url)
{
    for (int i = 0; i < NET_FS_GLOBAL_CACHE_COUNT; i++) {
        if (g_cache_pool[i].cache_buffer != NULL && strcmp(g_cache_pool[i].url, url) == 0) {
            g_cache_pool[i].last_access_time = ++g_cache_access_counter;
            LOGI("Cache HIT: %s (slot %d, size=%u)", url, i, g_cache_pool[i].cache_size);
            return &g_cache_pool[i];
        }
    }
    LOGD("Cache MISS: %s", url);
    return NULL;
}

static global_cache_entry_t *find_lru_cache_entry(void)
{
    global_cache_entry_t *lru_entry = &g_cache_pool[0];
    uint32_t min_access_time = g_cache_pool[0].last_access_time;

    for (int i = 1; i < NET_FS_GLOBAL_CACHE_COUNT; i++) {
        if (g_cache_pool[i].cache_buffer == NULL) {
            LOGD("Found empty cache slot %d", i);
            return &g_cache_pool[i];
        }

        if (g_cache_pool[i].in_use) {
            continue;
        }

        if (g_cache_pool[i].last_access_time < min_access_time) {
            min_access_time = g_cache_pool[i].last_access_time;
            lru_entry = &g_cache_pool[i];
        }
    }

    if (lru_entry->cache_buffer != NULL) {
        LOGI("Evicting cache: %s (size=%u)", lru_entry->url, lru_entry->cache_size);
        lv_mem_free(lru_entry->cache_buffer);
        lru_entry->cache_buffer = NULL;
    }

    return lru_entry;
}

static void store_to_global_cache(const char *url, uint8_t *buffer, uint32_t size)
{
    global_cache_entry_t *entry = find_lru_cache_entry();
    strncpy(entry->url, url, HTTP_CLIENT_MAX_URL_LENGTH - 1);
    entry->url[HTTP_CLIENT_MAX_URL_LENGTH - 1] = '\0';
    entry->cache_buffer = buffer;
    entry->cache_size = size;
    entry->last_access_time = ++g_cache_access_counter;
    entry->in_use = true;

    LOGI("Stored to global cache: %s (size=%u, slot=%ld)", url, size, entry - g_cache_pool);
}

static int download_entire_file(net_file_t *net_file)
{
    if (net_file == NULL || !net_file->connection_active) {
        LOGE("Invalid net_file or connection not active");
        return -1;
    }

    uint32_t file_size = net_file->http_info.TotalResponseBodyLength;
    if (file_size == 0) {
        LOGE("File size is 0, cannot download");
        return -1;
    }

    LOGI("Starting full file download: %u bytes from %s", file_size, net_file->url);

    net_file->cache_buffer = (uint8_t *)lv_mem_alloc(file_size);
    if (net_file->cache_buffer == NULL) {
        LOGE("Failed to allocate %u bytes for cache buffer", file_size);
        return -1;
    }

    uint32_t total_downloaded = 0;
    uint8_t *write_ptr = net_file->cache_buffer;

    while (total_downloaded < file_size) {
        uint32_t bytes_to_read = (file_size - total_downloaded > NET_FS_CACHE_CHUNK_SIZE)
                                     ? NET_FS_CACHE_CHUNK_SIZE
                                     : (file_size - total_downloaded);

        UINT32 received = 0;
        int ret = HTTPC_read(net_file->http_params, write_ptr, bytes_to_read, &received);

        if (ret != 0 && ret != HTTP_CLIENT_EOS) {
            LOGE("HTTPC_read failed during cache download: %d (downloaded %u/%u bytes)", ret, total_downloaded,
                 file_size);
            lv_mem_free(net_file->cache_buffer);
            net_file->cache_buffer = NULL;
            return -1;
        }

        if (received == 0) {
            if (total_downloaded < file_size) {
                LOGW("Download incomplete: got %u/%u bytes", total_downloaded, file_size);
            }
            break;
        }

        write_ptr += received;
        total_downloaded += received;

        static uint32_t last_progress = 0;
        uint32_t progress = (total_downloaded * 100) / file_size;
        if (progress >= last_progress + 10 || total_downloaded == file_size) {
            LOGI("Download progress: %u%% (%u/%u bytes)", progress, total_downloaded, file_size);
            last_progress = progress;
        }

        if (ret == HTTP_CLIENT_EOS) {
            LOGD("Reached end of stream");
            break;
        }
    }

    net_file->cache_size = total_downloaded;
    net_file->cache_position = 0;
    LOGH("net_file->cache_buffer", net_file->cache_buffer, 10);
    LOGI("File download completed: %u/%u bytes cached", total_downloaded, file_size);

    http_disconnect(net_file);

    return (total_downloaded == file_size) ? 0 : -1;
}
#endif // LV_FS_NET_ENABLE_CACHE_MODE

static void *fs_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    if (!(mode & LV_FS_MODE_RD)) {
        LOGE("Network FS only supports read mode");
        return NULL;
    }

    if (!is_valid_url(path)) {
        LOGE("Invalid URL: %s", path);
        return NULL;
    }

#ifdef LV_FS_NET_ENABLE_CACHE_MODE
    LOGI("Opening network file: %s (cache_mode=enabled)", path);
#else
    LOGI("Opening network file: %s (cache_mode=disabled)", path);
#endif

    net_file_t *net_file = (net_file_t *)lv_mem_alloc(sizeof(net_file_t));
    if (net_file == NULL) {
        LOGE("Failed to allocate net_file_t");
        return NULL;
    }
    memset(net_file, 0, sizeof(net_file_t));

    strncpy(net_file->url, path, HTTP_CLIENT_MAX_URL_LENGTH - 1);
    net_file->url[HTTP_CLIENT_MAX_URL_LENGTH - 1] = '\0';

    net_file->http_params = NULL;
    net_file->position = 0;
    net_file->bytes_read = 0;
    net_file->connection_active = false;
    net_file->eof_reached = false;

#ifdef LV_FS_NET_ENABLE_CACHE_MODE
    net_file->cache_buffer = NULL;
    net_file->cache_size = 0;
    net_file->cache_position = 0;
    net_file->owns_cache = false;

    global_cache_entry_t *cached = find_cache_entry(path);
    if (cached != NULL) {
        net_file->cache_buffer = cached->cache_buffer;
        net_file->cache_size = cached->cache_size;
        net_file->cache_position = 0;
        net_file->owns_cache = false;

        LOGI("Using cached file (size=%u bytes)", net_file->cache_size);

        return (void *)net_file;
    }
#endif

    if (http_connect(net_file, 0) != 0) {
        LOGE("Failed to connect to HTTP server");
        lv_mem_free(net_file);
        return NULL;
    }

    LOGI("Opened network file (size=%u bytes)", net_file->http_info.TotalResponseBodyLength);

#ifdef LV_FS_NET_ENABLE_CACHE_MODE
    LOGI("Cache mode enabled, downloading entire file...");
    if (download_entire_file(net_file) != 0) {
        LOGE("Failed to download file to cache");
        http_disconnect(net_file);
        if (net_file->http_params != NULL) {
            lv_mem_free(net_file->http_params);
        }
        lv_mem_free(net_file);
        return NULL;
    }

    net_file->owns_cache = false;
    store_to_global_cache(path, net_file->cache_buffer, net_file->cache_size);

    LOGI("File successfully cached, total %u bytes", net_file->cache_size);
#endif

    return (void *)net_file;
}

static lv_fs_res_t fs_close_cb(lv_fs_drv_t *drv, void *file_p)
{
    if (file_p == NULL) {
        LOGE("Attempted to close NULL file pointer");
        return LV_FS_RES_INV_PARAM;
    }

    net_file_t *net_file = (net_file_t *)file_p;

    LOGD("Closing network file: %s", net_file->url);

    http_disconnect(net_file);

    if (net_file->http_params != NULL) {
        lv_mem_free(net_file->http_params);
    }

#ifdef LV_FS_NET_ENABLE_CACHE_MODE
    global_cache_entry_t *cached = find_cache_entry(net_file->url);
    if (cached != NULL) {
        cached->in_use = false;
        LOGD("Released cache entry (keeping data): %s", net_file->url);
    }

#endif

    lv_mem_free(net_file);

    LOGD("Network file closed successfully");

    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read_cb(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    if (file_p == NULL || buf == NULL) {
        LOGE("Invalid parameters: file_p=%p, buf=%p", file_p, buf);
        if (br != NULL) {
            *br = 0;
        }
        return LV_FS_RES_INV_PARAM;
    }

    net_file_t *net_file = (net_file_t *)file_p;

#ifdef LV_FS_NET_ENABLE_CACHE_MODE
    if (net_file->cache_position >= net_file->cache_size) {
        if (br != NULL) {
            *br = 0;
        }
        LOGD("EOF reached in cache (position: %u/%u)", net_file->cache_position, net_file->cache_size);
        return LV_FS_RES_OK;
    }

    uint32_t available = net_file->cache_size - net_file->cache_position;
    uint32_t to_read = (btr < available) ? btr : available;

    memcpy(buf, net_file->cache_buffer + net_file->cache_position, to_read);
    net_file->cache_position += to_read;

    if (br != NULL) {
        *br = to_read;
    }

    LOGD("Read from cache: %u/%u bytes (position: %u/%u)", to_read, btr, net_file->cache_position,
         net_file->cache_size);

    return LV_FS_RES_OK;

#else
    if (net_file->eof_reached) {
        if (br != NULL) {
            *br = 0;
        }
        LOGD("EOF already reached");
        return LV_FS_RES_OK;
    }

    if (!net_file->connection_active) {
        LOGE("HTTP connection not active");
        if (br != NULL) {
            *br = 0;
        }
        return LV_FS_RES_UNKNOWN;
    }

    UINT32 received = 0;
    int ret = HTTPC_read(net_file->http_params, buf, btr, &received);

    if (ret != 0) {
        if (ret == HTTP_CLIENT_EOS || received == 0) {
            LOGD("End of stream reached");
            net_file->eof_reached = true;
        } else {
            LOGE("HTTPC_read failed: %d", ret);
        }
    }

    net_file->position += received;
    net_file->bytes_read += received;

    if (br != NULL) {
        *br = received;
    }

    LOGD("Read from stream: %u/%u bytes (position: %u, total_read: %u/%u)", received, btr, net_file->position,
         net_file->bytes_read, net_file->http_info.TotalResponseBodyLength);

    return LV_FS_RES_OK;
#endif
}

static lv_fs_res_t fs_seek_cb(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    if (file_p == NULL) {
        LOGE("Invalid file pointer");
        return LV_FS_RES_INV_PARAM;
    }

    net_file_t *net_file = (net_file_t *)file_p;
    uint32_t new_position;
    uint32_t old_position;
    uint32_t file_size;

#ifdef LV_FS_NET_ENABLE_CACHE_MODE
    old_position = net_file->cache_position;
    file_size = net_file->cache_size;
#else
    old_position = net_file->position;
    file_size = net_file->http_info.TotalResponseBodyLength;
#endif

    switch (whence) {
    case LV_FS_SEEK_SET:
        new_position = pos;
        break;

    case LV_FS_SEEK_CUR:
        new_position = old_position + pos;
        break;

    case LV_FS_SEEK_END:
        if (pos > file_size) {
            LOGE("Seek offset exceeds file size: %u > %u", pos, file_size);
            return LV_FS_RES_INV_PARAM;
        }
        new_position = file_size - pos;
        break;

    default:
        LOGE("Invalid whence parameter: %d", whence);
        return LV_FS_RES_INV_PARAM;
    }

    if (new_position > file_size) {
        LOGW("Seek position beyond file size, clamping");
        new_position = file_size;
    }

#ifdef LV_FS_NET_ENABLE_CACHE_MODE
    net_file->cache_position = new_position;
    LOGD("Cache seek from %u to %u (whence=%d, offset=%u)", old_position, new_position, whence, pos);
    return LV_FS_RES_OK;

#else
    if (new_position != net_file->position) {
        LOGD("Stream seek requires reconnect: %u -> %u", old_position, new_position);

        if (http_reconnect_at_position(net_file, new_position) != 0) {
            LOGE("Failed to reconnect at position %u", new_position);
            return LV_FS_RES_UNKNOWN;
        }

        net_file->eof_reached = false;
    }

    LOGD("Stream seek from %u to %u (whence=%d, offset=%u)", old_position, new_position, whence, pos);

    return LV_FS_RES_OK;
#endif
}

static lv_fs_res_t fs_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    if (file_p == NULL || pos_p == NULL) {
        return LV_FS_RES_INV_PARAM;
    }

    net_file_t *net_file = (net_file_t *)file_p;

#ifdef LV_FS_NET_ENABLE_CACHE_MODE
    *pos_p = net_file->cache_position;
#else
    *pos_p = net_file->position;
#endif

    return LV_FS_RES_OK;
}
