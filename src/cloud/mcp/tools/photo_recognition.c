#include "photo_recognition.h"
#include "aiui_mcp.h"
#include "vision_config.h"
#include "lisa_log.h"
#include "lisa_http.h"
#include "HTTPCUsr_api.h"
#include "cJSON.h"
#include "sysheap.h"
#include "video/video_camera.h"
#include "video/image_convert.h"
#include "controller/apps/data/lisaui_user_data.h"
#include "controller/view/assistant_view.h"
#include "cloud/core/lisa_aiui.h"
#include "cloud/app_cloud.h"
#include "sysutils.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>

// JPEG encoding (using libjpeg)
#include "jpeglib.h"

/* 外部获取AIUI鉴权token接口 */
extern const char *lisa_aiui_get_auth_token(void);

#define TAG "photo_recognition"

/**
 * @brief Convert HTTPS URL to HTTP URL (in-place)
 * @param url URL buffer (will be modified if starts with "https://")
 * @param max_len Maximum length of the URL buffer
 * @return 0 on success, -1 on failure
 */
static int convert_https_to_http(char *url, size_t max_len)
{
    if (!url || max_len == 0) {
        return -1;
    }

    // Check if URL starts with "https://"
    if (strncmp(url, "https://", 8) == 0) {
        // Calculate lengths
        size_t url_len = strlen(url);
        size_t new_len = url_len - 1;  // "https://" (8) -> "http://" (7), difference = 1

        if (new_len >= max_len) {
            LISA_LOGE(TAG, "URL too long after conversion");
            return -1;
        }

        memmove(url + 4, url + 5, url_len - 5 + 1);  // +1 for null terminator

        LISA_LOGI(TAG, "Converted HTTPS to HTTP: %s", url);
        return 0;
    }

    return 0;
}

// API配置
#define API_HOST "https://api.listenai.com"
#define API_UPLOAD_PATH "/v1/device/assets"
#define API_UPLOAD_URL API_HOST API_UPLOAD_PATH

// 图片尺寸使用video_camera.h中的定义

//UI display图片尺寸
#define DISPLAY_IMAGE_WIDTH  160
#define DISPLAY_IMAGE_HEIGHT 120

// 全局PSRAM缓冲区（用于UI显示，避免参数传递）
__psram_bss__ static uint16_t g_camera_image_buffer[CAMERA_IMAGE_WIDTH * CAMERA_IMAGE_HEIGHT];  // RGB565格式
static uint32_t g_camera_image_width = 0;
static uint32_t g_camera_image_height = 0;
static bool g_camera_image_ready = false;

// 全局PSRAM缓冲区（用于存放UI display的图片）
__psram_bss__ static uint16_t display_buffer[DISPLAY_IMAGE_WIDTH * DISPLAY_IMAGE_HEIGHT];

// 全局PSRAM缓冲区（用于JPEG编码）
__psram_bss__ static uint8_t g_rgb24_buffer[CAMERA_IMAGE_WIDTH * CAMERA_IMAGE_HEIGHT * 3];  // RGB24格式，用于JPEG编码

// 保存最后一次识图结果
static char g_last_recognition_result[512] = {0};

// HTTP headers全局变量（用于底层HTTP API回调）
static char *g_auth_header = NULL;
static volatile char *g_upload_headers = NULL;

/**
 * @brief Convert RGB565 to RGB24
 * @param rgb565 Input RGB565 data
 * @param rgb24 Output RGB24 data (must be pre-allocated)
 * @param width Image width
 * @param height Image height
 */
static void rgb565_to_rgb24(const uint16_t *rgb565, uint8_t *rgb24, int width, int height)
{
    for (int i = 0; i < width * height; i++) {
        uint16_t pixel = rgb565[i];
        // RGB565: RRRRR GGGGGG BBBBB
        uint8_t r = ((pixel >> 11) & 0x1F) << 3;  // 5 bits -> 8 bits
        uint8_t g = ((pixel >> 5) & 0x3F) << 2;   // 6 bits -> 8 bits
        uint8_t b = (pixel & 0x1F) << 3;          // 5 bits -> 8 bits
        
        rgb24[i * 3 + 0] = r;
        rgb24[i * 3 + 1] = g;
        rgb24[i * 3 + 2] = b;
    }
}

