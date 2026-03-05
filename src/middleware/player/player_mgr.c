#define TAG "player_mgr"

#include "player_mgr.h"
#include "sound_player.h"
#include "audio_player.h"
#include "short_player.h"
#include "listen_volume.h"
#include "app_player.h"
#include "evs_utils.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include <stdbool.h>
#include <string.h>

#define MAX_PLAYER_COUNT 8

typedef enum {
    PLAYER_TYPE_SOUND = 0,  // sound_player（TTS/LOCAL等）
    PLAYER_TYPE_AUDIO,      // audio_player（CONTENT）
    PLAYER_TYPE_AIP,        // AIP（识别，使用short_player）
} player_type_e;

typedef struct {
    player_status_cb_t cb;
    void *arg;
} status_cb_item_t;

typedef struct {
    player_focus_cb_t cb;
    void *arg;
} focus_cb_item_t;

typedef struct player_entry_s {
    int id;
    player_type_e type;
    void *player;           // sound_player_t* 或 audioplayer_t*
    player_config_t config;
} player_entry_t;

typedef struct player_mgr_s {
    listen_audiomgr_t *audio_mgr;
    player_entry_t players[MAX_PLAYER_COUNT];
    int player_count;
    status_cb_item_t status_cbs[MAX_PLAYER_COUNT];
    focus_cb_item_t focus_cbs[MAX_PLAYER_COUNT];
    
    // AIP 相关
    int aip_id;
    bool aip_focus_acquired;
    char *aip_pending_url;
    short_player_t *short_player;
} player_mgr_t;

static player_mgr_t *g_player_mgr = NULL;

// ============== 查找播放器 ==============

static player_entry_t *_get_player_entry(int id)
{
    if (g_player_mgr == NULL) {
        return NULL;
    }
    for (int i = 0; i < g_player_mgr->player_count; i++) {
        if (g_player_mgr->players[i].id == id) {
            return &g_player_mgr->players[i];
        }
    }
    return NULL;
}

static int _get_player_index(int id)
{
    if (g_player_mgr == NULL) {
        return -1;
    }
    for (int i = 0; i < g_player_mgr->player_count; i++) {
        if (g_player_mgr->players[i].id == id) {
            return i;
        }
    }
    return -1;
}

player_config_t *player_mgr_get_config(int player_id)
{
    player_entry_t *entry = _get_player_entry(player_id);
    if (entry != NULL) {
        return &entry->config;
    }
    return NULL;
}

// ============== 回调转发 ==============

static void _on_sound_status(uint16_t status)
{
    // 找到当前前景的 sound_player
    if (g_player_mgr == NULL) return;

    for (int i = 0; i < g_player_mgr->player_count; i++) {
        if (g_player_mgr->players[i].type == PLAYER_TYPE_SOUND) {
            sound_player_t *sp = (sound_player_t *)g_player_mgr->players[i].player;
            if (sp != NULL && sp->m_focus_state == FOREGROUND) {
                int id = g_player_mgr->players[i].id;
                if (g_player_mgr->status_cbs[i].cb != NULL) {
                    g_player_mgr->status_cbs[i].cb(id, status, g_player_mgr->status_cbs[i].arg);
                }
                break;
            }
        }
    }
}

static void _on_sound_focus(focus_state_e state, int by_which)
{
    // 找到对应的 sound_player
    if (g_player_mgr == NULL) return;

    for (int i = 0; i < g_player_mgr->player_count; i++) {
        if (g_player_mgr->players[i].type == PLAYER_TYPE_SOUND) {
            sound_player_t *sp = (sound_player_t *)g_player_mgr->players[i].player;
            if (sp != NULL && sp->m_focus_state == state) {
                int id = g_player_mgr->players[i].id;
                if (g_player_mgr->focus_cbs[i].cb != NULL) {
                    g_player_mgr->focus_cbs[i].cb(id, state, by_which, g_player_mgr->focus_cbs[i].arg);
                }
                break;
            }
        }
    }
}

