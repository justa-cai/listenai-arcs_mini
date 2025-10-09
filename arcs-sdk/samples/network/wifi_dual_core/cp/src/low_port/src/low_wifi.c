
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#include "wifi_api.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/inet.h" 

#include "low_wifi.h"
#include "plat_os.h"
#include "ls_bit_defs.h"

#include "ipc_master.h"
#include "ipc_master_utils.h"
#include "ls_event.h"
#include "wlif.h"
#include "net_al.h"
#include "net_ip.h"
#include "wifi_api.h"
#include "log_print.h"

#include "user_wifi.h"
#include "low_system.h"

#include "user_lisa_tag_def.h"

enum ap_stat
{
    AP_STAT_STOP = 0,       //ap没有开
    AP_STAT_READY,          //热点已经建立
    AP_STAT_APP_CONNECTED,  //有设备连到热点上
    AP_STAT_APP_DISCONNECT, //设备断开连接
    AP_STAT_APP_REVERSED1,  //保留
    AP_STAT_CONNECTING,     //收到配置的网络，处于尝试连网状态
    AP_STAT_SUCCESS,        //连网成功
    AP_STAT_FAILED,         //连网失败
};

enum sta_stat
{
    STAT_IDLE,
    STAT_CONNECTING,    
    STAT_CONNECTED,
    STAT_DISCONNECTED,
    STAT_CONNECT_FAILED
};

#define MAC_STR_SIZE				20 

#define WIFI_AP_STAT_STOP			0
#define WIFI_AP_STAT_START			1

#define WIFI_AP_CTRL_STOP			0
#define WIFI_AP_CTRL_START			1

#define WIFI_AP_SCAN_RESULT_NONE 	0			//未搜索到指定的热点
#define	WIFI_AP_SCAN_RESULT_HAVE  	1			//搜索到了指定的热点

#define WIFI_AP_SCAN_TIMEOUT		(20 * 1000)	//搜索热点的最大超时时间

static int32_t low_wifi_initflag = 0;
static int32_t use_fast_connect_next_time = 0;

static bool ls_wifi_started=false;
static enum ap_stat   ap_stat   = AP_STAT_STOP;
static enum sta_stat  sta_stat  = STAT_IDLE;
static TimerHandle_t  timer_handle_ip   = NULL;
static TimerHandle_t  timer_handle_connect  = NULL;
static int32_t low_wifi_err_code = 0;
static uint32_t low_wifi_ip = 0;
#define CONNECT_RETRY_CNT   3
static int32_t connect_retry = 8;  //连上网的重试次数
static int32_t wifi_ap_scan_result = WIFI_AP_SCAN_RESULT_NONE;		//是否查询到指定的ssid，
#define WIFI_CONNECT_TIMOUT 10000   //连上网断的超时时间
#define GET_IP_TIMEOUT      15000    //连到网络后获取IP的超时时间，这么长时间内没有获取到ip的话就认为这个网络连接失败

/* FreeRTOS event group to signal when wifi init done*/
static EventGroupHandle_t s_wifi_event_group;

#define WIFI_INIT_DONE_BIT BIT0

static int32_t low_wifi_mac_str_to_int(char *mac_str, uint8_t mac[]);
#define low_wifi_err_code_set(v)    low_wifi_err_code = v

void low_wifi_use_fast_connect_next_time(){
    use_fast_connect_next_time = 1;
    CLOGI("[low_wifi]use fast connect next time!\n"); 
}

void handle_wifi_disconnection(int32_t err) {
    low_wifi_err_code_set(err);
    sta_stat = STAT_DISCONNECTED;
}

void low_wifi_timout_cb_connect(TimerHandle_t timer)
{
    CLOGI("[low_wifi]connect timout!\n");
	connect_retry = CONNECT_RETRY_CNT;
    wifi_sta_disconnect();
    handle_wifi_disconnection(WIFI_REASON_CONNECT_TIMEOUT);
}

int32_t low_wifi_timer_start_connect(void)
{
    int32_t timer_id;
    timer_id = 3;
    timer_handle_connect = xTimerCreate("con_tim", WIFI_CONNECT_TIMOUT, pdFALSE, (void*)timer_id, low_wifi_timout_cb_connect);
    if(timer_handle_connect != NULL)
    {
        xTimerStart(timer_handle_connect, 10);
        return 0;
    }
    CLOGI("[low_wifi]timer_start_connect failed\n");
    return -1;
}

