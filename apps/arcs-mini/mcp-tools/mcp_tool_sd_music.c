#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lisa_log.h"
#include "lisa_mem.h"
#include "lsfs.h"
#include "mcp.h"
#include "cJSON.h"
#include "voice_cloud.h"
#include "voice_msg.h"
#include "voice_music_list.h"
#include "voice_intent/voice_intent_mgr.h"
#include "voice_intent/voice_intent_music.h"

#define TAG "mcp_tool_sd_music"

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

static cJSON *result_create_with_detail(const char *name, const char *text,
                                        bool is_error, cJSON *extra_result)
{
	cJSON *result = mcp_tool_call_result_create(name);
	if (!result) return NULL;

	cJSON *content_array = cJSON_CreateArray();
	cJSON *content_item = cJSON_CreateObject();
	if (!content_array || !content_item) {
		cJSON_Delete(content_array);
		cJSON_Delete(content_item);
		cJSON_Delete(result);
		return NULL;
	}

	cJSON_AddStringToObject(content_item, "type", "text");
	cJSON_AddStringToObject(content_item, "text", text);
	cJSON_AddItemToArray(content_array, content_item);
	cJSON_AddItemToObject(result, "content", content_array);
	cJSON_AddBoolToObject(result, "isError", is_error);

	if (extra_result) {
		cJSON_AddItemToObject(result, "result", extra_result);
	}

	return result;
}

/** @brief 简易 result_create */
static cJSON *result_create(const char *name, const char *text, bool is_error)
{
	return result_create_with_detail(name, text, is_error, NULL);
}

static bool path_has_mp3_suffix(const char *path)
{
	size_t len;

	if (!path) return false;

	len = strlen(path);
	if (len < 4) return false;

	return path[len - 4] == '.' &&
	       (path[len - 3] == 'm' || path[len - 3] == 'M') &&
	       (path[len - 2] == 'p' || path[len - 2] == 'P') &&
	       path[len - 1] == '3';
}

static bool path_ends_with_file_name(const char *path, const char *file_name)
{
	size_t path_len;
	size_t name_len;

	if (!path || !file_name || file_name[0] == '\0') return false;

	path_len = strlen(path);
	name_len = strlen(file_name);
	if (path_len < name_len) return false;

	return strcmp(path + path_len - name_len, file_name) == 0;
}

static int checked_snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list ap;
	int ret;

	if (!buf || size == 0) return -1;

	va_start(ap, fmt);
	ret = vsnprintf(buf, size, fmt, ap);
	va_end(ap);

	if (ret < 0 || (size_t)ret >= size) {
		buf[size - 1] = '\0';
		return -1;
	}

	return 0;
}

static int build_sd_music_url(char *url, size_t url_size,
                              const char *file_path, const char *file_name)
{
	bool file_path_is_full;
	size_t path_len;

	if (!url || url_size == 0 || !file_path || file_path[0] == '\0' ||
	    !file_name || file_name[0] == '\0') {
		return -1;
	}

	file_path_is_full = path_ends_with_file_name(file_path, file_name) ||
	                    path_has_mp3_suffix(file_path);
	if (file_path_is_full) {
		return checked_snprintf(url, url_size, "%s", file_path);
	}

	path_len = strlen(file_path);
	return checked_snprintf(url, url_size, "%s%s%s",
	                        file_path,
	                        (path_len > 0 && file_path[path_len - 1] == '/') ? "" : "/",
	                        file_name);
}

static cJSON *result_create_file_not_found(const char *name)
{
	cJSON *detail;

	detail = cJSON_CreateObject();
	if (!detail) {
		return result_create(name, "未找到对应的TF卡音频文件", true);
	}

	cJSON_AddBoolToObject(detail, "ok", false);
	cJSON_AddStringToObject(detail, "error_code", "FILE_NOT_FOUND");
	cJSON_AddStringToObject(detail, "message", "file not found");

	return result_create_with_detail(name, "未找到对应的TF卡音频文件", true, detail);
}

static void strip_mp3_suffix(char *name)
{
	size_t len;

	if (!name) return;

	len = strlen(name);
	if (len >= 4 && path_has_mp3_suffix(name)) {
		name[len - 4] = '\0';
	}
}

static void fill_track_display_name(music_item_t *track,
                                    const char *display_name,
                                    const char *file_name)
{
	const char *show = (display_name && display_name[0]) ? display_name : file_name;

	if (!track || !show) return;

	snprintf(track->m_name, sizeof(track->m_name), "%s", show);
	if (!display_name || display_name[0] == '\0') {
		strip_mp3_suffix(track->m_name);
	}
	snprintf(track->m_title, sizeof(track->m_title), "%s", track->m_name);
}

