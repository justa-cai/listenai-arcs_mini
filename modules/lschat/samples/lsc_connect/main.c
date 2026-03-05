#define TAG "main"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "lsc.h"
#include "lisa_log.h"
#include "lisa_thread.h"
#include "lisa_semaphore.h"

extern int net_init(void);
extern int net_up(void);
extern int net_down(void);

#define PRODUCT_ID_STRING "ce1dda98-f2b8-46f9-a7b9-8aee214a459b"
#define SECRET_ID_STRING  "c684eb9e-ff8f-4264-ad03-0369b38ab793"

#ifndef DEVICE_ID_STRING
#error "Please define DEVICE_ID_STRING first"
#endif

lisa_semaphore_t *sem_connected;

static void lsc_event_cb(lsc_event_e evt, void *data, uint32_t size, void *usr)
{
	LISA_NLOGI("evt : %s", STRINGS_LSC_EVT(evt));
	switch (evt) {
	case LSC_CONNECTED:
		lisa_semaphore_give(sem_connected);
		break;
	default:
		break;
	}
}

__attribute__((weak)) int net_down(void)
{
	return 0;
}

__attribute__((weak)) int net_up(void)
{
	return 0;
}

__attribute__((weak)) int net_init(void)
{
	return 0;
}

int main(void)
{
	int ret = 0;
	net_init();
	net_up();

	sem_connected = lisa_semaphore_create(1);

	lsc_config_t cfg = {
		.device_id = DEVICE_ID_STRING,
		.product_id = PRODUCT_ID_STRING,
		.secret_id = SECRET_ID_STRING,
		.if_auto_reconn = true,
		.reconn_interval_ms = 3000,
	};

	ret = lsc_init(&cfg);
	if (ret) {
		LISA_NLOGE("lsc_init faild(ret = %d)", ret);
		return 0;
	}

	lsc_add_callback(LSC_CONNECTED | LSC_DISCONNECTED | LSC_CLOUD_AUTH_FAILD | LSC_GOT_TOKEN, lsc_event_cb, NULL);

	lsc_connect();
	ret = lisa_semaphore_take(sem_connected, 3000);
	if (ret) {
		LISA_NLOGW("lsc_connect faild");
		return 0;
	}

	lisa_thread_delay(3);

	net_down();

	lisa_thread_delay(10);

	net_up();
}
