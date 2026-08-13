//! Raw LVGL C bindings used by ARCS Zig applications.

const device = @import("device.zig");

const lvgl_config = @cImport({
    @cInclude("arcs_zig_lvgl_config.h");
});

pub const config = struct {
    pub const color_depth = lvgl_config.ARCS_ZIG_LVGL_COLOR_DEPTH;
    pub const big_endian_system = lvgl_config.ARCS_ZIG_LVGL_BIG_ENDIAN_SYSTEM;
    pub const use_assert_style = lvgl_config.ARCS_ZIG_LVGL_USE_ASSERT_STYLE;
    pub const use_user_data = lvgl_config.ARCS_ZIG_LVGL_USE_USER_DATA;
    pub const use_large_coord = lvgl_config.ARCS_ZIG_LVGL_USE_LARGE_COORD;
};

comptime {
    requireSupportedLvglAbi(config.color_depth == 16, "LVGL Zig binding currently supports only LV_COLOR_DEPTH=16");
    requireSupportedLvglAbi(config.big_endian_system == 0, "LVGL Zig binding currently supports only little-endian LV_IMG header layout");
    requireSupportedLvglAbi(config.use_assert_style == 0, "LVGL Zig binding currently supports only LV_USE_ASSERT_STYLE=0");
    requireSupportedLvglAbi(config.use_user_data == 1, "LVGL Zig binding currently requires LV_USE_USER_DATA=1");
    requireSupportedLvglAbi(config.use_large_coord == 0, "LVGL Zig binding currently supports only 16-bit lv_coord_t");
}

fn requireSupportedLvglAbi(comptime ok: bool, comptime message: []const u8) void {
    if (!ok) @compileError(message);
}

