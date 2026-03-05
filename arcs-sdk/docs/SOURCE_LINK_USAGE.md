# 源码链接自动生成功能使用说明

## 功能概述

该功能通过 Sphinx 扩展自动在示例文档页面顶部添加源码位置信息,解决了在线文档无法知道示例源码路径的问题。

### 效果展示

当用户访问示例文档时,会在页面顶部看到一个美观的源码位置提示框:

```
┌────────────────────────────────────────────────────────┐
│ 📁 源码位置: samples/drivers/devices/lisa_uart/send_sync_int │
│                                          [查看源码]     │
└────────────────────────────────────────────────────────┘
```

点击"查看源码"按钮可以直接跳转到 GitHub/GitLab 仓库对应位置。

## 实现方案

### 1. 核心组件

- **Sphinx 扩展**: `docs/_ext/source_link.py`
  - 自动检测示例文档
  - 提取源码路径
  - 注入链接节点到文档树

- **CSS 样式**: `docs/assets/source-link.css`
  - 提供美观的视觉效果
  - 支持响应式设计
  - 支持深色模式

- **配置文件**: `docs/zh/conf.py`
  - 集成扩展
  - 配置仓库地址和匹配规则

### 2. 工作流程

```
┌──────────────┐
│ 示例 README.md│
└──────┬───────┘
       │
       │ 1. external_content 复制到 docs/zh/
       ▼
┌──────────────┐
│  Sphinx 构建  │
└──────┬───────┘
       │
       │ 2. source_link 扩展检测并处理
       ▼
┌──────────────┐
│ doctree 注入  │
│  源码链接节点  │
└──────┬───────┘
       │
       │ 3. HTML 生成
       ▼
┌──────────────┐
│  最终文档页面  │
│ (含源码链接)  │
└──────────────┘
```

## 配置说明

### 基本配置

在 `docs/zh/conf.py` 中已添加以下配置:

```python
# 添加扩展
extensions = [
    # ... 其他扩展
    "source_link"  # 自动添加源码链接
]

# 源码链接配置
source_link_base_url = os.environ.get(
    "SOURCE_LINK_BASE_URL",
    ""  # 留空则只显示本地路径
)

source_link_patterns = [
    "samples/**/*.md",
    "samples/**/*.rst",
    "demos/**/*.md",
    "demos/**/*.rst",
]

source_link_show_local_path = True
source_link_label = "📁 源码位置"
```

### 配置选项详解

#### `source_link_base_url`
- **用途**: 配置 GitHub/GitLab 仓库地址
- **格式**: `https://仓库地址/-/tree/分支名` 或 `https://仓库地址/tree/分支名`
- **示例**:
  - GitLab: `https://cloud.listenai.com/CSKG836746/arcs-sdk/public/arcs-sdk/-/tree/master`
  - GitHub: `https://github.com/listenai/arcs-sdk/tree/master`
- **说明**:
  - 如果设置,会在本地路径旁显示"查看源码"按钮
  - 如果不设置,只显示本地路径
  - 支持通过环境变量 `SOURCE_LINK_BASE_URL` 动态配置
  - 分支名会根据文档版本自动替换(见 `source_link_version_branches`)

#### `source_link_version_branches`
- **用途**: 文档版本到 Git 分支的映射
- **格式**: 字典,键为文档版本,值为对应的 Git 分支名
- **示例**:
  ```python
  source_link_version_branches = {
      'latest': 'master',
      'v0.1.0': 'release/v0.1.0',
      'v0.1.1': 'release/v0.1.1',
  }
  ```
- **说明**:
  - 自动根据文档版本切换源码链接指向的分支
  - 如果版本不在映射中,默认规则:
    - `latest` → `master`
    - `v0.1.0` → `release/v0.1.0` (自动添加 `release/` 前缀)
  - 确保源码链接始终指向对应版本的代码

#### `source_link_patterns`
- **用途**: 指定哪些文档需要添加源码链接
- **格式**: 文件路径通配符列表
- **默认值**: `["samples/**/*.md", "samples/**/*.rst"]`
- **说明**:
  - 支持通配符 `**` 匹配任意层级目录
  - 可以添加更多模式,如 `demos/**/*.md`

#### `source_link_show_local_path`
- **用途**: 是否显示本地路径
- **类型**: Boolean
- **默认值**: `True`
- **说明**:
  - `True`: 显示完整本地路径
  - `False`: 只显示"查看源码"按钮(需要配置 base_url)

#### `source_link_label`
- **用途**: 自定义标签文本
- **类型**: String
- **默认值**: `"📁 源码位置"`
- **说明**: 可以改为其他文本,如 `"🔗 源代码"`, `"📂 Source"`

## 使用方法

### 方法 1: 版本感知 + 本地路径(推荐,当前配置)

适用于有多版本文档的情况,自动根据文档版本链接到对应分支。

**配置**:
```python
source_link_base_url = "https://cloud.listenai.com/CSKG836746/arcs-sdk/public/arcs-sdk/-/tree/master"
source_link_show_local_path = True
source_link_version_branches = {
    'latest': 'master',
    'v0.1.0': 'release/v0.1.0',
    'v0.1.1': 'release/v0.1.1',
}
```

**效果**:
- **latest 版本文档**: `📁 源码位置: samples/... [查看源码]` → 链接到 `master` 分支
- **v0.1.0 版本文档**: `📁 源码位置: samples/... [查看源码]` → 链接到 `release/v0.1.0` 分支
- **v0.1.1 版本文档**: `📁 源码位置: samples/... [查看源码]` → 链接到 `release/v0.1.1` 分支

