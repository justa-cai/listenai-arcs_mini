#ifndef TEST_CHERRYUSB_ADB_DEVICE_HOST_ESP_HEAP_CAPS_H_
#define TEST_CHERRYUSB_ADB_DEVICE_HOST_ESP_HEAP_CAPS_H_

#include <stddef.h>
#include <stdint.h>

#define MALLOC_CAP_DEFAULT  0x00000001u
#define MALLOC_CAP_INTERNAL 0x00000002u

size_t heap_caps_get_largest_free_block(uint32_t caps);

#endif
