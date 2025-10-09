/**
 ****************************************************************************************
 *
 * @file atcmd_iperf.c
 *
 * @brief
 *
 * Copyright (C) ListenAI  2023-2024
 *
 ****************************************************************************************
 */
#include <stdbool.h>
#include "atcmd.h"
#include "log_print.h"
#include "lwiperf.h"
#include "lwip/inet.h"
#include "net_ip.h"
#include "ls_wifi_type.h"

extern void lwiperf_start_tcp_server_default_task(void *arg);
extern void lwiperf_start_tcp_client_task(void *arg);

int atcmd_lwiperf_start_tcp_server_default(int type, char *arg)
{
	if(xTaskCreate(lwiperf_start_tcp_server_default_task, ((const char*)"lwiperf_server"), DEFAULT_THREAD_STACKSIZE, arg, DEFAULT_THREAD_PRIO, NULL) != pdPASS)
	{	
		CLOGE("ERROR: Create lwiperf tcp server task failed.\r\n");
		return -1;
	}
	return 0;
}

void lwiperf_start_tcp_server_default_task(void *arg)
{
	char *cur;
    char *next = arg;
    int8_t token_idx = -1;
    struct ip_addr_cfg cfg = {0};

	int error_no = 0;
	uint16_t remote_port;

    if(arg) {
		do
		{
			cur = atcmd_next_token(&next);
			token_idx++;
			
			switch (token_idx)
			{
				case 0:
				{
					remote_port = atoi((char*)cur);
					if(remote_port < 1 || remote_port > 65535) {
						error_no = 1;
						goto err_exit;
					}
					break;
				}

				default:
					error_no = 10;
					goto err_exit;
			}
		} while(next);
	} else {
		remote_port = 5001;
	}
    
	/* get ip addr */
        ls_get_ip(WIFI_VIF_DEFAULT_IDX, &cfg);
	ip_addr_t ip_addr = {
	            .addr = cfg.ipv4.addr,
	};

	lwiperf_start_tcp_server((ip_addr_t *)&ip_addr, remote_port, NULL, NULL);

err_exit:
	if(error_no) {
		CLOGE("[%s]: err:%d", __FUNCTION__, error_no);
	}
	vTaskDelete(NULL);
	return;
} 

int atcmd_lwiperf_start_tcp_client(int type, char *arg)
{
	
	if(xTaskCreate(lwiperf_start_tcp_client_task, ((const char*)"lwiperf_client"), DEFAULT_THREAD_STACKSIZE, arg, DEFAULT_THREAD_PRIO, NULL) != pdPASS)
	{	
		CLOGE("ERROR: Create lwiperf tcp client task failed.\r\n");
		return -1;
	}
	return 0;
}

void lwiperf_start_tcp_client_task(void *arg)
{
	char *cur;
    char *next = arg;
    int8_t token_idx = -1;

	int error_no = 0;
	uint16_t remote_port;
	ip_addr_t addr;
    uint32_t amount = 10;
	if(arg) {
		do
		{
			cur = atcmd_next_token(&next);
			token_idx++;
			
			switch (token_idx)
			{
				case 0:
				{
					if(cur != NULL) {
						if (inet_aton(cur, &addr) == 0) {
							error_no = 1;
							goto err_exit;
						}
					} else {
						error_no = 2;
						goto err_exit;
					}
					break;
				}
				case 1:
				{
					if(cur != NULL) {
						remote_port = atoi((char*)cur);
						if(remote_port < 1 || remote_port > 65535) {
							error_no = 3;
							goto err_exit;
						}
					} else {
						error_no = 4;
						goto err_exit;
					}
					break;
				}
				case 2:
				{
					if(cur != NULL) {
						amount = atoi((char*)cur);
						if(amount < 10 ) {
							error_no = 5;
							goto err_exit;
						}
					} else {
						amount = 10;
					}
					break;
				}
				default:
					error_no = 10;
					goto err_exit;
					break;
			}
		} while(next);
	} else {
		error_no = 11;
		goto err_exit;
	}

	lwiperf_start_tcp_client( &addr, remote_port, LWIPERF_CLIENT, NULL, NULL, amount);

err_exit:
	if(error_no) {
		CLOGE("[%s]: err:%d", __FUNCTION__, error_no);
	}
		
	vTaskDelete(NULL);
	return;
}

const atcmd_item_t atcmd_iperf_table[] =
{
   {atcmd_lwiperf_start_tcp_client, "AT+IPERFCLI", "iperf client\r\n"},
   {atcmd_lwiperf_start_tcp_server_default, "AT+IPERFSER", "iperf server\r\n"},
};

void atcmd_iperf_register(void)
{
    atcmd_entry_add_table(atcmd_iperf_table, sizeof(atcmd_iperf_table)/sizeof(atcmd_item_t));
}

void atcmd_iperf_help(void)
{
    int i;
    int item_len;
    item_len = sizeof(atcmd_iperf_table)/sizeof(atcmd_item_t);
    for (i = 0; i < item_len; i++)
        CLOGI("%s: %s\n", atcmd_iperf_table[i].atcmd_entry.name, atcmd_iperf_table[i].atcmd_entry.help);
}