JFlash Download:
1. 打开eclipse的Run/External tools/External tools configurations，点击New launch configurations。
2. Main页窗口：Location选择JFlash.exe路径，例如：C:\Program Files\SEGGER\JLink\JFlash.exe；
             Working Directory选择项目路径，例如：${workspace_loc:/listenai_nos}
             Arguments输入命令参数，例如：-openprjtools/vega.jflash -openout/vega/boot.bin,0x100000 -jlinkdevicesxmlpathtools/flash_tool/JLinkDevices.xml -auto
             Arguments输入命令参数，例如: -openprjtools/mars.jflash -openout/mars/pmp.bin,0x38000000 -jlinkdevicesxmlpathtools/flash_tool/JLinkDevices.xml -auto
             Arguments输入命令参数，例如: -openprjtools/arcs.jflash -openout/arcs/coremark.bin,0x30000000 -jlinkdevicesxmlpathtools/flash_tool/JLinkDevices.xml -auto
3. Build页窗口：不选择Build before launch
4. 在菜单或者工具栏上，点击执行刚才建立的工具就可以通过jlink烧录flash了。

JLinkGDBServerCL.exe Debug:
在Debugger的设置页面，注意Device Name选择，针对ListenAI公司的芯片，主要有以下几种，名字在JLinkDeivce.xml文件中定义：
(1)Venus
(2)Vega
(3)Arcs
(4)MARS
(5)CSK5060

Option的选项页面需要增加下面的配置:
-JlinkDevicesXMLPath tools/flash_tool/JLinkDevices.xml




