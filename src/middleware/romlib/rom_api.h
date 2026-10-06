/*
 * ROM 库查询 HTTP 客户端。
 *
 * PC 端 pad_gui.py 同时提供只读 ROM API (默认 :38202, 见 doc/pad-http-api.md),
 * 本模块负责: ① 推导 API 地址; ② 带关键词查询 /api/roms 并解析成结构化候选。
 *
 * 仅供独立工作线程同步调用 (内部走阻塞 HTTP, 不要放在 voice.ebus / BT task)。
 */
#ifndef ROM_API_H
#define ROM_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ROM_API_ITEM_NAME_LEN 128
#define ROM_API_ITEM_PATH_LEN 192
#define ROM_API_ITEM_EXT_LEN  8

#define ROM_API_MAPPER_INVALID (-1)

/* 返回值 (解释性错误码, 便于工具侧给用户可读提示) */
#define ROM_API_OK          0
#define ROM_API_ERR_PARAM   (-1)    /* 参数非法 / 关键词或路径过长 */
#define ROM_API_ERR_NO_BASE (-2)    /* 没有可用的 API 地址 (未配置且未确认到 PC 地址) */
#define ROM_API_ERR_NET     (-3)    /* 连接/请求失败 (pad_gui 未启动或不在同一网段) */
#define ROM_API_ERR_HTTP    (-4)    /* HTTP 实例创建失败 */
#define ROM_API_ERR_PARSE   (-5)    /* 响应不是预期的 JSON 结构 */
#define ROM_API_ERR_NOMEM   (-6)
#define ROM_API_ERR_ABORTED (-7)    /* 下载回调主动中断 (调用方已判定失败) */
#define ROM_API_ERR_NOFILE  (-8)    /* 服务端没有这个 ROM (404) */

typedef struct {
    char name[ROM_API_ITEM_NAME_LEN];       /* 文件名 (不含目录) */
    char rel_path[ROM_API_ITEM_PATH_LEN];   /* 相对 ROM 根目录的路径 (= API 的 id) */
    char ext[ROM_API_ITEM_EXT_LEN];         /* 小写扩展名, 如 "nes" */
    uint32_t size_kb;                       /* 文件大小 (KB, 向下取整) */
    int mapper;                             /* iNES mapper; 未解析为 ROM_API_MAPPER_INVALID */
} rom_api_item_t;

typedef struct {
    rom_api_item_t *items;  /* 候选列表, 由 lisa_mem_calloc 分配 */
    uint32_t count;         /* items 实际条数 */
    uint32_t total;         /* ROM 库总量 */
    uint32_t matched;       /* 关键词命中总数 (可能大于 count, 即被 limit 截断) */
} rom_api_result_t;

typedef struct {
    char name[ROM_API_ITEM_NAME_LEN];
    char rel_path[ROM_API_ITEM_PATH_LEN];
    char ext[ROM_API_ITEM_EXT_LEN];
    uint32_t size_bytes;                    /* 精确字节数 (加载前需要, 见 rom_api_stat) */
    int mapper;                             /* iNES mapper; 未解析为 ROM_API_MAPPER_INVALID */
} rom_api_stat_t;

/* 查询单个 ROM 的详情 (GET /api/roms/{id})。rel_path 即搜索结果的 rel_path。
 * 用于取精确 size_bytes —— 加载前必须先知道大小才能分配暂存缓冲。 */
int rom_api_stat(const char *rel_path, rom_api_stat_t *out);

/* 下载分块回调: 返回非 0 表示调用方已判定失败 (之后不再回调, 但仍会把响应读完) */
typedef int (*rom_api_chunk_cb_t)(const uint8_t *data, uint32_t len, void *user);

/* 流式下载 ROM 原始字节 (GET /api/roms/{id}/download), 每块经 cb 交给调用方。
 * 不做缓存: 大 ROM 不会在内存里额外保存一份。
 * out_bytes 非空时写入实际收到的总字节数。 */
int rom_api_download(const char *rel_path, rom_api_chunk_cb_t cb, void *user, uint32_t *out_bytes);

/* 解析出 API base URL (形如 "http://192.168.31.205:38202"), 成功返回 ROM_API_OK。
 * 顺序: ① KV user.rom_api_url (允许填裸 host:port, 自动补 http:// 与默认端口);
 *       ② 回退到设备自己确认到的 PC 端地址 (UDP 发现来源 / WS 会话对端)。 */
int rom_api_resolve_base(char *buf, size_t len);

/* 按关键词查询 ROM 库 (文件名/相对路径子串匹配, 服务端不区分大小写)。
 * 同步阻塞 (数秒), 供独立工作线程调用。成功返回 ROM_API_OK 并填 out。 */
int rom_api_search(const char *query, uint32_t limit, rom_api_result_t *out);

/* 释放 rom_api_search 填出的 items; 可重复调用。 */
void rom_api_result_free(rom_api_result_t *r);

#ifdef __cplusplus
}
#endif

#endif /* ROM_API_H */