void low_wifi_timer_stop_connect(void)
{
    if(timer_handle_connect == NULL)
    {
        return;
    }
    xTimerStop(timer_handle_connect, 10);
    timer_handle_connect = NULL;
}

void low_wifi_timout_cb_get_ip(TimerHandle_t timer)
{
    CLOGI("[low_wifi]getip timout!\n");
    wifi_sta_disconnect();
    ls_dhcpc_stop(WIFI_VIF_STA_IDX);
    handle_wifi_disconnection(WIFI_REASON_GET_IP_TIMEOUT);
}

int32_t low_wifi_timer_start_get_ip(void)
{
    int32_t timer_id;
    timer_id = 2;
    timer_handle_ip = xTimerCreate("ip_tim", GET_IP_TIMEOUT, pdFALSE, (void*)timer_id, low_wifi_timout_cb_get_ip);
    if(timer_handle_ip != NULL)
    {
        xTimerStart(timer_handle_ip, 10);
        return 0;
    }
    CLOGI("[low_wifi]timer_start_get_ip failed\n");
    return -1;
}

void low_wifi_timer_stop_get_ip(void)
{
    if(timer_handle_ip == NULL)
    {
        return;
    }
    xTimerStop(timer_handle_ip, 10);
    timer_handle_ip = NULL;
}
//  TickType_t end_time = 0;
//  TickType_t start_time = 0;

int wifi_event_cb_t(void *arg, event_module_t event_module, int event_id, void *event_data)
{
    event_ap_sta_add_param_t *sta_add_param;
    event_ap_sta_del_param_t *sta_del_param;
    event_connect_fail_param_t *conn_fail_evt;
    event_disconnect_param_t *disc_evt;
    event_scan_done_param_t *scan_evt;

    uint32_t tmp;


    switch (event_id) {
    case EVENT_WIFI_INIT_DONE:
        CLOGI("event <%d %d>  wifi init done\n", event_module, event_id);
        xEventGroupSetBits(s_wifi_event_group, WIFI_INIT_DONE_BIT);
        break;
    case EVENT_WIFI_CONNECTED:
    // end_time = xTaskGetTickCount();
    // CLOGI("Request took %.2f seconds\n", 
    //       (float)(end_time - start_time)/configTICK_RATE_HZ);
        net_if_t *net_if;
        CLOGI("event <%d %d>  connected \n", event_module, event_id);
        wlif_netif_up(WIFI_VIF_STA_IDX, VIF_STA);
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        if (net_if && !net_if->static_ip) {
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        }
        low_wifi_timer_start_get_ip();
        break;
    case EVENT_WIFI_GOT_IP:
        sta_stat = STAT_CONNECTED;
        low_wifi_timer_stop_get_ip();
        wifi_ssid_ip_save();
        CLOGI("event <%d %d>  IP obtained \n", event_module, event_id);
        break;
    case EVENT_WIFI_STA_DHCP_FAIL:
        CLOGI("event <%d %d>  DHCP FAILED \n", event_module, event_id);
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        break;
    case EVENT_WIFI_DISCONNECT:
        disc_evt = (event_disconnect_param_t *)event_data;
        CLOGI("event <%d %d>  disconnected:%d max retry reach %d \n", event_module, event_id, disc_evt->reason_code, disc_evt->max_retry_reach);
        wlif_netif_down(WIFI_VIF_STA_IDX);
        CLOGI("wlif_netif_down completed\n");
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        CLOGI("ls_dhcpc_stop completed\n");
        low_wifi_timer_stop_get_ip();
        tmp = disc_evt->reason_code;
        handle_wifi_disconnection(tmp);
        break; 
    case EVENT_WIFI_SCAN_DONE:
        scan_evt = (event_scan_done_param_t *)event_data;
        CLOGI("event <%d %d>  scan done \n", event_module, event_id);
        if (scan_evt->status)
            CLOGI("scan success, scan cnt %d  \n", scan_evt->result_cnt);
        break;
    case EVENT_WIFI_STA_CONNECT_FAIL:
        conn_fail_evt = (event_connect_fail_param_t *)event_data;
        low_wifi_timer_stop_get_ip();
        handle_wifi_disconnection(WIFI_REASON_CONNECT_TIMEOUT);
        CLOGI("event <%d %d>  connect fail:%d max retry reach %d \n", event_module, event_id, conn_fail_evt->reason_code, conn_fail_evt->max_retry_reach);
        break;
    default:
        CLOGI("rx event <%d %d>\n", event_module, event_id);
        break;
    }

    return LS_OK;
}

