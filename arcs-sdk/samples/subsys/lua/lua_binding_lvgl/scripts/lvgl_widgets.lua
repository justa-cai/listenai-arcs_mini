-- lvgl_widgets.lua
-- Create a simple LVGL8 screen from Lua.
--
-- Usage: copy to SD card, then run in shell:
--   luarun lvgl_widgets.lua

local lvgl = lvgl
local MAIN = lvgl.LV_PART_MAIN
local SCREEN_W = 320
local SCREEN_H = 240

local scr = lvgl.scr_act()
if scr == nil then
    print("LVGL display is not ready")
    return
end

lvgl.obj_clean(scr)
lvgl.obj_set_style_bg_color(scr, 0x18202a, MAIN)
lvgl.obj_set_style_pad_all(scr, 0, MAIN)

local title = lvgl.label_create(scr)
lvgl.label_set_text(title, "Lua LVGL")
lvgl.obj_set_style_text_color(title, 0xffffff, MAIN)
lvgl.obj_align(title, lvgl.LV_ALIGN_TOP_LEFT, 12, 8)

local subtitle = lvgl.label_create(scr)
lvgl.label_set_text(subtitle, "320x240 display/touch demo")
lvgl.obj_set_style_text_color(subtitle, 0xa9b7c6, MAIN)
lvgl.obj_align(subtitle, lvgl.LV_ALIGN_TOP_LEFT, 12, 31)

local badge = lvgl.label_create(scr)
lvgl.label_set_text(badge, SCREEN_W .. "x" .. SCREEN_H)
lvgl.obj_set_style_text_color(badge, 0x5dd8ff, MAIN)
lvgl.obj_align(badge, lvgl.LV_ALIGN_TOP_RIGHT, -12, 13)

local controls = lvgl.obj_create(scr)
lvgl.obj_set_size(controls, 196, 172)
lvgl.obj_align(controls, lvgl.LV_ALIGN_TOP_LEFT, 8, 58)
lvgl.obj_set_style_radius(controls, 8, MAIN)
lvgl.obj_set_style_bg_color(controls, 0x243241, MAIN)
lvgl.obj_set_style_bg_opa(controls, lvgl.LV_OPA_COVER, MAIN)
lvgl.obj_set_style_pad_all(controls, 10, MAIN)

local label = lvgl.label_create(controls)
lvgl.label_set_text(label, "Touch controls")
lvgl.obj_set_style_text_color(label, 0xffffff, MAIN)
lvgl.obj_align(label, lvgl.LV_ALIGN_TOP_LEFT, 2, 0)

local bar_label = lvgl.label_create(controls)
lvgl.label_set_text(bar_label, "Load")
lvgl.obj_set_style_text_color(bar_label, 0xa9b7c6, MAIN)
lvgl.obj_align(bar_label, lvgl.LV_ALIGN_TOP_LEFT, 2, 28)

local bar = lvgl.bar_create(controls)
lvgl.obj_set_size(bar, 126, 12)
lvgl.obj_align(bar, lvgl.LV_ALIGN_TOP_RIGHT, -2, 30)
lvgl.bar_set_range(bar, 0, 100)
lvgl.bar_set_value(bar, 64, lvgl.LV_ANIM_OFF)

local slider_label = lvgl.label_create(controls)
lvgl.label_set_text(slider_label, "Level")
lvgl.obj_set_style_text_color(slider_label, 0xa9b7c6, MAIN)
lvgl.obj_align(slider_label, lvgl.LV_ALIGN_TOP_LEFT, 2, 62)

local slider = lvgl.slider_create(controls)
lvgl.obj_set_size(slider, 126, 18)
lvgl.obj_align(slider, lvgl.LV_ALIGN_TOP_RIGHT, -2, 60)
lvgl.slider_set_range(slider, 0, 100)
lvgl.slider_set_value(slider, 35, lvgl.LV_ANIM_OFF)

local sw_label = lvgl.label_create(controls)
lvgl.label_set_text(sw_label, "Switch")
lvgl.obj_set_style_text_color(sw_label, 0xa9b7c6, MAIN)
lvgl.obj_align(sw_label, lvgl.LV_ALIGN_TOP_LEFT, 2, 98)

local sw = lvgl.switch_create(controls)
lvgl.obj_align(sw, lvgl.LV_ALIGN_TOP_RIGHT, -2, 92)
lvgl.obj_add_state(sw, lvgl.LV_STATE_CHECKED)

local checkbox = lvgl.checkbox_create(controls)
lvgl.checkbox_set_text(checkbox, "Lua checkbox")
lvgl.obj_set_style_text_color(checkbox, 0xffffff, MAIN)
lvgl.obj_align(checkbox, lvgl.LV_ALIGN_BOTTOM_LEFT, -4, 0)

local meters = lvgl.obj_create(scr)
lvgl.obj_set_size(meters, 108, 172)
lvgl.obj_align(meters, lvgl.LV_ALIGN_TOP_RIGHT, -8, 58)
lvgl.obj_set_style_radius(meters, 8, MAIN)
lvgl.obj_set_style_bg_color(meters, 0x202a35, MAIN)
lvgl.obj_set_style_bg_opa(meters, lvgl.LV_OPA_COVER, MAIN)
lvgl.obj_set_style_pad_all(meters, 8, MAIN)

local meter_label = lvgl.label_create(meters)
lvgl.label_set_text(meter_label, "Meter")
lvgl.obj_set_style_text_color(meter_label, 0xffffff, MAIN)
lvgl.obj_align(meter_label, lvgl.LV_ALIGN_TOP_MID, 0, 0)

local arc = lvgl.arc_create(meters)
lvgl.obj_set_size(arc, 76, 76)
lvgl.obj_align(arc, lvgl.LV_ALIGN_TOP_MID, 0, 28)
lvgl.arc_set_range(arc, 0, 100)
lvgl.arc_set_bg_angles(arc, 135, 45)
lvgl.arc_set_value(arc, 72)

local button = lvgl.btn_create(meters)
lvgl.obj_set_size(button, 84, 34)
lvgl.obj_align(button, lvgl.LV_ALIGN_BOTTOM_MID, 0, -2)
lvgl.obj_set_style_radius(button, 8, MAIN)
lvgl.obj_set_style_bg_color(button, lvgl.palette_main(lvgl.LV_PALETTE_BLUE), MAIN)

local button_label = lvgl.label_create(button)
lvgl.label_set_text(button_label, "Press")
lvgl.obj_center(button_label)

print("LVGL Lua widgets screen created")
