#include "show_image.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "display/lv_img_net_loader.h"
#include "assistant_controller.h"
#include "assistant_view.h"
#include "controller/apps/groups/llm/launcher/group_launcher.h"
#include "controller/apps/groups/llm/launcher/pages/launcher_pages.h"
#include "lisaui_manager.h"
#include "user_groups.h"
#include "video/video_camera.h"
#include "FreeRTOS.h"
#include "task.h"

#define TAG "show_image"

// 图片URL最大长度
#define IMAGE_URL_MAX_LEN 512

// 存储当前显示的图片URL（用于管理图片生命周期）
static char *current_image_url = NULL;

// 文生图等待状态
typedef enum {
    TEXT2IMG_STATE_NOT_WAITING = 0,  // 没有等待文生图
    TEXT2IMG_STATE_WAITING = 1,      // 正在等待文生图URL
    TEXT2IMG_STATE_CANCELLED = 2     // 等待已取消（用户有新交互）
} text2img_waiting_state_t;

// 文生图等待状态标志
// 当大模型意识到是文生图并下发二维码时，该标志会被设置为WAITING
// 如果在图片URL到来之前发生新ASR结果、重新唤醒、按键等动作，该标志会被设置为CANCELLED
// 图片URL到来时会检查该标志，如果是CANCELLED则不展示图片
static text2img_waiting_state_t text2img_waiting_state = TEXT2IMG_STATE_NOT_WAITING;

/**
 * @brief 设置文生图等待状态
 * @param waiting true表示正在等待文生图URL，false表示取消等待
 */
void show_image_set_waiting_state(bool waiting)
{
    if (waiting) {
        text2img_waiting_state = TEXT2IMG_STATE_WAITING;
        LISA_LOGI(TAG, "Text2img waiting state set to WAITING");
    } else {
        text2img_waiting_state = TEXT2IMG_STATE_NOT_WAITING;
        LISA_LOGI(TAG, "Text2img waiting state set to NOT_WAITING");
    }
}

/**
 * @brief 获取文生图等待状态
 * @return true表示正在等待文生图URL，false表示未在等待
 */
bool show_image_is_waiting(void)
{
    return (text2img_waiting_state == TEXT2IMG_STATE_WAITING);
}

/**
 * @brief 取消文生图等待状态（在ASR/唤醒/按键等事件发生时调用）
 */
void show_image_cancel_waiting(void)
{
    if (text2img_waiting_state == TEXT2IMG_STATE_WAITING) {
        LISA_LOGI(TAG, "Cancelling text2img waiting due to user interaction");
        text2img_waiting_state = TEXT2IMG_STATE_CANCELLED;
    }
}

/**
 * @brief 检查文生图等待是否已被取消
 * @return true表示已被取消，false表示未被取消
 */
bool show_image_is_cancelled(void)
{
    return (text2img_waiting_state == TEXT2IMG_STATE_CANCELLED);
}

/**
 * @brief 显示图片处理函数
 *
 * 该工具用于在UI上显示来自网络URL的图片。
 * 支持HTTP/HTTPS协议的图片资源。
 */