os_thread_entry hw_wifi_init_handle(os_thread_arg_t parm)
{
#ifndef INSTALL_FW
    uint8_t mac[6] = {0};
    customer_wifi_start();
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_INIT_DONE_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    CLOGI("init done\n");
    ls_wifi_started=true;
    low_wifi_initflag = 1;
    CLOGI("[low wifi]init success\n");
    //CwifiManager要求系统启动后，CWifiManger接手之前，wifi处于关闭状态。
    //但因为聆思当前的wifi在此处关闭后会导致scan总是失败，所以暂不处理
    //新方案是在系统启动后，检查wifi开关状态，如果是开启状态，走mod_wifi_on，否则走mod_wifi_off
    // low_wifi_off();
#endif
    os_thread_delete(NULL);
}

extern void *ppGetTaskHdl(void);

int32_t low_wifi_hw_init(void){
    CLOGD("[low_wifi]hwinit start\n");
    int32_t rev = 100;
    if (low_wifi_initflag) {
    CLOGD("[low_wifi]hwinit ok\n");
        return 0;
    }

    uint8_t mac[6] = {0};

    if(low_wifi_get_mac_addr(mac) == 0) {
        ipc_wifi_mac_pre_set(mac);
    } else {
        CLOGI("[low wifi]wifi_mac_set failed\n");
    }
    s_wifi_event_group = xEventGroupCreate();
    // register event
    customer_wifi_event_start(wifi_event_cb_t);

    // user_wifi_pre_init();
    os_thread_t wifi_init_handle;
    os_thread_create(&wifi_init_handle, "app_wifi_init", hw_wifi_init_handle, NULL, 8*1024, OS_PRIO_2);
    return 0;
}
/// @brief Wifi开启（硬件射频启用，但无需重新调用硬件初始化）。需要包含所有在wifi关闭之后重新打开时执行的操作
/// @param  
/// @return 
int32_t low_wifi_on()
{    
    CLOGD("[low_wifi]init start\n");
    CLOGD("[low_wifi]on start\n");
    wifi_on();
    ls_wifi_started = true;
    CLOGD("[low_wifi]init ok\n");
    CLOGD("[low_wifi]on ok\n");
    return 0;
}
/// @brief Wifi关闭（硬件射频关闭）
/// @param  
/// @return 
int32_t low_wifi_off()
{
    CLOGI("[low_wifi]term start\n");
    CLOGI("[low_wifi]off start\n");
    low_wifi_disconnect();
    wifi_off();
    ls_wifi_started = false;
    CLOGI("[low_wifi]term over\n");
    CLOGI("[low_wifi]off over\n");
    return 0;
}

int32_t low_wifi_factory_set_mac_addr(char *mac_str)
{
    // 48:38:B6:45:B5:0D
    int len = strlen(mac_str);
    if((len != 12) && (len != 17)) {
        CLOGI("sn length error, sn:%s, len:%d\n", mac_str, len);
        return -1;
    }
    CLOGI("factory mac set:%s\n", mac_str);
    lisa_kv_set_string(LISA_KV_DEV_WIFI_MAC, mac_str);
    return 0;
}

int32_t low_wifi_get_mac_addr(uint8_t mac[])
{
    char *mac_str = NULL;
    lisa_kv_get_string(LISA_KV_DEV_WIFI_MAC, &mac_str);
    if(mac_str == NULL) {
        CLOGI("lisa kv get mac is NULL\n");
        return -1;
    }

    int32_t rev, i, m[6];
    if(strlen(mac_str) == 17) {
        rev = sscanf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]);
    } else if(strlen(mac_str) == 12) {
        rev = sscanf(mac_str, "%02x%02x%02x%02x%02x%02x",
                &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]);
    } else {
        CLOGI("mac_str length error, mac_str:%s, len:%d\n", mac_str, strlen(mac_str));
        if (mac_str) lisa_kv_free(mac_str);
        return -1;
    }

    if(rev == 6)
    {
        for(i = 0; i < 6; i++)
        {
            mac[i] = m[i];
        }
        CLOGI("get mac:%02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        if (mac_str) lisa_kv_free(mac_str);
        return 0;
    } else {
        CLOGI("get mac error, mac_str:%s, len:%d\n", mac_str, strlen(mac_str));
    }

    if (mac_str) lisa_kv_free(mac_str);
    return -1;
}

