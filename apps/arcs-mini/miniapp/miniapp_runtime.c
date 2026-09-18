#include "miniapp.h"

#include <stdio.h>
#include <math.h>
#include <time.h>
#include "cJSON.h"
#include "listen_system.h"
#include "miniapp_storage.h"
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include <soc/chip.h>
#include "lauxlib.h"
#include "alarm_handler.h"
#include "alarm_ring.h"
#include "app_player.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "lua.h"
#include "lualib.h"
#include "lsc.h"
#include "service_led.h"
#include "service_power_policy.h"
#include "voice_cloud.h"
#include "voice_msg.h"
#include "voice_intent_mgr.h"
#include "voice_player/voice_player_music.h"
#include "voice_player/voice_player_tts.h"
#include "voice_player_comm.h"

#define TAG "miniapp"
#define MINIAPP_HOOK_GRANULARITY 1000u

typedef enum {
    MINIAPP_EVENT_INSTALL,
    MINIAPP_EVENT_CLICK,
    MINIAPP_EVENT_EXIT,
} miniapp_event_type_t;

typedef struct {
    miniapp_event_type_t type;
    char button_id[16];
    void *request;
    uint32_t generation;
} miniapp_event_t;

typedef struct {
    uint16_t frequency_hz;
    uint16_t duration_ms;
    uint32_t generation;
} miniapp_buzzer_command_t;

typedef struct {
    size_t used;
    size_t limit;
    uint32_t instruction_left;
    TickType_t deadline;
#if configGENERATE_RUN_TIME_STATS && defined(SOC_TIMER_FREQ)
    uint32_t cpu_started;
    uint32_t cpu_limit;
#endif
    bool validating;
    bool faulted, retiring, storage_read;
    char id[MINIAPP_ID_MAX + 1];
    cJSON *storage_json;
    char storage_data[MINIAPP_STORAGE_MAX_BYTES + 1];
    size_t storage_size;
    uint32_t storage_ttl;
    unsigned storage_pending; /* 0: none, 1: save, 2: clear; startup transaction */
    uint32_t present_count;
    miniapp_scene_t scene;
    miniapp_scene_t presented_scene;
    int pending_led;
    uint32_t led_on_ms, led_off_ms;
    unsigned buzz_count;
    miniapp_buzzer_command_t buzz[4];
} miniapp_vm_t;

typedef struct {
    const miniapp_package_t *package;
    const char *source;
    SemaphoreHandle_t done;
    int result;
    char error[128];
} miniapp_request_t;

static QueueHandle_t s_event_queue;
static TaskHandle_t s_task;
static QueueHandle_t s_buzzer_queue;
static TaskHandle_t s_buzzer_task;
static miniapp_status_t s_status;
static miniapp_package_t s_current;
static volatile bool s_active;
static volatile bool s_install_busy;
static volatile uint32_t s_buzzer_generation;
static uint32_t s_install_generation;

static void miniapp_set_active(bool active)
{
    bool changed = s_active != active;
    s_active = active;
    s_status.active = active;
    service_power_policy_set_miniapp_active(active);
    if (changed) {
        /* Mode is captured in the start frame. Stop uploading immediately,
         * then let the voice task clear pending TTS continuation and finish
         * the interaction without cancelling its reply. A fresh wakeup uses
         * the new mode; replacing an active miniapp does not change it. */
        if (voice_cloud_is_session_active() || voice_cloud_is_uploading_audio()) {
            if (voice_cloud_chat_stop_local() != 0) {
                LISA_LOGW(TAG, "failed to stop voice input on miniapp mode change");
            }
        }
        if (voice_msg_pub(VOICE_MSG_CLOUD_MCP_CHAT_EXIT, NULL, 0) != 0) {
            LISA_LOGW(TAG, "failed to finish voice interaction on miniapp mode change");
        }
    }
}

static void miniapp_set_error(const char *message)
{
    snprintf(s_status.last_error, sizeof(s_status.last_error), "%s",
             message ? message : "unknown error");
    LISA_LOGE(TAG, "%s", s_status.last_error);
}

static void *miniapp_lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    miniapp_vm_t *vm = ud;
    if (nsize == 0) {
        if (ptr) {
            lisa_mem_free(ptr);
            vm->used = osize <= vm->used ? vm->used - osize : 0;
        }
        return NULL;
    }

    size_t old_size = ptr ? osize : 0;
    if (nsize > UINT32_MAX || vm->used > vm->limit ||
        (nsize > old_size && nsize - old_size > vm->limit - vm->used)) {
        return NULL;
    }

    void *next = ptr ? lisa_mem_realloc(ptr, (uint32_t)nsize)
                     : lisa_mem_alloc((uint32_t)nsize);
    if (next) {
        vm->used = vm->used - old_size + nsize;
    }
    return next;
}

static miniapp_vm_t *miniapp_vm_get(lua_State *L)
{
    return *(miniapp_vm_t **)lua_getextraspace(L);
}

static lua_Integer miniapp_integer(lua_State *L, int index, lua_Integer min, lua_Integer max)
{
    lua_Integer value = luaL_checkinteger(L, index);
    if (value < min || value > max) {
        luaL_error(L, "argument %d is out of range", index);
    }
    return value;
}

static int miniapp_lua_screen_begin(lua_State *L)
{
    miniapp_vm_t *vm = miniapp_vm_get(L);
    vm->scene.background = (uint32_t)luaL_checkinteger(L, 1) & 0xffffffu;
    vm->scene.rect_count = 0;
    vm->scene.text_count = 0;
    return 0;
}

