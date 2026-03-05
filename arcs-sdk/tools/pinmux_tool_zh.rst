.. _pinmux_tool:

====================================
引脚复用配置工具
====================================

LISA Pinmux Tool 是一个基于 Web 的可视化引脚复用配置工具，用于帮助开发者快速配置 ARCS 芯片的引脚功能和外设映射，并自动生成对应的 C 语言配置文件。

工具地址
========

访问 `LISA Pinmux Tool <https://tool.listenai.com/ls-pinmux-tool/>`_ 开始使用。

.. note::
   使用本工具需要网络连接，建议使用现代浏览器（Chrome、Firefox、Edge 等）以获得最佳体验。

工具功能概述
============

LISA Pinmux Tool 提供以下主要功能：

- **可视化引脚配置**：通过图形界面直观地配置芯片引脚功能
- **外设映射管理**：支持配置多种外设（UART、I2C、SPI、PWM、GPIO、DVP、SDIO 等）的引脚映射
- **引脚别名定义**：为常用引脚定义有意义的别名，提高代码可读性
- **代码自动生成**：一键生成符合 ARCS SDK 规范的 pinmux.h 和 pinmux.c 文件
- **项目管理**：支持创建、保存、导入和导出配置项目
- **多平台支持**：支持不同软件开发包版本、芯片型号和封装类型

快速开始
========

创建新项目
----------

1. **访问工具网站**

   在浏览器中打开 `LISA Pinmux Tool <https://tool.listenai.com/ls-pinmux-tool/>`_

2. **创建项目**

   根据实际情况选择以下两种方式之一：

   **方案 A：填写参数创建项目**

   适用场景：自定义配置

   .. raw:: html

      <video width="100%" controls loop muted style="max-width: 800px; margin: 20px auto; display: block;">
        <source src="../_static/pinmux_tool_create_project_PlanA.mp4" type="video/mp4">
        您的浏览器不支持视频标签。
      </video>

   在"创建项目"区域填写以下信息：

   - **软件开发包**：选择当前使用的 SDK 版本（如 v0.1.0、v0.1.1 等）
   - **芯片型号**：选择目标芯片型号（如 LS2664 等）
   - **封装类型**：选择芯片封装类型（如 QFN76 等）
   - **项目名称**：为项目起一个有意义的名称

   **方案 B：使用现有模版创建项目**

   适用场景：使用标准开发板

   .. raw:: html

      <video width="100%" controls loop muted style="max-width: 800px; margin: 20px auto; display: block;">
        <source src="../_static/pinmux_tool_create_project_PlanB.mp4" type="video/mp4">
        您的浏览器不支持视频标签。
      </video>

   根据手中开发板选择对应的模版：

   - 直接选择与您开发板型号匹配的模版
   - 可点击模版中的照片来辅助确认开发板型号
   - 快速启动项目，无需手动配置参数

   完成参数填写或模版选择后，点击"创建项目"按钮，系统将创建一个新的配置项目。

导入现有项目
------------

如果您已有之前保存的配置，可以通过以下方式导入：

1. 点击项目列表中的"导入"按钮
2. 上传之前导出的配置文件zip包
3. 配置将自动加载到工具中

配置引脚功能
============

引脚配置界面
------------

工具提供三种配置方式：

方式一：从驱动选择引脚
~~~~~~~~~~~~~~~~~~~~~~

在侧边栏选择外设驱动，然后为该驱动配置引脚。

.. raw:: html

   <video width="100%" controls loop muted style="max-width: 800px; margin: 20px auto; display: block;">
     <source src="../_static/pinmux_tool_sidebar_driver.mp4" type="video/mp4">
     您的浏览器不支持视频标签。
   </video>

方式二：从引脚选择驱动
~~~~~~~~~~~~~~~~~~~~~~

在侧边栏选择引脚，然后为该引脚配置对应的外设驱动。

.. raw:: html

   <video width="100%" controls loop muted style="max-width: 800px; margin: 20px auto; display: block;">
     <source src="../_static/pinmux_tool_sidebar_pin.mp4" type="video/mp4">
     您的浏览器不支持视频标签。
   </video>

方式三：交互式配置
~~~~~~~~~~~~~~~~~~

直接在芯片图片上点击引脚，为其配置外设驱动，并可设置引脚别名。

.. raw:: html

   <video width="100%" controls loop muted style="max-width: 800px; margin: 20px auto; display: block;">
     <source src="../_static/pinmux_tool_interactive_config.mp4" type="video/mp4">
     您的浏览器不支持视频标签。
   </video>

生成代码文件
============

生成配置文件
------------
.. raw:: html

   <video width="100%" controls loop muted style="max-width: 800px; margin: 20px auto; display: block;">
     <source src="../_static/pinmux_tool_export.mp4" type="video/mp4">
     您的浏览器不支持视频标签。
   </video>

配置完成后，预览代码，符合预期效果后点击工具中右上角的"导出"按钮，系统将自动生成以下文件：

- **pinmux.h**：引脚复用配置头文件
- **pinmux.c**：引脚复用配置实现文件
- **projects_data.json**：项目配置文件

.. note::
   生成的pinmux.h和pinmux.c文件可以直接替换到项目的 ``boards/<board_name>/`` 目录下。

项目管理
========

保存项目
--------

配置完成后，建议保存项目配置：

1. 点击"导出"或"保存"按钮
2. 将配置文件保存到本地
3. 建议将配置文件纳入版本控制系统管理