void low_wifi_bssid_show(uint8_t* bssid)
{
    int32_t i;
    for(i = 0; i < 5; i++)
    {
        CLOGI("%x:", bssid[i]);
    }
    CLOGI("%x\n", bssid[i]);
}

#define MAX_SCAN_NUM 32
int32_t low_wifi_scan(wifi_scan_cb cb, void* userdata)
{
#if 0
#else
    int32_t i;
    uint16_t len, wait_time = 3000;
    int ap_num = 0;
    scan_result_t result;
    char is_timeout = 0;
    int res;
    ls_err_t ret;
    wifi_scan_params_t config = {0};
    wifi_scan_result_t *results;
    // ls_event_register_cb(EVENT_WIFI, EVENT_WIFI_SCAN_DONE, wifi_scan_done_cb, NULL);

    if (!ls_wifi_started) {
        CLOGI("[low_wifi]low wifi not started,can not scan\n");
        low_wifi_err_code_set(WIFI_REASON_NOT_START);
        return -1;
    }

    ret = wifi_scan_start(&config);
    if (ret != LS_OK)
    {
        CLOGI("[low_wifi]scan find none ssid\n");
        return -1;
    }

    ls_event_wait(EVENT_WIFI, EVENT_WIFI_SCAN_DONE, LS_NEVER_TIMEOUT);
    // as scan_results will be write on the other core and read on this core,
    // should use rtos_aligned_malloc, aligned by HAL_DCACHE_CFG_LINE_SIZE
    results = rtos_aligned_malloc(sizeof(wifi_scan_result_t) * MAX_SCAN_NUM, HAL_DCACHE_CFG_LINE_SIZE);
    if (!results)
    {
        CLOGI("[low_wifi]scan alloc ap_record buf NULL\n");
        while(1);
    }
    wifi_sta_scanlist_dump(results, MAX_SCAN_NUM, &ap_num);

#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1)
    if ((results >= PSRAM_BASE_ADDRESS) && DCachePresent()) {
        vPortEnterCritical();
        HAL_InvalidateDCache_by_Addr((uint32_t *)scan_results, sizeof(wifi_scan_result_t) * MAX_SCAN_NUM);
        vPortExitCritical();
    }
#endif

    CLOGI("Got %d scan results\n", ap_num);
    if (ap_num == 0) {
        CLOGI("[low_wifi]scan done on ap found\n");
        rtos_aligned_free(results);
        return -1;
    }
    for (i = 0; i < ap_num; i++) {
        snprintf(result.ssid, LOW_WIFI_SSID_LEN, "%s", results[i].ssid);
        // CLOGI("result.ssid is %s\n", results[i].ssid);
        snprintf(result.bssid, 20, "%s", low_wifi_str_of_mac(results[i].bssid) ?: "");
        // CLOGI("result.bssid is %s\n", result.bssid);
        result.rssi = results[i].rssi;
        result.authmode = results[i].auth;
        result.pairwise_cipher = results[i].cipher;
        // result.group_cipher = results[i].group_cipher;  //no group_cipher
        result.open_flag = results[i].auth == WIFI_SEC_OPEN;
        if (cb) {
            cb(&result, userdata);
        }
    }
    rtos_aligned_free(results);

#endif
    return 0;
}

void low_wifi_cfg_reset(low_wifi_cfg_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->probe = NULL;
}

typedef struct ssid_bssid_stren_struct
{
    char ssid[LOW_WIFI_SSID_LEN];
    char bssid[LOW_WIFI_BSSID_LEN];
    int8_t rssi; 
}ssid_bssid_stren_t;

/*
 * 用于查找一个ssid信号最强的信号的bssid时的回调
 */
int32_t ssid_highest_bssid_cb(scan_result_t* result, void* parm)
{
    ssid_bssid_stren_t *highest = parm;
    if(strcmp((char*)result->ssid, highest->ssid) == 0)
    {
        if(result->rssi > highest->rssi)
        {
            memcpy(highest->bssid, result->bssid, 6);
            highest->rssi = result->rssi;
            CLOGI("[low_wifi]hiest_bssid_cb higher:%d %s\n",
                    highest->rssi, result->bssid?:"");
        }
        else
        {
            CLOGI("[low_wifi]lower %d %s\n", result->rssi, result->bssid?:"");
        }
    }
    return 0;
}

/*
 * 获取环境中这个ssid强度最强的一个网络的bssid
 * retval:
 *  0:找到了对应的bssid
 * -1:没有找到bssid,这应该是一个隐藏网络
 */
