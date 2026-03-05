.. _stream_text_sync:

流式数据获取（同步）
================================

概述
---------------------------
此示例演示如何使用同步的方式利用大模型平台下发的url获取流式文本数据, 获取到的数据在回掉函数中打印

.. note::

    回调函数上下文与调用线程为同一线程

编译
---------------------------

    ${SAMPLE_PATH} 需修改为对应示例的路径

   .. tabs::
    .. group-tab:: Ubuntu

        .. code-block:: bash

            cmake -B build -DDEFCONF_FILE=prj-linux.conf -S ${SAMPLE_PATH}
            cmake --build build

    .. group-tab:: Zephyr

        .. code-block:: bash

            west build -p -b csk6_duomotai_devkit ${SAMPLE_PATH}

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


成功运行后可以看到以下日志：

.. code-block:: bash

    [stream_text] response content: event: data
    data: 根据


    [main] sse_evt_cb_handle, evt:0, data:根据
    [stream_text] response content: event: data
    data: 图片中的


    [main] sse_evt_cb_handle, evt:0, data:图片中的
    [stream_text] response content: event: data
    data: 食材，以下是


    [main] sse_evt_cb_handle, evt:0, data:食材，以下是
    [stream_text] response content: event: data
    data: 三道菜的建议


    [main] sse_evt_cb_handle, evt:0, data:三道菜的建议
    [stream_text] response content: event: data
    data: ：

    1.


    [main] sse_evt_cb_handle, evt:0, data:：

    1.
    [stream_text] response content: event: data
    data:  蔬菜沙拉：使用生菜、胡萝卜和西红柿制作一份新鲜的蔬菜沙拉。


    [main] sse_evt_cb_handle, evt:0, data: 蔬菜沙拉：使用生菜、胡萝卜和西红柿制作一份新鲜的蔬菜沙拉。
    [stream_text] response content: event: data
    data: 可以将生菜撕成小块，与切片的胡萝卜和西红柿混合在一起，加入橄榄油、柠檬汁和盐调味。


    [main] sse_evt_cb_handle, evt:0, data:可以将生菜撕成小块，与切片的胡萝卜和西红柿混合在一起，加入橄榄油、柠檬汁和盐调味。
    [stream_text] response content: event: data
    data: 这是一道轻盈健康的前菜或配菜。



    [main] sse_evt_cb_handle, evt:0, data:这是一道轻盈健康的前菜或配菜。
    [stream_text] response content: event: data
    data: 
    2. 烤蔬菜：将胡萝卜、洋葱和西兰花切成小块，撒上盐、胡椒和橄榄油，然后放入烤箱烤至金黄酥脆。


    [main] sse_evt_cb_handle, evt:0, data:
    2. 烤蔬菜：将胡萝卜、洋葱和西兰花切成小块，撒上盐、胡椒和橄榄油，然后放入烤箱烤至金黄酥脆。
    [stream_text] response content: event: data
    data: 这道烤蔬菜可以作为主菜的配菜，也可以单独作为一道营养丰富的菜肴。



    [main] sse_evt_cb_handle, evt:0, data:这道烤蔬菜可以作为主菜的配菜，也可以单独作为一道营养丰富的菜肴。
    [stream_text] response content: event: data
    data: 
    3. 香蕉蓝莓奶昔：将香蕉、蓝莓和牛奶放入搅拌机中搅拌均匀，制作成奶昔。


    [main] sse_evt_cb_handle, evt:0, data:
    3. 香蕉蓝莓奶昔：将香蕉、蓝莓和牛奶放入搅拌机中搅拌均匀，制作成奶昔。
    [stream_text] response content: event: data
    data: 可以根据个人口味添加蜂蜜或糖来增加甜味。


    [main] sse_evt_cb_handle, evt:0, data:可以根据个人口味添加蜂蜜或糖来增加甜味。
    [stream_text] response content: event: data
    data: 这道奶昔既健康又美味，是一道适合早餐或下午茶的选择。




    [main] sse_evt_cb_handle, evt:0, data:这道奶昔既健康又美味，是一道适合早餐或下午茶的选择。
    [stream_text] response content: event: data
    data: 希望这些建议能够帮助你利用图片中的食材做出美味的菜肴！


    [main] sse_evt_cb_handle, evt:0, data:希望这些建议能够帮助你利用图片中的食材做出美味的菜肴！
    [stream_text] response content: event: done
    data: [DONE]


    [main] sse_evt_cb_handle, evt:1, data:(null)
    [main] lsc_stream_text_request sample done

示例中所使用的API文档见 ::ref:`lsc_stream_text`