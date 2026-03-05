#include "stdio.h"
#include <string.h>
#include <stdbool.h>

#include "acomp_fd.h"

#define TAG "app_fd"

#include "lisa_log.h"

/* 人脸识别结果回调 */
static void fd_event_handler(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{

    if (event & FD_CB_EVENT_ENGINE_RLT) {
        LISA_LOGI(TAG, "fd result:%d,%s", event_data_len, (char *)event_data);

        acomp_fd_result_info_t *info = (acomp_fd_result_info_t *)event_data;
        uint32_t result_cnt = info->results_cnt;
        if (result_cnt > 0) {
            uint32_t max_area_index = info->max_area_results_index;
            acomp_fd_result_t *result = (acomp_fd_result_t *)&info->results[max_area_index];


            LISA_LOGI(TAG, "detect face cnt: %d\n\n"
                            "max area result:\n"
                            "index: %d\n"
                            "face_rect: [x:%d, y:%d, w:%d, h:%d]\n"
                            "face_score: %f\n"
                            "face_align_cnt:%d\n"
                            "head_pose:[yaw:%f, pitch:%f, roll:%f]\n"
                            "face_live_result:[status:%d, scores[0]:%f, scores[1]:%f]\n"
                            "feature_cnt:%d\n"
                            "compare_cnt:%d\n"
                            "compare_scores:[%f, %f]\n",
                            result_cnt, 
                            max_area_index,
                            result->face_rect.x,
                            result->face_rect.y,
                            result->face_rect.w,
                            result->face_rect.h,
                            result->face_score,
                            result->n_align_point,
                            result->pose.yaw,
                            result->pose.pitch,
                            result->pose.roll,
                            result->live_result.status,
                            result->live_result.scores[0],
                            result->live_result.scores[1],
                            result->feature_cnt,
                            result->compare_cnt,
                            result->compare_scores[0],
                            result->compare_scores[1]);

        } else {
            LISA_LOGI(TAG, "no face detected");
        }
    }
}

int app_fd_init(void)
{
    /* 人脸识别算法 */
    acomp_fd_init();
    acomp_fd_prepare();
    acomp_fd_start();
    acomp_fd_add_callback(FD_CB_EVENT_ENGINE_RLT, fd_event_handler, NULL);
    const acomp_fd_live_detect_mode_t mode = {
        .enable = true,         // 启用活体检测
        .score_threshold = {
            0.5f,        // 非活体得分阈值（实际没用到这个参数）
            0.5f,        // 活体得分阈值（如果活体得分大于等于这个阈值， 就认为是活体，如果小于这个阈值，就不会进行人脸特征提取和人脸识别）
        },
    };
    acomp_fd_live_detect_mode_set(&mode);

    return 0;
}
