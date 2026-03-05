#include "stdio.h"
#include <string.h>
#include <stdbool.h>

// module: ic
#include "ic_message.h"
#include "workqueue.h"

#include "acomp.h"
#include "acomp_fd.h"
#include "fd_image_stream.h"

#include "video_camera.h"
#include "display.h"
#include "screen.h"

#include "button.h"
#include "button_adc.h"
#include "adc_key.h"

#define TAG "main"

#include "lisa_log.h"

#define FACE_FEATURE_COMPARE_SCORE_THRESHOLD    (1.0f * CONFIG_FACE_COMPARE_SCORE_THRESHOLD / 100)
#define SKIP_FRAME_CNT (((CONFIG_IMAGE_WIDTH == 640) && (CONFIG_IMAGE_HEIGHT == 480)) ? 3 : 2)

typedef struct {
    uint32_t index;
    button_event_t event;
}key_wq_msg_t;

static workqueue_t *key_wq = NULL;
static void key_wq_handler(void *para);

static uint8_t *image_buf = NULL;   /* 实时图片 */
static uint8_t *fd_result1 = NULL;  /* 实时人脸识别结果 */

static uint8_t *fd_features = NULL; /* 已注册的人脸特征 */
static int fd_feature_cnt = 0;      /* 已注册的人脸特征数量 */

/* 摄像头图片回调 */
static void video_camera_cb(camera_frame_t *frame, void *user_data)
{
    (void)user_data;
    static uint32_t frame_index = 0;

    /* 图片用于屏幕显示 */
    memcpy(image_buf, frame->buffer, frame->length);
    screen_update_fd_info(NULL, image_buf, CONFIG_IMAGE_WIDTH, CONFIG_IMAGE_HEIGHT, CONFIG_IMAGE_WIDTH * CONFIG_IMAGE_HEIGHT * 2);

    if ((++frame_index) % SKIP_FRAME_CNT != 0) {
        frame->func(frame->func_param);
        return;
    }

    LISA_LOGD(TAG, "idx:%u, camera buf:%p, len: %d, w:%d, h:%d, f:%d", 
                    (frame_index / SKIP_FRAME_CNT),
                    frame->buffer, 
                    frame->length,
                    frame->width,
                    frame->height,
                    frame->format);

    /* 图片用于人脸识别算法 */
    fd_image_stream_send((video_frame_t *)frame);
}

