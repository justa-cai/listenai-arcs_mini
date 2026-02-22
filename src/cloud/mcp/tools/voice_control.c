#include "voice_control.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "lisa_kv.h"
#include "kv/kv_user.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "jk_cloud/jk_cloud.h"
#include "jk_cloud/jk_tts.h"

#define TAG "voice_control"

// 当前音色（保存在全局变量中，用于查询）
static char current_voice_id[128] = "趣味口音-湾湾小何";

// 音色列表（用于验证音色ID有效性）
static const char* general_voices[] = {
    "通用场景-渊博小叔", "通用场景-开朗轻快", "通用场景-阳光青年", "通用场景-暖心体贴",
    "通用场景-甜美悦悦", "通用场景-少年梓辛", "通用场景-邻家男孩", "通用场景-灿灿",
    "通用场景-清新女声", "通用场景-温暖阿虎", "通用场景-开朗姐姐", "通用场景-清澈梓梓",
    "通用场景-甜美小源", "通用场景-爽快思思", "通用场景-知性女声", "通用场景-清爽男大",
    "通用场景-温柔文雅", "通用场景-邻家女孩", "通用场景-解说小明", "通用场景-知性温婉",
    "通用场景-心灵鸡汤"
};

static const char* video_voices[] = {
    "视频配音-佩奇猪", "视频配音-猴哥", "视频配音-贴心女声", "视频配音-熊二",
    "视频配音-磁性解说男声", "视频配音-亮嗓萌仔", "视频配音-广告解说", "视频配音-樱桃丸子",
    "视频配音-萌丫头", "视频配音-鸡汤妹妹", "视频配音-四郎", "视频配音-温柔小雅",
    "视频配音-邻居阿姨", "视频配音-顾姐", "视频配音-懒音绵宝", "视频配音-和蔼奶奶",
    "视频配音-俏皮女声", "视频配音-天才童声", "视频配音-少儿故事", "视频配音-武则天"
};

static const char* multilingual_voices[] = {
    "多语种-Anna", "多语种-Jackson", "多语种-Amanda", "多语种-Morgan",
    "多语种-Alvin", "多语种-Cutey", "多语种-Hope", "多语种-Smith",
    "多语种-Skye", "多语种-Shiny", "多语种-Candy", "多语种-Brayan",
    "多语种-Harmony", "多语种-Adam"
};

static const char* roleplay_voices[] = {
    "角色扮演-娇弱萝莉", "角色扮演-潇洒随性", "角色扮演-绿茶小哥", "角色扮演-傲慢娇声",
    "角色扮演-撒娇学妹", "角色扮演-高冷御姐", "角色扮演-东方浩然", "角色扮演-病弱少女",
    "角色扮演-奶气萌娃", "角色扮演-活泼女孩", "角色扮演-冷淡疏离", "角色扮演-憨厚敦实",
    "角色扮演-活泼刁蛮", "角色扮演-撒娇粘人", "角色扮演-傲娇霸总", "角色扮演-婆婆",
    "角色扮演-魅力女友", "角色扮演-深夜播客", "角色扮演-固执病娇", "角色扮演-柔美女友",
    "角色扮演-傲气凌人"
};

static const char* accent_voices[] = {
    "趣味口音-浩宇小哥", "趣味口音-京腔侃爷", "趣味口音-湾区大叔",
    "趣味口音-广州德哥", "趣味口音-湾湾小何", "趣味口音-广西远舟",
    "趣味口音-呆萌川妹"
};

static const char* audiobook_voices[] = {
    "有声阅读-儒雅青年", "有声阅读-活力小哥", "有声阅读-古风少御",
    "有声阅读-悬疑解说", "有声阅读-霸气青叔", "有声阅读-反卷青年",
    "有声阅读-温柔淑女", "有声阅读-擎苍"
};

static const char* all_voices[] = {
    "全部-渊博小叔", "全部-阳光青年", "全部-少年梓辛", "全部-豫州子轩",
    "全部-浩宇小哥", "全部-高冷御姐", "全部-温暖阿虎", "全部-京腔侃爷",
    "全部-湾区大叔", "全部-妹坨洁儿", "全部-爽快思思", "全部-广州德哥",
    "全部-湾湾小何", "全部-傲娇霸总", "全部-北京小爷", "全部-邻家女孩",
    "全部-魅力女友", "全部-广西远舟", "全部-深夜播客", "全部-柔美女友",
    "全部-呆萌川妹"
};

