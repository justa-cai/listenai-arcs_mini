#include <stdbool.h>
#include <assert.h>
#include <FreeRTOS.h>
#include <stdint.h>
#include <task.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define TAG "rtsp-test"
#include <lisa_log.h>
#include <lisa_kv.h>
#include <mac_manager.h>
#include <mac_manager_ops.h>
#include <wifi_manager/wifi_manager.h>
#include <lisa_wifi.h>
#include <ls_wifi_type.h>
#include <net_al.h>
#include <net_def.h>

#include "rtsp.h"
#include "session.h"
#include "streamer.h"

/* 模块化头文件 */
#include "audio.h"
#include "camera.h"

#define TARGET_WIFI_SSID   "Your_SSID"
#define TARGET_WIFI_PWD     "Your_Password"
#define WIFI_MGR_AUTO_CONNECT_INTERVAL_MS 2000
#define WIFI_MGR_SEARCH_AP_BUFFER_SIZE 10

static mac_manager_t *m_mac_manager;
static wifi_mgr_sta_config_t list[WIFI_MGR_SEARCH_AP_BUFFER_SIZE] = { 0 };
static volatile bool g_wifi_connected = false;
static volatile bool g_get_ip_success = false;

static int8_t custom_get_wifi_mac(uint8_t mac_addr[6])
{
    if (!mac_addr) return -1;
    int8_t ret = mac_manager_get(m_mac_manager, mac_addr, 6);
    LOGI("custom_get_wifi_mac: %d, %02X:%02X:%02X:%02X:%02X:%02X", ret,
         mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    return ret;
}

static void dhcp_status_callback(int vif_idx, bool success, uint32_t ip_addr, uint32_t netmask, uint32_t gateway, void *arg)
{
    if (success) {
        LOGI("DHCP Success on VIF-%d: IP=%d.%d.%d.%d", vif_idx,
             ip_addr & 0xff, (ip_addr >> 8) & 0xff, (ip_addr >> 16) & 0xff, (ip_addr >> 24) & 0xff);
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
    mac_manager_config_t config = { .random_mac_if_mac_invalid = false };
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
    wifi_mgr_sta_add_connection_cb(wifi_mgr_connection_status_cb, NULL);

    int count = wifi_mgr_storage_search_ap(list, WIFI_MGR_SEARCH_AP_BUFFER_SIZE, SEARCH_ALL, NULL);
    for (int i = 0; i < count; i++) {
        LOGI("Deleting saved AP: ssid=%s", list[i].ssid);
        wifi_mgr_storage_delete_ap(&list[i]);
    }

    wifi_mgr_storage_save_ap(&cfg);
    wifi_mgr_auto_connect_start(&autoconn_cfg);
    LOGI("WiFi auto-connect started");
}

static int wait_for_wifi_connection(void)
{
    int timeout = 30;
    LOGI("Waiting for WiFi connection and IP address...");

    while (timeout > 0) {
        if (g_wifi_connected && g_get_ip_success) {
            LOGI("WiFi connected and IP obtained");
            vTaskDelay(pdMS_TO_TICKS(500));
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        timeout--;
        if (timeout % 5 == 0) {
            LOGI("Waiting... (WiFi:%d, IP:%d, timeout:%d)", g_wifi_connected, g_get_ip_success, timeout);
        }
    }

    LOGI("WiFi connection timeout");
    return -1;
}

/* 流媒体任务上下文 */
typedef struct {
    rtsp_streamer_t *streamer;
    rtsp_session_t *session;
    volatile bool should_exit;
    TaskHandle_t task_handle;
} streaming_task_ctx_t;

/* 流媒体推送任务 - 独立处理音视频采集和发送 */
static void streaming_task(void *arg)
{
    streaming_task_ctx_t *ctx = (streaming_task_ctx_t *)arg;
    rtsp_streamer_t *streamer = ctx->streamer;
    rtsp_session_t *session = ctx->session;

    uint32_t last_video_ms = 0;
    uint32_t video_interval_ms = 30;  // 匹配摄像头实际帧率 ~12.5 FPS

    /* FPS 统计变量 */
    uint32_t frame_count = 0;
    uint32_t last_fps_print_ms = 0;

    /* 性能统计变量 */
    uint32_t total_capture_time = 0;
    uint32_t total_send_time = 0;
    uint32_t total_bytes_sent = 0;
    uint32_t total_loop_time = 0;
    uint32_t loop_count = 0;

    LOGI("Streaming task started");

    while (!ctx->should_exit && !session->is_stopped) {
        uint32_t loop_start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t current_ms = loop_start_ms;

        /* 等待客户端开始推流 */
        if (!session->is_streaming) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* 从摄像头捕获实时图像并转换为 JPEG */
        if (current_ms - last_video_ms >= video_interval_ms) {
            uint8_t *frame_buffer = NULL;
            uint32_t frame_len = 0;
            uint32_t capture_start = xTaskGetTickCount() * portTICK_PERIOD_MS;

            if (camera_capture_frame(&frame_buffer, &frame_len) == 0) {
                uint32_t capture_end = xTaskGetTickCount() * portTICK_PERIOD_MS;
                uint32_t capture_time = capture_end - capture_start;
                total_capture_time += capture_time;

                /* 使用实时捕获并编码的 JPEG 数据 */
                uint32_t send_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
                rtsp_streamer_stream_frame(streamer, frame_buffer, frame_len, current_ms);
                uint32_t send_end = xTaskGetTickCount() * portTICK_PERIOD_MS;
                uint32_t send_time = send_end - send_start;
                total_send_time += send_time;
                total_bytes_sent += frame_len;

                frame_count++;
            }
            last_video_ms = current_ms;
        }

        /* 音频流推送 - 每次循环都尝试清空队列中的所有音频帧 */
        {
            uint8_t *audio_frame = NULL;
            uint32_t audio_len = 0;

            while (audio_capture_frame(&audio_frame, &audio_len) == 0) {
                rtsp_streamer_stream_audio_frame(streamer, audio_frame,
                                                 audio_len, current_ms);
            }
        }

        /* 每5秒打印一次FPS和性能统计 */
        if (current_ms - last_fps_print_ms >= 5000) {
            if (last_fps_print_ms > 0) {
                uint32_t elapsed_ms = current_ms - last_fps_print_ms;
                float fps = (float)frame_count * 1000.0F / (float)elapsed_ms;
                float kbytes_per_sec = (float)total_bytes_sent * 1000.0F / (float)elapsed_ms / 1024.0F;

                uint32_t avg_capture = frame_count > 0 ? total_capture_time / frame_count : 0;
                uint32_t avg_send = frame_count > 0 ? total_send_time / frame_count : 0;
                uint32_t avg_loop = loop_count > 0 ? total_loop_time / loop_count : 0;

                LOGI("=== Performance Report ===");
                LOGI("Video FPS: %.2f (frames: %u, elapsed: %u ms)",
                     fps, frame_count, elapsed_ms);
                LOGI("Throughput: %.2f KB/s", kbytes_per_sec);
                LOGI("Avg Capture+Encode: %u ms/frame", avg_capture);
                LOGI("Avg Network Send: %u ms/frame", avg_send);
                LOGI("Avg Loop Time: %u ms", avg_loop);
                LOGI("=========================");
            }
            /* 重置统计 */
            frame_count = 0;
            total_bytes_sent = 0;
            total_capture_time = 0;
            total_send_time = 0;
            total_loop_time = 0;
            loop_count = 0;
            last_fps_print_ms = current_ms;
        }

        /* 统计循环耗时 */
        uint32_t loop_end_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        total_loop_time += (loop_end_ms - loop_start_ms);
        loop_count++;

        /* 短暂让出 CPU */
        taskYIELD();
    }

    LOGI("Streaming task exiting");
    vTaskDelete(NULL);
}

void handle_client(int client)
{
    rtsp_streamer_t streamer;
    rtsp_session_t session;
    streaming_task_ctx_t streaming_ctx;
    uint32_t camera_width = 0;
    uint32_t camera_height = 0;

    /* 获取摄像头分辨率 */
    camera_get_resolution(&camera_width, &camera_height);

    /* 使用摄像头实际宽高初始化 RTSP streamer */
    rtsp_streamer_init(&streamer, camera_width, camera_height);

    /* 初始化音频流 */
    rtsp_audio_config_t audio_config = {
        .payload_type = 0,
        .sample_rate = AUDIO_SAMPLE_RATE,
        .channels = AUDIO_CHANNELS,
        .codec_name = "PCMU"
    };
    rtsp_streamer_init_audio(&streamer, &audio_config);

    rtsp_session_init(&session, client, &streamer);
    rtsp_streamer_set_uri(&streamer, "0.0.0.0:8554", "mjpeg", "1");

    streamer.is_debug = false;
    session.is_debug = false;

    rtsp_streamer_add_session(&streamer, &session);

    /* 初始化流媒体任务上下文 */
    streaming_ctx.streamer = &streamer;
    streaming_ctx.session = &session;
    streaming_ctx.should_exit = false;
    streaming_ctx.task_handle = NULL;

    /* 设置推流状态为 true，开始采集音频 */
    audio_start_streaming();
    LOGI("Streaming started, audio capture enabled");

    /* 创建流媒体推送任务 */
    BaseType_t ret = xTaskCreate(
        streaming_task,
        "streaming",
        4096,
        &streaming_ctx,
        tskIDLE_PRIORITY + 2,  // 较高优先级确保实时性
        &streaming_ctx.task_handle
    );
    if (ret != pdPASS) {
        LOGE("Failed to create streaming task");
        goto cleanup;
    }
    LOGI("Streaming task created");

    /* 主循环：专注于 RTSP 信令处理 */
    while (!session.is_stopped) {
        /* rtsp_streamer_start 处理 RTSP 信令（DESCRIBE/SETUP/PLAY/TEARDOWN） */
        rtsp_streamer_start(&streamer, 500);  // 可以使用更长的超时
    }

    /* 通知流媒体任务退出 */
    streaming_ctx.should_exit = true;
    LOGI("Waiting for streaming task to exit...");

    /* 等待任务退出（最多等待1秒） */
    for (int i = 0; i < 100 && streaming_ctx.task_handle != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
        /* 检查任务是否已删除 - eTaskGetState 返回 eDeleted 或检查句柄 */
        if (eTaskGetState(streaming_ctx.task_handle) == eDeleted) {
            break;
        }
    }

cleanup:
    /* 推流结束，停止音频采集 */
    audio_stop_streaming();
    LOGI("Streaming stopped, audio capture disabled");

    /* 清空音频队列中的残留数据 */
    audio_clear_queue();

    /* 清理 RTSP 资源 */
    rtsp_streamer_deinit(&streamer);
    LOGI("RTSP resources cleaned up");
}

int main(int argc, char **argv)
{
    int sockfd;
    struct sockaddr_in serv_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    LOGI("RTSP Server Starting...");

    user_mac_manager_init();
    net_dhcp_register_status_callback(dhcp_status_callback, NULL);

    lisa_wifi_ops_t ops = { .custom_mac = custom_get_wifi_mac };
    lisa_wifi_init(&ops);

    user_fs_init();
    lisa_kv_init();

    user_wifi_manager_init();

    if (wait_for_wifi_connection() != 0) {
        LOGI("Failed to connect to WiFi");
        return -1;
    }

    /* 初始化摄像头 */
    if (camera_init() != 0) {
        LOGE("Failed to initialize camera");
        return -1;
    }

    /* 初始化音频 */
    if (audio_init() != 0) {
        LOGE("Failed to initialize audio");
        return -1;
    }

    LOGI("Starting RTSP server on port 8554");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(8554);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        LOGI("failed to reuse addr");
        return 0;
    }

    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) {
        LOGI("failed to bind port errno=%d", errno);
        return 0;
    }

    if (listen(sockfd, 5) != 0) return 0;

    for (;;) {
        int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len);
        LOGI("client connected: %s", inet_ntoa(client_addr.sin_addr));
        // Only handle one client at a time, if we want to handle multiple clients, use threads
        handle_client(clientfd);
    }

    close(sockfd);
    return 0;
}