#define RSSI_LOWEST -120
int32_t low_wifi_ssid_highest_bssid(char* ssid, uint8_t* bssid)
{
    ssid_bssid_stren_t stren;
    memset(&stren, 0, sizeof(ssid_bssid_stren_t));
    snprintf(stren.ssid, LOW_WIFI_SSID_LEN,"%s", ssid);
    stren.rssi = RSSI_LOWEST;
    low_wifi_scan(ssid_highest_bssid_cb, &stren); 
    if(stren.rssi != RSSI_LOWEST)
    {
        memcpy(bssid, stren.bssid, 6); 
        CLOGI("[low_wifi]ssid_highest_bssid ssid:%s rssi:%d ", ssid, stren.rssi);
        low_wifi_bssid_show(bssid);
        return 0;
    }
    return -1;
}

static char *glast_wifi=NULL, *glast_key=NULL;
static bool last_wifi_read=false;

void save_last_wifi(char* ssid)
{
    if (!ssid) return;
    if(glast_wifi)
    {
        if(strcmp(ssid,glast_wifi)==0) //same wifi
            return;
    }
    else
        glast_wifi=os_mem_alloc(LOW_WIFI_SSID_LEN); //nvs string limit
#if 0
    snprintf(glast_wifi,LOW_WIFI_SSID_LEN,ssid);
    nvs_t nvs_write={"wifi","lastw",glast_wifi,0,0};
    write_nvs_by_key(&nvs_write);
    printf("[low wifi]last wifi %s saved.",glast_wifi);
#else
#if CFG_NVS
    nvds_put(NVDS_TAG_WIFI_STA_SSID, NVDS_LEN_WIFI_STA_SSID, glast_wifi);
    printf("[low wifi]last wifi %s saved.",glast_wifi);
    #endif
#endif
}

char *read_last_wifi(void)
{
#if CFG_NVS
    int ret;
    uint8_t ssid[WIFI_SSID_LEN + 1] = {0};
    size_t len = WIFI_SSID_LEN;
    if (!last_wifi_read) {
        ret = nvds_get(NVDS_TAG_WIFI_STA_SSID, &len, ssid);
        if (ret == NVDS_OK && len <= WIFI_SSID_LEN) {
            if (!glast_wifi) {
                glast_wifi = os_mem_alloc(LOW_WIFI_SSID_LEN);
            }
            snprintf(glast_wifi, LOW_WIFI_SSID_LEN, ssid);
        }
        last_wifi_read = true;
    }

#endif

    return glast_wifi;
}

//not support
int32_t low_wifi_set_backup_dns_server(char *ip)
{
#if 0
    int32_t rev;
    tcpip_adapter_dns_info_t dns;
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ipaddr_addr(ip);
    rev = tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, ESP_NETIF_DNS_BACKUP, &dns);
    return rev == ESP_OK ? 0 : -1;
#endif
}

//extern void low_http_dns_cache_reset(void);