static void _on_audio_status(uint16_t status)
{
    if (g_player_mgr == NULL) return;
    
    // 找到 audio_player 的索引
    for (int i = 0; i < g_player_mgr->player_count; i++) {
        if (g_player_mgr->players[i].type == PLAYER_TYPE_AUDIO) {
            int id = g_player_mgr->players[i].id;
            if (g_player_mgr->status_cbs[i].cb != NULL) {
                g_player_mgr->status_cbs[i].cb(id, status, g_player_mgr->status_cbs[i].arg);
            }
            break;
        }
    }
}

static void _on_audio_focus(focus_state_e state, int by_which)
{
    if (g_player_mgr == NULL) return;
    
    // 找到 audio_player 的索引
    for (int i = 0; i < g_player_mgr->player_count; i++) {
        if (g_player_mgr->players[i].type == PLAYER_TYPE_AUDIO) {
            int id = g_player_mgr->players[i].id;
            if (g_player_mgr->focus_cbs[i].cb != NULL) {
                g_player_mgr->focus_cbs[i].cb(id, state, by_which, g_player_mgr->focus_cbs[i].arg);
            }
            break;
        }
    }
}

// ============== AIP 处理（使用 short_player）==============

static void _on_aip_focus(focus_state_e state, int by_which)
{
    if (g_player_mgr == NULL) {
        return;
    }

    LISA_LOGI(TAG, "AIP focus: %s, by_which=%d",
              listen_audiomgr_get_state_name(state), by_which);
    
    if (state == FOREGROUND) {
        g_player_mgr->aip_focus_acquired = true;
        // 播放唤醒音
        if (g_player_mgr->aip_pending_url != NULL && g_player_mgr->short_player != NULL) {
            listen_shortplayer_play(g_player_mgr->short_player, g_player_mgr->aip_pending_url);
            lisa_mem_free(g_player_mgr->aip_pending_url);
            g_player_mgr->aip_pending_url = NULL;
        }
    } else {
        g_player_mgr->aip_focus_acquired = false;
        if (g_player_mgr->short_player != NULL) {
            listen_shortplayer_stop(g_player_mgr->short_player);
        }
    }
    
    // 通知上层
    int idx = _get_player_index(g_player_mgr->aip_id);
    if (idx >= 0 && g_player_mgr->focus_cbs[idx].cb != NULL) {
        g_player_mgr->focus_cbs[idx].cb(g_player_mgr->aip_id, state, by_which,
                                        g_player_mgr->focus_cbs[idx].arg);
    }
}

static int _create_aip_player(player_config_t *config)
{
    g_player_mgr->aip_id = config->id;
    g_player_mgr->aip_focus_acquired = false;
    g_player_mgr->aip_pending_url = NULL;
    
    // 创建 short_player
    g_player_mgr->short_player = listen_shortplayer_create();

    // 注册到焦点管理器
    int ret = listen_audiomgr_register_channel(
        g_player_mgr->audio_mgr,
        config->id,
        config->name,
        config->priority,
        config->capture_ids,
        config->capture_count,
        _on_aip_focus
    );

    if (ret != 0) {
        LISA_LOGE(TAG, "Failed to register AIP channel");
        return -1;
    }
    
    // 保存到播放器列表
    int idx = g_player_mgr->player_count;
    g_player_mgr->players[idx].id = config->id;
    g_player_mgr->players[idx].type = PLAYER_TYPE_AIP;
    g_player_mgr->players[idx].player = NULL;
    memcpy(&g_player_mgr->players[idx].config, config, sizeof(player_config_t));
    g_player_mgr->player_count++;

    LISA_LOGI(TAG, "AIP player created: id=%d, name=%s", config->id, config->name);
    return 0;
}

// ============== 异步任务 ==============

typedef struct {
    int player_id;
    char *url;
    int throw_time_ms;
} play_param_t;

typedef struct {
    int player_id;
    audio_out_t item;
    bool interrupt;
} play_item_param_t;

typedef struct {
    int player_id;
    audio_out_t *items;
    int count;
} play_array_param_t;

