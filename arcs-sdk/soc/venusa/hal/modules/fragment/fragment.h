#ifndef __FRAGMENT_H__
#define __FRAGMENT_H__
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define FRAGMENT_MAX_SLICE           16      // hori or vert max slice num
#define FRAGMENT_ALIGN_DOWN(size, align)  ((size) = ((size) & ~((align) - 1)))
#define SCALER_MERGE_MAX_NUM         16      // hori or vert max merge(data average) num

// 图像格式枚举定义
typedef enum {
    RGB888,
    YUV422P,
    YUV420P,
    RGBA8888
} imag_format_e;

// 图像缩放参数结构体定义
typedef struct {
    // 基本参数
    int reg_imwidthin;
    int reg_imheightin;
    int reg_imwidthout;
    int reg_imheightout;
    int temp_imwidthin;
    int temp_imheightin;
    int partHori;
    int partVert;
    int width;
    int height;
    int width_max;
    int height_max;
    
    // 动态数组指针
    int reg_imwidthin_list[FRAGMENT_MAX_SLICE];
    int reg_imwidthout_list[FRAGMENT_MAX_SLICE];
    int reg_start_phase_x_list[FRAGMENT_MAX_SLICE];
    int reg_scalerstart_x_overlap[FRAGMENT_MAX_SLICE];
    int reg_SkipFristSampleFlag_list[FRAGMENT_MAX_SLICE];
//    int reg_tailFlag_list[FRAGMENT_MAX_SLICE];
    int reg_imheightin_list[FRAGMENT_MAX_SLICE];
    int reg_imheightout_list[FRAGMENT_MAX_SLICE];
    int reg_start_phase_y_list[FRAGMENT_MAX_SLICE];
    int reg_scalerstart_x_list[FRAGMENT_MAX_SLICE];
    int reg_scalerstart_y_list[FRAGMENT_MAX_SLICE];
    int reg_scalerstart_y_overlap[FRAGMENT_MAX_SLICE];
    int taildrop[FRAGMENT_MAX_SLICE];
    int reg_TilesStart_x_list[FRAGMENT_MAX_SLICE];
    
    // 缩放参数
    int reg_phasestep_x;
    int reg_phasestep_y;
    int widthout;
    int heightout;
    
    // 控制标志
    int reg_merge_flag;
    int reg_scaler_flag;
    int reg_scaler_up_x;
    int reg_scaler_up_y;
    
    // 浮点参数
    double phasestep_x_f;
    double phasestep_y_f;
    
    // 其他参数
    int reg_hori_num;
    int reg_vert_num;
    int scaler_imwidthin;
    int scaler_imheightin;
    imag_format_e in_format;     //fix RGB888
    imag_format_e out_format;    //fix RGB888
    int widthoutSum;
    int widthinSum;
    int tailFlag;
    int taliLastSample;
    int res_imwidthin;
    int hori_cnt;
    int flag;
} cal_scaler_param_t;

unsigned int dma2d_scaler_image_fragment(cal_scaler_param_t *param);

#endif /* __FRAGMENT_H__ */
