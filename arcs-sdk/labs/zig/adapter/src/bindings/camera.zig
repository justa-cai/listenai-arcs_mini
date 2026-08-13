///! ARCS SDK Camera driver FFI bindings.
///! Mirrors lisa_camera.h.
const device = @import("device.zig");

// Camera enums are represented as c_int because lisa_camera_get_pixformat()
// returns negative device errors through the enum-typed C API.
pub const PixelFormat = c_int;
pub const PIXFMT_RGB565: PixelFormat = 0;
pub const PIXFMT_RGB888: PixelFormat = 1;
pub const PIXFMT_YUV422: PixelFormat = 2;
pub const PIXFMT_YUV420: PixelFormat = 3;
pub const PIXFMT_GRAY: PixelFormat = 4;
pub const PIXFMT_JPEG: PixelFormat = 5;
pub const PIXFMT_RAW: PixelFormat = 6;

pub const FrameSize = c_int;
pub const FRAMESIZE_QQVGA: FrameSize = 0;
pub const FRAMESIZE_QCIF: FrameSize = 1;
pub const FRAMESIZE_QVGA: FrameSize = 2;
pub const FRAMESIZE_CIF: FrameSize = 3;
pub const FRAMESIZE_VGA: FrameSize = 4;
pub const FRAMESIZE_SVGA: FrameSize = 5;
pub const FRAMESIZE_XGA: FrameSize = 6;
pub const FRAMESIZE_HD: FrameSize = 7;
pub const FRAMESIZE_SXGA: FrameSize = 8;
pub const FRAMESIZE_UXGA: FrameSize = 9;
pub const FRAMESIZE_FHD: FrameSize = 10;

pub const BusType = c_int;
pub const BUS_DVP: BusType = 0;
pub const BUS_SPI: BusType = 1;

pub const SensorIndex = c_int;
pub const SENSOR_0: SensorIndex = 0;
pub const SENSOR_1: SensorIndex = 1;
pub const SENSOR_MAX: usize = 2;

pub const SensorPwdn = extern struct {
    pwdn_gpio_dev: ?*device.Device,
    pwdn_pin: u8,
    pwdn_active_level: u8,
};

pub const BusDvpConfig = extern struct {
    dvp_dev: ?*device.Device,
    dvp_freq: u32,
    pixel_offset: u16,
    line_offset: u16,
    pclk_polarity: u8,
    vsync_polarity: u8,
    hsync_polarity: u8,
    data_align: u8,
    dma_dev: ?*device.Device,
};

pub const BusSpiConfig = extern struct {
    spi_dev: ?*device.Device,
    spi_freq: u32,
    spi_mode: u8,
    spi_bit_order: u8,
    cs_gpio: ?*device.Device,
    cs_pin: u32,
    dma_dev: ?*device.Device,
};

pub const BusConfigUnion = extern union {
    dvp: BusDvpConfig,
    spi: BusSpiConfig,
};

pub const BusConfig = extern struct {
    bus_type: BusType,
    config: BusConfigUnion,
    pixel_format: PixelFormat,
    dma_channel: u8,
    width: u16,
    height: u16,
};

pub const HwConfig = extern struct {
    mclk_pad: u8,
    mclk_pin: u8,
    pwdn_gpio_dev: ?*device.Device,
    pwdn_pin: u8,
    reset_gpio_dev: ?*device.Device,
    reset_pin: u8,
    reset_active_level: u8,
    reset_delay_us: u32,
    pwdn_delay_us: u32,
    xclk_delay_us: u32,
    i2c_dev: ?*device.Device,
    multiplex_camera: bool,
    sensor_pwdn: [SENSOR_MAX]SensorPwdn,
};

pub const Config = extern struct {
    hw_config: HwConfig,
    xclk_freq_hz: u32,
    jpeg_quality: u8,
    fb_count: u8,
    enable_hmirror: bool,
    enable_vflip: bool,
    enable_colorbar: bool,
    mem_pool: ?*anyopaque,
    mem_pool_size: u32,
};

pub const FrameBuffer = extern struct {
    buf: ?[*]u8,
    len: u32,
    width: u16,
    height: u16,
    format: PixelFormat,
    timestamp: u32,
};

pub const Crop = extern struct {
    x: i16,
    y: i16,
    width: u16,
    height: u16,
};

pub const Capabilities = extern struct {
    max_width: u16,
    max_height: u16,
    supported_formats: u32,
};

pub const FrameCallback = ?*const fn (*const FrameBuffer, ?*anyopaque) callconv(.C) void;