typedef struct {
    int player_id;
} player_id_param_t;

typedef struct {
    int player_id;
    PLAY_MODE_E mode;
} switch_mode_param_t;

static int _play_impl(void *arg)
{
    play_param_t *p = (play_param_t *)arg;
    if (p == NULL) return 0;

    player_entry_t *entry = _get_player_entry(p->player_id);
    if (entry != NULL && p->url != NULL) {
        if (entry->type == PLAYER_TYPE_SOUND) {
            sound_player_t *player = (sound_player_t *)entry->player;
            if (player == NULL) {
                LISA_LOGE(TAG, "Player %d play not supported (player=%p)", p->player_id, player);
            } else {
                listen_soundplayer_play(player, p->url, true);
            }
        } else if (entry->type == PLAYER_TYPE_AUDIO) {
            audio_out_t item;
            memset(&item, 0, sizeof(audio_out_t));
            strncpy(item.m_url, p->url, sizeof(item.m_url) - 1);
            audioplayer_t *player = (audioplayer_t *)entry->player;
            if (player == NULL || player->on_directive == NULL) {
                LISA_LOGE(TAG, "Player %d play not supported (player=%p)", p->player_id, player);
            } else {
                player->on_directive(player, AUDIO_PLAY, &item, 1);
            }
        } else if (entry->type == PLAYER_TYPE_AIP) {
            // AIP：保存URL，请求焦点
            if (g_player_mgr->aip_pending_url != NULL) {
                lisa_mem_free(g_player_mgr->aip_pending_url);
            }
        g_player_mgr->aip_pending_url = (char *)lisa_mem_alloc(strlen(p->url) + 1);
            if (g_player_mgr->aip_pending_url != NULL) {
                strcpy(g_player_mgr->aip_pending_url, p->url);
            }
            listen_audiomgr_acquire_channel(g_player_mgr->audio_mgr, g_player_mgr->aip_id);
        }
    }

    if (p->url != NULL) lisa_mem_free(p->url);
    lisa_mem_free(p);
    return 0;
}

static int _play_item_impl(void *arg)
{
    play_item_param_t *p = (play_item_param_t *)arg;
    if (p == NULL) return 0;

    player_entry_t *entry = _get_player_entry(p->player_id);
    if (entry != NULL && entry->type == PLAYER_TYPE_SOUND) {
        sound_player_t *player = (sound_player_t *)entry->player;
        if (player == NULL) {
            LISA_LOGE(TAG, "Player %d play_item not supported (player=%p)", p->player_id, player);
        } else {
            listen_soundplayer_play_item(player, &p->item, p->interrupt);
        }
    }

    lisa_mem_free(p);
    return 0;
}
    
static int _play_array_impl(void *arg)
{
    play_array_param_t *p = (play_array_param_t *)arg;
    if (p == NULL) return 0;

    player_entry_t *entry = _get_player_entry(p->player_id);
    if (entry != NULL && p->items != NULL) {
        if (entry->type == PLAYER_TYPE_SOUND) {
            sound_player_t *player = (sound_player_t *)entry->player;
            if (player == NULL) {
                LISA_LOGE(TAG, "Player %d play_array not supported (player=%p)", p->player_id, player);
            } else {
                listen_soundplayer_play_array(player, p->items, p->count);
            }
        } else if (entry->type == PLAYER_TYPE_AUDIO) {
            audioplayer_t *player = (audioplayer_t *)entry->player;
            if (player == NULL || player->on_directive == NULL) {
                LISA_LOGE(TAG, "Player %d play_array not supported (player=%p)", p->player_id, player);
            } else {
                player->on_directive(player, AUDIO_PLAY, p->items, p->count);
            }
        }
    }

    if (p->items != NULL) lisa_mem_free(p->items);
    lisa_mem_free(p);
    return 0;
}

