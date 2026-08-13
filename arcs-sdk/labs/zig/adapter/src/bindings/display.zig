///! ARCS SDK Display 驱动 FFI 绑定
///! 映射 lisa_display.h
const device = @import("device.zig");

// ════════════════════════════════════════════════════════════════════
// Display 枚举
// ════════════════════════════════════════════════════════════════════

pub const PixelFormat = enum(u32) {
    rgb_888 = 0,
    rgb_565 = 1,
    bgr_565 = 2,
    argb_8888 = 3,
    mono_1 = 4,
};

pub const Orientation = enum(u32) {
    @"0" = 0,
    @"90" = 1,
    @"180" = 2,
    @"270" = 3,
};

pub const BusType = enum(u32) {
    spi_3wire = 0,
    spi_4wire = 1,
    i8080 = 2,
    qspi = 3,
    rgb = 4,
};

pub const CmdBusType = enum(u32) {
    none = 0,
    sw_spi = 1,
};

pub const RgbInputFormat = enum(u32) {
    rgb888 = 0,
    xrgb8888 = 1,
    rgb565 = 2,
};

pub const RgbPolarity = enum(u32) {
    positive = 0,
    negative = 1,
};

pub const RgbOutputFormat = enum(u32) {
    rgb888 = 0,
    rgb666 = 1,
    rgb565 = 2,
    bgr888 = 3,
    bgr666 = 4,
    bgr565 = 5,
};

pub const BacklightType = enum(u32) {
    pwm = 0,
    single_wire = 1,
};

pub const BacklightPolarity = enum(u32) {
    low = 0,
    high = 1,
};

// ════════════════════════════════════════════════════════════════════
// Display 结构体
// ════════════════════════════════════════════════════════════════════

pub const Capabilities = extern struct {
    width: u16,
    height: u16,
    pixel_format: PixelFormat,
    orientation: Orientation,
    supported_pixel_formats: u32,
};

pub const BufferDesc = extern struct {
    width: u16,
    height: u16,
    pitch: u16,
    buf_size: u32,
};

pub const Rect = extern struct {
    x: u16,
    y: u16,
    width: u16,
    height: u16,
};

pub const BusSpi4WireConfig = extern struct {
    spi_dev: ?*device.Device,
    dc_gpio: ?*device.Device,
    cs_gpio: ?*device.Device,
    cs_pin: u32,
    dc_pin: u32,
    spi_freq: u32,
};

pub const BusSpi3WireConfig = extern struct {
    spi_dev: ?*device.Device,
    spi_freq: u32,
};

pub const BusQspiConfig = extern struct {
    qspi_dev: ?*device.Device,
    qspi_freq: u32,
    cs_gpio: ?*device.Device,
    cs_pin: u32,
};

pub const RgbTimings = extern struct {
    h_res: u16,
    v_res: u16,
    h_pulse_width: u8,
    v_pulse_width: u8,
    h_front_blanking: u8,
    h_back_blanking: u8,
    v_front_blanking: u8,
    v_back_blanking: u8,
};

pub const BusRgbConfig = extern struct {
    rgb_dev: ?*device.Device,
    pclk_hz: u32,
    input_format: RgbInputFormat,
    output_format: RgbOutputFormat,
    output_lsb_first: bool,
    vsync_polarity: RgbPolarity,
    hsync_polarity: RgbPolarity,
    de_polarity: RgbPolarity,
    pclk_polarity: RgbPolarity,
    bounce_buffer_size: u32,
    timings: RgbTimings,
};

pub const CmdSwSpiConfig = extern struct {
    cs_gpio: ?*device.Device,
    cs_pin: u32,
    scl_gpio: ?*device.Device,
    scl_pin: u32,
    sda_gpio: ?*device.Device,
    sda_pin: u32,
    spi_freq: u32,
    lsb_first: u8,
    cs_high_active: u8,
    spi_mode: u8,
    use_dc_bit: u8,
    dc_zero_on_data: u8,
};

pub const CmdBusConfig = extern union {
    sw_spi: CmdSwSpiConfig,
};

