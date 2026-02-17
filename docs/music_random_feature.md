# 随机播放音乐功能

## 功能概述

从在线音乐服务器 (http://192.168.1.169:9100) 随机选择一首歌曲进行播放。

## 实现文件

| 文件 | 描述 |
|-----|------|
| `src/cloud/mcp/tools/music_random.c` | 随机播放功能实现 |
| `src/cloud/CMakeLists.txt` | 已添加新文件到构建系统 |

## 工作流程

```
用户请求随机播放
        │
        ▼
┌─────────────────────────────────────────────────────────────────────┐
│  music_random_handler() [MCP工具入口]                    │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  1. 获取音频播放器实例 get_audio_player()    │    │
│  │  2. 调用 get_random_music_from_server()      │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
        │
        ▼
┌─────────────────────────────────────────────────────────────────────┐
│  get_random_music_from_server()                               │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  1. 构造请求URL:                                │    │
│  │     http://192.168.1.169:9100/api/list        │    │
│  │                                                         │    │
│  │  2. 初始化HTTP请求:                             │    │
│  │     lisa_http_init(LISA_HTTP_GET, url)             │    │
│  │                                                         │    │
│  │  3. 执行HTTP请求 lisa_http_perform()           │    │
│  │     接收响应数据到动态缓冲区                       │    │
│  │                                                         │    │
│  │  4. 解析JSON响应 cJSON_Parse()              │    │
│  │     提取 files[] 数组                              │    │
│  │                                                         │    │
│  │  5. 随机选择:                                    │    │
│  │     random_index = lisa_rand32() % count            │    │
│  │     selected_item = files[random_index]             │    │
│  │                                                         │    │
│  │  6. 填充 audio_out_t:                            │    │
│  │     m_url: 歌曲URL                               │    │
│  │     m_name: 歌曲名称(过滤特殊字符)                 │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
        │
        ▼
播放器调用: player->on_directive(player, AUDIO_PLAY, audio, 1)
        │
        ▼
┌─────────────────────────────────────────────────────────────────────┐
│  音频播放器处理 (audio_player.c)                         │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  1. 获取音频焦点 (CONTENT通道)                    │    │
│  │  2. 初始化播放列表 listen_playlist_init()             │    │
│  │  3. 调用 app_player_play(CLOUD, url, cb)       │    │
│  │  4. LisaPlayer 准备并播放                         │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
```

## API调用示例

### 请求获取音乐列表

```http
GET http://192.168.1.169:9100/api/list
```

### 响应

```json
{
  "total": 280,
  "files": [
    {
      "name": "001.马健涛-搀扶_DJ伟然版.mp3",
      "size": 1112213,
      "url": "http://192.168.1.169:9100/001.马健涛-搀扶_DJ伟然版.mp3"
    },
    ...
  ]
}
```

## 代码结构

### 主要函数

| 函数 | 描述 |
|-----|------|
| `music_random_handler()` | MCP工具入口，处理随机播放请求 |
| `get_random_music_from_server()` | 从服务器获取音乐列表并随机选择 |
| `http_data_callback()` | HTTP数据接收回调，支持动态扩容 |
| `generate_music_random_schema()` | 生成MCP工具的JSON Schema |

### 关键配置

```c
#define MUSIC_SERVER_BASE_URL "http://192.168.1.169:9100"
#define MUSIC_API_LIST "/api/list"
#define MUSIC_API_TIMEOUT_MS (10000)  // 10秒
#define HTTP_RESPONSE_BUFFER_SIZE (4096)
```

## 播放器集成

随机播放功能通过以下方式与播放器集成：

```c
// 获取播放器实例
audioplayer_t *player = get_audio_player();

// 构造音频资源
audio_out_t *audio = lisa_mem_alloc(1, sizeof(audio_out_t));
audio->m_url = "...";  // 从API获取
audio->m_name = "..."; // 歌曲名称

// 调用播放
player->on_directive(player, AUDIO_PLAY, audio, 1);
```

## 使用方式

### 方式1: 语音命令
```
用户: "随机播放一首歌"
系统: 识别到 intent → 调用 ls.builtin.play_random_music
```

### 方式2: 按键触发
在 `src/main.c` 的按键处理中添加：
```c
case LISA_BTN_PRESS_QUINTUPLE_CLICK:  // 六键
    // 调用随机播放
    break;
```

### 方式3: UI按钮
在 UI 界面添加随机播放按钮，点击后调用 MCP 工具。

## 响应示例

### 成功响应
```json
{
  "content": [
    {
      "type": "text",
      "text": "正在播放: 001.马健涛-搀扶_DJ伟然版.mp3"
    }
  ]
}
```

### 错误响应
```json
{
  "content": [
    {
      "type": "text",
      "text": "错误：无法从服务器获取音乐"
    }
  ]
}
```

## 依赖项

| 依赖 | 用途 |
|-----|------|
| `lisa_porting/net/lisa_http.h` | HTTP请求 |
| `player/audio_player.h` | 播放器接口 |
| `player/audio_out.h` | 音频数据结构 |
| `cJSON.h` | JSON解析 |
| `lisa_time.h` | 随机数生成 |

## 错误处理

1. **HTTP请求失败**: 记录错误日志，返回错误消息
2. **JSON解析失败**: 清理已分配资源，返回错误
3. **播放器未初始化**: 返回播放器未就绪错误
4. **内存分配失败**: 返回内存不足错误
5. **音乐列表为空**: 返回无可用音乐错误

## 注意事项

1. **URL编码**: API URL 已经硬编码，如需修改服务器地址请更新 `MUSIC_SERVER_BASE_URL`
2. **超时设置**: 默认10秒超时，可根据网络情况调整 `MUSIC_API_TIMEOUT_MS`
3. **缓冲区管理**: HTTP响应使用动态扩容缓冲区，初始大小4KB
4. **字符过滤**: 歌曲名称中的方括号 `[]` 和引号 `"` 会被过滤
5. **内存管理**: 播放器会管理 audio_out_t 的内存，无需手动释放

## 扩展建议

1. **搜索随机**: 支持 API 的 `?q=` 参数进行关键词搜索后随机
2. **缓存列表**: 缓存已获取的音乐列表，避免频繁请求
3. **偏好设置**: 支持用户设置随机范围或排除特定歌曲
4. **历史记录**: 记录已播放歌曲，避免重复播放