/// Minimal LVGL C ABI surface used by this binding.
///
/// Zig 0.13 cannot translate all LVGL 8 bitfield-heavy internals on every host
/// target, so keep the public scalar typedefs and constants explicit here.
pub const c = struct {
    pub const lv_coord_t = i16;
    pub const lv_anim_enable_t = c_uint;
    pub const lv_event_code_t = c_uint;
    pub const lv_flex_align_t = c_uint;
    pub const lv_flex_flow_t = c_uint;
    pub const lv_label_long_mode_t = u8;
    pub const lv_align_t = u8;
    pub const lv_base_dir_t = u8;
    pub const lv_dir_t = u8;
    pub const lv_img_cf_t = u8;
    pub const lv_obj_flag_t = u32;
    pub const lv_opa_t = u8;
    pub const lv_part_t = u32;
    pub const lv_scrollbar_mode_t = u8;
    pub const lv_state_t = u16;
    pub const lv_style_selector_t = u32;
    pub const lv_text_align_t = u8;

    pub const lv_color_t = LvColor;
    pub const lv_point_t = LvPoint;
    pub const lv_area_t = LvArea;
    pub const lv_style_value_t = LvStyleValue;
    pub const lv_style_t = LvStyle;
    pub const lv_img_header_t = LvImgHeader;
    pub const lv_img_dsc_t = LvImgDsc;
    pub const lv_obj_t = LvObj;
    pub const lv_obj_class_t = LvObjClass;
    pub const lv_img_t = LvImg;
    pub const lv_timer_t = LvTimer;

    pub const LVGL_VERSION_MAJOR: c_int = 8;
    pub const LVGL_VERSION_MINOR: c_int = 4;
    pub const LVGL_VERSION_PATCH: c_int = 1;

    pub const LV_COLOR_DEPTH: c_int = config.color_depth;
    pub const LV_BIG_ENDIAN_SYSTEM: c_int = config.big_endian_system;
    pub const LV_USE_ASSERT_STYLE: c_int = config.use_assert_style;
    pub const LV_USE_USER_DATA: c_int = config.use_user_data;
    pub const LV_USE_LARGE_COORD: c_int = config.use_large_coord;

    pub const LV_ANIM_OFF: c_int = 0;
    pub const LV_ANIM_ON: c_int = 1;

    pub const LV_ALIGN_DEFAULT: c_int = 0;
    pub const LV_ALIGN_TOP_LEFT: c_int = 1;
    pub const LV_ALIGN_TOP_MID: c_int = 2;
    pub const LV_ALIGN_TOP_RIGHT: c_int = 3;
    pub const LV_ALIGN_BOTTOM_LEFT: c_int = 4;
    pub const LV_ALIGN_BOTTOM_MID: c_int = 5;
    pub const LV_ALIGN_BOTTOM_RIGHT: c_int = 6;
    pub const LV_ALIGN_LEFT_MID: c_int = 7;
    pub const LV_ALIGN_RIGHT_MID: c_int = 8;
    pub const LV_ALIGN_CENTER: c_int = 9;
    pub const LV_ALIGN_OUT_TOP_LEFT: c_int = 10;
    pub const LV_ALIGN_OUT_TOP_MID: c_int = 11;
    pub const LV_ALIGN_OUT_TOP_RIGHT: c_int = 12;
    pub const LV_ALIGN_OUT_BOTTOM_LEFT: c_int = 13;
    pub const LV_ALIGN_OUT_BOTTOM_MID: c_int = 14;
    pub const LV_ALIGN_OUT_BOTTOM_RIGHT: c_int = 15;
    pub const LV_ALIGN_OUT_LEFT_TOP: c_int = 16;
    pub const LV_ALIGN_OUT_LEFT_MID: c_int = 17;
    pub const LV_ALIGN_OUT_LEFT_BOTTOM: c_int = 18;
    pub const LV_ALIGN_OUT_RIGHT_TOP: c_int = 19;
    pub const LV_ALIGN_OUT_RIGHT_MID: c_int = 20;
    pub const LV_ALIGN_OUT_RIGHT_BOTTOM: c_int = 21;

    pub const LV_BASE_DIR_LTR: c_int = 0;
    pub const LV_BASE_DIR_RTL: c_int = 1;
    pub const LV_BASE_DIR_AUTO: c_int = 2;
    pub const LV_BASE_DIR_NEUTRAL: c_int = 32;
    pub const LV_BASE_DIR_WEAK: c_int = 33;

    pub const LV_DIR_NONE: c_int = 0;
    pub const LV_DIR_LEFT: c_int = 1;
    pub const LV_DIR_RIGHT: c_int = 2;
    pub const LV_DIR_TOP: c_int = 4;
    pub const LV_DIR_BOTTOM: c_int = 8;
    pub const LV_DIR_HOR: c_int = 3;
    pub const LV_DIR_VER: c_int = 12;
    pub const LV_DIR_ALL: c_int = 15;

    pub const LV_EVENT_ALL: c_int = 0;
    pub const LV_EVENT_PRESSED: c_int = 1;
    pub const LV_EVENT_CLICKED: c_int = 7;
    pub const LV_EVENT_VALUE_CHANGED: c_int = 28;
    pub const LV_EVENT_PREPROCESS: c_int = 128;

    pub const LV_FLEX_ALIGN_START: c_int = 0;
    pub const LV_FLEX_ALIGN_END: c_int = 1;
    pub const LV_FLEX_ALIGN_CENTER: c_int = 2;
    pub const LV_FLEX_ALIGN_SPACE_EVENLY: c_int = 3;
    pub const LV_FLEX_ALIGN_SPACE_AROUND: c_int = 4;
    pub const LV_FLEX_ALIGN_SPACE_BETWEEN: c_int = 5;

    pub const LV_FLEX_FLOW_ROW: c_int = 0;
    pub const LV_FLEX_FLOW_COLUMN: c_int = 1;
    pub const LV_FLEX_FLOW_ROW_WRAP: c_int = 4;
    pub const LV_FLEX_FLOW_ROW_REVERSE: c_int = 8;
    pub const LV_FLEX_FLOW_ROW_WRAP_REVERSE: c_int = 12;
    pub const LV_FLEX_FLOW_COLUMN_WRAP: c_int = 5;
    pub const LV_FLEX_FLOW_COLUMN_REVERSE: c_int = 9;
    pub const LV_FLEX_FLOW_COLUMN_WRAP_REVERSE: c_int = 13;

    pub const LV_IMG_CF_UNKNOWN: c_int = 0;
    pub const LV_IMG_CF_RAW: c_int = 1;
    pub const LV_IMG_CF_RAW_ALPHA: c_int = 2;
    pub const LV_IMG_CF_RAW_CHROMA_KEYED: c_int = 3;
    pub const LV_IMG_CF_TRUE_COLOR: c_int = 4;
    pub const LV_IMG_CF_TRUE_COLOR_ALPHA: c_int = 5;
    pub const LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED: c_int = 6;

    pub const LV_LABEL_LONG_WRAP: c_int = 0;
    pub const LV_LABEL_LONG_DOT: c_int = 1;
    pub const LV_LABEL_LONG_SCROLL: c_int = 2;
    pub const LV_LABEL_LONG_SCROLL_CIRCULAR: c_int = 3;
    pub const LV_LABEL_LONG_CLIP: c_int = 4;

    pub extern var LV_LAYOUT_FLEX: u16;

    pub const LV_OBJ_FLAG_HIDDEN: c_int = 1;
    pub const LV_OBJ_FLAG_CLICKABLE: c_int = 2;
    pub const LV_OBJ_FLAG_CLICK_FOCUSABLE: c_int = 4;
    pub const LV_OBJ_FLAG_CHECKABLE: c_int = 8;
    pub const LV_OBJ_FLAG_SCROLLABLE: c_int = 16;
    pub const LV_OBJ_FLAG_SCROLL_ELASTIC: c_int = 32;
    pub const LV_OBJ_FLAG_SCROLL_MOMENTUM: c_int = 64;
    pub const LV_OBJ_FLAG_SCROLL_ONE: c_int = 128;
    pub const LV_OBJ_FLAG_SCROLL_CHAIN_HOR: c_int = 256;
    pub const LV_OBJ_FLAG_SCROLL_CHAIN_VER: c_int = 512;
    pub const LV_OBJ_FLAG_SCROLL_CHAIN: c_int = 768;
    pub const LV_OBJ_FLAG_SCROLL_ON_FOCUS: c_int = 1024;
    pub const LV_OBJ_FLAG_SCROLL_WITH_ARROW: c_int = 2048;
    pub const LV_OBJ_FLAG_SNAPPABLE: c_int = 4096;
    pub const LV_OBJ_FLAG_PRESS_LOCK: c_int = 8192;
    pub const LV_OBJ_FLAG_EVENT_BUBBLE: c_int = 16384;
    pub const LV_OBJ_FLAG_GESTURE_BUBBLE: c_int = 32768;
    pub const LV_OBJ_FLAG_ADV_HITTEST: c_int = 65536;
    pub const LV_OBJ_FLAG_IGNORE_LAYOUT: c_int = 131072;
    pub const LV_OBJ_FLAG_FLOATING: c_int = 262144;
    pub const LV_OBJ_FLAG_OVERFLOW_VISIBLE: c_int = 524288;
    pub const LV_OBJ_FLAG_LAYOUT_1: c_int = 8388608;
    pub const LV_OBJ_FLAG_LAYOUT_2: c_int = 16777216;
    pub const LV_OBJ_FLAG_WIDGET_1: c_int = 33554432;
    pub const LV_OBJ_FLAG_WIDGET_2: c_int = 67108864;
    pub const LV_OBJ_FLAG_USER_1: c_int = 134217728;
    pub const LV_OBJ_FLAG_USER_2: c_int = 268435456;
    pub const LV_OBJ_FLAG_USER_3: c_int = 536870912;
    pub const LV_OBJ_FLAG_USER_4: c_int = 1073741824;

    pub const LV_OPA_TRANSP: c_int = 0;
    pub const LV_OPA_0: c_int = 0;
    pub const LV_OPA_10: c_int = 25;
    pub const LV_OPA_20: c_int = 51;
    pub const LV_OPA_30: c_int = 76;
    pub const LV_OPA_40: c_int = 102;
    pub const LV_OPA_50: c_int = 127;
    pub const LV_OPA_60: c_int = 153;
    pub const LV_OPA_70: c_int = 178;
    pub const LV_OPA_80: c_int = 204;
    pub const LV_OPA_90: c_int = 229;
    pub const LV_OPA_100: c_int = 255;
    pub const LV_OPA_COVER: c_int = 255;

    pub const LV_PART_MAIN: c_int = 0;
    pub const LV_PART_SCROLLBAR: c_int = 0x010000;
    pub const LV_PART_INDICATOR: c_int = 0x020000;
    pub const LV_PART_KNOB: c_int = 0x030000;
    pub const LV_PART_SELECTED: c_int = 0x040000;
    pub const LV_PART_ITEMS: c_int = 0x050000;
    pub const LV_PART_TICKS: c_int = 0x060000;
    pub const LV_PART_CURSOR: c_int = 0x070000;
    pub const LV_PART_CUSTOM_FIRST: c_int = 0x080000;
    pub const LV_PART_ANY: c_int = 0x0F0000;

    pub const LV_RADIUS_CIRCLE: c_int = 0x7FFF;

    pub const LV_SCROLLBAR_MODE_OFF: c_int = 0;
    pub const LV_SCROLLBAR_MODE_ON: c_int = 1;
    pub const LV_SCROLLBAR_MODE_ACTIVE: c_int = 2;
    pub const LV_SCROLLBAR_MODE_AUTO: c_int = 3;

    pub const LV_STATE_DEFAULT: c_int = 0;
    pub const LV_STATE_CHECKED: c_int = 0x0001;
    pub const LV_STATE_FOCUSED: c_int = 0x0002;
    pub const LV_STATE_FOCUS_KEY: c_int = 0x0004;
    pub const LV_STATE_EDITED: c_int = 0x0008;
    pub const LV_STATE_HOVERED: c_int = 0x0010;
    pub const LV_STATE_PRESSED: c_int = 0x0020;
    pub const LV_STATE_SCROLLED: c_int = 0x0040;
    pub const LV_STATE_DISABLED: c_int = 0x0080;
    pub const LV_STATE_USER_1: c_int = 0x1000;
    pub const LV_STATE_USER_2: c_int = 0x2000;
    pub const LV_STATE_USER_3: c_int = 0x4000;
    pub const LV_STATE_USER_4: c_int = 0x8000;
    pub const LV_STATE_ANY: c_int = 0xFFFF;

    pub const LV_TEXT_ALIGN_AUTO: c_int = 0;
    pub const LV_TEXT_ALIGN_LEFT: c_int = 1;
    pub const LV_TEXT_ALIGN_CENTER: c_int = 2;
    pub const LV_TEXT_ALIGN_RIGHT: c_int = 3;
};