pub const CameraApi = extern struct {
    setup: ?*const fn (*device.Device, *const Config) callconv(.C) i32,
    start: ?*const fn (*device.Device) callconv(.C) i32,
    stop: ?*const fn (*device.Device) callconv(.C) i32,
    capture: ?*const fn (*device.Device, *?*FrameBuffer) callconv(.C) i32,
    release_fb: ?*const fn (*device.Device, *FrameBuffer) callconv(.C) i32,
    get_capabilities: ?*const fn (*device.Device, *Capabilities) callconv(.C) i32,
    attach_bus: ?*const fn (*device.Device, *const BusConfig) callconv(.C) i32,
    set_hmirror: ?*const fn (*device.Device, bool) callconv(.C) i32,
    set_vflip: ?*const fn (*device.Device, bool) callconv(.C) i32,
    set_crop: ?*const fn (*device.Device, *const Crop) callconv(.C) i32,
    get_framesize: ?*const fn (*device.Device, *u16, *u16) callconv(.C) i32,
    set_pixformat: ?*const fn (*device.Device, PixelFormat) callconv(.C) i32,
    set_reg: ?*const fn (*device.Device, c_int, c_int, c_int) callconv(.C) i32,
    get_reg: ?*const fn (*device.Device, c_int, c_int) callconv(.C) i32,
    set_callback: ?*const fn (*device.Device, FrameCallback, ?*anyopaque) callconv(.C) i32,
    get_pixformat: ?*const fn (*device.Device) callconv(.C) PixelFormat,
    switch_sensor: ?*const fn (*device.Device, SensorIndex) callconv(.C) i32,
    get_current_sensor: ?*const fn (*device.Device, *SensorIndex) callconv(.C) i32,
};

pub fn setup(dev: *device.Device, config: *const Config) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.setup orelse return -6;
    return func(dev, config);
}

pub fn start(dev: *device.Device) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.start orelse return -6;
    return func(dev);
}

pub fn stop(dev: *device.Device) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.stop orelse return -6;
    return func(dev);
}

pub fn capture(dev: *device.Device, fb: *?*FrameBuffer) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.capture orelse return -6;
    return func(dev, fb);
}

pub fn releaseFb(dev: *device.Device, fb: *FrameBuffer) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.release_fb orelse return -6;
    return func(dev, fb);
}

pub fn getCapabilities(dev: *device.Device, caps: *Capabilities) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.get_capabilities orelse return -6;
    return func(dev, caps);
}

pub fn attachBus(dev: *device.Device, bus_config: *const BusConfig) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.attach_bus orelse return -6;
    return func(dev, bus_config);
}

pub fn setHmirror(dev: *device.Device, enable: bool) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.set_hmirror orelse return -6;
    return func(dev, enable);
}

pub fn setVflip(dev: *device.Device, enable: bool) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.set_vflip orelse return -6;
    return func(dev, enable);
}

pub fn setCrop(dev: *device.Device, crop: *const Crop) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.set_crop orelse return -6;
    return func(dev, crop);
}

pub fn getFramesize(dev: *device.Device, width: *u16, height: *u16) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.get_framesize orelse return -6;
    return func(dev, width, height);
}

pub fn getPixformat(dev: *device.Device) PixelFormat {
    const api = getApi(dev) orelse return -1;
    const func = api.get_pixformat orelse return -6;
    return func(dev);
}

pub fn setPixformat(dev: *device.Device, format: PixelFormat) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.set_pixformat orelse return -6;
    return func(dev, format);
}

pub fn setReg(dev: *device.Device, reg: c_int, mask: c_int, value: c_int) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.set_reg orelse return -6;
    return func(dev, reg, mask, value);
}

pub fn getReg(dev: *device.Device, reg: c_int, mask: c_int) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.get_reg orelse return -6;
    return func(dev, reg, mask);
}

pub fn setCallback(dev: *device.Device, callback: FrameCallback, user_data: ?*anyopaque) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.set_callback orelse return -6;
    return func(dev, callback, user_data);
}

pub fn switchSensor(dev: *device.Device, index: SensorIndex) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.switch_sensor orelse return -6;
    return func(dev, index);
}

pub fn getCurrentSensor(dev: *device.Device, index: *SensorIndex) i32 {
    const api = getApi(dev) orelse return -1;
    const func = api.get_current_sensor orelse return -6;
    return func(dev, index);
}

fn getApi(dev: *device.Device) ?*const CameraApi {
    const ptr = dev.api orelse return null;
    return @ptrCast(@alignCast(ptr));
}
