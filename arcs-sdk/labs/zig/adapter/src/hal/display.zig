///! ARCS SDK Zig Display 驱动封装
///!
///! 用法:
///!   var lcd = try arcs.Display.open("display0");
///!   const caps = try lcd.getCapabilities();
///!   try lcd.setBrightness(80);
///!   try lcd.fillRect(0, 0, caps.width, caps.height, arcs.Display.Color.blue);
const c_display = @import("../bindings/display.zig");
const c_device = @import("../bindings/device.zig");

pub const Display = struct {
    dev: *c_device.Device,

    const Self = @This();

    pub const Color = c_display.Color;
    pub const PixelFormat = c_display.PixelFormat;
    pub const Orientation = c_display.Orientation;
    pub const Capabilities = c_display.Capabilities;
    pub const Config = c_display.Config;
    pub const BusType = c_display.BusType;
    pub const BacklightType = c_display.BacklightType;
    pub const BacklightPolarity = c_display.BacklightPolarity;

    pub fn open(name: [:0]const u8) !Self {
        const dev = c_device.lisa_device_get(name.ptr) orelse return error.DeviceNotFound;
        if (!c_device.lisa_device_ready(dev)) return error.DeviceNotReady;
        return .{ .dev = dev };
    }

    /// 获取显示器能力信息
    pub fn getCapabilities(self: Self) !Capabilities {
        var caps: Capabilities = undefined;
        const ret = c_display.getCapabilities(self.dev, &caps);
        if (ret != 0) return error.QueryFailed;
        return caps;
    }

    /// 写入像素缓冲区到指定位置
    pub fn writeBuffer(self: Self, x: u16, y: u16, width: u16, height: u16, buf: []const u8) !void {
        const desc = c_display.BufferDesc{
            .width = width,
            .height = height,
            .pitch = @as(u32, width) * 2, // RGB565 = 2 bytes/pixel
            .buf_size = @intCast(buf.len),
        };
        const ret = c_display.writePixels(self.dev, x, y, &desc, buf.ptr);
        if (ret != 0) return error.WriteFailed;
    }

    /// 设置亮度 (0-100)
    pub fn setBrightness(self: Self, brightness: u8) !void {
        const ret = c_display.setBrightness(self.dev, brightness);
        if (ret != 0) return error.SetBrightnessFailed;
    }

    /// 设置屏幕方向
    pub fn setOrientation(self: Self, orientation: Orientation) !void {
        const ret = c_display.setOrientation(self.dev, orientation);
        if (ret != 0) return error.SetOrientationFailed;
    }

    /// 绑定显示总线配置
    pub fn attachBus(self: Self, config: *const Config) !void {
        const ret = c_display.attachBus(self.dev, config);
        if (ret != 0) return error.AttachBusFailed;
    }

    /// 关闭显示 (blanking)
    pub fn off(self: Self) !void {
        const ret = c_display.blankingOn(self.dev);
        if (ret != 0) return error.BlankingFailed;
    }

    /// 开启显示
    pub fn on(self: Self) !void {
        const ret = c_display.blankingOff(self.dev);
        if (ret != 0) return error.BlankingFailed;
    }

    /// 用单色填充矩形区域 (RGB565)
    pub fn fillRect(self: Self, x: u16, y: u16, w: u16, h: u16, color: u16) !void {
        // 分批填充, 避免超大栈分配
        const row_pixels = @as(usize, w);
        const row_bytes = row_pixels * 2;
        var row_buf: [640 * 2]u8 = undefined; // max 640 px/row

        if (row_bytes > row_buf.len) return error.OutOfRange;

        // 填充一行颜色
        const color_bytes = @as([2]u8, @bitCast(color));
        var i: usize = 0;
        while (i < row_bytes) : (i += 2) {
            row_buf[i] = color_bytes[0];
            row_buf[i + 1] = color_bytes[1];
        }

        // 逐行写入
        var row: u16 = 0;
        while (row < h) : (row += 1) {
            const desc = c_display.BufferDesc{
                .width = w,
                .height = 1,
                .pitch = @intCast(row_bytes),
                .buf_size = @intCast(row_bytes),
            };
            const ret = c_display.writePixels(self.dev, x, y + row, &desc, &row_buf);
            if (ret != 0) return error.WriteFailed;
        }
    }
};
