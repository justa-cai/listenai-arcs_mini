#pragma once

#include "ota_manager.h"

struct model_ota_cb {
    void (*on_ota_state_change)(const ota_state_t *state, void *arg);
};

int model_ota_init(void);

int model_ota_cb_register(const struct model_ota_cb *cb, void *arg);
int model_ota_cb_unregister(const struct model_ota_cb *cb);
