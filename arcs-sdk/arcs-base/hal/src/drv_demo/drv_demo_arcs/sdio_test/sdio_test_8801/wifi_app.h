#ifndef WIFI_APP_H_H_H
#define WIFI_APP_H_H_H

#include <stdint.h>
#include "marvell88w8801_core.h"

#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/dns.h"
#include "lwip/dhcp.h"
//#include "lwip/apps/ping/ping.h"
#include "lwip/apps/mqtt.h"

/* WI-FI error */
typedef enum
{
	WIFI_ERR_OK,
	WIFI_ERR_INVALID_TYPE,
	WIFI_ERR_NOT_INIT,
	WIFI_ERR_INVALID_MODE,
	WIFI_ERR_ALREDY_SCANING,
	WIFI_ERR_SCAN_INVALID_MODE,
	WIFI_ERR_NOT_SCANING,
}wifi_err_e;

/* Wi-Fi security */
typedef enum
{
	WIFI_SECURITY_NONE		= 0,
	WIFI_SECURITY_WEP		= 1,
	WIFI_SECURITY_WPA		= 2,
	WIFI_SECURITY_WPA2		= 3,
	WIFI_SECURITY_MAX,
}wifi_security_e;

typedef struct
{
	uint8_t ssid[WIFI_CONF_SSID_MAX_SIZE];		/* Ҫ������ssid�������ֵ����ôֻ������SSID */
	uint8_t ssid_len;										/* Ҫ������ssid���ȣ������0������������� */

	uint8_t channel[WIFI_CONF_CHANNEL_MAX];	/* Ҫ������ͨ�� */
	uint8_t channel_num;								/* Ҫ������ͨ�������������0����������ͨ�������� */

	uint16_t scan_max_time;							/* ����time */
}scan_condition_t;

typedef struct
{
	uint8_t ssid[WIFI_CONF_SSID_MAX_SIZE];		/* ���ӵ�AP��ssid */
	uint8_t ssid_len;										/* ���ӵ�AP��ssid len */
	uint8_t pwd[WIFI_CONF_PWD_MAX_SIZE];
	uint8_t pwd_len;
	wifi_security_e security;							/* ���ӵ�AP�ļ������� */
	uint8_t broadcast_ssid;
}ap_info_t;

uint8_t wifi_start(wifi_cb_t *cb);
uint8_t wifi_stop(void);
uint8_t wifi_start_scan(scan_condition_t *scan_condition);
uint8_t wifi_connect_ap(uint8_t *ssid,uint8_t ssid_len,uint8_t *pwd,uint8_t pwd_len);
uint8_t wifi_disconnect_ap(void);
uint8_t wifi_start_ap(ap_info_t *ap_info);
uint8_t wifi_stop_ap(void);
uint8_t wifi_disconnect_sta(uint8_t *mac_address);
void app_udp_dhcp_show_ip(uint8_t *ip,uint8_t *mask,uint8_t *gateway);

#endif

