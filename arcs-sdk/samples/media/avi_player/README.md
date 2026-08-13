# AVI Player

AVI 视频播放器示例，支持本地 SD 卡播放和在线 HTTP Range 流媒体播放，带触摸屏 LVGL UI。

## 功能

- 主菜单：本地播放、在线播放、USB MSC 文件管理
- 本地播放：浏览 SD 卡中的 `.avi` 文件并播放
- 在线播放：从 `.ini` 文件读取视频 URL 列表，自动连接 WiFi 后播放（仅支持 `.avi`）
- USB MSC：将 SD 卡挂载为 U 盘，方便电脑传文件

## 支持的媒体格式

- 视频：MJPEG（解码输出 RGB565）
- 音频：MP3（`WAVE_FORMAT_MPEGLAYER3 = 0x0055`）

## SD 卡目录结构

使用 FAT32 格式化的 SD 卡，按以下目录结构放置文件：

```
SD:/
├── Video/              # 本地视频文件目录
│   ├── demo.avi
│   └── sample.avi
├── URL/                # 在线视频 URL 配置目录
│   └── videos.ini      # 每行一个视频 URL
└── WiFi/               # WiFi 配置目录
    └── wifi.ini        # WiFi 连接配置
```

### 示例资源（`sd_res/` 目录，可直接拷到 SD 卡）

本 sample 在仓库里自带了一份 SD 卡内容模板，位于 `samples/media/avi_player/sd_res/`，
目录结构跟运行时期望完全一致：

```
sd_res/
├── Video/              (空目录 —— 大尺寸 .avi 不进仓库，用户自己填，见下方)
├── URL/
│   └── online_videos.ini   多条 HTTPS .avi 测试 URL（可直接用）
└── WiFi/
    └── wifi.ini            WiFi 配置占位示例（⚠️ 用前必改成你自己的 SSID/密码）
```

**使用方法**：把 FAT32 格式化好的 SD 卡插到电脑（读卡器，或在板子上点 USB MSC 进入
U 盘模式），把 `sd_res/` **目录里的内容**（是**内容**，不是 `sd_res` 目录本身）复制
到 SD 卡根目录。最终 SD 卡上应该出现 `Video/` / `URL/` / `WiFi/` 三个顶层目录。

示例资源只是仓库里的静态模板，**不会被固件编译进去**。

#### ⚠️ 两点使用前必做

**1. 放视频到 `sd_res/Video/`**

仓库里 `sd_res/Video/` 目录是故意留空的（大尺寸 `.avi` 不进 git，避免仓库膨胀）。
从以下任选一种补进去：

- **下载**：用 `sd_res/URL/online_videos.ini` 里列出的 `listenai-firmware-delivery`
  OSS 公网 URL 抓一份下来：

  ```sh
  cd samples/media/avi_player/sd_res/Video
  curl -L -O \
    https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/test/video/RealityEditor.avi
  ```

- **自己准备**：用 `tools/avi_encoder.py` 把 mp4 转成 MJPEG + MP3 的 `.avi`（这是
  sample 唯一支持的格式）。

  （`sd_res/Video/README.md` 里也写了相同的指引。）

**2. 改 `sd_res/WiFi/wifi.ini` 的 SSID/密码**

示例模板里的 `WIFI_SSID=test_ssid` / `WIFI_PWD=test_pwd` 是**占位符**，在线播放
（Online Play）功能会读这个文件连 Wi-Fi。一定要改成你实际可用的凭据：

```ini
WIFI_SSID=我的WiFi名称
WIFI_PWD=我的WiFi密码
```

不改的话点 Online Play 会提示 `WiFi failed!`。

### 本地播放

将 `.avi` 文件放入 SD 卡的 `Video/` 目录即可。进入主菜单 -> Local Videos，列表会自动扫描该目录下的所有 `.avi` 文件，点击即可播放。

### 在线播放

需要准备两个配置文件：

**1. WiFi 配置 - `WiFi/wifi.ini`**

```ini
WIFI_SSID=你的WiFi名称
WIFI_PWD=你的WiFi密码
```

**2. 视频 URL 列表 - `URL/` 目录下任意 `.ini` 文件**

每行一个视频 URL，支持 `http://` 和 `https://` 开头的链接。仅 `.avi` 后缀的 URL 可以播放，其他格式（如 `.mp4`）点击后会提示不支持。

示例 `URL/videos.ini`：

```ini
# 以 # 或 ; 开头的行为注释，空行会被忽略
https://example.com/Videos/demo_640x172_20fps.avi
https://example.com/Videos/sample_640x172_25fps.avi
```

进入主菜单 -> Online Videos，首次点击视频时会自动读取 WiFi 配置并连接，连接成功后开始播放。

### USB MSC 文件管理

进入主菜单 -> USB MSC，设备会将 SD 卡以 U 盘形式暴露给电脑，可以直接拖拽文件到对应目录。完成后点击 Back 返回主菜单。

## 编译

```sh
./build.sh -DBOARD=LS26840U_DEMO_IV1.1
```

## 关键配置（prj.conf）

| 配置项 | 说明 |
|--------|------|
| `CONFIG_AVI_PLAYER_AUDIO_MP3=y` | 启用 MP3 音频解码 |
| `CONFIG_AVI_PLAYER_HTTP_RANGE=y` | 启用 HTTP Range 在线播放 |
| `CONFIG_AVI_PLAYER_FIXED_FPS=y` | 启用固定帧率渲染 |
| `CONFIG_AVI_PLAYER_FIXED_FPS_VALUE=20` | 固定渲染帧率 |
| `CONFIG_AVI_PLAYER_MAX_VIDEO_AHEAD_MS=500` | 视频最大领先音频时间 |

### 没有声音时的排查

部分板卡需要外部 PA（功放）GPIO 拉起后才有声音输出。在 `prj.conf` 中启用：

```
CONFIG_LISA_AUDIO_PLAY_PA_ENABLE=y
```

并在 menuconfig 的 `Play (DAC) Configuration` 中按实际硬件引脚调整 PA 配置。

## 代码结构

```
src/
├── main.c              # 入口：硬件初始化、SD 卡挂载、UI 任务
├── app_context.c/h     # 全局上下文、屏幕切换
├── ui_main_menu.c/h    # 主菜单 UI
├── ui_local_list.c/h   # 本地视频列表 UI
├── ui_online_list.c/h  # 在线视频列表 UI
├── ui_playback.c/h     # 播放界面 UI + LVGL 视频渲染
├── ui_usb_msc.c/h      # USB MSC 界面
├── playback_mgr.c/h    # 播放管理（启动/停止播放任务）
├── fs_utils.c/h        # 文件系统工具（扫描文件、读取 URL/WiFi 配置）
└── wifi_utils.c/h      # WiFi 连接管理
```

## 局域网 HTTP 视频源

可使用自带工具在局域网内快速搭建视频流服务：

```sh
cd tools/
python3 ./avi_stream_server.py --port 10240
```
