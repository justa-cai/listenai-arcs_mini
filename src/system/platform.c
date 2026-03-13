#include "sys_init.h"
#include "cJSON.h"
#include "config_parser.h"
#include "spiflash.h"
#include "chip.h"
#include "nvs.h"
#include "voice_msg.h"
#include "pa_manager.h"
#include "evs_utils.h"
#include "lite_dac.h"
#include "remote_logger.h"
#include "ipc_master.h"
#include "app_wakeup.h"

#if CONFIG_FILE_SYSTEM
#include "lsfs.h"
#include "disk/disk_access.h"
#include "lisa_sdmmc.h"
#endif

#include "romfs.h"
#include <string.h>

#define SDMMC_DEVICE      "SD:"
#define SDMMC_MOUNT_POINT "/"SDMMC_DEVICE

#include "player_mgr.h"
#define TAG "platform"
#include "lisa_log.h"

#if CONFIG_4G_MODULE
#include "ml307_modem.h"
#endif

#define AT_4G_UART_DEVICE              "uart2"        /* UART device name */


#ifdef CONFIG_BOARD_ARCS_MINI
#define TONE_BIN_ADDR       (CMN_FLASH_REGION + 0x00100000)
#define TONE_BIN_SIZE       (1 * 1024 * 1024)
#define WAKE_WORD_BIN_ADDR  (CMN_FLASH_REGION + 0x00200000)
#define WAKE_WORD_BIN_SIZE  (2 * 1024 * 1024)
#endif // CONFIG_BOARD_ARCS_MINI

extern int lisa_shell_init(void);
extern int user_usb_start(void);
extern bool app_usb_msc_enabled(void);
extern int boot_watchdog_feed(void);

#if CONFIG_FILE_SYSTEM
static struct lsfs_mount_t sdmmc_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = SDMMC_MOUNT_POINT,
    .fs_data = NULL,
};
#endif

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

static player_config_t s_player_configs[] = {
	{
		.id = AIP,
		.name = "AIP",
		.priority = 30,
		.capture_ids = {AIP, TTS, LOCAL},
		.capture_count = 3,
		.fg_action = PLAY_ACTION_RECOGNIZE,
		.bg_action = PLAY_ACTION_RECOGNIZE_END,
		.none_action = PLAY_ACTION_RECOGNIZE_END,
	},
	{
		.id = TTS,
		.name = "TTS",
		.priority = 10,
		.capture_ids = {AIP, ALERT},
		.capture_count = 2,
		.fg_action = PLAY_ACTION_PLAY,
		.bg_action = PLAY_ACTION_STOP,
		.none_action = PLAY_ACTION_STOP,
	},
	{
		.id = ALERT,
		.name = "ALERT",
		.priority = 40,
		.capture_ids = {AIP, TTS, LOCAL},
		.capture_count = 3,
		.fg_action = PLAY_ACTION_PLAY,
		.bg_action = PLAY_ACTION_STOP,
		.none_action = PLAY_ACTION_STOP,
	},
	{
		.id = CONTENT,
		.name = "CONTENT",
		.priority = 50,
		.capture_ids = {AIP, ALERT, LOCAL},
		.capture_count = 3,
		.fg_action = PLAY_ACTION_PLAY,
		.bg_action = PLAY_ACTION_PAUSE,
		.none_action = PLAY_ACTION_STOP,
	},
	{
		.id = LOCAL,
		.name = "LOCAL",
		.priority = 10,
		.capture_ids = {AIP},
		.capture_count = 1,
		.fg_action = PLAY_ACTION_PLAY,
		.bg_action = PLAY_ACTION_STOP,
		.none_action = PLAY_ACTION_STOP,
	},	
};

