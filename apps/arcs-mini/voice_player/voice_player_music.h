#ifndef VOICE_PLAYER_MUSIC_H
#define VOICE_PLAYER_MUSIC_H

/**
 * @file voice_player_music.h
 * @brief 在线音乐播放业务的应用内接口。
 */

/**
 * @brief 初始化音乐业务并订阅云端歌单、播放控制消息。
 * @return 0 初始化成功，其他值表示失败。
 */
int voice_player_music_init(void);

/**
 * @brief 播放音乐 URL，播放 /SD:/ 本地文件前会先确认路径可访问。
 * @param url 音乐 URL 或本地文件路径。
 * @return APP_PLAYER_OK 成功，其他播放器错误码表示失败。
 */
int voice_player_play_music_url(const char *url);

#endif /* VOICE_PLAYER_MUSIC_H */
