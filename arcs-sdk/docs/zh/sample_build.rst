
**重要提示**：在编译前，请先确认您使用的开发板型号。SDK 目前支持以下开发板：

- **arcs_evb** - ARCS EVB 评估板
- **arcs_mini** - ARCS Mini 开发板

根据您的开发板型号，选择对应的编译命令：

在 SDK 根目录执行编译。Linux 使用 ``build.sh``：

.. code-block:: bash

   # 使用 arcs_evb 开发板
   ./build.sh -C -S samples/<示例路径> -DBOARD=arcs_evb
   
   # 或使用 arcs_mini 开发板
   ./build.sh -C -S samples/<示例路径> -DBOARD=arcs_mini

Windows PowerShell 使用 ``build.ps1``：

.. code-block:: powershell

   # 使用 arcs_evb 开发板
   .\build.ps1 -C -S samples/<示例路径> -DBOARD=arcs_evb

   # 或使用 arcs_mini 开发板
   .\build.ps1 -C -S samples/<示例路径> -DBOARD=arcs_mini

.. note::

   确保已安装对应平台的工具链和 ListenAI 开发工具包。Windows 环境请先执行 ``.\env.ps1`` 或直接使用 ``.\build.ps1`` 自动加载环境。
