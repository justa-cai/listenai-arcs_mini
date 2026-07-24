#include <string.h>

#define TAG "voice_player_music"
#include "lisa_log.h"
#include "sysheap.h"

#include "app_player.h"
#include "voice_msg.h"
#include "voice_music_list.h"
#include "voice_player_comm.h"
#if CONFIG_LSFS
#include "lsfs.h"
#endif
#include "voice_intent_mgr.h"
#include "voice_intent/voice_intent_music.h"

#include "voice_player_music.h"

/*
 * MUSIC 播放模块：集中管理音乐播放器的初始化、URL 播放、事件处理与播放控制。
 * voice_player_comm 只负责创建底层播放器实例和公共音频平台。
 */

/* ==================== 私有配置 ==================== */

#define ONLINE_TRACK_MAX 50
#define VOICE_PLAYER_SD_URL_PREFIX "/SD:/"

/* ==================== 内部工具：本地文件检查 ==================== */

static bool voice_player_music_url_is_sd_file(const char *url)
{
    return url != NULL &&
           strncmp(url, VOICE_PLAYER_SD_URL_PREFIX,
                   sizeof(VOICE_PLAYER_SD_URL_PREFIX) - 1) == 0;
}

/* SD 卡可能在歌单生成后被拔出，真正播放前再确认文件仍然可访问。 */
static int voice_player_music_check_sd_file(const char *url)
{
    if (!voice_player_music_url_is_sd_file(url)) {
        return APP_PLAYER_OK;
    }

#if CONFIG_LSFS
    struct lsfs_dirent file_info = {0};
    int ret = lsfs_stat(url, &file_info);

    if (ret != 0) {
        LOGW("Music SD file stat failed: url=%s ret=%d", url, ret);
        return APP_PLAYER_ERR_IO;
    }
    if (file_info.type != LSFS_DIR_ENTRY_FILE) {
        LOGW("Music SD path is not file: url=%s type=%d", url, file_info.type);
        return APP_PLAYER_ERR_INVALID_PARAM;
    }
    return APP_PLAYER_OK;
#else
    LOGW("Music SD file check unsupported: url=%s", url);
    return APP_PLAYER_ERR_NOT_SUPPORTED;
#endif
}

/* 仅本地 SD 曲目需要通知上层刷新拔卡/文件失效状态。 */
static void voice_player_music_notify_sd_play_failed(const char *url)
{
    if (voice_player_music_url_is_sd_file(url)) {
        voice_msg_pub(VOICE_MSG_APP_SD_MUSIC_PLAY_FAILED, NULL, 0);
    }
}

/* ==================== 内部工具：intent 管理 ==================== */

/* 从头开始播放新歌单时重建 MUSIC intent，保证音乐成为当前后台播放意图。 */
static void voice_player_music_restart_intent(void)
{
    if (voice_intent_contains(INTENT_MUSIC)) {
        voice_intent_pop(INTENT_MUSIC);
    }
    if (voice_intent_contains(INTENT_VOICE_SESSION)) {
        LOGI("force finish voice session before music playback");
        voice_intent_pop(INTENT_VOICE_SESSION);
    }
    voice_intent_music_set_user_paused(false);
    voice_intent_push(INTENT_MUSIC);
}

/* ==================== 内部工具：曲目信息与播放 ==================== */

/* 通知 UI 更新当前歌曲名；name 为空时清空显示。 */
static void voice_player_music_publish_name(const char *name)
{
    if (name != NULL && name[0] != '\0') {
        voice_msg_pub(VOICE_MSG_CLOUD_MUSIC_NAME, (void *)name, strlen(name) + 1);
    } else {
        voice_msg_pub(VOICE_MSG_CLOUD_MUSIC_NAME, NULL, 0);
    }
}

/* 播放指定曲目，播放成功后同步 UI 歌曲名。 */
static void voice_player_music_play_track(const music_item_t *track)
{
    if (track == NULL) {
        return;
    }

    if (voice_player_play_music_url(track->m_url) == APP_PLAYER_OK &&
        track->m_name[0] != '\0') {
        voice_player_music_publish_name(track->m_name);
    }
}

/* ==================== 播放器回调 ==================== */