pub const LvDisp = opaque {};
pub const LvEvent = opaque {};
pub const LvEventDsc = opaque {};
pub const LvFont = opaque {};

pub const LvColor = extern union {
    full: u16,
};

pub const LvStyleValue = extern union {
    num: i32,
    ptr: ?*const anyopaque,
    color: LvColor,
};

pub const LvStyle = extern struct {
    v_p: extern union {
        value1: LvStyleValue,
        values_and_props: ?*u8,
        const_props: ?*const anyopaque,
    },
    prop1: u16,
    has_group: u8,
    prop_cnt: u8,
};

pub const LvClassEventCb = ?*const fn (?*const LvObjClass, ?*LvEvent) callconv(.C) void;
pub const LvClassLifecycleCb = ?*const fn (?*const LvObjClass, ?*LvObj) callconv(.C) void;
pub const LvEventCb = ?*const fn (?*LvEvent) callconv(.C) void;
pub const LvTimerCb = ?*const fn (?*LvTimer) callconv(.C) void;
pub const TimerCb = LvTimerCb;

pub const LvArea = extern struct {
    x1: c.lv_coord_t,
    y1: c.lv_coord_t,
    x2: c.lv_coord_t,
    y2: c.lv_coord_t,
};

pub const LvPoint = extern struct {
    x: c.lv_coord_t,
    y: c.lv_coord_t,
};

