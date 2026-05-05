#pragma once

#include <stdint.h>

#define BIT(n) (1UL << (n))

#define TRANS_CB_EVENT_RESULT BIT(0)
#define TRANS_CB_EVENT_STATUS BIT(1)

typedef void (*trans_event_cb_t)(uint32_t event, void *event_data, uint32_t event_data_len, void *priv);

int acomp_translation_set_res_type(int type);
int acomp_translation_translate(const char *text, uint32_t len);
int acomp_translation_do_prepare(trans_event_cb_t event_cb, void *priv);
int acomp_translation_do_cleanup(void);
