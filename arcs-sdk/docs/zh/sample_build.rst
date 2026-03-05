
**重要提示**：在编译前，请先确认您使用的开发板型号。SDK 目前支持以下开发板：

- **arcs_evb** - ARCS EVB 评估板
- **arcs_mini** - ARCS Mini 开发板

根据您的开发板型号，选择对应的编译命令：

在示例目录下执行编译:

.. code-block:: bash

   # 使用 arcs_evb 开发板
   ./build.sh -C -DBOARD=arcs_evb
   
   # 或使用 arcs_mini 开发板
   ./build.sh -C -DBOARD=arcs_mini

.. note::

   如果在 SDK 根目录执行，需要指定示例路径:

   .. code-block:: bash

      # 使用 arcs_evb 开发板
      ./build.sh -C -S samples/<示例路径> -DBOARD=arcs_evb
      
      # 或使用 arcs_mini 开发板
      ./build.sh -C -S samples/<示例路径> -DBOARD=arcs_mini

.. note::

   确保已安装对应的工具链。