/* 将底层 MUSIC 播放事件转换为自动切歌、UI 名称和 intent 消息。 */
static void voice_player_music_event(app_player_t *player,
                                     app_player_event_t event,
                                     void *user_data)
{
    (void)player;
    (void)user_data;

    switch (event) {
    case APP_PLAYER_EVENT_PREPARED:
        LOGI("[MUSIC] Player prepared");
        break;
    case APP_PLAYER_EVENT_PLAYING: {
        music_item_t curr_track;

        LOGI("[MUSIC] Player playing");
        if (voice_music_list_get_current(&curr_track) == 0 &&
            curr_track.m_name[0] != '\0') {
            voice_player_music_publish_name(curr_track.m_name);
        }
        voice_msg_pub(VOICE_MSG_PLAYER_MUSIC_PLAYING, NULL, 0);
        break;
    }
    case APP_PLAYER_EVENT_PAUSED:
        LOGI("[MUSIC] Player paused");
        break;
    case APP_PLAYER_EVENT_COMPLETED: {
        music_item_t next_track;

        LOGI("[MUSIC] Player completed");
        if (voice_music_list_get_next(&next_track) == 0) {
            LOGI("Auto-advancing to next track");
            voice_player_music_play_track(&next_track);
        } else {
            LOGI("Playlist completed");
            voice_player_music_publish_name(NULL);
        }
        break;
    }
    case APP_PLAYER_EVENT_ERROR: {
        music_item_t curr_track;
        music_item_t next_track;

        LOGE("[MUSIC] Player error");
        if (voice_music_list_get_current(&curr_track) == 0) {
            voice_player_music_notify_sd_play_failed(curr_track.m_url);
        }
        if (voice_music_list_get_next(&next_track) == 0) {
            LOGI("Error occurred, trying next track");
            voice_player_music_play_track(&next_track);
        } else {
            voice_player_music_publish_name(NULL);
        }
        break;
    }
    case APP_PLAYER_EVENT_STOPPED:
        LOGI("[MUSIC] Player stopped");
        break;
    default:
        break;
    }
}

/* app_player 按平台焦点策略自动 pause/stop；这里只拦截 tone 结束后的过早恢复，
 * 等待后续 TTS/session 流程通过 MUSIC intent 决定恢复时机。 */
static bool voice_player_music_focus_changed(app_player_t *player,
                                             app_player_focus_state_t state,
                                             app_player_t *by_which,
                                             void *user_data)
{
    (void)player;
    (void)user_data;

    switch (state) {
    case APP_PLAYER_FOCUS_FOREGROUND:
        LOGI("[MUSIC] Got FOREGROUND focus (by player %p)", by_which);
        if (by_which == tone_player) {
            LOGI("[MUSIC] Tone completed, waiting for TTS/session policy");
            return true;
        }
        break;
    case APP_PLAYER_FOCUS_BACKGROUND:
        LOGI("[MUSIC] Moved to BACKGROUND (by player %p)", by_which);
        break;
    case APP_PLAYER_FOCUS_NONE:
        LOGI("[MUSIC] Lost focus (by player %p)", by_which);
        break;
    }

    return false;
}

/* 将云端下发的 audio item 转成内部 music_item_t。 */
static void voice_player_music_fill_track(music_item_t *track,
                                          const struct voice_msg_audio_item *item)
{
    memcpy(track->mid, item->id, sizeof(item->id));
    memcpy(track->m_name, item->name, sizeof(item->name));

    /* play_audio_link 链路会预填充 url，直接复制即可跳过 URL 解析。 */
    if (item->url[0] != '\0') {
        memcpy(track->m_url, item->url, sizeof(item->url));
    }
}

/* ==================== EBUS 回调：云端歌单 ==================== */

/* 云端歌曲列表到达（MCP kuwo / audio_url 等工具）。
 * 替换当前在线歌单为收到的曲目，从头开始播放，并通知 UI 显示第一首歌名。 */
static void voice_player_music_audio_item(void *unused, uint32_t msg_id,
                                          void *data, uint32_t len,
                                          void *user_data)
{
    struct voice_msg_audio_items *msg_items = (struct voice_msg_audio_items *)data;
    int count;
    music_item_t *tracks;
    char first_track_name[AUIDO_OUT_NAME_LEN] = {0};

    (void)unused;
    (void)msg_id;
    (void)len;
    (void)user_data;

    if (msg_items == NULL || msg_items->cnt == 0) {
        LOGE("Invalid data");
        return;
    }

    count = (msg_items->cnt > ONLINE_TRACK_MAX) ? ONLINE_TRACK_MAX : (int)msg_items->cnt;
    tracks = lisa_mem_alloc(count * sizeof(music_item_t));
    if (tracks == NULL) {
        LOGE("Failed to alloc tracks");
        return;
    }
    memset(tracks, 0, count * sizeof(music_item_t));

    for (int i = 0; i < count; i++) {
        voice_player_music_fill_track(&tracks[i], &msg_items->items[i]);
    }

    if (tracks[0].m_name[0] != '\0') {
        strncpy(first_track_name, tracks[0].m_name, sizeof(first_track_name) - 1);
    }

    if (voice_music_list_set(MUSIC_LIST_ONLINE, tracks, count) != 0) {
        LOGE("Failed to set online music list");
        lisa_mem_free(tracks);
        return;
    }
    voice_music_list_set_active(MUSIC_LIST_ONLINE);
    voice_music_list_set_current_index(0);
    lisa_mem_free(tracks);

    voice_player_music_publish_name(first_track_name);
    voice_player_music_restart_intent();
}

