#define TAG "main"

#include <stdio.h>
#include "lisa_kv.h"
#include "lisa_log.h"
#include "wifi_manager/wifi_manager.h"
#include "lisa_typedef.h"
#include "lisa_thread.h"
#include "pa_manager.h"
#include "FreeRTOSConfig.h"
#include "app_player.h"
#include "evs_utils.h"
#include "sysheap.h"
#include "listen_system.h"
#include "listen_mic_gain.h"
#include "app_tone.h"
#include "tone.h"
#include "listen_flash.h"
#include "app_client.h"
#include "listen_wifi.h"
#include "assistant_controller.h"
#include "assistant_view.h"
#include "ic_message.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "project_version.h"
#include "config_parser.h"
#include "Driver_WDT.h"
#include "chip.h"
#include "nvs.h"
#include "power/power_manager.h"
#include "lisa_display.h"
#include "lisa_aiui.h"
#include "bt_app_if.h"

#if (CONFIG_FLEXIBLE_BUTTON)
#include "lisa_btn.h"

#endif
#include "lisa_kv.h"

// #include "video_camera.h"

#include "IOMuxManager.h"
#include "Driver_GPIO.h"

#include "lsfs.h"
#include "listen_volume.h"
#include "led.h"
#include "assistant_controller.h"
#include "video_camera.h"

// 是否使用Litedac
// 需要配合lisaplayer的arcs_track.c中的配置项同步修改
#define ARCS_DAC_USE_LITE_DAC (1)

/**
 * @brief Factory reset function - to be implemented by user
 */
static void factory_reset(void)
{
    printf("\n=== Starting Factory Reset ===\n");
    // Delete user credentials
    printf("Deleting user credentials...\n");
    int ret1 = lisa_kv_del("user.pid");
    int ret2 = lisa_kv_del("user.sid");
    int ret5 = lisa_kv_del("user.token");
    int ret6 = lisa_kv_del("user.mic_gain");
    printf("  - user_pid delete: %s\n", ret1 == 0 ? "Success" : "Failed");
    printf("  - user_sid delete: %s\n", ret2 == 0 ? "Success" : "Failed");
    printf("  - user_token delete: %s\n", ret5 == 0 ? "Success" : "Failed");
    printf("  - user_mic_gain delete: %s\n", ret6 == 0 ? "Success" : "Failed");

    // Clear WiFi configurations
    printf("Clearing WiFi configurations...\n");
    int ret3 = lisa_kv_del("wifi-info-count");
    int ret4 = lisa_kv_del("wifi-list");
    printf("  - wifi-info-count delete: %s\n", ret3 == 0 ? "Success" : "Failed");
    printf("  - wifi-list delete: %s\n", ret4 == 0 ? "Success" : "Failed");

    // Add any additional reset operations here
    listen_set_volume(70);
    printf("=== Factory Reset Completed ===\n\n");
}

static void network_reset(void)
{
    printf("\n=== Starting Network Reset ===\n");

    // Clear WiFi configurations
    printf("Clearing WiFi configurations...\n");
    int ret3 = lisa_kv_del("wifi-info-count");
    int ret4 = lisa_kv_del("wifi-list");
    printf("  - wifi-info-count delete: %s\n", ret3 == 0 ? "Success" : "Failed");
    printf("  - wifi-list delete: %s\n", ret4 == 0 ? "Success" : "Failed");

    printf("=== Network Reset Completed ===\n\n");
}

#define APP_TASK_STACK (4096 * 2)
#define APP_TASK_PRIO  LISA_OS_PRIORITY_NORMAL

extern void listen_vol_adjust(int vol);

static int __handle_wifi_stack_init_done(void *arg)
{
    LISA_LOGI(TAG, "Wifi Stack Init Done");

    // // Flash init
    // listen_flash_init();
    // LISA_LOGI(TAG, "flash init end");

    // // EasyFlash init
    // easyflash_init();
    // LISA_LOGI(TAG, "easyflash init end");

    // #if !defined(ARCS_DAC_USE_LITE_DAC)
    //     // PA init
    //     pa_manager_pre_init();
    //     pa_manager_init(PA_MGR_NONE);
    //     LISA_LOGI(TAG, "PA init end");

    //     // Speaker init
    //     listen_spk_init();
    //     LISA_LOGI(TAG, "speaker init end");
    // #endif

    //     // System init
    //     ls_sys_init(TIMEZONE_SHANGHAI);
    //     LISA_LOGI(TAG, "system init end");

    //     // Tone init
    //     app_tone_default_init();
    //     LISA_LOGI(TAG, "tone init end");

    //     // LisaPlayer init
    //     app_player_init();
    //     LISA_LOGI(TAG, "app_player init end");

    //     // Welcome
    //     extern void app_main(void);
    //     app_main();

    //     extern int ui_start(void);
    //     ui_start();

    //     LISA_LOGI(TAG, "Application Ready!!!");

    return 0;
}

