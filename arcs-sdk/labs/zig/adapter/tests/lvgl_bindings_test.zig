const std = @import("std");
const lvgl = @import("lvgl");
const device = @import("../src/bindings/device.zig");
const c = lvgl.c;

fn expectLayout(comptime T: type, comptime size: usize, comptime alignment: usize) !void {
    try std.testing.expectEqual(size, @sizeOf(T));
    try std.testing.expectEqual(alignment, @alignOf(T));
}

fn expectOffset(comptime T: type, comptime field: []const u8, comptime offset: usize) !void {
    try std.testing.expectEqual(offset, @offsetOf(T, field));
}

fn expectSameType(comptime actual: type, comptime expected: type) void {
    comptime {
        if (actual != expected) @compileError("LVGL binding type mismatch");
    }
}

test "LVGL version and scalar constants match LVGL 8 headers" {
    try std.testing.expect(c.LVGL_VERSION_MAJOR == 8);
    try std.testing.expect(c.LVGL_VERSION_MINOR == 4);
    try std.testing.expect(c.LVGL_VERSION_PATCH == 1);

    try std.testing.expect(c.LV_COLOR_DEPTH == 16);
    try std.testing.expect(c.LV_BIG_ENDIAN_SYSTEM == 0);
    try std.testing.expect(c.LV_USE_ASSERT_STYLE == 0);
    try std.testing.expect(c.LV_USE_USER_DATA == 1);
    try std.testing.expect(c.LV_USE_LARGE_COORD == 0);
    try std.testing.expect(c.LV_STATE_CHECKED == 0x0001);
    try std.testing.expect(c.LV_STATE_DISABLED == 0x0080);
    try std.testing.expect(c.LV_STATE_USER_4 == 0x8000);
    try std.testing.expect(c.LV_ALIGN_CENTER == 9);
    try std.testing.expect(c.LV_DIR_HOR == (c.LV_DIR_LEFT | c.LV_DIR_RIGHT));
    try std.testing.expect(c.LV_ANIM_OFF == 0);
    try std.testing.expect(c.LV_ANIM_ON == 1);
    try std.testing.expect(c.LV_EVENT_CLICKED == 7);
    try std.testing.expect(c.LV_IMG_CF_TRUE_COLOR_ALPHA == 5);
}

test "LVGL ABI constants come from the translated LVGL config" {
    try std.testing.expectEqual(c.LV_COLOR_DEPTH, lvgl.config.color_depth);
    try std.testing.expectEqual(c.LV_BIG_ENDIAN_SYSTEM, lvgl.config.big_endian_system);
    try std.testing.expectEqual(c.LV_USE_ASSERT_STYLE, lvgl.config.use_assert_style);
    try std.testing.expectEqual(c.LV_USE_USER_DATA, lvgl.config.use_user_data);
    try std.testing.expectEqual(c.LV_USE_LARGE_COORD, lvgl.config.use_large_coord);
}

test "LVGL scalar typedef bindings match SDK ABI" {
    try expectLayout(c.lv_coord_t, 2, 2);
    try expectLayout(c.lv_anim_enable_t, 4, 4);
    try expectLayout(c.lv_event_code_t, 4, 4);
    try expectLayout(c.lv_flex_align_t, 4, 4);
    try expectLayout(c.lv_flex_flow_t, 4, 4);
    try expectLayout(c.lv_label_long_mode_t, 1, 1);
    try expectLayout(c.lv_align_t, 1, 1);
    try expectLayout(c.lv_base_dir_t, 1, 1);
    try expectLayout(c.lv_dir_t, 1, 1);
    try expectLayout(c.lv_img_cf_t, 1, 1);
    try expectLayout(c.lv_obj_flag_t, 4, 4);
    try expectLayout(c.lv_opa_t, 1, 1);
    try expectLayout(c.lv_part_t, 4, 4);
    try expectLayout(c.lv_scrollbar_mode_t, 1, 1);
    try expectLayout(c.lv_state_t, 2, 2);
    try expectLayout(c.lv_style_selector_t, 4, 4);
    try expectLayout(c.lv_text_align_t, 1, 1);
}

test "LVGL c namespace aliases expose the Zig extern declarations" {
    expectSameType(c.lv_color_t, lvgl.LvColor);
    expectSameType(c.lv_point_t, lvgl.LvPoint);
    expectSameType(c.lv_area_t, lvgl.LvArea);
    expectSameType(c.lv_style_value_t, lvgl.LvStyleValue);
    expectSameType(c.lv_style_t, lvgl.LvStyle);
    expectSameType(c.lv_img_header_t, lvgl.LvImgHeader);
    expectSameType(c.lv_img_dsc_t, lvgl.LvImgDsc);
    expectSameType(c.lv_obj_t, lvgl.LvObj);
    expectSameType(c.lv_obj_class_t, lvgl.LvObjClass);
    expectSameType(c.lv_img_t, lvgl.LvImg);
    expectSameType(c.lv_timer_t, lvgl.LvTimer);
}

