#include "local_music.h"
#include "aiui_mcp.h"
#include "lisa_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "proc_mgr.h"
#include "player/audio_player.h"
#include "player/music_manager.h"

#define TAG "local_music"

static mcp_result_t local_music_handler(const mcp_context_t *ctx, mcp_response_t *response)
{
    LISA_LOGI(TAG, "%s---", __func__);
    
    response->result = MCP_RESULT_SUCCESS;

    if (!ctx || !response) {
        return MCP_RESULT_INVALID_PARAM;
    }

    const char *action = "play";
    const char *keyword = NULL;
    char song_name[128] = {0};
    char response_text[512] = {0};

    for (uint32_t i = 0; i < ctx->param_count; i++) {
        if (strcmp(ctx->params[i].name, "action") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                action = cJSON_GetStringValue(ctx->params[i].value);
            }
        }
        else if (strcmp(ctx->params[i].name, "keyword") == 0) {
            if (cJSON_IsString(ctx->params[i].value)) {
                keyword = cJSON_GetStringValue(ctx->params[i].value);
            }
        }
    }

    LISA_LOGI(TAG, "Local music: action=%s, keyword=%s", action ? action : "NULL", keyword ? keyword : "NULL");

    if (strcmp(action, "search") == 0) {
        if (!keyword || strlen(keyword) == 0) {
            snprintf(response_text, sizeof(response_text), "请提供要搜索的关键词，例如：搜索DJ音乐");
        } else {
            LISA_LOGI(TAG, "Local music: searching and playing for keyword=%s", keyword);
            int ret = music_manager_search_and_play(keyword, song_name, sizeof(song_name));
            if (ret != 0) {
                snprintf(response_text, sizeof(response_text), "没有找到关于\"%s\"的歌曲，换个关键词试试？", keyword);
            } else if (song_name[0] != '\0') {
                snprintf(response_text, sizeof(response_text), "为您播放《%s》", song_name);
            } else {
                snprintf(response_text, sizeof(response_text), "为您播放关于\"%s\"的歌曲", keyword);
            }
        }
    }
    else if (strcmp(action, "next") == 0) {
        LISA_LOGI(TAG, "Local music: next");
        int ret = music_manager_play_next_random(song_name, sizeof(song_name));
        if (ret != 0) {
            LISA_LOGE(TAG, "Failed to play next music");
            snprintf(response_text, sizeof(response_text), "好的，为您播放下一首");
        } else if (song_name[0] != '\0') {
            snprintf(response_text, sizeof(response_text), "好的，为您播放下一首《%s》", song_name);
        } else {
            snprintf(response_text, sizeof(response_text), "好的，为您播放下一首");
        }
    }
    else if (strcmp(action, "prev") == 0) {
        LISA_LOGI(TAG, "Local music: prev");
        int ret = music_manager_play_next_random(song_name, sizeof(song_name));
        if (ret != 0) {
            LISA_LOGE(TAG, "Failed to play prev music");
            snprintf(response_text, sizeof(response_text), "好的，为您播放上一首");
        } else if (song_name[0] != '\0') {
            snprintf(response_text, sizeof(response_text), "好的，为您播放上一首《%s》", song_name);
        } else {
            snprintf(response_text, sizeof(response_text), "好的，为您播放上一首");
        }
    }
    else if (strcmp(action, "stop") == 0) {
        LISA_LOGI(TAG, "Local music: stop");
        audio_player_stop_by_user();
        snprintf(response_text, sizeof(response_text), "好的，已停止播放");
    }
    else {
        LISA_LOGI(TAG, "Local music: playing random music");
        int ret = music_manager_fetch_and_play_random(song_name, sizeof(song_name));
        if (ret != 0) {
            LISA_LOGE(TAG, "Failed to play random music");
        }

        if (song_name[0] != '\0') {
            snprintf(response_text, sizeof(response_text), "好的，马上为您播放《%s》", song_name);
        } else {
            snprintf(response_text, sizeof(response_text), "好的，马上为您播放音乐");
        }
    }

    cJSON *content_array = cJSON_CreateArray();
    if (!content_array) {
        LISA_LOGE(TAG, "Local music: content_array create fail");
        return MCP_RESULT_SUCCESS;
    }

    cJSON *text_item = cJSON_CreateObject();
    if (!text_item) {
        LISA_LOGE(TAG, "Local music: text_item create fail");
        cJSON_Delete(content_array);
        return MCP_RESULT_SUCCESS;
    }

    cJSON_AddStringToObject(text_item, "type", "text");
    cJSON_AddStringToObject(text_item, "text", response_text);
    cJSON_AddItemToArray(content_array, text_item);

    response->content = content_array;
    
    return response->result;
}

