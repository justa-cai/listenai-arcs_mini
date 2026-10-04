/*
 * ss_core.h - SoundSense WebSocket streaming client
 */
#ifndef SS_CORE_H
#define SS_CORE_H

#include <stdbool.h>
#include <stdint.h>

#define SS_CLASS_NUM 2
#define SS_CLASS_NAME_LEN 16
#define SS_EVENT_RING 64

typedef enum {
    SS_STATE_OFF = 0,     /* disabled */
    SS_STATE_RETRY_WAIT,  /* enabled, waiting to retry connect */
    SS_STATE_CONNECTING,  /* handshake in flight */
    SS_STATE_STREAMING,   /* welcome received, pushing audio */
} ss_state_t;

typedef struct {
    char class_name[SS_CLASS_NAME_LEN]; /* "snoring" / "baby_cry" */
    bool is_start;                      /* true = class_start */
    uint32_t ts_ms;                     /* wall clock epoch ms at receipt */
    uint32_t duration_ms;               /* class_end duration */
    uint8_t peak_prob_x100;             /* peak_prob * 100 */
} ss_event_t;

typedef struct {
    ss_state_t state;
    bool enabled;
    char server_url[128];
    uint32_t uptime_s;          /* seconds since enabled */
    uint32_t send_frames;       /* audio frames pushed */
    uint32_t send_drops;        /* frames dropped (queue full / not streaming) */
    uint32_t result_count;      /* result messages consumed */
    uint32_t event_count;       /* total events (start+end) */
    uint32_t reconnect_count;
    int32_t last_probs_x1000[SS_CLASS_NUM]; /* most recent smoothed probs */
    /* per-class running totals (updated on event receipt) */
    uint32_t class_event_count[SS_CLASS_NUM];
    uint32_t class_last_ts_ms[SS_CLASS_NUM];
    bool class_last_is_start[SS_CLASS_NUM];
} ss_status_t;

int ss_core_init(void);

void ss_core_set_enabled(bool enable);
bool ss_core_get_enabled(void);

void ss_core_set_server(const char *url);
const char *ss_core_get_server(void);

void ss_core_get_status(ss_status_t *out);

/* Copy the newest `max` events (oldest first) into out; returns count. */
uint32_t ss_core_get_events(ss_event_t *out, uint32_t max, const char *class_filter);

#endif /* SS_CORE_H */