static int miniapp_lua_screen_rect(lua_State *L)
{
    miniapp_vm_t *vm = miniapp_vm_get(L);
    if (vm->scene.rect_count >= MINIAPP_MAX_RECTS) {
        return luaL_error(L, "screen.rect limit is %d", MINIAPP_MAX_RECTS);
    }
    miniapp_rect_t *rect = &vm->scene.rects[vm->scene.rect_count++];
    rect->x = (int16_t)miniapp_integer(L, 1, 0, MINIAPP_SCREEN_WIDTH - 1);
    rect->y = (int16_t)miniapp_integer(L, 2, 0, MINIAPP_SCREEN_HEIGHT - 1);
    rect->w = (int16_t)miniapp_integer(L, 3, 0, MINIAPP_SCREEN_WIDTH);
    rect->h = (int16_t)miniapp_integer(L, 4, 0, MINIAPP_SCREEN_HEIGHT);
    rect->color = (uint32_t)luaL_checkinteger(L, 5) & 0xffffffu;
    if (rect->w <= 0 || rect->h <= 0 || rect->x < 0 || rect->y < 0 ||
        rect->x + rect->w > MINIAPP_SCREEN_WIDTH ||
        rect->y + rect->h > MINIAPP_SCREEN_HEIGHT) {
        return luaL_error(L, "invalid rectangle");
    }
    return 0;
}

static int miniapp_lua_screen_text(lua_State *L)
{
    miniapp_vm_t *vm = miniapp_vm_get(L);
    size_t len = 0;
    const char *text = luaL_checklstring(L, 1, &len);
    if (vm->scene.text_count >= MINIAPP_MAX_TEXTS) {
        return luaL_error(L, "screen.text limit is %d", MINIAPP_MAX_TEXTS);
    }
    if (len > MINIAPP_TEXT_MAX) {
        return luaL_error(L, "text is too long");
    }
    miniapp_text_t *item = &vm->scene.texts[vm->scene.text_count++];
    item->x = (int16_t)miniapp_integer(L, 2, 0, MINIAPP_SCREEN_WIDTH - 1);
    item->y = (int16_t)miniapp_integer(L, 3, 0, MINIAPP_SCREEN_HEIGHT - MINIAPP_TEXT_FONT_PX);
    item->color = (uint32_t)luaL_checkinteger(L, 4) & 0xffffffu;
    if (item->x < 0 || item->x >= MINIAPP_SCREEN_WIDTH || item->y < 0 ||
        item->y + MINIAPP_TEXT_FONT_PX > MINIAPP_SCREEN_HEIGHT) {
        return luaL_error(L, "invalid text position");
    }
    memcpy(item->text, text, len);
    item->text[len] = '\0';
    return 0;
}

static int miniapp_lua_screen_present(lua_State *L)
{
    miniapp_vm_t *vm = miniapp_vm_get(L);
    vm->present_count++;
    vm->presented_scene = vm->scene;
    if (!vm->validating && miniapp_ui_present(&vm->scene) != 0) {
        s_status.dropped_frames++;
    }
    return 0;
}

static int miniapp_lua_led_on(lua_State *L)
{
    if (strcmp(luaL_checkstring(L, 1), "status") != 0) {
        return luaL_error(L, "unknown LED id");
    }
    miniapp_vm_t *vm = miniapp_vm_get(L);
    if (vm->validating) vm->pending_led = 1;
    else service_led_on();
    return 0;
}

static int miniapp_lua_led_off(lua_State *L)
{
    if (strcmp(luaL_checkstring(L, 1), "status") != 0) {
        return luaL_error(L, "unknown LED id");
    }
    miniapp_vm_t *vm = miniapp_vm_get(L);
    if (vm->validating) vm->pending_led = 0;
    else service_led_off();
    return 0;
}

static int miniapp_lua_led_blink(lua_State *L)
{
    if (strcmp(luaL_checkstring(L, 1), "status") != 0) {
        return luaL_error(L, "unknown LED id");
    }
    uint32_t on_ms = (uint32_t)miniapp_integer(L, 2, 10, 5000);
    uint32_t off_ms = (uint32_t)miniapp_integer(L, 3, 10, 5000);
    if (on_ms < 10 || on_ms > 5000 || off_ms < 10 || off_ms > 5000) {
        return luaL_error(L, "blink interval must be 10..5000 ms");
    }
    miniapp_vm_t *vm = miniapp_vm_get(L);
    if (vm->validating) {
        vm->pending_led = 2; vm->led_on_ms = on_ms; vm->led_off_ms = off_ms;
    } else service_led_blink(on_ms, off_ms);
    return 0;
}

static void miniapp_buzzer_enqueue(const miniapp_buzzer_command_t *command)
{
    /* Sound effects are best effort. Audio contention must not fault the Lua
     * application or replay stale effects after a spoken reply or alarm. */
    if (!s_buzzer_queue || voice_player_tts_is_active() || alarm_ring_is_active()) return;
    (void)xQueueSend(s_buzzer_queue, command, 0);
}

