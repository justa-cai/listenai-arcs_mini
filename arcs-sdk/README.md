# ARCS SDK

## 简介

ARCS SDK 是一个专为嵌入式系统设计的轻量级软件开发工具包，提供了完整的驱动、组件库和 RTOS 支持。

### 主要特性

- **丰富的驱动支持**：提供 UART、GPIO、I2C、SPI、ADC/DAC、PWM、DMA、Flash、Display、Camera、Touch 等 20+ 外设驱动
- **RTOS 集成**：深度集成 FreeRTOS，提供统一的 RTOS 抽象层
- **图形界面**：内置 LVGL GUI 框架（v7/v8），支持多种 LCD/EPD 显示设备
- **网络功能**：支持 HTTP/HTTPS、WebSocket、MQTT 等网络协议
- **多媒体能力**：支持 MP3、AAC 音频解码，JPEG、PNG、GIF 图像处理
- **USB 支持**：集成 TinyUSB 协议栈
- **算法服务**：提供 OCR、TTS、翻译、拼读、语音唤醒等算法与服务能力接口。
- **开发工具**：Shell、日志、单元测试、死机回溯等

## 目录结构
```
arcs-sdk/
├── boards/                            ← 板级文件
├── soc/                               ← soc相关文件                             
├── startup/                           ← 系统启动相关初始化
├── drivers/                           ← 驱动文件 
├── cmake/                             ← CMake构建扩展 
├── components/                        ← 自研软件库
├── modules/                           ← 第三方开源库 
├── tools/                             ← 开发工具集
├── samples/                           ← 示例代码 
├── tests/                             ← 测试代码
├── docs/                              ← 文档构建 
├── README.md                          ← 仓库文档
├── VERSION                            ← 版本信息
├── LICENSE                            ← 开源许可证
```

---

## 📚 资源链接

| 资源类型 | 链接 |
|---------|------|
| **在线文档** | https://docs2.listenai.com/arcs-sdk/latest/zh/html/index.html |
| **引脚配置工具** | https://tool.listenai.com/ls-pinmux-tool |