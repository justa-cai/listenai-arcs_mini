#pragma once

#include "log_upload.h"

struct model_log_upload_cb {
    void (*on_log_upload_state_change)(const log_upload_state_t *state, void *arg);
};

int model_log_upload_init(void);
int model_log_upload_cb_register(const struct model_log_upload_cb *cb, void *arg);
int model_log_upload_cb_unregister(const struct model_log_upload_cb *cb);
