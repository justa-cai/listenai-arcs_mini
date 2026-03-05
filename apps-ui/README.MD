# 说明

### linux模拟器开发

1. 进入到sim目录,生成assets并编译运行

首次编译或修改图片资源后，需要先生成assets：

```bash
cd sim
# 生成assets C文件和头文件
python3 ../assets/assets.py ../assets/embed build/generated/assets

# 配置和编译
cmake -B build && cmake --build build

# 运行模拟器
./build/lisa_ui_app_sim
```

后续修改代码（非图片资源）可以直接编译运行：

```bash
cmake --build build && ./build/lisa_ui_app_sim
```

### arcs开发

按照正常的arcs工程编译即可