/**
 * @brief Encode RGB24 image to JPEG
 * @param rgb24 Input RGB24 data
 * @param width Image width
 * @param height Image height
 * @param quality JPEG quality (1-100)
 * @param jpeg_data Output JPEG data pointer (will be allocated)
 * @param jpeg_size Output JPEG size
 * @return 0 on success, -1 on failure
 */
static int encode_jpeg(const uint8_t *rgb24, int width, int height, int quality,
                       uint8_t **jpeg_data, size_t *jpeg_size)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    
    // Initialize JPEG compression
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    
    // Set up memory destination
    size_t outsize = 0;
    unsigned char *outbuffer = NULL;
    jpeg_mem_dest(&cinfo, &outbuffer, &outsize);
    
    // Set image parameters
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;  // RGB
    cinfo.in_color_space = JCS_RGB;
    
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    
    // Start compression
    jpeg_start_compress(&cinfo, TRUE);
    
    // Write scanlines
    JSAMPROW row_pointer[1];
    int row_stride = width * 3;
    
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = (JSAMPROW)&rgb24[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    
    // Finish compression
    jpeg_finish_compress(&cinfo);
    
    // Allocate output buffer and copy data
    *jpeg_data = (uint8_t *)exram_malloc(8, outsize);
    if (*jpeg_data == NULL) {
        jpeg_destroy_compress(&cinfo);
        if (outbuffer) free(outbuffer);
        return -1;
    }
    
    memcpy(*jpeg_data, outbuffer, outsize);
    *jpeg_size = outsize;
    
    // Cleanup
    jpeg_destroy_compress(&cinfo);
    if (outbuffer) free(outbuffer);
    
    return 0;
}

/**
 * @brief 将RGB565图像缩放到指定尺寸（使用最近邻插值）
 * @param src 输入图像数据（RGB565）
 * @param src_w 输入图像宽度
 * @param src_h 输入图像高度
 * @param dst 输出缩放后图像数据（RGB565）
 * @param dst_w 目标图像宽度
 * @param dst_h 目标图像高度
 */
static void image_compression(const uint16_t *src, uint32_t src_w, uint32_t src_h,
                          uint16_t *dst, uint32_t dst_w, uint32_t dst_h)
{
    if (!src || !dst || src_w == 0 || src_h == 0 || dst_w == 0 || dst_h == 0) {
        LISA_LOGE(TAG, "Invalid parameters for resize");
        return;
    }

    // 使用最近邻插值：计算缩放比例
    for (uint32_t dst_y = 0; dst_y < dst_h; dst_y++) {
        uint32_t src_y = (dst_y * src_h) / dst_h;
        if (src_y >= src_h) src_y = src_h - 1;

        const uint16_t *src_row = src + src_y * src_w;
        uint16_t *dst_row = dst + dst_y * dst_w;

        for (uint32_t dst_x = 0; dst_x < dst_w; dst_x++) {
            uint32_t src_x = (dst_x * src_w) / dst_w;
            if (src_x >= src_w) src_x = src_w - 1;

            dst_row[dst_x] = src_row[src_x];
        }
    }
}

/**
 * @brief 拍照识图核心逻辑 - 被MCP和按键共同调用
 * @return 0 on success, -1 on failure
 */
