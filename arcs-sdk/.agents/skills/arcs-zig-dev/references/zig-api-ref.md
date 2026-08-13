<!-- type: knowledge -->

# ARCS SDK Zig API 速查参考

完整的 Zig adapter API 参考，按使用频率排序。

---

## 导入

```zig
const arcs = @import("arcs");
```

---

## 日志

```zig
try arcs.log.init();
arcs.log.info("message {d}", .{42});
arcs.log.err("error: {s}", .{msg});

// 带模块标签
const log = arcs.log.scoped("my_module");
log.info("hello", .{});
log.debug("value={d}", .{x});
```

**级别**: `err` > `warn` > `info` > `debug` > `verbose`

---

## 内存分配

```zig
// PSRAM 分配器 (大容量, 4-16MB)
const alloc = arcs.allocator.psram();

// SRAM 分配器 (高速, 704KB)
const alloc = arcs.allocator.sram();

// 与标准库配合使用
var list = std.ArrayList(u8).init(alloc);
defer list.deinit();
```

---

## GPIO

```zig
var gpio = try arcs.Gpio.open("gpioa");

// 配置
try gpio.configOutput(pin, .{ .pull = .up, .init_high = true });
try gpio.configInput(pin, .{ .pull = .up, .debounce = true });

// 读写
const level = try gpio.read(pin);       // .low 或 .high
try gpio.write(pin, .high);
try gpio.toggle(pin);

// 中断
try gpio.onInterrupt(pin, .falling, handler, null);
try gpio.enableInterrupt(pin);
```

---

## UART

```zig
var uart = try arcs.Uart.open("uart0");
try uart.configure(.{ .baudrate = 115200 });
try uart.enableRx();

// 同步读写
try uart.writeAll("Hello\r\n");
const n = try uart.readTimeout(&buf, 1000);

// 轮询
uart.pollWrite('A');
const byte = uart.pollRead();

// std.io 接口
const writer = uart.writer();
try std.fmt.format(writer, "Value: {d}\n", .{42});
```

---

## I2C

```zig
var i2c = try arcs.I2c.open("i2c0");
try i2c.configure(.{ .speed = .fast });

try i2c.write(0x50, &data);
try i2c.read(0x50, &buf);
try i2c.readReg(0x50, 0x00, &buf);    // 寄存器读
try i2c.writeReg(0x50, 0x00, &data);  // 寄存器写
```

---

## SPI

```zig
var spi = try arcs.Spi.open("spi0");
try spi.configure(.{ .frequency = 10_000_000 });

try spi.write(&tx_data);
try spi.read(&rx_buf);
try spi.transferFull(&tx, &rx);  // 全双工
```

---

## ADC

```zig
var adc = try arcs.Adc.open("adc0");
try adc.setupChannel(0, .{});
const raw = try adc.read(0);
const mv = arcs.Adc.toMillivolts(raw, 3600, 10);
```

---

## PWM

```zig
var pwm = try arcs.Pwm.open("pwm0");
try pwm.set(0, 1000, 50);  // 通道0, 1kHz, 50% 占空比
try pwm.enable(0);
```

---

## Flash

```zig
var flash = try arcs.Flash.open("flash0");
try flash.read(0x1000, &buf);
try flash.erase(0x1000, 4096);
try flash.write(0x1000, &data);
```

---

## Display

```zig
var lcd = try arcs.Display.open("display0");
try lcd.setBrightness(80);
try lcd.fillRect(0, 0, 320, 240, arcs.Display.Color.blue);
const caps = try lcd.getCapabilities();
```

---

## RTC

```zig
var rtc = try arcs.Rtc.open("rtc0");
try rtc.setTime(.{ .year = 24, .month = 6, .day = 15 });
const now = try rtc.getTime();
```

---

## Audio

```zig
var audio = try arcs.Audio.open("audio0");
try audio.configRecord(.{ .format = .{ .sample_rate = 16000 } });
try audio.startRecord();
```

---

## Bluetooth

```zig
try arcs.Bluetooth.init(onReady);
try arcs.Bluetooth.startDiscovery(.general, 10);
try arcs.Bluetooth.connectByIndex(0);
```

---

## WiFi

```zig
try arcs.WiFi.init(.{ .on_init_done = onReady });
```

---

## 线程

```zig
_ = try arcs.Thread.spawn(.{
    .name = "worker",
    .stack_size = 4096,
    .priority = .normal,
}, workerFn, .{arg1, arg2});

arcs.Thread.sleep(100);  // ms
arcs.Thread.yield();
```

---

## 同步原语

```zig
// Mutex
var mtx = try arcs.Mutex.init();
defer mtx.deinit();
const held = mtx.acquire();
defer held.release();

// Semaphore
var sem = try arcs.Semaphore.init(1);
try sem.acquire(.{ .timeout_ms = 1000 });
sem.release();

// Channel (类型安全消息队列)
var ch = try arcs.Channel(u32).init(16);
try ch.send(42);
const val = try ch.recv();
```

---

## Timer

```zig
var timer = try arcs.Timer.periodic(500, callback);
try timer.start();

var once = try arcs.Timer.once(1000, callback);
try once.start();
```

---

## RingBuffer

```zig
var storage: [1024]u8 = undefined;
var rb = arcs.RingBuffer.init(&storage);
_ = rb.put("hello");
_ = rb.get(&buf);
```

---

## 设备框架

```zig
const dev = arcs.Device.get("uart0") orelse return error.NotFound;
const ready = arcs.Device.isReady(dev);
const total = arcs.Device.count();
```

---

## 便捷函数

```zig
arcs.sleep(1000);   // 休眠 1 秒
arcs.yield();       // 让出 CPU

// 低层 C 绑定 (直接 FFI)
const raw_ptr = arcs.c.mem.lisa_mem_alloc(1024);
arcs.c.mem.lisa_mem_free(raw_ptr);
```

---

## 类型

```zig
arcs.Priority     // .idle .low .normal .high .real_time
arcs.LogLevel     // .none .err .warn .info .debug .verbose
arcs.DeviceError  // .ok .invalid .not_found .timeout .io ...
```