/* ==================== EBUS 回调：播放控制 ==================== */

/* 用户主动播放控制（MCP ls.playback_control 工具驱动）。
 *
 * PLAY/PAUSE 只改变播放状态，不改变 MUSIC intent（仍在栈上）。
 * PAUSE 通过 user_paused 标记阻止后续自动 resume。
 * NEXT/PREVIOUS/REPLAY 切换曲目，同时清除 user_paused。
 * STOP 直接 pop MUSIC，让 intent on_exit 停止播放器。 */
static void voice_player_music_play_control(void *unused, uint32_t msg_id,
                                            void *data, uint32_t len,
                                            void *user_data)
{
    (void)unused;
    (void)data;
    (void)len;
    (void)user_data;

    switch (msg_id) {
    case VOICE_MSG_PLAY_CONTROL_PLAY:
        voice_intent_music_set_user_paused(false);
        app_player_resume(music_player);
        break;
    case VOICE_MSG_PLAY_CONTROL_PAUSE:
        voice_intent_music_set_user_paused(true);
        app_player_pause(music_player);
        voice_player_music_publish_name(NULL);
        break;
    case VOICE_MSG_PLAY_CONTROL_NEXT: {
        music_item_t next_track;
        voice_intent_music_set_user_paused(false);
        if (voice_music_list_get_next(&next_track) == 0) {
            voice_player_music_play_track(&next_track);
        }
    } break;
    case VOICE_MSG_PLAY_CONTROL_PREVIOUS: {
        music_item_t prev_track;
        voice_intent_music_set_user_paused(false);
        if (voice_music_list_get_prev(&prev_track) == 0) {
            voice_player_music_play_track(&prev_track);
        }
    } break;
    case VOICE_MSG_PLAY_CONTROL_REPLAY: {
        music_item_t curr_track;
        voice_intent_music_set_user_paused(false);
        if (voice_music_list_get_current(&curr_track) == 0) {
            voice_player_music_play_track(&curr_track);
        }
    } break;
    case VOICE_MSG_PLAY_CONTROL_STOP:
        if (voice_intent_contains(INTENT_MUSIC)) {
            voice_player_music_publish_name(NULL);
            voice_intent_pop(INTENT_MUSIC);
        }
        break;
    default:
        break;
    }
}

/* ==================== 对外 API ==================== */

int voice_player_play_music_url(const char *url)
{
    int ret;

    if (music_player == NULL || url == NULL || url[0] == '\0') {
        LOGW("Music play rejected: player=%p url=%p",
             (void *)music_player, (const void *)url);
        return APP_PLAYER_ERR_INVALID_PARAM;
    }

    ret = voice_player_music_check_sd_file(url);
    if (ret != APP_PLAYER_OK) {
        voice_player_music_notify_sd_play_failed(url);
        return ret;
    }

    ret = app_player_play(music_player, url);
    if (ret != APP_PLAYER_OK) {
        if (voice_player_music_url_is_sd_file(url)) {
            LOGW("Music SD play failed: url=%s ret=%d", url, ret);
        }
        voice_player_music_notify_sd_play_failed(url);
    }

    return ret;
}

int voice_player_music_init(void)
{
    int ret = 0;

    if (music_player == NULL) {
        LOGE("music player is not ready");
        return -1;
    }
    if (voice_music_list_init() != 0) {
        LOGE("music list init failed");
        return -1;
    }
    if (app_player_register_callback(music_player,
                                     voice_player_music_event,
                                     NULL) != APP_PLAYER_OK) {
        LOGE("music player event callback registration failed");
        return -1;
    }
    if (app_player_register_focus_cb(music_player,
                                     voice_player_music_focus_changed,
                                     NULL) != APP_PLAYER_OK) {
        LOGE("music player focus callback registration failed");
        return -1;
    }

    ret |= voice_msg_sub(VOICE_MSG_CLOUD_AUDIO_ITEM, voice_player_music_audio_item, NULL);
    ret |= voice_msg_sub(VOICE_MSG_PLAY_CONTROL_PLAY, voice_player_music_play_control, NULL);
    ret |= voice_msg_sub(VOICE_MSG_PLAY_CONTROL_PAUSE, voice_player_music_play_control, NULL);
    ret |= voice_msg_sub(VOICE_MSG_PLAY_CONTROL_NEXT, voice_player_music_play_control, NULL);
    ret |= voice_msg_sub(VOICE_MSG_PLAY_CONTROL_PREVIOUS, voice_player_music_play_control, NULL);
    ret |= voice_msg_sub(VOICE_MSG_PLAY_CONTROL_REPLAY, voice_player_music_play_control, NULL);
    ret |= voice_msg_sub(VOICE_MSG_PLAY_CONTROL_STOP, voice_player_music_play_control, NULL);

    return ret == 0 ? 0 : -1;
}
