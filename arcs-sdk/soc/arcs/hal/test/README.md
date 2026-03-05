# HAL 单元测试

本目录包含 HAL 层的单元测试。

## 依赖安装

### Unity 测试框架

```bash
git clone https://github.com/ThrowTheSwitch/Unity.git
cd Unity
mkdir build && cd build
cmake ..
sudo make install
```

## 构建测试

### 方法 1: 手动构建

```bash
cd test_xxx
cmake -B build -GNinja && cmake --build build
./build/tests
```

### 方法 2: 使用 CTest

```bash
cd test_xxx/build
ctest --verbose
```

## 测试框架

- **Unity**: C 语言单元测试框架
- **FFF (Fake Function Framework)**: Mock 框架

## 添加新测试

参考现有测试目录的结构:

```
test_your_feature/
├── CMakeLists.txt          # CMake 构建配置
├── README.md               # 测试说明文档
├── testcase.yaml           # 测试用例配置
├── test_your_feature.c     # 测试源文件
├── mock_*.c/h              # Mock 实现文件
└── stubs/                  # Stub 头文件目录
```
