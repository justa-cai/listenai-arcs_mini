#ifndef __LISA_AIUI_H__
#define __LISA_AIUI_H__

#include "lisa_aiui_ws.h"
#include "lisa_err.h"

#define TOKEN_NVS_KEY               "AIUI_TOKEN_KEY"
#define PRODUCT_ID_NVS_KEY          "AIUI_PID_KEY"
#define SECRET_ID_NVS_KEY           "AIUI_SID_KEY"
#define MUSIC_ACTIVATING			"music_activating"
#define INTERACTIVE_MODE_NVS_KEY    "INTERACTIVE_MODE_KEY"
#define STAGING_MODE_NVS_KEY        "STAGING_MODE_KEY"

enum {
    LISA_AIUI_FRAME_TYPE_UNKNOW = 0,
    LISA_AIUI_FRAME_TYPE_TEXT,
    LISA_AIUI_FRAME_TYPE_TTS,
    LISA_AIUI_FRAME_TYPE_AUDIO,
    LISA_AIUI_FRAME_TYPE_IMAGE,
};


/**
 * @brief	AIUI实例
 */
typedef struct lisa_aiui {

	/// @brief aiui websocket 句柄
	lisa_aiui_ws_t *aiui_ws;

	/// @brief websocket连接成功回调
	void (*aiui_ws_connected)();
	/// @brief websocket连接云端主动断开回调
	void (*aiui_ws_disconnect)();
	/// @brief websocket接收消息回调
	void (*aiui_ws_onmessage)(const char *msg, int len);
    char *auth_token;
    char *auth_header;
} lisa_aiui_t;

/**
 * @brief AIUI事件回调
 *
 */
typedef struct lisa_aiui_cb {
	void (*aiui_ws_connected_cb)();
	void (*aiui_ws_disconnect_cb)();
	void (*aiui_ws_onmessage_cb)(const char *msg, int len);
} lisa_aiui_cb_t;

typedef struct {
	char *package_id;
	char *version;
	char *description;
	char *url;
	char *md5_checksum;
	uint64_t time;
	uint32_t sdk_version;
	uint32_t is_ok;
} aiui_ota_t;

typedef enum {
    INTER_ONESHOT,
    INTER_CONTINUE,
	INTER_BUTTON,
} lisa_aiui_interactive_mode_e;

lisa_aiui_interactive_mode_e lisa_aiui_get_interactive_mode(void);
int lisa_aiui_set_interactive_mode(lisa_aiui_interactive_mode_e mode);

void lisa_aiui_update_product_id(const char *pid);
void lisa_aiui_update_secret_id(const char *sid);
void lisa_aiui_clear_token(void);

/**
 * @brief SDK初始化函数
 *

 * @param aiui_cb           AIUI回调事件，非局部变量
 * @param aiui_audio_cb     AIUI识别音频回调
 * @return lisa_aiui_t*
 */
lisa_aiui_t *lisa_aiui_create(lisa_aiui_cb_t *aiui_cb);

/**
 * @brief 建立websocket连接
 *
 * @param handle            sdk句柄
 * @param timestamp         当前时间戳(秒数)
 * @return lisa_err_t
 */
lisa_err_t lisa_aiui_connect(lisa_aiui_t *const handle, bool update_token);

/**
 * @brief 发送音频数据
 *
 * @param handle            sdk句柄
 * @param audio             音频数据
 * @param len               音频长度
 * @return lisa_err_t
 */

lisa_err_t lisa_aiui_send_audio(const lisa_aiui_t *handle, const void *audio, int len);

/**
 * @brief 发送文本数据
 *
 * @param handle            sdk句柄
 * @param txt               文本数据
 * @return lisa_err_t
 */
lisa_err_t lisa_aiui_send_txt(const lisa_aiui_t *handle, const char *const txt);

lisa_err_t lisa_aiui_tts(const lisa_aiui_t *handle, const char *const txt);

/**
 * @brief 开始请求
 *
 * @param handle            sdk句柄
 * @return lisa_err_t
 */
lisa_err_t lisa_aiui_start_send(const lisa_aiui_t *handle, const char *uuid);

/**
 * @brief 开始发送请求
 * 
 * @param handle 			sdk句柄
 * @return lisa_err_t 
 */
lisa_err_t lisa_aiui_start_send_record(const lisa_aiui_t *handle);

/**
 * @brief 发送--end--, 停止发送数据
 *
 * @param handle            sdk句柄
 * @return lisa_err_t
 */
lisa_err_t lisa_aiui_stop_send(const lisa_aiui_t *handle);

/**
 * @brief 发送cancel，停止本次的识别,启动下一次识别
 *
 * @param handle            sdk句柄
 * @return lisa_err_t
 */
lisa_err_t lisa_aiui_cancel_send(const lisa_aiui_t *handle);
lisa_err_t lisa_aiui_end_frame_send(const lisa_aiui_t *handle);
lisa_err_t lisa_aiui_active();

lisa_err_t lisa_aiui_not_active();

/**
 * @brief 断开连接，云端websockek主动断开后必须调用。
 *
 * @param handle            sdk句柄
 * @return lisa_err_t
 */
lisa_err_t lisa_aiui_disconnect(lisa_aiui_t *handle);

/**
 * @brief SDK逆初始化，释放资源
 *
 * @param handle            sdk句柄
 * @return lisa_err_t
 */

lisa_err_t lisa_aiui_destroy(lisa_aiui_t *handle);

int lisa_aiui_img_recognition(lisa_aiui_t *hd, const void *img_data, int img_len);

/**
 * @brief Check if staging mode is enabled
 * 
 * @return true if staging mode enabled, false otherwise
 */
int lisa_aiui_get_device_mode(void);
#endif  //__LISA_AIUI_H__