pub const BacklightPwmConfig = extern struct {
    dev: ?*device.Device,
    channel: u8,
    freq: u32,
};

pub const BacklightSingleWireConfig = extern struct {
    dev: ?*device.Device,
    pin: u8,
    steps: u8,
    current_level: u8,
};

pub const BacklightConfig = extern union {
    pwm: BacklightPwmConfig,
    sw: BacklightSingleWireConfig,
};

pub const Backlight = extern struct {
    type: BacklightType,
    config: BacklightConfig,
    blacklight_polarity: BacklightPolarity,
    priv_data: ?*anyopaque,
};

pub const BusConfig = extern union {
    spi_3wire: BusSpi3WireConfig,
    spi_4wire: BusSpi4WireConfig,
    qspi: BusQspiConfig,
    rgb: BusRgbConfig,
};

pub const Config = extern struct {
    panel_name: ?[*]const u8,
    bus_type: BusType,
    bus_config: BusConfig,
    cmd_bus_type: CmdBusType,
    cmd_bus_config: CmdBusConfig,
    rst_gpio: ?*device.Device,
    rst_pin: u32,
    te_gpio: ?*device.Device,
    te_pin: u32,
    backlight: Backlight,
    panel_init_params: ?*const anyopaque,
    panel_init_params_len: usize,
};

// ════════════════════════════════════════════════════════════════════
// Display API 虚表
// ════════════════════════════════════════════════════════════════════

pub const DisplayApi = extern struct {
    get_capabilities: ?*const fn (*device.Device, *Capabilities) callconv(.C) i32,
    write: ?*const fn (*device.Device, u16, u16, *const BufferDesc, ?*const anyopaque) callconv(.C) i32,
    blanking_on: ?*const fn (*device.Device) callconv(.C) i32,
    blanking_off: ?*const fn (*device.Device) callconv(.C) i32,
    set_brightness: ?*const fn (*device.Device, u8) callconv(.C) i32,
    set_orientation: ?*const fn (*device.Device, Orientation) callconv(.C) i32,
    attach_bus: ?*const fn (*const device.Device, *const Config) callconv(.C) i32,
};

// ════════════════════════════════════════════════════════════════════
// 内联辅助
// ════════════════════════════════════════════════════════════════════

pub fn getCapabilities(dev: *device.Device, caps: *Capabilities) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.get_capabilities orelse return -6;
    return func(dev, caps);
}

pub fn writePixels(dev: *device.Device, x: u16, y: u16, desc: *const BufferDesc, buf: ?*const anyopaque) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.write orelse return -6;
    return func(dev, x, y, desc, buf);
}

pub fn blankingOn(dev: *device.Device) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.blanking_on orelse return -6;
    return func(dev);
}

pub fn blankingOff(dev: *device.Device) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.blanking_off orelse return -6;
    return func(dev);
}

pub fn setBrightness(dev: *device.Device, brightness: u8) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.set_brightness orelse return -6;
    return func(dev, brightness);
}

pub fn setOrientation(dev: *device.Device, orientation: Orientation) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.set_orientation orelse return -6;
    return func(dev, orientation);
}

pub fn attachBus(dev: *device.Device, config: *const Config) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.attach_bus orelse return -6;
    return func(dev, config);
}

// ── 颜色常量 (RGB565) ───────────────────────────────────────────
pub const Color = struct {
    pub const black: u16 = 0x0000;
    pub const white: u16 = 0xFFFF;
    pub const red: u16 = 0xF800;
    pub const green: u16 = 0x07E0;
    pub const blue: u16 = 0x001F;
    pub const yellow: u16 = 0xFFE0;
    pub const cyan: u16 = 0x07FF;
    pub const magenta: u16 = 0xF81F;

    /// 从 RGB 分量 (0-255) 生成 RGB565 颜色
    pub fn rgb565(r: u8, g: u8, b: u8) u16 {
        return (@as(u16, r >> 3) << 11) | (@as(u16, g >> 2) << 5) | @as(u16, b >> 3);
    }
};

fn getApi(dev: *device.Device) ?*const DisplayApi {
    const ptr = dev.api orelse return null;
    return @ptrCast(@alignCast(ptr));
}