static int miniapp_lua_buzzer_play(lua_State *L)
{
    uint32_t frequency_hz = (uint32_t)miniapp_integer(L, 1, MINIAPP_BUZZER_MIN_HZ, MINIAPP_BUZZER_MAX_HZ);
    uint32_t duration_ms = (uint32_t)miniapp_integer(L, 2, MINIAPP_BUZZER_MIN_MS, MINIAPP_BUZZER_MAX_MS);
    if (frequency_hz < MINIAPP_BUZZER_MIN_HZ || frequency_hz > MINIAPP_BUZZER_MAX_HZ) {
        return luaL_error(L, "buzzer frequency must be %u..%u Hz",
                          MINIAPP_BUZZER_MIN_HZ, MINIAPP_BUZZER_MAX_HZ);
    }
    if (duration_ms < MINIAPP_BUZZER_MIN_MS || duration_ms > MINIAPP_BUZZER_MAX_MS) {
        return luaL_error(L, "buzzer duration must be %u..%u ms",
                          MINIAPP_BUZZER_MIN_MS, MINIAPP_BUZZER_MAX_MS);
    }
    miniapp_vm_t *vm = miniapp_vm_get(L);
    miniapp_buzzer_command_t command = {
        .frequency_hz = (uint16_t)frequency_hz,
        .duration_ms = (uint16_t)duration_ms,
        .generation = s_buzzer_generation,
    };
    if (vm->validating) {
        if (vm->buzz_count == 4) return 0;
        vm->buzz[vm->buzz_count++] = command;
    } else {
        miniapp_buzzer_enqueue(&command);
    }
    return 0;
}

#if MINIAPP_HAS_BUZZER
static void miniapp_wav_u32(uint8_t *out, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) out[i] = (uint8_t)(value >> (8u * i));
}

static bool miniapp_buzzer_player_active(void)
{
    app_player_state_t state = app_player_get_state(miniapp_player);
    return state == APP_PLAYER_STATE_PREPARING || state == APP_PLAYER_STATE_PREPARED ||
           state == APP_PLAYER_STATE_PLAYING || state == APP_PLAYER_STATE_PAUSED;
}

