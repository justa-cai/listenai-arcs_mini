# arcs芯片的emmc烧录
针对arcs芯片的扫描笔板子的emmc芯片的烧录工具

## 使用说明
- flash烧录

  ```shell
  cskburn -s /dev/ttyUSB0 -b 3000000 0 flash.bin --verify-all
  ```

- emmc烧录并进行MD5校验

  ```shell
  # ram_loader已经集成到烧录工具中了，不需要单独进行烧录
  # --emmc 表示烧录设备为emmc
  # 0 表示起始位置为0
  # emmc.bin 表示待烧录的文件
  # --verify-all 表示进行md5校验
  cskburn -s /dev/ttyUSB0 -b 3000000 --emmc 0 emmc.bin --verify-all
  ```

- emmc擦除

  ```shell
  # emmc擦除从0x0位置长度为0x10000的数据
  # --emmc 表示烧录设备为emmc
  cskburn -s /dev/ttyUSB0 -b 3000000 --emmc --erase 0:0x10000
  ```

- 只校验

  ```shell
  # 校验从0位置长度为0x10000的数据
  # --emmc 表示烧录设备为emmc
  cskburn -s /dev/ttyUSB0 -b 3000000 --emmc --verify 0:0x10000
  ```

- 读emmc中的数据

  ```shell
  # 读取从0位置长度为0x10000的数据到read.bin中
  # --emmc 表示烧录设备为emmc
  cskburn -s /dev/ttyUSB0 -b 3000000 --emmc --read 0:0x10000:read.bin
  ```