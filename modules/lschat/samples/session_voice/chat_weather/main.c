#define TAG "main"
#include "lsc.h"
#include "lisa_log.h"
#include "lisa_thread.h"

#define PRODUCT_ID_STRING "ce1dda98-f2b8-46f9-a7b9-8aee214a459b"
#define SECRET_ID_STRING  "c684eb9e-ff8f-4264-ad03-0369b38ab793"

#ifndef DEVICE_ID_STRING
#error "Please define DEVICE_ID_STRING first"
#endif

static void lsc_event_cb(lsc_event_e evt, void *data, uint32_t size, void *usr)
{
	LISA_NLOGI("evt : %s", STRINGS_LSC_EVT(evt));
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
	net_init();
	net_up();

	int ret;

	lsc_config_t cfg = {
		.device_id = DEVICE_ID_STRING,
		.product_id = PRODUCT_ID_STRING,
		.secret_id = SECRET_ID_STRING,
	};
	ret = lsc_init(&cfg);

	ret = lsc_add_callback(LSC_CONNECTED | LSC_DISCONNECTED | LSC_CLOUD_AUTH_FAILD | LSC_GOT_TOKEN, lsc_event_cb,
			       NULL);

	ret = lsc_connect();

	lisa_thread_mdelay(3000);

	extern void chat_weather_proc(void);
	chat_weather_proc();

	while (1) {
		lisa_thread_mdelay(1000);
	}
}