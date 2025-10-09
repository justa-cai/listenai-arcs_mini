#ifndef __LIS_TTS_H__
#define __LIS_TTS_H__

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus/*需要被.c文件使用的函数声明*/

#include "lis_algo.h"

typedef enum
{
    LIS_TTS_STATE_ING = 0,   // 进行中
    LIS_TTS_STATE_TTS_END,   // TTS 合成结束， 此时，播放一般还在继续
    LIS_TTS_STATE_ERR,       // 内部出错
    LIS_TTS_STATE_OVER,      // 正常结束
    LIS_TTS_STATE_EARLY_OVER // 提前结束，如esp32调用了lis_tts_stop或者按下笔头需要启动ocr，tts就会被强制结束
} lis_tts_status;

#define XTTS_ROLE_LINGXIAOQI (1) // 中文
#define XTTS_ROLE_LUCY (2) // 英文
#define XTTS_ROLE_YILIN (3) // 英文

// init
void lis_tts_init(void);
// init
void lis_tts_deinit(void);
// 增强音量设置[0-10]
lis_err_t lis_tts_enhance_vol(int32_t vol);
// 启动tts,并将和应用相关项在此通过参数传入，参数可设置范围需提供
lis_err_t lis_tts_start(char *txt, uint32_t txt_size, uint32_t speed, uint32_t vol, uint8_t role);

// 查询tts状态，在lis_tts_start后会轮询调用该函数获取tts状态
lis_tts_status lis_tts_get_status();

// 强制结束，阻塞式，
lis_err_t lis_tts_stop();

// 将原TTS引擎支持的参数和参数值进行开放，支持可设置，具体哪些参数可设置需要算法层提供，如1的读法，语种等
// 在lis_tts_start之前调用
lis_err_t lis_tts_set_param(int param, int param_value); // 非当前业务强相关接口，可后续讨论实现！！！

// 音频数据回传接口，非当前业务强相关接口，可后续讨论实现！！！
lis_err_t lis_tts_set_pcm_back(int is_back); //
// 获取合成音频
int lis_tts_get_pcm(char *buf, uint32_t buf_size);
// 清除合成流中的剩余音频
lis_err_t lis_tts_clear_pcm(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __LIS_TTS_H__