static int photo_recognition_core(void)
{
    // 1. 获取摄像头图片 (RGB565)
    size_t rgb565_size = 0;
    int ret = video_camera_capture_photo((uint8_t*)g_camera_image_buffer, &rgb565_size, 3000);
    if (ret) {
        LISA_LOGE(TAG, "Failed to capture photo");
        return -1;
    }
    LISA_LOGI(TAG, "Photo captured: %zu bytes RGB565", rgb565_size);

    // 2. 拷贝RGB565数据到全局PSRAM缓冲区
    uint32_t expected_size = CAMERA_IMAGE_WIDTH * CAMERA_IMAGE_HEIGHT * 2;
    if (rgb565_size != expected_size) {
        LISA_LOGW(TAG, "RGB565 size mismatch: expected %u, got %zu", expected_size, rgb565_size);
    }
    
    // 更新全局状态
    g_camera_image_width = CAMERA_IMAGE_WIDTH;
    g_camera_image_height = CAMERA_IMAGE_HEIGHT;
    g_camera_image_ready = true;
    
    LISA_LOGI(TAG, "RGB565 data copied to global buffer: %u bytes", expected_size);

    image_compression(g_camera_image_buffer, CAMERA_IMAGE_WIDTH, CAMERA_IMAGE_HEIGHT, display_buffer, DISPLAY_IMAGE_WIDTH, DISPLAY_IMAGE_HEIGHT);
    // 3. 通过ebus事件通知UI显示图片
    assistant_view_show_camera_image(display_buffer, DISPLAY_IMAGE_WIDTH, DISPLAY_IMAGE_HEIGHT);

    LISA_LOGI(TAG, "Camera image event sent (buffer addr: %p, %dx%d)", 
             g_camera_image_buffer, g_camera_image_width, g_camera_image_height);

    return 0;
}

/**
 * @brief HTTP header callback for HTTPC API
 */
static void* http_get_headers_callback(void)
{
    if (g_upload_headers == NULL) {
        LISA_LOGE(TAG, "g_upload_headers is NULL in callback!");
        return NULL;
    }
    LISA_LOGI(TAG, "Headers callback returning: %p", g_upload_headers);
    return (void*)g_upload_headers;
}

/**
 * @brief 获取multipart headers（包含boundary和Authorization）
 * @param boundary Multipart boundary字符串
 * @return headers字符串指针（需要调用方在使用完后清理g_auth_header）
 */
static char *http_client_get_multipart_headers(const char *boundary)
{
    const char *auth_token = lisa_aiui_get_auth_token();
    const char *headers = "Content-Type: multipart/form-data; boundary=%s\r\nAuthorization: Bearer %s";
    LISA_LOGI(TAG, "!!!!! auth token: %s", auth_token);
    if (!auth_token) {
        LISA_LOGE(TAG, "auth token is null");
        return NULL;
    }

    // 释放之前的 auth_header (避免内存泄漏)
    if (g_auth_header) {
        lisa_mem_free(g_auth_header);
        g_auth_header = NULL;
    }

    g_auth_header = lisa_mem_calloc(1, strlen(headers) + strlen(boundary) + strlen(auth_token) + 1);
    if (!g_auth_header) {
        LISA_LOGE(TAG, "Failed to allocate auth header");
        return NULL;
    }
    sprintf(g_auth_header, headers, boundary, auth_token);
    LISA_LOGI(TAG, "auth header: %s", g_auth_header);

    return g_auth_header;
}

/**
 * @brief 构造multipart/form-data格式的body（包含file和tool_call_id字段）
 * @param jpg_data JPEG数据
 * @param jpg_len JPEG数据长度
 * @param tool_call_id 工具调用ID
 * @param body_out 输出的body数据（需要调用方释放）
 * @param body_len_out 输出的body长度
 * @param boundary_out 输出的boundary字符串（需要调用方释放）
 * @return 0成功，-1失败
 */