static void miniapp_buzzer_task(void *argument)
{
    (void)argument;
    miniapp_buzzer_command_t command;
    /* The decoder retains the memory URL. Keep one bounded buffer alive and
     * synchronously stop its dedicated player before overwriting any bytes. */
    uint8_t *wav = NULL;
    for (;;) {
        if (xQueueReceive(s_buzzer_queue, &command, portMAX_DELAY) != pdTRUE) continue;
        if (voice_player_tts_is_active() || alarm_ring_is_active()) continue;
        if (!s_active || command.generation != s_buzzer_generation || !miniapp_player) continue;
        if (miniapp_buzzer_player_active() && app_player_stop(miniapp_player) != APP_PLAYER_OK) {
            LISA_LOGW(TAG, "failed to stop previous buzzer");
            continue;
        }
        if (!wav) wav = lisa_mem_alloc(44u + MINIAPP_BUZZER_MAX_MS * 32u);
        if (!wav) {
            LISA_LOGW(TAG, "failed to allocate buzzer buffer");
            continue;
        }
        uint32_t count = (uint32_t)command.duration_ms * 16u;
        uint32_t bytes = count * 2u;
        memset(wav, 0, 44);
        memcpy(wav, "RIFF", 4);
        miniapp_wav_u32(wav + 4, 36u + bytes);
        memcpy(wav + 8, "WAVEfmt ", 8);
        miniapp_wav_u32(wav + 16, 16);
        wav[20] = 1; /* PCM */
        wav[22] = 1; /* mono */
        miniapp_wav_u32(wav + 24, 16000);
        miniapp_wav_u32(wav + 28, 32000);
        wav[32] = 2;
        wav[34] = 16;
        memcpy(wav + 36, "data", 4);
        miniapp_wav_u32(wav + 40, bytes);
        uint32_t phase = 0;
        for (uint32_t i = 0; i < count; ++i) {
            phase = (phase + command.frequency_hz) % 16000u;
            int16_t sample = phase < 8000u ? 12000 : -12000;
            wav[44u + i * 2u] = (uint8_t)sample;
            wav[45u + i * 2u] = (uint8_t)((uint16_t)sample >> 8);
        }
        char url[64];
        snprintf(url, sizeof(url), "mem://addr=%usize=%u", (unsigned)(uintptr_t)wav, bytes + 44u);
        if (app_player_play(miniapp_player, url) != APP_PLAYER_OK) {
            LISA_LOGW(TAG, "failed to play buzzer");
            continue;
        }
        TickType_t started = xTaskGetTickCount();
        while (s_active && command.generation == s_buzzer_generation &&
               !alarm_ring_is_active() && !voice_player_tts_is_active() &&
               xTaskGetTickCount() - started < pdMS_TO_TICKS(command.duration_ms + 500u)) {
            app_player_state_t state = app_player_get_state(miniapp_player);
            if (state != APP_PLAYER_STATE_PREPARING && state != APP_PLAYER_STATE_PREPARED &&
                state != APP_PLAYER_STATE_PLAYING) break;
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        if (miniapp_buzzer_player_active()) (void)app_player_stop(miniapp_player);
    }
}
#endif

static void miniapp_storage_fault(miniapp_vm_t *vm)
{
    vm->faulted = true;
    vm->storage_pending = 0;
    cJSON_Delete(vm->storage_json);
    vm->storage_json = NULL;
    if (vm->storage_read) {
        const char *error = miniapp_storage_clear(vm->id);
        if (error) LISA_LOGW(TAG, "failed to clear app data: %s", error);
    }
}

static int miniapp_lua_clock_now(lua_State *L)
{
    struct timeval tv;
    if (!ls_sys_time_is_valid() || ls_sys_get_time(&tv) != 0) lua_pushnil(L);
    else lua_pushinteger(L, tv.tv_sec);
    return 1;
}

static int miniapp_lua_clock_localtime(lua_State *L)
{
    struct tm calendar;
    if (ls_sys_get_localtime(&calendar) != 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 8);
#define CLOCK_FIELD(name, value) do { lua_pushinteger(L, value); lua_setfield(L, -2, name); } while (0)
    CLOCK_FIELD("year", calendar.tm_year + 1900);
    CLOCK_FIELD("month", calendar.tm_mon + 1);
    CLOCK_FIELD("day", calendar.tm_mday);
    CLOCK_FIELD("hour", calendar.tm_hour);
    CLOCK_FIELD("min", calendar.tm_min);
    CLOCK_FIELD("sec", calendar.tm_sec);
    CLOCK_FIELD("wday", calendar.tm_wday);
    CLOCK_FIELD("utc_offset", ls_sys_timezone_offset_seconds());
#undef CLOCK_FIELD
    return 1;
}

static int miniapp_storage_result(lua_State *L, const char *error)
{
    lua_pushboolean(L, error == NULL);
    if (error) { lua_pushstring(L, error); return 2; }
    return 1;
}

static bool miniapp_storage_object_valid(cJSON *object)
{
    if (!cJSON_IsObject(object)) return false;
    unsigned count = 0;
    for (cJSON *item = object->child; item; item = item->next) {
        if (++count > 32 || !item->string || !*item->string || strlen(item->string) > 32) return false;
        if (cJSON_IsNumber(item)) { if (!isfinite(item->valuedouble)) return false; }
        else if (cJSON_IsString(item)) { if (strlen(item->valuestring) > 256) return false; }
        else if (!cJSON_IsBool(item)) return false;
    }
    return true;
}

static int miniapp_lua_storage_load(lua_State *L)
{
    miniapp_vm_t *vm = miniapp_vm_get(L);
    if (vm->faulted || vm->storage_pending == 2 || !ls_sys_time_is_valid()) { lua_pushnil(L); return 1; }
    size_t size = vm->storage_size;
    if (vm->storage_pending != 1 && !miniapp_storage_load(vm->id, vm->storage_data, &size)) {
        lua_pushnil(L);
        return 1;
    }
    vm->storage_json = cJSON_ParseWithLength(vm->storage_data, size + 1);
    if (!miniapp_storage_object_valid(vm->storage_json)) {
        cJSON_Delete(vm->storage_json);
        vm->storage_json = NULL;
        (void)miniapp_storage_clear(vm->id);
        lua_pushnil(L);
        return 1;
    }
    vm->storage_read = true;
    lua_newtable(L);
    for (cJSON *item = vm->storage_json->child; item; item = item->next) {
        if (cJSON_IsNumber(item)) lua_pushnumber(L, item->valuedouble);
        else if (cJSON_IsBool(item)) lua_pushboolean(L, cJSON_IsTrue(item));
        else lua_pushstring(L, item->valuestring);
        lua_setfield(L, -2, item->string);
    }
    cJSON_Delete(vm->storage_json);
    vm->storage_json = NULL;
    return 1;
}

static int miniapp_lua_storage_save(lua_State *L)
{
    miniapp_vm_t *vm = miniapp_vm_get(L);
    if (vm->faulted || vm->retiring) return miniapp_storage_result(L, "unavailable");
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_Integer ttl = luaL_optinteger(L, 2, MINIAPP_STORAGE_DEFAULT_TTL);
    if (ttl < 1 || ttl > MINIAPP_STORAGE_MAX_TTL) return miniapp_storage_result(L, "invalid_ttl");
    if (!ls_sys_time_is_valid()) return miniapp_storage_result(L, "clock_unavailable");
    vm->storage_json = cJSON_CreateObject();
    if (!vm->storage_json) return miniapp_storage_result(L, "no_memory");
    const char *error = NULL;
    unsigned count = 0;
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        size_t key_len = 0, value_len = 0;
        const char *key = lua_type(L, -2) == LUA_TSTRING ? lua_tolstring(L, -2, &key_len) : NULL;
        cJSON *item = NULL;
        if (++count > 32 || !key || !key_len || key_len > 32 || memchr(key, 0, key_len)) {
            error = "invalid_data";
        } else if (lua_type(L, -1) == LUA_TNUMBER && isfinite(lua_tonumber(L, -1))) {
            item = cJSON_CreateNumber(lua_tonumber(L, -1));
        } else if (lua_type(L, -1) == LUA_TBOOLEAN) {
            item = cJSON_CreateBool(lua_toboolean(L, -1));
        } else if (lua_type(L, -1) == LUA_TSTRING) {
            const char *value = lua_tolstring(L, -1, &value_len);
            if (value_len > 256 || memchr(value, 0, value_len)) error = "invalid_data";
            else item = cJSON_CreateString(value);
        } else error = "invalid_data";
        if (!error && (!item || !cJSON_AddItemToObject(vm->storage_json, key, item))) {
            cJSON_Delete(item);
            error = "no_memory";
        }
        lua_pop(L, 1);
        if (error) { lua_pop(L, 1); break; }
    }
    /* Never replace pending data on serialization failure. */
    char *encoded = !error ? cJSON_PrintUnformatted(vm->storage_json) : NULL;
    if (!error && !encoded) error = "no_memory";
    if (!error && strlen(encoded) > MINIAPP_STORAGE_MAX_BYTES) error = "too_large";
    if (!error) {
        if (vm->validating) {
            vm->storage_size = strlen(encoded);
            memcpy(vm->storage_data, encoded, vm->storage_size + 1);
            vm->storage_ttl = ttl;
            vm->storage_pending = 1;
        } else error = miniapp_storage_save(vm->id, encoded, strlen(encoded), ttl);
    }
    cJSON_free(encoded);
    cJSON_Delete(vm->storage_json);
    vm->storage_json = NULL;
    return miniapp_storage_result(L, error);
}

