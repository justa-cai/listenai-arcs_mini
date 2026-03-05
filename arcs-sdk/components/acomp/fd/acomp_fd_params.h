#pragma once

#define ACOMP_FD_MAX_RESULT_CNT     (10)
#define ACOMP_FD_MAX_ALIGN_CNT    (68)
#define ACOMP_FD_MAX_FEATURE_CNT   (384)

typedef enum
{
	PARAM_FACE_NONE					    = 0,

	//FACE DETECT
	PARAM_FACE_DETECT_BEGIN             = 10000,
	PARAM_DETECT_OUT_THRES              = 10001,
	PARAM_DETECT_PROTHRES				= 10002,
	PARAM_DETECT_NMSTHRES				= 10003,
	PARAM_DETECT_PIXESIZE				= 10004,
	PARAM_FACE_DETECT_END,

	//FACE ALIGN
	PARAM_FACE_ALIGN_BEGIN 		        = 20000,
	PARAM_FACE_ALIGN_END,

	//FACE LIVE
	PARAM_FACE_LIVE_BEGIN 		        = 30000,
	PARAM_LIVE_THRES                    = 30001,
	PARAM_FACE_LIVE_END,

	//FACE VERIFY
	PARAM_FACE_VERIFY_BEGIN             = 40000,
	PARAM_VERIFY_THRES                  = 40001,
	PARAM_VERIFY_REGU_A					= 40002,
	PARAM_VERIFY_REGU_B					= 40003,
	PARAM_FACE_VERIFY_END,

	PARAM_FACE_MAX                      = 0XFFFFFFFF,
}acomp_fd_params_e;


typedef enum
{
	PIX_FMT_RGB888             = 0,
	PIX_FMT_BGR888  	       = 1,
	PIX_FMT_RGB565             = 2,
	PIX_FMT_BGR565  	       = 3,
	PIX_FMT_YUV444_PACKED      = 4,
	PIX_FMT_YUV422_YUYV_PACKED = 5,
	PIX_FMT_YUV422_UYVY_PACKED = 6,
	PIX_FMT_YUV422_YVYU_PACKED = 7,
	PIX_FMT_YUV422_VYUY_PACKED = 8,
	PIX_FMT_GRAY               = 9,
	PIX_FMT_MAX    		       = 0XFFFFFFFF,
}acomp_fd_pixel_format;