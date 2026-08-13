---
orphan: true
---

# Video/ — 本地视频放这里

**这个目录在仓库里故意留空**，以免把大尺寸 `.avi` 文件提交进 git。

## 怎么填视频

任选一种：

### A. 从 OSS 下载现成的测试视频

在 `../URL/online_videos.ini` 里列出的 URL 都是 `listenai-firmware-delivery`
公开 OSS 上的 MJPEG + MP3 `.avi`，任意用 `curl` / `wget` 抓下来放这里即可：

```sh
curl -L -O \
  https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/test/video/RealityEditor.avi
```

### B. 放自己的视频

用 `tools/` 里的脚本把 mp4 转成 MJPEG+MP3 的 `.avi`（sample 支持的格式）：

```sh
cd ../../tools/
python3 ./avi_encoder.py <input> <output>.avi
```

（或其他工具，保证是 MJPEG 视频流 + MP3 音频流）

## 放置方式

把 `.avi` 文件直接放在本目录，然后把**整个 `sd_res/` 的内容**拷到 SD 卡根目录。
上电后进入主菜单 → **Local Play**，会自动扫描 `/SD:/Video/` 下所有 `.avi` 列出来。
