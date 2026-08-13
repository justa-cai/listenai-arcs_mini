#!/usr/bin/env python3
"""
Generate Lua C bindings for a practical LVGL widget API subset.

The LISA driver binding generator in this sample can parse lisa_* static inline
wrappers directly. LVGL uses a broader C API shape, so this script keeps an
explicit allowlist of stable widget/object functions and parses their
signatures from the LVGL headers.

Lua API design:
    local scr = lvgl.scr_act()
    local label = lvgl.label_create(scr)
    lvgl.label_set_text(label, "hello")
    lvgl.obj_align(label, lvgl.LV_ALIGN_CENTER, 0, 0)
"""

import argparse
import re
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple


# Headers that contain the target functions and the constants useful in Lua.
HEADER_RELATIVE_PATHS = [
    "lvgl.h",
    "src/lvgl.h",
    "src/hal/lv_hal_tick.h",
    "src/misc/lv_area.h",
    "src/misc/lv_anim.h",
    "src/misc/lv_color.h",
    "src/misc/lv_timer.h",
    "src/draw/lv_draw_rect.h",
    "src/core/lv_disp.h",
    "src/core/lv_obj.h",
    "src/core/lv_obj_pos.h",
    "src/core/lv_obj_scroll.h",
    "src/core/lv_obj_style.h",
    "src/core/lv_obj_style_gen.h",
    "src/core/lv_obj_tree.h",
    "src/widgets/lv_arc.h",
    "src/widgets/lv_bar.h",
    "src/widgets/lv_btn.h",
    "src/widgets/lv_checkbox.h",
    "src/widgets/lv_label.h",
    "src/widgets/lv_slider.h",
    "src/widgets/lv_switch.h",
    "src/extra/layouts/flex/lv_flex.h",
]


# Keep the generated binding intentionally small and stable. Add functions here
# when their argument/return types are covered by TYPE_KIND below.
TARGET_FUNCTIONS = [
    "lv_init",
    "lv_is_initialized",
    "lv_version_major",
    "lv_version_minor",
    "lv_version_patch",
    "lv_version_info",
    "lv_tick_get",
    "lv_timer_handler",
    "lv_scr_act",
    "lv_scr_load",
    "lv_obj_create",
    "lv_obj_del",
    "lv_obj_del_async",
    "lv_obj_clean",
    "lv_obj_get_parent",
    "lv_obj_get_child",
    "lv_obj_get_child_cnt",
    "lv_obj_set_parent",
    "lv_obj_set_pos",
    "lv_obj_set_x",
    "lv_obj_set_y",
    "lv_obj_set_size",
    "lv_obj_set_width",
    "lv_obj_set_height",
    "lv_obj_align",
    "lv_obj_align_to",
    "lv_obj_center",
    "lv_obj_update_layout",
    "lv_obj_add_flag",
    "lv_obj_clear_flag",
    "lv_obj_has_flag",
    "lv_obj_add_state",
    "lv_obj_clear_state",
    "lv_obj_has_state",
    "lv_obj_set_scrollbar_mode",
    "lv_obj_set_scroll_dir",
    "lv_obj_scroll_to",
    "lv_obj_scroll_by",
    "lv_obj_set_style_bg_color",
    "lv_obj_set_style_bg_opa",
    "lv_obj_set_style_border_width",
    "lv_obj_set_style_radius",
    "lv_obj_set_style_pad_all",
    "lv_obj_set_style_text_color",
    "lv_obj_set_style_opa",
    "lv_label_create",
    "lv_label_set_text",
    "lv_label_set_long_mode",
    "lv_label_set_recolor",
    "lv_label_get_text",
    "lv_btn_create",
    "lv_bar_create",
    "lv_bar_set_value",
    "lv_bar_set_range",
    "lv_bar_get_value",
    "lv_slider_create",
    "lv_slider_set_value",
    "lv_slider_set_range",
    "lv_slider_get_value",
    "lv_switch_create",
    "lv_checkbox_create",
    "lv_checkbox_set_text",
    "lv_checkbox_get_text",
    "lv_arc_create",
    "lv_arc_set_value",
    "lv_arc_set_range",
    "lv_arc_set_angles",
    "lv_arc_set_bg_angles",
    "lv_arc_set_rotation",
    "lv_obj_set_flex_flow",
    "lv_obj_set_flex_align",
    "lv_obj_set_flex_grow",
]