static void shutdown(void)
{
    LISA_LOGI(TAG, "shutdown");

    pa_manager_onoff(0);

#ifdef CONFIG_LISA_KV_POWEROFF_SAVE
    lisa_kv_poweroff_save();
#endif

    app_led_off();
    lisa_display_blanking_on(lisa_display_get());
    lisa_display_set_brightness(lisa_display_get(), 0);

    vTaskDelay(100);
}

static void _main_wifi_stack_init_done_cb(void)
{
    evs_handler_post_runnable(__handle_wifi_stack_init_done, NULL);
}

void enter_ble_config(void)
{
    assist_controller_trigger_event(CONTROLLER_EVENT_OPT_ENTER_BLE_CONFIG, NULL, 0);
    app_ble_adv_start(0, BLE_ADV_GEN);
}

#if (CONFIG_FLEXIBLE_BUTTON)
static void button_callback_handle(lisa_btn_event_t event, const lisa_btn_info_t *info)
{
    LISA_LOGI(TAG, "Button %d event: %d", info->id, event);

    /* 只处理Power按键 */
    if (info->id != LISA_BTN_ID_POWER) {
        return;
    }

    switch (event) {
    case LISA_BTN_PRESS_CLICK:
        /* 单击: 唤醒功能 */
        if (get_audio_listen_status()) {
            /* 设备处于仅聆听的时候，单击回到待唤醒状态 */
            extern void app_btn_idle(void);
            app_btn_idle();
        } else {
            extern void app_btn_wakeup(void);
            app_btn_wakeup();
        }
        break;

    case LISA_BTN_PRESS_DOUBLE_CLICK:
        /* 双击: 拍照识图 */
        LISA_LOGI(TAG, "power button double click, enter image recognition");
        extern int photo_recognition_trigger(void);
        photo_recognition_trigger();
        break;

    case LISA_BTN_PRESS_TRIPLE_CLICK:
        /* 三击: 调出小程序配置页 */
        LISA_LOGI(TAG, "power button triple click, toggle info page");
        assist_controller_trigger_event(CONTROLLER_EVENT_OPT_TOGGLE_INFO_PAGE, NULL, 0);
        break;

    case LISA_BTN_PRESS_QUINTUPLE_CLICK:
    case LISA_BTN_PRESS_REPEAT_CLICK:
        LISA_LOGI(TAG, "power button repeat click count: %d", info->click_count);
        if (info->click_count >= 8) { /* 连击超过8下: 恢复出厂设置 */
            LISA_LOGI(TAG, "Do factory reset");
            factory_reset();
            wifi_mgr_sta_disconnect(false);
            extern int play_factory_reset_audio(void);
            play_factory_reset_audio();
            extern int change_info_page(lisaui_userdata_qrcode_inter_mode_e mode);
            change_info_page(LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_NETWORK);
            enter_ble_config();
        } else if (info->click_count >= 5) { /* 连击超过5下: 进入BLE配置模式 */
            LISA_LOGI(TAG, "Do network reset");
            network_reset();
            wifi_mgr_sta_disconnect(false);
            extern int change_info_page(lisaui_userdata_qrcode_inter_mode_e mode);
            change_info_page(LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_NETWORK);
            enter_ble_config();
        }
        break;

    case LISA_BTN_PRESS_LONG_HOLD:
        /* 长按: 关机 */
        if (get_usb_status() == USB_STATUS_PLUG) {
            LISA_LOGI(TAG, "USB connected, ignore long press shutdown");
        } else {
            LISA_LOGI(TAG, "power button long press hold, shutting down...");
            power_shutdown();
        }
        break;

    default:
        break;
    }
}
#endif