cJSON* generate_local_music_schema(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        LISA_LOGE(TAG, "Failed to create root object for local_music schema");
        return NULL;
    }

    if (!cJSON_AddStringToObject(root, "type", "object")) {
        LISA_LOGE(TAG, "Failed to add type to local_music schema");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON_AddStringToObject(root, "description",
        "本地音乐播放工具。从设备本地音乐服务器随机选择一首歌曲进行播放，或控制音乐播放器的播放/停止、上一首/下一首切换，或根据关键词搜索并直接播放歌曲。搜索时记住关键词，后续切歌会继续播放相同关键词的歌曲。这是一个本地音乐播放功能，不涉及云端音乐服务或在线音乐平台。适用于用户想要听音乐、播放歌曲、随机播放、停止播放、暂停播放、切换歌曲、搜索歌曲等各种场景。");

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        LISA_LOGE(TAG, "Failed to create properties object");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *action_prop = cJSON_CreateObject();
    if (!action_prop) {
        LISA_LOGE(TAG, "Failed to create action property");
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(action_prop, "type", "string") ||
        !cJSON_AddStringToObject(action_prop, "description",
                           "操作类型：play（播放音乐、听歌、来首歌、放首歌、随机播放等）、"
                           "stop（停止播放、暂停、别放了、安静一下、关掉音乐等）、"
                           "next（下一首、下一个、切歌等）、"
                           "prev（上一首、前一首、上一个等）、"
                           "search（搜索并直接播放歌曲，记住关键词）。默认为play。")) {
        LISA_LOGE(TAG, "Failed to add action property fields");
        cJSON_Delete(action_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "action", action_prop);

    cJSON *keyword_prop = cJSON_CreateObject();
    if (!keyword_prop) {
        LISA_LOGE(TAG, "Failed to create keyword property");
        cJSON_Delete(action_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddStringToObject(keyword_prop, "type", "string") ||
        !cJSON_AddStringToObject(keyword_prop, "description",
                           "搜索关键词，当 action 为 search 时使用。支持模糊匹配歌曲名称。搜索后会记住关键词，后续切歌时会继续播放相同关键词的歌曲。")) {
        LISA_LOGE(TAG, "Failed to add keyword property fields");
        cJSON_Delete(keyword_prop);
        cJSON_Delete(action_prop);
        cJSON_Delete(properties);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(properties, "keyword", keyword_prop);

    cJSON_AddItemToObject(root, "properties", properties);

    cJSON *required = cJSON_CreateArray();
    if (!required) {
        LISA_LOGE(TAG, "Failed to create required array");
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "required", required);

    return root;
}

MCP_REGISTER_TOOL_STATIC(ls_built_in_local_music,
                         "ls.built_in.local_music",
                         "本地音乐播放工具。从设备本地音乐服务器随机选择一首歌曲进行播放，或控制音乐播放器的播放/停止、上一首/下一首切换，或根据关键词搜索并直接播放歌曲。搜索时记住关键词，后续切歌会继续播放相同关键词的歌曲。这是一个本地音乐播放功能，不涉及云端音乐服务或在线音乐平台。适用于以下各种场景：用户说\"播放音乐\"、\"听歌\"、\"来首歌\"、\"放首歌\"、\"随机播放\"、\"听点声音\"、\"来点音乐\"、\"娱乐一下\"、\"放点歌听\"、\"我想听歌\"、\"播放一首歌\"、\"放点音乐\"、\"来首歌听听\"、\"播放本地音乐\"、\"随便放首\"、\"听首歌\"、\"播放一首\"、\"来首音乐听听\"、\"放首歌吧\"、\"停止\"、\"停止播放\"、\"别放了\"、\"暂停\"、\"关掉音乐\"、\"安静一下\"、\"别唱了\"、\"不听了\"、\"关了\"、\"停止音乐\"、\"暂停音乐\"、\"下一首\"、\"上一个\"、\"切歌\"、\"前一首\"、\"搜索DJ\"、\"找首爱情\"、\"搜索周深\"、\"查一下\"等。action参数：play（默认播放）、stop（停止播放）、next（下一首）、prev（上一首）、search（搜索并直接播放，需配合keyword参数，记住关键词）。",
                         "1.0",
                         generate_local_music_schema,
                         0,
                         local_music_handler,
                         false,
                         NULL);
