/**
 * @file main.c
 * @brief VenusA 人脸检测 CP 示例主入口，负责摄像头取流、人脸结果缓存和按键统计。
 */

#include "stdio.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "ic_message.h"
#include "lisa_device.h"
#include "lisa_mutex.h"
#include "lisa_uart.h"
#include "workqueue.h"

#include "acomp.h"
#include "acomp_fd.h"
#include "fd_image_stream.h"
#include "version.h"
#include "video_camera.h"

#if CONFIG_FACE_DETECT_UVC_ENABLE
#include "uvc_stream.h"
#endif

#if CONFIG_BUTTON
#include "IOMuxManager.h"
#include "button.h"
#include "button_gpio.h"
#endif

#if CONFIG_ACOMP_LOGGER
#include "remote_logger.h"
#endif

#define TAG "main"
#include "lisa_log.h"

#define FACE_DETECT_MAIN_LOGI_ENABLE 1 /* 使能人脸检测主模块 info 级别日志。 */
#define FACE_DETECT_MAIN_LOGD_ENABLE 0 /* 使能人脸检测主模块 debug 级别日志。 */

#if FACE_DETECT_MAIN_LOGI_ENABLE
#define FD_MAIN_LOGI(...) LISA_LOGI(TAG, __VA_ARGS__)
#else
#define FD_MAIN_LOGI(...)                                                                                              \
    do {                                                                                                               \
    } while (0)
#endif

#if FACE_DETECT_MAIN_LOGD_ENABLE
#define FD_MAIN_LOGD(...) LISA_LOGD(TAG, __VA_ARGS__)
#else
#define FD_MAIN_LOGD(...)                                                                                              \
    do {                                                                                                               \
    } while (0)
#endif

/* 人脸特征比对阈值，由百分比形式的 Kconfig 配置换算得到。 */
#define FACE_FEATURE_COMPARE_SCORE_THRESHOLD (1.0f * CONFIG_FACE_COMPARE_SCORE_THRESHOLD / 100)
/* 每 FRAME_SAMPLE_INTERVAL 帧采样 1 帧送入人脸检测算法。 */
#define FRAME_SAMPLE_INTERVAL                (((CONFIG_IMAGE_WIDTH == 640) && (CONFIG_IMAGE_HEIGHT == 480)) ? 3U : 2U)
/* 允许注册或计数的人脸检测最低质量分。 */
#define FACE_QUALITY_SCORE_THRESHOLD         (0.7f)
/* 允许注册或计数的人脸 yaw/pitch/roll 最大偏转角度。 */
#define FACE_POSE_THRESHOLD_DEG              (30.0f)
/* 单次人脸结果事件载荷所需的最大缓存大小。 */
#define FACE_RESULT_BUFFER_SIZE              (sizeof(acomp_fd_result_info_t) + sizeof(acomp_fd_result_t) * ACOMP_FD_MAX_RESULT_CNT)
/* 通过 UART 镜像输出人脸统计信息时的发送超时时间。 */
#define FACE_STATS_UART_TX_TIMEOUT_MS        1000U

#if CONFIG_LISA_UART_DEVICE && CONFIG_LISA_UART1
/* 用于镜像输出人脸统计信息的 UART 设备。 */
static lisa_device_t *face_stats_uart_dev = NULL;
#endif

#if CONFIG_BUTTON
#define FACE_KEY_GPIO_PORT   CSK_IOMUX_PAD_A
#define FACE_KEY_GPIO_PIN    4
#define FACE_KEY_PRESS_LEVEL 0

/**
 * @brief 按键工作队列消息。
 */
typedef struct {
    button_event_t event; /* GPIO 回调提交的按键事件。 */
} key_wq_msg_t;

static workqueue_t *key_wq = NULL;                             /* 用于在 GPIO 回调之外处理按键事件的工作队列。 */
static uint8_t *fd_features = NULL;                            /* 保存已注册人脸特征，供人脸检测组件进行特征比对。 */
static uint8_t *fd_result_snapshot = NULL;                     /* 按键工作队列使用的人脸结果稳定快照缓存。 */
static uint32_t fd_feature_cnt = 0;                            /* 当前已注册的人脸特征条目数量。 */
static uint32_t fd_success_cnt[ACOMP_FD_MAX_RESULT_CNT] = {0}; /* 每个已注册人脸 ID 的识别成功次数。 */
static uint32_t fd_total_success_cnt = 0;                      /* 注册或识别成功的总次数。 */