/* 人脸识别结果回调 */
static void fd_event_handler(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{

    if (event & FD_CB_EVENT_ENGINE_RLT) {
        LISA_LOGI(TAG, "fd result:%d,%s", event_data_len, (char *)event_data);

        uint8_t *fd_result = psram_malloc_align(32, event_data_len);
        if (fd_result == NULL) {
            LISA_LOGE(TAG, "%s, %d, malloc len failed", __FUNCTION__, __LINE__);
            return;
        }

        /* 人脸识别结果用于用户来使用（比如用户判断此特征是否符合注册条件，或者是否符合比对条件） */
        memcpy(fd_result1, (uint8_t *)event_data, event_data_len);
        
        /* 人脸识别结果显示到屏幕上面  */
        memcpy(fd_result, (uint8_t *)event_data, event_data_len);
        screen_update_fd_info((acomp_fd_result_info_t *)fd_result, NULL, CONFIG_IMAGE_WIDTH, CONFIG_IMAGE_HEIGHT, CONFIG_IMAGE_WIDTH * CONFIG_IMAGE_HEIGHT * 2);
        
    } else {
        LISA_LOGW(TAG, "unknown fd event:0X%X", event);
    }
}

/* 按键事件回调 */
static void adc_button_callback(button_event_t evt, button_adc_config_t *user_data)
{
    LISA_LOGI(TAG, "adc_button_index: %d, event: %d", user_data->button_index, evt);

    if ((evt != LISA_BTN_PRESS_CLICK) 
#if CONFIG_ONLY_FACE_REGISTER
    || (user_data->button_index > 1)
#else
    || (user_data->button_index > 2)
#endif
    ) {
        return;
    }

    key_wq_msg_t *msg = psram_malloc(sizeof(key_wq_msg_t));
    if (msg == NULL){
        LISA_LOGE(TAG, "%s, %d, malloc failed", __FUNCTION__, __LINE__);
        return;
    }

    msg->index = user_data->button_index;
    msg->event = evt;

    int ret = workqueue_submit(key_wq, key_wq_handler, msg);
    if (ret == 0) {
        LISA_LOGE(TAG, "key_wq submit failed:%d", ret);
        psram_free(msg);
    }
}
static bool is_gray = true;
/* 按键事件处理 */
static void key_wq_handler(void *para)
{
    key_wq_msg_t *msg = (key_wq_msg_t *)para;
    acomp_fd_result_info_t *info = (acomp_fd_result_info_t *)fd_result1;
    
    
    if (msg->index == 0) {  /* 按键key1用来注册人脸信息 */
        uint32_t result_cnt = info->results_cnt;
        if (result_cnt > 0) {
            uint32_t max_area_index = info->max_area_results_index;
            acomp_fd_result_t *result = (acomp_fd_result_t *)&info->results[max_area_index];
            float max_register_score = 0;
            int index = 0;


            LISA_LOGI(TAG, "score: %3f, align_n:%d, head_pose:[%3f, %3f, %3f], live:%d, s[0]:%f, s[1]:%f, feature_cnt:%d, compare_cnt:%d", 
                            result->face_score,
                            result->n_align_point,
                            result->pose.yaw,
                            result->pose.pitch,
                            result->pose.roll,
                            result->live_result.status,
                            result->live_result.scores[0],
                            result->live_result.scores[1],
                            result->feature_cnt,
                            result->compare_cnt
                            );

            /* 判断人脸特征是否满足注册要求（这里仅比较人脸得分、人脸对齐点数、人脸姿态角度，实际应用中可能需要考虑是否是活体、人脸特征点数等） */
            if ((result->face_score > 0.8) 
                    && (result->n_align_point > 0) 
                    && ((result->pose.yaw < 30.0f) && (result->pose.yaw > -30.0f)) 
                    && ((result->pose.pitch < 30.0f) && (result->pose.pitch > -30.0f)) 
                    && ((result->pose.roll < 30.0f) && (result->pose.roll > -30.0f)) 
                    && (result->live_result.status == 1) 
                    && (result->feature_cnt > 0)
                )
            {

                /* 如果已经有注册过人脸，需要看是不是同一个人，如果是就不再注册 */
                if (result->compare_cnt > 0) {
                    for (int i = 0; i < result->compare_cnt; i++) {
                        LISA_LOGI(TAG, "com score[%d]: %f", i, result->compare_scores[i]);
                        if (result->compare_scores[i] > max_register_score) {
                            max_register_score = result->compare_scores[i];
                            index = i;
                        }
                    }

                    LISA_LOGI(TAG, "max score is %f", max_register_score);

                    /* 判断人脸比较得分是否大于阈值，如果大于阈值，说明是同一个人，不再注册 */
                    if (max_register_score > FACE_FEATURE_COMPARE_SCORE_THRESHOLD) {
                        LISA_LOGE(TAG, "this face is already registered, score:%f!", max_register_score);
                        goto KEY_WQ_ERR;
                    }
                }


                if (fd_feature_cnt >= ACOMP_FD_MAX_RESULT_CNT) {
                    LISA_LOGE(TAG, "register count can't > %d, skip this feature!", ACOMP_FD_MAX_RESULT_CNT);
                    goto KEY_WQ_ERR;
                }

                /* 保存人脸特征点 */
                acomp_fd_feature_result_t *feature = (acomp_fd_feature_result_t *)(fd_features + sizeof(acomp_fd_feature_result_t) * fd_feature_cnt);
                memcpy(&feature->features[0], &result->features[0], sizeof(float) * result->feature_cnt);
                feature->feature_cnt = result->feature_cnt;

                /* 显示人脸注册数量 */
                fd_feature_cnt++;                
                screen_update_registered(fd_feature_cnt);
                LISA_LOGI(TAG, "register ok!");

                /* 重新加载已经注册的人脸特征到人脸识别算法 */
                if (fd_feature_cnt > 0) {
                    acomp_fd_features_load((acomp_fd_feature_result_t *)fd_features, fd_feature_cnt);
                }

            } else {
                LISA_LOGE(TAG, "register not ok!");
            }
        }
    } else if (msg->index == 1) {   /* 按键key2用来比对人脸信息 */
        uint32_t result_cnt = info->results_cnt;
        float max_score = 0.0f;
        int index = 0;

        if (result_cnt > 0) {
            uint32_t max_area_index = info->max_area_results_index;
            acomp_fd_result_t *result = (acomp_fd_result_t *)&info->results[max_area_index];

            LISA_LOGI(TAG, "score: %3f, align_n:%d, head_pose:[%3f, %3f, %3f], live:%d, s[0]:%f, s[1]:%f, feature_cnt:%d, compare_cnt:%d", 
                            result->face_score,
                            result->n_align_point,
                            result->pose.yaw,
                            result->pose.pitch,
                            result->pose.roll,
                            result->live_result.status,
                            result->live_result.scores[0],
                            result->live_result.scores[1],
                            result->feature_cnt,
                            result->compare_cnt
                            );

            /* 先找出最大人脸比对得分(和已有注册人脸的比对得分) */
            if (result->compare_cnt > 0) {
                for (int i = 0; i < result->compare_cnt; i++) {
                    LISA_LOGI(TAG, "com score[%d]: %f", i, result->compare_scores[i]);
                    if (result->compare_scores[i] > max_score) {
                        max_score = result->compare_scores[i];
                        index = i;
                    }
                }
            }

            LISA_LOGI(TAG, "max score is %f", max_score);


            /* 然后要判断人脸特征是否满足要求（这里仅比较人脸得分、人脸对齐点数、人脸姿态角度、人脸特征点数、人脸比对个数，实际应用中可能需要考虑是否是活体等） */
            if ((result->face_score > 0.8f) 
                    && (result->n_align_point > 0) 
                    && ((result->pose.yaw < 30.0f) && (result->pose.yaw > -30.0f)) 
                    && ((result->pose.pitch < 30.0f) && (result->pose.pitch > -30.0f)) 
                    && ((result->pose.roll < 30.0f) && (result->pose.roll > -30.0f))
                    && (result->live_result.status == 1) 
                    && (result->feature_cnt > 0)
                    && (result->compare_cnt > 0)
            )
            {
                /* 最大人脸得分是否大于阈值 */
                bool flag = (max_score > FACE_FEATURE_COMPARE_SCORE_THRESHOLD) ? true : false;

                /* 识别的得分用于屏幕显示 */
                screen_update_compare(index, flag, max_score);

            } else {
                screen_update_compare(index, false, max_score);
                LISA_LOGE(TAG, "compare not ok!");
            }
            
        }
    } else if (msg->index == 2) {   /* 按键key3用来切换原图模式和灰度模式 */
        LISA_LOGI(TAG, "gray mode: %d", is_gray);
        video_camera_set_gray(!is_gray);
        is_gray = !is_gray;
    }

KEY_WQ_ERR:
    psram_free(msg);
}

int main(int argc, char **argv)
{
    printf("CP=======! Hard ID: %d\n", CONFIG_HARTID);

    ic_message_init();
    LISA_LOGI(TAG, "ic_message_init done!");

    vTaskDelay(pdMS_TO_TICKS(2000));

    /* 存储实时的输入图像 */
    image_buf = psram_malloc_align(32, CONFIG_IMAGE_WIDTH * CONFIG_IMAGE_HEIGHT * 2);
    if (image_buf == NULL) {
        LISA_LOGE(TAG, "%s, %d, malloc len failed", __FUNCTION__, __LINE__);
        return -1;
    }

    /* 存储实时的人脸检测结果 */
    fd_result1 = psram_malloc_align(32, sizeof(acomp_fd_result_info_t) + sizeof(acomp_fd_result_t) * ACOMP_FD_MAX_RESULT_CNT);
    if (fd_result1 == NULL) {
        LISA_LOGE(TAG, "%s, %d, malloc len failed", __FUNCTION__, __LINE__);
        return -1;
    }

    /* 存储注册的人脸特征 */
    fd_features = psram_malloc_align(32, sizeof(acomp_fd_feature_result_t) * ACOMP_FD_MAX_RESULT_CNT);
    if (fd_features == NULL) {
        LISA_LOGE(TAG, "%s, %d, malloc len failed", __FUNCTION__, __LINE__);
        return -1;
    }

    /* 算法组件初始化 */
    acomp_init();
    
    /* 人脸识别算法模块 */
    acomp_fd_init();
    acomp_fd_prepare();
    acomp_fd_start();
    acomp_fd_add_callback(FD_CB_EVENT_ENGINE_RLT, fd_event_handler, NULL);
    const acomp_fd_param_t param = {
        .key = PARAM_DETECT_OUT_THRES,
        .value = 0.7f,
    };
    acomp_fd_params_set(&param, 1);
#if CONFIG_FACE_LIVE_DETECT_ENABLE
    const acomp_fd_live_detect_mode_t mode = {
        .enable = true,         // 启用活体检测
        .score_threshold = {
            0.5f,        // 非活体得分阈值（实际没用到这个参数）
            1.0f * CONFIG_FACE_LIVE_DETECT_THRESHOLD / 100, // 活体得分阈值（如果活体得分大于等于这个阈值， 就继续进行人脸特征提取和人脸识别）
        },
    };
    acomp_fd_live_detect_mode_set(&mode);
#endif
    /* 人脸图片输入流初始化 */
    fd_image_stream_init();

    /* 屏幕显示模块 */
    display_init();

    /* 按键模块 */
    key_wq = workqueue_create("key_wq", 9, 10, 8192);
    if (key_wq == NULL) {
        LISA_LOGE(TAG, "Failed to create key_wq");
    }

    test_adc_button(adc_button_callback);

    /* 摄像头模块 */
    camera_config_t config = {
        .width = CONFIG_IMAGE_WIDTH,
        .height = CONFIG_IMAGE_HEIGHT,
        .format = CONFIG_IMAGE_FORMAT,
    };
    video_camera_init(&config);
    video_camera_register_callback(video_camera_cb, NULL);
    video_camera_start_capture();

    while(1){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return 0;
}
