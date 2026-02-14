#define TAG "LV_IMG_NET_LOADER"

#include "lv_img_net_loader.h"
#include "HTTPCUsr_api.h"
#include "lisa_log.h"
#include <string.h>
#include <stdio.h>

#define NET_IMG_TIMEOUT_SEC (15)
#define NET_IMG_MAX_SIZE (512 * 1024)  // 512KB max image size

typedef struct {
    lv_img_dsc_t img_dsc;
    uint8_t* data;
    char* url;
} net_img_cache_t;

// Simple cache for loaded images
#define NET_IMG_CACHE_SIZE 5
static net_img_cache_t img_cache[NET_IMG_CACHE_SIZE];
static int cache_initialized = 0;

static bool is_valid_image_url(const char* url);
static int download_image_data(const char* url, uint8_t** data, uint32_t* size);
static int find_cache_slot(const char* url);
static int find_empty_cache_slot(void);
static int find_lru_cache_slot(void);
static void clear_cache_slot(int slot);

static bool is_valid_image_url(const char* url)
{
    if (!url || strlen(url) == 0) {
        return false;
    }
    
    // Remove N: prefix if present
    if (strncmp(url, "N:", 2) == 0) {
        url += 2;
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

static int download_image_data(const char* url, uint8_t** data, uint32_t* size)
{
    HTTPParameters* http_params = NULL;
    HTTP_CLIENT http_info = {0};
    int ret = -1;
    uint8_t* buffer = NULL;
    uint32_t total_downloaded = 0;
    
    // Remove N: prefix if present
    if (strncmp(url, "N:", 2) == 0) {
        url += 2;
    }
    
    LOGI("Downloading image from: %s", url);
    
    // Allocate HTTP parameters
    http_params = (HTTPParameters*)lv_mem_alloc(sizeof(HTTPParameters));
    if (!http_params) {
        LOGE("Failed to allocate HTTP parameters");
        return -1;
    }
    memset(http_params, 0, sizeof(HTTPParameters));
    
    // Setup HTTP request
    strncpy(http_params->Uri, url, HTTP_CLIENT_MAX_URL_LENGTH - 1);
    http_params->HttpVerb = VerbGet;
    http_params->nTimeout = NET_IMG_TIMEOUT_SEC;
    
    // Open connection
    ret = HTTPC_open(http_params);
    if (ret != 0) {
        LOGE("HTTPC_open failed: %d", ret);
        goto cleanup;
    }
    
    // Send request
    ret = HTTPC_request(http_params, NULL);
    if (ret != 0) {
        LOGE("HTTPC_request failed: %d", ret);
        goto cleanup;
    }
    
    // Get response info
    ret = HTTPC_get_request_info(http_params, &http_info);
    if (ret != 0) {
        LOGE("HTTPC_get_request_info failed: %d", ret);
        goto cleanup;
    }
    
    uint32_t content_length = http_info.TotalResponseBodyLength;
    LOGI("Image size: %u bytes, status: %u", content_length, http_info.HTTPStatusCode);
    
    if (http_info.HTTPStatusCode != 200) {
        LOGE("HTTP error: %u", http_info.HTTPStatusCode);
        ret = -1;
        goto cleanup;
    }
    
    if (content_length == 0 || content_length > NET_IMG_MAX_SIZE) {
        LOGE("Invalid image size: %u", content_length);
        ret = -1;
        goto cleanup;
    }
    
    // Allocate buffer for image data
    buffer = (uint8_t*)lv_mem_alloc(content_length);
    if (!buffer) {
        LOGE("Failed to allocate %u bytes for image data", content_length);
        ret = -1;
        goto cleanup;
    }
    
    // Download image data
    uint8_t* write_ptr = buffer;
    uint32_t remaining = content_length;
    
    while (remaining > 0) {
        UINT32 received = 0;
        uint32_t to_read = (remaining > 4096) ? 4096 : remaining;
        
        ret = HTTPC_read(http_params, write_ptr, to_read, &received);
        if (ret != 0 && ret != HTTP_CLIENT_EOS) {
            LOGE("HTTPC_read failed: %d (downloaded %u/%u)", ret, total_downloaded, content_length);
            break;
        }
        
        if (received == 0) {
            LOGW("Received 0 bytes, stopping download");
            break;
        }
        
        write_ptr += received;
        total_downloaded += received;
        remaining -= received;
        
        // Log progress for large images
        if (content_length > 50000) {
            uint32_t progress = (total_downloaded * 100) / content_length;
            static uint32_t last_progress = 0;
            if (progress >= last_progress + 20) {
                LOGI("Download progress: %u%% (%u/%u bytes)", progress, total_downloaded, content_length);
                last_progress = progress;
            }
        }
        
        if (ret == HTTP_CLIENT_EOS) {
            break;
        }
    }
    
    if (total_downloaded == content_length) {
        *data = buffer;
        *size = total_downloaded;
        buffer = NULL; // Don't free in cleanup
        ret = 0;
        LOGI("Image download completed: %u bytes", total_downloaded);
    } else {
        LOGE("Download incomplete: %u/%u bytes", total_downloaded, content_length);
        ret = -1;
    }
    
cleanup:
    if (http_params) {
        HTTPC_close(http_params);
        lv_mem_free(http_params);
    }
    
    if (buffer) {
        lv_mem_free(buffer);
    }
    
    return ret;
}

static int find_cache_slot(const char* url)
{
    if (!cache_initialized) return -1;
    
    for (int i = 0; i < NET_IMG_CACHE_SIZE; i++) {
        if (img_cache[i].url && strcmp(img_cache[i].url, url) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_empty_cache_slot(void)
{
    for (int i = 0; i < NET_IMG_CACHE_SIZE; i++) {
        if (img_cache[i].url == NULL) {
            return i;
        }
    }
    return -1;
}

static int find_lru_cache_slot(void)
{
    // For simplicity, just use slot 0 as LRU
    return 0;
}

static void clear_cache_slot(int slot)
{
    if (slot < 0 || slot >= NET_IMG_CACHE_SIZE) return;
    
    if (img_cache[slot].data) {
        lv_mem_free(img_cache[slot].data);
        img_cache[slot].data = NULL;
    }
    
    if (img_cache[slot].url) {
        lv_mem_free(img_cache[slot].url);
        img_cache[slot].url = NULL;
    }
    
    memset(&img_cache[slot].img_dsc, 0, sizeof(lv_img_dsc_t));
}

void lv_img_net_loader_init(void)
{
    if (cache_initialized) return;
    
    memset(img_cache, 0, sizeof(img_cache));
    cache_initialized = 1;
    
    LOGI("Network image loader initialized");
}

lv_img_dsc_t* lv_img_net_load(const char* url)
{
    if (!url || !is_valid_image_url(url)) {
        LOGE("Invalid URL: %s", url ? url : "NULL");
        return NULL;
    }
    
    if (!cache_initialized) {
        lv_img_net_loader_init();
    }
    
    // Check cache first
    int cache_slot = find_cache_slot(url);
    if (cache_slot >= 0) {
        LOGI("Using cached image: %s", url);
        return &img_cache[cache_slot].img_dsc;
    }
    
    // Download image
    uint8_t* img_data = NULL;
    uint32_t img_size = 0;
    
    if (download_image_data(url, &img_data, &img_size) != 0) {
        LOGE("Failed to download image: %s", url);
        return NULL;
    }
    
    // Find cache slot
    cache_slot = find_empty_cache_slot();
    if (cache_slot < 0) {
        cache_slot = find_lru_cache_slot();
        clear_cache_slot(cache_slot);
    }
    
    // Store in cache
    img_cache[cache_slot].data = img_data;
    img_cache[cache_slot].url = (char*)lv_mem_alloc(strlen(url) + 1);
    if (img_cache[cache_slot].url) {
        strcpy(img_cache[cache_slot].url, url);
    }
    
    // Setup image descriptor
    img_cache[cache_slot].img_dsc.header.always_zero = 0;
    img_cache[cache_slot].img_dsc.data_size = img_size;
    img_cache[cache_slot].img_dsc.data = img_data;
    
    // Determine image format from file signature
    // 注意：设置正确的 cf 类型后，LVGL 会自动解析图片头获取 w/h
    if (img_size >= 8 && memcmp(img_data, "\x89PNG\r\n\x1a\n", 8) == 0) {
        // PNG 格式 - LVGL 会自动解析 PNG 头
        img_cache[cache_slot].img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
        img_cache[cache_slot].img_dsc.header.w = 0;  // LVGL 会自动从 PNG 头读取
        img_cache[cache_slot].img_dsc.header.h = 0;
        LOGI("Detected PNG format");
    }
    else if (img_size >= 2 && img_data[0] == 0xFF && img_data[1] == 0xD8) {
        // JPEG 格式 - LVGL 会自动解析 JPEG 头
        img_cache[cache_slot].img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
        img_cache[cache_slot].img_dsc.header.w = 0;  // LVGL 会自动从 JPEG 头读取
        img_cache[cache_slot].img_dsc.header.h = 0;
        LOGI("Detected JPEG format");
    }
    else {
        // 未知格式，作为 RAW 处理（但需要手动设置宽高，否则显示会有问题）
        img_cache[cache_slot].img_dsc.header.cf = LV_IMG_CF_RAW;
        img_cache[cache_slot].img_dsc.header.w = 0;
        img_cache[cache_slot].img_dsc.header.h = 0;
        LOGW("Unknown image format, treated as RAW");
    }
    
    LOGI("Cached image: %s (%u bytes, slot %d)", url, img_size, cache_slot);
    
    return &img_cache[cache_slot].img_dsc;
}

void lv_img_net_free(lv_img_dsc_t* img_dsc)
{
    if (!img_dsc) return;
    
    // Find the cache slot
    for (int i = 0; i < NET_IMG_CACHE_SIZE; i++) {
        if (&img_cache[i].img_dsc == img_dsc) {
            LOGI("Freeing cached image: %s", img_cache[i].url ? img_cache[i].url : "unknown");
            clear_cache_slot(i);
            return;
        }
    }
    
    LOGW("Image descriptor not found in cache");
}

lv_res_t lv_img_set_src_net(lv_obj_t* img, const char* url)
{
    if (!img || !url) {
        LOGE("Invalid parameters: img=%p, url=%s", img, url ? url : "NULL");
        return LV_RES_INV;
    }
    
    lv_img_dsc_t* img_dsc = lv_img_net_load(url);
    if (!img_dsc) {
        LOGE("Failed to load network image: %s", url);
        return LV_RES_INV;
    }
    
    lv_img_set_src(img, img_dsc);
    LOGI("Set image source: %s", url);
    
    return LV_RES_OK;
}