### 方法 2: 仅显示本地路径

适用于内部文档或没有公开仓库的情况。

**配置**:
```python
source_link_base_url = ""
source_link_show_local_path = True
```

**效果**:
```
📁 源码位置: samples/drivers/devices/lisa_uart/send_sync_int
```

### 方法 3: 固定分支链接

适用于只有单一版本或所有版本共用同一分支的情况。

**配置**:
```python
source_link_base_url = "https://github.com/listenai/arcs-sdk/tree/master"
source_link_show_local_path = True
# 不设置 source_link_version_branches
```

**效果**:
```
📁 源码位置: samples/drivers/devices/lisa_uart/send_sync_int [查看源码]
```
(所有版本都链接到 master 分支)

### 方法 4: 仅显示链接按钮

适用于希望极简显示的情况。

**配置**:
```python
source_link_base_url = "https://github.com/listenai/arcs-sdk/tree/master"
source_link_show_local_path = False
```

**效果**:
```
📁 源码位置: [查看源码]
```

## 构建和测试

### 本地测试

1. **设置仓库地址**(可选):
```bash
export SOURCE_LINK_BASE_URL="https://github.com/listenai/arcs-sdk/tree/master"
```

2. **构建文档**:
```bash
cd docs
make clean
make html
```

3. **查看效果**:
```bash
# 在浏览器中打开
firefox zh/_build/html/samples/drivers/devices/lisa_uart/send_sync_int/README.html
```

### CI/CD 集成

在构建脚本或 CI 配置中添加环境变量:

```yaml
# .github/workflows/docs.yml
- name: Build docs
  env:
    SOURCE_LINK_BASE_URL: "https://github.com/${{ github.repository }}/tree/${{ github.ref_name }}"
  run: |
    cd docs
    make html
```

或 GitLab CI:

```yaml
# .gitlab-ci.yml
build-docs:
  variables:
    SOURCE_LINK_BASE_URL: "https://gitlab.com/$CI_PROJECT_PATH/-/tree/$CI_COMMIT_REF_NAME"
  script:
    - cd docs
    - make html
```

## 自定义样式

如果需要修改源码链接框的外观,编辑 `docs/assets/source-link.css`:

### 预设的简洁样式

文件中包含了一个备选的简洁样式(已注释),如需使用:

1. 注释掉当前的渐变样式
2. 取消注释简洁样式部分

### 自定义颜色

修改 CSS 中的颜色变量:

```css
.source-link-box {
    background: linear-gradient(135deg, #YOUR_COLOR_1 0%, #YOUR_COLOR_2 100%);
    border-left-color: #YOUR_BORDER_COLOR;
}
```

## 扩展到其他文档类型

如果需要为其他类型的文档添加源码链接,只需在配置中添加对应的模式:

```python
source_link_patterns = [
    "samples/**/*.md",
    "samples/**/*.rst",
    "demos/**/*.md",
    "demos/**/*.rst",
    "components/**/*.md",      # 组件文档
    "drivers/**/*.md",         # 驱动文档
    "boards/**/*.md",          # 板级文档
]
```

## 故障排查

### 源码链接没有显示

1. **检查文档路径是否匹配模式**:
   ```python
   # 确认 source_link_patterns 包含对应的路径模式
   ```

2. **检查扩展是否正确加载**:
   ```bash
   # 查看构建日志中是否有 source_link 相关的错误
   make html 2>&1 | grep source_link
   ```

3. **检查 CSS 是否加载**:
   ```bash
   # 确认 HTML 中包含 source-link.css
   grep "source-link.css" zh/_build/html/samples/**/README.html
   ```

### 链接路径不正确

1. **检查 SDK_BASE 环境变量**:
   ```python
   # 在 conf.py 中确认
   print(f"SDK_BASE: {SDK_BASE}")
   ```

2. **检查 external_content 配置**:
   ```python
   # 确认文件被正确复制到 docs/zh/
   ```

### 样式显示异常

1. **清理构建缓存**:
   ```bash
   cd docs
   make clean
   make html
   ```

2. **检查浏览器缓存**:
   - 强制刷新页面 (Ctrl+Shift+R)
   - 或清除浏览器缓存

## 优势总结

### ✅ 自动化
- 无需手动修改每个 README.md
- 构建时自动处理
- 维护成本低

### ✅ 通用性
- 适用于所有示例文档
- 支持多种仓库(GitHub/GitLab/Gitee)
- 可扩展到其他文档类型

### ✅ 可配置
- 灵活的显示选项
- 支持环境变量配置
- 易于集成到 CI/CD

### ✅ 用户友好
- 美观的视觉效果
- 一键跳转到源码
- 响应式设计,支持移动端

## 后续优化建议

1. **添加多语言支持**:
   ```python
   source_link_label_zh = "📁 源码位置"
   source_link_label_en = "📁 Source Location"
   ```

2. **支持多仓库**:
   ```python
   source_link_repos = {
       "samples": "https://github.com/org/samples",
       "demos": "https://github.com/org/demos",
   }
   ```

3. **添加编辑链接**:
   在源码链接旁添加"编辑此页"按钮,方便贡献者直接编辑文档。

4. **集成版本信息**:
   根据文档版本自动切换 GitHub 分支或 tag。

## 技术支持

如有问题或建议,请联系文档团队或提交 Issue。