EXPLICIT_CONSTANTS = [
    "LVGL_VERSION_MAJOR",
    "LVGL_VERSION_MINOR",
    "LVGL_VERSION_PATCH",
    "LV_SIZE_CONTENT",
    "LV_COORD_MAX",
    "LV_COORD_MIN",
    "LV_RADIUS_CIRCLE",
]


STORAGE_OR_ATTR_TOKENS = {
    "static",
    "inline",
    "LVGL_FUNC_ALWAYS_INLINE",
    "LV_ATTRIBUTE_TIMER_HANDLER",
}


TYPE_KIND = {
    "void": "void",
    "bool": "bool",
    "char *": "string",
    "const char *": "string",
    "lv_obj_t *": "obj",
    "const lv_obj_t *": "obj",
    "struct _lv_obj_t *": "obj",
    "const struct _lv_obj_t *": "obj",
    "lv_disp_t *": "ptr",
    "const lv_disp_t *": "ptr",
    "lv_timer_t *": "ptr",
    "lv_color_t": "color",
    "int": "int",
    "uint8_t": "int",
    "uint16_t": "int",
    "uint32_t": "int",
    "int8_t": "int",
    "int16_t": "int",
    "int32_t": "int",
    "size_t": "int",
    "lv_coord_t": "int",
    "lv_opa_t": "int",
    "lv_align_t": "int",
    "lv_anim_enable_t": "int",
    "lv_arc_mode_t": "int",
    "lv_bar_mode_t": "int",
    "lv_dir_t": "int",
    "lv_flex_align_t": "int",
    "lv_flex_flow_t": "int",
    "lv_label_long_mode_t": "int",
    "lv_obj_flag_t": "int",
    "lv_part_t": "int",
    "lv_scr_load_anim_t": "int",
    "lv_scrollbar_mode_t": "int",
    "lv_slider_mode_t": "int",
    "lv_state_t": "int",
    "lv_style_selector_t": "int",
}


OPTIONAL_INT_DEFAULT = {
    "anim": "LV_ANIM_OFF",
    "anim_en": "LV_ANIM_OFF",
    "selector": "0",
}


class Param:
    def __init__(self, c_type, name):
        self.c_type = c_type
        self.name = name


class Function:
    def __init__(self, name, return_type, params, source):
        self.name = name
        self.return_type = return_type
        self.params = params
        self.source = source


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def normalize_type(c_type: str) -> str:
    c_type = c_type.strip()
    for token in STORAGE_OR_ATTR_TOKENS:
        c_type = re.sub(rf"\b{re.escape(token)}\b", "", c_type)
    c_type = c_type.replace("struct _lv_obj_t", "lv_obj_t")
    c_type = re.sub(r"\s+", " ", c_type).strip()
    c_type = re.sub(r"\s*\*\s*", " *", c_type).strip()
    return c_type


def parse_param(decl: str) -> Optional[Param]:
    decl = decl.strip()
    if not decl or decl == "void":
        return None
    decl = re.sub(r"\[[^\]]*\]", "", decl).strip()
    m = re.match(r"^(.*?)([A-Za-z_]\w*)$", decl)
    if not m:
        return None
    return Param(c_type=normalize_type(m.group(1)), name=m.group(2))


def parse_params(params: str) -> List[Param]:
    if params.strip() in ("", "void"):
        return []
    parsed = []
    for item in params.split(","):
        param = parse_param(item)
        if param:
            parsed.append(param)
    return parsed