// 检查音色ID是否有效
static bool is_valid_voice_id(const char *voice_id) {
    if (!voice_id || strlen(voice_id) == 0) {
        return false;
    }

    // 检查通用场景音色
    for (size_t i = 0; i < sizeof(general_voices) / sizeof(general_voices[0]); i++) {
        if (strcmp(voice_id, general_voices[i]) == 0) {
            return true;
        }
    }

    // 检查视频配音音色
    for (size_t i = 0; i < sizeof(video_voices) / sizeof(video_voices[0]); i++) {
        if (strcmp(voice_id, video_voices[i]) == 0) {
            return true;
        }
    }

    // 检查多语种音色
    for (size_t i = 0; i < sizeof(multilingual_voices) / sizeof(multilingual_voices[0]); i++) {
        if (strcmp(voice_id, multilingual_voices[i]) == 0) {
            return true;
        }
    }

    // 检查角色扮演音色
    for (size_t i = 0; i < sizeof(roleplay_voices) / sizeof(roleplay_voices[0]); i++) {
        if (strcmp(voice_id, roleplay_voices[i]) == 0) {
            return true;
        }
    }

    // 检查趣味口音音色
    for (size_t i = 0; i < sizeof(accent_voices) / sizeof(accent_voices[0]); i++) {
        if (strcmp(voice_id, accent_voices[i]) == 0) {
            return true;
        }
    }

    // 检查有声阅读音色
    for (size_t i = 0; i < sizeof(audiobook_voices) / sizeof(audiobook_voices[0]); i++) {
        if (strcmp(voice_id, audiobook_voices[i]) == 0) {
            return true;
        }
    }

    // 检查全部音色
    for (size_t i = 0; i < sizeof(all_voices) / sizeof(all_voices[0]); i++) {
        if (strcmp(voice_id, all_voices[i]) == 0) {
            return true;
        }
    }

    return false;
}

// 构建文本响应
static mcp_result_t build_text_response(mcp_response_t *response, const char *text, mcp_result_t result)
{
    if (!response) {
        return MCP_RESULT_INVALID_PARAM;
    }

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
    cJSON_AddStringToObject(text_item, "text", text ? text : "");
    cJSON_AddItemToArray(content_array, text_item);

    response->content = content_array;
    response->result = result;
    return result;
}

// 设置音色处理函数
static mcp_result_t voice_set_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    const char *voice_id = NULL;

    // 解析参数
    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "voice_id") == 0 && cJSON_IsString(ctx->params[i].value)) {
            voice_id = ctx->params[i].value->valuestring;
            break;
        }
    }

    if (!voice_id || strlen(voice_id) == 0) {
        return build_text_response(response, "错误：缺少 voice_id 参数", MCP_RESULT_INVALID_PARAM);
    }

    // 验证音色ID是否有效
    if (!is_valid_voice_id(voice_id)) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "错误：无效的音色ID \"%s\"", voice_id);
        return build_text_response(response, error_msg, MCP_RESULT_INVALID_PARAM);
    }

    // 获取 cloud 实例并设置音色
    jk_cloud_t *cloud = jk_cloud_get_instance();
    if (!cloud || !cloud->tts) {
        LISA_LOGE(TAG, "Failed to get cloud or TTS instance");
        return build_text_response(response, "错误：TTS 服务未初始化", MCP_RESULT_ERROR);
    }

    // 设置音色
    jk_tts_set_voice(cloud->tts, voice_id);

    // 保存到 KV 存储
    if (lisa_kv_set_string(KV_KEY_USER_VOICE_ID, voice_id) != 0) {
        LISA_LOGW(TAG, "Failed to save voice_id to KV storage");
    } else {
        LISA_LOGI(TAG, "Voice ID saved to KV: %s", voice_id);
    }

    // 更新当前音色记录
    strncpy(current_voice_id, voice_id, sizeof(current_voice_id) - 1);
    current_voice_id[sizeof(current_voice_id) - 1] = '\0';

    LISA_LOGI(TAG, "Voice ID set to: %s", voice_id);

    // 构建响应
    char result_msg[256];
    snprintf(result_msg, sizeof(result_msg), "已设置音色为：%s", voice_id);

    return build_text_response(response, result_msg, MCP_RESULT_SUCCESS);
}