int32_t low_wifi_connect_inner(low_wifi_cfg_t* cfg, size_t fast_connect)
{
    int32_t rev = 0;
    if(cfg == NULL){
        return -1;
    } 
    if (!ls_wifi_started) {
        CLOGI("[low_wifi]low wifi not started,inner can not connect\n");
        low_wifi_err_code_set(WIFI_REASON_NOT_START);
        return -1;
    }
    wifi_connect_cfg_t sta_config = {0};
    low_wifi_err_code_set(WIFI_REASON_NONE);
    snprintf(sta_config.ssid, 32, "%s", cfg->ssid);    // ls ssid 数组长度32
    snprintf(sta_config.key, 64, "%s", cfg->key); // ls password 数组长度64
    if (strlen(cfg->bssid)) {                          // 如果设置了bssid这里使用bssid
        rev = low_wifi_mac_str_to_int(cfg->bssid, sta_config.bssid);
        if (!rev) {
            CLOGI("\n[low wifi]connecting, set bssid:%s\n", cfg->bssid);
        } else {
            CLOGI("\n[low wifi]fail to set bssid!\n");
        }
    }
    sta_config.scan_method = NORMAL_SCAN;
    sta_config.failure_retry_cnt = connect_retry;
    sta_stat = STAT_CONNECTING;
    CLOGI("ssid is %s\n",sta_config.ssid);
    CLOGI("key is %s\n",sta_config.key);
    // start_time = xTaskGetTickCount();
    if(fast_connect) {
        CLOGI("do fast wifi connect\n");
        rev += user_wifi_connect(&sta_config);
    } else {
        CLOGI("do normal wifi connect\n");
        rev += wifi_sta_connect(&sta_config);
    }
    CLOGI("[low_wifi]wifi_connect rev:%d,ssid:%s\n", rev, cfg->ssid ?: "");
    while (sta_stat == STAT_CONNECTING) {
        CLOGI("fast_connect=%d\n", fast_connect);
        vTaskDelay(800);
    }

    if(sta_stat == STAT_DISCONNECTED)
	{
        switch(low_wifi_err_code_get())
        {
        case 2:
            CLOGI("[low_wifi]connect failed for err key[%s]\n", cfg->key?:"");
            rev = LOW_CONN_ERR_KEY;
            break;
        case WIFI_REASON_CONNECT_TIMEOUT:
            rev = LOW_CONN_ERR_TOAP;
            break;
        case WIFI_REASON_GET_IP_TIMEOUT:
            rev = LOW_CONN_ERR_IP;
            break;
        default:
            rev = LOW_CONN_ERR_TOAP;
        }
        return rev;
	}

    if (cfg->probe != NULL) {
        CLOGI("[low wifi]connected do probe\n");
        os_thread_sleep(100);
        rev = cfg->probe();
        if(rev) //add backup url,no need to retry
        {
            CLOGI("[low wifi]connect ok but probe failed!retry!\n");
            os_thread_sleep(500);
            rev = cfg->probe();
        }
    } else {
        CLOGI("[low_wifi]connect probe no need\n");
        rev = 0;
    }
    if (rev) {
        CLOGI("[low wifi]connect ok but probe failed\n");
        return LOW_CONN_ERR_NOOUT;
    }
    //low_http_dns_cache_reset();//如果正在使用dns(切换网络)，此时reset会崩溃
	return 0;
}
int32_t low_wifi_fast_connect_inner(low_wifi_cfg_t* cfg){
    return low_wifi_connect_inner(cfg, 1);
}
int32_t low_wifi_normal_connect_inner(low_wifi_cfg_t* cfg){
    return low_wifi_connect_inner(cfg, 0);
}
int32_t low_wifi_connect(low_wifi_cfg_t* cfg){
    if(cfg == NULL){
        return -1;
    }
    if (!ls_wifi_started) {
        CLOGI("[low_wifi]low wifi not started,can not connect\n");
        low_wifi_err_code_set(WIFI_REASON_NOT_START);
        return -1;
    }
    if(use_fast_connect_next_time){
        use_fast_connect_next_time = 0;
        int32_t rev = low_wifi_fast_connect_inner(cfg);
        if (rev == 0|| rev == LOW_CONN_ERR_KEY)
            return 0;
    }
    return low_wifi_normal_connect_inner(cfg);
}
int32_t low_wifi_disconnect(void)
{
#if 0
    connect_retry = CONNECT_RETRY_CNT; //不允许重连
    printf("[low_wifi]disconnect sta_stat:%d\n", sta_stat);
    switch(sta_stat)
    {
        case STAT_IDLE:
            break;
        case STAT_CONNECTING:
            printf("[low_wifi]wifi_disconnect when is connecting\n");
            while(sta_stat == STAT_CONNECTING) //正在连网，等连网结束
            {
                ets_printf(".");
                os_thread_sleep(50);
            }
            if(sta_stat == STAT_DISCONNECTED) //如果连网失败，这里什么也不用做
            {
                break;
            }
            //连网成功了，和STAT_CONNECTED同样处理
        case STAT_CONNECTED:
			connect_retry = CONNECT_RETRY_CNT;
            esp_wifi_disconnect();
            sta_stat = STAT_DISCONNECTED; //网络断开后把状态设下
            break;
        case STAT_DISCONNECTED: //当前网络是断开的，什么也不用做
            break;
        default:
            printf("[low_wifi]wifi_disconnect err stat:%d\n", sta_stat);
            return 0;
    }
#else
    wifi_sta_disconnect();
    low_wifi_timer_stop_get_ip();
    ls_dhcpc_stop(WIFI_VIF_STA_IDX);
    handle_wifi_disconnection(WIFI_REASON_NONE);
#endif
    return 0;
}
int32_t low_wifi_stat_chk_connect(void)
{
#define WIFI_STAT_RET_OK 0
#define WIFI_STAT_RET_NOT_OK !WIFI_STAT_RET_OK
    int link_status = 0;
    if (!ls_wifi_started) {
        CLOGI("[low_wifi]low wifi not started,can not check connect\n");
        low_wifi_err_code_set(WIFI_REASON_NOT_START);
        return WIFI_STAT_RET_NOT_OK;
    }
    //聆思已经修复link_status的bug
    wifi_get_sta_state(&link_status);
    CLOGI("[low_wifi]low wifi link_status:%d\n",link_status);
    return (link_status ? WIFI_STAT_RET_OK : WIFI_STAT_RET_NOT_OK);
}