导入项目
--------

1. 点击"导入"按钮
2. 选择之前保存的配置文件
3. 配置将自动加载到工具中

刷新项目列表
------------

点击"刷新"按钮可以刷新项目列表，查看最新的项目信息。


生成的文件结构
--------------

pinmux.h 文件内容
~~~~~~~~~~~~~~~~~

生成的头文件包含以下内容：

- **引脚别名宏定义**：所有在工具中定义的引脚别名都会被生成为宏定义
- **外设 pinmux 函数声明**：所有外设的引脚复用函数声明

示例：

.. code-block:: c

   // Pin aliases
   #define LCD_PWM_PIN 0
   #define LCD_RST_PIN 1
   #define CP_LOG_RX_PIN 2
   #define CP_LOG_TX_PIN 3
   #define AP_LOG_TX_PIN 21
   #define TP_INT_PIN 24
   #define TP_RST_PIN 25
   #define PA_EN_PIN 27
   #define LCD_TE_PIN 8
   #define LED_PIN 9
   #define CAMERA_PWDN_PIN 7
   #define LCD_CD_PIN 0

   // Function declarations
   void lisa_adc_pinmux();
   void lisa_uart0_pinmux();
   void lisa_i2c0_pinmux();
   void lisa_spi0_pinmux();
   void lisa_pwm_pinmux();
   // ... 其他外设函数

pinmux.c 文件内容
~~~~~~~~~~~~~~~~~

生成的实现文件包含以下内容：

- **所有外设的 pinmux 函数实现**：配置了引脚的外设会有实际的配置代码，未配置的外设函数体为空
- **weak 属性**：所有函数使用 ``__attribute__((weak))`` 修饰，允许用户重写

示例：

.. code-block:: c

   __attribute__((weak)) void lisa_uart0_pinmux()
   {
       IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CP_LOG_RX_PIN, 2);
       IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CP_LOG_TX_PIN, 2);
   }

   __attribute__((weak)) void lisa_i2c0_pinmux()
   {
       IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, 8);
       IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 23, 8);
   }

   __attribute__((weak)) void lisa_pwm_pinmux()
   {
       IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, LCD_PWM_PIN, 12);
   }

   // 未配置的外设函数体为空
   __attribute__((weak)) void lisa_i2c1_pinmux()
   {
   }


使用小tips
----------

**使用引脚别名**：在代码中使用工具中定义的引脚别名（如 ``LCD_RST_PIN``、``LED_PIN``），而不是直接使用数字，可以提高代码可读性和可维护性。

**自定义 pinmux 函数**：由于生成的函数都使用 ``weak`` 属性，您可以在自己的代码中重新实现这些函数以覆盖默认配置。自定义实现不需要 ``weak`` 属性，链接器会自动选择非 weak 的实现。

示例：

.. code-block:: c

   #include "pinmux.h"

   // 使用引脚别名，提高可读性
   void configure_lcd(void)
   {
       gpio_set_pin(LCD_RST_PIN, 1);
       gpio_set_pin(LCD_TE_PIN, 0);
       gpio_set_pin(LCD_PWM_PIN, 0);
   }

   // 自定义 pinmux 函数，覆盖默认配置
   void lisa_uart0_pinmux()
   {
       IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 4, 2);
       IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 5, 2);
   }


注意事项
========

配置前的准备
------------

- **确认芯片型号和封装**：确保选择正确的芯片型号和封装类型，不同封装可用的引脚可能不同
- **查看原理图**：配置前建议查看开发板原理图，确认实际硬件连接
- **检查引脚冲突**：确保同一引脚不会被多个外设使用

配置时的建议
------------

- **使用引脚别名**：为常用引脚设置有意义的别名，提高代码可读性
- **分组管理外设**：按照功能模块分组配置外设，便于管理
- **保留备份**：修改配置前建议先保存或导出当前配置

生成文件的使用
--------------

- **不要手动修改生成的文件**：建议统一使用工具生成，手动修改可能导致与工具不同步
- **使用 weak 函数重写**：如需自定义配置，请在用户代码中重写函数，而不是直接修改生成的文件
- **版本控制**：将生成的 pinmux.h 和 pinmux.c 纳入版本控制，但可以考虑将配置文件（JSON）也一并管理

常见问题
========

Q: 生成的代码与现有代码冲突怎么办？
------------------------------------

A: 生成的文件使用 weak 属性，您可以在用户代码中重新实现这些函数。如果仍有冲突，请检查函数名是否正确。

Q: 如何修改已生成文件的配置？
--------------------------------

A: 推荐做法是在工具中修改配置后重新生成文件。如果需要临时修改，可以在用户代码中重新实现对应的 pinmux 函数。

Q: 引脚功能编号如何确定？
--------------------------

A: 引脚功能编号由芯片硬件决定，工具会根据选择的芯片型号自动处理。如果需要了解具体编号，可以查看生成的代码或芯片数据手册。

Q: 是否支持多板型配置？
------------------------

A: 可以为不同的板型创建不同的项目，每个项目对应一套 pinmux 配置。生成的文件需要放在对应板型的 ``boards/<board_name>/`` 目录下。

Q: 如何回退到之前的配置？
--------------------------

A: 如果之前导出了配置文件，可以通过导入功能恢复。如果使用了版本控制，可以从版本历史中恢复之前的文件。