static int miniapp_lua_storage_clear(lua_State *L)
{
    miniapp_vm_t *vm = miniapp_vm_get(L);
    if (vm->faulted || vm->retiring) return miniapp_storage_result(L, "unavailable");
    if (vm->validating) { vm->storage_pending = 2; return miniapp_storage_result(L, NULL); }
    return miniapp_storage_result(L, miniapp_storage_clear(vm->id));
}

static void miniapp_register_function(lua_State *L, const char *table,
                                      const char *name, lua_CFunction function)
{
    lua_getglobal(L, table);
    lua_pushcfunction(L, function);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
}

static void miniapp_open_safe_libraries(lua_State *L)
{
    static const struct {
        const char *name;
        lua_CFunction open;
    } libs[] = {
        {LUA_GNAME, luaopen_base},
        {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},
        {LUA_UTF8LIBNAME, luaopen_utf8},
    };

    for (size_t i = 0; i < sizeof(libs) / sizeof(libs[0]); ++i) {
        luaL_requiref(L, libs[i].name, libs[i].open, 1);
        lua_pop(L, 1);
    }

    const char *forbidden[] = {
        "collectgarbage", "dofile", "load", "loadfile", "print", "warn",
        "pcall", "xpcall", "setmetatable"
    };
    for (size_t i = 0; i < sizeof(forbidden) / sizeof(forbidden[0]); ++i) {
        lua_pushnil(L);
        lua_setglobal(L, forbidden[i]);
    }

#if MINIAPP_HAS_SCREEN
    lua_newtable(L);
    lua_setglobal(L, "screen");
    miniapp_register_function(L, "screen", "begin", miniapp_lua_screen_begin);
    miniapp_register_function(L, "screen", "rect", miniapp_lua_screen_rect);
    miniapp_register_function(L, "screen", "text", miniapp_lua_screen_text);
    miniapp_register_function(L, "screen", "present", miniapp_lua_screen_present);
#endif

#if MINIAPP_HAS_LEDS
    lua_newtable(L);
    lua_setglobal(L, "led");
    miniapp_register_function(L, "led", "on", miniapp_lua_led_on);
    miniapp_register_function(L, "led", "off", miniapp_lua_led_off);
    miniapp_register_function(L, "led", "blink", miniapp_lua_led_blink);
#endif

#if MINIAPP_HAS_BUZZER
    lua_newtable(L);
    lua_setglobal(L, "buzzer");
    miniapp_register_function(L, "buzzer", "play", miniapp_lua_buzzer_play);
#endif

    lua_newtable(L);
    lua_setglobal(L, "clock");
    miniapp_register_function(L, "clock", "now", miniapp_lua_clock_now);
    miniapp_register_function(L, "clock", "localtime", miniapp_lua_clock_localtime);
    lua_newtable(L);
    lua_setglobal(L, "storage");
    miniapp_register_function(L, "storage", "load", miniapp_lua_storage_load);
    miniapp_register_function(L, "storage", "save", miniapp_lua_storage_save);
    miniapp_register_function(L, "storage", "clear", miniapp_lua_storage_clear);

    lua_newtable(L);
#if MINIAPP_HAS_SCREEN
    lua_pushinteger(L, MINIAPP_SCREEN_WIDTH);
    lua_setfield(L, -2, "width");
    lua_pushinteger(L, MINIAPP_SCREEN_HEIGHT);
    lua_setfield(L, -2, "height");
#endif
    lua_pushinteger(L, MINIAPP_API_VERSION);
    lua_setfield(L, -2, "api_version");
    lua_setglobal(L, "app");
}

static void miniapp_hook(lua_State *L, lua_Debug *ar)
{
    (void)ar;
    miniapp_vm_t *vm = miniapp_vm_get(L);
    TickType_t now = xTaskGetTickCount();
    if (vm->instruction_left <= MINIAPP_HOOK_GRANULARITY) {
        luaL_error(L, "instruction budget exceeded");
        return;
    }
    if ((int32_t)(now - vm->deadline) >= 0) {
#if configGENERATE_RUN_TIME_STATS && defined(SOC_TIMER_FREQ)
        /* A radio calibration can preempt this task for hundreds of ms.
         * FreeRTOS accounts completed running slices, excluding other tasks.
         * The current slice may be incomplete; the instruction limit remains
         * the hard bound even when no context switch updates this counter. */
        uint32_t cpu = (uint32_t)ulTaskGetRunTimeCounter(NULL) - vm->cpu_started;
        if (cpu < vm->cpu_limit) {
            vm->instruction_left -= MINIAPP_HOOK_GRANULARITY;
            return;
        }
#endif
        luaL_error(L, "wall-time budget exceeded");
        return;
    }
    vm->instruction_left -= MINIAPP_HOOK_GRANULARITY;
}