// 获取当前音色处理函数
static mcp_result_t voice_get_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    // 获取 cloud 实例以获取最新音色
    jk_cloud_t *cloud = jk_cloud_get_instance();
    const char *actual_voice_id = current_voice_id;

    if (cloud && cloud->tts && cloud->tts->voice_id) {
        actual_voice_id = cloud->tts->voice_id;
    }

    LISA_LOGI(TAG, "Current voice ID: %s", actual_voice_id);

    // 构建响应
    char result_msg[256];
    snprintf(result_msg, sizeof(result_msg), "当前音色为：%s", actual_voice_id);

    return build_text_response(response, result_msg, MCP_RESULT_SUCCESS);
}

// 生成设置音色的参数 Schema
cJSON* generate_voice_set_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for voice_set schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to voice_set schema");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        LISA_LOGE(TAG, "Failed to create properties object");
        cJSON_Delete(root);
        return NULL;
    }

    // voice_id 参数
    cJSON *voice_id_prop = cJSON_CreateObject();
    if (!voice_id_prop) {
        LISA_LOGE(TAG, "Failed to create voice_id property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }

    // 只添加常用音色到枚举，避免堆栈溢出
    static const char* common_voices[] = {
        "通用场景-阳光青年", "通用场景-温暖阿虎", "通用场景-开朗姐姐",
        "趣味口音-湾湾小何", "趣味口音-京腔侃爷", "趣味口音-呆萌川妹",
        "角色扮演-高冷御姐", "角色扮演-傲娇霸总", "角色扮演-柔美女友",
        "通用场景-渊博小叔", "通用场景-知性女声", "有声阅读-悬疑解说"
    };

    cJSON *voice_enum = cJSON_CreateArray();
    for (size_t i = 0; i < sizeof(common_voices) / sizeof(common_voices[0]); i++) {
        cJSON_AddItemToArray(voice_enum, cJSON_CreateString(common_voices[i]));
    }

    cJSON_AddStringToObject(voice_id_prop, "type", "string");
    cJSON_AddItemToObject(voice_id_prop, "enum", voice_enum);
    cJSON_AddStringToObject(voice_id_prop, "description",
        "音色ID，格式为\"分类-音色名称\"。支持7大类共104种音色：通用场景、视频配音、多语种、角色扮演、趣味口音、有声阅读、全部。");

    cJSON_AddItemToObject(properties, "voice_id", voice_id_prop);
    cJSON_AddItemToObject(root, "properties", properties);

    // required 数组
    cJSON *required = cJSON_CreateArray();
    if (!required) {
        LISA_LOGE(TAG, "Failed to create required array");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON_AddItemToArray(required, cJSON_CreateString("voice_id"));
    cJSON_AddItemToObject(root, "required", required);

    return root;
}

// 生成获取当前音色的参数 Schema
cJSON* generate_voice_get_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for voice_get schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to voice_get schema");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        LISA_LOGE(TAG, "Failed to create properties object");
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "properties", properties);

    // 无 required 参数
    cJSON *required = cJSON_CreateArray();
    if (!required) {
        LISA_LOGE(TAG, "Failed to create required array");
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "required", required);

    return root;
}

// 获取当前音色（供外部调用）
const char* voice_control_get_current_voice(void)
{
    jk_cloud_t *cloud = jk_cloud_get_instance();
    if (cloud && cloud->tts && cloud->tts->voice_id) {
        return cloud->tts->voice_id;
    }
    return current_voice_id;
}

// 注册设置音色工具
MCP_REGISTER_TOOL_STATIC(ls_voice_set,
                          "ls.voice_set",
                          "设置TTS发音音色。支持7大类共104种音色，包括通用场景、视频配音、多语种、角色扮演、趣味口音、有声阅读等分类。音色ID格式为\"分类-音色名称\"，例如：\"通用场景-阳光青年\"、\"趣味口音-湾湾小何\"、\"角色扮演-高冷御姐\"等。",
                          "1.0",
                          generate_voice_set_schema,
                          1,
                          voice_set_handler,
                          false,
                          NULL);

// 注册获取当前音色工具
MCP_REGISTER_TOOL_STATIC(ls_voice_get,
                          "ls.voice_get",
                          "获取当前TTS发音音色。",
                          "1.0",
                          generate_voice_get_schema,
                          0,
                          voice_get_handler,
                          false,
                          NULL);
