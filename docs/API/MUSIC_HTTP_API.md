# MP3 音乐资源服务 & 图片生成服务 API 文档

## 服务信息

| 项目 | 说明 |
|------|------|
| 服务地址 | `http://192.168.1.169:9100` | 音乐服务 + 图片生成服务 |
| 协议 | HTTP |
| 编码 | UTF-8 |
| 数据格式 | JSON |

---

## API 接口说明

所有接口均返回**短 ID**，避免中文字符 URL 编码问题。

### 1. 获取 MP3 资源列表

获取所有 MP3 文件列表，支持递归搜索子目录。

**接口地址：** `GET /api/list`

**请求参数：** 无

**请求示例：**

```bash
# 获取所有 MP3 列表（包括子目录）
curl "http://192.168.1.169:9100/api/list"
```

**响应示例：**

```json
{
  "total": 280,
  "files": [
    {
      "id": 1,
      "name": "马健涛-搀扶_DJ伟然版",
      "size": 1112213,
      "image": "http://192.168.1.169:9100/api/image/123"
    },
    {
      "id": 2,
      "name": "LBI利比（时柏尘）-跳楼机_DJHZ版",
      "size": 1326455,
      "image": null
    }
  ]
}
```

---

### 2. 搜索 MP3 歌曲

根据关键词搜索 MP3 歌曲，支持递归搜索所有子目录。

**接口地址：** `GET /api/search`

**请求参数：**

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| q | string | 是 | 搜索关键词，支持模糊匹配文件名（不区分大小写） |

**请求示例：**

```bash
# 搜索包含 "DJ" 的歌曲
curl "http://192.168.1.169:9100/api/search?q=DJ"

# 搜索 "爱情" 相关歌曲
curl "http://192.168.1.169:9100/api/search?q=爱情"

# 搜索 "周深" 的歌曲
curl "http://192.168.1.169:9100/api/search?q=周深"
```

**响应示例：**

```json
{
  "total": 5,
  "files": [
    {
      "id": 12,
      "name": "马健涛-搀扶_DJ伟然版",
      "size": 1112213,
      "image": "http://192.168.1.169:9100/api/image/456"
    },
    {
      "id": 23,
      "name": "LBI利比-跳楼机_DJHZ版",
      "size": 1326455,
      "image": null
    }
  ]
}
```

**响应字段说明：**

| 字段名 | 类型 | 说明 |
|--------|------|------|
| total | number | 文件总数 |
| files | array | 文件列表 |
| files[].id | number | 歌曲短 ID（用于下载） |
| files[].name | string | 文件名（中文） |
| files[].size | number | 文件大小（字节） |
| files[].image | string/null | 歌曲封面图片短链接（如果存在）|

---

### 2. 下载 MP3 文件

根据短 ID 下载指定的 MP3 文件，避免中文字符 URL 编码问题。

**接口地址：** `GET /api/download/{id}`

**请求示例：**

```bash
# 下载 ID 为 1 的歌曲
curl "http://192.168.1.169:9100/api/download/1" -o song.mp3

# 下载 ID 为 5 的歌曲
curl "http://192.168.1.169:9100/api/download/5" -o song05.mp3
```

**响应：** 返回 MP3 文件流（Content-Type: audio/mpeg）

**错误响应：**

```json
HTTP/1.0 404 Not Found
{
  "error": "Song not found"
}
```

---

### 3. 下载 MP3 文件

根据短 ID 下载指定的 MP3 文件，支持子目录中的文件。

**接口地址：** `GET /api/download/{id}`

**请求示例：**

```bash
# 下载 ID 为 1 的歌曲
curl "http://192.168.1.169:9100/api/download/1" -o song.mp3

# 下载 ID 为 5 的歌曲
curl "http://192.168.1.169:9100/api/download/5" -o song05.mp3"
```

**响应：** 返回 MP3 文件流（Content-Type: audio/mpeg）

---

### 4. 获取随机歌曲

随机返回一首 MP3 歌曲信息（包括子目录），支持按关键词过滤。

**接口地址：** `GET /api/random`

**请求参数：**

| 参数名 | 类型 | 必填 | 说明 |
|--------|------|------|------|
| q | string | 否 | 搜索关键词，随机返回匹配该关键词的歌曲 |

**请求示例：**

```bash
# 获取随机歌曲（全局随机）
curl "http://192.168.1.169:9100/api/random"

# 从包含 "DJ" 的歌曲中随机选择一首
curl "http://192.168.1.169:9100/api/random?q=DJ"

# 从包含 "爱情" 的歌曲中随机选择一首
curl "http://192.168.1.169:9100/api/random?q=爱情"
```

**响应示例：**