static void key_wq_handler(void *para);
#endif

static uint8_t *fd_result1 = NULL;           /* 最新人脸检测/识别结果缓存，由 fd_result_mutex 保护。 */
static lisa_mutex_t *fd_result_mutex = NULL; /* 保护最新人脸结果缓存及其元数据的互斥锁。 */
static uint32_t fd_result1_len = 0;          /* fd_result1 中当前有效数据长度。 */
static bool fd_result_ready = false;         /* fd_result1 是否已经缓存过有效结果载荷。 */

/**
 * @brief 初始化可选的人脸统计 UART 镜像输出。
 *
 * @details
 * 当 Kconfig 未开启 UART 支持时，该函数编译为空操作。初始化失败时仅禁用
 * UART 镜像输出，普通 printf 输出仍然继续工作。
 */
static void face_stats_uart_init(void)
{
#if CONFIG_LISA_UART_DEVICE && CONFIG_LISA_UART1
    lisa_uart_config_t uart_cfg = LISA_UART_CONFIG_LOW_SPEED();
    int ret;

    face_stats_uart_dev = lisa_device_get("uart1");
    if (!lisa_device_ready(face_stats_uart_dev)) {
        LISA_LOGW(TAG, "FACE_STATS uart1 not ready");
        face_stats_uart_dev = NULL;
        return;
    }

    ret = lisa_uart_configure(face_stats_uart_dev, &uart_cfg);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGW(TAG, "FACE_STATS uart1 configure failed:%d", ret);
        face_stats_uart_dev = NULL;
        return;
    }

    LISA_LOGI(TAG, "FACE_STATS uart1 mirror ready, baud:%u", uart_cfg.baudrate);
#endif
}

/**
 * @brief 向可选 UART 镜像输出写入统计文本。
 *
 * @param data 待发送的以 '\0' 结尾的字符串；NULL 或空字符串会被忽略。
 */
static void face_stats_uart_write(const char *data)
{
#if CONFIG_LISA_UART_DEVICE && CONFIG_LISA_UART1
    size_t len;

    if ((face_stats_uart_dev == NULL) || (data == NULL)) {
        return;
    }

    len = strlen(data);
    if (len == 0U) {
        return;
    }

    (void)lisa_uart_write_sync(face_stats_uart_dev, (const uint8_t *)data, (uint32_t)len,
                               FACE_STATS_UART_TX_TIMEOUT_MS);
#else
    (void)data;
#endif
}

/**
 * @brief 将格式化人脸统计信息输出到 stdout 和 UART 镜像。
 *
 * @param fmt printf 兼容的格式化字符串。
 */
static void face_stats_printf(const char *fmt, ...)
{
    char line[128];
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    if (len < 0) {
        return;
    }

    if ((size_t)len >= sizeof(line)) {
        len = (int)sizeof(line) - 1;
    }

    printf("%s", line);          // 打印到log
    face_stats_uart_write(line); // 结果输出
}

/**
 * @brief 缓存 ACOMP 人脸检测结果事件前校验载荷合法性。
 *
 * @param event_data 事件载荷指针。
 * @param event_data_len 事件载荷长度，单位字节。
 *
 * @retval true  载荷长度、结果数量和最大面积结果索引均合法。
 * @retval false 载荷为空、长度异常或内部字段不一致。
 */
