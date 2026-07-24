#include <string.h>
#include <stdio.h>

#include "cJSON.h"
#include "lisa_mem.h"
#include "lisa_ui_anim_ext.h"
#include "lisa_ui_assets.h"
#include "lv_img_utils.h"

#define LOG_TAG "emoji_anim"
#include "lisa_ui_log.h"

#include "romfs/romfs.h"

#define EMOJI_CONFIG_PATH   "/emoji.json"
#define EMOJI_DEFAULT_DELAY 100
#define EMOJI_MAX_COUNT     64

struct emoji_phase_owner {
    char *path_pattern;
    int frame_begin;
    int frame_end;
};

struct emoji_config {
    char *name;
    char *alias_name;
    int alias_idx;
    uint8_t valid;
    lisa_ui_anim_ext_config_t anim;
    struct emoji_phase_owner enter_owner;
    struct emoji_phase_owner loop_owner;
    struct emoji_phase_owner exit_owner;
};

static struct emoji_config *s_emoji_configs;
static uint32_t s_emoji_count;
static const lisa_ui_anim_ext_config_t *s_fallback_anim;
static char *s_fallback_name;
static const lisa_ui_anim_ext_config_t s_empty_anim;
static int s_emoji_offset_x;
static int s_emoji_offset_y;

static char *emoji_strdup(const char *src)
{
    uint32_t len;
    char *dst;

    if (src == NULL) {
        return NULL;
    }

    len = strlen(src) + 1;
    dst = lisa_mem_alloc(len);
    if (dst == NULL) {
        return NULL;
    }

    memcpy(dst, src, len);
    return dst;
}

static void emoji_anim_config_free(lisa_ui_anim_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    if (cfg->delays != NULL) {
        lisa_mem_free((void *)cfg->delays);
        cfg->delays = NULL;
    }

    if (cfg->frames != NULL) {
        lisa_mem_free((void *)cfg->frames);
        cfg->frames = NULL;
    }
}

static void emoji_phase_owner_free(struct emoji_phase_owner *owner)
{
    if (owner == NULL) {
        return;
    }

    lisa_mem_free(owner->path_pattern);
    memset(owner, 0, sizeof(*owner));
}

static void emoji_config_entry_release(struct emoji_config *cfg)
{
    if (cfg == NULL) {
        return;
    }

    lisa_mem_free(cfg->name);
    lisa_mem_free(cfg->alias_name);
    emoji_anim_config_free(&cfg->anim.enter);
    emoji_anim_config_free(&cfg->anim.loop);
    emoji_anim_config_free(&cfg->anim.exit);
    emoji_phase_owner_free(&cfg->enter_owner);
    emoji_phase_owner_free(&cfg->loop_owner);
    emoji_phase_owner_free(&cfg->exit_owner);
}

/* Drop a single emoji: free its allocations and reset the slot to an
   invalid (skipped) state, leaving the rest of the array untouched. */
static void emoji_config_invalidate(struct emoji_config *cfg)
{
    if (cfg == NULL) {
        return;
    }

    emoji_config_entry_release(cfg);
    memset(cfg, 0, sizeof(*cfg));
    cfg->alias_idx = -1;
    cfg->valid = 0;
}

static void emoji_anim_cleanup(void)
{
    if (s_emoji_configs != NULL) {
        for (uint32_t i = 0; i < s_emoji_count; i++) {
            emoji_config_entry_release(&s_emoji_configs[i]);
        }
        lisa_mem_free(s_emoji_configs);
    }

    s_emoji_configs = NULL;
    s_emoji_count = 0;
    s_fallback_anim = NULL;
    s_emoji_offset_x = 0;
    s_emoji_offset_y = 0;
    lisa_mem_free(s_fallback_name);
    s_fallback_name = NULL;
}

static int json_get_number(const cJSON *obj, const char *key, int *out)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(item)) {
        return -1;
    }

    *out = item->valueint;
    return 0;
}