#if CFG_NVS
#define NVDS_FLASH_ADDRESS_OFFSET (0xFF8000) // The last 32KB of 16B flash
#define NVDS_FLASH_SIZE           (0x8000)   // 32KB
struct nvs_fs arcs_nvs_fs;
FLASH_DEV arcs_flash_dev = {.base_addr = CMN_FLASHC_BASE,
                            .d_width = 4,
                            .sclk_div = 0xFF, // divider is 1
                            .run_mod = RUN_WITHOUT_INT,
                            .timeout = 0x180000};
int arcs_nvs_init(void)
{
    struct flash_pages_info info;
    flash_if_init(&arcs_flash_dev, 0, 0);
    arcs_nvs_fs.offset = NVDS_FLASH_ADDRESS_OFFSET;
    arcs_nvs_fs.flash_device = &arcs_flash_dev;
    flash_get_page_info_by_offs(&arcs_flash_dev, arcs_nvs_fs.offset, &info);
    arcs_nvs_fs.sector_size = info.size;
    arcs_nvs_fs.sector_count = NVDS_FLASH_SIZE / info.size;
    nvds_init(&arcs_nvs_fs);
    return 0;
}
#endif

static void log_shell_backend_output(const uint8_t *log, uint32_t len, void *data)
{
    extern void lisa_shell_output_raw(const char *data, int len);
    lisa_shell_output_raw((const char *)log, len);
}

static void app_task(void *param)
{
    ipc_mem_init(1);
    ipc_master_init(NULL);
    LISA_LOGI(TAG, "app task enter");

    ls_sys_init(TIMEZONE_SHANGHAI);
    LISA_LOGI(TAG, "system init end");

    extern int lisa_shell_init(void);

    lisa_shell_init();

    /* shell的串口和系统默认的日志输出串口是同一个, 这里暂停系统默认的日志输出 */
    lisa_log_backend_pause("sys.log");
    lisa_log_backend_add("user.shell", log_shell_backend_output, NULL);

    // extern int lisa_log_init(void);
    // lisa_log_init();
    // lisa_log_output_handle_set(lisa_shell_output_raw);

    /**
     * AP侧先核间通信建立后再进行wifi初始化
     * 此处先进行核间通信的建立
     */

    ic_message_init();
    LISA_LOGI(TAG, "IC message init end");

    // extern int sdmmc_hard_init(void);
    // sdmmc_hard_init();

#ifdef CONFIG_LISA_KV_TYPE_LSFS
    extern int user_fs_init(void);
    user_fs_init();
#endif

    // Check if KV storage is already initialized (e.g., from factory reset)
    lisa_kv_init();

    evs_utils_init();

    extern void update_device_id(void);
    update_device_id();

    extern int user_usb_start(void);
    user_usb_start();

    extern bool app_usb_msc_enabled(void);
    /* USB-MSC模式下, 不运行应用程序, 只支持USB文件传输 */
    if (app_usb_msc_enabled()) {
        while (1) {
            lisa_thread_delay(1000);
        }
    }

    int ret;
    // struct lsfs_file_t file;

    // lsfs_file_t_init(&file);
    // ret = lsfs_open(&file, "/SD:/font/lv_font_chinese_18.bin", LSFS_O_READ);
    // if (ret != 0) {
    //     LISA_LOGE(TAG, "lvgl font file not found, do not start ui");
    //     while(1) {
    // 		lisa_thread_delay(1000);
    // 	}
    // }
    // lsfs_close(&file);
    arcs_nvs_init();

    // Utils init
    LISA_LOGI(TAG, "utils init end");

    // Wifi预初始化
    ls_wifi_pre_init(_main_wifi_stack_init_done_cb);
    LISA_LOGI(TAG, "BLE init start\n");
    ls_ble_init();
    LISA_LOGI(TAG, "BLE init end\n");

    extern void comm_service_init(void);
    comm_service_init();
    LISA_LOGI(TAG, "comm service init end");

    /* LED service */
    app_led_init();
    app_led_off();
    extern void audio_play_service_start();
    audio_play_service_start();
    LISA_LOGI(TAG, "audio play service start");

    extern void audio_record_service_start();
    audio_record_service_start();
    LISA_LOGI(TAG, "audio record service start");

#if !defined(ARCS_DAC_USE_LITE_DAC)
    // PA init
    pa_manager_pre_init();
    pa_manager_init(PA_MGR_NONE);
    LISA_LOGI(TAG, "PA init end");

    // Speaker init
    listen_spk_init();
    LISA_LOGI(TAG, "speaker init end");
#endif

    // System init

    // Tone init
    app_tone_default_init();
    LISA_LOGI(TAG, "tone init end");

    pa_manager_pre_init();
    pa_manager_init(PA_MGR_NONE);
    LISA_LOGI(TAG, "PA init end");

    // LisaPlayer init
    app_player_init();
    LISA_LOGI(TAG, "app_player init end");

    lisa_aiui_set_interactive_mode(INTER_CONTINUE);

    // Welcome
    extern void app_main(void);
    app_main();

    // extern int ui_start(void);
    // ui_start();

    LISA_LOGI(TAG, "Application Ready!!!");

#if (CONFIG_FLEXIBLE_BUTTON)
    lisa_btn_init(button_callback_handle, NULL);
#endif

#if (CONFIG_BATTERY_COLLECTION)
    battery_adc_sample_init();
#endif
    listen_mic_gain_init();
    assistant_view_userdata_load();

    video_camera_init();
    camera_senor_release();

    assist_controller_init();

    // Check if WiFi account is already configured
    bool has_wifi_config = false;

    // Use the same API as wifi list command to get saved WiFi networks
    wifi_mgr_sta_config_t *matched_list = NULL;
    ret = wifi_mgr_storage_search_ap(&matched_list, SEARCH_ALL, NULL);

    if (ret > 0 && matched_list != NULL) {
        printf("Found %d saved WiFi network(s)\n", ret);

        // Print the found WiFi configurations for debugging
        for (int i = 0; i < ret; i++) {
            printf("WiFi config [%d]: SSID=[%s], BSSID=[%02x:%02x:%02x:%02x:%02x:%02x]\n", i + 1, matched_list[i].ssid,
                   matched_list[i].bssid[0], matched_list[i].bssid[1], matched_list[i].bssid[2],
                   matched_list[i].bssid[3], matched_list[i].bssid[4], matched_list[i].bssid[5]);
        }

        has_wifi_config = true;

        // Free the matched list
        free(matched_list);
    } else if (ret == 0) {
        printf("No saved WiFi networks found\n");
    } else {
        printf("Failed to get saved WiFi networks, error: %d\n", ret);
    }

    if (!has_wifi_config) {
        printf("No valid WiFi configuration found\n");
        extern int change_info_page(lisaui_userdata_qrcode_inter_mode_e mode);
        change_info_page(LISAUI_USERDATA_QRCODE_INTER_CONFIGURE_NETWORK);
        enter_ble_config();
    } else {
        ;
    }

    // Add your logic here based on has_wifi_config
}