static int _stop_impl(void *arg)
{
    player_id_param_t *p = (player_id_param_t *)arg;
    if (p == NULL) return 0;

    player_entry_t *entry = _get_player_entry(p->player_id);
    if (entry != NULL) {
        if (entry->type == PLAYER_TYPE_SOUND) {
            // listen_soundplayer_stop((sound_player_t *)entry->player);
            sound_player_t *player = (sound_player_t *)entry->player;
            if (player == NULL || player->stop == NULL) {
                LISA_LOGE(TAG, "Player %d stop not supported (player=%p)", p->player_id, player);
            } else {
                player->stop(player, false);
            }
        } else if (entry->type == PLAYER_TYPE_AUDIO) {
            audioplayer_t *player = (audioplayer_t *)entry->player;
            if (player == NULL || player->stop == NULL) {
                LISA_LOGE(TAG, "Player %d stop not supported (player=%p)", p->player_id, player);
            } else {
                player->stop(player, false);
            }
            // 恢复被暂停的 audio_player
            if (player != NULL) {
                listen_audioplayer_resume(player);
            }
        } else if (entry->type == PLAYER_TYPE_AIP) {
            listen_audiomgr_release_channel(g_player_mgr->audio_mgr, g_player_mgr->aip_id);
            // 找到 audio_player 并恢复
            for (int i = 0; i < g_player_mgr->player_count; i++) {
                if (g_player_mgr->players[i].type == PLAYER_TYPE_AUDIO) {
                    listen_audioplayer_resume((audioplayer_t *)g_player_mgr->players[i].player);
                    break;
                }
            }
        }
    }
    
    lisa_mem_free(p);
    return 0;
}

static int _pause_impl(void *arg)
{
    player_id_param_t *p = (player_id_param_t *)arg;
    if (p == NULL) return 0;

    player_entry_t *entry = _get_player_entry(p->player_id);
    if (entry != NULL && entry->type == PLAYER_TYPE_AUDIO) {
        audioplayer_t *player = (audioplayer_t *)entry->player;
        if (player == NULL || player->pause == NULL) {
            LISA_LOGE(TAG, "Player %d pause not supported (player=%p)", p->player_id, player);
        } else {
            player->pause(player);
        }
    }
    
    lisa_mem_free(p);
    return 0;
}

static int _resume_impl(void *arg)
{
    player_id_param_t *p = (player_id_param_t *)arg;
    if (p == NULL) return 0;

    player_entry_t *entry = _get_player_entry(p->player_id);
    if (entry != NULL && entry->type == PLAYER_TYPE_AUDIO) {
        audioplayer_t *player = (audioplayer_t *)entry->player;
        if (player == NULL || player->resumeByVoice == NULL) {
            LISA_LOGE(TAG, "Player %d resume not supported (player=%p)", p->player_id, player);
        } else {
            player->resumeByVoice(player);
        }
    }
    
    lisa_mem_free(p);
    return 0;
}

static int _next_impl(void *arg)
{
    player_id_param_t *p = (player_id_param_t *)arg;
    if (p == NULL) return 0;

    player_entry_t *entry = _get_player_entry(p->player_id);
    if (entry != NULL && entry->type == PLAYER_TYPE_AUDIO) {
        audioplayer_t *player = (audioplayer_t *)entry->player;
        if (player == NULL || player->next == NULL) {
            LISA_LOGE(TAG, "Player %d next not supported (player=%p)", p->player_id, player);
        } else {
            player->next(player);
        }
    }

    lisa_mem_free(p);
    return 0;
}
    
static int _prev_impl(void *arg)
{
    player_id_param_t *p = (player_id_param_t *)arg;
    if (p == NULL) return 0;

    player_entry_t *entry = _get_player_entry(p->player_id);
    if (entry != NULL && entry->type == PLAYER_TYPE_AUDIO) {
        audioplayer_t *player = (audioplayer_t *)entry->player;
        if (player == NULL || player->prev == NULL) {
            LISA_LOGE(TAG, "Player %d prev not supported (player=%p)", p->player_id, player);
        } else {
            player->prev(player);
        }
    }
    
    lisa_mem_free(p);
    return 0;
}

