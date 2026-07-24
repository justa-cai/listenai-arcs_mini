#include "stdint.h"
#include "stdio.h"
#include "shell.h"
#include "cmd.h"
#include "stddef.h"
#include "string.h"
#include "sys_network_manager.h"
// #include "listen_wifi.h"
#if CONFIG_WIFI_MANAGER
#include "wifi_manager/wifi_manager.h"
#include "wifi_manager/wifi_manager_storage.h"
#endif

static int wifi_cmd_help(int argc, char **argv);
static int wifi_list(int argc, char **argv);
static int wifi_scan(int argc, char **argv);

#if CONFIG_WIFI_MANAGER
static int wifi_save_ap_storage_only(wifi_mgr_sta_config_t *sta_cfg)
{
    int ret = wifi_mgr_storage_save_ap(sta_cfg);
    wifi_mgr_ops_t *ops;
    wifi_storage_ctx_t storage_ctx = {0};
    void *mutex;

    if (ret == 0) {
        return 0;
    }

    ops = wifi_mgr_ops_get();
    if (ops == NULL || ops->mem_ops == NULL || ops->os_ops == NULL) {
        return ret;
    }

    mutex = ops->os_ops->mutex_create();
    if (mutex == NULL) {
        return ret;
    }

    wifi_storage_ops_t storage_ops = {
        .malloc = ops->mem_ops->malloc,
        .calloc = ops->mem_ops->calloc,
        .free = ops->mem_ops->free,
        .mutex_lock = ops->os_ops->mutex_lock,
        .mutex_unlock = ops->os_ops->mutex_unlock,
    };

    if (wifi_storage_init(&storage_ctx, &storage_ops, mutex) == 0) {
        ret = wifi_storage_save_ap(&storage_ctx, sta_cfg);
        (void)wifi_storage_deinit(&storage_ctx);
    }

    ops->os_ops->mutex_delete(mutex);
    return ret;
}
#endif

static int wifi_connect(int argc, char **argv)
{
    if (argc < 1 || argc > 2) {
        shellPrint(shellGetCurrent(), "Usage: wifi connect ssid [password]\n");
        return -1;
    }
    char *ssid = argv[0];
    char *pwd = (argc >= 2) ? argv[1] : NULL;

    if (ssid == NULL || strlen(ssid) == 0) {
        shellPrint(shellGetCurrent(), "wifi ssid param error\n");
        return -1;
    }
    
    // 检查密码参数（如果提供了密码）
    if (pwd != NULL && strlen(pwd) == 0) {
        shellPrint(shellGetCurrent(), "wifi password param error\n");
        return -1;
    }

#if CONFIG_WIFI_MANAGER
    wifi_mgr_sta_config_t sta_cfg = {0};
    sys_network_status_t network_status;


    snprintf(sta_cfg.ssid, sizeof(sta_cfg.ssid), "%s", ssid);

    // 如果提供了密码，则设置密码
    if (pwd != NULL) {
        snprintf(sta_cfg.pwd, sizeof(sta_cfg.pwd), "%s", pwd);
    }

    if (sys_network_get_status(&network_status) == 0 &&
        network_status.active_bearer == SYS_NETWORK_BEARER_MODEM) {
        shellPrint(shellGetCurrent(), "4G network active, save wifi ap only\n");
        int ret = wifi_save_ap_storage_only(&sta_cfg);
        if (ret != 0) {
            shellPrint(shellGetCurrent(), "wifi save ap error %d", ret);
            return -1;
        }
        return 0;
    }

    int ret = wifi_mgr_sta_connect(&sta_cfg, true);
    if (ret != 0) {
        shellPrint(shellGetCurrent(), "wifi connect error %d", ret);
        return -1;
    }
#else
    // 对于非wifi_manager版本，如果没有密码则传递空字符串
    int ret = ls_wifi_connect(ssid, pwd ? pwd : "");
    if (ret != 0) {
        shellPrint(shellGetCurrent(), "wifi connect error %d", ret);
        return -1;
    }
#endif

    return 0;
}
#if CONFIG_WIFI_MANAGER
static int wifi_disconnect(int argc, char **argv)
{
    return wifi_mgr_sta_disconnect(true);
}

static int wifi_auto_connect(int argc, char **argv)
{
    if (argc < 1) {
        shellPrint(shellGetCurrent(), "invalid param index %d\n", argc);
        return -1;
    }
    uint32_t interval_ms = atoi(argv[0]);

    if (interval_ms <= 0) {
        shellPrint(shellGetCurrent(), "stop auto connect\n");
        wifi_mgr_auto_connect_stop();
        return 0;
    }

    wifi_mgr_autoconn_config_t cnn_cfg = {
        .interval_ms = interval_ms,
    };
    wifi_mgr_auto_connect_start(&cnn_cfg);

    return 0;
}
#endif


