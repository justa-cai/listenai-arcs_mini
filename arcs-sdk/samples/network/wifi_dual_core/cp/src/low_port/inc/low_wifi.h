
#ifndef __low_wifi_h__
#define __low_wifi_h__

#ifdef __cplusplus
extern "C" {
#endif


#include "stdint.h"
#include "stdbool.h"

/*
 * wifi或者网络模块的初始化。全局初始化，仅在开机后运行一次的逻辑。需要在系统初始化的合适时机运行。0表示成功，非0失败
 */

int32_t low_wifi_hw_init(void);

/// @brief Wifi开启（硬件射频启用，但无需重新调用硬件初始化）。0表示成功，非0失败
/// @param  
/// @return 
int32_t low_wifi_on(void);

/// @brief Wifi开启（硬件射频启用，但无需重新调用硬件初始化）。0表示成功，非0失败
/// @param  
/// @return 
#define low_wifi_init low_wifi_on

/// @brief 下次wifi连接使用快连
void low_wifi_use_fast_connect_next_time();

/*
 * wifi或者网络模块关闭。0表示成功，非0失败
 */
int32_t low_wifi_off(void);
#define low_wifi_term low_wifi_off

//读取最近一次连接成功的wifi，如果有的话,可能为NULL，不用释放
char *read_last_wifi(void);

/*
* 工厂写wifi的mac地址
*/
int32_t low_wifi_factory_set_mac_addr(char *mac);


/*
 * 获取设备自己的mac地址 
 * mac:
 *  mac[6],mac放到这里
 * retval:
 *  0:ok
 * -1:failed
 */
int32_t low_wifi_get_mac_addr(uint8_t mac[]);

#define LOW_WIFI_SSID_LEN           33 //ssid要支持到32位,这个什要配成33
#define LOW_WIFI_BSSID_LEN          20
#define LOW_WIFI_KEY_LEN            64
#define LOW_WIFI_FLAG_LEN           64
/*
 * 扫描的结果，后续有可能会在这个结构体中增加新的属性
 */
typedef struct scan_result_struct
{
    char ssid[LOW_WIFI_SSID_LEN];   //扫描到的ssid
    char bssid[LOW_WIFI_BSSID_LEN]; //bssid 平台不支持的话,可以不实现
    //char flag[LOW_WIFI_FLAG_LEN];   //该网络的flag,其中主要是包含该网络的加密信息
    int32_t authmode;
    int32_t pairwise_cipher;
    int32_t group_cipher;
    int8_t rssi;                    //该ssid的信号强度
    int8_t open_flag;               //是否为开放的
    char flag[LOW_WIFI_FLAG_LEN];   //该网络的flag,其中主要是包含该网络的加密信息
}scan_result_t;

/*
 * 扫描到wifi信号时的回调
 * result:
 *  扫描到的结果
 * userdata:
 *  启动扫描时传过来的指针参数
 * retval:用来控制扫描
 *  0:继续扫描
 *  1:扫描可以停下来了
 */
typedef int32_t(*wifi_scan_cb)(scan_result_t* result, void* userdata);

/*
 * 连上wifi，获取到ip之后，调用该函数看能否访问外网
 * retval:
 *  0:可以访问外网
 * -1:访问不了外网
 */
typedef int32_t(*probe_fun)(void);

/*
 * 启动扫描环境中的网络，每扫描到一个网络调用一次回调，并且根据回调值来确定是继续扫描还是中止
 * 这个函数要求实现成阻塞的
 * cb:
 *  扫描到网络时的回调函数
 * userdata:
 *  调用回调的时候把这个参数传给回调函数
 * retval:
 *  0:扫描成功
 * <0 :失败
 */
int32_t low_wifi_scan(wifi_scan_cb cb, void* userdata);

/*
 * 网络配置结构体
 */
typedef struct low_wifi_cfg_struct
{
    char ssid[LOW_WIFI_SSID_LEN];   //网络的ssid或者bssid
    char key[LOW_WIFI_KEY_LEN];     //密码
    char bssid[LOW_WIFI_BSSID_LEN];
    char flag[LOW_WIFI_FLAG_LEN];
    //char flag[LOW_WIFI_FLAG_LEN];
    int32_t authmode;
    int32_t pairwise_cipher;
    int32_t group_cipher;
    probe_fun probe;    //连上wifi之后调用该函数来检查网络是否可用
}low_wifi_cfg_t;

void low_wifi_cfg_reset(low_wifi_cfg_t *cfg);

// #define LOW_CONN_ERR_NOOUT  1  //无法访问外网
// #define LOW_CONN_ERR_NONE   0
// #define LOW_CONN_ERR_KEY   -1
// #define LOW_CONN_ERR_IP    -2
// #define LOW_CONN_ERR_TOAP  -3

#define LOW_CONN_ERR_NONE           0
#define LOW_CONN_ERR_PROBE          1
#define LOW_CONN_ERR_PROBE_AUTHOR   2
#define LOW_CONN_ERR_NOOUT          3
#define LOW_CONN_ERR_KEY           -1
#define LOW_CONN_ERR_IP            -2
#define LOW_CONN_ERR_TOAP          -3

/*
 * 开始连网
 * 这个函数要求实现成阻塞的，直到连网成功或者失败
 * cfg:
 *  需要连的网络
 * retval:
 *  0:成功
 *  1:已连上网络，但访问不了外网(ping不通)
 * -1:失败,密码错误
 * -2:获取IP出错
 * -3:连wifi超时,密码错误以外的原因导致的
 */
int32_t low_wifi_connect(low_wifi_cfg_t* cfg);

/*
 * WIFI 快速连接wifi
 * 这个函数要求实现成阻塞的，直到连网成功或者失败
 * cfg:
 *  需要连的网络
 * retval:
 *  0:成功
 *  1:已连上网络，但访问不了外网(ping不通)
 * -1:失败,密码错误
 * -2:获取IP出错
 * -3:连wifi超时,密码错误以外的原因导致的
 */
int32_t low_wifi_fast_connect(low_wifi_cfg_t* cfg);

/*
 * 如果当前网络已经连接，就断开网络
 * 如果当前正在连网，就中止连接过程
 * 这个函数要实现成阻塞的，直到网络断开才退出
 * retval:
 * 0:成功
 *-1:失败,断网操作应该是不会失败才对
 */
int32_t low_wifi_disconnect(void);

/*
 * 检查网络连接状态,拿到ip才认为网络是连着的
 * retval:
 * 0:网络正常
 *-1:网络断掉了
 */
int32_t low_wifi_stat_chk_connect(void);

/*
 * 检查wifi信号质量好不好，不同的硬件平台根据自己的情况来判断
 * retval:
 * 0:网络质量好
 *-1:网络质量不好
 */
int32_t low_wifi_stat_chk_quality(void);

/*
 * 获取当前所连网络的信号强度
 * retval:
 *   rssi:我们认为不会出现rssi为-1的情况
 *  -1:failed
 */
int32_t low_wifi_cur_rssi_get(void);

/*
 * 获取当前连接的网络ssid,只有当前网络处于连接状态才能返回ssid
 * ssid_name:当前所连网络的ssid填到这里
 * retval:
 * 0:成功
 *-1:失败,当前网络断开
 */
int32_t low_wifi_cur_ssid_get(char* ssid_name);

/*
 * 获取当前所连接的路由器（AP)的mac地址
 * mac[]:
 *  mac[6],mac放到这里
 * retval:
 *  0:ok
 * -1:failed
 */
int32_t low_wifi_cur_bssid_get(uint8_t mac[]);

#define WIFI_REASON_NONE				0
#define WIFI_REASON_CONNECT_TIMEOUT		104
#define WIFI_REASON_GET_IP_TIMEOUT		105
#define WIFI_REASON_NOT_START		    106

/*
 * wifi模块错误码的获取
 */
int32_t low_wifi_err_code_get(void);

/*
 * 获取设备端IP
 */
int32_t low_wifi_get_ip(char ip[]);

/*
 * 设置设备使用的dns服务器ip
 */
int32_t low_wifi_set_dns_server(char *ip);

/*
 * 设置wifi速率
 */

int32_t low_wifi_cli_rate_set(int config_rate);

/*
 * 启动wifi
 */
int32_t low_wifi_start();

/*
 * 停止wifi
 */
int32_t low_wifi_stop();

/*
 * 配置wifi连接重试次数
 */
void low_wifi_config_wifi_connect_retry(uint8_t retry);

/*
 * 设置下次连接wifi使用快速重连方式。除密码错误外的其他失败原因会使用正常方式重试。
 */
void low_wifi_use_fast_connect_next_time();
/*
 * 获取wifi是否启动
 */
bool low_wifi_is_started(void);

/***以下是非必须函数*******/

char *low_wifi_str_of_mac(uint8_t mac[]);
char *low_wifi_str_of_authmode(int32_t authmode);
char *low_wifi_str_of_cipher(int32_t cipher);

/*
 * 休眠前执行该函数,由硬件组合确定是否需要实现
 * 1.r853+aic8800
 */
int32_t low_wifi_pause(void);

/*
 * 唤醒后执行该函数，由硬件组合确定是否需要实现
 * 1.r853+aic8800
 */
int32_t low_wifi_resume(void);
#ifdef __cplusplus
}
#endif
#endif

