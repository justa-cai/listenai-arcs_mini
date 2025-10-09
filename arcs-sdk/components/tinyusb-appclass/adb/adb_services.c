#define LOG_TAG "adb.srv"

#include "adb_services.h"
#include "adb_shell.h"
#include "adb.h"
#include "adb_utils.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>


static struct adb_service adb_services[ADB_SERVICE_MAX_NUM] = {0};
static struct adb_service_handle const* adb_services_handle[ADB_SERVICE_HANDLE_MAX_NUM] = {0};

#define ADB_SERVICE_LOCAL_ID_DEFAULT 100

static uint32_t local_id_get(void)
{
	static uint32_t local_id = ADB_SERVICE_LOCAL_ID_DEFAULT;
	if (local_id == 0) {
		local_id = ADB_SERVICE_LOCAL_ID_DEFAULT;
	}

	return local_id++;
}

static const struct adb_service_handle *adb_service_handle_find(const uint8_t *name)
{
	int i;

	for (i = 0; i < ADB_SERVICE_HANDLE_MAX_NUM; i++) {
		if (adb_services_handle[i] == NULL) {
			continue;
		}

		if (strcmp((char *)name, (char *)adb_services_handle[i]->name) == 0) {
			return adb_services_handle[i];
		}
	}

	return NULL;
}

static struct adb_service *adb_service_find(uint32_t local_id, uint32_t remote_id)
{
    int i;

    for (i = 0; i < sizeof(adb_services) / sizeof(adb_services[0]); i++) {
        if (adb_services[i].used == 0) {
            continue;
        }

        if (adb_services[i].local_id == local_id && adb_services[i].remote_id == remote_id) {
            return &adb_services[i];
        }
    }

    return NULL;
}

uint32_t adb_service_open(const uint8_t *name, const uint8_t *args, uint32_t remote_id)
{
	uint32_t i;

	const struct adb_service_handle *service_hd = adb_service_handle_find(name);

	if (service_hd == NULL || service_hd->open == NULL) {
		ADB_LOGE("service handle not found, name: %s\n", name);
		return 0;
	}

	for (i = 0; i < sizeof(adb_services) / sizeof(adb_services[0]); i++) {
		if (adb_services[i].used) {
			continue;
		}

		adb_services[i].used = 1;
		adb_services[i].local_id = local_id_get();
		adb_services[i].remote_id = remote_id;
		adb_services[i].hd = service_hd;

		if (service_hd->open(&adb_services[i], args) != 0) {
			ADB_LOGE("service handle open failed, name: %s\n", service_hd->name);
			return 0;
		}

		ADB_LOGI("service open success, idx: %d, local_id: %d, remote_id: %d, handle: %s\n",
			i, adb_services[i].local_id, remote_id, service_hd->name);

		return adb_services[i].local_id;
	}

	ADB_LOGE("no available service slot\n");

	return 0;
}

int adb_service_write(uint32_t local_id, uint32_t remote_id, adb_packet_t *p)
{
	struct adb_service *s = adb_service_find(local_id, remote_id);

	if (s == NULL) {
		ADB_LOGE("service not found, local_id: %d, remote_id: %d\n", local_id, remote_id);
		return -1;
	}

	if (s->hd == NULL || s->hd->write == NULL) {
		ADB_LOGE("service handle invalid, %p, %p\n", s->hd, s->hd->write);
		return -1;
	}

	if (s->hd->write(s, p) != 0) {
		ADB_LOGE("service write failed, local_id: %d, remote_id: %d\n", local_id, remote_id);
		adb_service_close(local_id, remote_id);
		return -1;
	}

	return 0;
}

void adb_service_write_remote(struct adb_service *s, uint8_t *data, int len)
{
	if (s && data && len > 0) {
		adb_write(s->local_id, s->remote_id, data, len);
	}
}

void adb_service_close(uint32_t local_id, uint32_t remote_id)
{
	struct adb_service *s = adb_service_find(local_id, remote_id);

	ADB_LOGI("adb service close, local_id: %d, remote_id: %d, %p\n", local_id, remote_id, s);

	if (s) {
		if (s->hd != NULL && s->hd->close != NULL) {
			s->hd->close(s);
		}

		adb_close(local_id, remote_id);
		ADB_LOGI("adb service close, %p, %p\n", s->hd, s->hd->write);
		s->used = 0;
		s->hd = NULL;
		s->local_id = 0;
		s->remote_id = 0;
        s->data = NULL;
	}
}

int adb_service_hd_register(const struct adb_service_handle *handle)
{
	int i;

	for (i = 0; i < ADB_SERVICE_HANDLE_MAX_NUM; i++) {

		if (adb_services_handle[i] == NULL) {
			ADB_LOGI("service handle register success, name: %s\n", handle->name);
			adb_services_handle[i] = handle;
			return 0;
		}
	}

	ADB_LOGE("service handle register failed, name: %s\n", handle->name);

	return -1;
}
