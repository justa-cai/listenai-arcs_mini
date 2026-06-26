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

/* 对外接口：使用一次性 upload_url 启动异步上传，可选传入完成回调。 */
int log_upload_trigger(const char *upload_url, log_upload_complete_cb_t complete_cb, void *arg);
/* 对外接口：读取当前上传状态和结果快照。 */
int log_upload_get_state_snapshot(log_upload_state_t *state);
/* 对外接口：返回当前是否有上传任务正在运行。 */
int log_upload_is_running(void);

#ifdef __cplusplus
}
#endif
