/**
 * @file xz_cloud.h
 * @brief 小智云端 API - 与 app_cloud 和 jk_cloud 类似的接口
 */

#ifndef __XZ_CLOUD_H__
#define __XZ_CLOUD_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 小智云端句柄 */
typedef struct xz_cloud_s *xz_cloud_t;

/** 应用客户端结构 (前向声明) */
struct app_client_s;

/**
 * @brief 创建小智云端
 * @param client 应用客户端
 * @return 小智云端句柄
 */
xz_cloud_t xz_cloud_create(struct app_client_s *client);

/**
 * @brief 销毁小智云端
 * @param cloud 小智云端句柄
 */
void xz_cloud_destroy(xz_cloud_t cloud);

/**
 * @brief 处理 WiFi 连接事件
 * @param cloud 小智云端句柄
 */
void xz_cloud_process_wifi_connected(xz_cloud_t cloud);

/**
 * @brief 处理 WiFi 断开事件
 * @param cloud 小智云端句柄
 */
void xz_cloud_process_wifi_disconnected(xz_cloud_t cloud);

/**
 * @brief 发送文本消息
 * @param txt 文本内容
 */
void xz_cloud_txt(const char *txt);

/**
 * @brief 发送 TTS 请求
 * @param text 文本内容
 */
void xz_cloud_tts(const char *text);

/**
 * @brief 检查连接状态
 * @return true 已连接, false 未连接
 */
bool xz_cloud_is_connected(void);

/**
 * @brief 检查 WiFi 连接状态
 * @return true 已连接, false 未连接
 */
bool xz_cloud_is_wifi_connected(void);

/**
 * @brief 写入音频数据
 * @param cloud 小智云端句柄
 * @param audio 音频数据 (PCM, 16kHz, 单声道, int16)
 * @param len 数据长度 (字节)
 */
void xz_cloud_audio(xz_cloud_t cloud, const char *audio, uint32_t len);

/**
 * @brief 唤醒语音助手 (开始交互, 相当于 F 键)
 * @param cloud 小智云端句柄
 */
void xz_cloud_wakeup(xz_cloud_t cloud);

/**
 * @brief 停止语音助手交互 (相当于 S 键)
 * @param cloud 小智云端句柄
 */
void xz_cloud_stop_interaction(xz_cloud_t cloud);

/**
 * @brief 检查是否正在交互
 * @param cloud 小智云端句柄
 * @return true 正在交互, false 未交互
 */
bool xz_cloud_is_interacting(xz_cloud_t cloud);

/**
 * @brief 获取云端句柄
 * @return 小智云端句柄
 */
xz_cloud_t xz_cloud_get_instance(void);

/**
 * @brief 执行 OTA 激活
 * @param server_url 激活服务器 URL (NULL 使用默认)
 * @return 0 成功, -1 失败
 */
int xz_cloud_activate(const char *server_url);

/**
 * @brief 检查是否已激活
 * @return true 已激活, false 未激活
 */
bool xz_cloud_is_activated(void);

#ifdef __cplusplus
}
#endif

#endif /* __XZ_CLOUD_H__ */