static void miniapp_set_deadline(miniapp_vm_t *vm, uint32_t milliseconds)
{
    vm->deadline = xTaskGetTickCount() + pdMS_TO_TICKS(milliseconds);
#if configGENERATE_RUN_TIME_STATS && defined(SOC_TIMER_FREQ)
    vm->cpu_started = (uint32_t)ulTaskGetRunTimeCounter(NULL);
    vm->cpu_limit = (uint32_t)((uint64_t)milliseconds * SOC_TIMER_FREQ /
                             1000u / (1000u / configTICK_RATE_HZ));
#endif
}

static int miniapp_open_libraries_protected(lua_State *L)
{
    miniapp_open_safe_libraries(L);
    return 0;
}

static lua_State *miniapp_vm_create(miniapp_vm_t *vm, bool validating)
{
    memset(vm, 0, sizeof(*vm));
    vm->limit = MINIAPP_LUA_HEAP_LIMIT;
    vm->validating = validating;
    vm->pending_led = -1;
    vm->scene.background = 0;
    lua_State *L = lua_newstate(miniapp_lua_alloc, vm);
    if (!L) {
        return NULL;
    }
    *(miniapp_vm_t **)lua_getextraspace(L) = vm;
    lua_pushcfunction(L, miniapp_open_libraries_protected);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        lua_close(L);
        return NULL;
    }
    lua_sethook(L, miniapp_hook, LUA_MASKCOUNT, MINIAPP_HOOK_GRANULARITY);
    return L;
}

static int miniapp_run_chunk(lua_State *L, miniapp_vm_t *vm,
                             const char *source, size_t source_len,
                             char *error, size_t error_size)
{
    vm->instruction_left = MINIAPP_CHUNK_INSTRUCTION_LIMIT;
    miniapp_set_deadline(vm, MINIAPP_CHUNK_DEADLINE_MS);
    int rc = luaL_loadbufferx(L, source, source_len, "@cloud-miniapp", "t");
    if (rc == LUA_OK) {
        rc = lua_pcall(L, 0, 0, 0);
    }
    if (rc != LUA_OK) {
        miniapp_storage_fault(vm);
        snprintf(error, error_size, "%s", lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : "Lua error");
        lua_pop(L, 1);
        return -1;
    }
    return 0;
}

typedef struct {
    const char *name;
    const char *button;
    lua_Integer argument;
    int nargs;
} miniapp_callback_t;

static int miniapp_callback_protected(lua_State *L)
{
    miniapp_callback_t *call = lua_touserdata(L, 1);
    lua_getglobal(L, call->name);
    if (lua_isnil(L, -1) && strcmp(call->name, "on_tick") != 0) {
        return 0;
    }
    if (!lua_isfunction(L, -1)) {
        return luaL_error(L, "%s must be a function", call->name);
    }
    if (call->button) lua_pushstring(L, call->button);
    else if (call->nargs) lua_pushinteger(L, call->argument);
    lua_call(L, call->nargs, 0);
    return 0;
}

static int miniapp_invoke(lua_State *L, miniapp_vm_t *vm, miniapp_callback_t *call)
{
    vm->instruction_left = MINIAPP_CALLBACK_INSTRUCTION_LIMIT;
    TickType_t started = xTaskGetTickCount();
    miniapp_set_deadline(vm, MINIAPP_CALLBACK_DEADLINE_MS);
    /* Even callback lookup and string arguments may allocate. Keep them under
     * pcall so an exhausted heap cannot invoke Lua's process-wide panic. */
    lua_pushcfunction(L, miniapp_callback_protected);
    lua_pushlightuserdata(L, call);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        miniapp_storage_fault(vm);
        miniapp_set_error(lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : "Lua error");
        lua_pop(L, 1);
        return -1;
    }
    uint32_t elapsed = (uint32_t)((xTaskGetTickCount() - started) * portTICK_PERIOD_MS);
    s_status.last_frame_ms = elapsed;
    if (elapsed > s_status.max_frame_ms) s_status.max_frame_ms = elapsed;
    return 0;
}

static int miniapp_call(lua_State *L, miniapp_vm_t *vm, const char *name,
                        int nargs, lua_Integer argument)
{
    miniapp_callback_t call = {.name = name, .nargs = nargs, .argument = argument};
    return miniapp_invoke(L, vm, &call);
}

static int miniapp_call_button(lua_State *L, miniapp_vm_t *vm, const char *button_id)
{
    miniapp_callback_t call = {.name = "on_button_click", .nargs = 1, .button = button_id};
    return miniapp_invoke(L, vm, &call);
}

static void miniapp_runtime_stop(lua_State **state, miniapp_vm_t *vm, bool navigate_home)
{
    if (*state) {
        (void)miniapp_call(*state, vm, "on_exit", 0, 0);
        lua_close(*state);
        *state = NULL;
    }
    ++s_buzzer_generation;
    service_led_off();
    miniapp_set_active(false);
    taskENTER_CRITICAL();
    memset(&s_current, 0, sizeof(s_current));
    taskEXIT_CRITICAL();
    s_status.installed = false;
    if (s_buzzer_queue) {
        xQueueReset(s_buzzer_queue);
    }
    if (navigate_home) {
        (void)miniapp_ui_close();
    }
    LISA_LOGI(TAG, "runtime stopped");
}

/* Only the Lua task owns VM lifetime. The previous VM stays intact until the
 * candidate has loaded, executed startup callbacks and reached the UI thread. */
