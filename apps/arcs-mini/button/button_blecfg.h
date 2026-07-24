#ifndef BUTTON_BLECFG_H
#define BUTTON_BLECFG_H

#include <stdbool.h>

int button_blecfg_init(void);
bool button_blecfg_start(void);
bool button_blecfg_is_user_entered(void);

#endif /* BUTTON_BLECFG_H */
