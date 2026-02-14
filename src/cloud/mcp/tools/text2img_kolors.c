#include "text2img_kolors.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "text2img_config.h"
#include "show_image.h"
#include "assistant_view.h"
#include "HTTPCUsr_api.h"
#include "sysheap.h"
#include "FreeRTOS.h"
#include "task.h"

#define TAG "text2img_kolors"

#define HTTP_RESPONSE_BUFFER_SIZE 8192

static char *g_request_headers = NULL;

static void* http_get_headers_callback(void)
{
    if (g_request_headers == NULL) {
        LISA_LOGE(TAG, "g_request_headers is NULL in callback!");
        return NULL;
    }
    LISA_LOGI(TAG, "Headers callback returning: %p", g_request_headers);
    return (void*)g_request_headers;
}

static char *http_client_get_json_headers(void)
{
    const char *header_format = "Content-Type: application/json";
    size_t header_size = strlen(header_format) + 1;
    char *headers = (char *)lisa_mem_calloc(1, header_size);
    if (!headers) {
        LISA_LOGE(TAG, "Failed to allocate headers");
        return NULL;
    }

    snprintf(headers, header_size, "%s", header_format);
    LISA_LOGI(TAG, "Headers prepared: %s", headers);
    
    return headers;
}

static mcp_result_t text2img_kolors_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    const char *prompt = NULL;
    const char *model = "Kwai-Kolors/Kolors";
    const char *image_size = "1024x1024";
    const char *negative_prompt = NULL;
    int batch_size = 1;
    int num_inference_steps = 20;
    double guidance_scale = 7.5;

    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "prompt") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                prompt = cJSON_GetStringValue(ctx->params[i].value);
            }
        } else if (strcmp(ctx->params[i].name, "model") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                const char *value = cJSON_GetStringValue(ctx->params[i].value);
                if (value && strlen(value) > 0) {
                    model = value;
                }
            }
        } else if (strcmp(ctx->params[i].name, "image_size") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                const char *value = cJSON_GetStringValue(ctx->params[i].value);
                if (value && strlen(value) > 0) {
                    image_size = value;
                }
            }
        } else if (strcmp(ctx->params[i].name, "negative_prompt") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                const char *value = cJSON_GetStringValue(ctx->params[i].value);
                if (value && strlen(value) > 0) {
                    negative_prompt = value;
                }
            }
        } else if (strcmp(ctx->params[i].name, "batch_size") == 0) {
            if (cJSON_IsNumber(ctx->params[i].value)) {
                batch_size = ctx->params[i].value->valueint;
                if (batch_size < 1) batch_size = 1;
                if (batch_size > 4) batch_size = 4;
            }
        } else if (strcmp(ctx->params[i].name, "num_inference_steps") == 0) {
            if (cJSON_IsNumber(ctx->params[i].value)) {
                num_inference_steps = ctx->params[i].value->valueint;
                if (num_inference_steps < 1) num_inference_steps = 1;
                if (num_inference_steps > 100) num_inference_steps = 100;
            }
        } else if (strcmp(ctx->params[i].name, "guidance_scale") == 0) {
            if (cJSON_IsNumber(ctx->params[i].value)) {
                guidance_scale = ctx->params[i].value->valuedouble;
                if (guidance_scale < 0) guidance_scale = 0;
                if (guidance_scale > 20) guidance_scale = 20;
            }
        }
    }

    if (!prompt || strlen(prompt) == 0) {
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
        cJSON_AddStringToObject(text_item, "text", "错误：缺少必需参数 prompt");
        cJSON_AddItemToArray(content_array, text_item);

        response->content = content_array;
        response->result = MCP_RESULT_INVALID_PARAM;
        return MCP_RESULT_INVALID_PARAM;
    }

    LISA_LOGI(TAG, "Text2img: prompt='%s', model='%s', image_size='%s'", prompt, model, image_size);

    show_image_set_waiting_state(true);
    assistant_view_show_loading("正在生成图片...");

    cJSON *request_body = cJSON_CreateObject();
    if (!request_body) {
        show_image_cancel_waiting();
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    cJSON_AddStringToObject(request_body, "prompt", prompt);
    if (model && strlen(model) > 0) {
        cJSON_AddStringToObject(request_body, "model", model);
    }
    if (image_size && strlen(image_size) > 0) {
        cJSON_AddStringToObject(request_body, "image_size", image_size);
    }
    if (batch_size >= 1) {
        cJSON_AddNumberToObject(request_body, "batch_size", batch_size);
    }
    cJSON_AddNumberToObject(request_body, "num_inference_steps", 20);
    cJSON_AddNumberToObject(request_body, "guidance_scale", 7.5);

    char *request_body_str = cJSON_PrintUnformatted(request_body);
    cJSON_Delete(request_body);

    if (!request_body_str) {
        show_image_cancel_waiting();
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    LISA_LOGI(TAG, "Request body: %s", request_body_str);

    const char *base_url = text2img_config_get_base_url();
    char url[256];
    snprintf(url, sizeof(url), "%s/api/image/generate", base_url);

    HTTPParameters *http_param = (HTTPParameters *)lisa_mem_calloc(1, sizeof(HTTPParameters));
    if (!http_param) {
        lisa_mem_free(request_body_str);
        show_image_cancel_waiting();
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    strncpy(http_param->Uri, url, sizeof(http_param->Uri) - 1);
    http_param->HttpVerb = VerbPost;
    http_param->nTimeout = 30;
    http_param->pData = (uint8_t *)request_body_str;
    http_param->pLength = strlen(request_body_str);

    g_request_headers = http_client_get_json_headers();
    if (!g_request_headers) {
        lisa_mem_free(request_body_str);
        lisa_mem_free(http_param);
        show_image_cancel_waiting();
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    LISA_LOGI(TAG, "HTTP config: URI=%s, Verb=%d, Timeout=%ds", http_param->Uri, http_param->HttpVerb, http_param->nTimeout);

    const int MAX_RETRIES = 2;
    int retry_count = 0;
    int open_ret = -1;
    int req_ret = -1;
    int http_success = 0;
    char *response_buf = NULL;
    HTTP_CLIENT http_client = {0};

    for (retry_count = 0; retry_count < MAX_RETRIES; retry_count++) {
        if (retry_count > 0) {
            LISA_LOGI(TAG, "Retrying HTTP request (attempt %d/%d)...", retry_count + 1, MAX_RETRIES);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        open_ret = HTTPC_open(http_param);
        if (open_ret != 0) {
            LISA_LOGE(TAG, "HTTPC_open failed: %d (attempt %d/%d)", open_ret, retry_count + 1, MAX_RETRIES);
            if (retry_count < MAX_RETRIES - 1) {
                continue;
            }
            goto exit;
        }

        LISA_LOGI(TAG, "HTTP connection opened successfully (attempt %d)", retry_count + 1);

        req_ret = HTTPC_request(http_param, http_get_headers_callback);
        if (req_ret != 0) {
            LISA_LOGE(TAG, "HTTPC_request failed: %d (attempt %d/%d)", req_ret, retry_count + 1, MAX_RETRIES);
            HTTPC_close(http_param);
            
            if (retry_count < MAX_RETRIES - 1) {
                continue;
            }
            goto exit;
        }

        LISA_LOGI(TAG, "HTTP request sent successfully on attempt %d", retry_count + 1);
        http_success = 1;
        break;
    }

    if (!http_success) {
        LISA_LOGE(TAG, "HTTP request failed after %d retries", MAX_RETRIES);
        goto exit;
    }

    if (HTTPC_get_request_info(http_param, &http_client) != 0) {
        LISA_LOGE(TAG, "HTTPC_get_request_info failed");
        HTTPC_close(http_param);
        goto exit;
    }

    LISA_LOGI(TAG, "Response state: %d, body length: %u", http_client.HttpState, http_client.TotalResponseBodyLength);

    if (http_client.TotalResponseBodyLength > 0) {
        response_buf = (char *)lisa_mem_calloc(1, http_client.TotalResponseBodyLength + 1);
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

            response_buf[readsize] = '\0';
            LISA_LOGI(TAG, "Response: %s", response_buf);
        }
    }

    HTTPC_close(http_param);

    if (!response_buf) {
        LISA_LOGE(TAG, "Failed to read response");
        goto exit;
    }

    cJSON *response_json = cJSON_Parse(response_buf);
    lisa_mem_free(response_buf);

    if (!response_json) {
        LISA_LOGE(TAG, "Failed to parse response JSON");
        show_image_cancel_waiting();
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    // Check for success field (new API adds success field)
    cJSON *success_item = cJSON_GetObjectItem(response_json, "success");
    if (!success_item || !cJSON_IsTrue(success_item)) {
        LISA_LOGE(TAG, "API request failed (success != true)");
        cJSON_Delete(response_json);
        show_image_cancel_waiting();
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    cJSON *images_array = cJSON_GetObjectItem(response_json, "images");
    if (!images_array || !cJSON_IsArray(images_array)) {
        LISA_LOGE(TAG, "No images array in response");
        cJSON_Delete(response_json);
        show_image_cancel_waiting();
        response->result = MCP_RESULT_ERROR;
        return MCP_RESULT_ERROR;
    }

    cJSON *first_image = cJSON_GetArrayItem(images_array, 0);
    if (!first_image) {
        LISA_LOGE(TAG, "No image in array");
        cJSON_Delete(response_json);
        goto exit;
    }

    cJSON *url_item = cJSON_GetObjectItem(first_image, "url");
    if (!url_item || !cJSON_IsString(url_item)) {
        LISA_LOGE(TAG, "No url in image object");
        cJSON_Delete(response_json);
        goto exit;
    }

    const char *image_url = cJSON_GetStringValue(url_item);
    LISA_LOGI(TAG, "Generated image URL: %s", image_url);

    int ret = show_image_load_and_display(image_url);
    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to load and display image");
    }

    cJSON_Delete(response_json);

    lisa_mem_free(g_request_headers);
    g_request_headers = NULL;
    lisa_mem_free(request_body_str);
    lisa_mem_free(http_param);

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

exit:
    if (g_request_headers) {
        lisa_mem_free(g_request_headers);
        g_request_headers = NULL;
    }
    if (request_body_str) {
        lisa_mem_free(request_body_str);
    }
    if (http_param) {
        lisa_mem_free(http_param);
    }
    if (response_buf) {
        lisa_mem_free(response_buf);
    }
    
    show_image_cancel_waiting();
    response->result = MCP_RESULT_ERROR;
    return MCP_RESULT_ERROR;
}

cJSON *generate_text2img_kolors_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for text2img_kolors schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *prompt_prop = cJSON_CreateObject();
    if (!prompt_prop) {
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(prompt_prop, "type", "string") ||
        !cJSON_AddStringToObject(prompt_prop, "description", "图片描述提示词，用于描述要生成的图片内容。支持中文和英文。可以详细描述画面主体、场景、风格、色彩、构图等要素。例如：'一只可爱的橘猫坐在窗台上，阳光透过窗户洒在它身上，背景是蓝天白云，温馨治愈风格'或 'sunset over the ocean, orange and purple sky, peaceful atmosphere, digital art'")) {
        cJSON_Delete(prompt_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "prompt", prompt_prop);

    cJSON *negative_prompt_prop = cJSON_CreateObject();
    if (!negative_prompt_prop) {
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(negative_prompt_prop, "type", "string") ||
        !cJSON_AddStringToObject(negative_prompt_prop, "description", "负面提示词，用于描述不希望在图片中出现的内容。可以帮助提高图片质量，避免不需要的元素。例如：'模糊, 低质量, 失真, 水印, 文字, bad quality, blurry, watermark, text'")) {
        cJSON_Delete(negative_prompt_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "negative_prompt", negative_prompt_prop);

    cJSON *model_prop = cJSON_CreateObject();
    if (!model_prop) {
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(model_prop, "type", "string") ||
        !cJSON_AddStringToObject(model_prop, "description", "模型名称，默认 Kwai-Kolors/Kolors")) {
        cJSON_Delete(model_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "model", model_prop);

    cJSON *image_size_prop = cJSON_CreateObject();
    if (!image_size_prop) {
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(image_size_prop, "type", "string") ||
        !cJSON_AddStringToObject(image_size_prop, "description", "图片尺寸，格式为'宽x高'，如 1024x1024。支持尺寸：1024x1024(1:1正方形), 960x1280(3:4竖图), 720x1280(9:16手机竖屏), 720x1440(1:2长竖图), 768x1024(3:4竖图)")) {
        cJSON_Delete(image_size_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "image_size", image_size_prop);

    cJSON *batch_size_prop = cJSON_CreateObject();
    if (!batch_size_prop) {
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(batch_size_prop, "type", "integer") ||
        !cJSON_AddNumberToObject(batch_size_prop, "minimum", 1) ||
        !cJSON_AddNumberToObject(batch_size_prop, "maximum", 4) ||
        !cJSON_AddNumberToObject(batch_size_prop, "default", 1) ||
        !cJSON_AddStringToObject(batch_size_prop, "description", "一次生成的图片数量，范围1-4，默认为1。数量越多生成时间越长")) {
        cJSON_Delete(batch_size_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "batch_size", batch_size_prop);

    cJSON *num_inference_steps_prop = cJSON_CreateObject();
    if (!num_inference_steps_prop) {
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(num_inference_steps_prop, "type", "integer") ||
        !cJSON_AddNumberToObject(num_inference_steps_prop, "minimum", 1) ||
        !cJSON_AddNumberToObject(num_inference_steps_prop, "maximum", 100) ||
        !cJSON_AddNumberToObject(num_inference_steps_prop, "default", 20) ||
        !cJSON_AddStringToObject(num_inference_steps_prop, "description", "推理步数，范围1-100，默认为20。步数越多图片质量越好但生成时间越长，20-50步通常能获得较好效果")) {
        cJSON_Delete(num_inference_steps_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "num_inference_steps", num_inference_steps_prop);

    cJSON *guidance_scale_prop = cJSON_CreateObject();
    if (!guidance_scale_prop) {
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(guidance_scale_prop, "type", "number") ||
        !cJSON_AddNumberToObject(guidance_scale_prop, "minimum", 0) ||
        !cJSON_AddNumberToObject(guidance_scale_prop, "maximum", 20) ||
        !cJSON_AddNumberToObject(guidance_scale_prop, "default", 7.5) ||
        !cJSON_AddStringToObject(guidance_scale_prop, "description", "引导系数，范围0-20，默认为7.5。值越大生成图片越接近提示词，值越小创意性越强可能包含意外元素。推荐值：7.5-12")) {
        cJSON_Delete(guidance_scale_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "guidance_scale", guidance_scale_prop);

    cJSON_AddItemToObject(root, "properties", properties);

    cJSON *required = cJSON_CreateArray();
    if (!required) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *prompt_str = cJSON_CreateString("prompt");
    if (!prompt_str) {
        cJSON_Delete(required);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToArray(required, prompt_str);
    cJSON_AddItemToObject(root, "required", required);

    return root;
}

MCP_REGISTER_TOOL_STATIC(text2img_kolors,
                          "ls.built_in.text2img_kolors",
                          "文生图",
                          "1.0",
                          generate_text2img_kolors_schema,
                          1,
                          text2img_kolors_handler,
                          false,
                          NULL);