static int _switch_mode_impl(void *arg)
{
    switch_mode_param_t *p = (switch_mode_param_t *)arg;
    if (p == NULL) return 0;

    player_entry_t *entry = _get_player_entry(p->player_id);
    if (entry != NULL && entry->type == PLAYER_TYPE_AUDIO) {
        audioplayer_t *player = (audioplayer_t *)entry->player;
        if (player == NULL) {
            LISA_LOGE(TAG, "Player %d switch_mode not supported (player=%p)", p->player_id, player);
        } else {
            listen_audioplayer_switch_mode(player, p->mode);
        }
    }
    
    lisa_mem_free(p);
    return 0;
}

// ============== 公开接口 ==============

int player_mgr_init(player_config_t *configs, int config_count)
{
    if (g_player_mgr != NULL) {
        LISA_LOGW(TAG, "player_mgr already initialized");
        return 0;
    }

    if (configs == NULL || config_count <= 0) {
        LISA_LOGE(TAG, "Invalid parameters");
        return -1;
    }

    g_player_mgr = (player_mgr_t *)lisa_mem_calloc(1, sizeof(player_mgr_t));
    if (g_player_mgr == NULL) {
        LISA_LOGE(TAG, "Failed to allocate player_mgr");
        return -1;
    }
    
    // 初始化底层播放器
    app_player_init();
    listen_volume_init();

    // 创建焦点管理器
    g_player_mgr->audio_mgr = listen_audiomgr_create();
    if (g_player_mgr->audio_mgr == NULL) {
        LISA_LOGE(TAG, "Failed to create audio_mgr");
        lisa_mem_free(g_player_mgr);
        g_player_mgr = NULL;
        return -1;
    }
    
    g_player_mgr->player_count = 0;
    g_player_mgr->aip_id = -1;
    g_player_mgr->short_player = NULL;

    // 根据配置创建播放器
    for (int i = 0; i < config_count && g_player_mgr->player_count < MAX_PLAYER_COUNT; i++) {
        player_config_t *cfg = &configs[i];
        int idx = g_player_mgr->player_count;

        // 根据 bg_action 判断播放器类型
        if (cfg->fg_action == PLAY_ACTION_RECOGNIZE) {
            // AIP 识别
            _create_aip_player(cfg);
        } else if (cfg->bg_action == PLAY_ACTION_PAUSE) {
            // 支持暂停 -> audio_player
            audio_player_config_t audio_cfg = {
                .id = cfg->id,
                .name = cfg->name,
                .priority = cfg->priority,
                .capture_count = cfg->capture_count,
            };
            memcpy(audio_cfg.capture_ids, cfg->capture_ids, sizeof(audio_cfg.capture_ids));

            audioplayer_t *player = listen_audioplayer_create(&audio_cfg, g_player_mgr->audio_mgr);
            if (player != NULL) {
                audio_player_callback_t *cb = (audio_player_callback_t *)lisa_mem_calloc(1, sizeof(audio_player_callback_t));
                if (cb != NULL) {
                    cb->on_focus_state = _on_audio_focus;
                    cb->on_play_state = _on_audio_status;
                    listen_audioplayer_add_callback(player, cb);
                }

                g_player_mgr->players[idx].id = cfg->id;
                g_player_mgr->players[idx].type = PLAYER_TYPE_AUDIO;
                g_player_mgr->players[idx].player = player;
                memcpy(&g_player_mgr->players[idx].config, cfg, sizeof(player_config_t));
                g_player_mgr->player_count++;

                LISA_LOGI(TAG, "Audio player created: id=%d, name=%s", cfg->id, cfg->name);
            }
        } else {
            // 其他 -> sound_player
            sound_player_config_t sound_cfg = {
                .id = cfg->id,
                .name = cfg->name,
                .priority = cfg->priority,
                .capture_count = cfg->capture_count,
            };
            memcpy(sound_cfg.capture_ids, cfg->capture_ids, sizeof(sound_cfg.capture_ids));

            sound_player_t *player = listen_soundplayer_create(&sound_cfg, g_player_mgr->audio_mgr);
            if (player != NULL) {
                sound_player_callback_t *cb = (sound_player_callback_t *)lisa_mem_calloc(1, sizeof(sound_player_callback_t));
                if (cb != NULL) {
                    cb->on_focus_state = _on_sound_focus;
                    cb->on_play_state = _on_sound_status;
                    listen_soundplayer_add_callback(player, cb);
                }

                g_player_mgr->players[idx].id = cfg->id;
                g_player_mgr->players[idx].type = PLAYER_TYPE_SOUND;
                g_player_mgr->players[idx].player = player;
                memcpy(&g_player_mgr->players[idx].config, cfg, sizeof(player_config_t));
                g_player_mgr->player_count++;

                LISA_LOGI(TAG, "Sound player created: id=%d, name=%s", cfg->id, cfg->name);
            }
        }
    }

    LISA_LOGI(TAG, "player_mgr initialized: %d players", g_player_mgr->player_count);
    return 0;
}

