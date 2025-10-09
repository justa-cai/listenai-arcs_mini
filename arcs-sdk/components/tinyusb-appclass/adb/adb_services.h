#ifndef __ADB_SERVICE_H__
#define __ADB_SERVICE_H__

#include <stdint.h>
#include "adb.h"

#define ADB_SERVICE_MAX_NUM        10
#define ADB_SERVICE_HANDLE_MAX_NUM 3

struct adb_service_handle;

struct adb_service {
	uint8_t used;
	uint32_t local_id;
	uint32_t remote_id;
	const struct adb_service_handle *hd;
	void *data;
};

struct adb_service_handle {
	uint8_t *name;
	int (*open)(struct adb_service *, const uint8_t *);
	int (*close)(struct adb_service *);
	int (*write)(struct adb_service *, adb_packet_t *);
};

uint32_t adb_service_open(const uint8_t *name, const uint8_t *args, uint32_t remote_id);
int adb_service_write(uint32_t local_id, uint32_t remote_id, adb_packet_t *);
void adb_service_close(uint32_t local_id, uint32_t remote_id);
void adb_service_write_remote(struct adb_service *s, uint8_t *data, int len);
int adb_service_hd_register(const struct adb_service_handle const *handle);
#endif
