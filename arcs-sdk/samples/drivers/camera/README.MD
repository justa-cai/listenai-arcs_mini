# 示例说明
该示例演示如何使用GC032A的Camera驱动，从camera获取图像数据，并通过串口发送出去；
程序启动后，会通过串口发送30帧图像出去，电脑接收到图像后可通过yuvview查看是否正常。

## 接口说明

### DVP
| 引脚名称 | 引脚号 |
| --- | --- |
| CAM_HSYNC | PA10 |
| CAM_VSYNC | PA11 |
| CAM_PCLK  | PA12 |
| CAM_D4    | PA13 |
| CAM_D5    | PA14 |
| CAM_D6    | PA15 |
| CAM_D7    | PA16 |
| CAM_D8    | PA17 |
| CAM_D9    | PA18 |
| CAM_D10   | PA19 |
| CAM_D11   | PA20 |
| CAM_MCLK  | PA26 |
| CAM_PWDN  | PB07 |

### UART
| 引脚名称 | 引脚号 |
| --- | --- |
| UART_TX | PA21 |