static void parse_global_offset(const cJSON *root_json)
{
    const cJSON *offset_json = cJSON_GetObjectItemCaseSensitive(root_json, "offset");

    s_emoji_offset_x = 0;
    s_emoji_offset_y = 0;

    if (!cJSON_IsObject(offset_json)) {
        LISA_UI_LOGW("emoji offset missing or invalid, use default offset: x=0, y=0");
        return;
    }

    if (json_get_number(offset_json, "x", &s_emoji_offset_x) != 0) {
        s_emoji_offset_x = 0;
    }

    if (json_get_number(offset_json, "y", &s_emoji_offset_y) != 0) {
        s_emoji_offset_y = 0;
    }
}

static int parse_phase_range(const cJSON *phase_json, int *frame_begin, int *frame_end)
{
    const cJSON *range_json = cJSON_GetObjectItemCaseSensitive(phase_json, "range");
    const cJSON *begin_json;
    const cJSON *end_json;

    if (!cJSON_IsArray(range_json) || cJSON_GetArraySize(range_json) != 2) {
        LISA_UI_LOGE("Phase range must be [begin, end]");
        return -1;
    }

    begin_json = cJSON_GetArrayItem(range_json, 0);
    end_json = cJSON_GetArrayItem(range_json, 1);
    if (!cJSON_IsNumber(begin_json) || !cJSON_IsNumber(end_json)) {
        LISA_UI_LOGE("Phase range item must be number");
        return -1;
    }

    *frame_begin = begin_json->valueint;
    *frame_end = end_json->valueint;
    if (*frame_begin < 0 || *frame_end < *frame_begin) {
        LISA_UI_LOGE("Invalid phase range: [%d, %d]", *frame_begin, *frame_end);
        return -1;
    }

    return 0;
}

static int parse_phase_interval_overrides(const cJSON *overrides_json, uint16_t frame_count, uint32_t default_interval,
                                          const uint32_t **out_delays)
{
    uint32_t *delays = lisa_mem_alloc(sizeof(uint32_t) * frame_count);
    int count = cJSON_GetArraySize(overrides_json);

    if (delays == NULL) {
        LISA_UI_LOGE("Failed to allocate interval overrides array");
        return -1;
    }

    for (uint16_t i = 0; i < frame_count; i++) {
        delays[i] = default_interval;
    }

    for (int i = 0; i < count; i++) {
        const cJSON *override_json = cJSON_GetArrayItem(overrides_json, i);
        const cJSON *idx_json;
        const cJSON *interval_json;
        int idx;
        int interval;

        if (!cJSON_IsArray(override_json) || cJSON_GetArraySize(override_json) != 2) {
            LISA_UI_LOGE("interval_overrides[%d] must be [frame_idx, interval_ms]", i);
            lisa_mem_free(delays);
            return -1;
        }

        idx_json = cJSON_GetArrayItem(override_json, 0);
        interval_json = cJSON_GetArrayItem(override_json, 1);
        if (!cJSON_IsNumber(idx_json) || !cJSON_IsNumber(interval_json)) {
            LISA_UI_LOGE("interval_overrides[%d] item must be number", i);
            lisa_mem_free(delays);
            return -1;
        }

        idx = idx_json->valueint;
        interval = interval_json->valueint;
        if (idx < 0 || idx >= frame_count || interval < 0) {
            LISA_UI_LOGE("Invalid interval override at index: %d, frame: %d, interval: %d", i, idx, interval);
            lisa_mem_free(delays);
            return -1;
        }

        delays[idx] = (uint32_t)interval;
    }

    *out_delays = delays;
    return 0;
}