#define WIFI_QUALITY_THREAD  -60

int32_t low_wifi_stat_chk_quality(void)
{
    int32_t rev = 0;
    int rssi = 0;
    if (!ls_wifi_started) {
        CLOGI("[low_wifi]low wifi not started,can not check quality\n");
        low_wifi_err_code_set(WIFI_REASON_NOT_START);
        return -1;
    }
    rev = wifi_get_ap_rssi(&rssi);
    if (rev) {
        CLOGI("[low_wifi]stat_chk_quality get ap info failed\n");
        return -1;
    }
    CLOGD("rssi is %d\n",rssi);
    if (rssi >= WIFI_QUALITY_THREAD) {
        return 0;
    }
    return -1;
}

int32_t low_wifi_cur_rssi_get(void)
{
    int32_t rev = 0;
    int rssi = 0;
    if (!ls_wifi_started) {
        CLOGI("[low_wifi]low wifi not started,can not get rssi\n");
        low_wifi_err_code_set(WIFI_REASON_NOT_START);
        return -1;
    }
    rev = wifi_get_ap_rssi(&rssi);
    if (rev) {
        CLOGI("[low_wifi]cur_rssi get failed\n");
        return -1;
    }
    CLOGD("rssi is %d\n",rssi);
    return rssi;
}

int32_t low_wifi_cur_ssid_get(char* ssid_name)
{
    int32_t rev = 0;
    wifi_link_status_t status = {0};
    if (!ls_wifi_started) {
        CLOGI("[low_wifi]low wifi not started,can not get ssid\n");
        low_wifi_err_code_set(WIFI_REASON_NOT_START);
        return -1;
    }
    rev = wifi_get_link_status(&status);
    if (rev) {
        CLOGI("[low_wifi]cur_ssid_get failed\n");
        return -1;
    }
    sprintf(ssid_name, "%s", status.ssid);
    CLOGD("ssid_name is %s\n",ssid_name);
    return 0;
}

int32_t low_wifi_cur_bssid_get(uint8_t mac[])
{
    int32_t rev = 0;
    wifi_link_status_t status = {0};
    if (!ls_wifi_started) {
        CLOGI("[low_wifi]low wifi not started,can not get bssid\n");
        low_wifi_err_code_set(WIFI_REASON_NOT_START);
        return -1;
    }
    rev = wifi_get_link_status(&status);
    if (rev) {
        CLOGI("[low_wifi]cur_bssid_get failed\n");
        return -1;
    }
    memcpy(mac, status.bssid, 6);
    low_wifi_bssid_show(mac);
    return 0;
}

int32_t low_wifi_err_code_get(void)
{
    return low_wifi_err_code;
}

int32_t low_wifi_get_ip(char ip[])
{
    int32_t link_status = 0;
    struct ip_addr_cfg cfg = {0};
    int32_t rev = 0;
    if (!ls_wifi_started) {
        CLOGI("[low_wifi]low wifi not started,can not get ip\n");
        low_wifi_err_code_set(WIFI_REASON_NOT_START);
        return -1;
    }
    link_status = low_wifi_stat_chk_connect();
    if (!link_status) {
        return -1;
    }
    CLOGD("wifi is linking\n");
    rev = ls_get_ip(WIFI_VIF_DEFAULT_IDX, &cfg);
    if (rev) {
        CLOGE(" get ip fail, rev %x \r\n", rev);
        return -1;
    }
    low_wifi_ip = cfg.ipv4.addr;
    sprintf(ip, "%s", inet_ntoa(low_wifi_ip) ?: "");
    return 0;
}

int32_t low_wifi_set_dns_server(char *ip)
{
#if 0
    int32_t rev;
    tcpip_adapter_dns_info_t dns;
    dns.ip.type = ESP_IPADDR_TYPE_V4;
    dns.ip.u_addr.ip4.addr = ipaddr_addr(ip);
    rev = tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, ESP_NETIF_DNS_MAIN, &dns);
    return rev == ESP_OK ? 0 : -1;