static bool fd_result_payload_valid(const void *event_data, uint32_t event_data_len)
{
    const acomp_fd_result_info_t *info = (const acomp_fd_result_info_t *)event_data;
    uint32_t expected_len;

    if ((event_data == NULL) || (event_data_len < sizeof(acomp_fd_result_info_t)) ||
        (event_data_len > FACE_RESULT_BUFFER_SIZE)) {
        LISA_LOGW(TAG, "invalid fd result len:%u", event_data_len);
        return false;
    }

    if (info->results_cnt > ACOMP_FD_MAX_RESULT_CNT) {
        LISA_LOGW(TAG, "invalid fd result count:%u", info->results_cnt);
        return false;
    }

    if ((info->results_cnt > 0U) && (info->max_area_results_index >= info->results_cnt)) {
        LISA_LOGW(TAG, "invalid fd max index:%u count:%u", info->max_area_results_index, info->results_cnt);
        return false;
    }

    expected_len = sizeof(acomp_fd_result_info_t) + sizeof(acomp_fd_result_t) * info->results_cnt;
    if (event_data_len < expected_len) {
        LISA_LOGW(TAG, "short fd result len:%u expected:%u", event_data_len, expected_len);
        return false;
    }

    return true;
}

/**
 * @brief 摄像头帧回调，按采样间隔转发图像到人脸检测。
 *
 * @details
 * 回调会按配置先把帧送到 UVC 流，再根据 @ref FRAME_SAMPLE_INTERVAL 抽帧，
 * 仅将选中的图像帧送入人脸图像输入流。函数返回前始终调用原始帧释放回调。
 *
 * @param frame 摄像头模块提供的图像帧。
 * @param user_data 用户回调数据，当前未使用。
 */
static void video_camera_cb(camera_frame_t *frame, void *user_data)
{
    (void)user_data;
    static uint32_t frame_index = 0;
    uint32_t current_index;
    int ret;

    if (frame == NULL) {
        return;
    }

#if CONFIG_FACE_DETECT_UVC_ENABLE
    (void)uvc_stream_submit_frame(frame);
#endif

    /* 按采样间隔抽帧，降低算法输入帧率。 */
    current_index = frame_index++;
    if ((current_index % FRAME_SAMPLE_INTERVAL) != 0U) {
        goto VIDEO_CAMERA_CB_EXIT;
    }

    FD_MAIN_LOGD("idx:%u, camera buf:%p, len:%d, w:%d, h:%d, f:%d", (current_index / FRAME_SAMPLE_INTERVAL),
                 frame->buffer, frame->length, frame->width, frame->height, frame->format);

    /* 图片用于人脸识别算法 */
    ret = fd_image_stream_send_no_release((video_frame_t *)frame);
    if (ret != 0) {
        LISA_LOGW(TAG, "fd image stream send failed:%d", ret);
    }

VIDEO_CAMERA_CB_EXIT:
    if (frame->func != NULL) {
        frame->func(frame->func_param);
    }
}

/**
 * @brief 人脸检测事件回调，用于缓存引擎结果。
 *
 * @details
 * 仅处理引擎结果事件。校验通过的载荷会在互斥锁保护下复制到共享结果缓存，
 * 并按配置转发到 UVC 绘制路径用于人脸框显示。
 *
 * @param event 人脸检测组件上报的事件位图。
 * @param event_data 事件载荷指针。
 * @param event_data_len 事件载荷长度，单位字节。
 * @param priv 私有回调数据，当前未使用。
 */