static mcp_result_t show_image_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    const char *url = NULL;

    // 解析参数: url (图片资源地址)
    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "url") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                url = cJSON_GetStringValue(ctx->params[i].value);
            }
        }
    }

    // 参数校验
    if (!url || strlen(url) == 0) {
        // 创建content数组
        cJSON *content_array = cJSON_CreateArray();
        if (!content_array) {
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        // 创建text item
        cJSON *text_item = cJSON_CreateObject();
        if (!text_item) {
            cJSON_Delete(content_array);
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        cJSON_AddStringToObject(text_item, "type", "text");
        cJSON_AddStringToObject(text_item, "text", "错误：缺少必需参数 url");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    // 验证URL长度
    size_t url_len = strlen(url);
    if (url_len >= IMAGE_URL_MAX_LEN) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg),
                "错误：URL长度超过限制 (%zu >= %d)", url_len, IMAGE_URL_MAX_LEN);
        // 创建content数组
        cJSON *content_array = cJSON_CreateArray();
        if (!content_array) {
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        // 创建text item
        cJSON *text_item = cJSON_CreateObject();
        if (!text_item) {
            cJSON_Delete(content_array);
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        cJSON_AddStringToObject(text_item, "type", "text");
        cJSON_AddStringToObject(text_item, "text", "已完成操作");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    // 验证URL格式（必须是http://或https://）
    if (strncmp(url, "http://", 7) != 0 &&
        strncmp(url, "https://", 8) != 0 &&
        strncmp(url, "N:", 2) != 0) {
        // 创建content数组
        cJSON *content_array = cJSON_CreateArray();
        if (!content_array) {
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        // 创建text item
        cJSON *text_item = cJSON_CreateObject();
        if (!text_item) {
            cJSON_Delete(content_array);
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        cJSON_AddStringToObject(text_item, "type", "text");
        cJSON_AddStringToObject(text_item, "text", "错误：URL必须以 http://, https:// 或 N: 开头");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Show image: Loading image from URL: %s", url);

    // 检查文生图等待状态
    // 如果状态是CANCELLED，说明用户在等待期间有了新的交互（ASR/唤醒/按键），应该抛弃图片
    if (text2img_waiting_state == TEXT2IMG_STATE_CANCELLED) {
        LISA_LOGW(TAG, "Text2img waiting was cancelled, discarding image URL");
        text2img_waiting_state = TEXT2IMG_STATE_NOT_WAITING;
        
        // 创建content数组 - 返回已处理的消息
        cJSON *content_array = cJSON_CreateArray();
        if (!content_array) {
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        cJSON *text_item = cJSON_CreateObject();
        if (!text_item) {
            cJSON_Delete(content_array);
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        cJSON_AddStringToObject(text_item, "type", "text");
        cJSON_AddStringToObject(text_item, "text", "已完成操作");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_SUCCESS;
        return MCP_RESULT_SUCCESS;
    }
    
    // 如果状态是WAITING，说明这是正常的文生图流程，清除等待状态并继续显示
    if (text2img_waiting_state == TEXT2IMG_STATE_WAITING) {
        LISA_LOGI(TAG, "Text2img image arrived as expected, clearing waiting state");
        text2img_waiting_state = TEXT2IMG_STATE_NOT_WAITING;
    }

    // 预处理URL：将 w_240 替换为 w_<DISPLAY_NET_IMAGE_WIDTH> 以适配小屏幕设备
    char processed_url[IMAGE_URL_MAX_LEN];
    const char *final_url = url;
    
    // 查找 w_240 并替换为 w_<DISPLAY_NET_IMAGE_WIDTH>
    const char *w_240_pos = strstr(url, "w_240");
    if (w_240_pos) {
        // 计算前缀长度
        size_t prefix_len = w_240_pos - url;
        // 计算后缀起始位置
        const char *suffix = w_240_pos + 5;  // strlen("w_240") = 5
        
        // 构造目标宽度字符串 w_<DISPLAY_NET_IMAGE_WIDTH>
        char width_str[16];
        snprintf(width_str, sizeof(width_str), "w_%d", DISPLAY_NET_IMAGE_WIDTH);
        size_t width_str_len = strlen(width_str);
        
        // 确保缓冲区足够
        if (prefix_len + width_str_len + strlen(suffix) < IMAGE_URL_MAX_LEN) {
            // 构造新URL
            memcpy(processed_url, url, prefix_len);
            memcpy(processed_url + prefix_len, width_str, width_str_len);
            strcpy(processed_url + prefix_len + width_str_len, suffix);
            final_url = processed_url;
            LISA_LOGI(TAG, "URL preprocessed: w_240 -> %s", width_str);
        } else {
            LISA_LOGW(TAG, "Processed URL would exceed buffer, using original");
        }
    }

    // 预加载图片以验证URL是否有效
    lv_img_dsc_t *img_dsc = lv_img_net_load(final_url);
    if (!img_dsc) {
        LISA_LOGE(TAG, "Failed to load image from URL: %s", final_url);
        // 创建content数组
        cJSON *content_array = cJSON_CreateArray();
        if (!content_array) {
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        // 创建text item
        cJSON *text_item = cJSON_CreateObject();
        if (!text_item) {
            cJSON_Delete(content_array);
            response->result = MCP_RESULT_ERROR;
            return MCP_RESULT_ERROR;
        }

        cJSON_AddStringToObject(text_item, "type", "text");
        cJSON_AddStringToObject(text_item, "text", "错误：无法加载图片，请检查URL是否有效");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    LISA_LOGI(TAG, "Image loaded successfully: %u bytes", img_dsc->data_size);

    // 保存当前图片URL（用于后续清理）- 使用处理后的URL
    if (current_image_url) {
        lisa_mem_free(current_image_url);
    }
    size_t final_url_len = strlen(final_url);
    current_image_url = (char *)lisa_mem_alloc(final_url_len + 1);
    if (current_image_url) {
        strncpy(current_image_url, final_url, final_url_len);
        current_image_url[final_url_len] = '\0';
    }

    // 如果当前在二维码页面，需要先返回主页才能显示图片
    // 这种情况通常发生在高质量文生图：先显示QR，然后图片URL在10秒内到达
    if (is_info_page_active()) {
        LISA_LOGI(TAG, "Currently on info page, navigating to home before showing image");
        lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER, 
                                   GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
                                   LISAUI_GROUP_LAUNCHER_PAGE_INDEX_PRIMARY, 0);
        // 短暂延时确保页面切换完成
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 将图片发送给UI显示（使用UI线程的workqueue）
    int ui_ret = assistant_view_show_net_image(img_dsc);
    if (ui_ret != 0) {
        LISA_LOGE(TAG, "Failed to dispatch image to UI, ret=%d", ui_ret);
    } else {
        LISA_LOGI(TAG, "Image cached and dispatched to UI for display");
    }

    // 创建content数组
    cJSON *content_array = cJSON_CreateArray();
    if (!content_array) {
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    // 创建text item
    cJSON *text_item = cJSON_CreateObject();
    if (!text_item) {
        cJSON_Delete(content_array);
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    cJSON_AddStringToObject(text_item, "type", "text");
    cJSON_AddStringToObject(text_item, "text", "已完成操作");
    cJSON_AddItemToArray(content_array, text_item);

    response->content = content_array;
    response->result = MCP_RESULT_SUCCESS;

    return MCP_RESULT_SUCCESS;
}

/**
 * @brief 生成显示图片工具的参数 Schema
 */
cJSON* generate_show_image_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for show_image schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to show_image schema");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        LISA_LOGE(TAG, "Failed to create properties object");
        cJSON_Delete(root);
        return NULL;
    }

    // url 参数
    cJSON *url_prop = cJSON_CreateObject();
    if (!url_prop) {
        LISA_LOGE(TAG, "Failed to create url property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(url_prop, "type", "string") ||
        !cJSON_AddStringToObject(url_prop, "description", "图片资源地址")) {
        LISA_LOGE(TAG, "Failed to add url property fields");
        cJSON_Delete(url_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "url", url_prop);

    cJSON_AddItemToObject(root, "properties", properties);

    // required 数组
    cJSON *required = cJSON_CreateArray();
    if (!required) {
        LISA_LOGE(TAG, "Failed to create required array");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *url_str = cJSON_CreateString("url");
    if (!url_str) {
        LISA_LOGE(TAG, "Failed to create url string for required array");
        cJSON_Delete(required);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToArray(required, url_str);
    cJSON_AddItemToObject(root, "required", required);

    return root;
}

/**
 * @brief 直接加载并显示图片（无等待状态的检查）
 * @param url 图片URL
 * @return 0表示成功，-1表示失败
 * 
 * @note 此函数不进行文生图等待状态检查，收到URL后直接加载并显示
 * @note 会自动将URL中的 w_240 或 w_200 替换为 w_<DISPLAY_NET_IMAGE_WIDTH> 以适配小屏幕
 */
int show_image_load_and_display(const char *url)
{
    if (!url || strlen(url) == 0) {
        LISA_LOGE(TAG, "Invalid URL parameter");
        return -1;
    }
    
    LISA_LOGI(TAG, "Loading and displaying image from URL: %s", url);
    
    // URL 预处理：将 w_240 或 w_200 （旧链路是w_200） 替换为 w_<DISPLAY_NET_IMAGE_WIDTH>
    char processed_url[IMAGE_URL_MAX_LEN];
    const char *final_url = url;
    const char *w_pos = strstr(url, "w_240");
    if (!w_pos) {
        w_pos = strstr(url, "w_200");
    }
    if (w_pos) {
        size_t prefix_len = w_pos - url;
        const char *suffix = w_pos + 5; // strlen("w_240") or strlen("w_200")
        
        // 构造目标宽度字符串 w_<DISPLAY_NET_IMAGE_WIDTH>
        char width_str[16];
        snprintf(width_str, sizeof(width_str), "w_%d", DISPLAY_NET_IMAGE_WIDTH);
        size_t width_str_len = strlen(width_str);
        
        if (prefix_len + width_str_len + strlen(suffix) < IMAGE_URL_MAX_LEN) {
            memcpy(processed_url, url, prefix_len);
            memcpy(processed_url + prefix_len, width_str, width_str_len);
            strcpy(processed_url + prefix_len + width_str_len, suffix);
            final_url = processed_url;
            LISA_LOGI(TAG, "URL preprocessed: width param -> %s", width_str);
        } else {
            LISA_LOGW(TAG, "Processed URL would exceed buffer, using original");
        }
    }
    
    // 如果当前在二维码页面，需要先返回主页才能显示图片
    // 这种情况通常发生在高质量文生图：先显示QR，然后图片URL在10秒内到达
    if (is_info_page_active()) {
        LISA_LOGI(TAG, "Currently on info page, navigating to home before showing image");
        lisaui_manager_group_enter(LISAUI_GROUP_INDEX_LAUNCHER, 
                                   GROUP_ENTER_PAGE_METHOD_FIX_PAGE_INDEX,
                                   LISAUI_GROUP_LAUNCHER_PAGE_INDEX_PRIMARY, 0);
        // 短暂延时确保页面切换完成
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // 加载图片
    lv_img_dsc_t *img_dsc = lv_img_net_load(final_url);
    if (!img_dsc) {
        LISA_LOGE(TAG, "Failed to load image from URL: %s", final_url);
        return -1;
    }
    
    LISA_LOGI(TAG, "Image loaded successfully: %u bytes, %dx%d", 
             img_dsc->data_size, img_dsc->header.w, img_dsc->header.h);
    
    // 发送到UI显示
    int ui_ret = assistant_view_show_net_image(img_dsc);
    if (ui_ret != 0) {
        LISA_LOGE(TAG, "Failed to dispatch image to UI, ret=%d", ui_ret);
        return -1;
    }
    
    LISA_LOGI(TAG, "Image dispatched to UI for display");
    return 0;
}

// 使用静态段注册宏注册显示图片工具
MCP_REGISTER_TOOL_STATIC(show_image,
                          "ls.built_in.show_image",
                          "显示图片",
                          "1.0",
                          generate_show_image_schema,
                          1,
                          show_image_handler,
                          false,
                          NULL);
