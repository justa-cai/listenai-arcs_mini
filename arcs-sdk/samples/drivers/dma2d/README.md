# dma2d的示例
本示例展示了使用dma2d实现YUV444到RGB888的图像格式转换功能


## 软件编译
在本目录下执行下面命令(PC端要求是ubuntu平台)

```shell
./build.sh
```

## 固件烧录


### cp固件烧录

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/arcs.bin
```

## 日志输出

芯片的串口日志如下：

```
I/elog            [00:00:00.000 1 elog_async] EasyLogger V2.2.99 is initialize success.
I/Dma2d Sample    [00:00:00.001 1 main] Hello, world! Dma2d
I/Dma2d Sample    [00:00:00.001 1 main] [DMA2D][IMAGE] YUV444 Transfer to RGB888 with DMA2D function, the image size[16, 16], DMA2D_Image_YUV444_to_RGB888
I/Dma2d Sample    [00:00:00.001 1 main] rgb_value = 0x00b7de3e
I/Dma2d Sample    [00:00:00.001 1 main] YUV444 Transfer to RGB888 with DMA2D function Success!
```