static void fd_event_handler(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{
    (void)priv;

    if (event & FD_CB_EVENT_ENGINE_RLT) {
        FD_MAIN_LOGD("fd result:%u,%p", event_data_len, event_data);
        if (!fd_result_payload_valid(event_data, event_data_len)) {
#if CONFIG_FACE_DETECT_UVC_ENABLE && CONFIG_FACE_DETECT_UVC_DRAW_FACE_BOX
            (void)uvc_stream_update_face_result(NULL, 0);
#endif
            return;
        }

        if (lisa_mutex_lock(fd_result_mutex, LISA_OS_WAIT_FOREVER) != 0) {
            LISA_LOGW(TAG, "fd result lock failed");
            return;
        }
        memcpy(fd_result1, (uint8_t *)event_data, event_data_len);
        fd_result1_len = event_data_len;
        fd_result_ready = true;
        lisa_mutex_unlock(fd_result_mutex);
#if CONFIG_FACE_DETECT_UVC_ENABLE && CONFIG_FACE_DETECT_UVC_DRAW_FACE_BOX
        (void)uvc_stream_update_face_result((const acomp_fd_result_info_t *)event_data, event_data_len);
#endif
    } else {
        LISA_LOGW(TAG, "unknown fd event:0X%X", event);
    }
}

#if CONFIG_BUTTON
/**
 * @brief 将最新人脸结果复制到调用者提供的快照缓存。
 *
 * @param snapshot 目标缓存，容量至少为 @ref FACE_RESULT_BUFFER_SIZE 字节。
 * @param snapshot_len 输出实际复制到 @p snapshot 的字节数。
 *
 * @retval true  已成功复制一份就绪的人脸结果载荷。
 * @retval false 参数无效、结果尚未就绪或互斥锁加锁失败。
 */
static bool face_result_snapshot(uint8_t *snapshot, uint32_t *snapshot_len)
{
    if ((snapshot == NULL) || (snapshot_len == NULL)) {
        return false;
    }

    if (lisa_mutex_lock(fd_result_mutex, LISA_OS_WAIT_FOREVER) != 0) {
        LISA_LOGW(TAG, "fd result snapshot lock failed");
        return false;
    }

    if (!fd_result_ready || (fd_result1_len < sizeof(acomp_fd_result_info_t))) {
        lisa_mutex_unlock(fd_result_mutex);
        return false;
    }

    memcpy(snapshot, fd_result1, fd_result1_len);
    *snapshot_len = fd_result1_len;
    lisa_mutex_unlock(fd_result_mutex);

    return true;
}

/**
 * @brief 检查按键触发时使用的人脸结果是否满足识别/注册质量要求。
 *
 * @details
 * 该函数只做结果有效性和质量门限判断，不修改注册库或统计数据。
 * 只有检测分数、关键点、头部姿态和特征维度全部满足要求时，才允许
 * 后续进入人脸注册或识别计数流程。
 *
 * @param result 最新缓存的人脸检测/对齐/特征结果。
 *
 * @retval true  人脸质量满足要求，可以继续注册或识别计数。
 * @retval false 人脸结果为空、检测分数过低、姿态偏转过大或特征维度异常。
 */
static bool face_result_quality_passed(acomp_fd_result_t *result)
{
    bool score_ok;
    bool align_ok;
    bool yaw_ok;
    bool pitch_ok;
    bool roll_ok;
    bool feature_ok;

    if (result == NULL) {
        LISA_LOGW(TAG, "FACE_QUALITY result=NULL");
        return false;
    }

    score_ok = result->face_score > FACE_QUALITY_SCORE_THRESHOLD;
    align_ok = result->n_align_point > 0;
    yaw_ok = (result->pose.yaw < FACE_POSE_THRESHOLD_DEG) && (result->pose.yaw > -FACE_POSE_THRESHOLD_DEG);
    pitch_ok = (result->pose.pitch < FACE_POSE_THRESHOLD_DEG) && (result->pose.pitch > -FACE_POSE_THRESHOLD_DEG);
    roll_ok = (result->pose.roll < FACE_POSE_THRESHOLD_DEG) && (result->pose.roll > -FACE_POSE_THRESHOLD_DEG);
    feature_ok = (result->feature_cnt > 0) && (result->feature_cnt <= ACOMP_FD_MAX_FEATURE_CNT);

    FD_MAIN_LOGI("FACE_QUALITY score:%d(%f>%f), align:%d(%d), yaw:%d(%f), pitch:%d(%f), roll:%d(%f), feature:%d(%d/%d)",
                 score_ok, result->face_score, FACE_QUALITY_SCORE_THRESHOLD, align_ok, result->n_align_point, yaw_ok,
                 result->pose.yaw, pitch_ok, result->pose.pitch, roll_ok, result->pose.roll, feature_ok,
                 result->feature_cnt, ACOMP_FD_MAX_FEATURE_CNT);

    return score_ok && align_ok && yaw_ok && pitch_ok && roll_ok && feature_ok;
}

/**
 * @brief 将某个 ID 的成功次数换算为带一位小数的百分比。
 *
 * @param times 某个人脸 ID 的成功次数。
 *
 * @return 百分比乘以 10 后的整数值；当总成功次数为 0 时返回 0。
 */
static uint32_t face_percent_x10(uint32_t times)
{
    if (fd_total_success_cnt == 0U) {
        return 0U;
    }

    return (uint32_t)(((uint64_t)times * 1000U + fd_total_success_cnt / 2U) / fd_total_success_cnt);
}

/**
 * @brief 格式化 @ref face_percent_x10 生成的百分比值。
 *
 * @param buf 目标字符串缓存。
 * @param buf_size @p buf 的字节大小。
 * @param percent_x10 百分比乘以 10 后的整数值。
 */
static void face_format_percent(char *buf, size_t buf_size, uint32_t percent_x10)
{
    if ((buf == NULL) || (buf_size == 0U)) {
        return;
    }
    buf[0] = '\0';

    if ((percent_x10 % 10U) == 0U) {
        (void)snprintf(buf, buf_size, "%u%%", percent_x10 / 10U);
    } else {
        (void)snprintf(buf, buf_size, "%u.%u%%", percent_x10 / 10U, percent_x10 % 10U);
    }
}

/**
 * @brief 输出当前识别结果统计和所有人脸 ID 的累计统计。
 *
 * @param current_id 当前按键事件注册或识别到的人脸 ID。
 * @param current_score 当前结果对应的人脸分数或比对分数。
 */
static void face_stats_output(uint32_t current_id, float current_score)
{
    char percent[16];

    if ((fd_total_success_cnt == 0U) || (current_id >= fd_feature_cnt)) {
        return;
    }

    face_stats_printf("Current ID: %u\n", current_id);
    face_stats_printf("Current Score: %.2f\n", current_score);
    face_stats_printf("Current Times: %u\n", fd_success_cnt[current_id]);
    face_format_percent(percent, sizeof(percent), face_percent_x10(fd_success_cnt[current_id]));
    face_stats_printf("Current Attration: %s\n", percent);
    face_stats_printf("ToTal Attration:\n");

    for (uint32_t i = 0; i < fd_feature_cnt; i++) {
        face_format_percent(percent, sizeof(percent), face_percent_x10(fd_success_cnt[i]));
        face_stats_printf("ID: %u Times: %u Attraction: %s\n", i, fd_success_cnt[i], percent);
    }
}

/**
 * @brief 在已注册人脸中查找当前结果的最佳匹配项。
 *
 * @param result 包含比对分数的人脸结果。
 * @param match_id 输出匹配到的人脸 ID。
 * @param match_score 输出最佳比对分数。
 *
 * @retval true  存在超过 @ref FACE_FEATURE_COMPARE_SCORE_THRESHOLD 的比对分数。
 * @retval false 参数无效、没有可用比对分数或所有分数均未达到阈值。
 */
static bool face_best_match(acomp_fd_result_t *result, uint32_t *match_id, float *match_score)
{
    float max_score = 0.0f;
    uint32_t max_index = 0;
    int compare_cnt;

    if ((result == NULL) || (match_id == NULL) || (match_score == NULL)) {
        return false;
    }

    compare_cnt = result->compare_cnt;
    if (compare_cnt < 0) {
        compare_cnt = 0;
    }

    if (compare_cnt > (int)fd_feature_cnt) {
        compare_cnt = fd_feature_cnt;
    }

    for (int i = 0; i < compare_cnt; i++) {
        FD_MAIN_LOGD("com score[%d]:%f", i, result->compare_scores[i]);
        if (result->compare_scores[i] > max_score) {
            max_score = result->compare_scores[i];
            max_index = i;
        }
    }

    *match_id = max_index;
    *match_score = max_score;
    return (compare_cnt > 0) && (max_score > FACE_FEATURE_COMPARE_SCORE_THRESHOLD);
}

/**
 * @brief 注册新人脸或为已匹配人脸累加识别次数。
 *
 * @details
 * 如果当前结果匹配已注册特征，仅更新对应统计信息；否则在特征表未满时注册
 * 新特征，并重新加载到人脸检测组件。
 *
 * @param result 从快照中选中的有效人脸结果。
 * @param current_id 输出注册或识别到的人脸 ID。
 * @param current_score 输出当前人脸分数或比对分数。
 *
 * @retval true  已完成注册或匹配，并更新统计信息。
 * @retval false 参数无效、特征表已满或特征加载失败。
 */
static bool face_register_or_count(acomp_fd_result_t *result, uint32_t *current_id, float *current_score)
{
    uint32_t match_id = 0;
    float match_score = 0.0f;

    if ((result == NULL) || (current_id == NULL) || (current_score == NULL)) {
        return false;
    }

    if (face_best_match(result, &match_id, &match_score)) {
        /* 匹配到已注册特征 */
        fd_success_cnt[match_id]++;
        fd_total_success_cnt++;
        FD_MAIN_LOGI("FACE_RESULT ok action=verify id=%u score=%f count=%u", match_id, match_score,
                     fd_success_cnt[match_id]);
        *current_id = match_id;
        *current_score = match_score;
        return true;
    }

    /* 未匹配到, 注册新特征 */
    if (fd_feature_cnt >= ACOMP_FD_MAX_RESULT_CNT) {
        LISA_LOGW(TAG, "FACE_RESULT fail reason=full score=%f registered=%u", match_score, fd_feature_cnt);
        return false;
    }

    acomp_fd_feature_result_t *feature =
        (acomp_fd_feature_result_t *)(fd_features + sizeof(acomp_fd_feature_result_t) * fd_feature_cnt);
    memcpy(&feature->features[0], &result->features[0], sizeof(float) * result->feature_cnt);
    feature->feature_cnt = result->feature_cnt;

    fd_success_cnt[fd_feature_cnt] = 1;
    fd_total_success_cnt++;
    fd_feature_cnt++;

    /* 导入外部特征到注册库，用于后续人脸识别 */
    int ret = acomp_fd_features_load((acomp_fd_feature_result_t *)fd_features, fd_feature_cnt);
    if (ret != 0) {
        fd_feature_cnt--;
        fd_total_success_cnt--;
        fd_success_cnt[fd_feature_cnt] = 0;
        LISA_LOGE(TAG, "FACE_RESULT fail reason=feature_load ret=%d", ret);
        return false;
    }

    *current_id = fd_feature_cnt - 1U;
    *current_score = result->face_score;
    FD_MAIN_LOGI("FACE_RESULT ok action=register id=%u score=%f count=%u", *current_id, *current_score,
                 fd_success_cnt[*current_id]);
    return true;
}

/**
 * @brief GPIO 按键回调，将单击事件投递到工作队列处理。
 *
 * @param evt 按键驱动上报的事件。
 * @param user_data GPIO 按键配置指针，当前未使用。
 */
static void gpio_button_callback(button_event_t evt, button_gpio_config_t *user_data)
{
    (void)user_data;

    if (evt != LISA_BTN_PRESS_CLICK) {
        return;
    }

    key_wq_msg_t *msg = psram_malloc(sizeof(key_wq_msg_t));
    if (msg == NULL) {
        LISA_LOGE(TAG, "%s, %d, malloc failed", __FUNCTION__, __LINE__);
        return;
    }

    msg->event = evt;

    int ret = workqueue_submit(key_wq, key_wq_handler, msg);
    if (ret == 0) {
        LISA_LOGE(TAG, "key_wq submit failed:%d", ret);
        psram_free(msg);
    }
}

/**
 * @brief 初始化 PA04 按键及其工作队列。
 *
 * @retval 0  按键和工作队列初始化成功。
 * @retval -1 初始化失败。
 */
static int face_key_init(void)
{
    button_config_t btn_cfg = {
        .short_press_time_ms = 1500,
        .long_press_time_ms = 2000,
        .long_hold_time_ms = 4500,
        .press_logic_level = FACE_KEY_PRESS_LEVEL,
    };
    button_gpio_config_t btn_gpio_cfg = {
        .gpio_port = FACE_KEY_GPIO_PORT,
        .gpio_pin_num = FACE_KEY_GPIO_PIN,
    };

    key_wq = workqueue_create("key_wq", 9, 10, 8192);
    if (key_wq == NULL) {
        LISA_LOGE(TAG, "Failed to create key_wq");
        return -1;
    }

    if (!button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, gpio_button_callback)) {
        LISA_LOGE(TAG, "Failed to create PA04 gpio button");
        return -1;
    }

    FD_MAIN_LOGI("PA04 gpio button ready");
    return 0;
}

