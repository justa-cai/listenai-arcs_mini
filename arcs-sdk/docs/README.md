# 本地文档构建指南

本文档指导您如何在本地环境中构建和生成项目文档。

## 文档架构说明

本项目采用**Sphinx + Doxygen**混合架构生成多语言技术文档，支持中英文双语输出。

### 架构组成

```
docs/
├── zh/                     # 中文文档源文件
│   ├── conf.py            # Sphinx中文配置文件
│   ├── index.rst          # 中文文档主页
│   ├── get_started.rst    # 快速开始指南
│   ├── components/        # 组件文档
│   └── api_doc.md         # API文档入口
├── doxygen/               # Doxygen配置与定制
│   ├── Doxyfile          # Doxygen配置文件
│   ├── mainpage.md       # API文档主页
│   └── custom/           # 自定义样式和模板
├── _ext/                  # Sphinx扩展插件
│   ├── doxyrunner.py     # Doxygen集成插件
│   └── external_content.py # 外部内容处理
├── assets/                # 静态资源文件
├── requirements.txt       # Python依赖列表
├── Makefile              # 构建配置
└── README.md             # 本指南
```

### 文档生成流程

1. **Sphinx处理**：处理RST/MD格式的用户文档
2. **Doxygen集成**：自动提取代码注释生成API文档
3. **多语言支持**：通过`make en`/`make zh`生成对应语言版本
4. **输出统一**：最终在`output/`目录生成完整的HTML文档

### 技术特点

- **🌐 多语言支持**：中英文双语文档生成
- **📚 混合文档**：结合手写文档和自动API文档
- **🔧 可扩展**：通过`_ext/`目录的插件系统支持自定义功能
- **📱 响应式**：使用Read the Docs主题，支持移动端阅读

## 环境准备

### 安装依赖

在开始构建文档之前，需要先安装必要的Python依赖包：

```bash
pip install --user -r requirements.txt
```

## 构建文档

### 生成文档

1. 切换到项目的 `docs` 目录：
   ```bash
   cd docs
   ```

2. 执行构建命令：

   **构建所有语言版本（推荐）：**
   ```bash
   make
   ```

   **仅构建中文文档：**
   ```bash
   make zh
   ```

   **仅构建英文文档 (暂不支持)：**
   ```bash
   make en
   ```

   **清理构建缓存：**
   ```bash
   make clean
   ```

### 查看生成结果

文档构建完成后，生成的文档文件将保存在 `output` 目录中：

```
output/
├── zh/           # 中文文档
│   └── html/
│       └── index.html
└── en/           # 英文文档
    └── html/
        └── index.html
```

您可以通过浏览器打开对应语言目录下的 `index.html` 文件来查看文档内容。

### 故障排除

**常见问题：**

- **依赖缺失**：确保已安装 `requirements.txt` 中的所有依赖包
- **权限问题**：如果使用 `--user` 安装依赖后仍有问题，可尝试使用虚拟环境
- **构建失败**：使用 `make clean` 清理缓存后重新构建

## 📚 文档贡献

### 贡献指南

如果您想为 ARCS SDK 贡献文档，请参阅 [文档贡献指南](CONTRIBUTING.md)，其中包含：

- **组件文档编写规范**：标准模板和编写要求
- **示例文档编写指南**：示例项目的文档化要求  
- **文档索引管理**：如何正确添加新文档到索引
- **质量检查清单**：确保文档质量的检查要点
- **提交流程**：从编写到发布的完整流程

### 主要贡献领域

- **组件文档** (`components/`): 为SDK组件编写完整的使用文档
- **示例文档** (`samples/`): 为示例项目提供清晰的说明文档
- **API文档**: 通过代码注释改进自动生成的API文档

