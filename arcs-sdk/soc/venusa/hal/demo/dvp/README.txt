使用freertos，完成下述代码。注意链表操作需要加锁。
在函数dvp_init()中，建立链表，包含buf地址和状态（done、used、unused）；
	链表资源来自全局数组，避免后面频繁malloc，链表节点最大数量等于预留的图像buf数量；
	切换摄像头引脚的pinmux，复位控制；
	执行函数DVP_Initialize()和DVP_EnableClockout()；
	I2C初始化，发送摄像头配置序列；
	执行函数DMA2D_Initialize()和DMA2D_Config()，切换DMA的handshake。
在函数dvp_buf_add()中，添加buf链表节点
	如果有2个及以上buf链表处于used，则新添加的buf设置成unused
	如果只有1个buf链表处于used，则新添加的buf设置成used，执行函数DMA2D_Start_Normal()；
	如果没有buf链表处于used，则新添加的buf设置成used，执行函数DMA2D_Start_Normal()，执行函数DVP_Start()。
在中断函数dvp_gpdma_callback()中，收到block完成中断，
	通过链表获知第1个used的链表节点，状态改为done，发送信号dvpDoneSemaphore给task；
	通过链表获取unused的buf，如果有就设置成used，执行函数DMA2D_Start_Normal()；
	最后如果链表没有used的buf，执行函数DVP_Stop()和DMA2D_Stop()。
在函数dvp_buf_get()中，获知第1个done的buf地址，并删掉该buf链表节点
在函数dvp_buf_num_get()中，获知done、used和unused的buf链表节点数量。因为DMA2D是shadow方式进行轮转，所以最多只有2个链表节点处在used状态。
在任务主函数task()中，先执行dvp_init()，再执行dvp_buf_add()，有几个图像buf就调用几次dvp_buf_add()，然后进入while循环检测：
	收到信号dvpDoneSemaphore，执行dvp_buf_get()获知buf地址;
	处理数据；
	执行函数dvp_buf_add()。

错误检测和恢复：
在中断函数dvp_callback()中，检测到中断EOF_CNT_ABNOR_ISR，发送信号dvpErrSemaphore给task。另外出现中断FIFO_OVFLOW、PIXEL_ABNOR、H_SYNC_ABNOR，说明当前帧数据出错，但不一定会影响到后续帧，所以对这些中断没有走错误恢复流程。
在任务主函数task()中，收到信号dvpErrSemaphore，表示当前帧数据异常，走停止再恢复流程。后续就不再执行dvp_buf_add，通过函数dvp_buf_num_get()获知used状态的链表节点数量为0就说明DMA2D已经彻底停下。再重新调用dvp_buf_add()进行收图，有几个图像buf就调用几次dvp_buf_add()。
注意：因为DVP_Start之后，第1个sof中断到来之前，可能会误报1次中断EOF_CNT_ABNOR_ISR，所以需要开始启动收图等几帧之后再判断错误。

遗留：
在中断函数dvp_gpdma_callback()中使用xQueueSendFromISR(dvpQueue, &used_node->buf_addr, &xHigherPriorityTaskWoken)会导致卡死或报Stack Overflow，只能改用xSemaphoreGiveFromISR(dvpSemaphore, &xHigherPriorityTaskWoken)，并新增函数dvp_buf_get()。

