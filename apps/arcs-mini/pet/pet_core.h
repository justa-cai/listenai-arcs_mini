/*
 * pet_core.h - virtual pet state machine (pure logic, no LVGL/UI includes)
 *
 * Thread model: all entry points take one internal mutex; safe to call from
 * the MCP worker thread (synchronous TTS result) and from UI timers (cheap
 * snapshot copies). Side effects such as animations or sounds are surfaced as
 * pending event bits consumed via pet_core_evt_ack() by the UI thread only.
 *
 * Time model: everything is incremental. Decay is applied in
 * apply_elapsed(dt) chunks measured against an anchor {wall, uptime}; when
 * the wall clock is not SNTP-valid, uptime deltas are used, which means the
 * pet is simply frozen while powered off with no valid clock. Timers (egg
 * age, day cut, poop countdown, sickness zero-duration, reminder cooldowns)
 * are uptime-based accumulators advanced by the same dt.
 */
#ifndef PET_CORE_H
#define PET_CORE_H

#include <stdbool.h>
#include <stdint.h>

/* ---------------- types ---------------- */

typedef enum {
    PET_STAGE_EGG = 0,
    PET_STAGE_BABY,
    PET_STAGE_ADULT,
} pet_stage_t;

typedef enum {
    PET_MOOD_EGG_CALM = 0,
    PET_MOOD_EGG_WOBBLE,
    PET_MOOD_SLEEP,
    PET_MOOD_SICK,
    PET_MOOD_HUNGRY,
    PET_MOOD_SAD,
    PET_MOOD_DIRTY,
    PET_MOOD_TIRED,
    PET_MOOD_CONTENT,
    PET_MOOD_NORMAL,
} pet_mood_t;

/* Keep numeric order in sync with the pet_care MCP tool action enum. */
typedef enum {
    PET_ACTION_FEED = 0,
    PET_ACTION_CLEAN,
    PET_ACTION_PLAY,
    PET_ACTION_LIGHT,
    PET_ACTION_MEDICINE,
    PET_ACTION_STATUS,
    PET_ACTION_NEXT,
    PET_ACTION_CONFIRM,
    PET_ACTION_CANCEL,
    PET_ACTION_GREET, /* offline command words: 你好呀 */
    PET_ACTION_LOVE,  /* 我爱你 */
    PET_ACTION_PAT,   /* 摸摸它 */
    PET_ACTION_NUM,
} pet_action_t;

typedef enum {
    PET_ITEM_MEAL = 0,
    PET_ITEM_SNACK,
    PET_ITEM_LIGHT_WAKE,  /* for PET_ACTION_LIGHT: turn the light on / wake */
    PET_ITEM_LIGHT_SLEEP, /* for PET_ACTION_LIGHT: turn the light off / sleep */
} pet_item_t;

/* UI-facing reaction/reminder events (consumed by the UI thread). */
typedef enum {
    PET_EVT_EAT = 0x1,
    PET_EVT_PLAY = 0x2,
    PET_EVT_CLEAN = 0x4,
    PET_EVT_MEDICINE = 0x8,
    PET_EVT_REJECT = 0x10,
    PET_EVT_HATCH = 0x20,
    PET_EVT_EVOLVE = 0x40,
    PET_EVT_SLEPT = 0x80,
    PET_EVT_WOKE = 0x100,
    PET_EVT_GREET = 0x200,
    PET_EVT_LOVE = 0x400,
    PET_EVT_PAT = 0x800,
    /* reminder announcements (audio/TTS on the UI side) */
    PET_REMIND_HUNGRY = 0x1000,
    PET_REMIND_SAD = 0x2000,
    PET_REMIND_DIRTY = 0x4000,
    PET_REMIND_TIRED = 0x8000,
    PET_REMIND_SICK = 0x10000,
    PET_REMIND_MISS = 0x20000,
} pet_evt_t;