pub const LvObj = extern struct {
    class_p: ?*const LvObjClass,
    parent: ?*LvObj,
    spec_attr: ?*anyopaque,
    styles: ?*anyopaque,
    user_data: ?*anyopaque,
    coords: LvArea,
    flags: u32,
    state: u16,
    layout_bits: u16,
};

pub const LvObjClass = extern struct {
    base_class: ?*const LvObjClass,
    constructor_cb: LvClassLifecycleCb,
    destructor_cb: LvClassLifecycleCb,
    user_data: ?*anyopaque,
    event_cb: LvClassEventCb,
    width_def: c.lv_coord_t,
    height_def: c.lv_coord_t,
    bitfields: u32,
};

pub const LvImg = extern struct {
    obj: LvObj,
    src: ?*const anyopaque,
    offset: LvPoint,
    w: c.lv_coord_t,
    h: c.lv_coord_t,
    angle: u16,
    pivot: LvPoint,
    zoom: u16,
    src_bits: u8,
    size_mode_bits: u8,
};

pub const LvTimer = extern struct {
    period: u32,
    last_run: u32,
    timer_cb: ?*const anyopaque,
    user_data: ?*anyopaque,
    repeat_count: i32,
    bitfields: u32,
};

pub const LvImgHeader = packed struct(u32) {
    cf: u5,
    always_zero: u3,
    reserved: u2,
    w: u11,
    h: u11,
};

