
编译完成后，使用 SDK tools 目录下的 cskburn 工具烧录固件:

.. code-block:: bash

   ./tools/burn/cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/arcs.bin -C arcs

.. note::

   **烧录参数说明**：
   
   - ``-s /dev/ttyUSB0``：串口设备路径，**需要根据实际情况修改**
     - Linux 系统：通常是 ``/dev/ttyUSB0`` 或 ``/dev/ttyACM0``
     - 可通过 ``ls /dev/tty*`` 命令查看可用串口设备
     - 不同开发板或 USB 转串口芯片可能使用不同的设备名
   - ``-b 3000000``：烧录波特率（3Mbps）
   - ``0x0``：烧录起始地址
   - ``build/arcs.bin``：编译生成的固件路径
   - ``-C arcs``：芯片类型
   
   **注意事项**：

   - 确保开发板已正确连接到电脑
   - 如果无法识别串口设备，请检查 USB 连接线是否正常，或尝试其他 USB 端口