/* Cheap copy for renderers. */
typedef struct {
    uint8_t satiety; /* 0..100 */
    uint8_t happy;
    uint8_t clean;
    uint8_t energy;
    pet_stage_t stage;
    pet_mood_t mood;
    bool sleeping;
    bool sick;
    bool wall_valid;
    uint8_t poops;
    uint8_t egg_crack; /* 0..2 */
    uint16_t age_days;
    uint16_t care_count; /* feeds until hatch hint */
    uint32_t evt_bits;   /* pending events, ack via pet_core_evt_ack() */
} pet_snapshot_t;

typedef struct {
    int code; /* 0 = applied, -1 = rejected, -2 = invalid arg */
    char tts[96];
    uint8_t tone_id; /* PET_TONE_* voice clip (PET_TONE_NONE = silent) */
} pet_result_t;

/* ---------------- persistence (shared with pet_save.c) ---------------- */

#define PET_PERSIST_MAGIC 0x50455431 /* 'PET1' */
#define PET_PERSIST_VERSION 2

typedef struct {
    uint32_t magic;
    uint16_t version;
    /* stats are x10 fixed point, 0..1000 */
    uint16_t satiety, happy, clean, energy;
    uint8_t stage;
    uint8_t sleeping;
    uint8_t sick;
    uint8_t poops;
    uint16_t age_days;
    uint16_t good_days;
    uint16_t good_hours;
    uint16_t care_count;
    /* incremental accumulators, seconds (uptime/decay domain) */
    uint32_t egg_age_s;
    uint32_t day_elapsed_s;
    uint32_t zero_duration_s; /* how long the lowest stat has been 0 */
    int32_t poop_due_s;       /* countdown, -1 = none scheduled */
    uint32_t care_gap_s;      /* time since last interaction */
    uint32_t hour_elapsed_s;  /* good_hour sampling window */
    /* anchor for decay deltas */
    uint8_t anchor_wall_valid;
    uint64_t anchor_wall_s;
    uint64_t anchor_uptime_ms;
    /* last known birth wall time (-1 unknown), for wall-clock day cuts */
    int64_t born_wall_s;
    uint32_t crc;
} pet_persist_t;

/* ---------------- API ---------------- */

/* Load or create the pet. Call once from pet_init(). */
int pet_core_init(void);

/* Advance state; call from a ~1 s UI timer. */
void pet_core_tick(void);

/* Apply an action (voice command / MCP tool / button). Returns the TTS text. */
int pet_core_action(uint8_t action, uint8_t item, pet_result_t *out);

/* Copy the current visible state. */
void pet_core_snapshot(pet_snapshot_t *out);

/* Acknowledge consumed event bits (UI thread). */
void pet_core_evt_ack(uint32_t bits);

/* Wall clock became SNTP-valid (VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS). */
void pet_core_on_clock_valid(void);

/* Save immediately (power-off hook). */
void pet_core_flush_save(void);

/* ---------------- debug/testing helpers (pet shell command) ---------------- */

enum {
    PET_DBG_SLEEPING = 0,
    PET_DBG_SICK,
    PET_DBG_POOPS,
    PET_DBG_EGG_AGE_S, /* egg age in seconds */
    PET_DBG_CARE_COUNT,
    PET_DBG_AGE_DAYS,
    PET_DBG_GOOD_DAYS,
};

/* Set one stat directly: idx 0..3 = satiety/happy/clean/energy, val 0..100. */
int pet_core_debug_set_stat(int idx, int val);

/* Jump to a stage (0 egg / 1 baby / 2 adult) without the evolution event. */
int pet_core_debug_set_stage(int stage);

/* Set a misc field by PET_DBG_* id. */
int pet_core_debug_set_field(int field, int val);

/* Fast-forward the whole state machine by N seconds (decay, poops,
 * sickness, day cut, evolution checks) without waiting for real time. */
int pet_core_debug_time_travel(uint32_t seconds);

/* Inject UI event bits (reaction animations / reminders). */
int pet_core_debug_evt(uint32_t bits);

/* Start over with a fresh egg (drops the save). */
int pet_core_debug_reset(void);

#endif /* PET_CORE_H */
