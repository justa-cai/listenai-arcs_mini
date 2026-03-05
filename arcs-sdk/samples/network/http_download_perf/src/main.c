#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stdbool.h"
#include "assert.h"
#include "FreeRTOS.h"
#include "task.h"

#define TAG "http-test"
#include "lisa_log.h"
#include "lisa_kv.h"
#include "mac_manager.h"
#include "mac_manager_ops.h"
#include "wifi_manager/wifi_manager.h"
#include "user_fs.h"
#include "lisa_wifi.h"
#include "ls_wifi_type.h"
#include "net_al.h"
#include "net_def.h"
#include "HTTPCUsr_api.h"

// ============================================================
// 重要：使用前请修改以下配置！
// ============================================================
#define SERVER_HOST      "your_server_ip"// 修改为测试服务器的 IP 地址，例如："192.168.1.100"
#define SERVER_PORT 8080                 // 修改为测试服务器的端口号，默认 8080

#define TARGET_WIFI_SSID   "your_wifi_ssid"// 修改为目标 WiFi 的 SSID
#define TARGET_WIFI_PWD     "your_wifi_pwd"// 修改为目标 WiFi 的密码

// 编译时检查：确保用户已修改默认配置
#ifndef SERVER_HOST
#error "请修改 SERVER_HOST 为实际的测试服务器 IP 地址（例如：192.168.1.100）"
#endif

#ifndef TARGET_WIFI_SSID
#error "请修改 TARGET_WIFI_SSID 为实际的 WiFi SSID"
#endif

#ifndef TARGET_WIFI_PWD
#error "请修改 TARGET_WIFI_PWD 为实际的 WiFi 密码"
#endif

#define WIFI_MGR_AUTO_CONNECT_INTERVAL_MS 2000

// 测试用的缓冲区大小(字节)
#define BUF_SIZE_2KB   (2 * 1024)   // 2KB
#define BUF_SIZE_4KB   (4 * 1024)   // 4KB
#define BUF_SIZE_8KB   (8 * 1024)   // 8KB
#define BUF_SIZE_16KB  (16 * 1024)  // 16KB
#define BUF_SIZE_32KB  (32 * 1024)  // 32KB
#define BUF_SIZE_64KB  (64 * 1024)  // 64KB
#define BUF_SIZE_128KB (128 * 1024) // 128KB (最大)

// 下载速度统计结构
typedef struct {
    uint32_t total_bytes;     // 总下载字节数
    uint32_t start_time;      // 开始时间（毫秒）
    uint32_t last_print_time; // 上次打印时间
    uint32_t last_bytes;      // 上次打印时的字节数
    uint32_t chunk_count;     // 数据块计数
    uint32_t min_chunk_size;  // 最小数据块大小
    uint32_t max_chunk_size;  // 最大数据块大小
} download_stats_t;

// 测试结果记录
typedef struct {
    uint32_t file_size_mb;  // 文件大小(MB)
    uint32_t buffer_size;   // 缓冲区大小(字节)
    float avg_speed_kbps;   // 平均速度(KB/s)
    uint32_t total_time_ms; // 总时间(毫秒)
    uint32_t chunk_count;   // 数据块数量
} test_result_t;

#define MAX_TEST_RESULTS 20
#define WIFI_MGR_SEARCH_AP_BUFFER_SIZE 10

static download_stats_t g_download_stats = {0};
static test_result_t g_test_results[MAX_TEST_RESULTS] = {0};
static int g_test_count = 0;
static mac_manager_t *m_mac_manager;
static wifi_mgr_sta_config_t list[WIFI_MGR_SEARCH_AP_BUFFER_SIZE] = { 0 };

static int8_t custom_get_wifi_mac(uint8_t mac_addr[6])
{
    int8_t ret = 0;

    if (!mac_addr)
        return -1;

    ret = mac_manager_get(m_mac_manager, mac_addr, 6);
    LOGI("custom_get_wifi_mac: %d, %02X:%02X:%02X:%02X:%02X:%02X", ret, mac_addr[0], mac_addr[1], mac_addr[2],
         mac_addr[3], mac_addr[4], mac_addr[5]);
    return ret;
}

static volatile bool g_wifi_connected = false;
static volatile bool g_get_ip_success = false;

