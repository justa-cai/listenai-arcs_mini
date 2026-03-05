#ifndef SERVICE_BRIGHTNESS_H
#define SERVICE_BRIGHTNESS_H

#include <stdint.h>

void service_brightness_init(void);
void service_brightness_set(int brightness);
int service_brightness_get(void);

#endif
