<!-- type: worker -->
<!-- 开发规则: README.md#开发规则 -->

# ARCS SDK 文档编写助手

AI 读源码按规范生成/修复 SDK 文档的执行指令。

## 触发条件

- 用户要求新建、修复、更新 SDK 文档
- 用户反馈在线文档错误（提供 URL）
- 文档构建 warning/error 需要修复

---

## Step 1: 识别意图与文档类型

### 意图

| 意图 | 判断信号 | 路径 |
|------|----------|------|
| **新建文档** | 写/创建/添加 README、新驱动/组件/示例文档 | → Step 3A |
| **修复/更新文档** | 客户反馈 URL、文档内容有误、API 过时 | → Step 3B |
| **修复构建问题** | `make zh` 输出 WARNING/ERROR、CI 失败 | → Step 3C |

不明确时询问：是新建文档、修复在线文档错误，还是解决构建问题？

### 文档类型

| 类型 | 路径特征 |
|------|----------|
| 示例文档 | `samples/`、`demos/` |
| 模块文档 | `drivers/`、`components/`、`modules/` |
| 工具文档 | `tools/` |

---

## Step 2: 加载规范与上下文

```
① Read references/knowledge/doc-standards.md（对应类型的章节）
② Read references/experience/ 下相关模块经验文件（如存在）
```

### 文档验证环境前置检查

在执行文档构建验证前，先确认本机环境，而不是假设仓库已经携带完整工具链：

1. `docs/.venv` 只作为本地临时虚拟环境使用，不能提交到仓库
2. 文档严格构建至少依赖：
   - Python 虚拟环境中的 `Sphinx` 与 `docs/requirements.txt` 中的插件
   - 系统中的 `doxygen`
3. 若缺少 Python 依赖，优先在 `docs/` 下创建本地 venv：

```bash
cd docs
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
```

4. 若本机无 `make`，可直接使用 venv 里的 Python 调用 Sphinx
5. 若构建失败于 `No such file or directory: 'doxygen'`，优先判断为系统依赖缺失，而不是文档内容错误
6. 若 `-W` 失败，必须区分“本次修改新增的 warning”与“仓库已有 warning”，不能把已有 warning 误记到当前文档修改上

---

## Step 3A: 新建文档

### 3A-1: 读取源码

```
① Read  目标模块头文件（drivers/lisa_xxx/lisa_xxx.h 或 components/xxx/include/xxx.h）
   → 获取真实 API 函数签名、结构体、枚举，不要凭记忆写 API
② Read  Kconfig 文件 → 获取配置项
③ Read  实现文件（按需）→ 理解行为细节
```

### 3A-2: 读取风格参考

```
Glob 同类型目录下的 README.md，Read 1-2 个质量好的现有文档作为风格参考：
  - 驱动类 → drivers/lisa_uart/README.md
  - 示例类 → 同 category 下的其他 README.md
```

### 3A-3: 按模板生成

按 `doc-standards.md` 中对应类型的模板结构生成文档。

关键要求：
- API 签名必须从头文件复制，不可凭记忆
- 编译/烧录章节使用 RST 引用格式（示例文档）
- 不使用 emoji 标题

### 3A-4: 注册到索引

在对应的 `index_zh.rst` 中追加条目：

```rst
.. toctree::
    :maxdepth: 1

    existing/README.md
    <new_name>/README.md     ← 新增
```

各类型索引文件位置：

| 类型 | 索引文件 |
|------|----------|
| 驱动 | `drivers/index_zh.rst` |
| 组件 | `components/index_zh.rst` |
| 示例 | `samples/<category>/index_zh.rst` |
| Demo | `demos/index_zh.rst` |
| 板级 | `boards/index_zh.rst` |

若是示例新分类，先创建 `samples/<new_category>/index_zh.rst`，再在 `samples/index_zh.rst` 中注册。

### 3A-5: 构建验证（不可跳过）

```bash
cd docs && make zh SPHINXOPTS="-W" 2>&1 | grep -E "WARNING|ERROR"
```

期望：空输出（无新增 warning）。有 warning 则按 Step 3C 修复。

若本机无 `make`，可用等价命令：

```bash
cd docs && PATH="$(pwd)/.venv/bin:$PATH" .venv/bin/python -m sphinx.cmd.build -M html zh output/zh -W
```

---

## Step 3B: 修复/更新文档

### 3B-1: URL → 源文件定位

```
在线 URL 前缀（去掉）：
  https://docs2.listenai.com/arcs-sdk/latest/zh/html/

转换规则：
  <path>.html  →  <path>.md 或 <path>.rst
```

示例：

| 在线 URL（去掉前缀后） | SDK 源文件 |
|----------------------|------------|
| `drivers/lisa_uart/README.html` | `drivers/lisa_uart/README.md` |
| `samples/drivers/devices/lisa_gpio/output_basic/README.html` | `samples/drivers/devices/lisa_gpio/output_basic/README.md` |
| `get_started.html` | `docs/zh/get_started.rst` |

### 3B-2: 读取并对比

```
① Read  源文件（文档）
② Read  对应头文件/源码
③ 对比文档与源码，找出差异（API 签名、参数说明、配置项等）
```

### 3B-3: 错误类型与修复策略