static void dhcp_status_callback(int vif_idx, bool success, uint32_t ip_addr, uint32_t netmask, uint32_t gateway, void *arg)
{
    if (success) {
        LOGI("DHCP Success on VIF-%d: IP=%d.%d.%d.%d, Mask=%d.%d.%d.%d, GW=%d.%d.%d.%d",
             vif_idx,
             ip_addr & 0xff, (ip_addr >> 8) & 0xff, (ip_addr >> 16) & 0xff, (ip_addr >> 24) & 0xff,
             netmask & 0xff, (netmask >> 8) & 0xff, (netmask >> 16) & 0xff, (netmask >> 24) & 0xff,
             gateway & 0xff, (gateway >> 8) & 0xff, (gateway >> 16) & 0xff, (gateway >> 24) & 0xff);
        g_get_ip_success = true;
    } else {
        LOGI("DHCP Failed on VIF-%d", vif_idx);
    }
}

static void wifi_mgr_connection_status_cb(wifi_mgr_connection_info_t *connection_info, void *arg)
{
    net_if_t *net_if;

    LOGI("WiFi connection status: %d", connection_info->status);

    switch (connection_info->status) {
    case WIFI_MGR_STA_CONNECTED:
        LOGI("WiFi connected to AP");
        g_wifi_connected = true;
        net_if = net_if_get(WIFI_VIF_STA_IDX);
        net_if_up(net_if);
        if (!net_if->static_ip) {
            ls_dhcpc_start(WIFI_VIF_STA_IDX);
        }
        break;

    case WIFI_MGR_STA_DISCONNECTED:
        LOGI("WiFi disconnected from AP");
        g_wifi_connected = false;
        ls_dhcpc_stop(WIFI_VIF_STA_IDX);
        net_if_down(net_if_get(WIFI_VIF_STA_IDX));
        break;

    default:
        break;
    }
}

static void user_mac_manager_init(void)
{
    mac_manager_config_t config = {
        .random_mac_if_mac_invalid = false,
    };

    m_mac_manager = mac_manager_init(mac_manager_ops_get()->mem_ops, mac_manager_ops_get()->content_ops, &config);
    if (m_mac_manager == NULL) {
        LOGI("mac_manager_init failed\n");
        assert(0);
    }
}

static void user_wifi_manager_init(void)
{
    wifi_mgr_sta_config_t cfg = {
        .ssid = TARGET_WIFI_SSID,
        .pwd = TARGET_WIFI_PWD,
    };

    wifi_mgr_autoconn_config_t autoconn_cfg = {
        .interval_ms = WIFI_MGR_AUTO_CONNECT_INTERVAL_MS,
    };

    wifi_mgr_init(wifi_mgr_ops_get());
    wifi_mgr_sta_enable();

    // 注册 WiFi 连接状态回调
    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_status_cb, NULL);

    /* 搜索并删除所有已保存的 AP */
    int count = wifi_mgr_storage_search_ap(list, WIFI_MGR_SEARCH_AP_BUFFER_SIZE, SEARCH_ALL, NULL);
    for (int i = 0; i < count; i++) {
        LOGI("Deleting saved AP: ssid=%s", list[i].ssid);
        wifi_mgr_storage_delete_ap(&list[i]);
    }

    // 保存 AP 配置并启动自动连接
    wifi_mgr_storage_save_ap(&cfg);
    wifi_mgr_auto_connect_start(&autoconn_cfg);

    LOGI("WiFi auto-connect started");
}

