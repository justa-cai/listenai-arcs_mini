#ifndef ARCS_MINI_MINIAPP_H
#define ARCS_MINI_MINIAPP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MINIAPP_API_VERSION 3
#define MINIAPP_SCREEN_WIDTH CONFIG_MINIAPP_SCREEN_WIDTH
#define MINIAPP_SCREEN_HEIGHT CONFIG_MINIAPP_SCREEN_HEIGHT
#define MINIAPP_TEXT_FONT_PX 16
#define MINIAPP_SOURCE_MAX CONFIG_MINIAPP_SOURCE_MAX_BYTES
#define MINIAPP_MAX_RECTS 128
#define MINIAPP_MAX_TEXTS 8
#define MINIAPP_TEXT_MAX 63
#define MINIAPP_LUA_HEAP_LIMIT CONFIG_MINIAPP_HEAP_MAX_BYTES
#define MINIAPP_TICK_MS 20u
#define MINIAPP_CHUNK_INSTRUCTION_LIMIT 500000u
#define MINIAPP_CHUNK_DEADLINE_MS 150u
#define MINIAPP_CALLBACK_INSTRUCTION_LIMIT 250000u
/* Wall time includes preemption by display, network and audio tasks. The
 * independent instruction limit still bounds the script's own work. */
#define MINIAPP_CALLBACK_DEADLINE_MS 100u
#define MINIAPP_BUTTON_EXIT_HOLD_MS 3000u
#define MINIAPP_BUZZER_MIN_HZ 100u
#define MINIAPP_BUZZER_MAX_HZ 5000u
#define MINIAPP_BUZZER_MIN_MS 20u
#define MINIAPP_BUZZER_MAX_MS 3000u
#ifdef CONFIG_MINIAPP_SCREEN
#define MINIAPP_HAS_SCREEN 1
#else
#define MINIAPP_HAS_SCREEN 0
#endif
#ifdef CONFIG_MINIAPP_BUTTON
#define MINIAPP_HAS_BUTTONS 1
#else
#define MINIAPP_HAS_BUTTONS 0
#endif
#ifdef CONFIG_MINIAPP_LED
#define MINIAPP_HAS_LEDS 1
#else
#define MINIAPP_HAS_LEDS 0
#endif
#ifdef CONFIG_MINIAPP_BUZZER
#define MINIAPP_HAS_BUZZER 1
#else
#define MINIAPP_HAS_BUZZER 0
#endif

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint32_t color;
} miniapp_rect_t;

typedef struct {
    int16_t x;
    int16_t y;
    uint32_t color;
    char text[MINIAPP_TEXT_MAX + 1];
} miniapp_text_t;

typedef struct {
    uint32_t background;
    uint16_t rect_count;
    uint16_t text_count;
    miniapp_rect_t rects[MINIAPP_MAX_RECTS];
    miniapp_text_t texts[MINIAPP_MAX_TEXTS];
} miniapp_scene_t;

typedef struct {
    bool installed;
    bool active;
    uint32_t source_size;
    uint32_t source_hash;
    uint32_t frames;
    uint32_t dropped_frames;
    uint32_t last_frame_ms;
    uint32_t max_frame_ms;
    char last_error[128];
} miniapp_status_t;

/* Strings are bounded UTF-8 bytes; identifiers are opaque, never paths. */
#define MINIAPP_ID_MAX 127
#define MINIAPP_VERSION_MAX 127

typedef struct {
    char id[MINIAPP_ID_MAX + 1];
    char version[MINIAPP_VERSION_MAX + 1];
    char hash[33];
    uint32_t size;
} miniapp_package_t;

#ifdef CONFIG_MINIAPP
int miniapp_init(void);
bool miniapp_is_active(void);
int miniapp_button_click(const char *button_id);
int miniapp_exit(void);
/* One install at a time, including its download. Never queue a second install. */
bool miniapp_install_begin(void);
void miniapp_install_end(void);
bool miniapp_matches(const miniapp_package_t *package);
int miniapp_install(const miniapp_package_t *package, const char *source,
                    char *error, size_t error_size);
#else
static inline bool miniapp_is_active(void) { return false; }
#endif

int miniapp_ui_init(void);
int miniapp_ui_open(void);
int miniapp_ui_close(void);
int miniapp_ui_present(const miniapp_scene_t *scene);
int miniapp_ui_wait_frame(uint32_t timeout_ms);
void miniapp_runtime_ui_closed(void);

#endif