static int parse_anim_phase(const cJSON *phase_json, lisa_ui_anim_config_t *cfg, struct emoji_phase_owner *owner, int loop)
{
    const cJSON *path_json;
    const cJSON *overrides_json;
    int frame_begin;
    int frame_end;
    int frame_count;
    int interval_ms = EMOJI_DEFAULT_DELAY;

    memset(cfg, 0, sizeof(*cfg));
    memset(owner, 0, sizeof(*owner));

    if (phase_json == NULL) {
        return 0;
    }

    if (!cJSON_IsObject(phase_json)) {
        LISA_UI_LOGE("Animation phase must be an object");
        return -1;
    }

    path_json = cJSON_GetObjectItemCaseSensitive(phase_json, "path");
    if (!cJSON_IsString(path_json) || path_json->valuestring == NULL) {
        LISA_UI_LOGE("Missing phase path");
        return -1;
    }

    if (parse_phase_range(phase_json, &frame_begin, &frame_end) != 0) {
        return -1;
    }

    frame_count = frame_end - frame_begin + 1;
    owner->path_pattern = emoji_strdup(path_json->valuestring);
    if (owner->path_pattern == NULL) {
        LISA_UI_LOGE("Failed to allocate phase path");
        return -1;
    }
    owner->frame_begin = frame_begin;
    owner->frame_end = frame_end;

    cfg->frames = lisa_mem_calloc(frame_count, sizeof(lv_img_dsc_t));
    if (cfg->frames == NULL) {
        LISA_UI_LOGE("Failed to allocate phase frames");
        return -1;
    }
    cfg->frame_count = (uint16_t)frame_count;

    if (json_get_number(phase_json, "interval_ms", &interval_ms) == 0 && interval_ms > 0) {
        cfg->default_delay = interval_ms;
    } else {
        cfg->default_delay = EMOJI_DEFAULT_DELAY;
    }
    cfg->loop = loop;

    overrides_json = cJSON_GetObjectItemCaseSensitive(phase_json, "interval_overrides");
    if (overrides_json != NULL) {
        if (!cJSON_IsArray(overrides_json)) {
            LISA_UI_LOGE("Animation interval_overrides must be an array");
            return -1;
        }

        if (parse_phase_interval_overrides(overrides_json, cfg->frame_count, cfg->default_delay, &cfg->delays) != 0) {
            return -1;
        }
    }

    return 0;
}