test "LVGL geometry and color bindings mirror C layout" {
    try expectLayout(lvgl.LvColor, 2, 2);
    try expectLayout(lvgl.LvPoint, 4, 2);
    try expectLayout(lvgl.LvArea, 8, 2);

    try expectOffset(lvgl.LvPoint, "x", 0);
    try expectOffset(lvgl.LvPoint, "y", 2);
    try expectOffset(lvgl.LvArea, "x1", 0);
    try expectOffset(lvgl.LvArea, "y1", 2);
    try expectOffset(lvgl.LvArea, "x2", 4);
    try expectOffset(lvgl.LvArea, "y2", 6);

    const color: lvgl.LvColor = .{ .full = 0x55aa };
    try std.testing.expectEqual(@as(u16, 0x55aa), color.full);
}

test "LVGL style bindings mirror C layout" {
    const ptr_size = @sizeOf(?*anyopaque);
    const ptr_align = @alignOf(?*anyopaque);

    try expectLayout(lvgl.LvStyleValue, ptr_size, ptr_align);
    try expectLayout(lvgl.LvStyle, std.mem.alignForward(usize, ptr_size + 4, ptr_align), ptr_align);

    try expectOffset(lvgl.LvStyle, "v_p", 0);
    try expectOffset(lvgl.LvStyle, "prop1", ptr_size);
    try expectOffset(lvgl.LvStyle, "has_group", ptr_size + 2);
    try expectOffset(lvgl.LvStyle, "prop_cnt", ptr_size + 3);
}

test "LVGL object and class bindings mirror C layout" {
    const ptr_size = @sizeOf(?*anyopaque);
    const ptr_align = @alignOf(?*anyopaque);
    const class_bitfields_offset = 5 * ptr_size + 4;
    const obj_coords_offset = 5 * ptr_size;
    const obj_flags_offset = obj_coords_offset + @sizeOf(lvgl.LvArea);

    try expectLayout(lvgl.LvObjClass, std.mem.alignForward(usize, class_bitfields_offset + 4, ptr_align), ptr_align);
    try expectLayout(lvgl.LvObj, std.mem.alignForward(usize, obj_flags_offset + 8, ptr_align), ptr_align);

    try expectOffset(lvgl.LvObjClass, "base_class", 0);
    try expectOffset(lvgl.LvObjClass, "constructor_cb", ptr_size);
    try expectOffset(lvgl.LvObjClass, "destructor_cb", 2 * ptr_size);
    try expectOffset(lvgl.LvObjClass, "user_data", 3 * ptr_size);
    try expectOffset(lvgl.LvObjClass, "event_cb", 4 * ptr_size);
    try expectOffset(lvgl.LvObjClass, "width_def", 5 * ptr_size);
    try expectOffset(lvgl.LvObjClass, "height_def", 5 * ptr_size + 2);
    try expectOffset(lvgl.LvObjClass, "bitfields", class_bitfields_offset);

    try expectOffset(lvgl.LvObj, "class_p", 0);
    try expectOffset(lvgl.LvObj, "parent", ptr_size);
    try expectOffset(lvgl.LvObj, "spec_attr", 2 * ptr_size);
    try expectOffset(lvgl.LvObj, "styles", 3 * ptr_size);
    try expectOffset(lvgl.LvObj, "user_data", 4 * ptr_size);
    try expectOffset(lvgl.LvObj, "coords", obj_coords_offset);
    try expectOffset(lvgl.LvObj, "flags", obj_flags_offset);
    try expectOffset(lvgl.LvObj, "state", obj_flags_offset + 4);
    try expectOffset(lvgl.LvObj, "layout_bits", obj_flags_offset + 6);
}

