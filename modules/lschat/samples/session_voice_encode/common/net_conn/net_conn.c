#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_heap.h>

#include "csk6/csk_wifi.h"

static csk_wifi_event_cb_t wifi_event_cb;
static struct net_mgmt_event_callback dhcp_cb;
K_SEM_DEFINE(ipgot_sem, 0, 1);
static void handler_cb(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event != NET_EVENT_IPV4_DHCP_BOUND) {
		return;
	}

	char buf[NET_IPV4_ADDR_LEN];

	printk("Your address: %s\n", net_addr_ntop(AF_INET, &iface->config.dhcpv4.requested_ip, buf, sizeof(buf)));
	printk("Lease time: %u seconds\n", iface->config.dhcpv4.lease_time);
	printk("Subnet: %s\n", net_addr_ntop(AF_INET, &iface->config.ip.ipv4->netmask, buf, sizeof(buf)));
	printk("Router: %s\n", net_addr_ntop(AF_INET, &iface->config.ip.ipv4->gw, buf, sizeof(buf)));
	k_sem_give(&ipgot_sem);
}

static void wifi_event_handler(csk_wifi_event_t events, void *event_data, uint32_t data_len, void *arg)
{
	if (events & CSK_WIFI_EVT_STA_CONNECTED) {
		printk("[WiFi sta] connected\n");
	} else if (events & CSK_WIFI_EVT_STA_DISCONNECTED) {
		printk("[WiFi sta] disconnected\n");
	} else {
		abort();
	}
}

int net_init(void)
{
	int ret;
	csk_wifi_init();
	uint8_t mac_addr[6] = {0};
	ret = csk_wifi_get_mac(CSK_WIFI_MODE_STA, mac_addr);
	if (ret != 0) {
		printk("wifi get mac failed, ret: %d\n", ret);
		return 0;
	}
	printk("wifi station mac addr: %x:%x:%x:%x:%x:%x\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3],
	       mac_addr[4], mac_addr[5]);
	wifi_event_cb.handler = &wifi_event_handler;
	wifi_event_cb.events = CSK_WIFI_EVT_STA_CONNECTED | CSK_WIFI_EVT_STA_DISCONNECTED;
	wifi_event_cb.arg = NULL;
	csk_wifi_add_callback(&wifi_event_cb);

	return 0;
}

int net_up(void)
{
	csk_wifi_sta_config_t sta_config = {
		.ssid = "mywifi", .pwd = "12345678", .encryption_mode = CSK_WIFI_AUTH_WPA2_PSK};

	int retry_count = 0;
	csk_wifi_result_t wifi_result;
	do {
		int ret;
		printk("connecting to wifi: %s ...\n", sta_config.ssid);
		ret = csk_wifi_sta_connect(&sta_config, &wifi_result, K_FOREVER);
		if (ret == 0) {
			break;
		} else {
			if (wifi_result == CSK_WIFI_ERR_STA_FAILED) {
				retry_count++;
				printk("retry to connecting wifi ... %d\n", retry_count);
			} else {
				printk("AP not found or invalid password\n");
				return 0;
			}
		}
	} while (retry_count < 10);
	printk("--------------------------Current AP info-------------------------------\n");
	printk("ssid: %s  pwd: %s  bssid: %s  channel: %d  rssi: %d\n", sta_config.ssid, sta_config.pwd,
	       sta_config.bssid, sta_config.channel, sta_config.rssi);
	printk("------------------------------------------------------------------------\n");
	net_mgmt_init_event_callback(&dhcp_cb, handler_cb, NET_EVENT_IPV4_DHCP_BOUND);
	net_mgmt_add_event_callback(&dhcp_cb);
	struct net_if *iface = net_if_get_default();
	if (!iface) {
		printk("wifi interface not available");
		return 0;
	}
	net_dhcpv4_start(iface);

	k_sem_take(&ipgot_sem, K_FOREVER);

	return 0;
}

int net_down(void)
{
	int ret;
	csk_wifi_result_t result;

	ret = csk_wifi_sta_disconnect(&result, K_FOREVER);
	if (ret < 0) {
		printk("station disconnect failed, reason code: %d, ret: %d", result, ret);
		return -1;
	}

	net_mgmt_del_event_callback(&dhcp_cb);

	return 0;
}