static int voice_platform_init(void)
{
    struct ipc_master_cb_tag ipc_cb = {
        .wifi_tx_data_cfm   = NULL,
        .wifi_rx_data       = NULL,
        .indication_handler = NULL
    };
    heap_caps_malloc_extmem_enable(16);
    cJSON_InitHooks(&cjson_hooks);

    ls_sys_init(8);

    ipc_mem_init(1);
    int ipc_ready = (ipc_master_init(&ipc_cb) == 0);

#if CONFIG_LISA_SHELL
    lisa_shell_init();
    // lisa_log_backend_add("user.shell", log_shell_backend_output, NULL);
#endif
    /**
     * AP侧先核间通信建立后再进行wifi初始化
     * 此处先进行核间通信的建立
     */

    if (ipc_ready) {
        ic_message_init();
        LISA_LOGI(TAG, "IC message init end");
    } else {
        LISA_LOGW(TAG, "IPC not ready, skip ic_message_init");
    }

#ifdef CONFIG_FILE_SYSTEM
    lisa_sdmmc_probe(lisa_device_get("sdmmc0"));
    disk_init(NULL);
    lsfs_init();

    if (lsfs_mount(&sdmmc_mnt) != 0) {
        LOGI("Mount failed, formatting...");
        if (lsfs_mkfs(LSFS_FATFS, SDMMC_DEVICE, NULL, 0) == 0) {
            if (lsfs_mount(&sdmmc_mnt) == 0) {
                LOGI("Mounted %s successfully\n", SDMMC_MOUNT_POINT);
            }
        } else {
            LOGE("Failed to mount filesystem: %d");
        }
    } else {
        LOGI("Mounted %s successfully\n", SDMMC_MOUNT_POINT);
    }
#endif

    // Check if KV storage is already initialized (e.g., from factory reset)
    lisa_kv_init();
    user_usb_start();
    /* USB-MSC模式下, 不运行应用程序, 只支持USB文件传输 */
    if (app_usb_msc_enabled()) {
        LISA_LOGW(TAG," Enter USB MSC mode, application will not start.\n");
        while (1) {
            boot_watchdog_feed();
            lisa_thread_delay(100);
        }
    }
    arcs_nvs_init();

    #if CONFIG_4G_MODULE
    lisa_4g_module_init(AT_4G_UART_DEVICE);
    #endif

    if (ipc_ready) {
        // Wifi预初始化
        ls_wifi_init(NULL);
        ls_wifi_pre_init(NULL);
        LISA_LOGI(TAG, "BLE init start\n");
        lisa_bluetooth_init();
        LISA_LOGI(TAG, "BLE init end\n");
    } else {
        LISA_LOGW(TAG, "IPC not ready, skip WiFi and BLE init");
    }

#if CONFIG_ACOMP
    acomp_init();
#if CONFIG_ACOMP_LOGGER
    acomp_logger_init();
    acomp_logger_start();
    acomp_logger_set_output_callback(remote_log_output_printf);
#endif

#ifndef CONFIG_BOARD_ARCS_MINI
    struct romfs *romfs = NULL;
    if (romfs_init(&romfs, 0x30100000, 0x700000) != 0) {
        LOGE("romfs init failed");
        romfs = NULL;
    }

    char *temp_buf = NULL;
    char locale[16] = {0};
    if (lisa_kv_get_string("user.locale", &temp_buf) == 0) {
        strncpy(locale, temp_buf, sizeof(locale) - 1);
        lisa_kv_free(temp_buf);
    }
#endif // CONFIG_BOARD_ARCS_MINI

#if CONFIG_ACOMP_WAKEUP
    struct wakeup_algo_resources res = {0};

#ifdef CONFIG_BOARD_ARCS_MINI
    res.mlp.size = 0;
    res.wrap.size = 0;

    struct romfs *romfs = NULL;
    if (romfs_init(&romfs, (const void *)WAKE_WORD_BIN_ADDR, WAKE_WORD_BIN_SIZE) == 0) {
        if (romfs_info_get(romfs, "/cae_esr.bin", &res.mlp.addr, &res.mlp.size) == 0) {
            LISA_LOGI(TAG, "algo resource info, name: cae_esr.bin, address: %p, size: %d", res.mlp.addr, res.mlp.size);
        } else {
            LISA_LOGW(TAG, "Load cae_esr.bin from wake_word ROMFS failed");
            res.mlp.size = 0;
        }

        if (romfs_info_get(romfs, "/wrap.json", &res.wrap.addr, &res.wrap.size) == 0) {
            LISA_LOGI(TAG, "algo resource info, name: wrap.json, address: %p, size: %d", res.wrap.addr, res.wrap.size);
        } else {
            LISA_LOGW(TAG, "Load wrap.json from wake_word ROMFS failed");
            res.wrap.size = 0;
        }

        romfs_deinit(&romfs);
    } else {
        LISA_LOGW(TAG, "Init wake_word ROMFS failed");
    }

    if (res.mlp.size == 0 || res.wrap.size == 0) {
        LISA_LOGW(TAG, "Wakeup algo resources not loaded, wakeup engine will be skipped");
    }
    app_wakeup_init(&res);
#else // !CONFIG_BOARD_ARCS_MINI
    if (romfs != NULL) {
        const char *algo_mlp_path = "zh-CN/algo.bin";
        const char *algo_wrap_path = "zh-CN/wrap.json";

        if (strcmp(locale, "en-GB") == 0) {
            algo_mlp_path = "en-GB/algo.bin";
            algo_wrap_path = "en-GB/wrap.json";
        }

        if (romfs_info_get(romfs, algo_mlp_path, &res.mlp.addr, &res.mlp.size) != 0) {
            LOGE("romfs file info get failed, path: %s", algo_mlp_path);
            while (1) {
                vTaskDelay(100);
            }
        }
        LOGI("algo resource info, name: %s, address: %p, size: %d", algo_mlp_path, res.mlp.addr, res.mlp.size);

        if (romfs_info_get(romfs, algo_wrap_path, &res.wrap.addr, &res.wrap.size) != 0) {
            LOGE("romfs file info get failed, path: %s", algo_wrap_path);
            while (1) {
                vTaskDelay(100);
            }
        }
        LOGI("algo resource info, name: %s, address: %p, size: %d", algo_wrap_path, res.wrap.addr, res.wrap.size);
    } else {
        res.mlp.addr = CONFIG_ACOMP_WAKEUP_RES_CAE_ESR_MLP_ADDRESS;
        res.mlp.size = CONFIG_ACOMP_WAKEUP_RES_CAE_ESR_MLP_LENGTH;
        res.wrap.addr = CONFIG_ACOMP_WAKEUP_RES_AI_WRAP_ADDRESS;
        res.wrap.size = CONFIG_ACOMP_WAKEUP_RES_AI_WRAP_LENGTH;
    }
    app_wakeup_init(&res);
#endif // CONFIG_BOARD_ARCS_MINI
#endif

#endif
	evs_utils_init();

#ifdef CONFIG_BOARD_ARCS_MINI
    app_tone_init(TONE_BIN_ADDR, TONE_BIN_SIZE);
#else // !CONFIG_BOARD_ARCS_MINI
    if (romfs) {
        const char *tone_path = "zh-CN/tone.bin";
        if (strcmp(locale, "en-GB") == 0) {
            tone_path = "en-GB/tone.bin";
        }
        uint8_t *tone_data;
        uint32_t tone_size;

        if (romfs_info_get(romfs, tone_path, &tone_data, &tone_size) != 0) {
            LOGE("romfs file info get failed, path: %s", tone_path);
            while (1) {
                vTaskDelay(100);
            }
        }

        if (app_tone_init((uint32_t)tone_data) != 0) {
            LOGE("tone init failed, addr: %p", tone_data);
            while (1) {
                vTaskDelay(100);
            }
        }
    } else {
        app_tone_init((uint32_t)0x30100000);
    }
#endif // CONFIG_BOARD_ARCS_MINI

    LISA_LOGI(TAG, "tone init end");
    pa_manager_pre_init();
    pa_manager_init(PA_MGR_NONE);
    // lite_dac_init();
    // dac_aud_t aud = {.rate=16000};
    // lite_dac_ctrl(ADAC_CTRL_AUD_CFG, &aud);
    // lite_dac_ctrl(ADAC_CTRL_START, NULL);
    player_mgr_init(s_player_configs, sizeof(s_player_configs) / sizeof(s_player_configs[0]));
    if (ipc_ready) {
        network_probe_init();
    }

    mcp_init();

    voice_msg_pub(VOICE_MSG_PLATFORM_READY, NULL, 0);

    // app_ble_adv_start(0, BLE_ADV_GEN);
    LISA_LOGI(TAG, "==== Application Ready!!!=====\n");

#ifndef CONFIG_BOARD_ARCS_MINI
    if (romfs) {
        romfs_deinit(&romfs);
    }
#endif // CONFIG_BOARD_ARCS_MINI

    return 0;
}

SYS_INIT(voice_platform_init, SYS_INIT_LEVEL_PRE_APPLICATION, 60);

#if CONFIG_LISA_FLASH_ARCS_HALT_REMOTE_CORE && CONFIG_WIFI_LWIP_SAME_CORE
static int app_ipc_init(void)
{
    ic_lock_init();
    ipc_master_init(NULL);

    return 0;
}

SYS_INIT(app_ipc_init,SYS_INIT_LEVEL_PRE_DEVICES_INIT,5); /* 优先级 */
#endif