static int wait_for_wifi_connection(void)
{
    int timeout = 30; // 30 秒超时

    LOGI("Waiting for WiFi connection and IP address...");

    // 等待 WiFi 连接和 IP 获取
    while (timeout > 0) {
        if (g_wifi_connected && g_get_ip_success) {
            LOGI("WiFi connected and IP obtained, starting HTTP tests...");
            vTaskDelay(pdMS_TO_TICKS(500)); // 短暂延迟确保网络稳定
            return 0;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
        timeout--;

        if (timeout % 5 == 0) {
            LOGI("Waiting... (WiFi:%d, IP:%d, timeout:%d)", g_wifi_connected, g_get_ip_success, timeout);
        }
    }

    LOGI("WiFi connection or IP acquisition timeout (WiFi:%d, IP:%d)", g_wifi_connected, g_get_ip_success);
    return -1;
}

// 格式化字节数为人类可读的格式
static void format_bytes(uint32_t bytes, char *buf, size_t buf_size)
{
    float value;
    const char *unit;

    if (bytes >= 1024 * 1024) {
        value = (float)bytes / (1024.0f * 1024.0f);
        unit = "MB";
    } else if (bytes >= 1024) {
        value = (float)bytes / 1024.0f;
        unit = "KB";
    } else {
        value = (float)bytes;
        unit = "B";
    }

    snprintf(buf, buf_size, "%.2f %s", value, unit);
}

// 打印下载完成统计信息并保存结果
static void print_download_stats(uint32_t file_size_mb, uint32_t buffer_size)
{
    if (g_download_stats.start_time == 0) {
        return;
    }

    uint32_t end_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t total_time = end_time - g_download_stats.start_time;

    if (total_time > 0) {
        float avg_speed_kbps = (float)g_download_stats.total_bytes * 1000.0f / total_time / 1024.0f;
        uint32_t avg_chunk_size =
            g_download_stats.chunk_count > 0 ? g_download_stats.total_bytes / g_download_stats.chunk_count : 0;
        char size_str[32], min_str[32], max_str[32], avg_str[32];

        format_bytes(g_download_stats.total_bytes, size_str, sizeof(size_str));
        format_bytes(g_download_stats.min_chunk_size, min_str, sizeof(min_str));
        format_bytes(g_download_stats.max_chunk_size, max_str, sizeof(max_str));
        format_bytes(avg_chunk_size, avg_str, sizeof(avg_str));

        LOGI("========== 下载完成 ==========");
        LOGI("总大小: %s (%u bytes)", size_str, g_download_stats.total_bytes);
        LOGI("总时间: %u ms (%.2f 秒)", total_time, (float)total_time / 1000.0f);
        LOGI("平均速度: %.2f KB/s (%.2f Mbps)", avg_speed_kbps, avg_speed_kbps * 8 / 1024);
        LOGI("--- 数据块统计 ---");
        LOGI("总数据块数: %u", g_download_stats.chunk_count);
        LOGI("数据块大小: 最小=%s, 最大=%s, 平均=%s", min_str, max_str, avg_str);
        LOGI("================================");

        // 保存测试结果
        if (g_test_count < MAX_TEST_RESULTS) {
            g_test_results[g_test_count].file_size_mb = file_size_mb;
            g_test_results[g_test_count].buffer_size = buffer_size;
            g_test_results[g_test_count].avg_speed_kbps = avg_speed_kbps;
            g_test_results[g_test_count].total_time_ms = total_time;
            g_test_results[g_test_count].chunk_count = g_download_stats.chunk_count;
            g_test_count++;
        }
    }

    // 重置统计数据
    memset(&g_download_stats, 0, sizeof(download_stats_t));
}

// 打印测试结果汇总报告
static void print_summary_report(void)
{
    if (g_test_count == 0) {
        LOGI("没有测试结果");
        return;
    }

    LOGI("\n");
    LOGI("╔════════════════════════════════════════════════════════════════╗");
    LOGI("║          HTTP 下载性能测试汇总报告                            ║");
    LOGI("╚════════════════════════════════════════════════════════════════╝");
    LOGI("");
    LOGI("序号 | 文件大小 | 缓冲区   | 平均速度      | 总时间    | 数据块数");
    LOGI("-----|----------|----------|---------------|-----------|----------");

    for (int i = 0; i < g_test_count; i++) {
        test_result_t *r = &g_test_results[i];
        char buf_str[16];

        // 格式化缓冲区大小
        if (r->buffer_size >= 1024) {
            snprintf(buf_str, sizeof(buf_str), "%uKB", r->buffer_size / 1024);
        } else {
            snprintf(buf_str, sizeof(buf_str), "%uB", r->buffer_size);
        }

        LOGI(" %2d  | %4uMB   | %-8s | %7.2f KB/s | %7.2fs | %u", i + 1, r->file_size_mb, buf_str, r->avg_speed_kbps,
             (float)r->total_time_ms / 1000.0f, r->chunk_count);
    }

    LOGI("");

    // 分析不同文件大小下的最佳缓冲区
    LOGI("═══════════════════════════════════════════════════════════════");
    LOGI("                       性能分析");
    LOGI("═══════════════════════════════════════════════════════════════");

    // 找出每个文件大小的最佳缓冲区
    uint32_t file_sizes[] = {10, 50, 100};
    for (int f = 0; f < 3; f++) {
        uint32_t target_size = file_sizes[f];
        float max_speed = 0;
        uint32_t best_buffer = 0;

        for (int i = 0; i < g_test_count; i++) {
            if (g_test_results[i].file_size_mb == target_size) {
                if (g_test_results[i].avg_speed_kbps > max_speed) {
                    max_speed = g_test_results[i].avg_speed_kbps;
                    best_buffer = g_test_results[i].buffer_size;
                }
            }
        }

        if (max_speed > 0) {
            char buf_str[16];
            if (best_buffer >= 1024) {
                snprintf(buf_str, sizeof(buf_str), "%uKB", best_buffer / 1024);
            } else {
                snprintf(buf_str, sizeof(buf_str), "%uB", best_buffer);
            }
            LOGI("• %uMB 文件: 最佳缓冲区 = %s, 速度 = %.2f KB/s (%.2f Mbps)", target_size, buf_str, max_speed,
                 max_speed * 8 / 1024);
        }
    }

    LOGI("");
    LOGI("═══════════════════════════════════════════════════════════════");
}

// 更新下载统计信息
static void update_download_stats(uint32_t chunk_size)
{
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // 首次接收数据，初始化统计
    if (g_download_stats.start_time == 0) {
        g_download_stats.start_time = current_time;
        g_download_stats.last_print_time = current_time;
        g_download_stats.total_bytes = 0;
        g_download_stats.last_bytes = 0;
        g_download_stats.chunk_count = 0;
        g_download_stats.min_chunk_size = 0xFFFFFFFF;
        g_download_stats.max_chunk_size = 0;
        LOGI("========== 开始下载 ==========");
        LOGI("注意: 数据以小块方式接收，不会占用大量内存");
    }

    // 统计数据块信息
    g_download_stats.chunk_count++;
    if (chunk_size < g_download_stats.min_chunk_size) {
        g_download_stats.min_chunk_size = chunk_size;
    }
    if (chunk_size > g_download_stats.max_chunk_size) {
        g_download_stats.max_chunk_size = chunk_size;
    }

    // 累加接收的字节数
    g_download_stats.total_bytes += chunk_size;

    // 每隔 1 秒打印一次速度
    if (current_time - g_download_stats.last_print_time >= 1000) {
        uint32_t interval = current_time - g_download_stats.last_print_time;
        uint32_t bytes_in_interval = g_download_stats.total_bytes - g_download_stats.last_bytes;
        float speed_kbps = (float)bytes_in_interval * 1000.0f / interval / 1024.0f;
        char size_str[32];

        format_bytes(g_download_stats.total_bytes, size_str, sizeof(size_str));
        LOGI("下载中: %s (%.2f KB/s) | 已接收 %u 个数据块", size_str, speed_kbps, g_download_stats.chunk_count);

        g_download_stats.last_print_time = current_time;
        g_download_stats.last_bytes = g_download_stats.total_bytes;
    }
}

// 执行 HTTP GET 请求
static int http_get_request(const char *uri, const char *test_name, uint32_t file_size_mb, uint32_t buffer_size)
{
    int ret;
    HTTPParameters http_params;
    CHAR *response_buf = NULL;
    INT32 recv_size = 0;
    char buf_size_str[32];

    format_bytes(buffer_size, buf_size_str, sizeof(buf_size_str));

    LOGI("\n========================================");
    LOGI("%s", test_name);
    LOGI("缓冲区大小: %s (%u bytes)", buf_size_str, buffer_size);
    LOGI("========================================");

    // 重置统计数据
    memset(&g_download_stats, 0, sizeof(download_stats_t));

    // 分配响应缓冲区
    response_buf = (CHAR *)malloc(buffer_size);
    if (response_buf == NULL) {
        LOGE("Failed to allocate response buffer");
        return -1;
    }

    // 初始化 HTTP 参数
    memset(&http_params, 0, sizeof(HTTPParameters));
    snprintf(http_params.Uri, HTTP_CLIENT_MAX_URL_LENGTH, "http://%s:%d%s", SERVER_HOST, SERVER_PORT, uri);
    http_params.HttpVerb = VerbGet;
    http_params.Verbose = FALSE;
    http_params.nTimeout = 120; // 120 秒超时

    LOGI("URL: %s", http_params.Uri);

    // 打开 HTTP 连接
    ret = HTTPC_open(&http_params);
    if (ret != 0) {
        LOGE("HTTPC_open failed: %d", ret);
        free(response_buf);
        return -1;
    }

    // 发送 HTTP 请求
    ret = HTTPC_request(&http_params, NULL);
    if (ret != 0) {
        LOGE("HTTPC_request failed: %d", ret);
        HTTPC_close(&http_params);
        free(response_buf);
        return -1;
    }

    // 读取响应数据（流式读取）
    while (1) {
        ret = HTTPC_read(&http_params, response_buf, buffer_size, &recv_size);
        if (ret != 0 || recv_size <= 0) {
            // 读取完成或出错
            break;
        }

        // 更新下载统计
        update_download_stats(recv_size);
    }

    // 关闭 HTTP 连接
    HTTPC_close(&http_params);
    free(response_buf);

    // 打印统计信息
    print_download_stats(file_size_mb, buffer_size);

    // 等待一段时间再进行下一个请求
    vTaskDelay(pdMS_TO_TICKS(2000));

    return 0;
}

int main(int argc, char **argv)
{
    int ret;

    LOGI("HTTP Performance Test Starting...");

    // 初始化 MAC 管理器
    user_mac_manager_init();

    // 注册 DHCP 状态回调
    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    // 初始化 LISA WiFi
    lisa_wifi_ops_t ops = {
        .custom_mac = custom_get_wifi_mac,
    };
    lisa_wifi_init(&ops);

    // 初始化文件系统和 KV 存储
    user_fs_init();
    lisa_kv_init();

    // 初始化 WiFi 管理器并启动自动连接
    user_wifi_manager_init();

    // 等待 WiFi 连接成功
    ret = wait_for_wifi_connection();
    if (ret != 0) {
        LOGI("Failed to connect to WiFi");
        return -1;
    }

    LOGI("\n========================================");
    LOGI("HTTP 下载性能测试");
    LOGI("测试不同缓冲区大小对下载速度的影响");
    LOGI("========================================");

    // 测试方案 1: 使用 10MB 文件测试不同缓冲区大小
    LOGI("\n***** 第一组: 10MB 文件，测试不同缓冲区 *****");
    http_get_request("/random?size=10", "10MB - 2KB 缓冲区", 10, BUF_SIZE_2KB);
    http_get_request("/random?size=10", "10MB - 8KB 缓冲区", 10, BUF_SIZE_8KB);
    http_get_request("/random?size=10", "10MB - 16KB 缓冲区", 10, BUF_SIZE_16KB);
    http_get_request("/random?size=10", "10MB - 32KB 缓冲区", 10, BUF_SIZE_32KB);
    http_get_request("/random?size=10", "10MB - 64KB 缓冲区", 10, BUF_SIZE_64KB);
    http_get_request("/random?size=10", "10MB - 128KB 缓冲区", 10, BUF_SIZE_128KB);

    // 测试方案 2: 使用 50MB 文件测试关键缓冲区大小
    LOGI("\n***** 第二组: 50MB 文件，测试关键缓冲区 *****");
    http_get_request("/random?size=50", "50MB - 2KB 缓冲区", 50, BUF_SIZE_2KB);
    http_get_request("/random?size=50", "50MB - 16KB 缓冲区", 50, BUF_SIZE_16KB);
    http_get_request("/random?size=50", "50MB - 64KB 缓冲区", 50, BUF_SIZE_64KB);
    http_get_request("/random?size=50", "50MB - 128KB 缓冲区", 50, BUF_SIZE_128KB);

    // 测试方案 3: 使用 100MB 文件测试最优缓冲区
    LOGI("\n***** 第三组: 100MB 文件，测试最优缓冲区 *****");
    http_get_request("/random?size=100", "100MB - 16KB 缓冲区", 100, BUF_SIZE_16KB);
    http_get_request("/random?size=100", "100MB - 64KB 缓冲区", 100, BUF_SIZE_64KB);
    http_get_request("/random?size=100", "100MB - 128KB 缓冲区", 100, BUF_SIZE_128KB);

    // 打印汇总报告
    print_summary_report();

    LOGI("\n========================================");
    LOGI("所有测试完成");
    LOGI("========================================");

    return 0;
}
