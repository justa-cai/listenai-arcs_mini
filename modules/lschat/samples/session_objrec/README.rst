.. _objrec_sample:

==============================
物体识别
==============================

该示例演示了如何将一张jpg图片进行识别，并获取识别的结果。

.. toctree::
   :hidden:

   ../sample_build

.. include:: ../sample_build.rst

演示图片：

.. image:: test.jpg

在成功连接大模型平台后，通过session_objrec_run接口将图片上传至大模型平台，并同步等待平台下发结果

.. code-block:: c

    ......
    err = session_objrec_run(objrec, test_img, sizeof(test_img), result, 10 * 1000);
    if (err) {
        LISA_NLOGE("session_objrec_run failed, err:%d", err);
        session_objrec_delete(objrec);
        lisa_mem_free(result);
        return;
    }
    ......

成功获取到识别结果后，需要将结果中的text_url再次向云端进行请求，获取到最终的文本结果，
具体的文本数据会在lsc_sse_evt_cb_handle中进行回调

.. code-block:: c

    struct lsc_stream_text_request_ctx *ctx =
		lsc_stream_text_request_new(result->text_url, lsc_sse_evt_cb_handle, NULL);
	if (ctx) {
		err = lsc_stream_text_request_start(ctx, 10);
		if (err) {
			LISA_NLOGE("lsc_stream_text_request failed, err:%d", err);
		}
		lsc_stream_text_request_delete(ctx);
	} else {
		LISA_NLOGE("lsc_stream_text_request_ctx new failed");
	}

成功请求数据后，将会打印以下日志

.. code-block:: bash

    [stream_text] response content: event: data
    data: 图片


    [main] sse evt:0, data:图片
    [stream_text] response content: event: data
    data: 里面有一些


    [main] sse evt:0, data:里面有一些
    [stream_text] response content: event: data
    data: 紫色的圆形


    [main] sse evt:0, data:紫色的圆形
    [stream_text] response content: event: data
    data: 物体，看起来像是


    [main] sse evt:0, data:物体，看起来像是
    [stream_text] response content: event: data
    data: 葡萄。


    [main] sse evt:0, data:葡萄。
    [stream_text] response content: event: done
    data: [DONE]

示例中所使用的API文档见 ::ref:`session_objrec`