static const struct listen_cmd_t g_wifi_cmds[] = {
    {"connect", wifi_connect, "connect wifi, ex: wifi connect ssid [password]"},
#if CONFIG_WIFI_MANAGER
    {"disconnect", wifi_disconnect, "disconnect wifi, ex: wifi disconnect"},
    {"auto", wifi_auto_connect, "auto connect wifi, interval_ms <=0 stop, ex: wifi auto interval_ms"},
    {"scan", wifi_scan, "scan and list available wifi networks"},
    {"list", wifi_list, "show saved wifi networks"},
#endif
    {"help", wifi_cmd_help, "show help"},
};

static int wifi_cmd_help(int argc, char **argv)
{
    int cmd_len = sizeof(g_wifi_cmds) / sizeof(g_wifi_cmds[0]);
    for (int i = 0; i < cmd_len; i++) {
        if (g_wifi_cmds[i].help != NULL) {
            shellPrint(shellGetCurrent(), "%-17s\t:\t%s\n", g_wifi_cmds[i].name, g_wifi_cmds[i].help);
        }
    }

    return 0;
}

#if CONFIG_WIFI_MANAGER
static int wifi_scan(int argc, char **argv)
{
    wifi_mgr_scan_info_t *ap_info = NULL;
    uint32_t size = 32; // Maximum number of APs to scan
    int ret;
    
    // Allocate memory for scan results
    ap_info = malloc(sizeof(wifi_mgr_scan_info_t) * size);
    if (ap_info == NULL) {
        shellPrint(shellGetCurrent(), "Failed to allocate memory for scan results\n");
        return -1;
    }
    
    // Perform WiFi scan (synchronous)
    shellPrint(shellGetCurrent(), "Scanning for WiFi networks...\n");
    ret = wifi_mgr_scan_ap(ap_info, size, false);
    
    if (ret < 0) {
        shellPrint(shellGetCurrent(), "WiFi scan failed with error: %d\n", ret);
        free(ap_info);
        return -1;
    }
    
    // Print scan results
    shellPrint(shellGetCurrent(), "\nFound %d WiFi networks:\n", ret);
    shellPrint(shellGetCurrent(), "%-4s %-32s %-18s %-8s %-8s\n", "No.", "SSID", "BSSID", "Channel", "RSSI");
    shellPrint(shellGetCurrent(), "----------------------------------------------------------------\n");
    
    for (int i = 0; i < ret; i++) {
        shellPrint(shellGetCurrent(), "%-4d %-32s %02x:%02x:%02x:%02x:%02x:%02x %-8d %-8d\n",
                   i + 1,
                   ap_info[i].ssid,
                   ap_info[i].bssid[0], ap_info[i].bssid[1], ap_info[i].bssid[2],
                   ap_info[i].bssid[3], ap_info[i].bssid[4], ap_info[i].bssid[5],
                   ap_info[i].channel,
                   ap_info[i].rssi);
    }
    
    // Free allocated memory
    free(ap_info);
    return 0;
}

static int wifi_list(int argc, char **argv)
{
    wifi_mgr_sta_config_t *matched_list = NULL;

    matched_list = psram_malloc(sizeof(wifi_mgr_sta_config_t) * 64);
    if (matched_list == NULL) {
        return -1;
    }

    int ret;

    // 搜索所有已保存的WiFi网络
    ret = wifi_mgr_storage_search_ap(matched_list, 64, SEARCH_ALL, NULL);

    if (ret < 0) {
        shellPrint(shellGetCurrent(), "Failed to get saved WiFi networks, error: %d\n", ret);
        psram_free(matched_list);
        return -1;
    }
    
    if (ret == 0 || matched_list == NULL) {
        shellPrint(shellGetCurrent(), "No saved WiFi networks found\n");
        psram_free(matched_list);
        return 0;
    }
    
    // 打印已保存的WiFi网络
    shellPrint(shellGetCurrent(), "\nFound %d saved WiFi networks:\n", ret);
    shellPrint(shellGetCurrent(), "%-4s %-32s %-18s %-8s\n", "No.", "SSID", "BSSID", "Channel");
    shellPrint(shellGetCurrent(), "----------------------------------------------------------\n");
    
    for (int i = 0; i < ret; i++) {
        shellPrint(shellGetCurrent(), "%-4d %-32s %02x:%02x:%02x:%02x:%02x:%02x %-8d\n",
                   i + 1,
                   matched_list[i].ssid,
                   matched_list[i].bssid[0], matched_list[i].bssid[1], matched_list[i].bssid[2],
                   matched_list[i].bssid[3], matched_list[i].bssid[4], matched_list[i].bssid[5],
                   matched_list[i].channel);
    }

    psram_free(matched_list);

    return 0;
}
#endif

static int wifi_cmd_handler(int argc, char **argv)
{
    if (argc == 1) {
        wifi_cmd_help(argc, argv);
        return 0;
    }

    int i;

    for (i = 0; i < sizeof(g_wifi_cmds) / sizeof(g_wifi_cmds[0]); i++) {
        if (strcmp(g_wifi_cmds[i].name, argv[1]) == 0) {
            return g_wifi_cmds[i].exec(argc - 2, argv + 2);
        }
    }

    wifi_cmd_help(argc, argv);

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, wifi,
                 wifi_cmd_handler, wifi cmd group);
