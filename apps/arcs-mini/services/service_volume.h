#ifndef SERVICE_VOLUME_H
#define SERVICE_VOLUME_H

#include <stdint.h>

void service_volume_init(void);
void service_volume_set(int volume);
int service_volume_get(void);
void service_volume_adjust(int delta);

#endif