#else
    struct in_addr addr;
    if (inet_pton(AF_INET, ip, &addr) != 1) {
        return -1;
    }
    return net_set_dns(addr.s_addr);
#endif
}

int32_t low_wifi_mac_str_to_int(char *mac_str, uint8_t mac[])
{
    int32_t rev, i, m[6];
    rev = sscanf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x",
            &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]);
    if(rev == 6)
    {
        for(i = 0; i < 6; i++)
        {
            mac[i] = m[i];
        }
        return 0;
    }
    return -1;
}

char *low_wifi_str_of_mac(uint8_t mac[])
{
    static char mac_str[20];
    sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1],mac[2], mac[3], mac[4], mac[5]);
    return mac_str;
}

char *low_wifi_str_of_authmode(int32_t authmode)
{
    switch (authmode) {
    case WIFI_SEC_OPEN:
        return "OPEN";
    case WIFI_SEC_WEP:
        return "WEP";
    case WIFI_SEC_WPA_PSK:
        return "WPA_PSK";
    case WIFI_SEC_WPA2_PSK:
        return "WPA2_PSK";
    case WIFI_SEC_WPA_PSK_WPA2_PSK:
        return "WPA_WPA2_PSK";
    case WIFI_SEC_WPA_ENTERPRISE:
        return  "WPA_ENTERPRISE";
    case WIFI_SEC_WPA3_SAE:
        return "WPA3_PSK";
    case WIFI_SEC_WPA2_PSK_WPA3_SAE:
        return "WPA2_WPA3_PSK";
    default:
        return "UNKNOWN";
    }
}

char *low_wifi_str_of_cipher(int32_t cipher)
{
    switch(cipher)
    {
    // case WIFI_CIPHER_TYPE_NONE:   //not support
    //     return "NONE";
    // case WIFI_CIPHER_TYPE_WEP40:
    //     return "WEP40";
    // case WIFI_CIPHER_TYPE_WEP104:
    //     return "WEP104";
    // case WIFI_CIPHER_TYPE_TKIP:
    //     return "TKIP";
    // case WIFI_CIPHER_TYPE_CCMP:
    //     return "CCMP";
    // case WIFI_CIPHER_TYPE_TKIP_CCMP:
    //     return "TKIP_CCMP";
    default:
        return "UNKNOWN";
    }
}


/**
 * @brief 设置 Wi-Fi 速率（十六进制值）
 * @param rate 速率值，支持以下范围：
 *   - 0xff: 自动选择最佳速率
 *   - 802.11b (长前导码): 0x0 (1Mbps) ~ 0x3 (11Mbps)
 *   - 802.11b (短前导码): 0x5 (2Mbps) ~ 0x7 (11Mbps)
 *   - 802.11g: 0x10 (6Mbps) ~ 0x17 (54Mbps)
 *   - MCS0~MCS9 (Long GI / 1.6us): 0x20 ~ 0x29
 *   - MCS0~MCS9 (Short GI / 0.8us): 0x30 ~ 0x39
 *   - MCS0~MCS9 (3.2us GI): 0x40 ~ 0x49
 * @return 成功返回 0，失败返回错误码
 */
int32_t low_wifi_cli_rate_set(int config_rate)
{
    int32_t rev = 0;
    int rate = 0;
    if (!ls_wifi_started) {
        CLOGI("[low_wifi]low wifi not started,can not config rate\n");
        low_wifi_err_code_set(WIFI_REASON_NOT_START);
        return -1;
    }

    rate = config_rate;
    if (rate < 0 || rate == 0x4 || (rate > 0x7 && rate < 0x10) \
            || (rate > 0x17 && rate < 0x20) || (rate > 0x29 && rate < 0x30) \
            || (rate > 0x39 && rate < 0x40) || (rate > 0x49 && rate < 0xff))
    {
        CLOGI(": invalid rate value 0x%x\n", rate);
        return -1;
    }

    rev = wifi_rate_config(rate);

    if (rev) {
        CLOGI(" config rate fail\n");
    } else {
        CLOGI(" config rate success\n");
    }

    return rev;
}

int32_t low_wifi_start()
{
    return low_wifi_on();
}

int32_t low_wifi_stop()
{
    return low_wifi_off();
}


void low_wifi_config_wifi_connect_retry(uint8_t retry)
{
    connect_retry = retry;
}

bool low_wifi_is_started(void)
{
    return ls_wifi_started;
}


int32_t low_wifi_pause(void)
{
    return 0;
}

int32_t low_wifi_resume(void)
{
    return 0;
}