```json
{
  "id": 56,
  "name": "我记得（珍藏版）",
  "size": 1320081,
  "image": "http://192.168.1.169:9100/api/image/789"
}
```

**无匹配结果时响应：**

```json
{
  "id": null,
  "name": null,
  "size": null
}
```

```json
{
  "id": 56,
  "name": "我记得（珍藏版）",
  "size": 1320081
}
```

**响应字段说明：**

| 字段名 | 类型 | 说明 |
|--------|------|------|
| id | number | 随机歌曲 ID |
| name | string | 随机文件名 |
| size | number | 文件大小（字节） |
| image | string/null | 歌曲封面图片地址（如果存在）|

---

## API 接口总览

| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/list` | GET | 获取所有 MP3 列表（递归搜索子目录） |
| `/api/search?q=关键词` | GET | 搜索 MP3 歌曲（递归搜索子目录） |
| `/api/random` | GET | 获取随机歌曲（支持?q=关键词过滤） |
| `/api/download/{id}` | GET | 根据 ID 下载歌曲 |
| `/api/image/get/{相对路径}` | GET | 获取歌曲封面图片（PNG，240x240） |

---

## HTTP 状态码

| 状态码 | 说明 |
|--------|------|
| 200 | 请求成功 |
| 400 | 请求参数错误 |
| 404 | 文件/歌曲不存在 |
| 500 | 服务器错误 |

---

## 使用示例

### Python 示例

```python
import requests

BASE_URL = "http://192.168.1.169:9100"

# 获取所有音乐列表
response = requests.get(f"{BASE_URL}/api/list")
data = response.json()

print(f"共有 {data['total']} 首歌曲")

# 搜索歌曲
keyword = "爱情"
response = requests.get(f"{BASE_URL}/api/search", params={"q": keyword})
songs = response.json()['files']

for song in songs:
    print(f"[{song['id']}] {song['name']} ({song['size']} bytes)")
    if song.get('image'):
        print(f"    封面: {song['image']}")

# 下载歌曲（使用短 ID）
if songs:
    song_id = songs[0]['id']
    download_url = f"{BASE_URL}/api/download/{song_id}"
    song_data = requests.get(download_url).content
    with open(songs[0]['name'] + ".mp3", 'wb') as f:
        f.write(song_data)
    print(f"已下载: {songs[0]['name']}.mp3")

# 下载封面图片
    if songs[0].get('image'):
        image_url = songs[0]['image']
        image_data = requests.get(image_url).content
        with open(f"{songs[0]['name']}_cover.png", 'wb') as f:
            f.write(image_data)
        print(f"已下载封面: {songs[0]['name']}_cover.png")
```

### JavaScript/Fetch 示例

```javascript
const BASE_URL = "http://192.168.1.169:9100";

// 获取所有音乐列表
async function getSongs() {
  const url = `${BASE_URL}/api/list`;
  const response = await fetch(url);
  const data = await response.json();

  console.log(`共有 ${data.total} 首歌曲`);
  return data.files;
}

// 搜索歌曲
async function searchSongs(keyword) {
  const url = `${BASE_URL}/api/search?q=${encodeURIComponent(keyword)}`;
  const response = await fetch(url);
  const data = await response.json();

  console.log(`找到 ${data.total} 首歌曲`);
  return data.files;
}

// 搜索并显示
searchSongs('DJ').then(songs => {
  songs.forEach(song => {
    console.log(`[${song.id}] ${song.name}`);
    if (song.image) {
      console.log(`  封面: ${song.image}`);
    }
  });
});

// 播放音乐（HTML5 Audio）
function playSong(songId) {
  const audio = new Audio(`${BASE_URL}/api/download/${songId}`);
  audio.play();
}

// 显示歌曲封面
function showCover(imageUrl) {
  if (!imageUrl) return;
  
  const img = document.createElement('img');
  img.src = imageUrl;
  img.style.maxWidth = '240px';
  img.style.maxHeight = '240px';
  document.body.appendChild(img);
}
```

### C/C++ libcurl 示例

```c
#include <curl/curl.h>
#include <stdio.h>

void download_song(int song_id) {
    CURL *curl;
    FILE *fp;
    char url[256];
    char filename[] = "song.mp3";

    snprintf(url, sizeof(url), "http://192.168.1.169:9100/api/download/%d", song_id);

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    fp = fopen(filename, "wb");
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(fp);

    printf("下载完成: %s\n", filename);
}
```

---

## 错误处理

### 歌曲不存在

```json
HTTP/1.0 404 Not Found
Content-Type: application/json; charset=utf-8
Content-Length: 27

{
  "error": "Song not found"
}
```

### 无效的歌曲 ID

```json
HTTP/1.0 400 Bad Request
Content-Type: application/json; charset=utf-8
Content-Length: 28