static int build_multipart_body(const uint8_t *jpg_data, uint32_t jpg_len, const char *tool_call_id,
                                uint8_t **body_out, uint32_t *body_len_out, char **boundary_out)
{
    if (!jpg_data || jpg_len == 0 || !tool_call_id || !body_out || !body_len_out || !boundary_out) {
        return -1;
    }

    // 生成boundary
    const char *boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    *boundary_out = lisa_mem_calloc(1, strlen(boundary) + 1);
    if (!*boundary_out) {
        LISA_LOGE(TAG, "Failed to allocate boundary");
        return -1;
    }
    strcpy(*boundary_out, boundary);

    // 使用固定文件名
    const char *filename = "photo_reco.jpeg";

    // 构造multipart body (包含file和tool_call_id两个字段)
    const char *header_template = "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\nContent-Type: image/jpeg\r\n\r\n";
    const char *footer_template = "\r\n--%s\r\nContent-Disposition: form-data; name=\"tool_call_id\"\r\n\r\n%s\r\n--%s--\r\n";

    // 计算所需内存大小
    int header_len = strlen(header_template) + strlen(boundary) + strlen(filename) - 4; // -4 for %s %s
    int footer_len = strlen(footer_template) + strlen(boundary) * 2 + strlen(tool_call_id) - 6; // -6 for %s %s %s
    uint32_t total_len = header_len + jpg_len + footer_len;

    *body_out = lisa_mem_calloc(1, total_len + 1); // +1 for safety
    if (!*body_out) {
        LISA_LOGE(TAG, "Failed to allocate multipart body");
        lisa_mem_free(*boundary_out);
        *boundary_out = NULL;
        return -1;
    }

    // 写入header（包含动态生成的文件名）
    int offset = sprintf((char *)*body_out, header_template, boundary, filename);

    // 写入二进制数据
    memcpy(*body_out + offset, jpg_data, jpg_len);
    offset += jpg_len;

    // 写入footer（包含tool_call_id）
    int footer_written = sprintf((char *)(*body_out + offset), footer_template, boundary, tool_call_id, boundary);
    offset += footer_written;

    *body_len_out = offset; // 使用实际写入的字节数
    LISA_LOGI(TAG, "Multipart body built: %d bytes, filename: %s, tool_call_id: %s", *body_len_out, filename, tool_call_id);
    
    // 打印multipart body的前200字节和后200字节用于调试
    LISA_LOGI(TAG, "Multipart header (first 200 bytes): %.200s", (char *)*body_out);
    if (*body_len_out > 200) {
        LISA_LOGI(TAG, "Multipart footer (last 200 bytes): %.200s", (char *)(*body_out + *body_len_out - 200));
    }

    return 0;
}

/**
 * @brief Upload JPEG image to vision API using low-level HTTPC API
 * @param call_id Tool call ID from MCP context
 * @param jpeg_data JPEG image data
 * @param jpeg_size JPEG image size
 * @return 0 on success, -1 on failure
 */
