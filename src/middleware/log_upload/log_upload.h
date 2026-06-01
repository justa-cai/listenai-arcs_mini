#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_UPLOAD_STATE_IDLE = 0,
    LOG_UPLOAD_STATE_STARTING,
    LOG_UPLOAD_STATE_UPLOADING,
    LOG_UPLOAD_STATE_SUCCESSED,
    LOG_UPLOAD_STATE_FAILED,
} log_upload_state_e;

typedef struct {
    log_upload_state_e state;
    int result;
} log_upload_state_t;

typedef void (*log_upload_complete_cb_t)(const log_upload_state_t *state, void *arg);

/* Legacy wrapper kept for compatibility. Use log_upload_trigger_with_url() for MCP uploads. */
int log_upload_trigger(void);
/* Schedule one asynchronous upload task with the provided one-time upload URL. */
int log_upload_trigger_with_url(const char *upload_url, log_upload_complete_cb_t complete_cb, void *arg);
int log_upload_get_state_snapshot(log_upload_state_t *state);
int log_upload_is_running(void);

#ifdef __cplusplus
}
#endif
