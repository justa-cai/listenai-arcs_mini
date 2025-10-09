#ifndef __LISTENAI_AUDIOMGR_H__
#define __LISTENAI_AUDIOMGR_H__

#define CHANNEL_NUM (6)

typedef enum { AIP = 0, TTS, ALERT, CONTENT, LOCAL, EXTRA} channel_type_e;
typedef enum { FOREGROUND = 0, BACKGROUND, NONE } focus_state_e;

typedef struct focus_state_s {
	channel_type_e m_channel;
	int m_priority;
	focus_state_e m_state;
} focus_state_t;

typedef struct channel_callback_s {
	void (*on_focus_state)(focus_state_e state, channel_type_e by_which);
	channel_type_e m_channel_type;
} channel_callback_cb;

typedef struct listen_audiomgr_s {
    channel_callback_cb *m_callbacks[CHANNEL_NUM];
	focus_state_t *m_aip;
	focus_state_t *m_tts;
	focus_state_t *m_alert;
	focus_state_t *m_content;
	focus_state_t *m_local;
	focus_state_t *m_extra;

	focus_state_t *m_foreground_channel;
	focus_state_t *m_background_channel;
} listen_audiomgr_t;

/**
 * @brief   焦点管理初始化
 * @return  listen_audiomgr_t
 */
listen_audiomgr_t* listen_audiomgr_create();

/**
 * @brief 焦点管理销毁
 * @param handle    焦点管理句柄
 */
void listen_audiomgr_destory(listen_audiomgr_t *handle);

/**
 * @brief 获取焦点
 * @param handle    焦点管理句柄
 * @param type      通道类型
 */
void listen_audiomgr_acquire_channel(listen_audiomgr_t *handle, channel_type_e type);

/**
 * @brief 释放焦点
 * @param handle    焦点管理句柄
 * @param type      通道类型
 */
void listen_audiomgr_release_channel(listen_audiomgr_t *handle, channel_type_e type);

/**
 * @brief 添加通道焦点回调
 * @param handle    焦点管理句柄
 * @param callback  焦点回调
 */
void listen_audiomgr_add_channel_callback(listen_audiomgr_t *handle, channel_callback_cb *callback);

#endif