int player_mgr_register_status_cb(int player_id, player_status_cb_t cb, void *arg)
{
    int idx = _get_player_index(player_id);
    if (idx < 0) {
        LISA_LOGE(TAG, "Player not found: %d", player_id);
        return -1;
    }
    
    g_player_mgr->status_cbs[idx].cb = cb;
    g_player_mgr->status_cbs[idx].arg = arg;
    return 0;
}

int player_mgr_register_focus_cb(int player_id, player_focus_cb_t cb, void *arg)
{
    int idx = _get_player_index(player_id);
    if (idx < 0) {
        LISA_LOGE(TAG, "Player not found: %d", player_id);
        return -1;
    }

    g_player_mgr->focus_cbs[idx].cb = cb;
    g_player_mgr->focus_cbs[idx].arg = arg;
    return 0;
}

int player_mgr_play(int player_id, const char *url, int throw_time_ms)
{
    if (g_player_mgr == NULL || url == NULL) {
        return -1;
    }
    
    LISA_LOGI(TAG, "player_mgr_play: id=%d, url=%s", player_id, url);

    play_param_t *param = (play_param_t *)lisa_mem_calloc(1, sizeof(play_param_t));
    if (param == NULL) return -1;

    param->player_id = player_id;
    param->throw_time_ms = throw_time_ms;
    param->url = (char *)lisa_mem_alloc(strlen(url) + 1);
    if (param->url != NULL) strcpy(param->url, url);

    return evs_handler_post_runnable(_play_impl, param);
}

int player_mgr_play_item(int player_id, audio_out_t *item, bool interrupt)
{
    if (g_player_mgr == NULL || item == NULL) {
        return -1;
    }

    LISA_LOGI(TAG, "player_mgr_play_item: id=%d, interrupt=%d", player_id, interrupt);

    play_item_param_t *param = (play_item_param_t *)lisa_mem_calloc(1, sizeof(play_item_param_t));
    if (param == NULL) return -1;

    param->player_id = player_id;
    memcpy(&param->item, item, sizeof(audio_out_t));
    param->interrupt = interrupt;

    return evs_handler_post_runnable(_play_item_impl, param);
}

int player_mgr_play_array(int player_id, const audio_out_t *items, int count)
{
    if (g_player_mgr == NULL || items == NULL || count <= 0) {
        return -1;
    }
    
    LISA_LOGI(TAG, "player_mgr_play_array: id=%d, count=%d", player_id, count);

    play_array_param_t *param = (play_array_param_t *)lisa_mem_calloc(1, sizeof(play_array_param_t));
    if (param == NULL) return -1;

    param->player_id = player_id;
    param->count = count;
    param->items = (audio_out_t *)lisa_mem_alloc(sizeof(audio_out_t) * count);
    if (param->items != NULL) {
        memcpy(param->items, items, sizeof(audio_out_t) * count);
    }

    return evs_handler_post_runnable(_play_array_impl, param);
}