{
  "error": "Invalid song ID"
}
```

---

## 注意事项

1. **短 ID 机制**：所有歌曲通过数字 ID 标识，避免中文字符编码问题
2. **下载方式**：使用 `/api/download/{id}` 下载，而非直接访问文件名
3. **跨域支持**：接口已添加 CORS 头，支持跨域请求
4. **Content-Length**：所有响应均包含 `Content-Length` 头，方便 IoT 设备处理
5. **递归搜索**：支持递归搜索所有子目录中的 MP3 文件
6. **歌名清理**：返回的歌名已自动去掉数字前缀和 `.mp3` 后缀，下载时需自行添加 `.mp3`
7. **歌曲封面**：
   - MP3 文件同目录下的 PNG 图片会自动作为封面返回
   - 封面图片尺寸统一为 240x240 像素
   - 图片路径格式：`/api/image/{短ID}`
   - 如果没有对应的 PNG 图片，image 字段返回 null
8. **图片缓存**：图片访问支持 7 天缓存

---

## 更新日志

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0.0 | 2025-01-01 | 初始版本，支持列表查询和文件下载 |
| 1.1.0 | 2025-01-02 | 新增短 ID 机制，新增随机歌曲接口 |
| 1.2.0 | 2025-02-14 | 支持递归搜索子目录，新增搜索接口，歌名自动清理 |
| 1.3.0 | 2025-02-14 | 新增混合智能搜索（传统+AI） |
| 1.4.0 | 2025-02-14 | 新增图片生成服务（/api/image/generate） |
| 1.5.0 | 2025-02-14 | 图片自动缩放到 240x240 像素 |
| 1.6.0 | 2025-02-18 | 新增歌曲封面图片返回（image字段），图片改为短链接格式（/api/image/{短ID}) |
| 1.7.0 | 2025-02-18 | 图片接口更新，支持直接访问 MP3 目录下的 PNG 图片 |

---

# 图片服务

## 服务信息

| 项目 | 说明 |
|------|------|
| 服务地址 | `http://192.168.1.169:9100` |
| 协议 | HTTP |
| 编码 | UTF-8 |
| 图片格式 | PNG |
| 图片尺寸 | 240x240 像素 |

---

## API 接口说明

### 1. 获取歌曲封面图片

根据文件路径获取歌曲封面图片（PNG格式，240x240像素）。

**接口地址：** `GET /api/image/get/{相对路径}`

**请求示例：**

```bash
# 获取 DJ 目录下的歌曲封面
curl "http://192.168.1.169:9100/api/image/get/dj/151.留什么给你小葡萄_DJ版.png"

# 获取其他目录的歌曲封面
curl "http://192.168.1.169:9100/api/image/get/周杰伦/稻香.png"
```

**响应：** 返回图片文件流（Content-Type: image/png）

**错误响应：**

```json
HTTP/1.0 404 Not Found
{
  "error": "Image not found"
}
```

---

## 图片服务总览

| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/image/{短ID}` | GET | 获取歌曲封面图片（PNG，240x240） |

---

## 图片服务使用示例

### Python 示例

```python
import requests

BASE_URL = "http://192.168.1.169:9100"

# 从歌曲列表获取封面 URL
response = requests.get(f"{BASE_URL}/api/search", params={"q": "爱情"})
songs = response.json()['files']

if songs and songs[0].get('image'):
    # 获取封面图片
    image_url = songs[0]['image']
    image_data = requests.get(image_url).content
    
    # 保存图片
    with open(f"{songs[0]['name']}_cover.png", 'wb') as f:
        f.write(image_data)
    
    print(f"已下载封面: {songs[0]['name']}_cover.png")
else:
    print("该歌曲没有封面图片")
```

### JavaScript/Fetch 示例

```javascript
const BASE_URL = "http://192.168.1.169:9100";

// 获取歌曲封面
async function getCoverImage(song) {
  if (!song.image) {
    console.log('该歌曲没有封面图片');
    return;
  }

   const response = await fetch(song.image);
   const blob = await response.blob();
   
   // 创建图片元素显示
   const img = document.createElement('img');
   img.src = URL.createObjectURL(blob);
   img.style.maxWidth = '240px';
   img.style.maxHeight = '240px';
   document.body.appendChild(img);
  
   console.log('封面图片加载成功 (短链接)');
}

// 使用示例
async function main() {
  const response = await fetch(`${BASE_URL}/api/search?q=爱情`);
  const data = await response.json();
  
  if (data.files.length > 0) {
    await getCoverImage(data.files[0]);
  }
}

