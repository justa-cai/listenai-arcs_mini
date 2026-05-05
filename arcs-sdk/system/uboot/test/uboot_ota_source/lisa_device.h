#ifndef LISA_DEVICE_H
#define LISA_DEVICE_H

typedef struct lisa_device {
    int reserved;
} lisa_device_t;

lisa_device_t *lisa_device_get(const char *name);

#endif
