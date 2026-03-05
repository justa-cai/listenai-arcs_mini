
编译
--------------------------------------

.. warning::
    使用本例程前，需要正确配置宏 :literal:`DEVICE_ID_STRING`，请先联系FAE注册对应的设备ID

    各个sample中默认未定义 :literal:`DEVICE_ID_STRING`，若不手动定义此宏，示例代码是无法正常编译通过的。

.. note::
    编译命令中的 :literal:`${SAMPLE_PATH}` 需要根据实际代码路径进行填写，如：:literal:`samples/lsc_connect`


1. 方式一， 通过修改代码的方式在 :literal:`main.c` 中定义 :literal:`DEVICE_ID_STRING` 宏
    
    .. code-block:: c

        #define DEVICE_ID_STRING "XXXXX"

    定义:literal:`DEVICE_ID_STRING` 宏后再编译代码

    .. tabs::

        .. group-tab:: Ubuntu

            .. code-block:: bash

                cmake -B build -DDEFCONF_FILE=prj-linux.conf -S ${SAMPLE_PATH}
                cmake --build build

        .. group-tab:: Zephyr

            .. code-block:: bash

                west build -p -b csk6_duomotai_devkit ${SAMPLE_PATH}
            
2. 方式二，通过编译命令进行传递

或者通过通过编译命令传递

.. tabs::
    .. group-tab:: Ubuntu

        .. code-block:: bash

            cmake -B build -DDEFCONF_FILE=prj-linux.conf -DDEVICE_ID_STRING="XXXXX" -S ${SAMPLE_PATH}
            cmake --build build

    .. group-tab:: Zephyr

        .. code-block:: bash

            west build -p -b csk6_duomotai_devkit ${SAMPLE_PATH} -- -DDEVICE_ID_STRING="XXXXX"

运行
---------------------------

.. tabs::
    .. group-tab:: Ubuntu

        成功编译后直接运行build目录下的可执行程序

        .. code-block:: bash

            ./build/xxxxx

    .. group-tab:: Zephyr

        zephyr环境下将板子的DAP-USB连接电脑进行烧录和查看日志

        .. code-block:: bash

            west flash