static int miniapp_replace(lua_State **current, miniapp_vm_t **current_vm,
                           miniapp_request_t *request)
{
    miniapp_vm_t *candidate = lisa_mem_calloc(1, sizeof(*candidate));
    lua_State *next = candidate ? miniapp_vm_create(candidate, true) : NULL;
#if MINIAPP_HAS_SCREEN
    bool had_current = *current != NULL;
#endif
    if (!next) {
        snprintf(request->error, sizeof(request->error), "not enough memory for Lua VM");
        lisa_mem_free(candidate);
        return -1;
    }
    snprintf(candidate->id, sizeof(candidate->id), "%s", request->package->id);
    if (miniapp_run_chunk(next, candidate, request->source, request->package->size,
                         request->error, sizeof(request->error)) != 0) {
        goto failed;
    }
    if (miniapp_call(next, candidate, "on_start", 0, 0) != 0 ||
        miniapp_call(next, candidate, "on_tick", 1, MINIAPP_TICK_MS) != 0) {
        snprintf(request->error, sizeof(request->error), "%s", s_status.last_error);
        goto failed;
    }
#if MINIAPP_HAS_SCREEN
    if (!candidate->present_count) {
        snprintf(request->error, sizeof(request->error), "startup must present a frame");
        goto failed;
    }
    if (miniapp_ui_present(&candidate->presented_scene) != 0 || miniapp_ui_open() != 0 ||
        miniapp_ui_wait_frame(2000) != 0) {
        snprintf(request->error, sizeof(request->error), "failed to show miniapp frame");
        if (had_current) {
            (void)miniapp_ui_present(&(*current_vm)->presented_scene);
        } else {
            (void)miniapp_ui_close();
        }
        goto failed;
    }
#endif
    /* The candidate is now usable. Do not run old callbacks with device side
     * effects: they must not clear the new scene or enqueue old sounds. */
    if (*current) {
        (*current_vm)->validating = true;
        (*current_vm)->retiring = true;
        (void)miniapp_call(*current, *current_vm, "on_exit", 0, 0);
        lua_close(*current);
        lisa_mem_free(*current_vm);
    }
    *current = next;
    *current_vm = candidate;
    candidate->validating = false;
    if (candidate->storage_pending) {
        const char *error = candidate->storage_pending == 2 ? miniapp_storage_clear(candidate->id) :
            miniapp_storage_save(candidate->id, candidate->storage_data, candidate->storage_size, candidate->storage_ttl);
        if (error) LISA_LOGW(TAG, "startup app data not saved: %s", error);
        candidate->storage_pending = 0;
    }
    ++s_buzzer_generation;
    if (s_buzzer_queue) {
        xQueueReset(s_buzzer_queue);
    }
    service_led_off();
    taskENTER_CRITICAL();
    s_current = *request->package;
    taskEXIT_CRITICAL();
    memset(&s_status, 0, sizeof(s_status));
    s_status.installed = true;
    s_status.source_size = request->package->size;
    miniapp_set_active(true);
    if (candidate->pending_led == 1) service_led_on();
    if (candidate->pending_led == 2) service_led_blink(candidate->led_on_ms, candidate->led_off_ms);
    for (unsigned i = 0; i < candidate->buzz_count; ++i) {
        candidate->buzz[i].generation = s_buzzer_generation;
        miniapp_buzzer_enqueue(&candidate->buzz[i]);
    }
    if (voice_intent_contains(INTENT_MUSIC)) {
        voice_player_music_publish_info(NULL, NULL);
        (void)voice_intent_pop(INTENT_MUSIC);
    } else if (music_player && app_player_get_state(music_player) != APP_PLAYER_STATE_IDLE) {
        (void)app_player_stop(music_player);
    }
    return 0;
failed:
    lua_close(next);
    lisa_mem_free(candidate);
    return -1;
}

static void miniapp_task(void *argument)
{
    (void)argument;
    lua_State *L = NULL;
    miniapp_vm_t *vm = NULL;
    TickType_t next_tick = xTaskGetTickCount();
    for (;;) {
        TickType_t now = xTaskGetTickCount();
        TickType_t wait = s_active ? ((int32_t)(next_tick - now) > 0 ? next_tick - now : 0)
                                  : portMAX_DELAY;
        miniapp_event_t event;
        if (xQueueReceive(s_event_queue, &event, wait) == pdTRUE) {
            if (event.type == MINIAPP_EVENT_INSTALL) {
                miniapp_request_t *request = event.request;
                if (event.generation != s_buzzer_generation || alarm_ring_is_active()) {
                    snprintf(request->error, sizeof(request->error), "application changed or alarm is ringing");
                    request->result = -1;
                } else {
                    request->result = miniapp_replace(&L, &vm, request);
                }
                next_tick = xTaskGetTickCount() + pdMS_TO_TICKS(MINIAPP_TICK_MS);
                xSemaphoreGive(request->done);
                /* request belongs to the waiting caller; never touch it again. */
            } else if (event.generation != s_buzzer_generation) {
                /* Input queued for a previous instance cannot affect its replacement. */
                continue;
            } else if (event.type == MINIAPP_EVENT_CLICK && L && !alarm_ring_is_active()) {
                if (miniapp_call_button(L, vm, event.button_id) != 0) {
                    miniapp_runtime_stop(&L, vm, true);
                    lisa_mem_free(vm);
                    vm = NULL;
                }
            } else if (event.type == MINIAPP_EVENT_EXIT && L) {
                miniapp_runtime_stop(&L, vm, true);
                lisa_mem_free(vm);
                vm = NULL;
            }
        }
        /* Events must not starve ticks when the input queue remains busy. */
        now = xTaskGetTickCount();
        if (L && (int32_t)(now - next_tick) >= 0) {
            if (alarm_ring_is_active()) {
                next_tick = now + pdMS_TO_TICKS(MINIAPP_TICK_MS);
                continue;
            }
            if (miniapp_call(L, vm, "on_tick", 1, MINIAPP_TICK_MS) != 0) {
                miniapp_runtime_stop(&L, vm, true);
                lisa_mem_free(vm);
                vm = NULL;
            } else {
                s_status.frames++;
            }
            next_tick = xTaskGetTickCount() + pdMS_TO_TICKS(MINIAPP_TICK_MS);
        }
    }
}