static int upload_jpeg_to_vision_api(const char *call_id, const uint8_t *jpeg_data, size_t jpeg_size)
{
    int ret = -1;
    char *boundary = NULL;
    uint8_t *multipart_body = NULL;
    uint32_t multipart_len = 0;
    char *response_buf = NULL;
    HTTPParameters *http_param = NULL;
    
    // Get vision config
    vision_config_t vision_config;
    if (vision_config_get(&vision_config) != 0) {
        LISA_LOGE(TAG, "Vision config not available");
        return -1;
    }

    // Convert HTTPS to HTTP if needed
    if (vision_config.url[0] != '\0') {
        convert_https_to_http(vision_config.url, sizeof(vision_config.url));
    }

    // Fallback to staging URL
    const char *fallback_url = "http://staging-api.listenai.com/v1/xiaoling/vision/explain?t=xiaoling";
    const char *target_url = (vision_config.url[0] != '\0') ? vision_config.url : fallback_url;

    LISA_LOGI(TAG, "Uploading JPEG (%zu bytes) to: %s", jpeg_size, target_url);
    
    // 构造multipart/form-data body
    if (build_multipart_body(jpeg_data, (uint32_t)jpeg_size, call_id, &multipart_body, &multipart_len, &boundary) != 0) {
        LISA_LOGE(TAG, "Failed to build multipart body");
        goto exit;
    }

    // 获取包含boundary的headers
    g_upload_headers = http_client_get_multipart_headers(boundary);
    if (!g_upload_headers) {
        LISA_LOGE(TAG, "Failed to get headers");
        goto exit;
    }
    
    LISA_LOGI(TAG, "Headers prepared");
    
    // Allocate HTTP parameters
    http_param = (HTTPParameters *)lisa_mem_calloc(1, sizeof(HTTPParameters));
    if (!http_param) {
        LISA_LOGE(TAG, "Failed to allocate HTTPParameters");
        goto exit;
    }
    
    // Configure HTTP request (following reference code pattern)
    strncpy(http_param->Uri, target_url, sizeof(http_param->Uri) - 1);
    http_param->HttpVerb = VerbPost;
    http_param->nTimeout = 15;  // 
    http_param->pData = multipart_body;  // Direct pointer, no size limit
    http_param->pLength = multipart_len;
    
    LISA_LOGI(TAG, "HTTP config: URI=%s, Verb=%d, Timeout=%ds, DataLen=%u", 
              http_param->Uri, http_param->HttpVerb, http_param->nTimeout, multipart_len);
    
    // Disable SSL verify for staging env
    // HTTPC_set_ssl_verify_mode(0);
    
    // Retry loop: attempt upload up to 3 times
    const int MAX_RETRIES = 1;
    int retry_count = 0;
    int req_ret = -1;
    
    for (retry_count = 0; retry_count < MAX_RETRIES; retry_count++) {
        if (retry_count > 0) {
            LISA_LOGI(TAG, "Retrying upload (attempt %d/%d)...", retry_count + 1, MAX_RETRIES);
            vTaskDelay(pdMS_TO_TICKS(1000));  // Wait 1 second before retry
        }
        
        // Open HTTP connection
        int open_ret = HTTPC_open(http_param);
        if (open_ret != 0) {
            LISA_LOGE(TAG, "HTTPC_open failed: %d (attempt %d/%d)", open_ret, retry_count + 1, MAX_RETRIES);
            if (retry_count < MAX_RETRIES - 1) {
                continue;  // Retry
            }
            goto exit;
        }
        
        LISA_LOGI(TAG, "HTTP connection opened successfully (attempt %d)", retry_count + 1);
        
        // Send HTTP request (with custom headers callback)
        req_ret = HTTPC_request(http_param, http_get_headers_callback);
        if (req_ret != 0) {
            LISA_LOGE(TAG, "HTTPC_request failed: %d (attempt %d/%d)", req_ret, retry_count + 1, MAX_RETRIES);
            HTTPC_close(http_param);
            
            if (retry_count < MAX_RETRIES - 1) {
                continue;  // Retry
            }
            goto exit;
        }
        
        // Request successful
        LISA_LOGI(TAG, "HTTP request sent successfully on attempt %d", retry_count + 1);
        break;
    }
    
    // Check if all retries failed
    if (retry_count >= MAX_RETRIES) {
        LISA_LOGE(TAG, "Upload failed after %d retries", MAX_RETRIES);
        goto exit;
    }
    
    // Get response info
    HTTP_CLIENT http_client = {0};
    if (HTTPC_get_request_info(http_param, &http_client) != 0) {
        LISA_LOGE(TAG, "HTTPC_get_request_info failed");
        HTTPC_close(http_param);
        goto exit;
    }
    
    LISA_LOGI(TAG, "Response state: %d, body length: %u", 
             http_client.HttpState, http_client.TotalResponseBodyLength);
    
    // Read response data
    if (http_client.TotalResponseBodyLength > 0) {
        response_buf = lisa_mem_calloc(1, http_client.TotalResponseBodyLength + 1);
        if (response_buf) {
            unsigned int received = 0;
            unsigned int readsize = 0;
            
            do {
                if (HTTPC_read(http_param, response_buf + readsize, 4096, (void *)&received) != 0) {
                    if (received > 0) readsize += received;
                    break;
                }
                readsize += received;
            } while (readsize < http_client.TotalResponseBodyLength);
            
            LISA_LOGI(TAG, "Response: %.*s", readsize, response_buf);
            
            // Parse JSON response - check if message is "ok"
            cJSON *json = cJSON_ParseWithLength(response_buf, readsize);
            if (json) {
                cJSON *message = cJSON_GetObjectItem(json, "message");
                if (cJSON_IsString(message) && message->valuestring) {
                    if (strcmp(message->valuestring, "ok") == 0) {
                        LISA_LOGI(TAG, "Upload successful (message=ok)");
                        ret = 0;
                    } else {
                        LISA_LOGE(TAG, "Upload failed: message=%s", message->valuestring);
                    }
                } else {
                    LISA_LOGE(TAG, "Upload failed: no valid message field in response");
                }
                cJSON_Delete(json);
            } else {
                LISA_LOGE(TAG, "Failed to parse JSON response");
            }
        }
    }
    
    // Close HTTP connection
    HTTPC_close(http_param);
    
    LISA_LOGI(TAG, "HTTP Status Code: %d, Upload result: %s", 
              http_client.HttpState, ret == 0 ? "SUCCESS" : "FAILED");
    
exit:
    // Cleanup
    g_upload_headers = NULL;
    
    if (boundary) {
        lisa_mem_free(boundary);
    }
    if (multipart_body) {
        lisa_mem_free(multipart_body);
    }
    if (http_param) {
        lisa_mem_free(http_param);
    }
    if (response_buf) {
        lisa_mem_free(response_buf);
    }
    
    return ret;
}

