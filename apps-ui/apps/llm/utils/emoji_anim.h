#ifndef __EMOJI_ANIM_H__
#define __EMOJI_ANIM_H__

#include "lisa_ui_anim_ext.h"

#define EMOJI_NAME_NEUTRAL "neutral"
#define EMOJI_NAME_BATTERY "battery"
#define EMOJI_NAME_SLEEPY  "sleepy"
#define EMOJI_NAME_WAIT    "wait"
#define EMOJI_NAME_WAKEUP  "wakeup"

const lisa_ui_anim_ext_config_t *emoji_anim_get_by_name(const char *name);
int emoji_anim_get_loaded_count(void);
const char *emoji_anim_get_loaded_name(int index);
int emoji_anim_is_alias(int index);

int lisa_ui_anim_init(uint32_t flash_addr, uint32_t flash_size);

#endif