/* ============================================================================
 * play_tf_card_audio 工具
 *
 * 云端下发 items 数组 → 构建临时播放列表 → 顺序播放。
 * ============================================================================ */

static cJSON *play_tf_card_list(const char *name)
{
	cJSON *tool = mcp_tool_list_info_create_default(name,
	    "播放本地TF卡音频");

	if (!tool) return NULL;

	/* 获取 create_default 已创建好的 properties 和 required */
	cJSON *properties = mcp_tool_info_properties_get(tool);
	cJSON *required   = mcp_tool_info_required_get(tool);
	if (!properties || !required) {
		cJSON_Delete(tool);
		return NULL;
	}

	/* mode */
	{
		cJSON *prop = cJSON_CreateObject();
		cJSON_AddStringToObject(prop, "type", "string");
		cJSON_AddStringToObject(prop, "description",
		    "播放模式，single 表示从指定列表的 start_index 开始播放");
		cJSON *e = cJSON_CreateArray();
		cJSON_AddItemToArray(e, cJSON_CreateString("single"));
		cJSON_AddItemToObject(prop, "enum", e);
		cJSON_AddItemToObject(properties, "mode", prop);
		cJSON_AddItemToArray(required, cJSON_CreateString("mode"));
	}

	/* items: array with sub-properties */
	{
		cJSON *prop = cJSON_CreateObject();
		cJSON_AddStringToObject(prop, "type", "array");
		cJSON_AddStringToObject(prop, "description",
		    "TF卡音频文件列表，端侧按顺序尝试播放");

		cJSON *items_schema = cJSON_CreateObject();
		cJSON_AddStringToObject(items_schema, "type", "object");

		cJSON *items_props = cJSON_CreateObject();
		cJSON *items_required = cJSON_CreateArray();

		/* file_id */
		{
			cJSON *p = cJSON_CreateObject();
			cJSON_AddStringToObject(p, "type", "string");
			cJSON_AddStringToObject(p, "description", "云端生成的音频文件唯一标识");
			cJSON_AddItemToObject(items_props, "file_id", p);
			cJSON_AddItemToArray(items_required, cJSON_CreateString("file_id"));
		}
		/* file_name */
		{
			cJSON *p = cJSON_CreateObject();
			cJSON_AddStringToObject(p, "type", "string");
			cJSON_AddStringToObject(p, "description", "音频文件名");
			cJSON_AddItemToObject(items_props, "file_name", p);
			cJSON_AddItemToArray(items_required, cJSON_CreateString("file_name"));
		}
		/* file_path */
		{
			cJSON *p = cJSON_CreateObject();
			cJSON_AddStringToObject(p, "type", "string");
			cJSON_AddStringToObject(p, "description", "端侧TF卡中的文件路径或稳定文件标识");
			cJSON_AddItemToObject(items_props, "file_path", p);
			cJSON_AddItemToArray(items_required, cJSON_CreateString("file_path"));
		}
		/* display_name */
		{
			cJSON *p = cJSON_CreateObject();
			cJSON_AddStringToObject(p, "type", "string");
			cJSON_AddStringToObject(p, "description", "用于界面展示或播报的音频名称");
			cJSON_AddItemToObject(items_props, "display_name", p);
		}

		cJSON_AddItemToObject(items_schema, "properties", items_props);
		cJSON_AddItemToObject(items_schema, "required", items_required);
		cJSON_AddItemToObject(prop, "items", items_schema);
		cJSON_AddItemToObject(properties, "items", prop);
		cJSON_AddItemToArray(required, cJSON_CreateString("items"));
	}

	/* start_index */
	{
		cJSON *prop = cJSON_CreateObject();
		cJSON_AddStringToObject(prop, "type", "integer");
		cJSON_AddStringToObject(prop, "description",
		    "起播位置，从 items 中的第几个文件开始播放，默认从 0 开始");
		cJSON_AddNumberToObject(prop, "minimum", 0);
		cJSON_AddItemToObject(properties, "start_index", prop);
		cJSON_AddItemToArray(required, cJSON_CreateString("start_index"));
	}

	return tool;
}

