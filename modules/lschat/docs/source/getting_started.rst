========
快速开始
========

lschat支持在linux平台和zephyr-rtos(当前仅支持CSK6多模态开发板)下编译运行

环境搭建
--------

.. tabs::
    .. group-tab:: Ubuntu

        .. code-block:: bash

            apt-get install cmake git
            pip install kconfiglib
    .. group-tab:: Zephyr

        zephyr环境搭建见: `zephyr_v3.4.0 <https://docs.zephyrproject.org/3.4.0/>`_


代码拉取
--------

.. tabs::
    .. group-tab:: Ubuntu

        .. code-block:: bash

            git clone https://cloud.listenai.com/CSKG962172/ls_chat.git
            git submodule update --init --recursive
    .. group-tab:: Zephyr

        .. code-block:: bash

            west init -m https://cloud.listenai.com/CSKG962172/ls_chat.git
            west update


编译示例
--------

.. tabs::
    .. group-tab:: Ubuntu

        .. code-block:: bash

            cmake -B build -DDEFCONF_FILE=prj-linux.conf -S samples/lsc_connect
            cmake --build build
    .. group-tab:: Zephyr

        .. code-block:: bash

            west build -p -b csk6_duomotai_devkit samples/lsc_connect

运行示例
--------

.. tabs::
    .. group-tab:: Ubuntu

        成功编译后直接运行build目录下的lsc_connect，可见如下日志输出：

        .. code-block:: bash

            ./build/lsc_connect
                ./build/lsc_connect
                [lsc_core] lsc connect
                [lsc_conn] [conn_auth] dev_id:F97CE114C70DE8E2 pro_id:ce1dda98-f2b8-46f9-a7b9-8aee214a459b ser_id:c684eb9e-ff8f-4264-ad03-0369b38ab793
                [lisa-ws] websocket thread waiting for connect sem
                [core-sntp] Using server ntp.aliyun.com for time query
                [core-sntp] Server DNS resolved: Address=0xcb6b0658
                [core-sntp] Obtained current time for SNTP request packet: Time=1719904825s 719ms
                [core-sntp] Sending serialized SNTP request packet to the server: Addr=3412788824, Port=123
                [core-sntp] Received server response: PacketSize=48
                [core-sntp] Updating system time: ServerTime=3928893625 705ms ClockOffset=-2085978496s
                ......
                ......
                ......
                [lsc_conn] [_ws_event_cb] evt:2
                [lsc_core] [_lsc_core_conn_evt_cb] evt:conn connected 
                [lsc_conn] ws data: 81, {"action":"connected","cid":"9a0b421aef94","code":"0","data":"","desc":"success"}
                [lsc_core] [_lsc_core_conn_evt_cb] evt:conn data cjson 
                [main] evt : lsc connected

    .. group-tab:: Zephyr

        .. code-block:: bash

            west flash
