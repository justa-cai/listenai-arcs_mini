#include <string.h>
#include <stdbool.h>
#include "wlif.h"
#include "net_ip.h"
#include "net_al.h"
#include "cli_main.h"
#include "ipc_master.h"
#include "ipc_master_utils.h"
#include "shell_def.h"

#include "sysheap.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "evs_utils.h"
#include "listen_wifi.h"
#include "ls_event.h"
#include "wifi_api.h"
#include "ls_wifi_type.h"

#if CONFIG_WIFI_MANAGER
#include "wifi_manager/wifi_manager.h"
#endif

#include "lwip/dns.h"

#define TAG "app-wifi"

static bool s_wifi_statck_init_done = false;
static ls_wifi_stack_init_done_cb s_pre_init_done_cb = NULL;

static int _handle_wifi_got_ip(void *arg)
{
    net_if_t *net_if = net_if_get(WIFI_VIF_STA_IDX);
    if (net_if) {
        uint32_t ip, mask, gw;
        net_if_get_ip(net_if, &ip, &mask, &gw);

        char addr_str[16 + 1];
        ip4_addr_t addr;

        addr.addr = ip;
        if (ip4addr_ntoa_r(&addr, addr_str, 16) != NULL) {
            LISA_LOGI(TAG, "IP: %s", addr_str);
        }

        addr.addr = mask;
        if (ip4addr_ntoa_r(&addr, addr_str, 16) != NULL) {
            LISA_LOGI(TAG, "Mask: %s", addr_str);
        }

        addr.addr = gw;
        if (ip4addr_ntoa_r(&addr, addr_str, 16) != NULL) {
            LISA_LOGI(TAG, "Gateway: %s", addr_str);
        }
    }
    // add new dns server
    ls_wifi_refresh_dnsserver("114.114.114.114");

    return 0;
}

static int _app_wifi_event_cb(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    LISA_LOGI(TAG, "_app_wifi_event_cb event_id:%d",event_id);
    switch (event_id) {
    case EVENT_WIFI_INIT_DONE: {
        LISA_LOGD(TAG, "wifi init done");
        s_wifi_statck_init_done = true;
        // next can connect wifi
        if (s_pre_init_done_cb) {
            s_pre_init_done_cb();
            s_pre_init_done_cb = NULL;
        }
    } break;
    case EVENT_WIFI_CONNECTED: {
        LISA_LOGD(TAG, "wifi connected");
        net_if_t *net_if = net_if_get(WIFI_VIF_STA_IDX);
        if (!net_if->static_ip) {
            // start dhcp
            net_if_up(net_if);
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        }
    } break;
    case EVENT_WIFI_GOT_IP: {
        LISA_LOGD(TAG, "Got IP");
        evs_handler_post_runnable(_handle_wifi_got_ip, NULL);
    } break;
    case EVENT_WIFI_STA_DHCP_FAIL: {
        LISA_LOGE(TAG, "wifi dhcp fail");
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
    } break;
    case EVENT_WIFI_DISCONNECT: {
        LISA_LOGD(TAG, "wifi disconnected");
    } break;
    case EVENT_WIFI_STA_CONNECT_FAIL: {
        LISA_LOGE(TAG, "wifi connect fail");
    } break;
    case EVENT_WIFI_AP_STARTED: {
        LISA_LOGD(TAG, "wifi ap started");
        // start DHCPS
        ls_dhcps_start(WIFI_VIF_AP_IDX);
    } break;
    case EVENT_WIFI_AP_STA_ADD: {
        LISA_LOGD(TAG, "wifi ap sta add");
    } break;
    case EVENT_WIFI_AP_STA_DEL: {
        LISA_LOGD(TAG, "wifi ap sta delete");
    } break;
    case EVENT_WIFI_AP_STOPPED: {
        LISA_LOGD(TAG, "wifi ap stoped");
        ls_dhcps_stop();
    } break;
    default: {
        LISA_LOGW(TAG, "[%s %d]unknow wifi event %d", __FUNCTION__,__LINE__,event_id);
    } break;
    }

    return LS_OK;
}

void ls_wifi_pre_init(ls_wifi_stack_init_done_cb pre_init_done)
{
    s_pre_init_done_cb = pre_init_done;
    int ret = 0;

    struct ipc_master_cb_tag ipc_cb = {.wifi_tx_data_cfm = wlif_tx_cfm,
                                       .wifi_rx_data_ind = wlif_rx_buf_forward,
                                       .indication_handler = ipc_indication_handler};

    ipc_master_init(&ipc_cb);
    // ipc wifi init
    ipc_wifi_init();

    // register event
    ls_event_init();

    // register wifi event
    ls_event_register_cb(EVENT_WIFI, EVENT_ID_ALL, _app_wifi_event_cb, NULL);

    // init shell
    // shell_init(cli_shell_process);

    // init wifi manager
#if CONFIG_WIFI_MANAGER
    do{
        vTaskDelay(pdMS_TO_TICKS(10));
    }while(!s_wifi_statck_init_done);
    /*TODO: remove this delay*/
    vTaskDelay(pdMS_TO_TICKS(1000));
#endif
}

void ls_wifi_refresh_dnsserver(const char *const dns_srv)
{
    if (DNS_MAX_SERVERS >= 2) {
        ip_addr_t dns_ip_addr;
        if (ipaddr_aton(dns_srv, &dns_ip_addr)) {
            const ip_addr_t *dns_srv = dns_getserver(DNS_MAX_SERVERS - 1);
            if (dns_srv->addr != dns_ip_addr.addr) {
                LISA_LOGD(TAG, "%dth old dns server: %s", DNS_MAX_SERVERS, ipaddr_ntoa(dns_srv));

                // set new dns server
                dns_setserver(DNS_MAX_SERVERS - 1, &dns_ip_addr);

                dns_srv = dns_getserver(DNS_MAX_SERVERS - 1);
                LISA_LOGD(TAG, "%dth new dns server: %s", DNS_MAX_SERVERS, ipaddr_ntoa(dns_srv));
            }
        }
    }
}