/**
 * @brief 处理按键单击事件并更新人脸注册/统计信息。
 *
 * @details
 * 处理流程为复制最新结果快照、选择最大面积人脸、执行质量门限检查，然后将其
 * 注册为新人脸或按已匹配人脸累计识别次数。GPIO 回调分配的消息会在退出前释放。
 *
 * @param para @ref gpio_button_callback 投递的 @ref key_wq_msg_t 指针。
 */
static void key_wq_handler(void *para)
{
    key_wq_msg_t *msg = (key_wq_msg_t *)para;
    uint32_t snapshot_len = 0;
    uint32_t current_id = 0;
    float current_score = 0.0f;
    acomp_fd_result_info_t *info = (acomp_fd_result_info_t *)fd_result_snapshot;

    if ((msg == NULL) || (msg->event != LISA_BTN_PRESS_CLICK)) {
        goto KEY_WQ_EXIT;
    }

    FD_MAIN_LOGI("----------face_detect result-----------------");
    if (!face_result_snapshot(fd_result_snapshot, &snapshot_len)) {
        LISA_LOGW(TAG, "FACE_RESULT fail reason=no_result");
        goto KEY_WQ_EXIT;
    }

    if (info->results_cnt == 0) {
        LISA_LOGW(TAG, "FACE_RESULT fail reason=no_face");
        goto KEY_WQ_EXIT;
    }

    if (info->results_cnt > ACOMP_FD_MAX_RESULT_CNT) {
        LISA_LOGW(TAG, "FACE_RESULT fail reason=too_many_results count=%u", info->results_cnt);
        goto KEY_WQ_EXIT;
    }

    if (info->max_area_results_index >= info->results_cnt || info->max_area_results_index >= ACOMP_FD_MAX_RESULT_CNT) {
        LISA_LOGW(TAG, "FACE_RESULT fail reason=invalid_result index=%u count=%u", info->max_area_results_index,
                  info->results_cnt);
        goto KEY_WQ_EXIT;
    }

    uint32_t expected_len = sizeof(acomp_fd_result_info_t) + sizeof(acomp_fd_result_t) * info->results_cnt;
    if (snapshot_len < expected_len) {
        LISA_LOGW(TAG, "FACE_RESULT fail reason=short_payload len=%u expected=%u", snapshot_len, expected_len);
        goto KEY_WQ_EXIT;
    }

    uint32_t required_len =
        sizeof(acomp_fd_result_info_t) + sizeof(acomp_fd_result_t) * (info->max_area_results_index + 1U);
    if (snapshot_len < required_len) {
        LISA_LOGW(TAG, "FACE_RESULT fail reason=short_result len=%u required=%u", snapshot_len, required_len);
        goto KEY_WQ_EXIT;
    }

    acomp_fd_result_t *result = &info->results[info->max_area_results_index];
    FD_MAIN_LOGI("face score:%3f, align_n:%d, head_pose:[%3f,%3f,%3f], live:%d, feature_cnt:%d, compare_cnt:%d",
                 result->face_score, result->n_align_point, result->pose.yaw, result->pose.pitch, result->pose.roll,
                 result->live_result.status, result->feature_cnt, result->compare_cnt);

    if (!face_result_quality_passed(result)) {
        LISA_LOGW(TAG, "FACE_RESULT fail reason=low_quality");
        goto KEY_WQ_EXIT;
    }

    if (face_register_or_count(result, &current_id, &current_score)) {
        face_stats_output(current_id, current_score);
    }
    FD_MAIN_LOGI("-----------------end-----------------------");

KEY_WQ_EXIT:
    if (msg != NULL) {
        psram_free(msg);
    }
}
#endif