pub const LvImgDsc = extern struct {
    header: LvImgHeader,
    data_size: u32,
    data: [*c]const u8,
};

pub const Obj = LvObj;
pub const ObjClass = LvObjClass;
pub const Event = LvEvent;
pub const EventDsc = LvEventDsc;
pub const Timer = LvTimer;
pub const Font = LvFont;
pub const Color = LvColor;
pub const Style = LvStyle;
pub const Area = LvArea;
pub const Point = LvPoint;
pub const Img = LvImg;
pub const ImgDsc = LvImgDsc;

pub extern const lv_obj_class: LvObjClass;
pub extern const lv_img_class: LvObjClass;
pub extern const lv_font_montserrat_14: LvFont;

pub extern fn lv_bar_get_value(obj: ?*const LvObj) callconv(.C) i32;
pub extern fn lv_bar_set_range(obj: ?*LvObj, min: i32, max: i32) callconv(.C) void;
pub extern fn lv_bar_set_value(obj: ?*LvObj, value: i32, anim: c.lv_anim_enable_t) callconv(.C) void;
pub extern fn lv_btn_create(parent: ?*LvObj) callconv(.C) ?*LvObj;
pub extern fn lv_btnmatrix_create(parent: ?*LvObj) callconv(.C) ?*LvObj;
pub extern fn lv_btnmatrix_get_btn_text(obj: ?*const LvObj, btn_id: u16) callconv(.C) [*c]const u8;
pub extern fn lv_btnmatrix_get_selected_btn(obj: ?*const LvObj) callconv(.C) u16;
pub extern fn lv_btnmatrix_set_map(obj: ?*LvObj, map: [*c][*c]const u8) callconv(.C) void;
pub extern fn lv_checkbox_create(parent: ?*LvObj) callconv(.C) ?*LvObj;
pub extern fn lv_checkbox_set_text(obj: ?*LvObj, text: [*c]const u8) callconv(.C) void;
pub extern fn lv_disp_get_default() callconv(.C) ?*LvDisp;
pub extern fn lv_disp_get_layer_sys(disp: ?*LvDisp) callconv(.C) ?*LvObj;
pub extern fn lv_disp_get_scr_act(disp: ?*LvDisp) callconv(.C) ?*LvObj;
pub extern fn lv_disp_load_scr(scr: ?*LvObj) callconv(.C) void;
pub extern fn lv_event_get_code(event: ?*LvEvent) callconv(.C) c.lv_event_code_t;
pub extern fn lv_event_get_target(event: ?*LvEvent) callconv(.C) ?*LvObj;
pub extern fn lv_event_get_user_data(event: ?*LvEvent) callconv(.C) ?*anyopaque;
pub extern fn lv_img_create(parent: ?*LvObj) callconv(.C) ?*LvObj;
pub extern fn lv_img_set_src(obj: ?*LvObj, src: ?*const anyopaque) callconv(.C) void;
pub extern fn lv_img_set_zoom(obj: ?*LvObj, zoom: u16) callconv(.C) void;
pub extern fn lv_init() callconv(.C) void;
pub extern fn lv_label_create(parent: ?*LvObj) callconv(.C) ?*LvObj;
pub extern fn lv_label_get_text(obj: ?*const LvObj) callconv(.C) [*c]const u8;
pub extern fn lv_label_set_long_mode(obj: ?*LvObj, long_mode: c.lv_label_long_mode_t) callconv(.C) void;
pub extern fn lv_label_set_text(obj: ?*LvObj, text: [*c]const u8) callconv(.C) void;
pub extern fn lv_mem_alloc(size: usize) callconv(.C) ?*anyopaque;
pub extern fn lv_mem_free(data: ?*anyopaque) callconv(.C) void;
pub extern fn lv_obj_add_event_cb(obj: ?*LvObj, event_cb: LvEventCb, filter: c.lv_event_code_t, user_data: ?*anyopaque) callconv(.C) ?*LvEventDsc;
pub extern fn lv_obj_add_flag(obj: ?*LvObj, flags: u32) callconv(.C) void;
pub extern fn lv_obj_add_state(obj: ?*LvObj, state: c.lv_state_t) callconv(.C) void;
pub extern fn lv_obj_add_style(obj: ?*LvObj, style: *LvStyle, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_align(obj: ?*LvObj, alignment: c.lv_align_t, x_ofs: c.lv_coord_t, y_ofs: c.lv_coord_t) callconv(.C) void;
pub extern fn lv_obj_align_to(
    obj: ?*LvObj,
    base: ?*LvObj,
    alignment: c.lv_align_t,
    x_ofs: c.lv_coord_t,
    y_ofs: c.lv_coord_t,
) callconv(.C) void;
pub extern fn lv_obj_class_create_obj(class_p: ?*const LvObjClass, parent: ?*LvObj) callconv(.C) ?*LvObj;
pub extern fn lv_obj_class_init_obj(obj: ?*LvObj) callconv(.C) void;
pub extern fn lv_obj_clean(obj: ?*LvObj) callconv(.C) void;
pub extern fn lv_obj_clear_flag(obj: ?*LvObj, flags: u32) callconv(.C) void;
pub extern fn lv_obj_clear_state(obj: ?*LvObj, state: c.lv_state_t) callconv(.C) void;
pub extern fn lv_obj_create(parent: ?*LvObj) callconv(.C) ?*LvObj;
pub extern fn lv_obj_del(obj: ?*LvObj) callconv(.C) void;
pub extern fn lv_obj_del_async(obj: ?*LvObj) callconv(.C) void;
pub extern fn lv_obj_get_child(obj: ?*const LvObj, id: i32) callconv(.C) ?*LvObj;
pub extern fn lv_obj_get_child_cnt(obj: ?*const LvObj) callconv(.C) u32;
pub extern fn lv_obj_get_parent(obj: ?*const LvObj) callconv(.C) ?*LvObj;
pub extern fn lv_obj_has_class(obj: ?*const LvObj, class_p: ?*const LvObjClass) callconv(.C) bool;
pub extern fn lv_obj_is_valid(obj: ?*const LvObj) callconv(.C) bool;
pub extern fn lv_obj_move_to_index(obj: ?*LvObj, index: i32) callconv(.C) void;
pub extern fn lv_obj_set_flex_align(obj: ?*LvObj, main_place: c.lv_flex_align_t, cross_place: c.lv_flex_align_t, track_place: c.lv_flex_align_t) callconv(.C) void;
pub extern fn lv_obj_set_flex_flow(obj: ?*LvObj, flow: c.lv_flex_flow_t) callconv(.C) void;
pub extern fn lv_obj_set_flex_grow(obj: ?*LvObj, grow: u8) callconv(.C) void;
pub extern fn lv_obj_set_height(obj: ?*LvObj, h: c.lv_coord_t) callconv(.C) void;
pub extern fn lv_obj_set_layout(obj: ?*LvObj, layout: u32) callconv(.C) void;
pub extern fn lv_obj_set_pos(obj: ?*LvObj, x: c.lv_coord_t, y: c.lv_coord_t) callconv(.C) void;
pub extern fn lv_obj_set_scroll_dir(obj: ?*LvObj, dir: c.lv_dir_t) callconv(.C) void;
pub extern fn lv_obj_set_scrollbar_mode(obj: ?*LvObj, mode: c.lv_scrollbar_mode_t) callconv(.C) void;
pub extern fn lv_obj_set_size(obj: ?*LvObj, w: c.lv_coord_t, h: c.lv_coord_t) callconv(.C) void;
pub extern fn lv_obj_set_style_base_dir(obj: ?*LvObj, value: c.lv_base_dir_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_bg_color(obj: ?*LvObj, value: LvColor, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_bg_opa(obj: ?*LvObj, value: c.lv_opa_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_border_color(obj: ?*LvObj, value: LvColor, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_border_opa(obj: ?*LvObj, value: c.lv_opa_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_border_width(obj: ?*LvObj, value: c.lv_coord_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_flex_cross_place(obj: ?*LvObj, value: c.lv_flex_align_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_flex_flow(obj: ?*LvObj, value: c.lv_flex_flow_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_flex_main_place(obj: ?*LvObj, value: c.lv_flex_align_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_min_height(obj: ?*LvObj, value: c.lv_coord_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_opa(obj: ?*LvObj, value: c.lv_opa_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_outline_width(obj: ?*LvObj, value: c.lv_coord_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_pad_bottom(obj: ?*LvObj, value: c.lv_coord_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_pad_column(obj: ?*LvObj, value: c.lv_coord_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_pad_left(obj: ?*LvObj, value: c.lv_coord_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_pad_right(obj: ?*LvObj, value: c.lv_coord_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_pad_row(obj: ?*LvObj, value: c.lv_coord_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_pad_top(obj: ?*LvObj, value: c.lv_coord_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_radius(obj: ?*LvObj, value: c.lv_coord_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_shadow_width(obj: ?*LvObj, value: c.lv_coord_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_text_align(obj: ?*LvObj, value: c.lv_text_align_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_text_color(obj: ?*LvObj, value: LvColor, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_text_font(obj: ?*LvObj, value: ?*const LvFont, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_text_letter_space(obj: ?*LvObj, value: c.lv_coord_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_style_text_line_space(obj: ?*LvObj, value: c.lv_coord_t, selector: c.lv_style_selector_t) callconv(.C) void;
pub extern fn lv_obj_set_width(obj: ?*LvObj, w: c.lv_coord_t) callconv(.C) void;
pub extern fn lv_obj_update_layout(obj: ?*LvObj) callconv(.C) void;
pub extern fn lv_slider_create(parent: ?*LvObj) callconv(.C) ?*LvObj;
pub extern fn lv_style_init(style: *LvStyle) callconv(.C) void;
pub extern fn lv_style_set_bg_color(style: *LvStyle, value: LvColor) callconv(.C) void;
pub extern fn lv_style_set_bg_opa(style: *LvStyle, value: c.lv_opa_t) callconv(.C) void;
pub extern fn lv_style_set_border_opa(style: *LvStyle, value: c.lv_opa_t) callconv(.C) void;
pub extern fn lv_style_set_border_width(style: *LvStyle, value: c.lv_coord_t) callconv(.C) void;
pub extern fn lv_style_set_pad_bottom(style: *LvStyle, value: c.lv_coord_t) callconv(.C) void;
pub extern fn lv_style_set_pad_left(style: *LvStyle, value: c.lv_coord_t) callconv(.C) void;
pub extern fn lv_style_set_pad_right(style: *LvStyle, value: c.lv_coord_t) callconv(.C) void;
pub extern fn lv_style_set_pad_top(style: *LvStyle, value: c.lv_coord_t) callconv(.C) void;
pub extern fn lv_style_set_radius(style: *LvStyle, value: c.lv_coord_t) callconv(.C) void;
pub extern fn lv_style_set_shadow_color(style: *LvStyle, value: LvColor) callconv(.C) void;
pub extern fn lv_style_set_shadow_opa(style: *LvStyle, value: c.lv_opa_t) callconv(.C) void;
pub extern fn lv_style_set_shadow_width(style: *LvStyle, value: c.lv_coord_t) callconv(.C) void;
pub extern fn lv_textarea_add_text(obj: ?*LvObj, text: [*c]const u8) callconv(.C) void;
pub extern fn lv_textarea_create(parent: ?*LvObj) callconv(.C) ?*LvObj;
pub extern fn lv_textarea_del_char(obj: ?*LvObj) callconv(.C) void;
pub extern fn lv_textarea_get_text(obj: ?*LvObj) callconv(.C) [*c]const u8;
pub extern fn lv_textarea_set_cursor_click_pos(obj: ?*LvObj, en: bool) callconv(.C) void;
pub extern fn lv_textarea_set_one_line(obj: ?*LvObj, en: bool) callconv(.C) void;
pub extern fn lv_textarea_set_password_mode(obj: ?*LvObj, en: bool) callconv(.C) void;
pub extern fn lv_textarea_set_placeholder_text(obj: ?*LvObj, txt: [*c]const u8) callconv(.C) void;
pub extern fn lv_textarea_set_text(obj: ?*LvObj, text: [*c]const u8) callconv(.C) void;
pub extern fn lv_tick_get() callconv(.C) u32;
pub extern fn lv_timer_create(cb: LvTimerCb, period: u32, user_data: ?*anyopaque) callconv(.C) ?*LvTimer;
pub extern fn lv_timer_del(timer: ?*LvTimer) callconv(.C) void;
pub extern fn lv_timer_handler() callconv(.C) u32;
pub extern fn lv_timer_pause(timer: ?*LvTimer) callconv(.C) void;
pub extern fn lv_timer_reset(timer: ?*LvTimer) callconv(.C) void;
pub extern fn lv_timer_resume(timer: ?*LvTimer) callconv(.C) void;
pub extern fn lv_timer_set_period(timer: ?*LvTimer, period: u32) callconv(.C) void;
pub extern fn lv_timer_set_repeat_count(timer: ?*LvTimer, repeat_count: i32) callconv(.C) void;

// LVGL porting functions provided by the ARCS display/touch port.
pub extern fn lv_port_disp_init(display_dev: ?*device.Device) callconv(.C) void;
pub extern fn lv_port_indev_init(touch_dev: ?*device.Device) callconv(.C) void;