def parse_functions(headers: Dict[str, str]) -> Dict[str, Function]:
    functions = {}
    pattern = re.compile(
        r"^[ \t]*(?P<ret>[A-Za-z_][\w \t\*]*?)\s*(?P<name>lv_[A-Za-z0-9_]+)\s*"
        r"\((?P<params>[^()]*)\)\s*(?:;|\{)",
        re.MULTILINE,
    )

    for relpath, raw in headers.items():
        text = strip_comments(raw)
        for match in pattern.finditer(text):
            name = match.group("name")
            if name not in TARGET_FUNCTIONS or name in functions:
                continue
            ret = normalize_type(match.group("ret"))
            functions[name] = Function(
                name=name,
                return_type=ret,
                params=parse_params(match.group("params")),
                source=relpath,
            )
    return functions


def parse_enum_constants(headers: Dict[str, str]) -> List[str]:
    names = set()
    enum_pattern = re.compile(r"(?:typedef\s+)?enum(?:\s+\w+)?\s*\{(?P<body>.*?)\}\s*(?:\w+\s*)?;", re.DOTALL)

    for raw in headers.values():
        text = strip_comments(raw)
        for match in enum_pattern.finditer(text):
            for entry in match.group("body").split(","):
                entry = entry.strip()
                if not entry:
                    continue
                name = entry.split("=", 1)[0].strip().split()[0]
                if name.startswith("LV_") and not name.startswith("_LV_"):
                    names.add(name)

    names.update(EXPLICIT_CONSTANTS)
    return sorted(names)


def type_kind(c_type: str) -> Optional[str]:
    return TYPE_KIND.get(normalize_type(c_type))


def lua_name(c_name: str) -> str:
    if c_name.startswith("lv_"):
        return c_name[3:]
    return c_name


def c_wrapper_name(c_name: str) -> str:
    return "l_lvgl_" + lua_name(c_name)


def gen_fetch_param(param: Param, idx: int) -> Tuple[List[str], str]:
    c_type = normalize_type(param.c_type)
    kind = type_kind(c_type)
    if kind is None:
        raise ValueError(f"unsupported parameter type {c_type} for {param.name}")

    lines = []
    name = param.name

    if kind == "obj":
        if name == "parent":
            lines.append(f"    lv_obj_t *{name} = lua_lvgl_opt_parent(L, {idx});")
        elif c_type.startswith("const "):
            lines.append(f"    const lv_obj_t *{name} = (const lv_obj_t *)lua_lvgl_opt_ptr(L, {idx});")
        else:
            lines.append(f"    lv_obj_t *{name} = (lv_obj_t *)lua_lvgl_check_ptr(L, {idx}, \"{name}\");")
        return lines, name

    if kind == "ptr":
        lines.append(f"    {c_type} {name} = ({c_type})lua_lvgl_opt_ptr(L, {idx});")
        return lines, name

    if kind == "string":
        lines.append(f"    const char *{name} = luaL_checkstring(L, {idx});")
        return lines, name

    if kind == "bool":
        lines.append(f"    bool {name} = lua_toboolean(L, {idx});")
        return lines, name

    if kind == "color":
        lines.append(f"    lv_color_t {name} = lv_color_hex((uint32_t)luaL_checkinteger(L, {idx}));")
        return lines, name

    if kind == "int":
        default = OPTIONAL_INT_DEFAULT.get(name)
        if default is not None:
            lines.append(f"    {c_type} {name} = ({c_type})luaL_optinteger(L, {idx}, {default});")
        else:
            lines.append(f"    {c_type} {name} = ({c_type})luaL_checkinteger(L, {idx});")
        return lines, name

    raise ValueError(f"unsupported parameter kind {kind} for {param.name}")


def gen_push_return(return_type: str, value: str) -> Tuple[List[str], int]:
    c_type = normalize_type(return_type)
    kind = type_kind(c_type)
    if kind is None:
        raise ValueError(f"unsupported return type {c_type}")

    if kind == "void":
        return [], 0
    if kind == "bool":
        return [f"    lua_pushboolean(L, {value});"], 1
    if kind == "int":
        return [f"    lua_pushinteger(L, {value});"], 1
    if kind == "string":
        return [
            f"    if ({value}) lua_pushstring(L, {value});",
            "    else lua_pushnil(L);",
        ], 1
    if kind == "color":
        return [f"    lua_pushinteger(L, (lua_Integer)(lv_color_to32({value}) & 0x00ffffff));"], 1
    if kind in ("obj", "ptr"):
        return [
            f"    if ({value}) lua_pushlightuserdata(L, (void *){value});",
            "    else lua_pushnil(L);",
        ], 1
    raise ValueError(f"unsupported return kind {kind}")


