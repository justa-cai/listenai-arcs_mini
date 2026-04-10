#ifndef SERVICE_VOLUME_H
#define SERVICE_VOLUME_H

#include <stdint.h>

void service_volume_init(void);
void service_volume_set(int volume);
void service_volume_set_temp(int volume);
void service_volume_restore_from_kv(void);
int service_volume_get(void);
void service_volume_adjust(int delta);

#endif