int miniapp_init(void)
{
    if (s_task) {
        return 0;
    }
    s_event_queue = xQueueCreate(8, sizeof(miniapp_event_t));
#if MINIAPP_HAS_BUZZER
    s_buzzer_queue = xQueueCreate(4, sizeof(miniapp_buzzer_command_t));
    if (!s_buzzer_queue) {
        goto failed;
    }
#endif
    if (!s_event_queue || miniapp_ui_init() != 0) {
        goto failed;
    }
#if MINIAPP_HAS_BUZZER
    if (xTaskCreate(miniapp_buzzer_task, "miniapp.buzz", 2048, NULL, 4,
                    &s_buzzer_task) != pdPASS) {
        goto failed;
    }
#endif
    if (xTaskCreate(miniapp_task, "miniapp.lua", 8192, NULL, 5, &s_task) != pdPASS) {
        goto failed;
    }
    return 0;
failed:
    if (s_buzzer_task) {
        vTaskDelete(s_buzzer_task);
        s_buzzer_task = NULL;
    }
    if (s_buzzer_queue) {
        vQueueDelete(s_buzzer_queue);
        s_buzzer_queue = NULL;
    }
    if (s_event_queue) {
        vQueueDelete(s_event_queue);
        s_event_queue = NULL;
    }
    return -1;
}

bool miniapp_is_active(void) { return s_active; }

const char *lsc_get_nlu_custom_mode(void)
{
    return miniapp_is_active() ? "miniapp" : NULL;
}

bool miniapp_install_begin(void)
{
    bool accepted;
    taskENTER_CRITICAL();
    accepted = s_task && !s_install_busy;
    if (accepted) {
        s_install_busy = true;
        s_install_generation = s_buzzer_generation;
    }
    taskEXIT_CRITICAL();
    return accepted;
}

void miniapp_install_end(void)
{
    taskENTER_CRITICAL();
    s_install_busy = false;
    taskEXIT_CRITICAL();
}

bool miniapp_matches(const miniapp_package_t *package)
{
    bool match;
    taskENTER_CRITICAL();
    match = s_active && s_current.size == package->size &&
            strcmp(s_current.id, package->id) == 0 &&
            strcmp(s_current.version, package->version) == 0 &&
            strcmp(s_current.hash, package->hash) == 0;
    taskEXIT_CRITICAL();
    return match;
}

int miniapp_install(const miniapp_package_t *package, const char *source,
                    char *error, size_t error_size)
{
    if (!s_task || !package || !source || !error || !error_size ||
        package->size == 0 || package->size > MINIAPP_SOURCE_MAX ||
        memchr(source, '\0', package->size)) {
        if (error && error_size) {
            snprintf(error, error_size, "invalid Lua source or runtime unavailable");
        }
        return -1;
    }
    miniapp_request_t request = {.package = package, .source = source, .result = -1};
    request.done = xSemaphoreCreateBinary();
    if (!request.done) {
        snprintf(error, error_size, "not enough memory for install request");
        return -1;
    }
    miniapp_event_t event = {.type = MINIAPP_EVENT_INSTALL, .request = &request,
                             .generation = s_install_generation};
    if (xQueueSend(s_event_queue, &event, 0) != pdTRUE) {
        vSemaphoreDelete(request.done);
        snprintf(error, error_size, "miniapp queue is busy");
        return -1;
    }
    /* The MCP worker owns these buffers until the bounded Lua/UI operations
     * finish. A caller timeout must never free an in-flight request. */
    xSemaphoreTake(request.done, portMAX_DELAY);
    vSemaphoreDelete(request.done);
    snprintf(error, error_size, "%s", request.error);
    return request.result;
}

int miniapp_button_click(const char *button_id)
{
    if (!MINIAPP_HAS_BUTTONS) return 0;
    if (!s_active || !button_id || strcmp(button_id, "function") != 0) {
        return -1;
    }
    miniapp_event_t event = {.type = MINIAPP_EVENT_CLICK, .generation = s_buzzer_generation};
    snprintf(event.button_id, sizeof(event.button_id), "%s", button_id);
    return xQueueSend(s_event_queue, &event, 0) == pdTRUE ? 0 : -1;
}

int miniapp_exit(void)
{
    if (!s_active) {
        return -1;
    }
    miniapp_event_t event = {.type = MINIAPP_EVENT_EXIT, .generation = s_buzzer_generation};
    return xQueueSendToFront(s_event_queue, &event, 0) == pdTRUE ? 0 : -1;
}

void miniapp_runtime_ui_closed(void)
{
    if (s_active) {
        (void)miniapp_exit();
    }
}