main();
```

**响应示例：**

```json
{
  "success": true,
  "images": [
    {
      "url": "http://192.168.1.169:9100/api/image/get/img_12345_0_1739500000.png",
      "filename": "img_12345_0_1739500000.png",
      "index": 0,
      "size": 1024576
    }
  ],
  "parameters": {
    "model": "black-forest-labs/FLUX.1-dev",
    "prompt": "a beautiful sunset over the ocean, with seagulls flying",
    "negative_prompt": "",
    "image_size": "1024x1024",
    "batch_size": 1,
    "num_inference_steps": 20,
    "guidance_scale": 7.5,
    "seed": 12345
  },
  "timings": {
    "inference": 5.467
  }
}
```

**响应字段说明：**

| 字段名 | 类型 | 说明 |
|--------|------|------|
| images[].url | string | 图片的内部访问地址（已缓存到本地） |
| images[].filename | string | 图片文件名 |
| images[].index | number | 图片索引 |
| images[].size | number | 图片文件大小（字节） |
| images[].error | string | 下载失败时的错误信息（可选） |

**图片缓存机制：**
- 生成的图片自动下载并缓存到服务器本地
- **图片自动缩放到 240x240 像素**
- 返回的 URL 为内部地址，格式：`http://192.168.1.169:9100/api/image/get/{filename}`
- 图片缓存有效期：7 天
- 适合内网环境，无需访问外网
- 缩放算法：LANCZOS（高质量缩放）

---

### 2. 获取歌曲封面图片

根据图片短ID获取歌曲封面图片（PNG格式，240x240像素）。

**接口地址：** `GET /api/image/{短ID}`

**请求示例：**

```bash
# 获取 ID 为 123 的图片
curl "http://192.168.1.169:9100/api/image/123"

# 获取 ID 为 456 的图片
curl "http://192.168.1.169:9100/api/image/456"
```

**响应：** 返回图片文件流（Content-Type: image/png）

**错误响应：**

```json
HTTP/1.0 404 Not Found
{
  "error": "Image not found"
}
```

---

## 图片生成 API 总览

| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/image/generate` | POST | 根据文本生成并缓存图片 |
| `/api/image/get/{filename}` | GET | 获取缓存的图片 |

---

## 图片生成使用示例

### Python 示例

```python
import requests
import urllib.request

BASE_URL = "http://192.168.1.169:9100"

# 生成图片
response = requests.post(
    f"{BASE_URL}/api/image/generate",
    json={
        "prompt": "a cat sitting on a windowsill, looking at the rain",
        "model": "black-forest-labs/FLUX.1-dev",
        "image_size": "1024x1024",
        "batch_size": 1
    }
)

data = response.json()

if data['success']:
    # 图片已缓存到本地，直接使用内部 URL
    image_url = data['images'][0]['url']
    filename = data['images'][0]['filename']
    
    print(f"图片内部地址: {image_url}")
    
    # 在网页中显示图片
    print(f"<img src='{image_url}' />")
    
    # 下载图片（可选）
    urllib.request.urlretrieve(image_url, filename)
    print(f"图片已保存: {filename}")
    
    print(f"推理时间: {data['timings']['inference']}s")
else:
    print(f"生成失败: {data.get('error')}")
```

### JavaScript/Fetch 示例

```javascript
const BASE_URL = "http://192.168.1.169:9100";

// 生成图片
async function generateImage() {
  const response = await fetch(`${BASE_URL}/api/image/generate`, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify({
      prompt: 'a peaceful mountain landscape at sunrise',
      image_size: '1024x1024',
      batch_size: 1
    })
  });

  const data = await response.json();

  if (data.success) {
    // 图片已缓存到本地，直接使用内部 URL
    const imageUrl = data.images[0].url;
    const filename = data.images[0].filename;
    
    console.log('图片内部地址:', imageUrl);
    
    // 显示图片
    const img = document.createElement('img');
    img.src = imageUrl;
    document.body.appendChild(img);
    
    // 下载图片（可选）
    const link = document.createElement('a');
    link.href = imageUrl;
    link.download = filename;
    link.click();
  } else {
    console.error('生成失败:', data.error);
  }
}

generateImage();
```

---

## 图片生成常见参数说明

### Prompt（提示词）优化技巧

1. **具体描述**：
   - ✅ 好: "a cat sitting on a windowsill, orange fur, green eyes"
   - ❌ 差: "a cat"

2. **风格指定**：
   - "in the style of Van Gogh"
   - "anime style"
   - "photorealistic"

3. **负面提示词（negative_prompt）**：
   - "blur, low quality, distorted"
   - "watermark, text, logo"

### 模型选择

- **Qwen/Qwen-Image-Edit-2509**: 适合通用场景，支持多种尺寸
- **Kolors/Kolors**: 适合艺术创作，色彩表现力强

