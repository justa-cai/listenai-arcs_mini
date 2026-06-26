#ifndef VOICE_MUSIC_LIST_H
#define VOICE_MUSIC_LIST_H


#ifdef __cplusplus
extern "C" {
#endif


#define AUIDO_OUT_UTF8_CN_BYTES_MAX (3)
#define AUIDO_OUT_LOCAL_PREFIX_LEN (11) /* "/SD:/audio/" */
#define AUIDO_OUT_LOCAL_DIR_DEPTH_MAX (3)
#define AUIDO_OUT_LOCAL_DIR_NAME_CHARS_MAX (32)
#define AUIDO_OUT_LOCAL_FILE_NAME_CHARS_MAX (64)
#define AUIDO_OUT_LOCAL_EXT_BYTES_MAX (8)

#define AUIDO_OUT_NAME_LEN \
	((AUIDO_OUT_LOCAL_FILE_NAME_CHARS_MAX * AUIDO_OUT_UTF8_CN_BYTES_MAX) + \
	 AUIDO_OUT_LOCAL_EXT_BYTES_MAX + 1)

#define AUIDO_OUT_SD_PATH_LEN \
	(AUIDO_OUT_LOCAL_PREFIX_LEN + \
	 (AUIDO_OUT_LOCAL_DIR_DEPTH_MAX * \
	  ((AUIDO_OUT_LOCAL_DIR_NAME_CHARS_MAX * AUIDO_OUT_UTF8_CN_BYTES_MAX) + 1)) + \
	 (AUIDO_OUT_NAME_LEN - 1) + 1)

#define AUIDO_OUT_URL_LEN (512)
#define AUIDO_OUT_MID_LEN (128)
#define AUIDO_OUT_TITLE_LEN (64)
#define AUIDO_OUT_ARTIST_LEN (32)
#define AUIDO_OUT_ALBUM_LEN (32)

#if AUIDO_OUT_SD_PATH_LEN > AUIDO_OUT_URL_LEN
#error "AUIDO_OUT_URL_LEN is too small for the configured SD path limit"
#endif

/** @brief 音乐曲目项 */
typedef struct music_item_s {
	char m_url[AUIDO_OUT_URL_LEN];       /**< 文件路径或云端 URL */
	char mid[AUIDO_OUT_MID_LEN];         /**< 音乐 ID */
	char m_name[AUIDO_OUT_NAME_LEN];     /**< 文件名 */
	char m_title[AUIDO_OUT_TITLE_LEN];   /**< ID3 标题（为空时用 m_name） */
	char m_artist[AUIDO_OUT_ARTIST_LEN]; /**< ID3 艺术家 */
	char m_album[AUIDO_OUT_ALBUM_LEN];   /**< ID3 专辑 */
} music_item_t;

/**
 * @brief 音乐列表类型
 *
 * ONLINE / OFFLINE 为独立子列表，ALL 为跨列表遍历模式。
 * 新增列表在 MUSIC_LIST_COUNT 之前添加即可自动扩展。
 */
typedef enum {
    MUSIC_LIST_ONLINE = 0,   /**< 在线音乐（云端URL） */
    MUSIC_LIST_OFFLINE,      /**< 离线音乐（TF卡本地文件） */
    MUSIC_LIST_COUNT,        /**< 内部标记：真实列表数量 */
    MUSIC_LIST_ALL = MUSIC_LIST_COUNT,  /**< 跨列表遍历（所有列表） */
} voice_music_list_id_t;

/**
 * @brief 播放模式
 *
 * 控制列表播放完毕后的行为。
 * SEQUENTIAL / LOOP_ALL / SHUFFLE 在 active_list == ALL 时跨列表遍历。
 */
typedef enum {
    MUSIC_MODE_ONCE = 0,       /**< 单次播放：播完当前曲目后停止 */
    MUSIC_MODE_REPEAT_ONE,     /**< 单曲循环：重复播放当前曲目 */
    MUSIC_MODE_SEQUENTIAL,     /**< 顺序播放：当前列表内顺序播完即停 */
    MUSIC_MODE_LOOP_ALL,       /**< 列表循环：播完最后一首后回到第一首 */
    MUSIC_MODE_SHUFFLE,        /**< 随机播放：打乱后播放 */
} voice_music_mode_t;

/**
 * @brief   初始化音乐列表模块
 * @return  0 成功，-1 失败
 * @note    从 PSRAM 分配上下文，子列表内存在 set 时按需分配。
 */
int voice_music_list_init(void);

/**
 * @brief   设置指定列表的音乐列表（替换该列表的旧列表）
 * @param   id 曲目列表（ONLINE / OFFLINE，不能为 ALL）
 * @param   items  曲目数组
 * @param   count  曲目数量
 * @return  0 成功，-1 失败
 * @note    释放该列表旧列表后分配新列表，自动切换 active_list 为该列表。
 */
int voice_music_list_set(voice_music_list_id_t id, music_item_t *items, int count);

/**
 * @brief   清空音乐列表
 * @param   id 要清空的列表，传 MUSIC_LIST_ALL 清空全部
 * @return  0 成功，-1 失败
 */
int voice_music_list_clear(voice_music_list_id_t id);

/**
 * @brief   获取指定索引的曲目（在 active_list 子列表中查找）
 * @param   index 曲目索引（0-based，相对于 active_list 子列表）
 * @param   out   输出参数
 * @return  0 成功，-1 失败
 * @note    active_list == ALL 时按 ONLINE→OFFLINE 顺序跨列表查找
 */
int voice_music_list_get_by_index(int index, music_item_t *out);

/**
 * @brief   获取当前正在播放的曲目
 * @param   out 输出参数
 * @return  0 成功，-1 失败（尚未播放或列表为空）
 * @note    直接按 playing_list 子列表的 current_index 获取
 */
int voice_music_list_get_current(music_item_t *out);

/**
 * @brief   获取下一首曲目（根据当前播放模式自动推进）
 * @param   out 输出参数
 * @return  0 成功，-1 失败（列表为空或无可播放曲目）
 * @note    active_list == ALL 时跨列表遍历：当前列表到尾后切换到下一列表。
 *          mode 为 ONCE/REPEAT_ONE 的行为不受 ALL 影响。
 */
int voice_music_list_get_next(music_item_t *out);

/**
 * @brief   获取上一首曲目
 * @param   out 输出参数
 * @return  0 成功，-1 失败（列表为空或无可播放曲目）
 * @note    active_list == ALL 时跨列表后退。
 */
int voice_music_list_get_prev(music_item_t *out);

/**
 * @brief   获取当前 active_list 下的曲目总数
 * @return  曲目数量
 * @note    active_list == ALL 时返回所有列表的总数
 */
int voice_music_list_count(void);

/**
 * @brief   获取当前播放索引（相对于 playing_list 子列表）
 * @return  当前索引（-1 表示尚未播放）
 */
int voice_music_list_current_index(void);

/**
 * @brief   设置当前播放索引（相对于 playing_list 子列表）
 * @param   index 目标索引（0-based，-1 表示重置）
 * @note    调用后 get_current 返回该索引对应的曲目，通常配合 push(intent) 使用
 */
void voice_music_list_set_current_index(int index);

/**
 * @brief   设置活跃列表
 * @param   id 列表类型，ALL 启用跨列表遍历
 */
void voice_music_list_set_active(voice_music_list_id_t id);

/**
 * @brief   设置播放模式
 * @param   mode 播放模式
 */
void voice_music_list_set_mode(voice_music_mode_t mode);

/**
 * @brief   获取当前活跃列表
 * @return  当前列表设置
 */
voice_music_list_id_t voice_music_list_get_active(void);

/**
 * @brief   获取当前播放模式
 * @return  当前播放模式
 */
voice_music_mode_t voice_music_list_get_mode(void);

#ifdef __cplusplus
}
#endif

#endif