static int find_emoji_index_by_name(const char *name)
{
    if (name == NULL) {
        return -1;
    }

    for (uint32_t i = 0; i < s_emoji_count; i++) {
        if (!s_emoji_configs[i].valid) {
            continue;
        }
        if (strcmp(s_emoji_configs[i].name, name) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static int emoji_has_valid_phase(const struct emoji_config *cfg)
{
    return cfg->anim.enter.frame_count > 0 || cfg->anim.loop.frame_count > 0 || cfg->anim.exit.frame_count > 0;
}

/* Walk an alias chain and report whether it terminates on a valid, non-alias
   target. Returns 0 for a missing/invalid target or a cycle. */
static int emoji_alias_chain_ok(uint32_t start)
{
    uint32_t idx = start;
    uint32_t steps = 0;

    while (1) {
        const struct emoji_config *cfg;

        if (idx >= s_emoji_count) {
            return 0;
        }

        cfg = &s_emoji_configs[idx];
        if (!cfg->valid) {
            return 0;
        }
        if (cfg->alias_idx < 0) {
            return 1;
        }
        if (++steps > s_emoji_count) {
            return 0;
        }

        idx = (uint32_t)cfg->alias_idx;
    }
}

static void resolve_animation_aliases(void)
{
    uint8_t drop[EMOJI_MAX_COUNT] = {0};

    for (uint32_t i = 0; i < s_emoji_count; i++) {
        struct emoji_config *cfg = &s_emoji_configs[i];
        int alias_idx;

        if (!cfg->valid) {
            continue;
        }

        if (cfg->alias_name == NULL) {
            if (!emoji_has_valid_phase(cfg)) {
                LISA_UI_LOGW("Skip animation[%s]: no valid phase", cfg->name);
                emoji_config_invalidate(cfg);
            }
            continue;
        }

        if (emoji_has_valid_phase(cfg)) {
            LISA_UI_LOGW("Skip animation[%s]: alias must not define phase", cfg->name);
            emoji_config_invalidate(cfg);
            continue;
        }

        alias_idx = find_emoji_index_by_name(cfg->alias_name);
        if (alias_idx < 0) {
            LISA_UI_LOGW("Skip animation[%s]: alias target not found: %s", cfg->name, cfg->alias_name);
            emoji_config_invalidate(cfg);
            continue;
        }
        if ((uint32_t)alias_idx == i) {
            LISA_UI_LOGW("Skip animation[%s]: alias self not allowed", cfg->name);
            emoji_config_invalidate(cfg);
            continue;
        }

        cfg->alias_idx = alias_idx;
    }

    /* Snapshot broken/cyclic chains before invalidating so the verdict does
       not depend on iteration order. */
    for (uint32_t i = 0; i < s_emoji_count; i++) {
        const struct emoji_config *cfg = &s_emoji_configs[i];
        if (cfg->valid && cfg->alias_idx >= 0 && !emoji_alias_chain_ok(i)) {
            drop[i] = 1;
        }
    }
    for (uint32_t i = 0; i < s_emoji_count; i++) {
        if (drop[i]) {
            LISA_UI_LOGW("Skip animation[%s]: alias chain broken or cyclic", s_emoji_configs[i].name);
            emoji_config_invalidate(&s_emoji_configs[i]);
        }
    }
}

static const lisa_ui_anim_ext_config_t *emoji_anim_get_by_index(uint32_t idx, uint32_t depth)
{
    const struct emoji_config *cfg;

    if (idx >= s_emoji_count || depth > s_emoji_count) {
        return NULL;
    }

    cfg = &s_emoji_configs[idx];
    if (!cfg->valid) {
        return NULL;
    }
    if (cfg->alias_idx >= 0) {
        return emoji_anim_get_by_index((uint32_t)cfg->alias_idx, depth + 1);
    }

    return &cfg->anim;
}

static void resolve_default_emoji(void)
{
    int fallback_idx;

    s_fallback_anim = NULL;

    fallback_idx = find_emoji_index_by_name(s_fallback_name);
    if (fallback_idx < 0) {
        LISA_UI_LOGE("default_emoji '%s' not loaded, fallback disabled", s_fallback_name);
        return;
    }

    s_fallback_anim = emoji_anim_get_by_index((uint32_t)fallback_idx, 0);
    if (s_fallback_anim == NULL) {
        LISA_UI_LOGE("default_emoji '%s' resolve failed, fallback disabled", s_fallback_name);
    }
}

static int parse_one_animation(const cJSON *item, struct emoji_config *cfg)
{
    const cJSON *name_json;
    const cJSON *alias_json;
    const cJSON *intro_json;
    const cJSON *loop_json;
    const cJSON *outro_json;

    cfg->alias_idx = -1;

    if (!cJSON_IsObject(item)) {
        LISA_UI_LOGE("animation must be an object");
        return -1;
    }

    name_json = cJSON_GetObjectItemCaseSensitive(item, "name");
    if (!cJSON_IsString(name_json) || name_json->valuestring == NULL) {
        LISA_UI_LOGE("animation missing name");
        return -1;
    }

    if (find_emoji_index_by_name(name_json->valuestring) >= 0) {
        LISA_UI_LOGE("Duplicated animation name: %s", name_json->valuestring);
        return -1;
    }

    cfg->name = emoji_strdup(name_json->valuestring);
    if (cfg->name == NULL) {
        LISA_UI_LOGE("Failed to allocate animation name");
        return -1;
    }

    alias_json = cJSON_GetObjectItemCaseSensitive(item, "alias");
    if (alias_json != NULL) {
        if (!cJSON_IsString(alias_json) || alias_json->valuestring == NULL) {
            LISA_UI_LOGE("animation[%s] alias must be string", cfg->name);
            return -1;
        }
        cfg->alias_name = emoji_strdup(alias_json->valuestring);
        if (cfg->alias_name == NULL) {
            LISA_UI_LOGE("Failed to allocate alias for animation: %s", cfg->name);
            return -1;
        }
    }

    intro_json = cJSON_GetObjectItemCaseSensitive(item, "intro");
    loop_json = cJSON_GetObjectItemCaseSensitive(item, "loop");
    outro_json = cJSON_GetObjectItemCaseSensitive(item, "outro");

    if (parse_anim_phase(intro_json, &cfg->anim.enter, &cfg->enter_owner, 0) != 0 ||
        parse_anim_phase(loop_json, &cfg->anim.loop, &cfg->loop_owner, -1) != 0 ||
        parse_anim_phase(outro_json, &cfg->anim.exit, &cfg->exit_owner, 0) != 0) {
        return -1;
    }

    return 0;
}

static int parse_animations(const cJSON *root_json)
{
    const cJSON *default_emoji_json;
    const cJSON *animations_json;
    uint32_t count;

    if (!cJSON_IsObject(root_json)) {
        LISA_UI_LOGE("emoji config root must be an object");
        return -1;
    }

    default_emoji_json = cJSON_GetObjectItemCaseSensitive(root_json, "default_emoji");
    if (!cJSON_IsString(default_emoji_json) || default_emoji_json->valuestring == NULL) {
        LISA_UI_LOGE("default_emoji is required");
        return -1;
    }

    animations_json = cJSON_GetObjectItemCaseSensitive(root_json, "animations");
    if (!cJSON_IsArray(animations_json)) {
        LISA_UI_LOGE("animations must be an array");
        return -1;
    }

    count = cJSON_GetArraySize(animations_json);
    if (count == 0) {
        LISA_UI_LOGE("animations is empty");
        return -1;
    }
    if (count > EMOJI_MAX_COUNT) {
        LISA_UI_LOGE("emoji config too large: %u", count);
        return -1;
    }

    parse_global_offset(root_json);

    s_fallback_name = emoji_strdup(default_emoji_json->valuestring);
    if (s_fallback_name == NULL) {
        LISA_UI_LOGE("Failed to allocate default_emoji");
        return -1;
    }

    s_emoji_configs = lisa_mem_calloc(count, sizeof(struct emoji_config));
    if (s_emoji_configs == NULL) {
        LISA_UI_LOGE("Failed to allocate emoji configs");
        return -1;
    }
    s_emoji_count = count;

    for (uint32_t i = 0; i < count; i++) {
        const cJSON *item = cJSON_GetArrayItem(animations_json, i);
        struct emoji_config *cfg = &s_emoji_configs[i];

        if (parse_one_animation(item, cfg) != 0) {
            LISA_UI_LOGW("Skip invalid animation[%u]: %s", i, cfg->name ? cfg->name : "(unnamed)");
            emoji_config_invalidate(cfg);
            continue;
        }

        cfg->valid = 1;
    }

    return 0;
}

static int load_phase_assets(struct romfs *fs, const lisa_ui_anim_config_t *cfg, const struct emoji_phase_owner *owner)
{
    char path[ROMFS_PATH_MAX];

    if (cfg->frame_count == 0) {
        return 0;
    }

    for (int frame_idx = owner->frame_begin; frame_idx <= owner->frame_end; frame_idx++) {
        uint8_t *data = NULL;
        uint32_t size = 0;
        int path_len = snprintf(path, sizeof(path), owner->path_pattern, frame_idx);

        if (path_len < 0 || path_len >= sizeof(path)) {
            LISA_UI_LOGE("Invalid asset path pattern: %s", owner->path_pattern);
            return -1;
        }

        if (romfs_info_get(fs, path, &data, &size) != 0) {
            LISA_UI_LOGE("Failed to get asset from romfs, path: %s", path);
            return -1;
        }

        lv_img_png_src_init((lv_img_dsc_t *)&cfg->frames[frame_idx - owner->frame_begin], data, size);
    }

    return 0;
}

static void load_emoji_assets(struct romfs *fs)
{
    for (uint32_t i = 0; i < s_emoji_count; i++) {
        struct emoji_config *cfg = &s_emoji_configs[i];

        if (!cfg->valid || cfg->alias_name != NULL) {
            continue;
        }

        if (load_phase_assets(fs, &cfg->anim.enter, &cfg->enter_owner) != 0 ||
            load_phase_assets(fs, &cfg->anim.loop, &cfg->loop_owner) != 0 ||
            load_phase_assets(fs, &cfg->anim.exit, &cfg->exit_owner) != 0) {
            LISA_UI_LOGW("Skip animation[%s]: asset load failed", cfg->name);
            emoji_config_invalidate(cfg);
            continue;
        }

        LISA_UI_LOGI("Loaded emoji animation assets: %s", cfg->name);
    }
}

static int emoji_anim_config_parse(const uint8_t *json_data, uint32_t json_size)
{
    cJSON *root;

    root = cJSON_ParseWithLength((const char *)json_data, json_size);
    if (root == NULL) {
        LISA_UI_LOGE("Failed to parse emoji config JSON");
        return -1;
    }

    if (parse_animations(root) != 0) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON_Delete(root);
    return 0;
}

const lisa_ui_anim_ext_config_t *emoji_anim_get_by_name(const char *name)
{
    int idx;
    const lisa_ui_anim_ext_config_t *anim_cfg;

    if (name == NULL) {
        return s_fallback_anim != NULL ? s_fallback_anim : &s_empty_anim;
    }

    idx = find_emoji_index_by_name(name);
    if (idx >= 0) {
        anim_cfg = emoji_anim_get_by_index((uint32_t)idx, 0);
        if (anim_cfg != NULL) {
            return anim_cfg;
        }
    }

    LISA_UI_LOGE("Invalid emoji name: %s, use fallback instead", name);
    return s_fallback_anim != NULL ? s_fallback_anim : &s_empty_anim;
}

int emoji_anim_get_offset_y(const char *name)
{
    (void)name;
    return s_emoji_offset_y;
}

int emoji_anim_get_offset_x(const char *name)
{
    (void)name;
    return s_emoji_offset_x;
}

int emoji_anim_has_name(const char *name)
{
    int idx;

    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    idx = find_emoji_index_by_name(name);
    if (idx < 0) {
        return 0;
    }

    return emoji_anim_get_by_index((uint32_t)idx, 0) != NULL;
}

int emoji_anim_get_loaded_count(void)
{
    return (int)s_emoji_count;
}

const char *emoji_anim_get_loaded_name(int index)
{
    if (index < 0 || (uint32_t)index >= s_emoji_count) {
        return NULL;
    }
    if (!s_emoji_configs[index].valid) {
        return NULL;
    }

    return s_emoji_configs[index].name;
}

int emoji_anim_is_alias(int index)
{
    if (index < 0 || (uint32_t)index >= s_emoji_count) {
        return 0;
    }
    if (!s_emoji_configs[index].valid) {
        return 0;
    }

    return s_emoji_configs[index].alias_idx >= 0;
}

int lisa_ui_anim_init(uint32_t flash_addr, uint32_t flash_size)
{
    int ret = 0;
    struct romfs *fs = NULL;
    uint8_t *config_data = NULL;
    uint32_t config_size = 0;
    uint32_t valid_count = 0;

    emoji_anim_cleanup();

    if (romfs_init(&fs, (const void *)flash_addr, flash_size) != 0) {
        LISA_UI_LOGE("Failed to init romfs");
        return -1;
    }

    if (romfs_info_get(fs, EMOJI_CONFIG_PATH, &config_data, &config_size) != 0) {
        LISA_UI_LOGE("Failed to load emoji config from romfs: %s", EMOJI_CONFIG_PATH);
        ret = -1;
        goto out;
    }

    if (emoji_anim_config_parse(config_data, config_size) != 0) {
        LISA_UI_LOGE("Failed to parse emoji config");
        ret = -1;
        goto out;
    }

    /* Order matters: load assets first so alias resolution can drop aliases
       whose target failed to load; bad entries are skipped, not fatal. */
    load_emoji_assets(fs);
    resolve_animation_aliases();
    resolve_default_emoji();

    for (uint32_t i = 0; i < s_emoji_count; i++) {
        if (s_emoji_configs[i].valid) {
            valid_count++;
        }
    }

    if (valid_count == 0) {
        LISA_UI_LOGE("No valid emoji animation loaded");
        ret = -1;
        goto out;
    }

    LISA_UI_LOGI("Emoji animation loaded, valid: %u / %u", valid_count, s_emoji_count);

out:
    if (ret != 0) {
        emoji_anim_cleanup();
    }
    romfs_deinit(&fs);
    return ret;
}