/**
 * @brief VenusA 人脸检测 CP 示例应用入口。
 *
 * @details
 * 依次初始化核间消息、可选统计输出、人脸检测组件、图像输入流、可选 UVC 流、
 * 可选按键处理和摄像头采集；正常运行时不会返回。
 *
 * @param argc 参数数量，当前未使用。
 * @param argv 参数列表，当前未使用。
 *
 * @return 初始化失败时返回错误码；正常运行时不返回。
 */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("VenusA face-detect CP demo, version:%s, build:%s, hart:%d\n", FACE_DETECT_VERSION, FACE_DETECT_BUILD_TIME,
           CONFIG_HARTID);
    int ret = ic_message_init();
    if (ret != 0) {
        LISA_LOGE(TAG, "ic_message_init failed: %d", ret);
        return ret;
    }
    face_stats_uart_init();

    /* 存储实时的人脸检测结果 */
    fd_result1 = psram_malloc_align(32, FACE_RESULT_BUFFER_SIZE);
    if (fd_result1 == NULL) {
        LISA_LOGE(TAG, "%s, %d, malloc result failed", __FUNCTION__, __LINE__);
        return -1;
    }
    memset(fd_result1, 0, FACE_RESULT_BUFFER_SIZE);

    fd_result_mutex = lisa_mutex_create();
    if (fd_result_mutex == NULL) {
        LISA_LOGE(TAG, "fd_result_mutex create failed");
        return -1;
    }

