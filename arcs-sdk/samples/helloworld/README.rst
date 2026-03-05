Hello World 示例
=================

功能说明
--------

这是一个最简单的 ARCS SDK 示例，演示了如何使用 LISA 日志系统输出 "Hello, world!" 消息。本示例用于验证开发环境配置和基本的日志输出功能。

硬件连接
--------

无需外部连接，仅需要开发板正常工作即可。

示例内容
--------

1. 定义日志标签（LOG_TAG）
2. 包含 LISA 日志头文件
3. 使用日志宏输出 "Hello, world!" 消息

.. include:: /sample_build.rst

.. include:: /sample_flash.rst

预期输出
--------

.. code-block:: text

   [I][logger_sample] Hello, world!

核心 API
--------

.. list-table::
   :header-rows: 1

   * - API
     - 说明
   * - ``LOGI()``
     - LISA 日志系统的信息级别日志宏
   * - ``LOG_TAG``
     - 日志标签宏，用于标识日志来源

关键代码
--------

.. code-block:: c

   /* 定义 LOG_TAG (必须在包含 lisa_log.h 之前) */
   #define LOG_TAG "logger_sample"
   
   /* 日志头文件 */
   #include <lisa_log.h>
   
   int main(int argc, char **argv)
   {
       LOGI("Hello, world! \n");
       return 0;
   }

注意事项
--------

1. **LOG_TAG 定义**: LOG_TAG 必须在包含 ``lisa_log.h`` 之前定义，否则会编译错误
2. **日志级别**: 本示例使用 ``LOGI()`` 输出信息级别日志，确保日志级别配置允许显示信息级别日志
3. **换行符**: 日志消息末尾的 ``\n`` 用于换行，确保输出格式清晰