test "LVGL image and timer bindings mirror C layout" {
    const ptr_size = @sizeOf(?*anyopaque);
    const ptr_align = @alignOf(?*anyopaque);
    const img_src_offset = @sizeOf(lvgl.LvObj);
    const img_offset_offset = img_src_offset + ptr_size;
    const img_w_offset = img_offset_offset + @sizeOf(lvgl.LvPoint);
    const img_pivot_offset = img_w_offset + 6;
    const img_zoom_offset = img_pivot_offset + @sizeOf(lvgl.LvPoint);

    try expectLayout(lvgl.LvImgHeader, 4, 4);
    try expectLayout(lvgl.LvImgDsc, std.mem.alignForward(usize, 8 + ptr_size, ptr_align), ptr_align);
    try expectLayout(lvgl.LvImg, std.mem.alignForward(usize, img_zoom_offset + 4, ptr_align), ptr_align);
    try expectLayout(lvgl.LvTimer, std.mem.alignForward(usize, 16 + 2 * ptr_size, ptr_align), ptr_align);

    try expectOffset(lvgl.LvImgDsc, "header", 0);
    try expectOffset(lvgl.LvImgDsc, "data_size", 4);
    try expectOffset(lvgl.LvImgDsc, "data", 8);

    try expectOffset(lvgl.LvImg, "obj", 0);
    try expectOffset(lvgl.LvImg, "src", img_src_offset);
    try expectOffset(lvgl.LvImg, "offset", img_offset_offset);
    try expectOffset(lvgl.LvImg, "w", img_w_offset);
    try expectOffset(lvgl.LvImg, "h", img_w_offset + 2);
    try expectOffset(lvgl.LvImg, "angle", img_w_offset + 4);
    try expectOffset(lvgl.LvImg, "pivot", img_pivot_offset);
    try expectOffset(lvgl.LvImg, "zoom", img_zoom_offset);
    try expectOffset(lvgl.LvImg, "src_bits", img_zoom_offset + 2);
    try expectOffset(lvgl.LvImg, "size_mode_bits", img_zoom_offset + 3);

    try expectOffset(lvgl.LvTimer, "period", 0);
    try expectOffset(lvgl.LvTimer, "last_run", 4);
    try expectOffset(lvgl.LvTimer, "timer_cb", 8);
    try expectOffset(lvgl.LvTimer, "user_data", 8 + ptr_size);
    try expectOffset(lvgl.LvTimer, "repeat_count", 8 + 2 * ptr_size);
    try expectOffset(lvgl.LvTimer, "bitfields", 12 + 2 * ptr_size);
}

test "LVGL Zig-only image header bit packing is usable" {
    const header: lvgl.LvImgHeader = .{
        .cf = c.LV_IMG_CF_TRUE_COLOR_ALPHA,
        .always_zero = 0,
        .reserved = 0,
        .w = 240,
        .h = 320,
    };

    try std.testing.expectEqual(@as(u5, c.LV_IMG_CF_TRUE_COLOR_ALPHA), header.cf);
    try std.testing.expectEqual(@as(u3, 0), header.always_zero);
    try std.testing.expectEqual(@as(u2, 0), header.reserved);
    try std.testing.expectEqual(@as(u11, 240), header.w);
    try std.testing.expectEqual(@as(u11, 320), header.h);
}

test "LVGL callback aliases accept C callbacks" {
    const event_cb: lvgl.LvEventCb = eventCallback;
    const timer_cb: lvgl.LvTimerCb = timerCallback;

    try std.testing.expect(event_cb != null);
    try std.testing.expect(timer_cb != null);
}

test "LVGL extern functions keep C signatures" {
    expectSameType(@TypeOf(lvgl.lv_obj_create), fn (?*lvgl.LvObj) callconv(.C) ?*lvgl.LvObj);
    expectSameType(@TypeOf(lvgl.lv_obj_add_event_cb), fn (?*lvgl.LvObj, lvgl.LvEventCb, c.lv_event_code_t, ?*anyopaque) callconv(.C) ?*lvgl.LvEventDsc);
    expectSameType(@TypeOf(lvgl.lv_obj_set_style_bg_color), fn (?*lvgl.LvObj, lvgl.LvColor, c.lv_style_selector_t) callconv(.C) void);
    expectSameType(@TypeOf(lvgl.lv_style_init), fn (*lvgl.LvStyle) callconv(.C) void);
    expectSameType(@TypeOf(lvgl.lv_img_set_src), fn (?*lvgl.LvObj, ?*const anyopaque) callconv(.C) void);
    expectSameType(@TypeOf(lvgl.lv_timer_create), fn (lvgl.LvTimerCb, u32, ?*anyopaque) callconv(.C) ?*lvgl.LvTimer);
    expectSameType(@TypeOf(lvgl.lv_port_disp_init), fn (?*device.Device) callconv(.C) void);
}

fn eventCallback(_: ?*lvgl.LvEvent) callconv(.C) void {}

fn timerCallback(_: ?*lvgl.LvTimer) callconv(.C) void {}