#if CONFIG_BUTTON
    fd_result_snapshot = psram_malloc_align(32, FACE_RESULT_BUFFER_SIZE);
    if (fd_result_snapshot == NULL) {
        LISA_LOGE(TAG, "%s, %d, malloc result snapshot failed", __FUNCTION__, __LINE__);
        return -1;
    }
    memset(fd_result_snapshot, 0, FACE_RESULT_BUFFER_SIZE);

    /* 存储注册的人脸特征 */
    fd_features = psram_malloc_align(32, sizeof(acomp_fd_feature_result_t) * ACOMP_FD_MAX_RESULT_CNT);
    if (fd_features == NULL) {
        LISA_LOGE(TAG, "%s, %d, malloc feature failed", __FUNCTION__, __LINE__);
        return -1;
    }
    memset(fd_features, 0, sizeof(acomp_fd_feature_result_t) * ACOMP_FD_MAX_RESULT_CNT);
#endif

    /* 算法组件初始化 */
    ret = acomp_init();
    if (ret != 0) {
        LISA_LOGE(TAG, "acomp_init failed: %d", ret);
        return ret;
    }

#if CONFIG_ACOMP_LOGGER
    /* ACOMP AP核lo转发初始化 */
    acomp_logger_init();
    acomp_logger_start();
    acomp_logger_set_output_callback(remote_log_output_printf);