def can_bind(func: Function) -> bool:
    if type_kind(func.return_type) is None:
        return False
    return all(type_kind(p.c_type) is not None for p in func.params)


def gen_binding_func(func: Function) -> str:
    ret_type = normalize_type(func.return_type)
    ret_kind = type_kind(ret_type)
    lines = [f"static int {c_wrapper_name(func.name)}(lua_State *L)", "{"]

    call_args = []
    for idx, param in enumerate(func.params, start=1):
        param_lines, arg = gen_fetch_param(param, idx)
        lines.extend(param_lines)
        call_args.append(arg)

    args = ", ".join(call_args)
    lines.append("    lua_lvgl_lock();")
    if ret_kind == "void":
        lines.append(f"    {func.name}({args});")
        lines.append("    lua_lvgl_unlock();")
        lines.append("    return 0;")
    else:
        lines.append(f"    {ret_type} ret = {func.name}({args});")
        lines.append("    lua_lvgl_unlock();")
        push_lines, nret = gen_push_return(ret_type, "ret")
        lines.extend(push_lines)
        lines.append(f"    return {nret};")

    lines.append("}")
    return "\n".join(lines)


def gen_helpers() -> str:
    return r'''static SemaphoreHandle_t lua_lvgl_mutex;

void lua_lvgl_lock_init(void)
{
    if (lua_lvgl_mutex == NULL) {
        lua_lvgl_mutex = xSemaphoreCreateMutex();
    }
}

void lua_lvgl_lock(void)
{
    if (lua_lvgl_mutex != NULL && xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xSemaphoreTake(lua_lvgl_mutex, portMAX_DELAY);
    }
}

void lua_lvgl_unlock(void)
{
    if (lua_lvgl_mutex != NULL && xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xSemaphoreGive(lua_lvgl_mutex);
    }
}

static void *lua_lvgl_opt_ptr(lua_State *L, int idx)
{
    if (lua_isnoneornil(L, idx)) {
        return NULL;
    }
    return lua_touserdata(L, idx);
}

static void *lua_lvgl_check_ptr(lua_State *L, int idx, const char *name)
{
    void *ptr = lua_touserdata(L, idx);
    luaL_argcheck(L, ptr != NULL, idx, name);
    return ptr;
}

static lv_obj_t *lua_lvgl_opt_parent(lua_State *L, int idx)
{
    if (lua_isnoneornil(L, idx)) {
        lv_disp_t *disp = lv_disp_get_default();
        return disp ? lv_disp_get_scr_act(disp) : NULL;
    }
    return (lv_obj_t *)lua_lvgl_check_ptr(L, idx, "parent");
}

static int l_lvgl_pct(lua_State *L)
{
    lv_coord_t value = (lv_coord_t)luaL_checkinteger(L, 1);
    lua_pushinteger(L, lv_pct(value));
    return 1;
}

static int l_lvgl_color_hex(lua_State *L)
{
    uint32_t value = (uint32_t)luaL_checkinteger(L, 1);
    lua_pushinteger(L, (lua_Integer)(lv_color_to32(lv_color_hex(value)) & 0x00ffffff));
    return 1;
}

static int l_lvgl_palette_main(lua_State *L)
{
    lv_palette_t palette = (lv_palette_t)luaL_checkinteger(L, 1);
    lua_pushinteger(L, (lua_Integer)(lv_color_to32(lv_palette_main(palette)) & 0x00ffffff));
    return 1;
}
'''


def gen_register_constants(constants: List[str]) -> str:
    lines = ["static void lvgl_register_constants(lua_State *L)", "{"]
    for name in constants:
        lines.append(f"    lua_pushinteger(L, {name});")
        lines.append(f'    lua_setfield(L, -2, "{name}");')
    lines.append("}")
    return "\n".join(lines)