/**
 * @brief 拍照识图处理函数 - MCP工具调用
 */
static mcp_result_t photo_recognition_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Photo recognition tool called");
    
    // Debug: print ctx->call_id
    LISA_LOGI(TAG, "DEBUG: ctx->call_id = %s", ctx->call_id ? ctx->call_id : "NULL");

    // Check if vision config is available
    if (!vision_config_is_valid()) {
        LISA_LOGE(TAG, "Vision config not available, cannot perform recognition");
        response->content = cJSON_CreateString("视觉识别功能未就绪，请稍后重试");
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    // 调用核心逻辑（拍照）
    int ret = photo_recognition_core();
    if (ret != 0) {
        response->content = cJSON_CreateString("拍照失败，请重试");
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    LISA_LOGI(TAG, "Photo captured: %dx%d", g_camera_image_width, g_camera_image_height);

    // Convert RGB565 to RGB24
    rgb565_to_rgb24(g_camera_image_buffer, g_rgb24_buffer, CAMERA_IMAGE_WIDTH, CAMERA_IMAGE_HEIGHT);
    
    // Encode to JPEG (降低质量以减小文件大小，避免发送失败)
    uint8_t *jpeg_data = NULL;
    size_t jpeg_size = 0;
    ret = encode_jpeg(g_rgb24_buffer, CAMERA_IMAGE_WIDTH, CAMERA_IMAGE_HEIGHT, 50, &jpeg_data, &jpeg_size);
    if (ret != 0 || jpeg_data == NULL) {
        response->content = cJSON_CreateString("图片编码失败");
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }
    
    LISA_LOGI(TAG, "JPEG encoded: %zu bytes (quality=50)", jpeg_size);
    
    // Check if JPEG is too large
    if (jpeg_size > 30000) {
        LISA_LOGW(TAG, "JPEG size too large: %zu bytes, may fail to send", jpeg_size);
    }

    // Upload image to vision API (使用真实的tool_call_id)
    const char *call_id = ctx->call_id ? ctx->call_id : "unknown_call_id";
    LISA_LOGI(TAG, "Using tool_call_id: %s", call_id);
    ret = upload_jpeg_to_vision_api(call_id, jpeg_data, jpeg_size);

    // 释放JPEG数据
    if (jpeg_data) {
        free(jpeg_data);
    }
    
    if (ret != 0) {
        response->content = cJSON_CreateString("图片上传失败");
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    // 返回成功信息
    response->content = cJSON_CreateString("拍照成功，正在识别中...");
    response->result = MCP_RESULT_SUCCESS;

    return MCP_RESULT_SUCCESS;
}

/**
 * @brief 获取上次识图结果处理函数（空实现，待后续完善）
 */
static mcp_result_t get_last_result_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Get last recognition result (not implemented yet)");

    // 返回未实现提示
    response->content = cJSON_CreateString("暂无识图结果");
    response->result = MCP_RESULT_SUCCESS;

    return MCP_RESULT_SUCCESS;
}

// 拍照识图工具参数定义（无参数）
static mcp_param_def_t photo_recognition_params[] = {
    MCP_PARAM_DEF_END
};

// 获取上次结果工具参数定义（无参数）
static mcp_param_def_t get_last_result_params[] = {
    MCP_PARAM_DEF_END
};

// 注册拍照识图工具
MCP_REGISTER_TOOL_STATIC(ls_take_photo,
                         "照相工具，支持通过摄像头查看用户的外貌、衣着和展示的物品。例如：用户想让你看看发型是否合适、衣服搭配效果，或展示一件物品供你识别与评价；遇到这类场景可以使用此工具。"
                         "该工具会调用摄像头拍照，然后使用AI模型识别图像中的物体、场景或文字内容。",
                         "1.0",
                         photo_recognition_params,
                         0,  // 无参数
                         photo_recognition_handler,
                         false,
                         NULL);

// 注册获取上次识图结果工具
MCP_REGISTER_TOOL_STATIC(get_last_recognition,
                         "获取上次拍照识图的结果。可以通过类似'刚才识别的是什么'、'上次拍照的结果'等方式触发。",
                         "1.0",
                         get_last_result_params,
                         0,  // 无参数
                         get_last_result_handler,
                         false,
                         NULL);

const char* get_photo_recognition_result(void)
{
    if (g_last_recognition_result[0] != '\0') {
        return g_last_recognition_result;
    }
    return "暂无识图结果";
}

/**
 * @brief Trigger photo recognition manually (e.g., from button press)
 * @return 0 on success, -1 on failure
 */
int photo_recognition_trigger(void)
{
    LISA_LOGI(TAG, "Photo recognition triggered by button");
    
    // 调用核心逻辑（拍照）
    LISA_LOGI(TAG, "Starting photo capture...");
    int ret = photo_recognition_core();
    if (ret != 0) {
        LISA_LOGE(TAG, "Photo capture failed");
        return ret;
    }
    LISA_LOGI(TAG, "Photo capture completed successfully");
    
    LISA_LOGI(TAG, "Converting RGB565 to RGB24...");
    rgb565_to_rgb24(g_camera_image_buffer, g_rgb24_buffer, CAMERA_IMAGE_WIDTH, CAMERA_IMAGE_HEIGHT);
    LISA_LOGI(TAG, "RGB565 to RGB24 conversion completed");
    
    LISA_LOGI(TAG, "Encoding JPEG...");
    uint8_t *jpeg_data = NULL;
    size_t jpeg_size = 0;
    ret = encode_jpeg(g_rgb24_buffer, CAMERA_IMAGE_WIDTH, CAMERA_IMAGE_HEIGHT, 85, &jpeg_data, &jpeg_size);
    if (ret != 0 || jpeg_data == NULL) {
        LISA_LOGE(TAG, "JPEG encoding failed");
        return -1;
    }
    LISA_LOGI(TAG, "JPEG encoding completed: %zu bytes", jpeg_size);
    
    extern const app_cloud_t *app_cloud_get(void);
    const app_cloud_t *cloud = app_cloud_get();
    if (cloud == NULL || cloud->aiui == NULL) {
        LISA_LOGE(TAG, "Cloud or AIUI handle is NULL, cannot send image");
        free(jpeg_data);
        return -1;
    }
    
    extern int lisa_aiui_send_jpg_img(lisa_aiui_t *handle, const void *img_data, int img_len);
    ret = lisa_aiui_send_jpg_img(cloud->aiui, jpeg_data, jpeg_size);
    free(jpeg_data);  
    
    if (ret != 0) {
        LISA_LOGE(TAG, "lisa_aiui_send_jpg_img failed: %d", ret);
        return -1;
    }
    
    LISA_LOGI(TAG, "Image sent via WebSocket successfully");
    
    LISA_LOGI(TAG, "Photo recognition completed");
    
    return 0;
}