#endif

    /* 人脸识别算法模块 */
    ret = acomp_fd_init();
    if (ret != 0) {
        LISA_LOGE(TAG, "acomp_fd_init failed: %d", ret);
        return ret;
    }

    ret = acomp_fd_prepare();
    if (ret != 0) {
        LISA_LOGE(TAG, "acomp_fd_prepare failed: %d", ret);
        return ret;
    }

    ret = acomp_fd_start();
    if (ret != 0) {
        LISA_LOGE(TAG, "acomp_fd_start failed: %d", ret);
        return ret;
    }
    acomp_fd_add_callback(FD_CB_EVENT_ENGINE_RLT, fd_event_handler, NULL);

    const acomp_fd_param_t param = {
        .key = PARAM_DETECT_OUT_THRES,
        .value = 0.7f,
    };
    acomp_fd_params_set(&param, 1);

#if CONFIG_FACE_LIVE_DETECT_ENABLE
    const acomp_fd_live_detect_mode_t mode = {
        .enable = true,
        .score_threshold =
            {
                0.5f,
                1.0f * CONFIG_FACE_LIVE_DETECT_THRESHOLD / 100,
            },
    };
    acomp_fd_live_detect_mode_set(&mode);
#endif

    /* 人脸图片输入流初始化 */
    fd_image_stream_init();

#if CONFIG_FACE_DETECT_UVC_ENABLE
    ret = uvc_stream_init();
    if (ret != 0) {
        LISA_LOGE(TAG, "uvc_stream_init failed: %d", ret);
        return ret;
    }
#endif

#if CONFIG_BUTTON
    ret = face_key_init();
    if (ret != 0) {
        return ret;
    }
#endif

    /* 摄像头模块 */
    camera_config_t config = {
        .width = CONFIG_IMAGE_WIDTH,
        .height = CONFIG_IMAGE_HEIGHT,
        .format = CONFIG_IMAGE_FORMAT,
    };
    video_camera_init(&config);
    video_camera_register_callback(video_camera_cb, NULL);
    video_camera_start_capture();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