| 错误类型 | 处理方式 |
|----------|----------|
| API 签名/参数说明有误 | Read 头文件获取正确签名，更新文档 |
| 代码示例无法编译 | 参考同类 sample 的 main.c，修正示例代码 |
| Kconfig 项不存在/名称错误 | Grep `CONFIG_` 在 Kconfig 中验证，修正 |
| 链接失效 | 改为有效路径或删除 |
| 内容缺失 | Read 头文件补充 |
| 中文表述有误 | 直接编辑修正 |

### 3B-4: 构建验证（不可跳过）

```bash
cd docs && make zh SPHINXOPTS="-W" 2>&1 | grep -E "WARNING|ERROR"
```

若本机无 `make`，可用等价命令：

```bash
cd docs && PATH="$(pwd)/.venv/bin:$PATH" .venv/bin/python -m sphinx.cmd.build -M html zh output/zh -W
```

---

## Step 3C: 修复构建问题

### 3C-1: 获取构建输出

若用户未提供，运行：
```bash
cd docs && make zh 2>&1 | grep -E "WARNING|ERROR"
```

若本机无 `make`，可先直接执行：

```bash
cd docs && PATH="$(pwd)/.venv/bin:$PATH" .venv/bin/python -m sphinx.cmd.build -M html zh output/zh
```

### 3C-2: 诊断对照表

| 警告/错误信息 | 根本原因 | 修复方法 |
|--------------|----------|----------|
| `document isn't included in any toctree` | 文件未在 `index_zh.rst` 注册 | 添加条目 |
| `Unknown directive type` | RST 指令名拼写错误 | 修正拼写 |
| `Malformed table` | RST 表格列宽不一致 | 对齐分隔行 |
| `image file not readable` | 图片路径失效 | 改用正确路径或移入 `docs/assets/` |
| `Pygments lexer not found for language` | 代码块语言标注非法 | 改为白名单语言 |
| `Unknown interpreted text role` | RST 角色不存在 | 删除或改为 MyST 语法 |
| `Title underline too short` | RST 标题下的 `===` 长度不足 | 补齐至与标题等长 |

有效代码块语言白名单：`c`、`kconfig`、`bash`、`ini`、`python`、`cmake`、`text`、`rst`、`json`、`yaml`

已被压制的 warning（不会导致 CI 失败，可忽略）：
```
myst.header, myst.domains, myst.xref_missing
ref.doc, ref.ref
toc.excluded, toc.not_readable, toc.not_included, toc.secnum, toc.circular
```

### 3C-3: 修复并验证

```bash
cd docs && make zh SPHINXOPTS="-W" 2>&1 | grep -E "WARNING|ERROR"
```

若本机无 `make`，可用等价命令：

```bash
cd docs && PATH="$(pwd)/.venv/bin:$PATH" .venv/bin/python -m sphinx.cmd.build -M html zh output/zh -W
```

---

## 文档系统参考

### 各类型文档的源文件位置

| 类型 | SDK 中的源文件位置 | 对应索引文件 |
|------|-------------------|-------------|
| 驱动 | `drivers/<lisa_xxx>/README.md` | `drivers/index_zh.rst` |
| 组件 | `components/<name>/README.md` | `components/index_zh.rst` |
| 示例 | `samples/<category>/<name>/README.md` | `samples/<category>/index_zh.rst` |
| Demo | `demos/<name>/README.md` | `demos/index_zh.rst` |
| 板级 | `boards/<board>/README.md` | `boards/index_zh.rst` |
| 顶层 | `docs/zh/*.rst` | `docs/zh/index.rst` |

`modules/` 目录下的 README 只有在 `conf.py` 的 `external_content_contents` 里显式列出才会被收录，不会自动全量收录。

### 明确被排除的路径

以下路径即使有 README.md 也不会被构建系统收录：
```
drivers/porting_docs/**
drivers/lisa_device/**
samples/drivers/hal/**/README.md
components/acomp/logger/**/README.md
demos/face_detect/src/button/FlexibleButton/README.md
```

### 构建命令

```bash
cd docs

# 普通构建
make zh

# CI 严格模式（提交前必须通过）
make zh SPHINXOPTS="-W"

# 只查看 warning/error
make zh 2>&1 | grep -E "^[^/].*(WARNING|ERROR)"

# 清理重建
make clean && make zh
```

若本机无 `make`，可使用：

```bash
cd docs

# 普通构建
PATH="$(pwd)/.venv/bin:$PATH" .venv/bin/python -m sphinx.cmd.build -M html zh output/zh

# CI 严格模式
PATH="$(pwd)/.venv/bin:$PATH" .venv/bin/python -m sphinx.cmd.build -M html zh output/zh -W
```

### external_content 收录机制

```
SDK 源文件（drivers/、components/、samples/ 等下的 *.md/*.rst）
    ↓  [external_content 扩展在构建时复制到 docs/zh/ 下]
docs/zh/<同路径>/（构建工作区，.gitignore 忽略）
    ↓  [Sphinx + MyST 处理]
docs/output/zh/html/（本地预览）
    ↓  [Read the Docs 部署]
https://docs2.listenai.com/arcs-sdk/latest/zh/html/
```

写好 README.md 后必须在 `index_zh.rst` 中注册，文档才会出现在文档站。

---

## Step 4: 经验沉淀

任务完成后，调用 experience-capture skill 记录值得保留的经验。
