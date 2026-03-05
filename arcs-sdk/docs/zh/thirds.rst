.. _thirds:

第三方库支持
############

ARCS SDK 集成了多个第三方开源库，以提供丰富的功能支持。以下是当前支持的第三方库列表。

********

实时操作系统 (RTOS)
============================

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - 库名称
     - 说明
   * - FreeRTOS
     - 经典的实时操作系统内核，提供任务调度、队列、信号量等核心功能
   * - FreeRTOS-CPP11
     - FreeRTOS 的 C++11 封装库，提供面向对象的 RTOS 接口
   * - rtos_al
     - RTOS 抽象层，提供统一的 RTOS API 接口

图形界面
============================

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - 库名称
     - 说明
   * - LVGL
     - 轻量级图形库，用于嵌入式系统的 GUI 开发
   * - LVGL8
     - LVGL 版本 8.x，提供更新的图形界面功能

网络协议
============================

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - 库名称
     - 说明
   * - coreHTTP
     - 轻量级 HTTP 客户端库，适用于嵌入式设备
   * - coreSNTP
     - 简单网络时间协议 (SNTP) 客户端实现
   * - libcurl
     - 强大的网络传输库，支持多种协议
   * - httpclient
     - HTTP 客户端实现
   * - http_ssl
     - 支持 SSL/TLS 的 HTTP 客户端
   * - nopoll
     - WebSocket 客户端和服务器库
   * - mbedtls
     - 轻量级 SSL/TLS 加密库
   * - coreMQTT
     - 轻量级的MQTT客户端库，适用于嵌入式设备
   * - coreMQTT-Agent
     - 针对coreMQTT接口的线程安全封装库

数据格式与解析
============================

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - 库名称
     - 说明
   * - cJSON
     - 轻量级 JSON 解析库
   * - libxml2
     - XML 解析和处理库

文件系统
============================

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - 库名称
     - 说明
   * - filesystem
     - 文件系统支持
   * - fs
     - 文件系统抽象层
   * - EasyFlash
     - 嵌入式 Flash 存储管理库，提供 KV 数据库、日志存储等功能

音频处理
============================

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - 库名称
     - 说明
   * - libmad
     - MPEG 音频解码器库
   * - libid3tag
     - ID3 标签解析库，用于读取音频文件元数据
   * - mp3dec
     - MP3 解码器
   * - aacdec
     - AAC 音频解码器
   * - speexdsp
     - Speex 数字信号处理库，提供音频处理功能
   * - resample
     - 音频重采样库
   * - lisa_player
     - 音频播放器实现

图像处理
============================

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - 库名称
     - 说明
   * - ijg
     - Independent JPEG Group 的 JPEG 编解码库
   * - libpng
     - PNG 图像格式处理库
   * - giflib
     - GIF 图像格式处理库
   * - libico
     - ICO 图标格式处理库
   * - freetype
     - 字体渲染引擎

工具库
============================

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - 库名称
     - 说明
   * - collections-c
     - C 语言数据结构集合库
   * - crc32
     - CRC32 校验和计算库
   * - zlib
     - 数据压缩库
   * - letter-shell
     - 嵌入式 Shell 命令行工具
   * - easylogger
     - 轻量级日志系统
   * - mempool
     - 内存池管理库
   * - heap
     - 堆内存管理实现
   * - ltlsf
     - TLSF (Two-Level Segregated Fit) 内存分配器
   * - uchardet
     - 字符编码检测库
   * - csk_sqlite3
     - SQLite3 数据库引擎

系统管理
============================

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - 库名称
     - 说明
   * - mac_manager
     - MAC 地址管理模块
   * - wifi_manager
     - Wi-Fi 连接管理模块

测试框架
============================

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - 库名称
     - 说明
   * - Unity
     - C 语言单元测试框架
   * - CppUTest
     - C/C++ 单元测试框架
   * - GoogleTest
     - Google 的 C++ 测试框架
   * - FFF
     - Fake Function Framework，C 语言函数 Mock 框架，用于单元测试

其他
============================

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - 库名称
     - 说明
   * - cpr
     - C++ HTTP 请求库
   * - tinyusb
     - 轻量级 USB 协议栈
   * - flexlayout
     - 灵活的布局引擎

********

使用说明
============================

这些第三方库已经集成到 ARCS SDK 的构建系统中，可以通过 Kconfig 配置启用所需的库。

具体的使用方法和 API 文档请参考各个库的官方文档或查看 ``modules/`` 目录下对应库的 README 文件。