static cJSON *play_tf_card_call(const char *id, const char *name, cJSON *args)
{
	(void)id;

	if (!args) {
		return result_create(name, "缺少参数", true);
	}

	/* ---- 解析 mode ---- */
	const cJSON *mode_json = mcp_tool_call_args_get(args, "mode");
	if (!mode_json || !cJSON_IsString(mode_json) ||
	    strcmp(mode_json->valuestring, "single") != 0) {
		return result_create(name, "mode 参数无效，当前仅支持 single", true);
	}

	/* ---- 解析 items 数组 ---- */
	const cJSON *items = mcp_tool_call_args_get(args, "items");
	if (!items || !cJSON_IsArray(items)) {
		return result_create(name, "items 参数无效，必须是数组", true);
	}

	int item_count = cJSON_GetArraySize(items);
	if (item_count <= 0) {
		return result_create(name, "items 数组为空", true);
	}

	/* ---- 解析 start_index ---- */
	const cJSON *start_json = mcp_tool_call_args_get(args, "start_index");
	int start_index = 0;
	if (start_json && cJSON_IsNumber(start_json)) {
		start_index = start_json->valueint;
	}
	if (start_index < 0 || start_index >= item_count) {
		start_index = 0;
	}

	music_item_t *online_tracks = lisa_mem_calloc(item_count, sizeof(music_item_t));
	if (!online_tracks) {
		return result_create(name, "内存不足", true);
	}

	for (int i = 0; i < item_count; i++) {
		const cJSON *item = cJSON_GetArrayItem(items, i);
		if (!item || !cJSON_IsObject(item)) {
			lisa_mem_free(online_tracks);
			return result_create(name, "items 中存在无效曲目", true);
		}

		const char *file_path = cJSON_GetStringValue(
		    cJSON_GetObjectItem(item, "file_path"));
		const char *file_id   = cJSON_GetStringValue(
		    cJSON_GetObjectItem(item, "file_id"));
		const char *display_name = cJSON_GetStringValue(
		    cJSON_GetObjectItem(item, "display_name"));
		const char *file_name = cJSON_GetStringValue(
		    cJSON_GetObjectItem(item, "file_name"));

		if (!file_path || file_path[0] == '\0' ||
		    !file_id || file_id[0] == '\0' ||
		    !file_name || file_name[0] == '\0') {
			lisa_mem_free(online_tracks);
			return result_create(name, "items 中缺少 file_id/file_name/file_path", true);
		}

		music_item_t *t = &online_tracks[i];
		if (build_sd_music_url(t->m_url, sizeof(t->m_url), file_path, file_name) != 0) {
			lisa_mem_free(online_tracks);
			return result_create(name, "TF卡音频文件路径过长", true);
		}

		struct lsfs_dirent file_info = {0};
		int stat_ret = lsfs_stat(t->m_url, &file_info);
		if (stat_ret != 0 || file_info.type != LSFS_DIR_ENTRY_FILE) {
			LOGW("TF card file unavailable: id=%s, path=%s, stat_ret=%d, type=%d",
			     file_id, t->m_url, stat_ret, file_info.type);
			lisa_mem_free(online_tracks);
			return result_create_file_not_found(name);
		}

		snprintf(t->mid, sizeof(t->mid), "%s", file_id);
		fill_track_display_name(t, display_name, file_name);
		LOGI("TF card track[%d]: id=%s, name=%s, url=%s",
		     i, t->mid, t->m_name, t->m_url);
	}

	/* TF 卡云端推荐列表使用 ONLINE 子列表承载，避免覆盖本地全量 OFFLINE 列表。 */
	if (voice_music_list_set(MUSIC_LIST_ONLINE, online_tracks, item_count) != 0) {
		lisa_mem_free(online_tracks);
		return result_create(name, "设置TF卡播放列表失败", true);
	}
	voice_music_list_set_mode(MUSIC_MODE_SEQUENTIAL);
	voice_music_list_set_current_index(start_index);
	lisa_mem_free(online_tracks);

	/* ---- 发布播放信息、停止聊天、推送意图 ---- */
	const char *played_id = "";
	{
		music_item_t current;
		if (voice_music_list_get_current(&current) == 0) {
			played_id = current.mid;
			char *show = current.m_name[0] ? current.m_name : current.m_title;
			voice_msg_pub(VOICE_MSG_CLOUD_MUSIC_NAME,
			              show, strlen(show) + 1);
		}
	}

	if (voice_intent_contains(INTENT_MUSIC)) {
		voice_intent_pop(INTENT_MUSIC);
	}
	if (voice_intent_contains(INTENT_VOICE_SESSION)) {
		LOGI("force finish voice session before TF card music playback");
		voice_intent_pop(INTENT_VOICE_SESSION);
	}
	voice_intent_music_set_user_paused(false);
	voice_msg_pub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, NULL, 0);
	voice_intent_push(INTENT_MUSIC);

	/* ---- 构建成功响应 ---- */
	cJSON *detail = cJSON_CreateObject();
	cJSON_AddBoolToObject(detail, "ok", true);
	cJSON_AddStringToObject(detail, "played_file_id", played_id);
	cJSON_AddStringToObject(detail, "message", "started");
	return result_create_with_detail(name, "已开始播放", false, detail);
}

/* ============================================================================
 * 工具注册
 * ============================================================================ */

MCP_TOOL_DEFINE(ls.built_in.play_tf_card_audio, play_tf_card_list, play_tf_card_call);