static void *cjson_malloc(size_t sz)
{
    return exram_malloc(4, sz);
}

static void cjson_free(void *ptr)
{
    exram_free(ptr);
}

cJSON_Hooks cjson_hooks = {
    .malloc_fn = cjson_malloc,
    .free_fn = cjson_free,
};

int main(int argc, char **argv)
{
    /* 关闭看门狗 */
    WDT_RegDef *wdt_hw = (WDT_RegDef *)IP_AP_WDT;
    wdt_hw->REG_WREN.all = 0x5AA5;
    wdt_hw->REG_CTRL.all = 0;

    printf("\r\n");
    printf("project commit: %s\r\n", PROJECT_VERSION_COMMIT);
    printf("project version: %s\r\n", PROJECT_VERSION_STR);
    printf("\r\n");

    GPIO_Initialize(GPIOB(), NULL, NULL);
    usb_plug_detect_gpio_init();

    power_config_t power_cfg = {
        .on_shutdown = shutdown,
    };
    power_init(&power_cfg);

    /* 等待长按 3s */
    if (!power_wait_settle()) {
        power_shutdown();
        return 0;
    }

    app_led_blink(50, 50);

    heap_caps_malloc_extmem_enable(16);
    cJSON_InitHooks(&cjson_hooks);

    uint32_t time = xTaskGetTickCount();
    const Config *config = config_init((const char *)0x300f0000);
    time = xTaskGetTickCount() - time;
    if (config) {
        config_print(config);
    } else {
        printf("config parse failed\n");
    }
    printf("config parse elapsed time: %d ms\n", time);

    printk("Application start\n");
    lisa_thread_attr_t att = {.name = "APP_BOOT_TASK", .stack_size = APP_TASK_STACK, .priority = APP_TASK_PRIO};

    lisa_thread_create(&att, app_task, NULL);
}