int player_mgr_stop(int player_id)
{
    if (g_player_mgr == NULL) return -1;

    LISA_LOGI(TAG, "player_mgr_stop: id=%d", player_id);

    player_id_param_t *param = (player_id_param_t *)lisa_mem_calloc(1, sizeof(player_id_param_t));
    if (param == NULL) return -1;

    param->player_id = player_id;
    return evs_handler_post_runnable(_stop_impl, param);
}

int player_mgr_pause(int player_id)
{
    if (g_player_mgr == NULL) return -1;

    player_entry_t *entry = _get_player_entry(player_id);
    if (entry == NULL || entry->type != PLAYER_TYPE_AUDIO) {
        LISA_LOGW(TAG, "Player %d does not support pause", player_id);
        return -1;
    }
    
    LISA_LOGI(TAG, "player_mgr_pause: id=%d", player_id);

    player_id_param_t *param = (player_id_param_t *)lisa_mem_calloc(1, sizeof(player_id_param_t));
    if (param == NULL) return -1;

    param->player_id = player_id;
    return evs_handler_post_runnable(_pause_impl, param);
}

int player_mgr_resume(int player_id)
{
    if (g_player_mgr == NULL) return -1;

    player_entry_t *entry = _get_player_entry(player_id);
    if (entry == NULL || entry->type != PLAYER_TYPE_AUDIO) {
        LISA_LOGW(TAG, "Player %d does not support resume", player_id);
        return -1;
    }
    
    LISA_LOGI(TAG, "player_mgr_resume: id=%d", player_id);

    player_id_param_t *param = (player_id_param_t *)lisa_mem_calloc(1, sizeof(player_id_param_t));
    if (param == NULL) return -1;

    param->player_id = player_id;
    return evs_handler_post_runnable(_resume_impl, param);
}

int player_mgr_play_next(int player_id)
{
    if (g_player_mgr == NULL) return -1;

    player_entry_t *entry = _get_player_entry(player_id);
    if (entry == NULL || entry->type != PLAYER_TYPE_AUDIO) {
        LISA_LOGW(TAG, "Player %d does not support next", player_id);
        return -1;
    }
    
    LISA_LOGI(TAG, "player_mgr_play_next: id=%d", player_id);

    player_id_param_t *param = (player_id_param_t *)lisa_mem_calloc(1, sizeof(player_id_param_t));
    if (param == NULL) return -1;

    param->player_id = player_id;
    return evs_handler_post_runnable(_next_impl, param);
}

int player_mgr_play_prev(int player_id)
{
    if (g_player_mgr == NULL) return -1;

    player_entry_t *entry = _get_player_entry(player_id);
    if (entry == NULL || entry->type != PLAYER_TYPE_AUDIO) {
        LISA_LOGW(TAG, "Player %d does not support prev", player_id);
        return -1;
    }
    
    LISA_LOGI(TAG, "player_mgr_play_prev: id=%d", player_id);

    player_id_param_t *param = (player_id_param_t *)lisa_mem_calloc(1, sizeof(player_id_param_t));
    if (param == NULL) return -1;

    param->player_id = player_id;
    return evs_handler_post_runnable(_prev_impl, param);
}

int player_mgr_switch_playmode(int player_id, PLAY_MODE_E mode)
{
    if (g_player_mgr == NULL) return -1;

    player_entry_t *entry = _get_player_entry(player_id);
    if (entry == NULL || entry->type != PLAYER_TYPE_AUDIO) {
        LISA_LOGW(TAG, "Player %d does not support switch mode", player_id);
        return -1;
    }
    
    LISA_LOGI(TAG, "player_mgr_switch_playmode: id=%d, mode=%d", player_id, mode);

    switch_mode_param_t *param = (switch_mode_param_t *)lisa_mem_calloc(1, sizeof(switch_mode_param_t));
    if (param == NULL) return -1;

    param->player_id = player_id;
    param->mode = mode;
    return evs_handler_post_runnable(_switch_mode_impl, param);
}

int player_mgr_set_volume(int volume)
{
	listen_set_volume(volume);
    return 0;
}

int player_mgr_get_volume(void)
{
    return listen_get_volume();
}
