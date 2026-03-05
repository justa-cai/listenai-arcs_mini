# 开发流程（TDD）

本模块采用 TDD（测试驱动开发）流程，建议步骤如下：

1. **定义行为**：明确需求与边界条件，写出期望的输入/输出。
2. **编写失败用例**：先写单元测试，确保在未修复前必定失败。
3. **实现最小改动**：修改业务代码，直到测试通过。
4. **回归验证**：运行相关测试集，确保无回归。
5. **整理与文档**：必要时更新说明文档与注释。

## 运行测试

- WiFi Manager 核心单测：
  ```bash
  cmake -B build_wm_mgr -G Ninja -S modules/wifi_manager/test/wifi_manager \
    && cmake --build build_wm_mgr \
    && ./build_wm_mgr/tests
  ```

- WiFi Manager ARCS 适配单测：
  ```bash
  cmake -B build_wm_arcs -G Ninja -S modules/wifi_manager/test/wifi_manager_arcs \
    && cmake --build build_wm_arcs \
    && ./build_wm_arcs/tests
  ```

- Storage 单测：
  ```bash
  cmake -B build_wm_storage -G Ninja -S modules/wifi_manager/test/wifi_manager_storage \
    && cmake --build build_wm_storage \
    && ./build_wm_storage/tests
  ```