def gen_source(functions: List[Function], constants: List[str]) -> str:
    lines = [
        "/* Auto-generated by gen_lvgl_bindings.py. Do not edit. */",
        "",
        '#include "lua.h"',
        '#include "lauxlib.h"',
        '#include "lualib.h"',
        '#include "lvgl.h"',
        '#include "FreeRTOS.h"',
        '#include "semphr.h"',
        '#include "task.h"',
        "#include <stdbool.h>",
        "#include <stdint.h>",
        "",
        gen_helpers(),
    ]

    for func in functions:
        lines.append(gen_binding_func(func))
        lines.append("")

    lines.extend([
        "static const luaL_Reg lvgl_lib[] = {",
        '    {"pct", l_lvgl_pct},',
        '    {"color_hex", l_lvgl_color_hex},',
        '    {"palette_main", l_lvgl_palette_main},',
    ])
    for func in functions:
        lines.append(f'    {{"{lua_name(func.name)}", {c_wrapper_name(func.name)}}},')
    lines.extend([
        "    {NULL, NULL}",
        "};",
        "",
        gen_register_constants(constants),
        "",
        "int luaopen_lvgl(lua_State *L)",
        "{",
        "    luaL_newlib(L, lvgl_lib);",
        "    lvgl_register_constants(L);",
        "    return 1;",
        "}",
        "",
    ])
    return "\n".join(lines)


def gen_header() -> str:
    return r'''/* Auto-generated by gen_lvgl_bindings.py. Do not edit. */
#pragma once

#include "lua.h"
#include "lauxlib.h"

int luaopen_lvgl(lua_State *L);
void lua_lvgl_lock_init(void);
void lua_lvgl_lock(void);
void lua_lvgl_unlock(void);

static void luaopen_lvgl_all(lua_State *L)
{
    luaL_requiref(L, "lvgl", luaopen_lvgl, 0);
    lua_setglobal(L, "lvgl");

    lua_getglobal(L, "lisa");
    if (lua_istable(L, -1)) {
        luaL_requiref(L, "lvgl", luaopen_lvgl, 0);
        lua_setfield(L, -2, "lvgl");
    }
    lua_pop(L, 1);
}
'''


def read_headers(lvgl_dir: Path) -> Dict[str, str]:
    headers = {}
    for relpath in HEADER_RELATIVE_PATHS:
        path = lvgl_dir / relpath
        if path.exists():
            headers[relpath] = path.read_text(encoding="utf-8", errors="ignore")
    return headers


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate Lua bindings for a subset of LVGL")
    parser.add_argument("--lvgl-dir", required=True, help="Path to modules/lvgl8 or an LVGL source root")
    parser.add_argument("--output-dir", required=True, help="Directory for generated lua_lvgl files")
    args = parser.parse_args()

    lvgl_dir = Path(args.lvgl_dir)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    headers = read_headers(lvgl_dir)
    if not headers:
        print(f"ERROR: no LVGL headers found under {lvgl_dir}", file=sys.stderr)
        return 1

    parsed = parse_functions(headers)
    missing = [name for name in TARGET_FUNCTIONS if name not in parsed]
    if missing:
        print("ERROR: failed to find LVGL functions:", file=sys.stderr)
        for name in missing:
            print(f"  {name}", file=sys.stderr)
        return 1

    functions = [parsed[name] for name in TARGET_FUNCTIONS]
    skipped = [f for f in functions if not can_bind(f)]
    if skipped:
        print("ERROR: unsupported LVGL function signatures:", file=sys.stderr)
        for func in skipped:
            params = ", ".join(f"{p.c_type} {p.name}" for p in func.params) or "void"
            print(f"  {func.return_type} {func.name}({params})", file=sys.stderr)
        return 1

    constants = parse_enum_constants(headers)
    source_path = output_dir / "lua_lvgl.c"
    header_path = output_dir / "lua_lvgl_all.h"
    source_path.write_text(gen_source(functions, constants), encoding="utf-8")
    header_path.write_text(gen_header(), encoding="utf-8")

    print(f"Generated {len(functions)} LVGL functions -> {source_path}")
    print(f"Generated {len(constants)} LVGL constants -> {header_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
